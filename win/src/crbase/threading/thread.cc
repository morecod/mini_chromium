// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/threading/thread.h"

#include "crbase/functional/bind.h"
#include "crbase/lazy_instance.h"
#include "crbase/tracing/location.h"
#include "crbase/synchronization/waitable_event.h"
#include "crbase/threading/thread_id_name_manager.h"
#include "crbase/threading/thread_local.h"
#include "crbase/threading/thread_restrictions.h"
#include "crbase/win/scoped_com_initializer.h"

namespace cr {

namespace {

// We use this thread-local variable to record whether or not a thread exited
// because its Stop method was called.  This allows us to catch cases where
// MessageLoop::QuitWhenIdle() is called directly, which is unexpected when
// using a Thread to setup and run a MessageLoop.
cr::LazyInstance<cr::ThreadLocalBoolean>::Leaky lazy_tls_bool =
    CR_LAZY_INSTANCE_INITIALIZER;

}  // namespace

// This is used to trigger the message loop to exit.
void ThreadQuitHelper() {
  MessageLoop::current()->QuitWhenIdle();
  Thread::SetThreadWasQuitProperly(true);
}

Thread::Options::Options()
    : message_loop_type(MessageLoop::TYPE_DEFAULT),
      timer_slack(TIMER_SLACK_NONE),
      stack_size(0),
      priority(ThreadPriority::NORMAL) {
}

Thread::Options::Options(MessageLoop::Type type,
                         size_t size)
    : message_loop_type(type),
      timer_slack(TIMER_SLACK_NONE),
      stack_size(size),
      priority(ThreadPriority::NORMAL) {
}

Thread::Options::~Options() {
}

Thread::Thread(const std::string& name)
    :
      com_status_(NONE),
      stopping_(false),
      running_(false),
      thread_(0),
      id_(kInvalidThreadId),
      id_event_(true, false),
      message_loop_(nullptr),
      message_loop_timer_slack_(TIMER_SLACK_NONE),
      name_(name),
      start_event_(false, false) {
}

Thread::~Thread() {
  Stop();
}

bool Thread::Start() {
  Options options;
  if (com_status_ == STA)
    options.message_loop_type = MessageLoop::TYPE_UI;
  return StartWithOptions(options);
}

bool Thread::StartWithOptions(const Options& options) {
  CR_DCHECK(!message_loop_);
  CR_DCHECK((com_status_ != STA) ||
              (options.message_loop_type == MessageLoop::TYPE_UI));

  // Reset |id_| here to support restarting the thread.
  id_event_.Reset();
  id_ = kInvalidThreadId;

  SetThreadWasQuitProperly(false);

  MessageLoop::Type type = options.message_loop_type;
  if (!options.message_pump_factory.is_null())
    type = MessageLoop::TYPE_CUSTOM;

  message_loop_timer_slack_ = options.timer_slack;
  std::unique_ptr<MessageLoop> message_loop = MessageLoop::CreateUnbound(
      type, options.message_pump_factory);
  message_loop_ = message_loop.get();
  start_event_.Reset();

  // Hold the thread_lock_ while starting a new thread, so that we can make sure
  // that thread_ is populated before the newly created thread accesses it.
  {
    AutoLock lock(thread_lock_);
    if (!PlatformThread::CreateWithPriority(options.stack_size, this, &thread_,
                                            options.priority)) {
      CR_DLOG(ERROR) << "failed to create thread";
      message_loop_ = nullptr;
      return false;
    }
  }

  // The ownership of message_loop is managemed by the newly created thread
  // within the ThreadMain.
  message_loop.release();

  CR_DCHECK(message_loop_);
  return true;
}

bool Thread::StartAndWaitForTesting() {
  bool result = Start();
  if (!result)
    return false;
  WaitUntilThreadStarted();
  return true;
}

bool Thread::WaitUntilThreadStarted() const {
  if (!message_loop_)
    return false;
  cr::ThreadRestrictions::ScopedAllowWait allow_wait;
  start_event_.Wait();
  return true;
}

void Thread::Stop() {
  AutoLock lock(thread_lock_);
  if (thread_.is_null())
    return;

  StopSoon();

  // Wait for the thread to exit.
  //
  // TODO(darin): Unfortunately, we need to keep message_loop_ around until
  // the thread exits.  Some consumers are abusing the API.  Make them stop.
  //
  PlatformThread::Join(thread_);
  thread_ = PlatformThreadHandle();

  // The thread should nullify message_loop_ on exit.
  CR_DCHECK(!message_loop_);

  stopping_ = false;
}

void Thread::StopSoon() {
  // We should only be called on the same thread that started us.

  CR_DCHECK_NE(GetThreadId(), PlatformThread::CurrentId());

  if (stopping_ || !message_loop_)
    return;

  stopping_ = true;
  task_runner()->PostTask(CR_FROM_HERE, cr::BindOnce(&ThreadQuitHelper));
}

PlatformThreadId Thread::GetThreadId() const {
  // If the thread is created but not started yet, wait for |id_| being ready.
  ///cr::ThreadRestrictions::ScopedAllowWait allow_wait;
  id_event_.Wait();
  return id_;
}

bool Thread::IsRunning() const {
  // If the thread's already started (i.e. message_loop_ is non-null) and
  // not yet requested to stop (i.e. stopping_ is false) we can just return
  // true. (Note that stopping_ is touched only on the same thread that
  // starts / started the new thread so we need no locking here.)
  if (message_loop_ && !stopping_)
    return true;
  // Otherwise check the running_ flag, which is set to true by the new thread
  // only while it is inside Run().
  AutoLock lock(running_lock_);
  return running_;
}

void Thread::Run(MessageLoop* message_loop) {
  message_loop->Run();
}

void Thread::SetThreadWasQuitProperly(bool flag) {
  lazy_tls_bool.Pointer()->Set(flag);
}

bool Thread::GetThreadWasQuitProperly() {
  bool quit_properly = true;
#ifndef NDEBUG
  quit_properly = lazy_tls_bool.Pointer()->Get();
#endif
  return quit_properly;
}

void Thread::ThreadMain() {
  // First, make GetThreadId() available to avoid deadlocks. It could be called
  // any place in the following thread initialization code.
  id_ = PlatformThread::CurrentId();
  CR_DCHECK_NE(kInvalidThreadId, id_);
  id_event_.Signal();

  // Complete the initialization of our Thread object.
  PlatformThread::SetName(name_.c_str());
  ///ANNOTATE_THREAD_NAME(name_.c_str());  // Tell the name to race detector.

  // Lazily initialize the message_loop so that it can run on this thread.
  CR_DCHECK(message_loop_);
  std::unique_ptr<MessageLoop> message_loop(message_loop_);
  message_loop_->BindToCurrentThread();
  message_loop_->set_thread_name(name_);
  message_loop_->SetTimerSlack(message_loop_timer_slack_);

  std::unique_ptr<win::ScopedCOMInitializer> com_initializer;
  if (com_status_ != NONE) {
    com_initializer.reset((com_status_ == STA) ?
        new win::ScopedCOMInitializer() :
        new win::ScopedCOMInitializer(win::ScopedCOMInitializer::kMTA));
  }

  // Let the thread do extra initialization.
  Init();

  {
    AutoLock lock(running_lock_);
    running_ = true;
  }

  start_event_.Signal();

  Run(message_loop_);

  {
    AutoLock lock(running_lock_);
    running_ = false;
  }

  // Let the thread do extra cleanup.
  CleanUp();

  com_initializer.reset();

  if (message_loop->type() != MessageLoop::TYPE_CUSTOM) {
    // Assert that MessageLoop::QuitWhenIdle was called by ThreadQuitHelper.
    // Don't check for custom message pumps, because their shutdown might not
    // allow this.
    CR_DCHECK(GetThreadWasQuitProperly());
  }

  // We can't receive messages anymore.
  // (The message loop is destructed at the end of this block)
  message_loop_ = nullptr;
}

}  // namespace cr
