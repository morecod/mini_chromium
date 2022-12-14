// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINLIB_WINBASE_TASK_SCHEDULER_SCHEDULER_WORKER_PARAMS_H_
#define WINLIB_WINBASE_TASK_SCHEDULER_SCHEDULER_WORKER_PARAMS_H_

namespace winbase {

enum class SchedulerBackwardCompatibility {
  // No backward compatibility.
  DISABLED,

  // On Windows, initialize COM STA to mimic SequencedWorkerPool and
  // BrowserThreadImpl. Behaves like DISABLED on other platforms.
  // TODO(fdoray): Get rid of this and force tasks that care about a
  // CoInitialized environment to request one explicitly (via an upcoming
  // execution mode).
  INIT_COM_STA,
};

}  // namespace winbase

#endif  // WINLIB_WINBASE_TASK_SCHEDULER_SCHEDULER_WORKER_PARAMS_H_