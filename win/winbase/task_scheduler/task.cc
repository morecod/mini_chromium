// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "winbase\task_scheduler\task.h"

#include <utility>

#include "winbase\atomic\atomic_sequence_num.h"
#include "winbase\functional\critical_closure.h"

namespace winbase {
namespace internal {

namespace {

AtomicSequenceNumber g_sequence_nums_for_tracing;

}  // namespace

Task::Task(const Location& posted_from,
           OnceClosure task,
           const TaskTraits& traits,
           TimeDelta delay)
    : PendingTask(
          posted_from,
          traits.shutdown_behavior() == TaskShutdownBehavior::BLOCK_SHUTDOWN
              ? MakeCriticalClosure(std::move(task))
              : std::move(task),
          delay.is_zero() ? TimeTicks() : TimeTicks::Now() + delay,
          Nestable::kNonNestable),
      // Prevent a delayed BLOCK_SHUTDOWN task from blocking shutdown before it
      // starts running by changing its shutdown behavior to SKIP_ON_SHUTDOWN.
      traits(
          (!delay.is_zero() &&
           traits.shutdown_behavior() == TaskShutdownBehavior::BLOCK_SHUTDOWN)
              ? TaskTraits::Override(traits,
                                     {TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
              : traits),
      delay(delay) {
  // TaskScheduler doesn't use |sequence_num| but tracing (toplevel.flow) relies
  // on it being unique. While this subtle dependency is a bit overreaching,
  // TaskScheduler is the only task system that doesn't use |sequence_num| and
  // the dependent code rarely changes so this isn't worth a big change and
  // faking it here isn't too bad for now (posting tasks is full of atomic ops
  // already).
  this->sequence_num = g_sequence_nums_for_tracing.GetNext();
}

// This should be "= default but MSVC has trouble with "noexcept = default" in
// this case.
Task::Task(Task&& other) noexcept
    : PendingTask(std::move(other)),
      traits(other.traits),
      delay(other.delay),
      sequenced_time(other.sequenced_time),
      sequenced_task_runner_ref(std::move(other.sequenced_task_runner_ref)),
      single_thread_task_runner_ref(
          std::move(other.single_thread_task_runner_ref)) {}

Task::~Task() = default;

Task& Task::operator=(Task&& other) = default;

}  // namespace internal
}  // namespace winbase