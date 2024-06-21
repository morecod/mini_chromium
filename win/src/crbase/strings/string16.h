// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_STRINGS_STRING16_H_
#define MINI_CHROMIUM_SRC_CRBASE_STRINGS_STRING16_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string>

namespace crbase {

typedef wchar_t char16;
typedef std::wstring string16;
typedef std::char_traits<wchar_t> string16_char_traits;

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_STRINGS_STRING16_H_