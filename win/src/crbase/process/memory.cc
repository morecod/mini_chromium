// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/process/memory.h"
#include "crbase/debug/alias.h"
#include "crbase/logging.h"

namespace crbase {

namespace {

// Breakpad server classifies base::`anonymous namespace'::OnNoMemory as
// out-of-memory crash.
#if defined(_MSC_VER)
__declspec(noinline)
#endif
void OnNoMemory(size_t size) {
  size_t tmp_size = size;
  crbase::debug::Alias(&tmp_size);
  CR_LOG(FATAL) << "Out of memory. size=" << tmp_size;
}

}  // namespace

void TerminateBecauseOutOfMemory(size_t size) {
  OnNoMemory(size);
}

bool UncheckedCalloc(size_t num_items, size_t size, void** result) {
  const size_t alloc_size = num_items * size;

  // Overflow check
  if (size && ((alloc_size / size) != num_items)) {
    *result = NULL;
    return false;
  }

  if (!UncheckedMalloc(alloc_size, result))
    return false;

  memset(*result, 0, alloc_size);
  return true;
}

}  // namespace crbase