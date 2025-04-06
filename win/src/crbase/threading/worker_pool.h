// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_THREADING_WORKER_POOL_H_
#define MINI_CHROMIUM_SRC_CRBASE_THREADING_WORKER_POOL_H_

#include "crbase/base_export.h"
#include "crbase/functional/callback_forward.h"
#include "crbase/threading/task_runner_util.h"
#include "crbase/memory/ref_counted.h"

namespace cr {

class Task;

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

class TaskRunner;

// This is a facility that runs tasks that don't require a specific thread or
// a message loop.
//
// WARNING: This shouldn't be used unless absolutely necessary. We don't wait
// for the worker pool threads to finish on shutdown, so the tasks running
// inside the pool must be extremely careful about other objects they access
// (MessageLoops, Singletons, etc). During shutdown these object may no longer
// exist.
class CRBASE_EXPORT WorkerPool {
 public:
  // This function posts |task| to run on a worker thread.  |task_is_slow|
  // should be used for tasks that will take a long time to execute.  Returns
  // false if |task| could not be posted to a worker thread.  Regardless of
  // return value, ownership of |task| is transferred to the worker pool.
  static bool PostTask(const tracked_objects::Location& from_here,
                       OnceClosure task, bool task_is_slow);

  // Just like TaskRunner::PostTaskAndReply, except the destination
  // for |task| is a worker thread and you can specify |task_is_slow| just
  // like you can for PostTask above.
  static bool PostTaskAndReply(const tracked_objects::Location& from_here,
                               OnceClosure task,
                               OnceClosure reply,
                               bool task_is_slow);

  // When you have these methods
  //
  //   R DoWorkAndReturn();
  //   void Callback(const R& result);
  //
  // and want to call them in a PostTaskAndReply kind of fashion where the
  // result of DoWorkAndReturn is passed to the Callback, you can use
  // PostTaskAndReplyWithResult as in this example:
  //
  // PostTaskAndReplyWithResult(
  //     FROM_HERE,
  //     BindOnce(&DoWorkAndReturn),
  //     BindOnce(&Callback));
  template <typename TaskReturnType, typename ReplyArgType>
  static bool PostTaskAndReplyWithResult(
      const tracked_objects::Location& from_here,
      OnceCallback<TaskReturnType()> task,
      OnceCallback<void(ReplyArgType)> reply,
      bool task_is_slow) {
    CR_DCHECK(task);
    CR_DCHECK(reply);
    TaskReturnType* result = new TaskReturnType();
    return WorkerPool::PostTaskAndReply(
        from_here,
        BindOnce(&internal::ReturnAsParamAdapter<TaskReturnType>, std::move(task),
                 result),
        BindOnce(&internal::ReplyAdapter<TaskReturnType, ReplyArgType>,
                 std::move(reply), Owned(result)),
        task_is_slow);
  }

  // Return true if the current thread is one that this WorkerPool runs tasks
  // on.  (Note that if the Windows worker pool is used without going through
  // this WorkerPool interface, RunsTasksOnCurrentThread would return false on
  // those threads.)
  static bool RunsTasksOnCurrentThread();

  // Get a TaskRunner wrapper which posts to the WorkerPool using the given
  // |task_is_slow| behavior.
  static const scoped_refptr<TaskRunner>& GetTaskRunner(bool task_is_slow);
};

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_THREADING_WORKER_POOL_H_