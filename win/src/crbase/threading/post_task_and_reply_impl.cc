// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/threading/post_task_and_reply_impl.h"

#include "crbase/functional/bind.h"
#include "crbase/threading/single_thread_task_runner.h"
#include "crbase/threading/thread_task_runner_handle.h"
#include "crbase/tracing/location.h"

namespace crbase {

namespace {

// This relay class remembers the MessageLoop that it was created on, and
// ensures that both the |task| and |reply| Closures are deleted on this same
// thread. Also, |task| is guaranteed to be deleted before |reply| is run or
// deleted.
//
// If this is not possible because the originating MessageLoop is no longer
// available, the the |task| and |reply| Closures are leaked.  Leaking is
// considered preferable to having a thread-safetey violations caused by
// invoking the Closure destructor on the wrong thread.
class PostTaskAndReplyRelay {
 public:
  PostTaskAndReplyRelay(const tracked_objects::Location& from_here,
                        const Closure& task,
                        const Closure& reply)
      : from_here_(from_here),
        origin_task_runner_(ThreadTaskRunnerHandle::Get()) {
    task_ = task;
    reply_ = reply;
  }

  ~PostTaskAndReplyRelay() {
    CR_DCHECK(origin_task_runner_->BelongsToCurrentThread());
    task_.Reset();
    reply_.Reset();
  }

  void Run() {
    task_.Run();
    origin_task_runner_->PostTask(
        from_here_, Bind(&PostTaskAndReplyRelay::RunReplyAndSelfDestruct,
                         crbase::Unretained(this)));
  }

 private:
  void RunReplyAndSelfDestruct() {
    CR_DCHECK(origin_task_runner_->BelongsToCurrentThread());

    // Force |task_| to be released before |reply_| is to ensure that no one
    // accidentally depends on |task_| keeping one of its arguments alive while
    // |reply_| is executing.
    task_.Reset();

    reply_.Run();

    // Cue mission impossible theme.
    delete this;
  }

  tracked_objects::Location from_here_;
  scoped_refptr<SingleThreadTaskRunner> origin_task_runner_;
  Closure reply_;
  Closure task_;
};

}  // namespace

namespace internal {

bool PostTaskAndReplyImpl::PostTaskAndReply(
    const tracked_objects::Location& from_here,
    const Closure& task,
    const Closure& reply) {
  // TODO(tzik): Use DCHECK here once the crash is gone. http://crbug.com/541319
  CR_CHECK(!task.is_null()) << from_here.ToString();
  CR_CHECK(!reply.is_null()) << from_here.ToString();
  PostTaskAndReplyRelay* relay =
      new PostTaskAndReplyRelay(from_here, task, reply);
  if (!PostTask(from_here, Bind(&PostTaskAndReplyRelay::Run,
                                Unretained(relay)))) {
    delete relay;
    return false;
  }

  return true;
}

}  // namespace internal

}  // namespace crbase