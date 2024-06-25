// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRURL_URL_EXPORT_H_
#define MINI_CHROMIUM_SRC_CRURL_URL_EXPORT_H_

#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_COMPONENT_BUILD)
#if defined(MINI_CHROMIUM_COMPILER_MSVC)

#if defined(MINI_CHROMIUM_IMPLEMENTATION)
#define CRURL_EXPORT __declspec(dllexport)
#else
#define CRURL_EXPORT __declspec(dllimport)
#endif  // defined(MINI_CHROMIUM_IMPLEMENTATION)

#else  // defined(MINI_CHROMIUM_COMPILER_MSVC)
#if defined(MINI_CHROMIUM_IMPLEMENTATION)
#define CRURL_EXPORT __attribute__((visibility("default")))
#else
#define CRURL_EXPORT
#endif  // defined(MINI_CHROMIUM_IMPLEMENTATION)
#endif

#else  // defined(MINI_CHROMIUM_IMPLEMENTATION)
#define CRURL_EXPORT
#endif

#endif  // MINI_CHROMIUM_SRC_CRURL_URL_EXPORT_H_
