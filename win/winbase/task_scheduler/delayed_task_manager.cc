// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "winbase\task_scheduler\delayed_task_manager.h"

#include <algorithm>

#include "winbase\functional\bind.h"
#include "winbase\logging.h"
#include "winbase\task_runner.h"
#include "winbase\task_scheduler\task.h"

namespace winbase {
namespace internal {

DelayedTaskManager::DelayedTaskManager(
    std::unique_ptr<const TickClock> tick_clock)
    : tick_clock_(std::move(tick_clock)) {
  WINBASE_DCHECK(tick_clock_);
}

DelayedTaskManager::~DelayedTaskManager() = default;

void DelayedTaskManager::Start(
    scoped_refptr<TaskRunner> service_thread_task_runner) {
  WINBASE_DCHECK(service_thread_task_runner);

  decltype(tasks_added_before_start_) tasks_added_before_start;

  {
    AutoSchedulerLock auto_lock(lock_);
    WINBASE_DCHECK(!service_thread_task_runner_);
    WINBASE_DCHECK(!started_.IsSet());
    service_thread_task_runner_ = std::move(service_thread_task_runner);
    tasks_added_before_start = std::move(tasks_added_before_start_);
    // |service_thread_task_runner_| must not change after |started_| is set
    // (cf. comment above |lock_| in header file).
    started_.Set();
  }

  const TimeTicks now = tick_clock_->NowTicks();
  for (auto& task_and_callback : tasks_added_before_start) {
    const TimeDelta delay =
        std::max(TimeDelta(), task_and_callback.first.delayed_run_time - now);
    AddDelayedTaskNow(std::move(task_and_callback.first), delay,
                      std::move(task_and_callback.second));
  }
}

void DelayedTaskManager::AddDelayedTask(
    Task task,
    PostTaskNowCallback post_task_now_callback) {
  WINBASE_DCHECK(task.task);

  const TimeDelta delay = task.delay;
  WINBASE_DCHECK(!delay.is_zero());

  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  WINBASE_CHECK(task.task);

  // If |started_| is set, the DelayedTaskManager is in a stable state and
  // AddDelayedTaskNow() can be called without synchronization. Otherwise, it is
  // necessary to acquire |lock_| and recheck.
  if (started_.IsSet()) {
    AddDelayedTaskNow(std::move(task), delay,
                      std::move(post_task_now_callback));
  } else {
    AutoSchedulerLock auto_lock(lock_);
    if (started_.IsSet()) {
      AddDelayedTaskNow(std::move(task), delay,
                        std::move(post_task_now_callback));
    } else {
      tasks_added_before_start_.push_back(
          {std::move(task), std::move(post_task_now_callback)});
    }
  }
}

void DelayedTaskManager::AddDelayedTaskNow(
    Task task,
    TimeDelta delay,
    PostTaskNowCallback post_task_now_callback) {
  WINBASE_DCHECK(task.task);
  WINBASE_DCHECK(started_.IsSet());
  // TODO(fdoray): Use |task->delayed_run_time| on the service thread
  // MessageLoop rather than recomputing it from |delay|.
  service_thread_task_runner_->PostDelayedTask(
      WINBASE_FROM_HERE,
      BindOnce(std::move(post_task_now_callback), std::move(task)),
      delay);
}

}  // namespace internal
}  // namespace winbase