// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINLIB_WINBASE_STRINGS_STRING16_H_
#define WINLIB_WINBASE_STRINGS_STRING16_H_

// WHAT:
// A version of std::basic_string that provides 2-byte characters even when
// wchar_t is not implemented as a 2-byte type. You can access this class as
// string16. We also define char16, which string16 is based upon.
//
// WHY:
// On Windows, wchar_t is 2 bytes, and it can conveniently handle UTF-16/UCS-2
// data. Plenty of existing code operates on strings encoded as UTF-16.
//
// On many other platforms, sizeof(wchar_t) is 4 bytes by default. We can make
// it 2 bytes by using the GCC flag -fshort-wchar. But then std::wstring fails
// at run time, because it calls some functions (like wcslen) that come from
// the system's native C library -- which was built with a 4-byte wchar_t!
// It's wasteful to use 4-byte wchar_t strings to carry UTF-16 data, and it's
// entirely improper on those systems where the encoding of wchar_t is defined
// as UTF-32.
//
// Here, we define string16, which is similar to std::wstring but replaces all
// libc functions with custom, 2-byte-char compatible routines. It is capable
// of carrying UTF-16-encoded data.

#include <string>

#include "winbase\base_export.h"

namespace winbase {

using char16 = wchar_t;
///using char16_traits = std::char_traits<wchar_t>;
using string16 = std::wstring;

}  // namespace winbase

#endif  // WINLIB_WINBASE_STRINGS_STRING16_H_