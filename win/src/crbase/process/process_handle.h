// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_PROCESS_PROCESS_HANDLE_H_
#define MINI_CHROMIUM_SRC_CRBASE_PROCESS_PROCESS_HANDLE_H_

#include <windows.h>

#include <stdint.h>
#include <sys/types.h>

#include "crbase/base_export.h"
#include "crbase/files/file_path.h"


namespace crbase {

// ProcessHandle is a platform specific type which represents the underlying OS
// handle to a process.
// ProcessId is a number which identifies the process in the OS.
typedef HANDLE ProcessHandle;
typedef DWORD ProcessId;
typedef HANDLE UserTokenHandle;
const ProcessHandle kNullProcessHandle = NULL;
const ProcessId kNullProcessId = 0;

// Returns the id of the current process.
// Note that on some platforms, this is not guaranteed to be unique across
// processes (use GetUniqueIdForProcess if uniqueness is required).
CRBASE_EXPORT ProcessId GetCurrentProcId();

// Returns a unique ID for the current process. The ID will be unique across all
// currently running processes within the chrome session, but IDs of terminated
// processes may be reused. This returns an opaque value that is different from
// a process's PID.
CRBASE_EXPORT uint32_t GetUniqueIdForProcess();

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

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_PROCESS_PROCESS_HANDLE_H_