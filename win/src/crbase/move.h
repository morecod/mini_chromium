// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_MOVE_H_
#define MINI_CHROMIUM_SRC_CRBASE_MOVE_H_

#include <utility>

#include "crbase/macros.h"

// A macro to disallow the copy constructor and copy assignment functions.
// This should be used in the private: declarations for a class.
//
// Use this macro instead of CR_DISALLOW_COPY_AND_ASSIGN if you want to pass
// ownership of the type through a crbase::Callback without heap-allocating it
// into a std::unique_ptr.  The class must define a move constructor and move
// assignment operator to make this work.
//
// This version of the macro adds a Pass() function and a cryptic
// MoveOnlyTypeForCPP03 typedef for the crbase::Callback implementation to use.
// See IsMoveOnlyType template and its usage in base/callback_internal.h
// for more details.
// TODO(crbug.com/566182): Remove this macro and use DISALLOW_COPY_AND_ASSIGN
// everywhere instead.
#define CR_DISALLOW_COPY_AND_ASSIGN_WITH_MOVE_FOR_BIND(type)       \
 private:                                                          \
  type(const type&) = delete;                                      \
  void operator=(const type&) = delete;                            \
                                                                   \
 public:                                                           \
  type&& Pass() { return std::move(*this); }                       \ 
  typedef void MoveOnlyTypeForCPP03;                               \
                                                                   \
 private:

   
// TODO(crbug.com/566182): DEPRECATED!
// Use CR_DISALLOW_COPY_AND_ASSIGN instead, or if your type will be used in
// Callbacks, use CR_DISALLOW_COPY_AND_ASSIGN_WITH_MOVE_FOR_BIND instead.
#define CR_MOVE_ONLY_TYPE_FOR_CPP_03(type) \
    CR_DISALLOW_COPY_AND_ASSIGN_WITH_MOVE_FOR_BIND(type)

#endif  // MINI_CHROMIUM_CRBASE_MOVE_H_