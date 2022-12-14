// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "winbase\win\scoped_native_library.h"

namespace winbase {

ScopedNativeLibrary::ScopedNativeLibrary() : library_(nullptr) {}

ScopedNativeLibrary::ScopedNativeLibrary(NativeLibrary library)
    : library_(library) {
}

ScopedNativeLibrary::ScopedNativeLibrary(const FilePath& library_path) {
  library_ = winbase::LoadNativeLibrary(library_path, nullptr);
}

ScopedNativeLibrary::~ScopedNativeLibrary() {
  if (library_)
    winbase::UnloadNativeLibrary(library_);
}

void* ScopedNativeLibrary::GetFunctionPointer(
    const char* function_name) const {
  if (!library_)
    return nullptr;
  return winbase::GetFunctionPointerFromNativeLibrary(library_, function_name);
}

void ScopedNativeLibrary::Reset(NativeLibrary library) {
  if (library_)
    winbase::UnloadNativeLibrary(library_);
  library_ = library;
}

NativeLibrary ScopedNativeLibrary::Release() {
  NativeLibrary result = library_;
  library_ = nullptr;
  return result;
}

}  // namespace winbase