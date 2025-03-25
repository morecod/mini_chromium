// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_MESSAGE_LOOP_MESSAGE_LOOP_TASK_RUNNER_H_
#define MINI_CHROMIUM_SRC_CRBASE_MESSAGE_LOOP_MESSAGE_LOOP_TASK_RUNNER_H_

#include "crbase/base_export.h"
#include "crbase/macros.h"
#include "crbase/memory/ref_counted.h"
#include "crbase/threading/pending_task.h"
#include "crbase/threading/single_thread_task_runner.h"
#include "crbase/synchronization/lock.h"
#include "crbase/threading/platform_thread.h"

namespace crbase {
namespace internal {

class IncomingTaskQueue;

// A stock implementation of SingleThreadTaskRunner that is created and managed
// by a MessageLoop. For now a MessageLoopTaskRunner can only be created as
// part of a MessageLoop.
class CRBASE_EXPORT MessageLoopTaskRunner : public SingleThreadTaskRunner {
 public:
  MessageLoopTaskRunner(const MessageLoopTaskRunner&) = delete;
  MessageLoopTaskRunner& operator=(const MessageLoopTaskRunner&) = delete;

  explicit MessageLoopTaskRunner(
      scoped_refptr<IncomingTaskQueue> incoming_queue);

  // Initialize this message loop task runner on the current thread.
  void BindToCurrentThread();

  // SingleThreadTaskRunner implementation
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       OnceClosure task,
                       crbase::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  OnceClosure task,
                                  crbase::TimeDelta delay) override;
  bool RunsTasksOnCurrentThread() const override;

 private:
  friend class RefCountedThreadSafe<MessageLoopTaskRunner>;
  ~MessageLoopTaskRunner() override;

  // The incoming queue receiving all posted tasks.
  scoped_refptr<IncomingTaskQueue> incoming_queue_;

  // ID of the thread |this| was created on.  Could be accessed on multiple
  // threads, protected by |valid_thread_id_lock_|.
  PlatformThreadId valid_thread_id_;
  mutable Lock valid_thread_id_lock_;
};

}  // namespace internal
}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_MESSAGE_LOOP_MESSAGE_LOOP_TASK_RUNNER_H_