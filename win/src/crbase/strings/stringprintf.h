// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_STRINGS_STRINGPRINTF_H_
#define MINI_CHROMIUM_SRC_CRBASE_STRINGS_STRINGPRINTF_H_

#include <stdarg.h>   // va_list
#include <sal.h>  // _Printf_format_string_

#include <string>

#include "crbase/base_export.h"

namespace crbase {

// Return a C++ string given printf-like input.
CRBASE_EXPORT std::string StringPrintf(_Printf_format_string_ const char* format,
                                     ...);
CRBASE_EXPORT std::wstring StringPrintf(
    _Printf_format_string_ const wchar_t* format,
    ...);

// Return a C++ string given vprintf-like input.
CRBASE_EXPORT std::string StringPrintV(const char* format, va_list ap);

// Store result into a supplied string and return it.
CRBASE_EXPORT const std::string& SStringPrintf(
    std::string* dst,
    _Printf_format_string_ const char* format,
    ...);

CRBASE_EXPORT const std::wstring& SStringPrintf(
    std::wstring* dst,
    _Printf_format_string_ const wchar_t* format,
    ...);

// Append result to a supplied string.
CRBASE_EXPORT void StringAppendF(std::string* dst,
                                 _Printf_format_string_ const char* format,
                                 ...);

CRBASE_EXPORT void StringAppendF(std::wstring* dst,
                                 _Printf_format_string_ const wchar_t* format,
                                 ...);

// Lower-level routine that takes a va_list and appends to a specified
// string.  All other routines are just convenience wrappers around it.
CRBASE_EXPORT void StringAppendV(std::string* dst, const char* format,
                               va_list ap);

CRBASE_EXPORT void StringAppendV(std::wstring* dst,
                                 const wchar_t* format, va_list ap);

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_STRINGS_STRINGPRINTF_H_
