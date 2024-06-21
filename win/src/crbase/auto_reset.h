// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_AUTO_RESET_H_
#define MINI_CHROMIUM_SRC_CRBASE_AUTO_RESET_H_

#include <utility>

#include "crbase\macros.h"

// crbase::AutoReset<> is useful for setting a variable to a new value only 
// within a particular scope. An crbase::AutoReset<> object resets a variable 
// to its original value upon destruction, making it an alternative to writing
// "var = false;" or "var = old_val;" at all of a block's exit points.
//
// This should be obvious, but note that an crbase::AutoReset<> instance should
// have a shorter lifetime than its scoped_variable, to prevent invalid memory
// writes when the crbase::AutoReset<> object is destroyed.

namespace crbase {

template<typename T>
class AutoReset {
 public:
  AutoReset(T* scoped_variable, T new_value)
      : scoped_variable_(scoped_variable),
        original_value_(std::move(*scoped_variable)) {
    *scoped_variable_ = std::move(new_value);
  }

  AutoReset(const AutoReset&) = delete;
  AutoReset& operator=(const AutoReset&) = delete;

  ~AutoReset() { *scoped_variable_ = std::move(original_value_); }

 private:
  T* scoped_variable_;
  T original_value_;
};

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_AUTO_RESET_H_