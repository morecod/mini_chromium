// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_CRBASE_IMMEDIATE_CRASH_H_
#define MINI_CHROMIUM_CRBASE_IMMEDIATE_CRASH_H_

#include "crbase/build_config.h"

// Crashes in the fastest possible way with no attempt at logging.
// There are several constraints; see http://crbug.com/664209 for more context.
//
// - TRAP_SEQUENCE_() must be fatal. It should not be possible to ignore the
//   resulting exception or simply hit 'continue' to skip over it in a debugger.
// - Different instances of TRAP_SEQUENCE_() must not be folded together, to
//   ensure crash reports are debuggable. Unlike __builtin_trap(), asm volatile
//   blocks will not be folded together.
//   Note: TRAP_SEQUENCE_() previously required an instruction with a unique
//   nonce since unlike clang, GCC folds together identical asm volatile
//   blocks.
// - TRAP_SEQUENCE_() must produce a signal that is distinct from an invalid
//   memory access.
// - TRAP_SEQUENCE_() must be treated as a set of noreturn instructions.
//   __builtin_unreachable() is used to provide that hint here. clang also uses
//   this as a heuristic to pack the instructions in the function epilogue to
//   improve code density.
//
// Additional properties that are nice to have:
// - TRAP_SEQUENCE_() should be as compact as possible.
// - The first instruction of TRAP_SEQUENCE_() should not change, to avoid
//   shifting crash reporting clusters. As a consequence of this, explicit
//   assembly is preferred over intrinsics.
//   Note: this last bullet point may no longer be true, and may be removed in
//   the future.

// Note: TRAP_SEQUENCE Is currently split into two macro helpers due to the fact
// that clang emits an actual instruction for __builtin_unreachable() on certain
// platforms (see https://crbug.com/958675). In addition, the int3/bkpt/brk will
// be removed in followups, so splitting it up like this now makes it easy to
// land the followups.

#if defined(MINI_CHROMIUM_COMPILER_GCC)

#if defined(MINI_CHROMIUM_ARCH_CPU_X86_FAMILY)

// TODO(https://crbug.com/958675): In theory, it should be possible to use just
// int3. However, there are a number of crashes with SIGILL as the exception
// code, so it seems likely that there's a signal handler that allows execution
// to continue after SIGTRAP.
#define CR_TRAP_SEQUENCE1_() asm volatile("int3")

#if defined(CR_BUILD_OS_APPLE)
// Intentionally empty: __builtin_unreachable() is always part of the sequence
// (see IMMEDIATE_CRASH below) and already emits a ud2 on Mac.
#define CR_TRAP_SEQUENCE2_() asm volatile("")
#else
#define CR_TRAP_SEQUENCE2_() asm volatile("ud2")
#endif  // defined(OS_APPLE)

#elif defined(CR_BUILD_ARCH_CPU_ARMEL)

// bkpt will generate a SIGBUS when running on armv7 and a SIGTRAP when running
// as a 32 bit userspace app on arm64. There doesn't seem to be any way to
// cause a SIGTRAP from userspace without using a syscall (which would be a
// problem for sandboxing).
// TODO(https://crbug.com/958675): Remove bkpt from this sequence.
#define CR_TRAP_SEQUENCE1_() asm volatile("bkpt #0")
#define CR_TRAP_SEQUENCE2_() asm volatile("udf #0")

#elif defined(CR_BUILD_ARCH_CPU_ARM64)

// This will always generate a SIGTRAP on arm64.
// TODO(https://crbug.com/958675): Remove brk from this sequence.
#define CR_TRAP_SEQUENCE1_() asm volatile("brk #0")
#define CR_TRAP_SEQUENCE2_() asm volatile("hlt #0")

#else

// Crash report accuracy will not be guaranteed on other architectures, but at
// least this will crash as expected.
#define CR_TRAP_SEQUENCE1_() __builtin_trap()
#define CR_TRAP_SEQUENCE2_() asm volatile("")

#endif  // CR_BUILD_ARCH_CPU_*

#elif defined(MINI_CHROMIUM_COMPILER_MSVC)

#if !defined(__clang__)

// MSVC x64 doesn't support inline asm, so use the MSVC intrinsic.
#define CR_TRAP_SEQUENCE1_() __debugbreak()
#define CR_TRAP_SEQUENCE2_()

#elif defined(CR_BUILD_ARCH_CPU_ARM64)

// Windows ARM64 uses "BRK #F000" as its breakpoint instruction, and
// __debugbreak() generates that in both VC++ and clang.
#define CR_TRAP_SEQUENCE1_() __debugbreak()
// Intentionally empty: __builtin_unreachable() is always part of the sequence
// (see CR_IMMEDIATE_CRASH below) and already emits a ud2 on Win64,
// https://crbug.com/958373
#define CR_TRAP_SEQUENCE2_() __asm volatile("")

#else

#define CR_TRAP_SEQUENCE1_() asm volatile("int3")
#define CR_TRAP_SEQUENCE2_() asm volatile("ud2")

#endif  // __clang__

#else

#error No supported trap sequence!

#endif  // CR_BUILD_COMPILER_GCC

#define CR_TRAP_SEQUENCE_() \
  do {                      \
    CR_TRAP_SEQUENCE1_();   \
    CR_TRAP_SEQUENCE2_();   \
  } while (false)

// CR_CHECK() and the trap sequence can be invoked from a constexpr function.
// This could make compilation fail on GCC, as it forbids directly using inline
// asm inside a constexpr function. However, it allows calling a lambda
// expression including the same asm.
// The side effect is that the top of the stacktrace will not point to the
// calling function, but to this anonymous lambda. This is still useful as the
// full name of the lambda will typically include the name of the function that
// calls CHECK() and the debugger will still break at the right line of code.
#if !defined(MINI_CHROMIUM_COMPILER_GCC)

#define CR_WRAPPED_TRAP_SEQUENCE_() CR_TRAP_SEQUENCE_()

#else

#define CR_WRAPPED_TRAP_SEQUENCE_() \
  do {                           \
    [] { CR_TRAP_SEQUENCE_(); }();  \
  } while (false)

#endif  // !defined(MINI_CHROMIUM_COMPILER_GCC)

#if defined(__clang__) || defined(MINI_CHROMIUM_COMPILER_GCC)

// __builtin_unreachable() hints to the compiler that this is noreturn and can
// be packed in the function epilogue.
#define CR_IMMEDIATE_CRASH()     \
  ({                             \
    CR_WRAPPED_TRAP_SEQUENCE_(); \
    __builtin_unreachable();     \
  })

#else

// This is supporting non-chromium user of logging.h to build with MSVC, like
// pdfium. On MSVC there is no __builtin_unreachable().
#define CR_IMMEDIATE_CRASH() CR_WRAPPED_TRAP_SEQUENCE_()

#endif  // defined(__clang__) || defined(CR_BUILD_COMPILER_GCC)

#endif  // MINI_CHROMIUM_CRBASE_IMMEDIATE_CRASH_H_