// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_MEMORY_SHARED_MEMORY_HANDLE_H_
#define MINI_CHROMIUM_SRC_CRBASE_MEMORY_SHARED_MEMORY_HANDLE_H_

#include <stddef.h>

#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_OS_WIN)
#include <windows.h>
#include "crbase/process/process_handle.h"
#elif defined(MINI_CHROMIUM_OS_POSIX)
#include <sys/types.h>
#include "crbase/file_descriptor_posix.h"
#endif

namespace cr {

class Pickle;

// SharedMemoryHandle is a platform specific type which represents
// the underlying OS handle to a shared memory segment.
#if defined(MINI_CHROMIUM_OS_POSIX)
typedef FileDescriptor SharedMemoryHandle;
#elif defined(MINI_CHROMIUM_OS_WIN)
class CRBASE_EXPORT SharedMemoryHandle {
 public:
  // The default constructor returns an invalid SharedMemoryHandle.
  SharedMemoryHandle();
  SharedMemoryHandle(HANDLE h, ProcessId pid);

  // Standard copy constructor. The new instance shares the underlying OS
  // primitives.
  SharedMemoryHandle(const SharedMemoryHandle& handle);

  // Standard assignment operator. The updated instance shares the underlying
  // OS primitives.
  SharedMemoryHandle& operator=(const SharedMemoryHandle& handle);

  // Comparison operators.
  bool operator==(const SharedMemoryHandle& handle) const;
  bool operator!=(const SharedMemoryHandle& handle) const;

  // Closes the underlying OS resources.
  void Close() const;

  // Whether the underlying OS primitive is valid.
  bool IsValid() const;

  // Whether |pid_| is the same as the current process's id.
  bool BelongsToCurrentProcess() const;

  // Whether handle_ needs to be duplicated into the destination process when
  // an instance of this class is passed over a Chrome IPC channel.
  bool NeedsBrokering() const;

  HANDLE GetHandle() const;
  ProcessId GetPID() const;

 private:
  HANDLE handle_;

  // The process in which |handle_| is valid and can be used. If |handle_| is
  // invalid, this will be kNullProcessId.
  ProcessId pid_;
};
#endif

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_MEMORY_SHARED_MEMORY_HANDLE_H_