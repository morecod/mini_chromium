// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/debug/alias.h"

namespace cr {
namespace debug {

#if defined(_MSC_VER)
#pragma optimize("", off)
#endif

void Alias(const void* var) {
}

#if defined(_MSC_VER)
#pragma optimize("", on)
#endif

}  // namespace debug
}  // namespace cr
