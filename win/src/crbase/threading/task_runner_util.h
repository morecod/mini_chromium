// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_CRBASE_THREADING_TASK_RUNNER_UTIL_H_
#define MINI_CHROMIUM_CRBASE_THREADING_TASK_RUNNER_UTIL_H_

#include <utility>

#include "crbase/functional/bind.h"
#include "crbase/functional/bind_helpers.h"
#include "crbase/functional/callback.h"
#include "crbase/logging.h"
#include "crbase/threading/task_runner.h"

namespace cr {

namespace internal {

// Adapts a function that produces a result via a return value to
// one that returns via an output parameter.
template <typename ReturnType>
void ReturnAsParamAdapter(OnceCallback<ReturnType()> func, ReturnType* result) {
  *result = std::move(func).Run();
}

// Adapts a T* result to a callblack that expects a T.
template <typename TaskReturnType, typename ReplyArgType>
void ReplyAdapter(OnceCallback<void(ReplyArgType)> callback,
                  TaskReturnType* result) {
  std::move(callback).Run(std::move(*result));
}

}  // namespace internal

// When you have these methods
//
//   R DoWorkAndReturn();
//   void Callback(const R& result);
//
// and want to call them in a PostTaskAndReply kind of fashion where the
// result of DoWorkAndReturn is passed to the Callback, you can use
// PostTaskAndReplyWithResult as in this example:
//
// PostTaskAndReplyWithResult(
//     target_thread_.task_runner(),
//     FROM_HERE,
//     BindOnce(&DoWorkAndReturn),
//     BindOnce(&Callback));
template <typename TaskReturnType, typename ReplyArgType>
bool PostTaskAndReplyWithResult(TaskRunner* task_runner,
                                const tracked_objects::Location& from_here,
                                OnceCallback<TaskReturnType()> task,
                                OnceCallback<void(ReplyArgType)> reply) {
  CR_DCHECK(task);
  CR_DCHECK(reply);
  TaskReturnType* result = new TaskReturnType();
  return task_runner->PostTaskAndReply(
      from_here,
      BindOnce(&internal::ReturnAsParamAdapter<TaskReturnType>, std::move(task),
               result),
      BindOnce(&internal::ReplyAdapter<TaskReturnType, ReplyArgType>,
               std::move(reply), Owned(result)));
}

// Callback version of PostTaskAndReplyWithResult above.
// Though RepeatingCallback is convertible to OnceCallback, we need this since
// we cannot use template deduction and object conversion at once on the
// overload resolution.
// TODO(crbug.com/714018): Update all callers of the Callback version to use
// OnceCallback.
template <typename TaskReturnType, typename ReplyArgType>
bool PostTaskAndReplyWithResult(TaskRunner* task_runner,
                                const tracked_objects::Location& from_here,
                                Callback<TaskReturnType()> task,
                                Callback<void(ReplyArgType)> reply) {
  return PostTaskAndReplyWithResult(
      task_runner, from_here, OnceCallback<TaskReturnType()>(std::move(task)),
      OnceCallback<void(ReplyArgType)>(std::move(reply)));
}

}  // namespace cr

#endif  // MINI_CHROMIUM_CRBASE_THREADING_TASK_RUNNER_UTIL_H_