// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_FUNCTIONAL_CALLBACK_FORWARD_H_
#define MINI_CHROMIUM_SRC_CRBASE_FUNCTIONAL_CALLBACK_FORWARD_H_

namespace crbase {

template <typename Sig>
class Callback;

// Syntactic sugar to make Callback<void()> easier to declare since it
// will be used in a lot of APIs with delayed execution.
using Closure = Callback<void()>;

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_FUNCTIONAL_CALLBACK_FORWARD_H_