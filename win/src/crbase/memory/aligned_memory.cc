// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/memory/aligned_memory.h"

#include "crbase/logging.h"
#include "crbase/build_config.h"

namespace crbase {

void* AlignedAlloc(size_t size, size_t alignment) {
  CR_DCHECK_GT(size, 0U);
  CR_DCHECK_EQ(alignment & (alignment - 1), 0U);
  CR_DCHECK_EQ(alignment % sizeof(void*), 0U);
  void* ptr = NULL;
#if defined(MINI_CHROMIUM_OS_WIN)
  ptr = _aligned_malloc(size, alignment);
#else
  if (posix_memalign(&ptr, alignment, size))
    ptr = NULL;
#endif
  // Since aligned allocations may fail for non-memory related reasons, force a
  // crash if we encounter a failed allocation; maintaining consistent behavior
  // with a normal allocation failure in Chrome.
  if (!ptr) {
    CR_DLOG(ERROR)
        << "If you crashed here, your aligned allocation is incorrect: "
        << "size=" << size << ", alignment=" << alignment;
    CR_CHECK(false);
  }
  // Sanity check alignment just to be safe.
  CR_DCHECK_EQ(reinterpret_cast<uintptr_t>(ptr) & (alignment - 1), 0U);
  return ptr;
}

}  // namespace crbase
