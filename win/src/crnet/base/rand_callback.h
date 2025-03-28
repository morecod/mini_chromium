// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_CRNET_BASE_RAND_CALLBACK_H_
#define MINI_CHROMIUM_CRNET_BASE_RAND_CALLBACK_H_

#include "crbase/functional/callback.h"

namespace crnet {

typedef crbase::RepeatingCallback<int(int, int)> RandIntCallback;

}  // namespace crnet

#endif  // MINI_CHROMIUM_CRNET_BASE_RAND_CALLBACK_H_