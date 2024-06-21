// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/strings/utf_string_conversions.h"

#include <stdint.h>

#include "crbase/strings/string_piece.h"
#include "crbase/strings/string_util.h"
#include "crbase/strings/utf_string_conversion_utils.h"
#include "crbase/logging.h"

namespace crbase {
    
namespace {

// Generalized Unicode converter -----------------------------------------------

// Converts the given source Unicode character type to the given destination
// Unicode character type as a STL string. The given input buffer and size
// determine the source, and the given output STL string will be replaced by
// the result.
template<typename SRC_CHAR, typename DEST_STRING>
bool ConvertUnicode(const SRC_CHAR* src,
                    size_t src_len,
                    DEST_STRING* output) {
  // ICU requires 32-bit numbers.
  bool success = true;
  int32_t src_len32 = static_cast<int32_t>(src_len);
  for (int32_t i = 0; i < src_len32; i++) {
    uint32_t code_point;
    if (ReadUnicodeCharacter(src, src_len32, &i, &code_point)) {
      WriteUnicodeCharacter(code_point, output);
    } else {
      WriteUnicodeCharacter(0xFFFD, output);
      success = false;
    }
  }

  return success;
}

}  // namespace

// UTF-8 <-> Wide --------------------------------------------------------------

bool WideToUTF8(const wchar_t* src, size_t src_len, std::string* output) {
  if (IsStringASCII(StringPiece16(src, src_len))) {
    *output = UTF16ToASCII(StringPiece16(src, src_len));
    return true;
  } else {
    PrepareForUTF8Output(src, src_len, output);
    return ConvertUnicode(src, src_len, output);
  }
}

std::string WideToUTF8(const std::wstring& wide) {
  if (IsStringASCII(wide)) 
    return UTF16ToASCII(wide);

  std::string ret;
  PrepareForUTF8Output(wide.data(), wide.length(), &ret);
  ConvertUnicode(wide.data(), wide.length(), &ret);
  return ret;
}

bool UTF8ToWide(const char* src, size_t src_len, std::wstring* output) {
  if (IsStringASCII(StringPiece(src, src_len))) {
    *output = ASCIIToUTF16(StringPiece(src, src_len));
    return true;
  } else {
    PrepareForUTF16Or32Output(src, src_len, output);
    return ConvertUnicode(src, src_len, output);
  }
}

std::wstring UTF8ToWide(StringPiece utf8) {
  if (IsStringASCII(utf8)) {
    return ASCIIToUTF16(utf8);
  }

  std::wstring ret;
  PrepareForUTF16Or32Output(utf8.data(), utf8.length(), &ret);
  ConvertUnicode(utf8.data(), utf8.length(), &ret);
  return ret;
}

// UTF-16 <-> Wide -------------------------------------------------------------

// When wide == UTF-16, then conversions are a NOP.
bool WideToUTF16(const wchar_t* src, size_t src_len, string16* output) {
  output->assign(src, src_len);
  return true;
}

string16 WideToUTF16(const std::wstring& wide) {
  return wide;
}

bool UTF16ToWide(const char16* src, size_t src_len, std::wstring* output) {
  output->assign(src, src_len);
  return true;
}

std::wstring UTF16ToWide(const string16& utf16) {
  return utf16;
}

// UTF16 <-> UTF8 --------------------------------------------------------------

// Easy case since we can use the "wide" versions we already wrote above.

bool UTF8ToUTF16(const char* src, size_t src_len, string16* output) {
  return UTF8ToWide(src, src_len, output);
}

string16 UTF8ToUTF16(StringPiece utf8) {
  return UTF8ToWide(utf8);
}

bool UTF16ToUTF8(const char16* src, size_t src_len, std::string* output) {
  return WideToUTF8(src, src_len, output);
}

std::string UTF16ToUTF8(StringPiece16 utf16) {
  if (IsStringASCII(utf16))
    return UTF16ToASCII(utf16);

  std::string ret;
  PrepareForUTF8Output(utf16.data(), utf16.length(), &ret);
  ConvertUnicode(utf16.data(), utf16.length(), &ret);
  return ret;
}

string16 ASCIIToUTF16(StringPiece ascii) {
  CR_DCHECK(IsStringASCII(ascii)) << ascii;
  string16 ret;
  if (!ascii.empty()) {
    ret.resize(ascii.length());
    for (size_t i = 0; i < ascii.length(); i++)
      ret[i] = static_cast<wchar_t>(ascii[i]);
  }
  return ret;
}

std::string UTF16ToASCII(StringPiece16 utf16) {
  CR_DCHECK(IsStringASCII(utf16)) << UTF16ToUTF8(utf16);
  std::string ret;
  if (!utf16.empty()) {
    ret.resize(utf16.length());
    for (size_t i = 0; i < utf16.length(); i++)
      ret[i] = static_cast<char>(utf16[i]);
  }
  return ret;
}

}  // namespace crbase