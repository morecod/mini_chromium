// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_SYNCHRONIZATION_LOCK_H_
#define MINI_CHROMIUM_SRC_CRBASE_SYNCHRONIZATION_LOCK_H_

#include "crbase/base_export.h"
#include "crbase/logging.h"
#include "crbase/macros.h"
#include "crbase/synchronization/lock_impl.h"
#include "crbase/threading/platform_thread.h"
#include "crbase/build_config.h"

namespace cr {

// A convenient wrapper for an OS specific critical section.  The only real
// intelligence in this class is in debug mode for the support for the
// AssertAcquired() method.
class CRBASE_EXPORT Lock {
 public:
  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;

#if !CR_DCHECK_IS_ON()
   // Optimized wrapper implementation
  Lock() : lock_() {}
  ~Lock() {}
  void Acquire() { lock_.Lock(); }
  void Release() { lock_.Unlock(); }

  // If the lock is not held, take it and return true. If the lock is already
  // held by another thread, immediately return false. This must not be called
  // by a thread already holding the lock (what happens is undefined and an
  // assertion may fail).
  bool Try() { return lock_.Try(); }

  // Null implementation if not debug.
  void AssertAcquired() const {}
#else
  Lock();
  ~Lock();

  // NOTE: Although windows critical sections support recursive locks, we do not
  // allow this, and we will commonly fire a DCHECK() if a thread attempts to
  // acquire the lock a second time (while already holding it).
  void Acquire() {
    lock_.Lock();
    CheckUnheldAndMark();
  }
  void Release() {
    CheckHeldAndUnmark();
    lock_.Unlock();
  }

  bool Try() {
    bool rv = lock_.Try();
    if (rv) {
      CheckUnheldAndMark();
    }
    return rv;
  }

  void AssertAcquired() const;
#endif  // CR_DCHECK_IS_ON()

#if defined(MINI_CHROMIUM_OS_POSIX)
  // The posix implementation of ConditionVariable needs to be able
  // to see our lock and tweak our debugging counters, as it releases
  // and acquires locks inside of pthread_cond_{timed,}wait.
  friend class ConditionVariable;
#elif defined(MINI_CHROMIUM_OS_WIN)
  // The Windows Vista implementation of ConditionVariable needs the
  // native handle of the critical section.
  friend class WinVistaCondVar;
#endif

 private:
#if CR_DCHECK_IS_ON()
  // Members and routines taking care of locks assertions.
  // Note that this checks for recursive locks and allows them
  // if the variable is set.  This is allowed by the underlying implementation
  // on windows but not on Posix, so we're doing unneeded checks on Posix.
  // It's worth it to share the code.
  void CheckHeldAndUnmark();
  void CheckUnheldAndMark();

  // All private data is implicitly protected by lock_.
  // Be VERY careful to only access members under that lock.
  PlatformThreadRef owning_thread_ref_;
#endif  // CR_DCHECK_IS_ON()

  // Platform specific underlying lock implementation.
  internal::LockImpl lock_;
};

// A helper class that acquires the given Lock while the AutoLock is in scope.
class AutoLock {
 public:
  AutoLock(const AutoLock&) = delete;
  AutoLock& operator=(const AutoLock&) = delete;

  struct AlreadyAcquired {};

  explicit AutoLock(Lock& lock) : lock_(lock) {
    lock_.Acquire();
  }

  AutoLock(Lock& lock, const AlreadyAcquired&) : lock_(lock) {
    lock_.AssertAcquired();
  }

  ~AutoLock() {
    lock_.AssertAcquired();
    lock_.Release();
  }

 private:
  Lock& lock_;
};

// AutoUnlock is a helper that will Release() the |lock| argument in the
// constructor, and re-Acquire() it in the destructor.
class AutoUnlock {
 public:
  AutoUnlock(const AutoUnlock&) = delete;
  AutoUnlock& operator=(const AutoUnlock&) = delete;

  explicit AutoUnlock(Lock& lock) : lock_(lock) {
    // We require our caller to have the lock.
    lock_.AssertAcquired();
    lock_.Release();
  }

  ~AutoUnlock() {
    lock_.Acquire();
  }

 private:
  Lock& lock_;
};

}  // namespace cr

#endif  //MINI_CHROMIUM_SRC_CRBASE_SYNCHRONIZATION_LOCK_H_