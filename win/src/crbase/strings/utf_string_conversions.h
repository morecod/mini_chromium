// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_STRINGS_UTF_STRING_CONVERSIONS_H_
#define MINI_CHROMIUM_SRC_CRBASE_STRINGS_UTF_STRING_CONVERSIONS_H_

#include <stddef.h>

#include <string>

#include "crbase/base_export.h"
#include "crbase/strings/string16.h"
#include "crbase/strings/string_piece.h"

namespace crbase {

// These convert between UTF-8, -16, and -32 strings. They are potentially slow,
// so avoid unnecessary conversions. The low-level versions return a boolean
// indicating whether the conversion was 100% valid. In this case, it will still
// do the best it can and put the result in the output buffer. The versions that
// return strings ignore this error and just return the best conversion
// possible.
CRBASE_EXPORT bool WideToUTF8(const wchar_t* src, size_t src_len,
                              std::string* output);
CRBASE_EXPORT std::string WideToUTF8(const std::wstring& wide);
CRBASE_EXPORT bool UTF8ToWide(const char* src,
                              size_t src_len,
                              std::wstring* output);
CRBASE_EXPORT std::wstring UTF8ToWide(StringPiece utf8);

CRBASE_EXPORT bool WideToUTF16(const wchar_t* src,
                               size_t src_len,
                               string16* output);
CRBASE_EXPORT string16 WideToUTF16(const std::wstring& wide);
CRBASE_EXPORT bool UTF16ToWide(const char16* src,
                               size_t src_len,
                               std::wstring* output);
CRBASE_EXPORT std::wstring UTF16ToWide(const string16& utf16);

CRBASE_EXPORT bool UTF8ToUTF16(const char* src,
                               size_t src_len,
                               string16* output);
CRBASE_EXPORT string16 UTF8ToUTF16(StringPiece utf8);
CRBASE_EXPORT bool UTF16ToUTF8(const char16* src,
                               size_t src_len,
                               std::string* output);
CRBASE_EXPORT std::string UTF16ToUTF8(StringPiece16 utf16);

// This converts an ASCII string, typically a hardcoded constant, to a UTF16
// string.
CRBASE_EXPORT string16 ASCIIToUTF16(StringPiece ascii);

// Converts to 7-bit ASCII by truncating. The result must be known to be ASCII
// beforehand.
CRBASE_EXPORT std::string UTF16ToASCII(StringPiece16 utf16);

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_STRINGS_UTF_STRING_CONVERSIONS_H_