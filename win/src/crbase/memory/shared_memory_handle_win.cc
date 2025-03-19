// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/memory/shared_memory_handle.h"

#include "crbase/logging.h"

namespace crbase {

SharedMemoryHandle::SharedMemoryHandle()
    : handle_(nullptr), pid_(kNullProcessId) {}

SharedMemoryHandle::SharedMemoryHandle(HANDLE h, ProcessId pid)
    : handle_(h), pid_(pid) {}

SharedMemoryHandle::SharedMemoryHandle(const SharedMemoryHandle& handle)
    : handle_(handle.handle_), pid_(handle.pid_) {}

SharedMemoryHandle& SharedMemoryHandle::operator=(
    const SharedMemoryHandle& handle) {
  if (this == &handle)
    return *this;

  handle_ = handle.handle_;
  pid_ = handle.pid_;
  return *this;
}

bool SharedMemoryHandle::operator==(const SharedMemoryHandle& handle) const {
  // Invalid handles are always equal.
  if (!IsValid() && !handle.IsValid())
    return true;

  return handle_ == handle.handle_ && pid_ == handle.pid_;
}

bool SharedMemoryHandle::operator!=(const SharedMemoryHandle& handle) const {
  return !(*this == handle);
}

void SharedMemoryHandle::Close() const {
  CR_DCHECK(handle_ != nullptr);
  CR_DCHECK(BelongsToCurrentProcess());
  ::CloseHandle(handle_);
}

bool SharedMemoryHandle::IsValid() const {
  return handle_ != nullptr;
}

bool SharedMemoryHandle::BelongsToCurrentProcess() const {
  return pid_ == GetCurrentProcId();
}

bool SharedMemoryHandle::NeedsBrokering() const {
  return false;
}

HANDLE SharedMemoryHandle::GetHandle() const {
  return handle_;
}

ProcessId SharedMemoryHandle::GetPID() const {
  return pid_;
}

}  // namespace crbase