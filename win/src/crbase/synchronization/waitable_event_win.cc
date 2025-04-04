// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/synchronization/waitable_event.h"

#include <windows.h>
#include <stddef.h>

#include "crbase/logging.h"
#include "crbase/numerics/safe_conversions.h"
#include "crbase/time/time.h"
///#include "crbase/threading/thread_restrictions.h"

namespace cr {

WaitableEvent::WaitableEvent(bool manual_reset, bool signaled)
    : handle_(CreateEvent(NULL, manual_reset, signaled, NULL)) {
  // We're probably going to crash anyways if this is ever NULL, so we might as
  // well make our stack reports more informative by crashing here.
  CR_CHECK(handle_.IsValid());
}

WaitableEvent::WaitableEvent(win::ScopedHandle handle)
    : handle_(handle.Pass()) {
  CR_CHECK(handle_.IsValid()) 
      << "Tried to create WaitableEvent from NULL handle";
}

WaitableEvent::~WaitableEvent() {
}

void WaitableEvent::Reset() {
  ::ResetEvent(handle_.Get());
}

void WaitableEvent::Signal() {
  ::SetEvent(handle_.Get());
}

bool WaitableEvent::IsSignaled() {
  return TimedWait(TimeDelta());
}

void WaitableEvent::Wait() {
  ///cr::ThreadRestrictions::AssertWaitAllowed();
  DWORD result = ::WaitForSingleObject(handle_.Get(), INFINITE);
  // It is most unexpected that this should ever fail.  Help consumers learn
  // about it if it should ever fail.
  CR_DCHECK_EQ(WAIT_OBJECT_0, result) << "WaitForSingleObject failed";
}

bool WaitableEvent::TimedWait(const TimeDelta& max_time) {
  ///cr::ThreadRestrictions::AssertWaitAllowed();
  CR_DCHECK_GE(max_time, TimeDelta());
  // Truncate the timeout to milliseconds. The API specifies that this method
  // can return in less than |max_time| (when returning false), as the argument
  // is the maximum time that a caller is willing to wait.
  DWORD timeout = saturated_cast<DWORD>(max_time.InMilliseconds());

  DWORD result = WaitForSingleObject(handle_.Get(), timeout);
  switch (result) {
    case WAIT_OBJECT_0:
      return true;
    case WAIT_TIMEOUT:
      return false;
  }
  // It is most unexpected that this should ever fail.  Help consumers learn
  // about it if it should ever fail.
  CR_NOTREACHED() << "WaitForSingleObject failed";
  return false;
}

// static
size_t WaitableEvent::WaitMany(WaitableEvent** events, size_t count) {
  ///cr::ThreadRestrictions::AssertWaitAllowed();
  HANDLE handles[MAXIMUM_WAIT_OBJECTS];
  CR_CHECK_LE(count, static_cast<size_t>(MAXIMUM_WAIT_OBJECTS))
      << "Can only wait on " << MAXIMUM_WAIT_OBJECTS << " with WaitMany";

  for (size_t i = 0; i < count; ++i)
    handles[i] = events[i]->handle();

  // The cast is safe because count is small - see the CHECK above.
  DWORD result =
      ::WaitForMultipleObjects(static_cast<DWORD>(count),
                               handles,
                               FALSE,      // don't wait for all the objects
                               INFINITE);  // no timeout
  if (result >= WAIT_OBJECT_0 + count) {
    CR_DPLOG(FATAL) << "WaitForMultipleObjects failed";
    return 0;
  }

  return result - WAIT_OBJECT_0;
}

}  // namespace cr
