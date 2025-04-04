// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/threading/task_runner.h"

#include "crbase/logging.h"
#include "crbase/threading/post_task_and_reply_impl.h"

namespace cr {

namespace {

// TODO(akalin): There's only one other implementation of
// PostTaskAndReplyImpl in WorkerPool.  Investigate whether it'll be
// possible to merge the two.
class PostTaskAndReplyTaskRunner : public internal::PostTaskAndReplyImpl {
 public:
  explicit PostTaskAndReplyTaskRunner(TaskRunner* destination);

 private:
  bool PostTask(const tracked_objects::Location& from_here,
                OnceClosure task) override;

  // Non-owning.
  TaskRunner* destination_;
};

PostTaskAndReplyTaskRunner::PostTaskAndReplyTaskRunner(
    TaskRunner* destination) : destination_(destination) {
  CR_DCHECK(destination_);
}

bool PostTaskAndReplyTaskRunner::PostTask(
    const tracked_objects::Location& from_here,
    OnceClosure task) {
  return destination_->PostTask(from_here, std::move(task));
}

}  // namespace

bool TaskRunner::PostTask(const tracked_objects::Location& from_here,
                          OnceClosure task) {
  return PostDelayedTask(from_here, std::move(task), TimeDelta());
}

bool TaskRunner::PostTaskAndReply(
    const tracked_objects::Location& from_here,
    OnceClosure task,
    OnceClosure reply) {
  return PostTaskAndReplyTaskRunner(this).PostTaskAndReply(
      from_here, std::move(task), std::move(reply));
}

TaskRunner::TaskRunner() {}

TaskRunner::~TaskRunner() {}

void TaskRunner::OnDestruct() const {
  delete this;
}

void TaskRunnerTraits::Destruct(const TaskRunner* task_runner) {
  task_runner->OnDestruct();
}

}  // namespace cr
