// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_ATOMIC_ATOMIC_SEQUENCE_NUM_H_
#define MINI_CHROMIUM_SRC_CRBASE_ATOMIC_ATOMIC_SEQUENCE_NUM_H_

#include "crbase/macros.h"
#include "crbase/atomic/atomicops.h"

namespace cr {

class AtomicSequenceNumber;

// Static (POD) AtomicSequenceNumber that MUST be used in global scope (or
// non-function scope) ONLY. This implementation does not generate any static
// initializer.  Note that it does not implement any constructor which means
// that its fields are not initialized except when it is stored in the global
// data section (.data in ELF). If you want to allocate an atomic sequence
// number on the stack (or heap), please use the AtomicSequenceNumber class
// declared below.
class StaticAtomicSequenceNumber {
 public:
  inline int GetNext() {
    return static_cast<int>(
        cr::subtle::NoBarrier_AtomicIncrement(&seq_, 1) - 1);
  }

 private:
  friend class AtomicSequenceNumber;

  inline void Reset() {
    cr::subtle::Release_Store(&seq_, 0);
  }

  cr::subtle::Atomic32 seq_;
};

// AtomicSequenceNumber that can be stored and used safely (i.e. its fields are
// always initialized as opposed to StaticAtomicSequenceNumber declared above).
// Please use StaticAtomicSequenceNumber if you want to declare an atomic
// sequence number in the global scope.
class AtomicSequenceNumber {
 public:
  AtomicSequenceNumber(const AtomicSequenceNumber&) = delete;
  AtomicSequenceNumber& operator=(const AtomicSequenceNumber&) = delete;

  AtomicSequenceNumber() {
    seq_.Reset();
  }

  inline int GetNext() {
    return seq_.GetNext();
  }

 private:
  StaticAtomicSequenceNumber seq_;
};

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_ATOMIC_ATOMIC_SEQUENCE_NUM_H_