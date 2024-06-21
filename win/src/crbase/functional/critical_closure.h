// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_FUNCTIONAL_CRITICAL_CLOSURE_H_
#define MINI_CHROMIUM_SRC_CRBASE_FUNCTIONAL_CRITICAL_CLOSURE_H_

#include "crbase/functional/callback.h"
#include "crbase/macros.h"

namespace crbase {

template <typename R>
inline Callback<R(void)> MakeCriticalClosure(const Callback<R(void)>& closure) {
  // No-op for platforms where the application does not need to acquire
  // background time for closures to finish when it goes into the background.
  return closure;
}

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_FUNCTIONAL_CRITICAL_CLOSURE_H_