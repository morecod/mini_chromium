// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_WIN_SCOPED_SC_HANDLE_H_
#define MINI_CHROMIUM_SRC_CRBASE_WIN_SCOPED_SC_HANDLE_H_

#include <windows.h>

#include "crbase/win/scoped_handle.h"
#include "crbase/macros.h"

namespace crbase {
namespace win {

class ScHandleTraits {
 public:
  typedef SC_HANDLE Handle;

  ScHandleTraits() = delete;
  ScHandleTraits(const ScHandleTraits&) = delete;
  ScHandleTraits& operator=(const ScHandleTraits&) = delete;

  static bool CloseHandle(SC_HANDLE handle) {
    return ::CloseServiceHandle(handle) != FALSE;
  }

  static bool IsHandleValid(SC_HANDLE handle) { return handle != nullptr; }
  static SC_HANDLE NullHandle() { return nullptr; }
};

typedef GenericScopedHandle<ScHandleTraits, DummyVerifierTraits> ScopedScHandle;

class TimerTraits {
 public:
  using Handle = HANDLE;

  TimerTraits() = delete;
  TimerTraits(const TimerTraits&) = delete;
  TimerTraits& operator=(const TimerTraits&) = delete;

  static bool CloseHandle(HANDLE handle) {
    return ::DeleteTimerQueue(handle) != FALSE;
  }

  static bool IsHandleValid(HANDLE handle) { return handle != nullptr; }
  static HANDLE NullHandle() { return nullptr; }
};

typedef GenericScopedHandle<TimerTraits, DummyVerifierTraits> ScopedTimerHandle;

}  // namespace win
}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_WIN_SCOPED_SC_HANDLE_H_