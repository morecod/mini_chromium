// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRNET_BASE_NET_EXPORT_H_
#define MINI_CHROMIUM_SRC_CRNET_BASE_NET_EXPORT_H_

#include "crbase/build_config.h"

// Defines NET_EXPORT so that functionality implemented by the net module can
// be exported to consumers, and NET_EXPORT_PRIVATE that allows unit tests to
// access features not intended to be used directly by real consumers.

#if defined(MINI_CHROMIUM_COMPONENT_BUILD)
#if defined(MINI_CHROMIUM_COMPILER_MSVC)

#if defined(MINI_CHROMIUM_IMPLEMENTATION)
#define CRNET_EXPORT __declspec(dllexport)
#define CRNET_EXPORT_PRIVATE __declspec(dllexport)
#else
#define CRNET_EXPORT __declspec(dllimport)
#define CRNET_EXPORT_PRIVATE __declspec(dllimport)
#endif  // defined(MINI_CHROMIUM_IMPLEMENTATION)

#else  // defined(MINI_CHROMIUM_COMPILER_MSVC)
#if defined(MINI_CHROMIUM_IMPLEMENTATION)
#define CRNET_EXPORT __attribute__((visibility("default")))
#define CRNET_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define CRNET_EXPORT
#define CRNET_EXPORT_PRIVATE
#endif
#endif

#else  /// defined(MINI_CHROMIUM_COMPONENT_BUILD)
#define CRNET_EXPORT
#define CRNET_EXPORT_PRIVATE
#endif

#endif  // MINI_CHROMIUM_SRC_CRNET_BASE_NET_EXPORT_H_