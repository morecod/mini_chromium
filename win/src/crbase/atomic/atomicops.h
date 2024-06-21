// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For atomic operations on reference counts, see atomic_refcount.h.
// For atomic operations on sequence numbers, see atomic_sequence_num.h.

// The routines exported by this module are subtle.  If you use them, even if
// you get the code right, it will depend on careful reasoning about atomicity
// and memory ordering; it will be less readable, and harder to maintain.  If
// you plan to use these routines, you should have a good reason, such as solid
// evidence that performance would otherwise suffer, or there being no
// alternative.  You should assume only properties explicitly guaranteed by the
// specifications in this file.  You are almost certainly _not_ writing code
// just for the x86; if you assume x86 semantics, x86 hardware bugs and
// implementations on other archtectures will cause your code to break.  If you
// do not know what you are doing, avoid these routines, and use a Mutex.
//
// It is incorrect to make direct assignments to/from an atomic variable.
// You should use one of the Load or Store routines.  The NoBarrier
// versions are provided when no barriers are needed:
//   NoBarrier_Store()
//   NoBarrier_Load()
// Although there are currently no compiler enforcement, you are encouraged
// to use these.
//

#ifndef MINI_CHROMIUM_SRC_CRBASE_ATOMIC_ATOMICOPS_H_
#define MINI_CHROMIUM_SRC_CRBASE_ATOMIC_ATOMICOPS_H_

#include <stdint.h>

// Small C++ header which defines implementation specific macros used to
// identify the STL implementation.
// - libc++: captures __config for _LIBCPP_VERSION
// - libstdc++: captures bits/c++config.h for __GLIBCXX__
#include <cstddef>
#include <atomic>

namespace crbase {
namespace subtle {

typedef int32_t Atomic32;
#if defined(_M_X64) || defined(__x86_64__)
// We need to be able to go between Atomic64 and AtomicWord implicitly.  This
// means Atomic64 and AtomicWord should be the same type on 64-bit.
#if defined(__ILP32__) 
// NaCl's intptr_t is not actually 64-bits on 64-bit!
// http://code.google.com/p/nativeclient/issues/detail?id=1162
typedef int64_t Atomic64;
#else
typedef intptr_t Atomic64;
#endif
#endif

// Use AtomicWord for a machine-sized pointer.  It will use the Atomic32 or
// Atomic64 routines below, depending on your architecture.
typedef intptr_t AtomicWord;

// Atomically execute:
//      result = *ptr;
//      if (*ptr == old_value)
//        *ptr = new_value;
//      return result;
//
// I.e., replace "*ptr" with "new_value" if "*ptr" used to be "old_value".
// Always return the old value of "*ptr"
//
// This routine implies no memory barriers.
Atomic32 NoBarrier_CompareAndSwap(volatile Atomic32* ptr,
                                  Atomic32 old_value,
                                  Atomic32 new_value);

// Atomically store new_value into *ptr, returning the previous value held in
// *ptr.  This routine implies no memory barriers.
Atomic32 NoBarrier_AtomicExchange(volatile Atomic32* ptr, Atomic32 new_value);

// Atomically increment *ptr by "increment".  Returns the new value of
// *ptr with the increment applied.  This routine implies no memory barriers.
Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32* ptr, Atomic32 increment);

Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                 Atomic32 increment);

// These following lower-level operations are typically useful only to people
// implementing higher-level synchronization operations like spinlocks,
// mutexes, and condition-variables.  They combine CompareAndSwap(), a load, or
// a store with appropriate memory-ordering instructions.  "Acquire" operations
// ensure that no later memory access can be reordered ahead of the operation.
// "Release" operations ensure that no previous memory access can be reordered
// after the operation.  "Barrier" operations have both "Acquire" and "Release"
// semantics.
Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                Atomic32 old_value,
                                Atomic32 new_value);
Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                Atomic32 old_value,
                                Atomic32 new_value);

void NoBarrier_Store(volatile Atomic32* ptr, Atomic32 value);
void Release_Store(volatile Atomic32* ptr, Atomic32 value);

Atomic32 NoBarrier_Load(volatile const Atomic32* ptr);
Atomic32 Acquire_Load(volatile const Atomic32* ptr);

// 64-bit atomic operations (only available on 64-bit processors).
#if defined(_M_X64) || defined(__x86_64__)
Atomic64 NoBarrier_CompareAndSwap(volatile Atomic64* ptr,
                                  Atomic64 old_value,
                                  Atomic64 new_value);
Atomic64 NoBarrier_AtomicExchange(volatile Atomic64* ptr, Atomic64 new_value);
Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64* ptr, Atomic64 increment);
Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr, Atomic64 increment);

Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                Atomic64 old_value,
                                Atomic64 new_value);
Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                Atomic64 old_value,
                                Atomic64 new_value);
void NoBarrier_Store(volatile Atomic64* ptr, Atomic64 value);
void Release_Store(volatile Atomic64* ptr, Atomic64 value);
Atomic64 NoBarrier_Load(volatile const Atomic64* ptr);
Atomic64 Acquire_Load(volatile const Atomic64* ptr);
#endif  // defined(_M_X64) || defined(__x86_64__)

// This implementation is transitional and maintains the original API for
// atomicops.h. This requires casting memory locations to the atomic types, and
// assumes that the API and the C++11 implementation are layout-compatible,
// which isn't true for all implementations or hardware platforms. The static
// assertion should detect this issue, were it to fire then this header
// shouldn't be used.
//
// TODO(jfb) If this header manages to stay committed then the API should be
//           modified, and all call sites updated.
typedef volatile std::atomic<Atomic32>* AtomicLocation32;
static_assert(sizeof(*(AtomicLocation32) nullptr) == sizeof(Atomic32),
              "incompatible 32-bit atomic layout");

inline Atomic32 NoBarrier_CompareAndSwap(volatile Atomic32* ptr,
                                         Atomic32 old_value,
                                         Atomic32 new_value) {
  ((AtomicLocation32)ptr)
      ->compare_exchange_strong(old_value,
                                new_value,
                                std::memory_order_relaxed,
                                std::memory_order_relaxed);
  return old_value;
}

inline Atomic32 NoBarrier_AtomicExchange(volatile Atomic32* ptr,
                                         Atomic32 new_value) {
  return ((AtomicLocation32)ptr)
      ->exchange(new_value, std::memory_order_relaxed);
}

inline Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32* ptr,
                                          Atomic32 increment) {
  return increment +
         ((AtomicLocation32)ptr)
             ->fetch_add(increment, std::memory_order_relaxed);
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return increment + ((AtomicLocation32)ptr)->fetch_add(increment);
}

inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  ((AtomicLocation32)ptr)
      ->compare_exchange_strong(old_value,
                                new_value,
                                std::memory_order_acquire,
                                std::memory_order_acquire);
  return old_value;
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  ((AtomicLocation32)ptr)
      ->compare_exchange_strong(old_value,
                                new_value,
                                std::memory_order_release,
                                std::memory_order_relaxed);
  return old_value;
}

inline void NoBarrier_Store(volatile Atomic32* ptr, Atomic32 value) {
  ((AtomicLocation32)ptr)->store(value, std::memory_order_relaxed);
}

inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  ((AtomicLocation32)ptr)->store(value, std::memory_order_release);
}

inline Atomic32 NoBarrier_Load(volatile const Atomic32* ptr) {
  return ((AtomicLocation32)ptr)->load(std::memory_order_relaxed);
}

inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  return ((AtomicLocation32)ptr)->load(std::memory_order_acquire);
}

#if defined(_M_X64) || defined(__x86_64__)

typedef volatile std::atomic<Atomic64>* AtomicLocation64;
static_assert(sizeof(*(AtomicLocation64) nullptr) == sizeof(Atomic64),
              "incompatible 64-bit atomic layout");

inline Atomic64 NoBarrier_CompareAndSwap(volatile Atomic64* ptr,
                                         Atomic64 old_value,
                                         Atomic64 new_value) {
  ((AtomicLocation64)ptr)
      ->compare_exchange_strong(old_value,
                                new_value,
                                std::memory_order_relaxed,
                                std::memory_order_relaxed);
  return old_value;
}

inline Atomic64 NoBarrier_AtomicExchange(volatile Atomic64* ptr,
                                         Atomic64 new_value) {
  return ((AtomicLocation64)ptr)
      ->exchange(new_value, std::memory_order_relaxed);
}

inline Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64* ptr,
                                          Atomic64 increment) {
  return increment +
         ((AtomicLocation64)ptr)
             ->fetch_add(increment, std::memory_order_relaxed);
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  return increment + ((AtomicLocation64)ptr)->fetch_add(increment);
}

inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  ((AtomicLocation64)ptr)
      ->compare_exchange_strong(old_value,
                                new_value,
                                std::memory_order_acquire,
                                std::memory_order_acquire);
  return old_value;
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  ((AtomicLocation64)ptr)
      ->compare_exchange_strong(old_value,
                                new_value,
                                std::memory_order_release,
                                std::memory_order_relaxed);
  return old_value;
}

inline void NoBarrier_Store(volatile Atomic64* ptr, Atomic64 value) {
  ((AtomicLocation64)ptr)->store(value, std::memory_order_relaxed);
}

inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
  ((AtomicLocation64)ptr)->store(value, std::memory_order_release);
}

inline Atomic64 NoBarrier_Load(volatile const Atomic64* ptr) {
  return ((AtomicLocation64)ptr)->load(std::memory_order_relaxed);
}

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  return ((AtomicLocation64)ptr)->load(std::memory_order_acquire);
}

#endif  // defined(_M_X64) || defined(__x86_64__)

}  // namespace subtle
}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_ATOMIC_ATOMICOPS_H_