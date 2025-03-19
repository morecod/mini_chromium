// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_BUILD_CONFIG_H_
#define MINI_CHROMIUM_SRC_CRBASE_BUILD_CONFIG_H_

// This file adds defines about the platform we're currently building on.
//  Operating System:
//    MINI_CHROMIUM_OS_WIN / OS_LINUX / OS_POSIX (LINUX)
//  Compiler:
//    MINI_CHROMIUM_COMPILER_MSVC / COMPILER_GCC
//  Processor:
//    MINI_CHROMIUM_ARCH_CPU_X86 / MINI_CHROMIUM_ARCH_CPU_X86_64 / 
//    MINI_CHROMIUM_ARCH_CPU_X86_FAMILY (X86 or X86_64)
//    MINI_CHROMIUM_ARCH_CPU_32_BITS / MINI_CHROMIUM_ARCH_CPU_64_BITS

// target os
#if defined(_WIN32) || defined(_WIN64)
#define MINI_CHROMIUM_OS_WIN 1
#elif defined(__linux__)
#define MINI_CHROMIUM_OS_LINUX 1
#else
#error Please add support for your platform  in crbase\build_config.h
#endif

#if defined(MINI_CHROMIUM_OS_LINUX)
#define MINI_CHROMIUM_OS_POXI 1
#endif

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

// Type detection for wchar_t.
#if defined(MINI_CHROMIUM_OS_WIN)
#define WCHAR_T_IS_UTF16
#elif defined(MINI_CHROMIUM_OS_POSIX) &&   \
    defined(MINI_CHROMIUM_COMPILER_GCC) && \
    defined(__WCHAR_MAX__) &&              \
    (__WCHAR_MAX__ == 0x7fffffff || __WCHAR_MAX__ == 0xffffffff)
#define WCHAR_T_IS_UTF32
#elif defined(MINI_CHROMIUM_OS_POSIX) &&   \
    defined(MINI_CHROMIUM_COMPILER_GCC) && \
    defined(__WCHAR_MAX__) &&              \
    (__WCHAR_MAX__ == 0x7fff || __WCHAR_MAX__ == 0xffff)
// On Posix, we'll detect short wchar_t, but projects aren't guaranteed to
// compile in this mode (in particular, Chrome doesn't). This is intended for
// other projects using base who manage their own dependencies and make sure
// short wchar works for them.
#define WCHAR_T_IS_UTF16
#else
#error Please add support for your compiler crbase/build_config.h
#endif

#endif  // MINI_CHROMIUM_SRC_CRBASE_BUILD_CONFIG_H_
