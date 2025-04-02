// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/debug/debugger.h"

#include <stdlib.h>
#include <windows.h>

namespace cr {
namespace debug {

bool BeingDebugged() {
  return ::IsDebuggerPresent() != 0;
}

void BreakDebugger() {
  __debugbreak();
}

}  // namespace debug
}  // namespace cr
