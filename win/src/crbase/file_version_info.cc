// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/file_version_info.h"

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#include "crbase/macros.h"
#include "crbase/logging.h"
#include "crbase/files/file_path.h"

using crbase::FilePath;

namespace crbase {

namespace {

typedef struct {
  WORD language;
  WORD code_page;
} LanguageAndCodePage;

// Returns the \VarFileInfo\Translation value extracted from the
// VS_VERSION_INFO resource in |data|.
LanguageAndCodePage* GetTranslate(const void* data) {
  static constexpr wchar_t kTranslation[] = L"\\VarFileInfo\\Translation";
  LPVOID translate = nullptr;
  UINT dummy_size;
  if (::VerQueryValueW(data, kTranslation, &translate, &dummy_size))
    return static_cast<LanguageAndCodePage*>(translate);
  return nullptr;
}

///const VS_FIXEDFILEINFO& GetVsFixedFileInfo(const void* data) {
///  static const wchar_t kRoot[] = L"\\";
///  LPVOID fixed_file_info = nullptr;
///  UINT dummy_size;
///  CR_CHECK(::VerQueryValueW(data, kRoot, &fixed_file_info, &dummy_size));
///  return *static_cast<VS_FIXEDFILEINFO*>(fixed_file_info);
///}

}  // namespace

FileVersionInfoWin::FileVersionInfoWin(void* data,
                                       WORD language,
                                       WORD code_page)
    : language_(language), code_page_(code_page) {
  data_.reset((char*) data);
  fixed_file_info_ = NULL;
  UINT size;
  ::VerQueryValueW(data_.get(), L"\\", (LPVOID*)&fixed_file_info_, &size);
}

FileVersionInfoWin::~FileVersionInfoWin() {
  CR_DCHECK(data_.get());
}


// static
std::unique_ptr<FileVersionInfo> FileVersionInfo::CreateFileVersionInfoForModule(
    HMODULE module) {
  // Note that the use of MAX_PATH is basically in line with what we do for
  // all registered paths (PathProviderWin).
  wchar_t system_buffer[MAX_PATH];
  system_buffer[0] = 0;
  if (!::GetModuleFileNameW(module, system_buffer, MAX_PATH))
    return NULL;

  FilePath app_path(system_buffer);
  return CreateFileVersionInfo(app_path);
}

// static
std::unique_ptr<FileVersionInfo> FileVersionInfo::CreateFileVersionInfo(
    const FilePath& file_path) {
  return FileVersionInfoWin::CreateFileVersionInfoWin(file_path);
}

string16 FileVersionInfoWin::company_name() {
  return GetStringValue(L"CompanyName");
}

string16 FileVersionInfoWin::company_short_name() {
  return GetStringValue(L"CompanyShortName");
}

string16 FileVersionInfoWin::internal_name() {
  return GetStringValue(L"InternalName");
}

string16 FileVersionInfoWin::product_name() {
  return GetStringValue(L"ProductName");
}

string16 FileVersionInfoWin::product_short_name() {
  return GetStringValue(L"ProductShortName");
}

string16 FileVersionInfoWin::comments() {
  return GetStringValue(L"Comments");
}

string16 FileVersionInfoWin::legal_copyright() {
  return GetStringValue(L"LegalCopyright");
}

string16 FileVersionInfoWin::product_version() {
  return GetStringValue(L"ProductVersion");
}

string16 FileVersionInfoWin::file_description() {
  return GetStringValue(L"FileDescription");
}

string16 FileVersionInfoWin::legal_trademarks() {
  return GetStringValue(L"LegalTrademarks");
}

string16 FileVersionInfoWin::private_build() {
  return GetStringValue(L"PrivateBuild");
}

string16 FileVersionInfoWin::file_version() {
  return GetStringValue(L"FileVersion");
}

string16 FileVersionInfoWin::original_filename() {
  return GetStringValue(L"OriginalFilename");
}

string16 FileVersionInfoWin::special_build() {
  return GetStringValue(L"SpecialBuild");
}

string16 FileVersionInfoWin::last_change() {
  return GetStringValue(L"LastChange");
}

bool FileVersionInfoWin::is_official_build() {
  return (GetStringValue(L"Official Build").compare(L"1") == 0);
}

bool FileVersionInfoWin::GetValue(const wchar_t* name,
                                  std::wstring* value_str) {
  WORD lang_codepage[8];
  size_t i = 0;
  // Use the language and codepage from the DLL.
  lang_codepage[i++] = language_;
  lang_codepage[i++] = code_page_;
  // Use the default language and codepage from the DLL.
  lang_codepage[i++] = ::GetUserDefaultLangID();
  lang_codepage[i++] = code_page_;
  // Use the language from the DLL and Latin codepage (most common).
  lang_codepage[i++] = language_;
  lang_codepage[i++] = 1252;
  // Use the default language and Latin codepage (most common).
  lang_codepage[i++] = ::GetUserDefaultLangID();
  lang_codepage[i++] = 1252;

  i = 0;
  while (i < cr_arraysize(lang_codepage)) {
    wchar_t sub_block[MAX_PATH];
    WORD language = lang_codepage[i++];
    WORD code_page = lang_codepage[i++];
    _snwprintf_s(sub_block, MAX_PATH, MAX_PATH,
                 L"\\StringFileInfo\\%04x%04x\\%ls", language, code_page, name);
    LPVOID value = NULL;
    uint32_t size;
    BOOL r = ::VerQueryValueW(data_.get(), sub_block, &value, &size);
    if (r && value) {
      value_str->assign(static_cast<wchar_t*>(value));
      return true;
    }
  }
  return false;
}

std::wstring FileVersionInfoWin::GetStringValue(const wchar_t* name) {
  std::wstring str;
  GetValue(name, &str);
  return str;
}

crbase::Version FileVersionInfoWin::GetFileVersion() const {
  return crbase::Version({ HIWORD(fixed_file_info_->dwFileVersionMS),
    LOWORD(fixed_file_info_->dwFileVersionMS),
    HIWORD(fixed_file_info_->dwFileVersionLS),
    LOWORD(fixed_file_info_->dwFileVersionLS) });
}

// static
std::unique_ptr<FileVersionInfoWin>
FileVersionInfoWin::CreateFileVersionInfoWin(const FilePath& file_path) {
  DWORD dummy;
  const wchar_t* path = file_path.value().c_str();
  const DWORD length = ::GetFileVersionInfoSizeW(path, &dummy);
  if (length == 0)
    return nullptr;

  void* data = calloc(length, 1);
  if (!data)
    return NULL;

  if (!::GetFileVersionInfoW(path, dummy, length, data)) {
    free(data);
    return nullptr;
  }

  const LanguageAndCodePage* translate = GetTranslate(data);
  if (!translate) {
    free(data);
    return nullptr;
  }

  return std::unique_ptr<FileVersionInfoWin>(new FileVersionInfoWin(
    data, translate->language, translate->code_page));
}

}  // namesapce crbase
