// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINLIB_WINBASE_BARRIER_CLOSURE_H_
#define WINLIB_WINBASE_BARRIER_CLOSURE_H_

#include "winbase\base_export.h"
#include "winbase\functional\callback.h"

namespace winbase {

// BarrierClosure executes |done_closure| after it has been invoked
// |num_closures| times.
//
// If |num_closures| is 0, |done_closure| is executed immediately.
//
// BarrierClosure is thread-safe - the count of remaining closures is
// maintained as a winbase::AtomicRefCount. |done_closure| will be run on
// the thread that calls the final Run() on the returned closures.
//
// |done_closure| is also cleared on the final calling thread.
WINBASE_EXPORT RepeatingClosure BarrierClosure(int num_closures,
                                               OnceClosure done_closure);

}  // namespace winbase

#endif  // WINLIB_WINBASE_BARRIER_CLOSURE_H_