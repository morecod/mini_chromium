// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_WIN_SCOPED_CO_MEM_H_
#define MINI_CHROMIUM_SRC_CRBASE_WIN_SCOPED_CO_MEM_H_

#include <objbase.h>

#include "crbase/macros.h"
#include "crbase/logging.h"


namespace cr {
namespace win {

// Simple scoped memory releaser class for COM allocated memory.
// Example:
//   cr::win::ScopedCoMem<ITEMIDLIST> file_item;
//   SHGetSomeInfo(&file_item, ...);
//   ...
//   return;  <-- memory released
template<typename T>
class ScopedCoMem {
 public:
  ScopedCoMem(const ScopedCoMem<T>&) = delete;
  ScopedCoMem<T>& operator=(const ScopedCoMem<T>&) = delete;

  ScopedCoMem() : mem_ptr_(NULL) {}
  ~ScopedCoMem() {
    Reset(NULL);
  }

  T** operator&() {  // NOLINT
    CR_DCHECK(mem_ptr_ == NULL);  // To catch memory leaks.
    return &mem_ptr_;
  }

  operator T*() {
    return mem_ptr_;
  }

  T* operator->() {
    CR_DCHECK(mem_ptr_ != NULL);
    return mem_ptr_;
  }

  const T* operator->() const {
    CR_DCHECK(mem_ptr_ != NULL);
    return mem_ptr_;
  }

  void Reset(T* ptr) {
    if (mem_ptr_)
      CoTaskMemFree(mem_ptr_);
    mem_ptr_ = ptr;
  }

  T* get() const {
    return mem_ptr_;
  }

 private:
  T* mem_ptr_;
};

}  // namespace win
}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_WIN_SCOPED_CO_MEM_H_