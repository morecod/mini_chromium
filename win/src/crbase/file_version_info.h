// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_FILE_VERSION_INFO_H_
#define MINI_CHROMIUM_SRC_CRBASE_FILE_VERSION_INFO_H_

#include <windows.h>
// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

#include <string>
#include <memory>

#include "crbase/base_export.h"
#include "crbase/macros.h"
#include "crbase/strings/string16.h"
#include "crbase/version.h"

namespace crbase {

class FilePath;

// Provides an interface for accessing the version information for a file. This
// is the information you access when you select a file in the Windows Explorer,
// right-click select Properties, then click the Version tab, and on the Mac
// when you select a file in the Finder and do a Get Info.
//
// This list of properties is straight out of Win32's VerQueryValue
// <http://msdn.microsoft.com/en-us/library/ms647464.aspx> and the Mac
// version returns values from the Info.plist as appropriate. TODO(avi): make
// this a less-obvious Windows-ism.

// Creates a FileVersionInfo for the current module. Returns NULL in case of
// error. The returned object should be deleted when you are done with it. This
// is done as a macro to force inlining of __ImageBase. It used to be inside of
// a method labeled with __forceinline, but inlining through __forceinline
// stopped working for Debug builds in VS2013 (http://crbug.com/516359).
#define CR_CREATE_FILE_VERSION_INFO_FOR_CURRENT_MODULE() \
    crbase::FileVersionInfo::CreateFileVersionInfoForModule( \
        reinterpret_cast<HMODULE>(&__ImageBase))

class CRBASE_EXPORT FileVersionInfo {
 public:
  virtual ~FileVersionInfo() {}
  // Creates a FileVersionInfo for the specified path. Returns NULL if something
  // goes wrong (typically the file does not exit or cannot be opened). The
  // returned object should be deleted when you are done with it.
  static std::unique_ptr<FileVersionInfo> CreateFileVersionInfo(
      const FilePath& file_path);

  // Creates a FileVersionInfo for the specified module. Returns NULL in case
  // of error. The returned object should be deleted when you are done with it.
  // See CREATE_FILE_VERSION_INFO_FOR_CURRENT_MODULE() helper above for a
  // CreateFileVersionInfoForCurrentModule() alternative for Windows.
  static std::unique_ptr<FileVersionInfo> CreateFileVersionInfoForModule(HMODULE module);

  // Accessors to the different version properties.
  // Returns an empty string if the property is not found.
  virtual string16 company_name() = 0;
  virtual string16 company_short_name() = 0;
  virtual string16 product_name() = 0;
  virtual string16 product_short_name() = 0;
  virtual string16 internal_name() = 0;
  virtual string16 product_version() = 0;
  virtual string16 private_build() = 0;
  virtual string16 special_build() = 0;
  virtual string16 comments() = 0;
  virtual string16 original_filename() = 0;
  virtual string16 file_description() = 0;
  virtual string16 file_version() = 0;
  virtual string16 legal_copyright() = 0;
  virtual string16 legal_trademarks() = 0;
  virtual string16 last_change() = 0;
  virtual bool is_official_build() = 0;
};

class CRBASE_EXPORT FileVersionInfoWin : public FileVersionInfo {
 public:
  FileVersionInfoWin(const FileVersionInfoWin&) = delete;
  FileVersionInfoWin& operator=(const FileVersionInfoWin&) = delete;

  FileVersionInfoWin(void* data, WORD language, WORD code_page);
  ~FileVersionInfoWin() override;

  // Accessors to the different version properties.
  // Returns an empty string if the property is not found.
  string16 company_name() override;
  string16 company_short_name() override;
  string16 product_name() override;
  string16 product_short_name() override;
  string16 internal_name() override;
  string16 product_version() override;
  string16 private_build() override;
  string16 special_build() override;
  string16 comments() override;
  string16 original_filename() override;
  string16 file_description() override;
  string16 file_version() override;
  string16 legal_copyright() override;
  string16 legal_trademarks() override;
  string16 last_change() override;
  bool is_official_build() override;

  // Lets you access other properties not covered above.
  bool GetValue(const wchar_t* name, std::wstring* value);

  // Similar to GetValue but returns a wstring (empty string if the property
  // does not exist).
  std::wstring GetStringValue(const wchar_t* name);

  // Get file version number in dotted version format.
  crbase::Version GetFileVersion() const;

  // Get the fixed file info if it exists. Otherwise NULL
  VS_FIXEDFILEINFO* fixed_file_info() { return fixed_file_info_; }

  // Behaves like CreateFileVersionInfo, but returns a FileVersionInfoWin.
  static std::unique_ptr<FileVersionInfoWin> CreateFileVersionInfoWin(
      const FilePath& file_path);

 private:
  struct FreeDeleter {
    inline void operator()(void* ptr) const {
      free(ptr);
    }
  };

  std::unique_ptr<char, FileVersionInfoWin::FreeDeleter> data_;
  WORD language_;
  WORD code_page_;

  // This is a pointer into the data_ if it exists. Otherwise NULL.
  VS_FIXEDFILEINFO* fixed_file_info_;
};

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_FILE_VERSION_INFO_H_