// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation shared by
// TaskRunner::PostTaskAndReply and WorkerPool::PostTaskAndReply.

#ifndef MINI_CHROMIUM_SRC_CRBASE_THREADING_POST_TASK_AND_REPLY_IMPL_H_
#define MINI_CHROMIUM_SRC_CRBASE_THREADING_POST_TASK_AND_REPLY_IMPL_H_

#include "crbase/functional/callback_forward.h"
#include "crbase/tracing/location.h"

namespace crbase {
namespace internal {

// Inherit from this in a class that implements PostTask appropriately
// for sending to a destination thread.
//
// Note that 'reply' will always get posted back to your current
// MessageLoop.
//
// If you're looking for a concrete implementation of
// PostTaskAndReply, you probably want crbase::SingleThreadTaskRunner, or you
// may want crbase::WorkerPool.
class PostTaskAndReplyImpl {
 public:
  virtual ~PostTaskAndReplyImpl() = default;

  // Implementation for TaskRunner::PostTaskAndReply and
  // WorkerPool::PostTaskAndReply.
  bool PostTaskAndReply(const tracked_objects::Location& from_here,
                        const Closure& task,
                        const Closure& reply);

 private:
  virtual bool PostTask(const tracked_objects::Location& from_here,
                        const Closure& task) = 0;
};

}  // namespace internal
}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_THREADING_POST_TASK_AND_REPLY_IMPL_H_