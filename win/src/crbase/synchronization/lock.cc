// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used for debugging assertion support.  The Lock class
// is functionally a wrapper around the LockImpl class, so the only
// real intelligence in the class is in the debugging logic.

#include "crbase/synchronization/lock.h"

#if CR_DCHECK_IS_ON()

namespace cr {

Lock::Lock() : lock_() {
}

Lock::~Lock() {
  CR_DCHECK(owning_thread_ref_.is_null());
}

void Lock::AssertAcquired() const {
  CR_DCHECK(owning_thread_ref_ == PlatformThread::CurrentRef());
}

void Lock::CheckHeldAndUnmark() {
  CR_DCHECK(owning_thread_ref_ == PlatformThread::CurrentRef());
  owning_thread_ref_ = PlatformThreadRef();
}

void Lock::CheckUnheldAndMark() {
  CR_DCHECK(owning_thread_ref_.is_null());
  owning_thread_ref_ = PlatformThread::CurrentRef();
}

}  // namespace cr

#endif  // CR_DCHECK_IS_ON()