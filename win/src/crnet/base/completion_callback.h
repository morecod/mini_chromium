// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_CRNET_BASE_COMPLETION_CALLBACK_H__
#define MINI_CHROMIUM_CRNET_BASE_COMPLETION_CALLBACK_H__

#include <stdint.h>

#include "crbase/functional/callback.h"
#include "crbase/functional/cancelable_callback.h"

namespace crnet {

// A callback specialization that takes a single int parameter. Usually this is
// used to report a byte count or network error code.
typedef crbase::Callback<void(int)> CompletionCallback;

// 64bit version of callback specialization that takes a single int64_t
// parameter. Usually this is used to report a file offset, size or network
// error code.
typedef crbase::Callback<void(int64_t)> Int64CompletionCallback;

typedef crbase::CancelableCallback<void(int)> CancelableCompletionCallback;

}  // namespace crnet

#endif  // MINI_CHROMIUM_CRNET_BASE_COMPLETION_CALLBACK_H__