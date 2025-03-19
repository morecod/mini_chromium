// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/process/launch.h"
#include "crbase/build_config.h"

namespace crbase {

LaunchOptions::LaunchOptions()
    : wait(false),
#if defined(MINI_CHROMIUM_OS_WIN)
      start_hidden(false),
      elevated(false),
      handles_to_inherit(NULL),
      inherit_handles(false),
      as_user(NULL),
      empty_desktop_name(false),
      job_handle(NULL),
      stdin_handle(NULL),
      stdout_handle(NULL),
      stderr_handle(NULL),
      force_breakaway_from_job_(false),
      grant_foreground_privilege(false)
#else
#if defined(MINI_CHROMIUM_OS_LINUX)
      , clone_flags(0)
      , allow_new_privs(false)
      , kill_on_parent_death(false)
#endif  // MINI_CHROMIUM_OS_LINUX
#if defined(MINI_CHROMIUM_OS_POSIX)
      , pre_exec_delegate(NULL)
#endif  // MINI_CHROMIUM_OS_POSIX
#endif  // !defined(MINI_CHROMIUM_OS_WIN)
    {
}

LaunchOptions::~LaunchOptions() {
}

}  // namespace crbase