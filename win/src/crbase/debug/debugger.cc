// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/debug/debugger.h"
#include "crbase/logging.h"
#include "crbase/threading/platform_thread.h"

namespace cr {
namespace debug {

static bool is_debug_ui_suppressed = false;

bool WaitForDebugger(int wait_seconds, bool silent) {
  for (int i = 0; i < wait_seconds * 10; ++i) {
    if (BeingDebugged()) {
      if (!silent)
        BreakDebugger();
      return true;
    }
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
  }
  return false;
}

}  // namespace debug
}  // namespace cr
