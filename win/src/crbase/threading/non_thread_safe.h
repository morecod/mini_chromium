// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_THREADING_NON_THREAD_SAFE_H_
#define MINI_CHROMIUM_SRC_CRBASE_THREADING_NON_THREAD_SAFE_H_

namespace cr {

// Do nothing implementation of NonThreadSafe, for release mode.
//
// Note: You should almost always use the NonThreadSafe class to get
// the right version of the class for your build configuration.
class NonThreadSafeDoNothing {
 public:
  bool CalledOnValidThread() const {
    return true;
  }

 protected:
  ~NonThreadSafeDoNothing() {}
  void DetachFromThread() {}
};

typedef NonThreadSafeDoNothing NonThreadSafe;

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_THREADING_NON_THREAD_SAFE_H_