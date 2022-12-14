// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINLIB_WINBASE_MESSAGE_LOOP_INCOMING_TASK_QUEUE_H_
#define WINLIB_WINBASE_MESSAGE_LOOP_INCOMING_TASK_QUEUE_H_

#include "winbase\base_export.h"
#include "winbase\functional\callback.h"
#include "winbase\macros.h"
#include "winbase\memory\ref_counted.h"
#include "winbase\pending_task.h"
#include "winbase\sequence_checker.h"
#include "winbase\synchronization\lock.h"
#include "winbase\time\time.h"

namespace winbase {

namespace internal {

// Implements a queue of tasks posted to the message loop running on the current
// thread. This class takes care of synchronizing posting tasks from different
// threads and together with MessageLoop ensures clean shutdown.
class WINBASE_EXPORT IncomingTaskQueue
    : public RefCountedThreadSafe<IncomingTaskQueue> {
 public:
  // TODO(gab): Move this to SequencedTaskSource::Observer in
  // https://chromium-review.googlesource.com/c/chromium/src/+/1088762.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Notifies this Observer that it is about to enqueue |task|. The Observer
    // may alter |task| as a result (e.g. add metadata to the PendingTask
    // struct). This may be called while holding a lock and shouldn't perform
    // logic requiring synchronization (override DidQueueTask() for that).
    virtual void WillQueueTask(PendingTask* task) = 0;

    // Notifies this Observer that a task was queued in the IncomingTaskQueue it
    // observes. |was_empty| is true if the task source was empty (i.e.
    // |!HasTasks()|) before this task was posted. DidQueueTask() can be invoked
    // from any thread.
    virtual void DidQueueTask(bool was_empty) = 0;
  };

  // Provides a read and remove only view into a task queue.
  class ReadAndRemoveOnlyQueue {
   public:
    ReadAndRemoveOnlyQueue() = default;
    ReadAndRemoveOnlyQueue(const ReadAndRemoveOnlyQueue&) = delete;
    ReadAndRemoveOnlyQueue& operator=(const ReadAndRemoveOnlyQueue&) = delete;
    virtual ~ReadAndRemoveOnlyQueue() = default;

    // Returns the next task. HasTasks() is assumed to be true.
    virtual const PendingTask& Peek() = 0;

    // Removes and returns the next task. HasTasks() is assumed to be true.
    virtual PendingTask Pop() = 0;

    // Whether this queue has tasks.
    virtual bool HasTasks() = 0;

    // Removes all tasks.
    virtual void Clear() = 0;
  };

  // Provides a read-write task queue.
  class Queue : public ReadAndRemoveOnlyQueue {
   public:
    Queue() = default;
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
    ~Queue() override = default;

    // Adds the task to the end of the queue.
    virtual void Push(PendingTask pending_task) = 0;
  };

  // Constructs an IncomingTaskQueue which will invoke |task_queue_observer|
  // when tasks are queued. |task_queue_observer| will be bound to this
  // IncomingTaskQueue's lifetime. Ownership is required as opposed to a raw
  // pointer since IncomingTaskQueue is ref-counted. For the same reasons,
  // |task_queue_observer| needs to support being invoked racily during
  // shutdown).
  explicit IncomingTaskQueue(std::unique_ptr<Observer> task_queue_observer);

  IncomingTaskQueue(const IncomingTaskQueue&) = delete;
  IncomingTaskQueue& operator=(const IncomingTaskQueue&) = delete;

  // Appends a task to the incoming queue. Posting of all tasks is routed though
  // AddToIncomingQueue() or TryAddToIncomingQueue() to make sure that posting
  // task is properly synchronized between different threads.
  //
  // Returns true if the task was successfully added to the queue, otherwise
  // returns false. In all cases, the ownership of |task| is transferred to the
  // called method.
  bool AddToIncomingQueue(const Location& from_here,
                          OnceClosure task,
                          TimeDelta delay,
                          Nestable nestable);

  // Instructs this IncomingTaskQueue to stop accepting tasks, this cannot be
  // undone. Note that the registered IncomingTaskQueue::Observer may still
  // racily receive a few DidQueueTask() calls while the Shutdown() signal
  // propagates to other threads and it needs to support that.
  void Shutdown();

  ReadAndRemoveOnlyQueue& triage_tasks() { return triage_tasks_; }

  Queue& delayed_tasks() { return delayed_tasks_; }

  Queue& deferred_tasks() { return deferred_tasks_; }

  bool HasPendingHighResolutionTasks() const {
    WINBASE_DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return delayed_tasks_.HasPendingHighResolutionTasks();
  }

  // Reports UMA metrics about its queues before the MessageLoop goes to sleep
  // per being idle.
  void ReportMetricsOnIdle() const;

 private:
  friend class RefCountedThreadSafe<IncomingTaskQueue>;

  // These queues below support the previous MessageLoop behavior of
  // maintaining three queue queues to process tasks:
  //
  // TriageQueue
  // The first queue to receive all tasks for the processing sequence (when
  // reloading from the thread-safe |incoming_queue_|). Tasks are generally
  // either dispatched immediately or sent to the queues below.
  //
  // DelayedQueue
  // The queue for holding tasks that should be run later and sorted by expected
  // run time.
  //
  // DeferredQueue
  // The queue for holding tasks that couldn't be run while the MessageLoop was
  // nested. These are generally processed during the idle stage.
  //
  // Many of these do not share implementations even though they look like they
  // could because of small quirks (reloading semantics) or differing underlying
  // data strucutre (TaskQueue vs DelayedTaskQueue).

  // The starting point for all tasks on the sequence processing the tasks.
  class TriageQueue : public ReadAndRemoveOnlyQueue {
   public:
    TriageQueue(IncomingTaskQueue* outer);
    TriageQueue(const TriageQueue&) = delete;
    TriageQueue& operator=(const TriageQueue&) = delete;
    ~TriageQueue() override;

    // ReadAndRemoveOnlyQueue:
    // The methods below will attempt to reload from the incoming queue if the
    // queue itself is empty (Clear() has special logic to reload only once
    // should destructors post more tasks).
    const PendingTask& Peek() override;
    PendingTask Pop() override;
    // Whether this queue has tasks after reloading from the incoming queue.
    bool HasTasks() override;
    void Clear() override;

   private:
    void ReloadFromIncomingQueueIfEmpty();

    IncomingTaskQueue* const outer_;
    TaskQueue queue_;
  };

  class DelayedQueue : public Queue {
   public:
    DelayedQueue();
    DelayedQueue(const DelayedQueue&) = delete;
    DelayedQueue& operator=(const DelayedQueue&) = delete;
    ~DelayedQueue() override;

    // Queue:
    const PendingTask& Peek() override;
    PendingTask Pop() override;
    // Whether this queue has tasks after sweeping the cancelled ones in front.
    bool HasTasks() override;
    void Clear() override;
    void Push(PendingTask pending_task) override;

    size_t Size() const;
    bool HasPendingHighResolutionTasks() const {
      return pending_high_res_tasks_ > 0;
    }

   private:
    DelayedTaskQueue queue_;

    // Number of high resolution tasks in |queue_|.
    int pending_high_res_tasks_ = 0;

    WINBASE_SEQUENCE_CHECKER(sequence_checker_);
  };

  class DeferredQueue : public Queue {
   public:
    DeferredQueue();
    DeferredQueue(const DeferredQueue&) = delete;
    DeferredQueue& operator=(const DeferredQueue&) = delete;
    ~DeferredQueue() override;

    // Queue:
    const PendingTask& Peek() override;
    PendingTask Pop() override;
    bool HasTasks() override;
    void Clear() override;
    void Push(PendingTask pending_task) override;

   private:
    TaskQueue queue_;

    WINBASE_SEQUENCE_CHECKER(sequence_checker_);
  };

  virtual ~IncomingTaskQueue();

  // Adds a task to |incoming_queue_|. The caller retains ownership of
  // |pending_task|, but this function will reset the value of
  // |pending_task->task|. This is needed to ensure that the posting call stack
  // does not retain |pending_task->task| beyond this function call.
  bool PostPendingTask(PendingTask* pending_task);

  // Does the real work of posting a pending task. Returns true if
  // |incoming_queue_| was empty before |pending_task| was posted.
  bool PostPendingTaskLockRequired(PendingTask* pending_task);

  // Loads tasks from the |incoming_queue_| into |*work_queue|. Must be called
  // from the sequence processing the tasks.
  void ReloadWorkQueue(TaskQueue* work_queue);

  // Checks calls made only on the MessageLoop thread.
  WINBASE_SEQUENCE_CHECKER(sequence_checker_);

  const std::unique_ptr<Observer> task_queue_observer_;

  // Queue for initial triaging of tasks on the |sequence_checker_| sequence.
  TriageQueue triage_tasks_;

  // Queue for delayed tasks on the |sequence_checker_| sequence.
  DelayedQueue delayed_tasks_;

  // Queue for non-nestable deferred tasks on the |sequence_checker_| sequence.
  DeferredQueue deferred_tasks_;

  // Synchronizes access to all members below this line.
  winbase::Lock incoming_queue_lock_;

  // An incoming queue of tasks that are acquired under a mutex for processing
  // on this instance's thread. These tasks have not yet been been pushed to
  // |triage_tasks_|.
  TaskQueue incoming_queue_;

  // True if new tasks should be accepted.
  bool accept_new_tasks_ = true;

  // The next sequence number to use for delayed tasks.
  int next_sequence_num_ = 0;

  // True if the outgoing queue (|triage_tasks_|) is empty. Toggled under
  // |incoming_queue_lock_| in ReloadWorkQueue() so that
  // PostPendingTaskLockRequired() can tell, without accessing the thread unsafe
  // |triage_tasks_|, if the IncomingTaskQueue has been made non-empty by a
  // PostTask() (and needs to inform its Observer).
  bool triage_queue_empty_ = true;
};

}  // namespace internal
}  // namespace winbase

#endif  // WINLIB_WINBASE_MESSAGE_LOOP_INCOMING_TASK_QUEUE_H_