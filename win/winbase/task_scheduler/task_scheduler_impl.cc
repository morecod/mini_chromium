// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "winbase\task_scheduler\task_scheduler_impl.h"

#include <algorithm>
#include <string>
#include <utility>

#include "winbase\compiler_specific.h"
#include "winbase\message_loop\message_loop.h"
///#include "winbase\metrics\field_trial_params.h"
#include "winbase\stl_util.h"
#include "winbase\strings\string_util.h"
#include "winbase\task_scheduler\delayed_task_manager.h"
#include "winbase\task_scheduler\environment_config.h"
#include "winbase\task_scheduler\scheduler_worker_pool_params.h"
#include "winbase\task_scheduler\sequence.h"
#include "winbase\task_scheduler\sequence_sort_key.h"
#include "winbase\task_scheduler\service_thread.h"
#include "winbase\task_scheduler\task.h"
#include "winbase\task_scheduler\task_tracker.h"
#include "winbase\time\time.h"

namespace winbase {
namespace internal {

TaskSchedulerImpl::TaskSchedulerImpl(StringPiece histogram_label)
    : TaskSchedulerImpl(histogram_label,
                        std::make_unique<TaskTrackerImpl>(histogram_label)) {}

TaskSchedulerImpl::TaskSchedulerImpl(
    StringPiece histogram_label,
    std::unique_ptr<TaskTrackerImpl> task_tracker)
    : task_tracker_(std::move(task_tracker)),
      service_thread_(std::make_unique<ServiceThread>(task_tracker_.get())),
      single_thread_task_runner_manager_(task_tracker_->GetTrackedRef(),
                                         &delayed_task_manager_) {
  WINBASE_DCHECK(!histogram_label.empty());

  static_assert(array_size(environment_to_worker_pool_) == ENVIRONMENT_COUNT,
                "The size of |environment_to_worker_pool_| must match "
                "ENVIRONMENT_COUNT.");
  static_assert(
      winbase::size(kEnvironmentParams) == ENVIRONMENT_COUNT,
      "The size of |kEnvironmentParams| must match ENVIRONMENT_COUNT.");

  int num_pools_to_create = CanUseBackgroundPriorityForSchedulerWorker()
                                ? ENVIRONMENT_COUNT
                                : ENVIRONMENT_COUNT_WITHOUT_BACKGROUND_PRIORITY;
  for (int environment_type = 0; environment_type < num_pools_to_create;
       ++environment_type) {
    worker_pools_.emplace_back(std::make_unique<SchedulerWorkerPoolImpl>(
        JoinString(
            {histogram_label, kEnvironmentParams[environment_type].name_suffix},
            "."),
        kEnvironmentParams[environment_type].name_suffix,
        kEnvironmentParams[environment_type].priority_hint,
        task_tracker_->GetTrackedRef(), &delayed_task_manager_));
  }

  // Map environment indexes to pools.
  environment_to_worker_pool_[FOREGROUND] = worker_pools_[FOREGROUND].get();
  environment_to_worker_pool_[FOREGROUND_BLOCKING] =
      worker_pools_[FOREGROUND_BLOCKING].get();

  if (CanUseBackgroundPriorityForSchedulerWorker()) {
    environment_to_worker_pool_[BACKGROUND] = worker_pools_[BACKGROUND].get();
    environment_to_worker_pool_[BACKGROUND_BLOCKING] =
        worker_pools_[BACKGROUND_BLOCKING].get();
  } else {
    // On platforms without background thread priority, tasks posted to the
    // background environment are run by foreground pools.
    environment_to_worker_pool_[BACKGROUND] = worker_pools_[FOREGROUND].get();
    environment_to_worker_pool_[BACKGROUND_BLOCKING] =
        worker_pools_[FOREGROUND_BLOCKING].get();
  }
}

TaskSchedulerImpl::~TaskSchedulerImpl() {
#if WINBASE_DCHECK_IS_ON()
  WINBASE_DCHECK(join_for_testing_returned_.IsSet());
#endif
}

void TaskSchedulerImpl::Start(
    const TaskScheduler::InitParams& init_params,
    SchedulerWorkerObserver* scheduler_worker_observer) {
  // This is set in Start() and not in the constructor because variation params
  // are usually not ready when TaskSchedulerImpl is instantiated in a process.
  ///if (winbase::GetFieldTrialParamValue("BrowserScheduler",
  ///                                     "AllTasksUserBlocking") == "true") {
  ///  all_tasks_user_blocking_.Set();
  ///}

  // Start the service thread. On platforms that support it (POSIX except NaCL
  // SFI), the service thread runs a MessageLoopForIO which is used to support
  // FileDescriptorWatcher in the scope in which tasks run.
  ServiceThread::Options service_thread_options;
  service_thread_options.message_loop_type = MessageLoop::Type::DEFAULT;
  service_thread_options.timer_slack = TimerSlack::TIMER_SLACK_MAXIMUM;
  WINBASE_CHECK(service_thread_->StartWithOptions(service_thread_options));

  // Needs to happen after starting the service thread to get its task_runner().
  scoped_refptr<TaskRunner> service_thread_task_runner =
      service_thread_->task_runner();
  delayed_task_manager_.Start(service_thread_task_runner);

  single_thread_task_runner_manager_.Start(scheduler_worker_observer);

  const SchedulerWorkerPoolImpl::WorkerEnvironment worker_environment =
      init_params.shared_worker_pool_environment ==
              InitParams::SharedWorkerPoolEnvironment::COM_MTA
          ? SchedulerWorkerPoolImpl::WorkerEnvironment::COM_MTA
          : SchedulerWorkerPoolImpl::WorkerEnvironment::NONE;

  // On platforms that can't use the background thread priority, background
  // tasks run in foreground pools. A cap is set on the number of background
  // tasks that can run in foreground pools to ensure that there is always room
  // for incoming foreground tasks and to minimize the performance impact of
  // background tasks.
  const int max_background_tasks_in_foreground_pool = std::max(
      1, std::min(init_params.background_worker_pool_params.max_tasks(),
                  init_params.foreground_worker_pool_params.max_tasks() / 2));
  worker_pools_[FOREGROUND]->Start(
      init_params.foreground_worker_pool_params,
      max_background_tasks_in_foreground_pool, service_thread_task_runner,
      scheduler_worker_observer, worker_environment);
  const int max_background_tasks_in_foreground_blocking_pool = std::max(
      1,
      std::min(
          init_params.background_blocking_worker_pool_params.max_tasks(),
          init_params.foreground_blocking_worker_pool_params.max_tasks() / 2));
  worker_pools_[FOREGROUND_BLOCKING]->Start(
      init_params.foreground_blocking_worker_pool_params,
      max_background_tasks_in_foreground_blocking_pool,
      service_thread_task_runner, scheduler_worker_observer,
      worker_environment);

  if (CanUseBackgroundPriorityForSchedulerWorker()) {
    worker_pools_[BACKGROUND]->Start(
        init_params.background_worker_pool_params,
        init_params.background_worker_pool_params.max_tasks(),
        service_thread_task_runner, scheduler_worker_observer,
        worker_environment);
    worker_pools_[BACKGROUND_BLOCKING]->Start(
        init_params.background_blocking_worker_pool_params,
        init_params.background_blocking_worker_pool_params.max_tasks(),
        service_thread_task_runner, scheduler_worker_observer,
        worker_environment);
  }
}

void TaskSchedulerImpl::PostDelayedTaskWithTraits(const Location& from_here,
                                                  const TaskTraits& traits,
                                                  OnceClosure task,
                                                  TimeDelta delay) {
  // Post |task| as part of a one-off single-task Sequence.
  const TaskTraits new_traits = SetUserBlockingPriorityIfNeeded(traits);
  GetWorkerPoolForTraits(new_traits)
      ->PostTaskWithSequence(
          Task(from_here, std::move(task), new_traits, delay),
          MakeRefCounted<Sequence>());
}

scoped_refptr<TaskRunner> TaskSchedulerImpl::CreateTaskRunnerWithTraits(
    const TaskTraits& traits) {
  const TaskTraits new_traits = SetUserBlockingPriorityIfNeeded(traits);
  return GetWorkerPoolForTraits(new_traits)
      ->CreateTaskRunnerWithTraits(new_traits);
}

scoped_refptr<SequencedTaskRunner>
TaskSchedulerImpl::CreateSequencedTaskRunnerWithTraits(
    const TaskTraits& traits) {
  const TaskTraits new_traits = SetUserBlockingPriorityIfNeeded(traits);
  return GetWorkerPoolForTraits(new_traits)
      ->CreateSequencedTaskRunnerWithTraits(new_traits);
}

scoped_refptr<SingleThreadTaskRunner>
TaskSchedulerImpl::CreateSingleThreadTaskRunnerWithTraits(
    const TaskTraits& traits,
    SingleThreadTaskRunnerThreadMode thread_mode) {
  return single_thread_task_runner_manager_
      .CreateSingleThreadTaskRunnerWithTraits(
          SetUserBlockingPriorityIfNeeded(traits), thread_mode);
}

scoped_refptr<SingleThreadTaskRunner>
TaskSchedulerImpl::CreateCOMSTATaskRunnerWithTraits(
    const TaskTraits& traits,
    SingleThreadTaskRunnerThreadMode thread_mode) {
  return single_thread_task_runner_manager_.CreateCOMSTATaskRunnerWithTraits(
      SetUserBlockingPriorityIfNeeded(traits), thread_mode);
}

///std::vector<const HistogramBase*> TaskSchedulerImpl::GetHistograms() const {
///  std::vector<const HistogramBase*> histograms;
///  for (const auto& worker_pool : worker_pools_)
///    worker_pool->GetHistograms(&histograms);
///
///  return histograms;
///}

int TaskSchedulerImpl::GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
    const TaskTraits& traits) const {
  // This method does not support getting the maximum number of BACKGROUND tasks
  // that can run concurrently in a pool.
  WINBASE_DCHECK_NE(traits.priority(), TaskPriority::BACKGROUND);
  return GetWorkerPoolForTraits(traits)
      ->GetMaxConcurrentNonBlockedTasksDeprecated();
}

void TaskSchedulerImpl::Shutdown() {
  // TODO(fdoray): Increase the priority of BACKGROUND tasks blocking shutdown.
  task_tracker_->Shutdown();
}

void TaskSchedulerImpl::FlushForTesting() {
  task_tracker_->FlushForTesting();
}

void TaskSchedulerImpl::FlushAsyncForTesting(OnceClosure flush_callback) {
  task_tracker_->FlushAsyncForTesting(std::move(flush_callback));
}

void TaskSchedulerImpl::JoinForTesting() {
#if WINBASE_DCHECK_IS_ON()
  WINBASE_DCHECK(!join_for_testing_returned_.IsSet());
#endif
  // The service thread must be stopped before the workers are joined, otherwise
  // tasks scheduled by the DelayedTaskManager might be posted between joining
  // those workers and stopping the service thread which will cause a CHECK. See
  // https://crbug.com/771701.
  service_thread_->Stop();
  single_thread_task_runner_manager_.JoinForTesting();
  for (const auto& worker_pool : worker_pools_)
    worker_pool->JoinForTesting();
#if WINBASE_DCHECK_IS_ON()
  join_for_testing_returned_.Set();
#endif
}

SchedulerWorkerPoolImpl* TaskSchedulerImpl::GetWorkerPoolForTraits(
    const TaskTraits& traits) const {
  return environment_to_worker_pool_[GetEnvironmentIndexForTraits(traits)];
}

TaskTraits TaskSchedulerImpl::SetUserBlockingPriorityIfNeeded(
    const TaskTraits& traits) const {
  return all_tasks_user_blocking_.IsSet()
             ? TaskTraits::Override(traits, {TaskPriority::USER_BLOCKING})
             : traits;
}

}  // namespace internal
}  // namespace winbase