// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "winbase\task_scheduler\scheduler_worker_pool_params.h"

namespace winbase {

SchedulerWorkerPoolParams::SchedulerWorkerPoolParams(
    int max_tasks,
    TimeDelta suggested_reclaim_time,
    SchedulerBackwardCompatibility backward_compatibility)
    : max_tasks_(max_tasks),
      suggested_reclaim_time_(suggested_reclaim_time),
      backward_compatibility_(backward_compatibility) {}

SchedulerWorkerPoolParams::SchedulerWorkerPoolParams(
    const SchedulerWorkerPoolParams& other) = default;

SchedulerWorkerPoolParams& SchedulerWorkerPoolParams::operator=(
    const SchedulerWorkerPoolParams& other) = default;

}  // namespace winbase