// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_WIN_STARTUP_INFORMATION_H_
#define MINI_CHROMIUM_SRC_CRBASE_WIN_STARTUP_INFORMATION_H_

#include <windows.h>
#include <stddef.h>

#include "crbase/base_export.h"
#include "crbase/macros.h"

namespace cr {
namespace win {

// Manages the lifetime of additional attributes in STARTUPINFOEX.
class CRBASE_EXPORT StartupInformation {
 public:
  StartupInformation(const StartupInformation&) = delete;
  StartupInformation& operator=(const StartupInformation&) = delete;
  StartupInformation();

  ~StartupInformation();

  // Initialize the attribute list for the specified number of entries.
  bool InitializeProcThreadAttributeList(DWORD attribute_count);

  // Sets one entry in the initialized attribute list.
  // |value| needs to live at least as long as the StartupInformation object
  // this is called on.
  bool UpdateProcThreadAttribute(DWORD_PTR attribute,
                                 void* value,
                                 size_t size);

  LPSTARTUPINFOW startup_info() { return &startup_info_.StartupInfo; }
  LPSTARTUPINFOW startup_info() const {
    return const_cast<const LPSTARTUPINFOW>(&startup_info_.StartupInfo);
  }

  bool has_extended_startup_info() const {
    return !!startup_info_.lpAttributeList;
  }

 private:
  STARTUPINFOEXW startup_info_;
};

}  // namespace win
}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_WIN_STARTUP_INFORMATION_H_
