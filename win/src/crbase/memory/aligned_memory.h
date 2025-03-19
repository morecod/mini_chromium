// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AlignedMemory is a POD type that gives you a portable way to specify static
// or local stack data of a given alignment and size. For example, if you need
// static storage for a class, but you want manual control over when the object
// is constructed and destructed (you don't want static initialization and
// destruction), use AlignedMemory:
//
//   static AlignedMemory<sizeof(MyClass), CR_ALIGNOF(MyClass)> my_class;
//
//   // ... at runtime:
//   new(my_class.void_data()) MyClass();
//
//   // ... use it:
//   MyClass* mc = my_class.data_as<MyClass>();
//
//   // ... later, to destruct my_class:
//   my_class.data_as<MyClass>()->MyClass::~MyClass();
//
// Alternatively, a runtime sized aligned allocation can be created:
//
//   float* my_array = static_cast<float*>(AlignedAlloc(size, alignment));
//
//   // ... later, to release the memory:
//   AlignedFree(my_array);
//
// Or using std::unique_ptr:
//
//   std::unique_ptr<float, AlignedFreeDeleter> my_array(
//       static_cast<float*>(AlignedAlloc(size, alignment)));

#ifndef MINI_CHROMIUM_SRC_CRBASE_MEMORY_ALIGNED_MEMORY_H_
#define MINI_CHROMIUM_SRC_CRBASE_MEMORY_ALIGNED_MEMORY_H_

#include <stddef.h>
#include <stdint.h>

#include "crbase/base_export.h"
#include "crbase/compiler_specific.h"
#include "crbase/build_config.h"

#if defined(_MSC_VER)
#include <malloc.h>
#else
#include <stdlib.h>
#endif

namespace crbase {

// AlignedMemory is specialized for all supported alignments.
// Make sure we get a compiler error if someone uses an unsupported alignment.
template <size_t Size, size_t ByteAlignment>
struct AlignedMemory {};

#define CR_DECL_ALIGNED_MEMORY(byte_alignment)                                \
  template <size_t Size>                                                      \
  class AlignedMemory<Size, byte_alignment> {                                 \
   public:                                                                    \
    CR_ALIGNAS(byte_alignment) uint8_t data_[Size];                           \
    void* void_data() { return static_cast<void*>(data_); }                   \
    const void* void_data() const { return static_cast<const void*>(data_); } \
    template <typename Type>                                                  \
    Type* data_as() {                                                         \
      return static_cast<Type*>(void_data());                                 \
    }                                                                         \
    template <typename Type>                                                  \
    const Type* data_as() const {                                             \
      return static_cast<const Type*>(void_data());                           \
    }                                                                         \
                                                                              \
   private:                                                                   \
    void* operator new(size_t);                                               \
    void operator delete(void*);                                              \
  }

// Specialization for all alignments is required because MSVC (as of VS 2008)
// does not understand ALIGNAS(ALIGNOF(Type)) or ALIGNAS(template_param).
// Greater than 4096 alignment is not supported by some compilers, so 4096 is
// the maximum specified here.
CR_DECL_ALIGNED_MEMORY(1);
CR_DECL_ALIGNED_MEMORY(2);
CR_DECL_ALIGNED_MEMORY(4);
CR_DECL_ALIGNED_MEMORY(8);
CR_DECL_ALIGNED_MEMORY(16);
CR_DECL_ALIGNED_MEMORY(32);
CR_DECL_ALIGNED_MEMORY(64);
CR_DECL_ALIGNED_MEMORY(128);
CR_DECL_ALIGNED_MEMORY(256);
CR_DECL_ALIGNED_MEMORY(512);
CR_DECL_ALIGNED_MEMORY(1024);
CR_DECL_ALIGNED_MEMORY(2048);
CR_DECL_ALIGNED_MEMORY(4096);

#undef CR_DECL_ALIGNED_MEMORY

CRBASE_EXPORT void* AlignedAlloc(size_t size, size_t alignment);

inline void AlignedFree(void* ptr) {
#if defined(MINI_CHROMIUM_OS_WIN)
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}

// Deleter for use with std::unique_ptr. E.g., use as
//   std::unique_ptr<Foo, crbase::AlignedFreeDeleter> foo;
struct AlignedFreeDeleter {
  inline void operator()(void* ptr) const {
    AlignedFree(ptr);
  }
};

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_MEMORY_ALIGNED_MEMORY_H_
