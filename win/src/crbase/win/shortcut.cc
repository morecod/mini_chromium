// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/win/shortcut.h"

#include <shellapi.h>
#include <shlobj.h>
#include <propkey.h>

#include "crbase/files/file_util.h"
#include "crbase/win/scoped_comptr.h"
#include "crbase/win/scoped_propvariant.h"
#include "crbase/win/win_util.h"
#include "crbase/win/windows_version.h"
#include "crbase/logging.h"

namespace cr {
namespace win {

namespace {

//  Name:     System.AppUserModel.IsDualMode -- PKEY_AppUserModel_IsDualMode
//  Type:     Boolean -- VT_BOOL
//  FormatID: {9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3}, 11
//
//  Deprecated. Indicates that an application supports dual desktop and
//  immersive modes. In Windows 8, this property is only applicable for web
//  browsers.
DEFINE_PROPERTYKEY(CRPKEY_AppUserModel_IsDualMode,
                   0x9F4C2855,
                   0x9F79,
                   0x4B39,
                   0xA8,
                   0xD0,
                   0xE1,
                   0xD4,
                   0x2D,
                   0xE1,
                   0xD5,
                   0xF3,
                   11);

// Initializes |i_shell_link| and |i_persist_file| (releasing them first if they
// are already initialized).
// If |shortcut| is not NULL, loads |shortcut| into |i_persist_file|.
// If any of the above steps fail, both |i_shell_link| and |i_persist_file| will
// be released.
void InitializeShortcutInterfaces(
    const wchar_t* shortcut,
    CR_SCOPED_COMPTR(IShellLinkW)* i_shell_link,
    CR_SCOPED_COMPTR(IPersistFile)* i_persist_file) {
  i_shell_link->Release();
  i_persist_file->Release();
  if (FAILED(i_shell_link->CreateInstance(CLSID_ShellLink, NULL,
                                          CLSCTX_INPROC_SERVER)) ||
      FAILED(i_persist_file->QueryFrom(i_shell_link->get())) ||
      (shortcut && FAILED((*i_persist_file)->Load(shortcut, STGM_READWRITE)))) {
    i_shell_link->Release();
    i_persist_file->Release();
  }
}

}  // namespace

ShortcutProperties::ShortcutProperties()
    : icon_index(-1), dual_mode(false), options(0U) {
}

ShortcutProperties::~ShortcutProperties() {
}

bool CreateOrUpdateShortcutLink(const FilePath& shortcut_path,
                                const ShortcutProperties& properties,
                                ShortcutOperation operation) {
  // A target is required unless |operation| is SHORTCUT_UPDATE_EXISTING.
  if (operation != SHORTCUT_UPDATE_EXISTING &&
      !(properties.options & ShortcutProperties::PROPERTIES_TARGET)) {
    return false;
  }

  bool shortcut_existed = PathExists(shortcut_path);

  // Interfaces to the old shortcut when replacing an existing shortcut.
  ///ScopedComPtr<IShellLink> old_i_shell_link;
  ///ScopedComPtr<IPersistFile> old_i_persist_file;
  CR_SCOPED_COMPTR(IShellLinkW) old_i_shell_link;
  CR_SCOPED_COMPTR(IPersistFile) old_i_persist_file;

  // Interfaces to the shortcut being created/updated.
  ///ScopedComPtr<IShellLinkW> i_shell_link;
  ///ScopedComPtr<IPersistFile> i_persist_file;
  CR_SCOPED_COMPTR(IShellLinkW) i_shell_link;
  CR_SCOPED_COMPTR(IPersistFile) i_persist_file;

  switch (operation) {
    case SHORTCUT_CREATE_ALWAYS:
      InitializeShortcutInterfaces(NULL, &i_shell_link, &i_persist_file);
      break;
    case SHORTCUT_UPDATE_EXISTING:
      InitializeShortcutInterfaces(shortcut_path.value().c_str(), &i_shell_link,
                                   &i_persist_file);
      break;
    case SHORTCUT_REPLACE_EXISTING:
      InitializeShortcutInterfaces(shortcut_path.value().c_str(),
                                   &old_i_shell_link, &old_i_persist_file);
      // Confirm |shortcut_path| exists and is a shortcut by verifying
      // |old_i_persist_file| was successfully initialized in the call above. If
      // so, initialize the interfaces to begin writing a new shortcut (to
      // overwrite the current one if successful).
      if (old_i_persist_file.get())
        InitializeShortcutInterfaces(NULL, &i_shell_link, &i_persist_file);
      break;
    default: break;
  }

  // Return false immediately upon failure to initialize shortcut interfaces.
  if (!i_persist_file.get())
    return false;

  if ((properties.options & ShortcutProperties::PROPERTIES_TARGET) &&
      FAILED(i_shell_link->SetPath(properties.target.value().c_str()))) {
    return false;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_WORKING_DIR) &&
      FAILED(i_shell_link->SetWorkingDirectory(
          properties.working_dir.value().c_str()))) {
    return false;
  }

  if (properties.options & ShortcutProperties::PROPERTIES_ARGUMENTS) {
    if (FAILED(i_shell_link->SetArguments(properties.arguments.c_str())))
      return false;
  } else if (old_i_persist_file.get()) {
    wchar_t current_arguments[MAX_PATH] = {0};
    if (SUCCEEDED(old_i_shell_link->GetArguments(current_arguments,
                                                 MAX_PATH))) {
      i_shell_link->SetArguments(current_arguments);
    }
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_DESCRIPTION) &&
      FAILED(i_shell_link->SetDescription(properties.description.c_str()))) {
    return false;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_ICON) &&
      FAILED(i_shell_link->SetIconLocation(properties.icon.value().c_str(),
                                           properties.icon_index))) {
    return false;
  }

  bool has_app_id =
      (properties.options & ShortcutProperties::PROPERTIES_APP_ID) != 0;
  bool has_dual_mode =
      (properties.options & ShortcutProperties::PROPERTIES_DUAL_MODE) != 0;
  if ((has_app_id || has_dual_mode) &&
      GetVersion() >= Version::WIN7) {
    ///ScopedComPtr<IPropertyStore> property_store;
    CR_SCOPED_COMPTR(IPropertyStore) property_store;
    if (FAILED(property_store.QueryFrom(i_shell_link.get())) ||
        !property_store.get())
      return false;

    if (has_app_id &&
        !SetAppIdForPropertyStore(property_store.get(),
                                  properties.app_id.c_str())) {
      return false;
    }
    if (has_dual_mode &&
        !SetBooleanValueForPropertyStore(property_store.get(),
                                         CRPKEY_AppUserModel_IsDualMode,
                                         properties.dual_mode)) {
      return false;
    }
  }

  // Release the interfaces to the old shortcut to make sure it doesn't prevent
  // overwriting it if needed.
  old_i_persist_file.Release();
  old_i_shell_link.Release();

  HRESULT result = i_persist_file->Save(shortcut_path.value().c_str(), TRUE);

  // Release the interfaces in case the SHChangeNotify call below depends on
  // the operations above being fully completed.
  i_persist_file.Release();
  i_shell_link.Release();

  // If we successfully created/updated the icon, notify the shell that we have
  // done so.
  const bool succeeded = SUCCEEDED(result);
  if (succeeded) {
    if (shortcut_existed) {
      // TODO(gab): SHCNE_UPDATEITEM might be sufficient here; further testing
      // required.
      SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    } else {
      SHChangeNotify(SHCNE_CREATE, SHCNF_PATH, shortcut_path.value().c_str(),
                     NULL);
    }
  }

  return succeeded;
}

bool ResolveShortcutProperties(const FilePath& shortcut_path,
                               uint32_t options,
                               ShortcutProperties* properties) {
  CR_DCHECK(options && properties);

  if (options & ~ShortcutProperties::PROPERTIES_ALL)
    CR_NOTREACHED() << "Unhandled property is used.";

  ///ScopedComPtr<IShellLinkW> i_shell_link;
  CR_SCOPED_COMPTR(IShellLinkW) i_shell_link;

  // Get pointer to the IShellLink interface.
  if (FAILED(i_shell_link.CreateInstance(CLSID_ShellLink, NULL,
                                         CLSCTX_INPROC_SERVER))) {
    return false;
  }

  ///ScopedComPtr<IPersistFile> persist;
  CR_SCOPED_COMPTR(IPersistFile) persist;
  // Query IShellLink for the IPersistFile interface.
  if (FAILED(persist.QueryFrom(i_shell_link.get())))
    return false;

  // Load the shell link.
  if (FAILED(persist->Load(shortcut_path.value().c_str(), STGM_READ)))
    return false;

  // Reset |properties|.
  properties->options = 0;

  wchar_t temp[MAX_PATH];
  if (options & ShortcutProperties::PROPERTIES_TARGET) {
    if (FAILED(i_shell_link->GetPath(temp, MAX_PATH, NULL, SLGP_UNCPRIORITY)))
      return false;
    properties->set_target(FilePath(temp));
  }

  if (options & ShortcutProperties::PROPERTIES_WORKING_DIR) {
    if (FAILED(i_shell_link->GetWorkingDirectory(temp, MAX_PATH)))
      return false;
    properties->set_working_dir(FilePath(temp));
  }

  if (options & ShortcutProperties::PROPERTIES_ARGUMENTS) {
    if (FAILED(i_shell_link->GetArguments(temp, MAX_PATH)))
      return false;
    properties->set_arguments(temp);
  }

  if (options & ShortcutProperties::PROPERTIES_DESCRIPTION) {
    // Note: description length constrained by MAX_PATH.
    if (FAILED(i_shell_link->GetDescription(temp, MAX_PATH)))
      return false;
    properties->set_description(temp);
  }

  if (options & ShortcutProperties::PROPERTIES_ICON) {
    int temp_index;
    if (FAILED(i_shell_link->GetIconLocation(temp, MAX_PATH, &temp_index)))
      return false;
    properties->set_icon(FilePath(temp), temp_index);
  }

  // Windows 7+ options, avoiding unnecessary work.
  if ((options & ShortcutProperties::PROPERTIES_WIN7) &&
      GetVersion() >= Version::WIN7) {
    ///ScopedComPtr<IPropertyStore> property_store;
    CR_SCOPED_COMPTR(IPropertyStore) property_store;
    if (FAILED(property_store.QueryFrom(i_shell_link.get())))
      return false;

    if (options & ShortcutProperties::PROPERTIES_APP_ID) {
      ScopedPropVariant pv_app_id;
      if (property_store->GetValue(PKEY_AppUserModel_ID,
                                   pv_app_id.Receive()) != S_OK) {
        return false;
      }
      switch (pv_app_id.get().vt) {
        case VT_EMPTY:
          properties->set_app_id(L"");
          break;
        case VT_LPWSTR:
          properties->set_app_id(pv_app_id.get().pwszVal);
          break;
        default:
          CR_NOTREACHED() << "Unexpected variant type: " << pv_app_id.get().vt;
          return false;
      }
    }

    if (options & ShortcutProperties::PROPERTIES_DUAL_MODE) {
      ScopedPropVariant pv_dual_mode;
      if (property_store->GetValue(CRPKEY_AppUserModel_IsDualMode,
                                   pv_dual_mode.Receive()) != S_OK) {
        return false;
      }
      switch (pv_dual_mode.get().vt) {
        case VT_EMPTY:
          properties->set_dual_mode(false);
          break;
        case VT_BOOL:
          properties->set_dual_mode(pv_dual_mode.get().boolVal == VARIANT_TRUE);
          break;
        default:
          CR_NOTREACHED() << "Unexpected variant type: "
                          << pv_dual_mode.get().vt;
          return false;
      }
    }
  }

  return true;
}

bool ResolveShortcut(const FilePath& shortcut_path,
                     FilePath* target_path,
                     string16* args) {
  uint32_t options = 0;
  if (target_path)
    options |= ShortcutProperties::PROPERTIES_TARGET;
  if (args)
    options |= ShortcutProperties::PROPERTIES_ARGUMENTS;
  CR_DCHECK(options);

  ShortcutProperties properties;
  if (!ResolveShortcutProperties(shortcut_path, options, &properties))
    return false;

  if (target_path)
    *target_path = properties.target;
  if (args)
    *args = properties.arguments;
  return true;
}

bool CanPinShortcutToTaskbar() {
  // "Pin to taskbar" appeared in Windows 7 and stopped being supported in
  // Windows 10.
  return GetVersion() >= Version::WIN7 && GetVersion() < Version::WIN10;
}

bool PinShortcutToTaskbar(const FilePath& shortcut) {
  CR_DCHECK(CanPinShortcutToTaskbar());

  intptr_t result = reinterpret_cast<intptr_t>(ShellExecuteW(
      NULL, L"taskbarpin", shortcut.value().c_str(), NULL, NULL, 0));
  return result > 32;
}

bool UnpinShortcutFromTaskbar(const FilePath& shortcut) {
  // "Unpin from taskbar" is only supported after Win7. It is possible to remove
  // a shortcut pinned by a user on Windows 10+.
  if (GetVersion() < Version::WIN7)
    return false;

  intptr_t result = reinterpret_cast<intptr_t>(ShellExecuteW(
      NULL, L"taskbarunpin", shortcut.value().c_str(), NULL, NULL, 0));
  return result > 32;
}

}  // namespace win
}  // namespace cr
