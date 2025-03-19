// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/process/kill.h"

#include "crbase/process/process_iterator.h"

namespace crbase {

bool KillProcesses(const FilePath::StringType& executable_name,
                   int exit_code,
                   const ProcessFilter* filter) {
  bool result = true;
  NamedProcessIterator iter(executable_name, filter);
  while (const ProcessEntry* entry = iter.NextProcessEntry()) {
    Process process = Process::Open(entry->pid());
    result &= process.Terminate(exit_code, true);
  }
  return result;
}

}  // namespace crbase