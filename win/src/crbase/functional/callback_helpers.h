// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines helpful methods for dealing with Callbacks.  Because Callbacks
// are implemented using templates, with a class per callback signature, adding
// methods to Callback<> itself is unattractive (lots of extra code gets
// generated).  Instead, consider adding methods here.
//
// ResetAndReturn(&cb) is like cb.Reset() but allows executing a callback (via a
// copy) after the original callback is Reset().  This can be handy if Run()
// reads/writes the variable holding the Callback.

#ifndef MINI_CHROMIUM_SRC_CRBASE_FUNCTIONAL_CALLBACK_HELPERS_H_
#define MINI_CHROMIUM_SRC_CRBASE_FUNCTIONAL_CALLBACK_HELPERS_H_

#include "crbase/base_export.h"
#include "crbase/functional/callback.h"
#include "crbase/macros.h"

namespace crbase {

template <typename Sig>
crbase::Callback<Sig> ResetAndReturn(crbase::Callback<Sig>* cb) {
  crbase::Callback<Sig> ret(*cb);
  cb->Reset();
  return ret;
}

// ScopedClosureRunner is akin to std::unique_ptr for Closures. It ensures that the
// Closure is executed and deleted no matter how the current scope exits.
class CRBASE_EXPORT ScopedClosureRunner {
 public:
  ScopedClosureRunner(const ScopedClosureRunner&) = delete;
  ScopedClosureRunner& operator=(const ScopedClosureRunner&) = delete;

  ScopedClosureRunner();
  explicit ScopedClosureRunner(const Closure& closure);
  ~ScopedClosureRunner();

  void Reset();
  void Reset(const Closure& closure);
  Closure Release();

 private:
  Closure closure_;
};

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_FUNCTIONAL_CALLBACK_HELPERS_H_