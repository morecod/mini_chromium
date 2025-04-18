// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_THREADING_THREAD_H_
#define MINI_CHROMIUM_SRC_CRBASE_THREADING_THREAD_H_

#include <stddef.h>

#include <string>
#include <memory>

#include "crbase/base_export.h"
#include "crbase/functional/callback.h"
#include "crbase/macros.h"
#include "crbase/message_loop/message_loop.h"
#include "crbase/message_loop/timer_slack.h"
#include "crbase/synchronization/lock.h"
#include "crbase/synchronization/waitable_event.h"
#include "crbase/threading/platform_thread.h"
#include "crbase/threading/single_thread_task_runner.h"
#include "crbase/build_config.h"

namespace cr {

class MessagePump;

// A simple thread abstraction that establishes a MessageLoop on a new thread.
// The consumer uses the MessageLoop of the thread to cause code to execute on
// the thread.  When this object is destroyed the thread is terminated.  All
// pending tasks queued on the thread's message loop will run to completion
// before the thread is terminated.
//
// WARNING! SUBCLASSES MUST CALL Stop() IN THEIR DESTRUCTORS!  See ~Thread().
//
// After the thread is stopped, the destruction sequence is:
//
//  (1) Thread::CleanUp()
//  (2) MessageLoop::~MessageLoop
//  (3.b)    MessageLoop::DestructionObserver::WillDestroyCurrentMessageLoop
class CRBASE_EXPORT Thread : PlatformThread::Delegate {
 public:
  Thread(const Thread&) = delete;
  Thread& operator=(const Thread&) = delete;

  struct CRBASE_EXPORT Options {
    typedef Callback<std::unique_ptr<MessagePump>()> MessagePumpFactory;

    Options();
    Options(MessageLoop::Type type, size_t size);
    ~Options();

    // Specifies the type of message loop that will be allocated on the thread.
    // This is ignored if message_pump_factory.is_null() is false.
    MessageLoop::Type message_loop_type;

    // Specifies timer slack for thread message loop.
    TimerSlack timer_slack;

    // Used to create the MessagePump for the MessageLoop. The callback is Run()
    // on the thread. If message_pump_factory.is_null(), then a MessagePump
    // appropriate for |message_loop_type| is created. Setting this forces the
    // MessageLoop::Type to TYPE_CUSTOM.
    MessagePumpFactory message_pump_factory;

    // Specifies the maximum stack size that the thread is allowed to use.
    // This does not necessarily correspond to the thread's initial stack size.
    // A value of 0 indicates that the default maximum should be used.
    size_t stack_size;

    // Specifies the initial thread priority.
    ThreadPriority priority;
  };

  // Constructor.
  // name is a display string to identify the thread.
  explicit Thread(const std::string& name);

  // Destroys the thread, stopping it if necessary.
  //
  // NOTE: ALL SUBCLASSES OF Thread MUST CALL Stop() IN THEIR DESTRUCTORS (or
  // guarantee Stop() is explicitly called before the subclass is destroyed).
  // This is required to avoid a data race between the destructor modifying the
  // vtable, and the thread's ThreadMain calling the virtual method Run().  It
  // also ensures that the CleanUp() virtual method is called on the subclass
  // before it is destructed.
  ~Thread() override;

#if defined(MINI_CHROMIUM_OS_WIN)
  // Causes the thread to initialize COM.  This must be called before calling
  // Start() or StartWithOptions().  If |use_mta| is false, the thread is also
  // started with a TYPE_UI message loop.  It is an error to call
  // init_com_with_mta(false) and then StartWithOptions() with any message loop
  // type other than TYPE_UI.
  void init_com_with_mta(bool use_mta) {
    CR_DCHECK(!message_loop_);
    com_status_ = use_mta ? MTA : STA;
  }
#endif

  // Starts the thread.  Returns true if the thread was successfully started;
  // otherwise, returns false.  Upon successful return, the message_loop()
  // getter will return non-null.
  //
  // Note: This function can't be called on Windows with the loader lock held;
  // i.e. during a DllMain, global object construction or destruction, atexit()
  // callback.
  bool Start();

  // Starts the thread. Behaves exactly like Start in addition to allow to
  // override the default options.
  //
  // Note: This function can't be called on Windows with the loader lock held;
  // i.e. during a DllMain, global object construction or destruction, atexit()
  // callback.
  bool StartWithOptions(const Options& options);

  // Starts the thread and wait for the thread to start and run initialization
  // before returning. It's same as calling Start() and then
  // WaitUntilThreadStarted().
  // Note that using this (instead of Start() or StartWithOptions() causes
  // jank on the calling thread, should be used only in testing code.
  bool StartAndWaitForTesting();

  // Blocks until the thread starts running. Called within StartAndWait().
  // Note that calling this causes jank on the calling thread, must be used
  // carefully for production code.
  bool WaitUntilThreadStarted() const;

  // Signals the thread to exit and returns once the thread has exited.  After
  // this method returns, the Thread object is completely reset and may be used
  // as if it were newly constructed (i.e., Start may be called again).
  //
  // Stop may be called multiple times and is simply ignored if the thread is
  // already stopped.
  //
  // NOTE: If you are a consumer of Thread, it is not necessary to call this
  // before deleting your Thread objects, as the destructor will do it.
  // IF YOU ARE A SUBCLASS OF Thread, YOU MUST CALL THIS IN YOUR DESTRUCTOR.
  void Stop();

  // Signals the thread to exit in the near future.
  //
  // WARNING: This function is not meant to be commonly used. Use at your own
  // risk. Calling this function will cause message_loop() to become invalid in
  // the near future. This function was created to workaround a specific
  // deadlock on Windows with printer worker thread. In any other case, Stop()
  // should be used.
  //
  // StopSoon should not be called multiple times as it is risky to do so. It
  // could cause a timing issue in message_loop() access. Call Stop() to reset
  // the thread object once it is known that the thread has quit.
  void StopSoon();

  // Returns the message loop for this thread.  Use the MessageLoop's
  // PostTask methods to execute code on the thread.  This only returns
  // non-null after a successful call to Start.  After Stop has been called,
  // this will return nullptr.
  //
  // NOTE: You must not call this MessageLoop's Quit method directly.  Use
  // the Thread's Stop method instead.
  //
  MessageLoop* message_loop() const { return message_loop_; }

  // Returns a TaskRunner for this thread. Use the TaskRunner's PostTask
  // methods to execute code on the thread. Returns nullptr if the thread is not
  // running (e.g. before Start or after Stop have been called). Callers can
  // hold on to this even after the thread is gone; in this situation, attempts
  // to PostTask() will fail.
  scoped_refptr<SingleThreadTaskRunner> task_runner() const {
    return message_loop_ ? message_loop_->task_runner() : nullptr;
  }

  // Returns the name of this thread (for display in debugger too).
  const std::string& thread_name() const { return name_; }

  // The native thread handle.
  PlatformThreadHandle thread_handle() { return thread_; }

  // Returns the thread ID.  Should not be called before the first Start*()
  // call.  Keeps on returning the same ID even after a Stop() call. The next
  // Start*() call renews the ID.
  //
  // WARNING: This function will block if the thread hasn't started yet.
  //
  PlatformThreadId GetThreadId() const;

  // Returns true if the thread has been started, and not yet stopped.
  bool IsRunning() const;

 protected:
  // Called just prior to starting the message loop
  virtual void Init() {}

  // Called to start the message loop
  virtual void Run(MessageLoop* message_loop);

  // Called just after the message loop ends
  virtual void CleanUp() {}

  static void SetThreadWasQuitProperly(bool flag);
  static bool GetThreadWasQuitProperly();

  void set_message_loop(MessageLoop* message_loop) {
    message_loop_ = message_loop;
  }

 private:
#if defined(MINI_CHROMIUM_OS_WIN)
  enum ComStatus {
    NONE,
    STA,
    MTA,
  };
#endif

  // PlatformThread::Delegate methods:
  void ThreadMain() override;

#if defined(MINI_CHROMIUM_OS_WIN)
  // Whether this thread needs to initialize COM, and if so, in what mode.
  ComStatus com_status_;
#endif

  // If true, we're in the middle of stopping, and shouldn't access
  // |message_loop_|. It may non-nullptr and invalid.
  // Should be written on the thread that created this thread. Also read data
  // could be wrong on other threads.
  bool stopping_;

  // True while inside of Run().
  bool running_;
  mutable Lock running_lock_;  // Protects |running_|.

  // The thread's handle.
  PlatformThreadHandle thread_;
  mutable Lock thread_lock_;  // Protects |thread_|.

  // The thread's id once it has started.
  PlatformThreadId id_;
  mutable WaitableEvent id_event_;  // Protects |id_|.

  // The thread's message loop.  Valid only while the thread is alive.  Set
  // by the created thread.
  MessageLoop* message_loop_;

  // Stores Options::timer_slack_ until the message loop has been bound to
  // a thread.
  TimerSlack message_loop_timer_slack_;

  // The name of the thread.  Used for debugging purposes.
  std::string name_;

  // Signaled when the created thread gets ready to use the message loop.
  mutable WaitableEvent start_event_;

  friend void ThreadQuitHelper();
};

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_THREADING_THREAD_H_