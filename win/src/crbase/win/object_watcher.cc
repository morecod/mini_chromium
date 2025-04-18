// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/win/object_watcher.h"

#include "crbase/functional/bind.h"
#include "crbase/logging.h"

namespace cr {
namespace win {

//-----------------------------------------------------------------------------

ObjectWatcher::ObjectWatcher()
    : object_(NULL),
      wait_object_(NULL),
      origin_loop_(NULL),
      run_once_(true),
      weak_factory_(this) {
}

ObjectWatcher::~ObjectWatcher() {
  StopWatching();
}

bool ObjectWatcher::StartWatchingOnce(HANDLE object, Delegate* delegate) {
  return StartWatchingInternal(object, delegate, true);
}

bool ObjectWatcher::StartWatchingMultipleTimes(HANDLE object,
                                               Delegate* delegate) {
  return StartWatchingInternal(object, delegate, false);
}

bool ObjectWatcher::StopWatching() {
  if (!wait_object_)
    return false;

  // Make sure ObjectWatcher is used in a single-threaded fashion.
  CR_DCHECK_EQ(origin_loop_, MessageLoop::current());

  // Blocking call to cancel the wait. Any callbacks already in progress will
  // finish before we return from this call.
  if (!UnregisterWaitEx(wait_object_, INVALID_HANDLE_VALUE)) {
    CR_DPLOG(FATAL) << "UnregisterWaitEx failed";
    return false;
  }

  weak_factory_.InvalidateWeakPtrs();
  object_ = NULL;
  wait_object_ = NULL;

  MessageLoop::current()->RemoveDestructionObserver(this);
  return true;
}

bool ObjectWatcher::IsWatching() const {
  return object_ != NULL;
}

HANDLE ObjectWatcher::GetWatchedObject() const {
  return object_;
}

// static
void CALLBACK ObjectWatcher::DoneWaiting(void* param, BOOLEAN timed_out) {
  CR_DCHECK(!timed_out);

  // The destructor blocks on any callbacks that are in flight, so we know that
  // that is always a pointer to a valid ObjectWater.
  ObjectWatcher* that = static_cast<ObjectWatcher*>(param);

  // `that` must not be touched once `PostTask` returns since the callback
  // could delete the instance on another thread.
  SingleThreadTaskRunner* const task_runner = 
      that->origin_loop_->task_runner().get();

  if (that->run_once_)
    task_runner->PostTask(CR_FROM_HERE, std::move(that->callback_));
  else
    task_runner->PostTask(CR_FROM_HERE, that->callback_);
}

bool ObjectWatcher::StartWatchingInternal(HANDLE object, Delegate* delegate,
                                          bool execute_only_once) {
  CR_CHECK(delegate);
  if (wait_object_) {
    CR_NOTREACHED() << "Already watching an object";
    return false;
  }
  run_once_ = execute_only_once;

  // Since our job is to just notice when an object is signaled and report the
  // result back to this thread, we can just run on a Windows wait thread.
  DWORD wait_flags = WT_EXECUTEINWAITTHREAD;
  if (run_once_)
    wait_flags |= WT_EXECUTEONLYONCE;

  // DoneWaiting can be synchronously called from RegisterWaitForSingleObject,
  // so set up all state now.
  callback_ = cr::BindRepeating(
      &ObjectWatcher::Signal, weak_factory_.GetWeakPtr(), delegate);
  object_ = object;
  origin_loop_ = MessageLoop::current();

  if (!RegisterWaitForSingleObject(&wait_object_, object, DoneWaiting,
                                   this, INFINITE, wait_flags)) {
    CR_DPLOG(FATAL) << "RegisterWaitForSingleObject failed";
    object_ = NULL;
    wait_object_ = NULL;
    return false;
  }

  // We need to know if the current message loop is going away so we can
  // prevent the wait thread from trying to access a dead message loop.
  MessageLoop::current()->AddDestructionObserver(this);
  return true;
}

void ObjectWatcher::Signal(Delegate* delegate) {
  // Signaling the delegate may result in our destruction or a nested call to
  // StartWatching(). As a result, we save any state we need and clear previous
  // watcher state before signaling the delegate.
  HANDLE object = object_;
  if (run_once_)
    StopWatching();
  delegate->OnObjectSignaled(object);
}

void ObjectWatcher::WillDestroyCurrentMessageLoop() {
  // Need to shutdown the watch so that we don't try to access the MessageLoop
  // after this point.
  StopWatching();
}

}  // namespace win
}  // namespace cr