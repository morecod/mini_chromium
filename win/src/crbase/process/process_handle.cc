// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/process/process_handle.h"

#include <stdint.h>
#include <windows.h>
#include <tlhelp32.h>

#include "crbase/logging.h"
#include "crbase/win/scoped_handle.h"

namespace crbase {

namespace {
bool g_have_unique_id = false;
uint32_t g_unique_id;

// The process which set |g_unique_id|.
ProcessId g_procid;

// Mangle IDs so that they are not accidentally used as PIDs, e.g. as an
// argument to kill or waitpid.
uint32_t MangleProcessId(ProcessId process_id) {
  // Add a large power of 10 so that the pid is still the pid is still readable
  // inside the mangled id.
  return static_cast<uint32_t>(process_id) + 1000000000U;
}

}  // namespace

uint32_t GetUniqueIdForProcess() {
  if (!g_have_unique_id) {
    return MangleProcessId(GetCurrentProcId());
  }

  // Make sure we are the same process that set |g_procid|. This check may have
  // false negatives (if a process ID was reused) but should have no false
  // positives.
  CR_DCHECK_EQ(GetCurrentProcId(), g_procid);
  return g_unique_id;
}

ProcessId GetCurrentProcId() {
  return ::GetCurrentProcessId();
}

ProcessHandle GetCurrentProcessHandle() {
  return ::GetCurrentProcess();
}

ProcessId GetProcId(ProcessHandle process) {
  // This returns 0 if we have insufficient rights to query the process handle.
  return GetProcessId(process);
}

ProcessId GetParentProcessId(ProcessHandle process) {
  ProcessId child_pid = GetProcId(process);
  PROCESSENTRY32 process_entry;
      process_entry.dwSize = sizeof(PROCESSENTRY32);

  win::ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (snapshot.IsValid() && Process32First(snapshot.Get(), &process_entry)) {
    do {
      if (process_entry.th32ProcessID == child_pid)
        return process_entry.th32ParentProcessID;
    } while (Process32Next(snapshot.Get(), &process_entry));
  }

  return 0u;
}

}  // namespace crbase