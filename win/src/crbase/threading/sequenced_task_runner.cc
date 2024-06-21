// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/threading/sequenced_task_runner.h"

#include "crbase/functional/bind.h"

namespace crbase {

bool SequencedTaskRunner::PostNonNestableTask(
    const tracked_objects::Location& from_here,
    const Closure& task) {
  return PostNonNestableDelayedTask(from_here, task, TimeDelta());
}

bool SequencedTaskRunner::DeleteSoonInternal(
    const tracked_objects::Location& from_here,
    void(*deleter)(const void*),
    const void* object) {
  return PostNonNestableTask(from_here, Bind(deleter, object));
}

bool SequencedTaskRunner::ReleaseSoonInternal(
    const tracked_objects::Location& from_here,
    void(*releaser)(const void*),
    const void* object) {
  return PostNonNestableTask(from_here, Bind(releaser, object));
}

}  // namespace crbase