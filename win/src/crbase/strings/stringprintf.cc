// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/strings/stringprintf.h"

#include <errno.h>
#include <stddef.h>

#include <vector>

#include "crbase/macros.h"
#include "crbase/strings/string_util.h"
#include "crbase/strings/utf_string_conversions.h"

namespace cr {

namespace {

// Overloaded wrappers around vsnprintf and vswprintf. The buf_size parameter
// is the size of the buffer. These return the number of characters in the
// formatted string excluding the NUL terminator. If the buffer is not
// large enough to accommodate the formatted string without truncation, they
// return the number of characters that would be in the fully-formatted string
// (vsnprintf, and vswprintf on Windows), or -1 (vswprintf on POSIX platforms).
inline int vsnprintfT(char* buffer,
                      size_t buf_size,
                      const char* format,
                      va_list argptr) {
  return cr::vsnprintf(buffer, buf_size, format, argptr);
}

inline int vsnprintfT(wchar_t* buffer,
                      size_t buf_size,
                      const wchar_t* format,
                      va_list argptr) {
  return cr::vswprintf(buffer, buf_size, format, argptr);
}

// Templatized backend for StringPrintF/StringAppendF. This does not finalize
// the va_list, the caller is expected to do that.
template <class StringType>
static void StringAppendVT(StringType* dst,
                           const typename StringType::value_type* format,
                           va_list ap) {
  // First try with a small fixed size buffer.
  // This buffer size should be kept in sync with StringUtilTest.GrowBoundary
  // and StringUtilTest.StringPrintfBounds.
  typename StringType::value_type stack_buf[1024];

  va_list ap_copy;
  va_copy(ap_copy, ap);

  int result = vsnprintfT(stack_buf, cr_arraysize(stack_buf), format, ap_copy);
  va_end(ap_copy);

  if (result >= 0 && result < static_cast<int>(cr_arraysize(stack_buf))) {
    // It fit.
    dst->append(stack_buf, result);
    return;
  }

  // Repeatedly increase buffer size until it fits.
  int mem_length = cr_arraysize(stack_buf);
  while (true) {
    if (result < 0) {
      // On Windows, vsnprintfT always returns the number of characters in a
      // fully-formatted string, so if we reach this point, something else is
      // wrong and no amount of buffer-doubling is going to fix it.
      return;
    } else {
      // We need exactly "result + 1" characters.
      mem_length = result + 1;
    }

    if (mem_length > 32 * 1024 * 1024) {
      // That should be plenty, don't try anything larger.  This protects
      // against huge allocations when using vsnprintfT implementations that
      // return -1 for reasons other than overflow without setting errno.
      return;
    }

    std::vector<typename StringType::value_type> mem_buf(mem_length);

    // NOTE: You can only use a va_list once.  Since we're in a while loop, we
    // need to make a new copy each time so we don't use up the original.
    va_copy(ap_copy, ap);
    result = vsnprintfT(&mem_buf[0], mem_length, format, ap_copy);
    va_end(ap_copy);

    if ((result >= 0) && (result < mem_length)) {
      // It fit.
      dst->append(&mem_buf[0], result);
      return;
    }
  }
}

}  // namespace

std::string StringPrintf(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::string result;
  StringAppendV(&result, format, ap);
  va_end(ap);
  return result;
}

std::wstring StringPrintf(const wchar_t* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::wstring result;
  StringAppendV(&result, format, ap);
  va_end(ap);
  return result;
}

std::string StringPrintV(const char* format, va_list ap) {
  std::string result;
  StringAppendV(&result, format, ap);
  return result;
}

const std::string& SStringPrintf(std::string* dst, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  dst->clear();
  StringAppendV(dst, format, ap);
  va_end(ap);
  return *dst;
}

const std::wstring& SStringPrintf(std::wstring* dst,
                                  const wchar_t* format, ...) {
  va_list ap;
  va_start(ap, format);
  dst->clear();
  StringAppendV(dst, format, ap);
  va_end(ap);
  return *dst;
}

void StringAppendF(std::string* dst, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  StringAppendV(dst, format, ap);
  va_end(ap);
}

void StringAppendF(std::wstring* dst, const wchar_t* format, ...) {
  va_list ap;
  va_start(ap, format);
  StringAppendV(dst, format, ap);
  va_end(ap);
}

void StringAppendV(std::string* dst, const char* format, va_list ap) {
  StringAppendVT(dst, format, ap);
}

void StringAppendV(std::wstring* dst, const wchar_t* format, va_list ap) {
  StringAppendVT(dst, format, ap);
}

}  // namespace cr
