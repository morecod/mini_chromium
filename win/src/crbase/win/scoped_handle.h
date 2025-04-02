// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_WIN_SCOPED_HANDLE_H_
#define MINI_CHROMIUM_SRC_CRBASE_WIN_SCOPED_HANDLE_H_

#include <windows.h>
#include <intrin.h>

#include "crbase/build_config.h"
#include "crbase/base_export.h"
#include "crbase/logging.h"
#include "crbase/macros.h"
#include "crbase/tracing/location.h"

// TODO(rvargas): remove this with the rest of the verifier.
#if defined(MINI_CHROMIUM_COMPILER_MSVC)
#define CR_WIN_GET_CALLER _ReturnAddress()
#elif defined(MINI_CHROMIUM_COMPILER_GCC)
#define CR_WIN_GET_CALLER __builtin_extract_return_addr(\
    __builtin_return_address(0))
#endif

namespace cr {
namespace win {

// Generic wrapper for raw handles that takes care of closing handles
// automatically. The class interface follows the style of
// the ScopedFILE class with two additions:
//   - IsValid() method can tolerate multiple invalid handle values such as NULL
//     and INVALID_HANDLE_VALUE (-1) for Win32 handles.
//   - Set() (and the constructors and assignment operators that call it)
//     preserve the Windows LastError code. This ensures that GetLastError() can
//     be called after stashing a handle in a GenericScopedHandle object. Doing
//     this explicitly is necessary because of bug 528394 and VC++ 2015.
template <class Traits, class Verifier>
class GenericScopedHandle {
 public:
  GenericScopedHandle(const GenericScopedHandle&) = delete;
  GenericScopedHandle& operator=(const GenericScopedHandle&) = delete;
  GenericScopedHandle &&Pass() { return std::move(*this); }
  typedef void MoveOnlyTypeForCPP03;

 public:
  typedef typename Traits::Handle Handle;

  GenericScopedHandle() : handle_(Traits::NullHandle()) {}

  explicit GenericScopedHandle(Handle handle) : handle_(Traits::NullHandle()) {
    Set(handle);
  }

  GenericScopedHandle(GenericScopedHandle&& other)
      : handle_(Traits::NullHandle()) {
    Set(other.Take());
  }

  ~GenericScopedHandle() {
    Close();
  }

  bool IsValid() const {
    return Traits::IsHandleValid(handle_);
  }

  GenericScopedHandle& operator=(GenericScopedHandle&& other) {
    CR_DCHECK_NE(this, &other);
    Set(other.Take());
    return *this;
  }

  void Set(Handle handle) {
    if (handle_ != handle) {
      // Preserve old LastError to avoid bug 528394.
      auto last_error = ::GetLastError();
      Close();

      if (Traits::IsHandleValid(handle)) {
        handle_ = handle;
        Verifier::StartTracking(handle, this, CR_WIN_GET_CALLER,
                                tracked_objects::GetProgramCounter());
      }
      ::SetLastError(last_error);
    }
  }

  Handle Get() const {
    return handle_;
  }

  // Transfers ownership away from this object.
  Handle Take() {
    Handle temp = handle_;
    handle_ = Traits::NullHandle();
    if (Traits::IsHandleValid(temp)) {
      Verifier::StopTracking(temp, this, CR_WIN_GET_CALLER,
                             tracked_objects::GetProgramCounter());
    }
    return temp;
  }

  // Explicitly closes the owned handle.
  void Close() {
    if (Traits::IsHandleValid(handle_)) {
      Verifier::StopTracking(handle_, this, CR_WIN_GET_CALLER,
                             tracked_objects::GetProgramCounter());

      Traits::CloseHandle(handle_);
      handle_ = Traits::NullHandle();
    }
  }

  Handle* Receive() {
    return &handle_;
  }

 private:
  Handle handle_;
};

#undef CR_WIN_GET_CALLER

// The traits class for Win32 handles that can be closed via CloseHandle() API.
class HandleTraits {
 public:
  typedef HANDLE Handle;

  HandleTraits() = delete;
  HandleTraits(const HandleTraits&) = delete;
  HandleTraits& operator=(const HandleTraits&) = delete;

  // Closes the handle.
  static bool CRBASE_EXPORT CloseHandle(HANDLE handle);

  // Returns true if the handle value is valid.
  static bool IsHandleValid(HANDLE handle) {
    return handle != NULL && handle != INVALID_HANDLE_VALUE;
  }

  // Returns NULL handle value.
  static HANDLE NullHandle() {
    return NULL;
  }
};

// Do-nothing verifier.
class DummyVerifierTraits {
 public:
  typedef HANDLE Handle;

  DummyVerifierTraits() = delete;
  DummyVerifierTraits(const DummyVerifierTraits&) = delete;
  DummyVerifierTraits& operator=(const DummyVerifierTraits&) = delete;

  static void StartTracking(HANDLE handle, const void* owner,
                            const void* pc1, const void* pc2) {}
  static void StopTracking(HANDLE handle, const void* owner,
                           const void* pc1, const void* pc2) {}
};


typedef GenericScopedHandle<HandleTraits, DummyVerifierTraits> ScopedHandle;

}  // namespace win
}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_WIN_SCOPED_HANDLE_H_
