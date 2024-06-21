// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_CRBASE_EXPORT_H_
#define MINI_CHROMIUM_SRC_CRBASE_CRBASE_EXPORT_H_

#if defined(MINI_CHROMIUM_COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MINI_CHROMIUM_IMPLEMENTATION)
#define CRBASE_EXPORT __declspec(dllexport)
#else
#define CRBASE_EXPORT __declspec(dllimport)
#endif  // defined(MINI_CHROMIUM_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MINI_CHROMIUM_IMPLEMENTATION)
#define CRBASE_EXPORT __attribute__((visibility("default")))
#else
#define CRBASE_EXPORT
#endif  // defined(MINI_CHROMIUM_IMPLEMENTATION)
#endif

#else  // defined(MINI_CHROMIUM_IMPLEMENTATION)
#define CRBASE_EXPORT
#endif

#endif  // MINI_CHROMIUM_SRC_CRBASE_CRBASE_EXPORT_H_
