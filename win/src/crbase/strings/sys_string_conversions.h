// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_STRINGS_SYS_STRING_CONVERSIONS_H_
#define MINI_CHROMIUM_SRC_CRBASE_STRINGS_SYS_STRING_CONVERSIONS_H_

// Provides system-dependent string type conversions for cases where it's
// necessary to not use ICU. Generally, you should not need this in Chrome,
// but it is used in some shared code. Dependencies should be minimal.

#include <stdint.h>

#include <string>

#include "crbase/base_export.h"
#include "crbase/strings/string16.h"
#include "crbase/strings/string_piece.h"
#include "crbase/build_config.h"

namespace crbase {

// Converts between wide and UTF-8 representations of a string. On error, the
// result is system-dependent.
CRBASE_EXPORT std::string SysWideToUTF8(const StringPiece16& wide);
CRBASE_EXPORT std::wstring SysUTF8ToWide(const StringPiece& utf8);

// Converts between wide and the system multi-byte representations of a string.
// DANGER: This will lose information and can change (on Windows, this can
// change between reboots).
CRBASE_EXPORT std::string SysWideToNativeMB(const StringPiece16& wide);
CRBASE_EXPORT std::wstring SysNativeMBToWide(const StringPiece& native_mb);

// Windows-specific ------------------------------------------------------------

#if defined(MINI_CHROMIUM_OS_WIN)

// Converts between 8-bit and wide strings, using the given code page. The
// code page identifier is one accepted by the Windows function
// MultiByteToWideChar().
CRBASE_EXPORT std::wstring SysMultiByteToWide(const StringPiece& mb,
                                              uint32_t code_page);
CRBASE_EXPORT std::string SysWideToMultiByte(const StringPiece16& wide,
                                             uint32_t code_page);
#endif 

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_STRINGS_SYS_STRING_CONVERSIONS_H_