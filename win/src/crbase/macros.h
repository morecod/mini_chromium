// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains macros and macro-like constructs (e.g., templates) that
// are commonly used throughout Chromium source. (It may also contain things
// that are closely related to things that are commonly used that belong in this
// file.)

#ifndef MINI_CHROMIUM_SRC_CRBASE_MACROS_H_
#define MINI_CHROMIUM_SRC_CRBASE_MACROS_H_

#include <stddef.h>  // For size_t.

// Put this in the declarations for a class to be uncopyable.
#define CR_DISALLOW_COPY(TypeName) \
  TypeName(const TypeName&) = delete;

// Put this in the declarations for a class to be unassignable.
#define CR_DISALLOW_ASSIGN(TypeName) \
  void operator=(const TypeName&) = delete;

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define CR_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;           \
  void operator=(const TypeName&) = delete;

// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
//
// This should be used in the private: declarations for a class
// that wants to prevent anyone from instantiating it. This is
// especially useful for classes containing only static methods.
#define CR_DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                              \
  CR_DISALLOW_COPY_AND_ASSIGN(TypeName)

// The arraysize(arr) macro returns the # of elements in an array arr.  The
// expression is a compile-time constant, and therefore can be used in defining
// new arrays, for example.  If you use arraysize on a pointer by mistake, you
// will get a compile-time error.  For the technical details, refer to
// http://blogs.msdn.com/b/the1/archive/2004/05/07/128242.aspx.

// This template function declaration is used in defining arraysize.
// Note that the function doesn't need an implementation, as we only
// use its type.
template <typename T, size_t N> char(&CrArraySizeHelper(T(&array)[N]))[N] {};
#define cr_arraysize(ar) (sizeof(CrArraySizeHelper(ar)))

#endif  // MINI_CHROMIUM_SRC_CRBASE_MACROS_H_
