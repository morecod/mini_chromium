// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/threading/thread_task_runner_handle.h"

#include "crbase/lazy_instance.h"
#include "crbase/threading/single_thread_task_runner.h"
#include "crbase/threading/thread_local.h"

namespace crbase {

namespace {

crbase::LazyInstance<crbase::ThreadLocalPointer<ThreadTaskRunnerHandle> >::Leaky
    lazy_tls_ptr = CR_LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
scoped_refptr<SingleThreadTaskRunner> ThreadTaskRunnerHandle::Get() {
  ThreadTaskRunnerHandle* current = lazy_tls_ptr.Pointer()->Get();
  CR_DCHECK(current);
  return current->task_runner_;
}

// static
bool ThreadTaskRunnerHandle::IsSet() {
  return lazy_tls_ptr.Pointer()->Get() != NULL;
}

ThreadTaskRunnerHandle::ThreadTaskRunnerHandle(
    const scoped_refptr<SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner) {
  CR_DCHECK(task_runner_->BelongsToCurrentThread());
  CR_DCHECK(!lazy_tls_ptr.Pointer()->Get());
  lazy_tls_ptr.Pointer()->Set(this);
}

ThreadTaskRunnerHandle::~ThreadTaskRunnerHandle() {
  CR_DCHECK(task_runner_->BelongsToCurrentThread());
  CR_DCHECK_EQ(lazy_tls_ptr.Pointer()->Get(), this);
  lazy_tls_ptr.Pointer()->Set(NULL);
}

}  // namespace ESDK