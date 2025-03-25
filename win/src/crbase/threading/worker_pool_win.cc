// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/threading/worker_pool.h"

#include "crbase/functional/bind.h"
#include "crbase/functional/callback.h"
#include "crbase/logging.h"
#include "crbase/lazy_instance.h"
#include "crbase/threading/pending_task.h"
#include "crbase/threading/thread_local.h"
///#include "crbase/trace_event/trace_event.h"
#include "crbase/tracing/tracked_objects.h"

namespace crbase {

namespace {

crbase::LazyInstance<ThreadLocalBoolean>::Leaky
    g_worker_pool_running_on_this_thread = CR_LAZY_INSTANCE_INITIALIZER;

DWORD CALLBACK WorkItemCallback(void* param) {
  PendingTask* pending_task = static_cast<PendingTask*>(param);
  ///TRACE_EVENT2("toplevel", "WorkItemCallback::Run",
  ///             "src_file", pending_task->posted_from.file_name(),
  ///             "src_func", pending_task->posted_from.function_name());

  g_worker_pool_running_on_this_thread.Get().Set(true);

  tracked_objects::TaskStopwatch stopwatch;
  stopwatch.Start();
  std::move(pending_task->task).Run();
  stopwatch.Stop();

  g_worker_pool_running_on_this_thread.Get().Set(false);

  tracked_objects::ThreadData::TallyRunOnWorkerThreadIfTracking(
      pending_task->birth_tally, pending_task->time_posted, stopwatch);

  delete pending_task;
  return 0;
}

// Takes ownership of |pending_task|
bool PostTaskInternal(PendingTask* pending_task, bool task_is_slow) {
  ULONG flags = 0;
  if (task_is_slow)
    flags |= WT_EXECUTELONGFUNCTION;

  if (!::QueueUserWorkItem(WorkItemCallback, pending_task, flags)) {
    CR_DPLOG(ERROR) << "QueueUserWorkItem failed";
    delete pending_task;
    return false;
  }

  return true;
}

}  // namespace

// static
bool WorkerPool::PostTask(const tracked_objects::Location& from_here,
                          OnceClosure task, bool task_is_slow) {
  PendingTask* pending_task = new PendingTask(from_here, std::move(task));
  return PostTaskInternal(pending_task, task_is_slow);
}

// static
bool WorkerPool::RunsTasksOnCurrentThread() {
  return g_worker_pool_running_on_this_thread.Get().Get();
}

}  // namespace crbase