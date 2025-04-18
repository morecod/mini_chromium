// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_SYNCHRONIZATION_WAITABLE_EVENT_WATCHER_H_
#define MINI_CHROMIUM_SRC_CRBASE_SYNCHRONIZATION_WAITABLE_EVENT_WATCHER_H_

#include "crbase/base_export.h"
#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_OS_WIN)
#include "crbase/win/object_watcher.h"
#else
#include "crbase/functional/callback.h"
#include "crbase/message_loop/message_loop.h"
#include "crbase/synchronization/waitable_event.h"
#endif

namespace cr {

class Flag;
class AsyncWaiter;
class AsyncCallbackTask;
class WaitableEvent;

// This class provides a way to wait on a WaitableEvent asynchronously.
//
// Each instance of this object can be waiting on a single WaitableEvent. When
// the waitable event is signaled, a callback is invoked on the sequence that
// called StartWatching(). This callback can be deleted by deleting the waiter.
//
// Typical usage:
//
//   class MyClass {
//    public:
//     void DoStuffWhenSignaled(WaitableEvent *waitable_event) {
//       watcher_.StartWatching(waitable_event,
//           base::BindOnce(&MyClass::OnWaitableEventSignaled, this);
//     }
//    private:
//     void OnWaitableEventSignaled(WaitableEvent* waitable_event) {
//       // OK, time to do stuff!
//     }
//     base::WaitableEventWatcher watcher_;
//   };
//
// In the above example, MyClass wants to "do stuff" when waitable_event
// becomes signaled. WaitableEventWatcher makes this task easy. When MyClass
// goes out of scope, the watcher_ will be destroyed, and there is no need to
// worry about OnWaitableEventSignaled being called on a deleted MyClass
// pointer.
//
// BEWARE: With automatically reset WaitableEvents, a signal may be lost if it
// occurs just before a WaitableEventWatcher is deleted. There is currently no
// safe way to stop watching an automatic reset WaitableEvent without possibly
// missing a signal.
//
// NOTE: you /are/ allowed to delete the WaitableEvent while still waiting on
// it with a Watcher. But pay attention: if the event was signaled and deleted
// right after, the callback may be called with deleted WaitableEvent pointer.

class CRBASE_EXPORT WaitableEventWatcher
#if defined(MINI_CHROMIUM_OS_WIN)
    : public win::ObjectWatcher::Delegate
#endif
{
 public:
  using EventCallback = OnceCallback<void(WaitableEvent*)>;

  WaitableEventWatcher(const WaitableEventWatcher&) = delete;
  WaitableEventWatcher& operator=(const WaitableEventWatcher&) = delete;

  WaitableEventWatcher();

#if defined(MINI_CHROMIUM_OS_WIN)
  ~WaitableEventWatcher() override;
#else
  ~WaitableEventWatcher();
#endif

  // When |event| is signaled, |callback| is called on the sequence that called
  // StartWatching().
  bool StartWatching(WaitableEvent* event, EventCallback callback);

  // Cancel the current watch. Must be called from the same sequence which
  // started the watch.
  //
  // Does nothing if no event is being watched, nor if the watch has completed.
  // The callback will *not* be called for the current watch after this
  // function returns. Since the callback runs on the same sequence as this
  // function, it cannot be called during this function either.
  void StopWatching();

 private:
#if defined(MINI_CHROMIUM_OS_WIN)
  void OnObjectSignaled(HANDLE h) override;

  // Duplicated handle of the event passed to StartWatching().
  win::ScopedHandle duplicated_event_handle_;

  // A watcher for |duplicated_event_handle_|. The handle MUST outlive
  // |watcher_|.
  win::ObjectWatcher watcher_;

  EventCallback callback_;
  WaitableEvent* event_ = nullptr;
#else
  // Instantiated in StartWatching(). Set before the callback runs. Reset in
  // StopWatching() or StartWatching().
  scoped_refptr<Flag> cancel_flag_;

  // Enqueued in the wait list of the watched WaitableEvent.
  AsyncWaiter* waiter_ = nullptr;

  // Kernel of the watched WaitableEvent.
  scoped_refptr<WaitableEvent::WaitableEventKernel> kernel_;

  // Ensures that StartWatching() and StopWatching() are called on the same
  // sequence.
  SequenceChecker sequence_checker_;
#endif

  /// DISALLOW_COPY_AND_ASSIGN(WaitableEventWatcher);
};
}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_SYNCHRONIZATION_WAITABLE_EVENT_WATCHER_H_