// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_BUILD_CONFIG_H_
#define MINI_CHROMIUM_SRC_CRBASE_BUILD_CONFIG_H_

// target os
#define MINI_CHROMIUM_OS_WIN 1

// compiler
#if defined(__GNUC__) || defined(__MINGW32__) || defined(__MINGW64__)
#define MINI_CHROMIUM_COMPILER_GCC 1
#elif defined(_MSC_VER)
#define MINI_CHROMIUM_COMPILER_MSVC 1
#else
#error Please add support for your compipler in crbase\build_config.h
#endif

// arch
#if defined(_M_X64) || defined(__x86_64__)
#define MINI_CHROMIUM_ARCH_CPU_X86_FAMILY 1
#define MINI_CHROMIUM_ARCH_CPU_X86_64 1
#define MINI_CHROMIUM_ARCH_CPU_64_BITS 1
#define MINI_CHROMIUM_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(_M_IX86) || defined(__i386__)
#define MINI_CHROMIUM_ARCH_CPU_X86_FAMILY 1
#define MINI_CHROMIUM_ARCH_CPU_X86 1
#define MINI_CHROMIUM_ARCH_CPU_32_BITS 1
#define MINI_CHROMIUM_ARCH_CPU_LITTLE_ENDIAN 1
#else
#error Please add support for your architecture in crbase\build_config.h
#endif

#endif  // MINI_CHROMIUM_SRC_CRBASE_BUILD_CONFIG_H_
