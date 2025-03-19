// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/synchronization/waitable_event_watcher.h"

#include "crbase/synchronization/waitable_event.h"
#include "crbase/win/object_watcher.h"
#include "crbase/logging.h"

namespace crbase {

WaitableEventWatcher::WaitableEventWatcher()
    : event_(NULL) {
}

WaitableEventWatcher::~WaitableEventWatcher() {
}

bool WaitableEventWatcher::StartWatching(
    WaitableEvent* event,
    const EventCallback& callback) {
  callback_ = callback;
  event_ = event;
  return watcher_.StartWatchingOnce(event->handle(), this);
}

void WaitableEventWatcher::StopWatching() {
  callback_.Reset();
  event_ = NULL;
  watcher_.StopWatching();
}

WaitableEvent* WaitableEventWatcher::GetWatchedEvent() {
  return event_;
}

void WaitableEventWatcher::OnObjectSignaled(HANDLE h) {
  WaitableEvent* event = event_;
  EventCallback callback = callback_;
  event_ = NULL;
  callback_.Reset();
  CR_DCHECK(event);

  callback.Run(event);
}

}  // namespace crbase