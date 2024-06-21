// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_PROCESS_MEMORY_H_
#define MINI_CHROMIUM_SRC_CRBASE_PROCESS_MEMORY_H_

#include <windows.h>
#include <stddef.h>

#include "crbase/base_export.h"
#include "crbase/process/process_handle.h"
#include "crbase/compiler_specific.h"

namespace crbase {

// Enables low fragmentation heap (LFH) for every heaps of this process. This
// won't have any effect on heaps created after this function call. It will not
// modify data allocated in the heaps before calling this function. So it is
// better to call this function early in initialization and again before
// entering the main loop.
// Note: Returns true on Windows 2000 without doing anything.
CRBASE_EXPORT bool EnableLowFragmentationHeap();

// Enables 'terminate on heap corruption' flag. Helps protect against heap
// overflow. Has no effect if the OS doesn't provide the necessary facility.
CRBASE_EXPORT void EnableTerminationOnHeapCorruption();

// Turns on process termination if memory runs out.
CRBASE_EXPORT void EnableTerminationOnOutOfMemory();

// Terminates process. Should be called only for out of memory errors.
// Crash reporting classifies such crashes as OOM.
CRBASE_EXPORT void TerminateBecauseOutOfMemory(size_t size);

// Returns the module handle to which an address belongs. The reference count
// of the module is not incremented.
CRBASE_EXPORT HMODULE GetModuleFromAddress(void* address);

// Special allocator functions for callers that want to check for OOM.
// These will not abort if the allocation fails even if
// EnableTerminationOnOutOfMemory has been called.
// This can be useful for huge and/or unpredictable size memory allocations.
// Please only use this if you really handle the case when the allocation
// fails. Doing otherwise would risk security.
// These functions may still crash on OOM when running under memory tools,
// specifically ASan and other sanitizers.
// Return value tells whether the allocation succeeded. If it fails |result| is
// set to NULL, otherwise it holds the memory address.
CRBASE_EXPORT CR_WARN_UNUSED_RESULT bool UncheckedMalloc(
    size_t size,
    void** result);
CRBASE_EXPORT CR_WARN_UNUSED_RESULT bool UncheckedCalloc(
    size_t num_items,
    size_t size,
    void** result);

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_PROCESS_MEMORY_H_