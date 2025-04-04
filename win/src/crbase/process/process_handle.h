// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_PROCESS_PROCESS_HANDLE_H_
#define MINI_CHROMIUM_SRC_CRBASE_PROCESS_PROCESS_HANDLE_H_

#include <stdint.h>
#include <sys/types.h>

#include "crbase/base_export.h"
#include "crbase/files/file_path.h"
#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_OS_WIN)
#include <windows.h>
#endif


namespace cr {

// ProcessHandle is a platform specific type which represents the underlying OS
// handle to a process.
// ProcessId is a number which identifies the process in the OS.
#if defined(MINI_CHROMIUM_OS_WIN)
typedef HANDLE ProcessHandle;
typedef DWORD ProcessId;
typedef HANDLE UserTokenHandle;
const ProcessHandle kNullProcessHandle = NULL;
const ProcessId kNullProcessId = 0;
#elif defined(MINI_CHROMIUM_OS_POSIX)
// On POSIX, our ProcessHandle will just be the PID.
typedef pid_t ProcessHandle;
typedef pid_t ProcessId;
const ProcessHandle kNullProcessHandle = 0;
const ProcessId kNullProcessId = 0;
#endif

// Returns the id of the current process.
// Note that on some platforms, this is not guaranteed to be unique across
// processes (use GetUniqueIdForProcess if uniqueness is required).
CRBASE_EXPORT ProcessId GetCurrentProcId();

// Returns a unique ID for the current process. The ID will be unique across all
// currently running processes within the chrome session, but IDs of terminated
// processes may be reused. This returns an opaque value that is different from
// a process's PID.
CRBASE_EXPORT uint32_t GetUniqueIdForProcess();

#if defined(MINI_CHROMIUM_OS_LINUX)
// When a process is started in a different PID namespace from the browser
// process, this function must be called with the process's PID in the browser's
// PID namespace in order to initialize its unique ID. Not thread safe.
// WARNING: To avoid inconsistent results from GetUniqueIdForProcess, this
// should only be called very early after process startup - ideally as soon
// after process creation as possible.
CRBASE_EXPORT void InitUniqueIdForProcessInPidNamespace(
    ProcessId pid_outside_of_namespace);
#endif

// Returns the ProcessHandle of the current process.
CRBASE_EXPORT ProcessHandle GetCurrentProcessHandle();

// Returns the process ID for the specified process. This is functionally the
// same as Windows' GetProcessId(), but works on versions of Windows before Win
// XP SP1 as well.
// DEPRECATED. New code should be using Process::Pid() instead.
// Note that on some platforms, this is not guaranteed to be unique across
// processes.
CRBASE_EXPORT ProcessId GetProcId(ProcessHandle process);

// Returns the ID for the parent of the given process.
CRBASE_EXPORT ProcessId GetParentProcessId(ProcessHandle process);

#if defined(MINI_CHROMIUM_OS_POSIX)
// Returns the path to the executable of the given process.
CRBASE_EXPORT FilePath GetProcessExecutablePath(ProcessHandle process);
#endif

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_PROCESS_PROCESS_HANDLE_H_