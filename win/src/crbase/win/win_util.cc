// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/win/win_util.h"

#include <aclapi.h>
#include <cfgmgr32.h>
#include <lm.h>
#include <powrprof.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>  // Must be before propkey.
#include <initguid.h>
#include <propkey.h>
#include <propvarutil.h>
#include <sddl.h>
#include <setupapi.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>

#include <memory>

#include "crbase/macros.h"
#include "crbase/logging.h"
#include "crbase/strings/string_util.h"
#include "crbase/strings/stringprintf.h"
#include "crbase/strings/utf_string_conversions.h"
#include "crbase/win/registry.h"
#include "crbase/win/scoped_co_mem.h"
#include "crbase/win/scoped_handle.h"
#include "crbase/win/scoped_propvariant.h"
#include "crbase/win/windows_version.h"

namespace cr {
namespace win {

namespace {

// Sets the value of |property_key| to |property_value| in |property_store|.
bool SetPropVariantValueForPropertyStore(
    IPropertyStore* property_store,
    const PROPERTYKEY& property_key,
    const ScopedPropVariant& property_value) {
  assert(property_store);

  HRESULT result = property_store->SetValue(property_key, property_value.get());
  if (result == S_OK)
    result = property_store->Commit();
  return SUCCEEDED(result);
}

void __cdecl ForceCrashOnSigAbort(int) {
  *((volatile int*)0) = 0x1337;
}

const wchar_t kWindows8OSKRegPath[] =
    L"Software\\Classes\\CLSID\\{054AAE20-4BEA-4347-8A35-64A533254A9D}"
    L"\\LocalServer32";

// Returns the current platform role. We use the PowerDeterminePlatformRoleEx
// API for that.
POWER_PLATFORM_ROLE GetPlatformRole() {
  typedef HRESULT(WINAPI* ZwPowerInformationFunc)(
      ULONG, PVOID, ULONG, PULONG, ULONG);
  static ZwPowerInformationFunc ZwPowerInformation = NULL;
  if (ZwPowerInformation == NULL) {
    // we don`t want using LoadlibraryW(L"PowerDeterminePlatformRoleEx"),
    // it may be making a dead lock when using in 'DllMain'.
    ZwPowerInformation = reinterpret_cast<ZwPowerInformationFunc>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "ZwPowerInformation"));
  }
  if (ZwPowerInformation == NULL)
    return PlatformRoleDesktop;

  ULONG value = PlatformRoleDesktop;
  HRESULT hr = ZwPowerInformation(0x4B, NULL, 0, &value, sizeof(value));
  if (FAILED(hr)) return PlatformRoleDesktop;

  return static_cast<POWER_PLATFORM_ROLE>(value);
  // return PowerDeterminePlatformRoleEx(POWER_PLATFORM_ROLE_V2);
}

}  // namespace

// Returns true if a physical keyboard is detected on Windows 8 and up.
// Uses the Setup APIs to enumerate the attached keyboards and returns true
// if the keyboard count is 1 or more.. While this will work in most cases
// it won't work if there are devices which expose keyboard interfaces which
// are attached to the machine.
bool IsKeyboardPresentOnSlate(std::string* reason) {
  bool result = false;

  if (GetVersion() < Version::WIN8) {
    *reason = "Detection not supported";
    return false;
  }

  // This function should be only invoked for machines with touch screens.
  if ((GetSystemMetrics(SM_DIGITIZER) & NID_INTEGRATED_TOUCH)
        != NID_INTEGRATED_TOUCH) {
    if (reason) {
      *reason += "NID_INTEGRATED_TOUCH\n";
      result = true;
    } else {
      return true;
    }
  }

  if (IsTabletDevice(reason)) {
    if (reason)
      *reason += "Tablet device.\n";
    return true;
  } else {
    if (reason) {
      *reason += "Not a tablet device";
      result = true;
    } else {
      return true;
    }
  }

  // To determine whether a keyboard is present on the device, we do the
  // following:-
  // 1. Check whether the device supports auto rotation. If it does then
  //    it possibly supports flipping from laptop to slate mode. If it
  //    does not support auto rotation, then we assume it is a desktop
  //    or a normal laptop and assume that there is a keyboard.

  // 2. If the device supports auto rotation, then we get its platform role
  //    and check the system metric SM_CONVERTIBLESLATEMODE to see if it is
  //    being used in slate mode. If yes then we return false here to ensure
  //    that the OSK is displayed.

  // 3. If step 1 and 2 fail then we check attached keyboards and return true
  //    if we find ACPI\* or HID\VID* keyboards.

  typedef BOOL (WINAPI* GetAutoRotationState)(PAR_STATE state);

  GetAutoRotationState get_rotation_state =
      reinterpret_cast<GetAutoRotationState>(::GetProcAddress(
          GetModuleHandleW(L"user32.dll"), "GetAutoRotationState"));

  if (get_rotation_state) {
    AR_STATE auto_rotation_state = AR_ENABLED;
    get_rotation_state(&auto_rotation_state);
    if ((auto_rotation_state & AR_NOSENSOR) ||
        (auto_rotation_state & AR_NOT_SUPPORTED)) {
      // If there is no auto rotation sensor or rotation is not supported in
      // the current configuration, then we can assume that this is a desktop
      // or a traditional laptop.
      if (reason) {
        *reason += (auto_rotation_state & AR_NOSENSOR) ? "AR_NOSENSOR\n" :
                                                         "AR_NOT_SUPPORTED\n";
        result = true;
      } else {
        return true;
      }
    }
  }

  const GUID KEYBOARD_CLASS_GUID =
      { 0x4D36E96B, 0xE325,  0x11CE,
          { 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 } };

  // Query for all the keyboard devices.
  HDEVINFO device_info =
      SetupDiGetClassDevs(&KEYBOARD_CLASS_GUID, NULL, NULL, DIGCF_PRESENT);
  if (device_info == INVALID_HANDLE_VALUE) {
    if (reason)
      *reason += "No keyboard info\n";
    return result;
  }

  // Enumerate all keyboards and look for ACPI\PNP and HID\VID devices. If
  // the count is more than 1 we assume that a keyboard is present. This is
  // under the assumption that there will always be one keyboard device.
  for (DWORD i = 0;; ++i) {
    SP_DEVINFO_DATA device_info_data = { 0 };
    device_info_data.cbSize = sizeof(device_info_data);
    if (!SetupDiEnumDeviceInfo(device_info, i, &device_info_data))
      break;

    // Get the device ID.
    wchar_t device_id[MAX_DEVICE_ID_LEN];
    CONFIGRET status = CM_Get_Device_IDW(device_info_data.DevInst,
                                         device_id,
                                         MAX_DEVICE_ID_LEN,
                                         0);
    if (status == CR_SUCCESS) {
      // To reduce the scope of the hack we only look for ACPI and HID\\VID
      // prefixes in the keyboard device ids.
      if (StartsWith(device_id, L"ACPI", CompareCase::INSENSITIVE_ASCII) ||
          StartsWith(device_id, L"HID\\VID", CompareCase::INSENSITIVE_ASCII)) {
        if (reason) {
          *reason += "device: ";
          *reason += WideToUTF8(device_id);
          *reason += '\n';
        }
        // The heuristic we are using is to check the count of keyboards and
        // return true if the API's report one or more keyboards. Please note
        // that this will break for non keyboard devices which expose a
        // keyboard PDO.
        result = true;
      }
    }
  }
  return result;
}

static bool g_crash_on_process_detach = false;

void GetNonClientMetrics(NONCLIENTMETRICS_XP* metrics) {
  CR_DCHECK(metrics);
  metrics->cbSize = sizeof(*metrics);
  const bool success = !!SystemParametersInfo(
      SPI_GETNONCLIENTMETRICS,
      metrics->cbSize,
      reinterpret_cast<NONCLIENTMETRICS*>(metrics),
      0);
  CR_DCHECK(success);
}

bool GetUserSidString(std::wstring* user_sid) {
  // Get the current token.
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return false;
  ScopedHandle token_scoped(token);

  DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
  std::unique_ptr<BYTE[]> user_bytes(new BYTE[size]);
  TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(user_bytes.get());

  if (!::GetTokenInformation(token, TokenUser, user, size, &size))
    return false;

  if (!user->User.Sid)
    return false;

  // Convert the data to a string.
  wchar_t* sid_string;
  if (!::ConvertSidToStringSidW(user->User.Sid, &sid_string))
    return false;

  *user_sid = sid_string;

  ::LocalFree(sid_string);

  return true;
}

bool UserAccountControlIsEnabled() {
  // This can be slow if Windows ends up going to disk.  Should watch this key
  // for changes and only read it once, preferably on the file thread.
  //   http://code.google.com/p/chromium/issues/detail?id=61644
  RegKey key(HKEY_LOCAL_MACHINE,
             L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
             KEY_READ);
  DWORD uac_enabled;
  if (key.ReadValueDW(L"EnableLUA", &uac_enabled) != ERROR_SUCCESS)
    return true;
  // Users can set the EnableLUA value to something arbitrary, like 2, which
  // Vista will treat as UAC enabled, so we make sure it is not set to 0.
  return (uac_enabled != 0);
}

bool SetBooleanValueForPropertyStore(IPropertyStore* property_store,
                                     const PROPERTYKEY& property_key,
                                     bool property_bool_value) {
  ScopedPropVariant property_value;
  if (FAILED(InitPropVariantFromBoolean(property_bool_value,
                                        property_value.Receive()))) {
    return false;
  }

  return SetPropVariantValueForPropertyStore(property_store,
                                             property_key,
                                             property_value);
}

bool SetStringValueForPropertyStore(IPropertyStore* property_store,
                                    const PROPERTYKEY& property_key,
                                    const wchar_t* property_string_value) {
  ScopedPropVariant property_value;
  if (FAILED(InitPropVariantFromString(property_string_value,
                                       property_value.Receive()))) {
    return false;
  }

  return SetPropVariantValueForPropertyStore(property_store,
                                             property_key,
                                             property_value);
}

bool SetAppIdForPropertyStore(IPropertyStore* property_store,
                              const wchar_t* app_id) {
  // App id should be less than 64 chars and contain no space. And recommended
  // format is CompanyName.ProductName[.SubProduct.ProductNumber].
  // See http://msdn.microsoft.com/en-us/library/dd378459%28VS.85%29.aspx
  assert(lstrlenW(app_id) < 64 && wcschr(app_id, L' ') == NULL);

  return SetStringValueForPropertyStore(property_store,
                                        PKEY_AppUserModel_ID,
                                        app_id);
}

static const char16 kAutoRunKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool AddCommandToAutoRun(HKEY root_key, const string16& name,
                         const string16& command) {
  RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_SET_VALUE);
  return (autorun_key.WriteValue(name.c_str(), command.c_str()) ==
      ERROR_SUCCESS);
}

bool RemoveCommandFromAutoRun(HKEY root_key, const string16& name) {
  RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_SET_VALUE);
  return (autorun_key.DeleteValue(name.c_str()) == ERROR_SUCCESS);
}

bool ReadCommandFromAutoRun(HKEY root_key,
                            const string16& name,
                            string16* command) {
  RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_QUERY_VALUE);
  return (autorun_key.ReadValue(name.c_str(), command) == ERROR_SUCCESS);
}

void SetShouldCrashOnProcessDetach(bool crash) {
  g_crash_on_process_detach = crash;
}

bool ShouldCrashOnProcessDetach() {
  return g_crash_on_process_detach;
}

void SetAbortBehaviorForCrashReporting() {
  // Prevent CRT's abort code from prompting a dialog or trying to "report" it.
  // Disabling the _CALL_REPORTFAULT behavior is important since otherwise it
  // has the sideffect of clearing our exception filter, which means we
  // don't get any crash.
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

  // Set a SIGABRT handler for good measure. We will crash even if the default
  // is left in place, however this allows us to crash earlier. And it also
  // lets us crash in response to code which might directly call raise(SIGABRT)
  signal(SIGABRT, ForceCrashOnSigAbort);
}

bool IsTabletDevice(std::string* reason) {
  if (GetVersion() < Version::WIN8) {
    if (reason)
      *reason = "Tablet device detection not supported below Windows 8\n";
    return false;
  }

  if (GetSystemMetrics(SM_MAXIMUMTOUCHES) == 0) {
    if (reason) {
      *reason += "Device does not support touch.\n";
    } else {
      return false;
    }
  }

  // If the device is docked, the user is treating the device as a PC.
  if (GetSystemMetrics(SM_SYSTEMDOCKED) != 0) {
    if (reason) {
      *reason += "SM_SYSTEMDOCKED\n";
    } else {
      return false;
    }
  }

  // PlatformRoleSlate was added in Windows 8+.
  POWER_PLATFORM_ROLE role = GetPlatformRole();
  bool mobile_power_profile = (role == PlatformRoleMobile);
  bool slate_power_profile = (role == PlatformRoleSlate);

  bool is_tablet = false;

  if (mobile_power_profile || slate_power_profile) {
    is_tablet = !GetSystemMetrics(SM_CONVERTIBLESLATEMODE);
    if (!is_tablet) {
      if (reason) {
        *reason += "Not in slate mode.\n";
      } else {
        return false;
      }
    } else {
      if (reason) {
        *reason += (role == PlatformRoleMobile) ? "PlatformRoleMobile\n" :
                                                  "PlatformRoleSlate\n";
      }
    }
  } else {
    if (reason)
      *reason += "Device role is not mobile or slate.\n";
  }
  return is_tablet;
}

enum DomainEnrollementState {UNKNOWN = -1, NOT_ENROLLED, ENROLLED};
static volatile long int g_domain_state = UNKNOWN;

bool IsEnrolledToDomain() {
  // Doesn't make any sense to retry inside a user session because joining a
  // domain will only kick in on a restart.
  if (g_domain_state == UNKNOWN) {
    LPWSTR domain;
    NETSETUP_JOIN_STATUS join_status;
    if(::NetGetJoinInformation(NULL, &domain, &join_status) != NERR_Success)
      return false;
    ::NetApiBufferFree(domain);
    ::InterlockedCompareExchange(&g_domain_state,
                                 join_status == ::NetSetupDomainName ?
                                     ENROLLED : NOT_ENROLLED,
                                 UNKNOWN);
  }

  return g_domain_state == ENROLLED;
}

void SetDomainStateForTesting(bool state) {
  g_domain_state = state ? ENROLLED : NOT_ENROLLED;
}

bool MaybeHasSHA256Support() {
  const OSInfo* os_info = OSInfo::GetInstance();

  if (os_info->version() == Version::PRE_XP)
    return false;  // Too old to have it and this OS is not supported anyway.

  if (os_info->version() == Version::XP)
    return os_info->service_pack().major >= 3;  // Windows XP SP3 has it.

  // Assume it is missing in this case, although it may not be. This category
  // includes Windows XP x64, and Windows Server, where a hotfix could be
  // deployed.
  if (os_info->version() == Version::SERVER_2003)
    return false;

  assert(os_info->version() >= Version::VISTA);
  return true;  // New enough to have SHA-256 support.
}

}  // namespace win
}  // namespace cr
