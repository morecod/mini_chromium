// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/message_loop/message_loop.h"

#include <algorithm>
#include <utility>
#include <memory>

#include "crbase/functional/bind.h"
#include "crbase/lazy_instance.h"
#include "crbase/logging.h"
#include "crbase/message_loop/message_pump_default.h"
#include "crbase/run_loop.h"
#include "crbase/threading/thread_task_runner_handle.h"
#include "crbase/threading/thread_local.h"
#include "crbase/time/time.h"
#include "crbase/tracing/tracked_objects.h"
#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_OS_POSIX)
#include "crbase/message_loop/message_pump_libevent.h"
#endif

namespace cr {

namespace {

// A lazily created thread local storage for quick access to a thread's message
// loop, if one exists.  This should be safe and free of static constructors.
LazyInstance<cr::ThreadLocalPointer<MessageLoop> >::Leaky lazy_tls_ptr =
    CR_LAZY_INSTANCE_INITIALIZER;

// Logical events for Histogram profiling. Run with --message-loop-histogrammer
// to get an accounting of messages and actions taken on each thread.
const int kTaskRunEvent = 0x1;
const int kTimerEvent = 0x2;

// Provide range of message IDs for use in histogramming and debug display.
const int kLeastNonZeroMessageId = 1;
const int kMaxMessageId = 1099;
const int kNumberOfDistinctMessagesDisplayed = 1100;

// Provide a macro that takes an expression (such as a constant, or macro
// constant) and creates a pair to initialize an array of pairs.  In this case,
// our pair consists of the expressions value, and the "stringized" version
// of the expression (i.e., the expression put in quotes).  For example, if
// we have:
//    #define FOO 2
//    #define BAR 5
// then the following:
//    VALUE_TO_NUMBER_AND_NAME(FOO + BAR)
// will expand to:
//   {7, "FOO + BAR"}
// We use the resulting array as an argument to our histogram, which reads the
// number as a bucket identifier, and proceeds to use the corresponding name
// in the pair (i.e., the quoted string) when printing out a histogram.
#define VALUE_TO_NUMBER_AND_NAME(name) {name, #name},

MessageLoop::MessagePumpFactory* message_pump_for_ui_factory_ = NULL;

MessagePumpForIO* ToPumpIO(MessagePump* pump) {
  return static_cast<MessagePumpForIO*>(pump);
}


std::unique_ptr<MessagePump> ReturnPump(std::unique_ptr<MessagePump> pump) {
  return pump;
}

}  // namespace

//------------------------------------------------------------------------------

MessageLoop::TaskObserver::TaskObserver() {
}

MessageLoop::TaskObserver::~TaskObserver() {
}

MessageLoop::DestructionObserver::~DestructionObserver() {
}

//------------------------------------------------------------------------------

MessageLoop::MessageLoop(Type type)
    : MessageLoop(type, MessagePumpFactoryCallback()) {
  BindToCurrentThread();
}

MessageLoop::MessageLoop(std::unique_ptr<MessagePump> pump)
    : MessageLoop(TYPE_CUSTOM, BindOnce(&ReturnPump, Passed(&pump))) {
  BindToCurrentThread();
}

MessageLoop::~MessageLoop() {
  // If |pump_| is non-null, this message loop has been bound and should be the
  // current one on this thread. Otherwise, this loop is being destructed before
  // it was bound to a thread, so a different message loop (or no loop at all)
  // may be current.
  CR_DCHECK((pump_ && current() == this) || (!pump_ && current() != this));

  // iOS just attaches to the loop, it doesn't Run it.
  // TODO(stuartmorgan): Consider wiring up a Detach().
  CR_DCHECK(!run_loop_);

#if defined(MINI_CHROMIUM_OS_WIN)
  if (in_high_res_mode_)
    Time::ActivateHighResolutionTimer(false);
#endif

  // Clean up any unprocessed tasks, but take care: deleting a task could
  // result in the addition of more tasks (e.g., via DeleteSoon).  We set a
  // limit on the number of times we will allow a deleted task to generate more
  // tasks.  Normally, we should only pass through this loop once or twice.  If
  // we end up hitting the loop limit, then it is probably due to one task that
  // is being stubborn.  Inspect the queues to see who is left.
  bool did_work;
  for (int i = 0; i < 100; ++i) {
    DeletePendingTasks();
    ReloadWorkQueue();
    // If we end up with empty queues, then break out of the loop.
    did_work = DeletePendingTasks();
    if (!did_work)
      break;
  }
  CR_DCHECK(!did_work);

  // Let interested parties have one last shot at accessing this.
  CR_FOR_EACH_OBSERVER(DestructionObserver, destruction_observers_,
                       WillDestroyCurrentMessageLoop());

  thread_task_runner_handle_.reset();

  // Tell the incoming queue that we are dying.
  incoming_task_queue_->WillDestroyCurrentMessageLoop();
  incoming_task_queue_ = NULL;
  unbound_task_runner_ = NULL;
  task_runner_ = NULL;

  // OK, now make it so that no one can find us.
  if (current() == this)
    lazy_tls_ptr.Pointer()->Set(nullptr);
}

// static
MessageLoop* MessageLoop::current() {
  // TODO(darin): sadly, we cannot enable this yet since people call us even
  // when they have no intention of using us.
  // DCHECK(loop) << "Ouch, did you forget to initialize me?";
  return lazy_tls_ptr.Pointer()->Get();
}

// static
bool MessageLoop::InitMessagePumpForUIFactory(MessagePumpFactory* factory) {
  if (message_pump_for_ui_factory_)
    return false;

  message_pump_for_ui_factory_ = factory;
  return true;
}

// static
std::unique_ptr<MessagePump> MessageLoop::CreateMessagePumpForType(Type type) {
  // TODO(rvargas): Get rid of the OS guards.
#if defined(MINI_CHROMIUM_OS_LINUX)
  typedef MessagePumpLibevent MessagePumpForUI;
#endif

  #define MESSAGE_PUMP_UI std::unique_ptr<MessagePump>(new MessagePumpForUI())

  #define MESSAGE_PUMP_DEFAULT \
  std::unique_ptr<MessagePump>(new MessagePumpDefault())

  if (type == MessageLoop::TYPE_UI) {
    if (message_pump_for_ui_factory_)
      return message_pump_for_ui_factory_();
    return MESSAGE_PUMP_UI;
  }
  if (type == MessageLoop::TYPE_IO)
    return std::unique_ptr<MessagePump>(new MessagePumpForIO());

  CR_DCHECK_EQ(MessageLoop::TYPE_DEFAULT, type);
  return MESSAGE_PUMP_DEFAULT;
}

void MessageLoop::AddDestructionObserver(
    DestructionObserver* destruction_observer) {
  CR_DCHECK_EQ(this, current());
  destruction_observers_.AddObserver(destruction_observer);
}

void MessageLoop::RemoveDestructionObserver(
    DestructionObserver* destruction_observer) {
  CR_DCHECK_EQ(this, current());
  destruction_observers_.RemoveObserver(destruction_observer);
}

void MessageLoop::PostTask(
    const tracked_objects::Location& from_here,
    OnceClosure task) {
  task_runner_->PostTask(from_here, std::move(task));
}

void MessageLoop::PostDelayedTask(
    const tracked_objects::Location& from_here,
    OnceClosure task,
    TimeDelta delay) {
  task_runner_->PostDelayedTask(from_here, std::move(task), delay);
}

void MessageLoop::PostNonNestableTask(
    const tracked_objects::Location& from_here,
    OnceClosure task) {
  task_runner_->PostNonNestableTask(from_here, std::move(task));
}

void MessageLoop::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    OnceClosure task,
    TimeDelta delay) {
  task_runner_->PostNonNestableDelayedTask(from_here, std::move(task), delay);
}

void MessageLoop::Run() {
  CR_DCHECK(pump_);
  RunLoop run_loop;
  run_loop.Run();
}

void MessageLoop::RunUntilIdle() {
  CR_DCHECK(pump_);
  RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void MessageLoop::QuitWhenIdle() {
  CR_DCHECK_EQ(this, current());
  if (run_loop_) {
    run_loop_->quit_when_idle_received_ = true;
  } else {
    CR_NOTREACHED() << "Must be inside Run to call Quit";
  }
}

void MessageLoop::QuitNow() {
  CR_DCHECK_EQ(this, current());
  if (run_loop_) {
    pump_->Quit();
  } else {
    CR_NOTREACHED() << "Must be inside Run to call Quit";
  }
}

bool MessageLoop::IsType(Type type) const {
  return type_ == type;
}

static void QuitCurrentWhenIdle() {
  MessageLoop::current()->QuitWhenIdle();
}

// static
RepeatingClosure MessageLoop::QuitWhenIdleClosure() {
  return BindRepeating(&QuitCurrentWhenIdle);
}

void MessageLoop::SetNestableTasksAllowed(bool allowed) {
  if (allowed) {
    // Kick the native pump just in case we enter a OS-driven nested message
    // loop.
    pump_->ScheduleWork();
  }
  nestable_tasks_allowed_ = allowed;
}

bool MessageLoop::NestableTasksAllowed() const {
  return nestable_tasks_allowed_;
}

bool MessageLoop::IsNested() {
  return run_loop_->run_depth_ > 1;
}

void MessageLoop::AddTaskObserver(TaskObserver* task_observer) {
  CR_DCHECK_EQ(this, current());
  task_observers_.AddObserver(task_observer);
}

void MessageLoop::RemoveTaskObserver(TaskObserver* task_observer) {
  CR_DCHECK_EQ(this, current());
  task_observers_.RemoveObserver(task_observer);
}

bool MessageLoop::is_running() const {
  CR_DCHECK_EQ(this, current());
  return run_loop_ != NULL;
}

bool MessageLoop::HasHighResolutionTasks() {
  return incoming_task_queue_->HasHighResolutionTasks();
}

//------------------------------------------------------------------------------

// static
std::unique_ptr<MessageLoop> MessageLoop::CreateUnbound(
    Type type, MessagePumpFactoryCallback pump_factory) {
  return std::unique_ptr<MessageLoop>(
      new MessageLoop(type, std::move(pump_factory)));
}

MessageLoop::MessageLoop(Type type, MessagePumpFactoryCallback pump_factory)
    : type_(type),
#if defined(MINI_CHROMIUM_OS_WIN)
      pending_high_res_tasks_(0),
      in_high_res_mode_(false),
      os_modal_loop_(false),
#endif
      nestable_tasks_allowed_(true),
      pump_factory_(std::move(pump_factory)),
      run_loop_(NULL),
      incoming_task_queue_(new internal::IncomingTaskQueue(this)),
      unbound_task_runner_(
          new internal::MessageLoopTaskRunner(incoming_task_queue_)),
      task_runner_(unbound_task_runner_) {
  // If type is TYPE_CUSTOM non-null pump_factory must be given.
  CR_DCHECK_EQ(type_ == TYPE_CUSTOM, !pump_factory_.is_null());
}

void MessageLoop::BindToCurrentThread() {
  CR_DCHECK(!pump_);
  if (!pump_factory_.is_null())
    pump_ = std::move(pump_factory_).Run();
  else
    pump_ = CreateMessagePumpForType(type_);

  CR_DCHECK(!current()) << "should only have one message loop per thread";
  lazy_tls_ptr.Pointer()->Set(this);

  incoming_task_queue_->StartScheduling();
  unbound_task_runner_->BindToCurrentThread();
  unbound_task_runner_ = nullptr;
  SetThreadTaskRunnerHandle();
}

void MessageLoop::SetTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  CR_DCHECK_EQ(this, current());
  CR_DCHECK(task_runner->BelongsToCurrentThread());
  CR_DCHECK(!unbound_task_runner_);
  task_runner_ = std::move(task_runner);
  SetThreadTaskRunnerHandle();
}

void MessageLoop::SetThreadTaskRunnerHandle() {
  CR_DCHECK_EQ(this, current());
  // Clear the previous thread task runner first, because only one can exist at
  // a time.
  thread_task_runner_handle_.reset();
  thread_task_runner_handle_.reset(new ThreadTaskRunnerHandle(task_runner_));
}

void MessageLoop::RunHandler() {
  CR_DCHECK_EQ(this, current());

#if defined(MINI_CHROMIUM_OS_WIN)
  if (run_loop_->dispatcher_ && type() == TYPE_UI) {
    static_cast<MessagePumpForUI*>(pump_.get())->
        RunWithDispatcher(this, run_loop_->dispatcher_);
    return;
  }
#endif

  pump_->Run(this);
}

bool MessageLoop::ProcessNextDelayedNonNestableTask() {
  if (run_loop_->run_depth_ != 1)
    return false;

  if (deferred_non_nestable_work_queue_.empty())
    return false;

  while (!deferred_non_nestable_work_queue_.empty()) {
    PendingTask pending_task =
      std::move(deferred_non_nestable_work_queue_.front());
    deferred_non_nestable_work_queue_.pop();

    if (!pending_task.task.IsCancelled()) {
      RunTask(&pending_task);
      return true;
    }

  }

  return false;
}

void MessageLoop::RunTask(PendingTask* pending_task) {
  CR_DCHECK(nestable_tasks_allowed_);

#if defined(MINI_CHROMIUM_OS_WIN)
  if (pending_task->is_high_res) {
    pending_high_res_tasks_--;
    CR_CHECK_GE(pending_high_res_tasks_, 0);
  }
#endif

  // Execute the task and assume the worst: It is probably not reentrant.
  nestable_tasks_allowed_ = false;

  CR_FOR_EACH_OBSERVER(TaskObserver, task_observers_,
                       WillProcessTask(*pending_task));
  ///task_annotator_.RunTask("MessageLoop::PostTask", pending_task);
  std::move(pending_task->task).Run();
  CR_FOR_EACH_OBSERVER(TaskObserver, task_observers_,
                       DidProcessTask(*pending_task));

  nestable_tasks_allowed_ = true;
}

bool MessageLoop::DeferOrRunPendingTask(PendingTask pending_task) {
  if (pending_task.nestable || run_loop_->run_depth_ == 1) {
    RunTask(&pending_task);
    // Show that we ran a task (Note: a new one might arrive as a
    // consequence!).
    return true;
  }

  // We couldn't run the task now because we're in a nested message loop
  // and the task isn't nestable.
  deferred_non_nestable_work_queue_.push(std::move(pending_task));
  return false;
}

void MessageLoop::AddToDelayedWorkQueue(PendingTask pending_task) {
  // Move to the delayed work queue.
  delayed_work_queue_.push(std::move(pending_task));
}

bool MessageLoop::DeletePendingTasks() {
  bool did_work = !work_queue_.empty();
  while (!work_queue_.empty()) {
    PendingTask pending_task = std::move(work_queue_.front());
    work_queue_.pop();
    if (!pending_task.delayed_run_time.is_null()) {
      // We want to delete delayed tasks in the same order in which they would
      // normally be deleted in case of any funny dependencies between delayed
      // tasks.
      AddToDelayedWorkQueue(std::move(pending_task));
    }
  }
  did_work |= !deferred_non_nestable_work_queue_.empty();
  while (!deferred_non_nestable_work_queue_.empty()) {
    deferred_non_nestable_work_queue_.pop();
  }
  did_work |= !delayed_work_queue_.empty();

  // Historically, we always delete the task regardless of valgrind status. It's
  // not completely clear why we want to leak them in the loops above.  This
  // code is replicating legacy behavior, and should not be considered
  // absolutely "correct" behavior.  See TODO above about deleting all tasks
  // when it's safe.
  while (!delayed_work_queue_.empty()) {
    delayed_work_queue_.pop();
  }
  return did_work;
}

void MessageLoop::ReloadWorkQueue() {
  // We can improve performance of our loading tasks from the incoming queue to
  // |*work_queue| by waiting until the last minute (|*work_queue| is empty) to
  // load. That reduces the number of locks-per-task significantly when our
  // queues get large.
  if (work_queue_.empty()) {
#if defined(MINI_CHROMIUM_OS_WIN)
    pending_high_res_tasks_ +=
        incoming_task_queue_->ReloadWorkQueue(&work_queue_);
#else
    incoming_task_queue_->ReloadWorkQueue(&work_queue_);
#endif
  }
}

void MessageLoop::ScheduleWork() {
  pump_->ScheduleWork();
}

bool MessageLoop::DoWork() {
  if (!nestable_tasks_allowed_) {
    // Task can't be executed right now.
    return false;
  }

  for (;;) {
    ReloadWorkQueue();
    if (work_queue_.empty())
      break;

    // Execute oldest task.
    do {
      PendingTask pending_task = std::move(work_queue_.front());
      work_queue_.pop();
      if (pending_task.task.IsCancelled()) {
        // do nothing~
      } else if (!pending_task.delayed_run_time.is_null()) {
        AddToDelayedWorkQueue(std::move(pending_task));
        // If we changed the topmost task, then it is time to reschedule.
        if (delayed_work_queue_.top().task.Equals(pending_task.task))
          pump_->ScheduleDelayedWork(pending_task.delayed_run_time);
      } else {
        if (DeferOrRunPendingTask(std::move(pending_task)))
          return true;
      }
    } while (!work_queue_.empty());
  }

  // Nothing happened.
  return false;
}

bool MessageLoop::DoDelayedWork(TimeTicks* next_delayed_work_time) {
  if (!nestable_tasks_allowed_ || delayed_work_queue_.empty()) {
    recent_time_ = *next_delayed_work_time = TimeTicks();
    return false;
  }

  // When we "fall behind", there will be a lot of tasks in the delayed work
  // queue that are ready to run.  To increase efficiency when we fall behind,
  // we will only call Time::Now() intermittently, and then process all tasks
  // that are ready to run before calling it again.  As a result, the more we
  // fall behind (and have a lot of ready-to-run delayed tasks), the more
  // efficient we'll be at handling the tasks.

  TimeTicks next_run_time = delayed_work_queue_.top().delayed_run_time;
  if (next_run_time > recent_time_) {
    recent_time_ = TimeTicks::Now();  // Get a better view of Now();
    if (next_run_time > recent_time_) {
      *next_delayed_work_time = next_run_time;
      return false;
    }
  }

  PendingTask pending_task = std::move(
      const_cast<PendingTask&>(delayed_work_queue_.top()));
  delayed_work_queue_.pop();

  if (!delayed_work_queue_.empty())
    *next_delayed_work_time = delayed_work_queue_.top().delayed_run_time;

  return DeferOrRunPendingTask(std::move(pending_task));
}

bool MessageLoop::DoIdleWork() {
  if (ProcessNextDelayedNonNestableTask())
    return true;

  if (run_loop_->quit_when_idle_received_)
    pump_->Quit();

  // When we return we will do a kernel wait for more tasks.

#if defined(MINI_CHROMIUM_OS_WIN)
  // On Windows we activate the high resolution timer so that the wait
  // _if_ triggered by the timer happens with good resolution. If we don't
  // do this the default resolution is 15ms which might not be acceptable
  // for some tasks.
  bool high_res = pending_high_res_tasks_ > 0;
  if (high_res != in_high_res_mode_) {
    in_high_res_mode_ = high_res;
    Time::ActivateHighResolutionTimer(in_high_res_mode_);
  }
#endif

  return false;
}

void MessageLoop::DeleteSoonInternal(const tracked_objects::Location& from_here,
                                     void(*deleter)(const void*),
                                     const void* object) {
  PostNonNestableTask(from_here, BindOnce(deleter, object));
}

void MessageLoop::ReleaseSoonInternal(
    const tracked_objects::Location& from_here,
    void(*releaser)(const void*),
    const void* object) {
  PostNonNestableTask(from_here, BindOnce(releaser, object));
}

//------------------------------------------------------------------------------
// MessageLoopForUI

#if defined(MINI_CHROMIUM_OS_LINUX)
bool MessageLoopForUI::WatchFileDescriptor(
    int fd,
    bool persistent,
    MessagePumpLibevent::Mode mode,
    MessagePumpLibevent::FileDescriptorWatcher *controller,
    MessagePumpLibevent::Watcher *delegate) {
  return static_cast<MessagePumpLibevent*>(pump_.get())->WatchFileDescriptor(
      fd,
      persistent,
      mode,
      controller,
      delegate);
}
#endif

//------------------------------------------------------------------------------
// MessageLoopForIO

void MessageLoopForIO::AddIOObserver(
    MessageLoopForIO::IOObserver* io_observer) {
  ToPumpIO(pump_.get())->AddIOObserver(io_observer);
}

void MessageLoopForIO::RemoveIOObserver(
    MessageLoopForIO::IOObserver* io_observer) {
  ToPumpIO(pump_.get())->RemoveIOObserver(io_observer);
}

#if defined(MINI_CHROMIUM_OS_WIN)
void MessageLoopForIO::RegisterIOHandler(HANDLE file, IOHandler* handler) {
  ToPumpIO(pump_.get())->RegisterIOHandler(file, handler);
}

bool MessageLoopForIO::RegisterJobObject(HANDLE job, IOHandler* handler) {
  return ToPumpIO(pump_.get())->RegisterJobObject(job, handler);
}

bool MessageLoopForIO::WaitForIOCompletion(DWORD timeout, IOHandler* filter) {
  return ToPumpIO(pump_.get())->WaitForIOCompletion(timeout, filter);
}
#elif defined(MINI_CHROMIUM_OS_POSIX)
bool MessageLoopForIO::WatchFileDescriptor(int fd,
                                           bool persistent,
                                           Mode mode,
                                           FileDescriptorWatcher* controller,
                                           Watcher* delegate) {
  return ToPumpIO(pump_.get())->WatchFileDescriptor(
      fd,
      persistent,
      mode,
      controller,
      delegate);
}
#endif

}  // namespace cr
