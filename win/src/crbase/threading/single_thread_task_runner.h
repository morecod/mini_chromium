// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_THREADING_SINGLE_THREAD_TASK_RUNNER_H_
#define MINI_CHROMIUM_SRC_CRBASE_THREADING_SINGLE_THREAD_TASK_RUNNER_H_

#include "crbase/base_export.h"
#include "crbase/threading/sequenced_task_runner.h"

namespace cr {

// A SingleThreadTaskRunner is a SequencedTaskRunner with one more
// guarantee; namely, that all tasks are run on a single dedicated
// thread.  Most use cases require only a SequencedTaskRunner, unless
// there is a specific need to run tasks on only a single thread.
//
// SingleThreadTaskRunner implementations might:
//   - Post tasks to an existing thread's MessageLoop (see
//     MessageLoop::task_runner()).
//   - Create their own worker thread and MessageLoop to post tasks to.
//   - Add tasks to a FIFO and signal to a non-MessageLoop thread for them to
//     be processed. This allows TaskRunner-oriented code run on threads
//     running other kinds of message loop, e.g. Jingle threads.
class CRBASE_EXPORT SingleThreadTaskRunner : public SequencedTaskRunner {
 public:
  // A more explicit alias to RunsTasksOnCurrentThread().
  bool BelongsToCurrentThread() const {
    return RunsTasksOnCurrentThread();
  }

 protected:
  ~SingleThreadTaskRunner() override {}
};

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_THREADING_SINGLE_THREAD_TASK_RUNNER_H_