// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_PENDING_TASK_H_
#define MINI_CHROMIUM_SRC_CRBASE_PENDING_TASK_H_

#include <queue>

#include "crbase/base_export.h"
#include "crbase/functional/callback.h"
#include "crbase/time/time.h"
#include "crbase/tracing/location.h"
#include "crbase/tracing/tracking_info.h"

namespace crbase {

// Contains data about a pending task. Stored in TaskQueue and DelayedTaskQueue
// for use by classes that queue and execute tasks.
struct CRBASE_EXPORT PendingTask : public TrackingInfo {
  PendingTask(const tracked_objects::Location& posted_from,
              const Closure& task);
  PendingTask(const tracked_objects::Location& posted_from,
              const Closure& task,
              TimeTicks delayed_run_time,
              bool nestable);
  ~PendingTask();

  // Used to support sorting.
  bool operator<(const PendingTask& other) const;

  // The task to run.
  Closure task;

  // The site this PendingTask was posted from.
  tracked_objects::Location posted_from;

  // Secondary sort key for run time.
  int sequence_num;

  // OK to dispatch from a nested loop.
  bool nestable;

  // Needs high resolution timers.
  bool is_high_res;
};

// Wrapper around std::queue specialized for PendingTask which adds a Swap
// helper method.
class CRBASE_EXPORT TaskQueue : public std::queue<PendingTask> {
 public:
  void Swap(TaskQueue* queue);
};

// PendingTasks are sorted by their |delayed_run_time| property.
typedef std::priority_queue<PendingTask> DelayedTaskQueue;

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_PENDING_TASK_H_