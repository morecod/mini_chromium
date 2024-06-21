// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/threading/worker_pool.h"

#include "crbase/functional/bind.h"
///#include "crbase/debug/leak_annotations.h"
#include "crbase/lazy_instance.h"
#include "crbase/macros.h"
#include "crbase/threading/task_runner.h"
#include "crbase/threading/post_task_and_reply_impl.h"
#include "crbase/tracing/tracked_objects.h"

namespace crbase {

namespace {

class PostTaskAndReplyWorkerPool : public internal::PostTaskAndReplyImpl {
 public:
  explicit PostTaskAndReplyWorkerPool(bool task_is_slow)
      : task_is_slow_(task_is_slow) {
  }
  ~PostTaskAndReplyWorkerPool() override = default;

 private:
  bool PostTask(const tracked_objects::Location& from_here,
                const Closure& task) override {
    return WorkerPool::PostTask(from_here, task, task_is_slow_);
  }

  bool task_is_slow_;
};

// WorkerPoolTaskRunner ---------------------------------------------
// A TaskRunner which posts tasks to a WorkerPool with a
// fixed ShutdownBehavior.
//
// Note that this class is RefCountedThreadSafe (inherited from TaskRunner).
class WorkerPoolTaskRunner : public TaskRunner {
 public:
  WorkerPoolTaskRunner(const WorkerPoolTaskRunner&) = delete;
  WorkerPoolTaskRunner& operator=(const WorkerPoolTaskRunner&) = delete;

  explicit WorkerPoolTaskRunner(bool tasks_are_slow);

  // TaskRunner implementation
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const Closure& task,
                       crbase::TimeDelta delay) override;
  bool RunsTasksOnCurrentThread() const override;

 private:
  ~WorkerPoolTaskRunner() override;

  // Helper function for posting a delayed task. Asserts that the delay is
  // zero because non-zero delays are not supported.
  bool PostDelayedTaskAssertZeroDelay(
      const tracked_objects::Location& from_here,
      const Closure& task,
      crbase::TimeDelta delay);

  const bool tasks_are_slow_;
};

WorkerPoolTaskRunner::WorkerPoolTaskRunner(bool tasks_are_slow)
    : tasks_are_slow_(tasks_are_slow) {
}

WorkerPoolTaskRunner::~WorkerPoolTaskRunner() {
}

bool WorkerPoolTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const Closure& task,
    crbase::TimeDelta delay) {
  return PostDelayedTaskAssertZeroDelay(from_here, task, delay);
}

bool WorkerPoolTaskRunner::RunsTasksOnCurrentThread() const {
  return WorkerPool::RunsTasksOnCurrentThread();
}

bool WorkerPoolTaskRunner::PostDelayedTaskAssertZeroDelay(
    const tracked_objects::Location& from_here,
    const Closure& task,
    crbase::TimeDelta delay) {
  CR_DCHECK_EQ(delay.InMillisecondsRoundedUp(), 0)
      << "WorkerPoolTaskRunner does not support non-zero delays";
  return WorkerPool::PostTask(from_here, task, tasks_are_slow_);
}

struct TaskRunnerHolder {
  TaskRunnerHolder() {
    taskrunners_[0] = new WorkerPoolTaskRunner(false);
    taskrunners_[1] = new WorkerPoolTaskRunner(true);
  }
  scoped_refptr<TaskRunner> taskrunners_[2];
};

crbase::LazyInstance<TaskRunnerHolder>::Leaky
    g_taskrunners = CR_LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool WorkerPool::PostTaskAndReply(const tracked_objects::Location& from_here,
                                  const Closure& task,
                                  const Closure& reply,
                                  bool task_is_slow) {
  // Do not report PostTaskAndReplyRelay leaks in tests. There's nothing we can
  // do about them because WorkerPool doesn't have a flushing API.
  // http://crbug.com/248513
  // http://crbug.com/290897
  // Note: this annotation does not cover tasks posted through a TaskRunner.
  ///ANNOTATE_SCOPED_MEMORY_LEAK;
  return PostTaskAndReplyWorkerPool(task_is_slow).PostTaskAndReply(
      from_here, task, reply);
}

// static
const scoped_refptr<TaskRunner>&
WorkerPool::GetTaskRunner(bool tasks_are_slow) {
  return g_taskrunners.Get().taskrunners_[tasks_are_slow];
}

}  // namespace crbase