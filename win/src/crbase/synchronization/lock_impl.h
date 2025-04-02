// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_SYNCHRONIZATION_LOCK_IMPL_H_
#define MINI_CHROMIUM_SRC_CRBASE_SYNCHRONIZATION_LOCK_IMPL_H_

#include "crbase/base_export.h"
#include "crbase/macros.h"
#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_OS_WIN)
#include <windows.h>
#elif defined(MINI_CHROMIUM_OS_POSIX)
#include <pthread.h>
#endif

namespace cr {
namespace internal {

// This class implements the underlying platform-specific spin-lock mechanism
// used for the Lock class.  Most users should not use LockImpl directly, but
// should instead use Lock.
class CRBASE_EXPORT LockImpl {
 public:
  typedef CRITICAL_SECTION NativeHandle;

  LockImpl(const LockImpl&) = delete;
  LockImpl& operator=(const LockImpl&) = delete;

  LockImpl();
  ~LockImpl();

  // If the lock is not held, take it and return true.  If the lock is already
  // held by something else, immediately return false.
  bool Try();

  // Take the lock, blocking until it is available if necessary.
  void Lock();

  // Release the lock.  This must only be called by the lock's holder: after
  // a successful call to Try, or a call to Lock.
  void Unlock();

  // Return the native underlying lock.
  // TODO(awalker): refactor lock and condition variables so that this is
  // unnecessary.
  NativeHandle* native_handle() { return &native_handle_; }

 private:
  NativeHandle native_handle_;
};

}  // namespace internal
}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_SYNCHRONIZATION_LOCK_IMPL_H_