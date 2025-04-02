// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_CRBASE_DEBUG_STACK_TRACE_H_
#define MINI_CHROMIUM_CRBASE_DEBUG_STACK_TRACE_H_

#include <stddef.h>

#include <iosfwd>
#include <string>

#include "crbase/base_export.h"
#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_OS_POSIX)
#include <unistd.h>
#endif

#if defined(MINI_CHROMIUM_OS_WIN)
struct _EXCEPTION_POINTERS;
struct _CONTEXT;
#endif

namespace cr {
namespace debug {

// Enables stack dump to console output on exception and signals.
// When enabled, the process will quit immediately. This is meant to be used in
// unit_tests only! This is not thread-safe: only call from main thread.
// In sandboxed processes, this has to be called before the sandbox is turned
// on.
// Calling this function on Linux opens /proc/self/maps and caches its
// contents. In non-official builds, this function also opens the object files
// that are loaded in memory and caches their file descriptors (this cannot be
// done in official builds because it has security implications).
CRBASE_EXPORT bool EnableInProcessStackDumping();

// A stacktrace can be helpful in debugging. For example, you can include a
// stacktrace member in a object (probably around #ifndef NDEBUG) so that you
// can later see where the given object was created from.
class CRBASE_EXPORT StackTrace {
 public:
  // Creates a stacktrace from the current location.
  StackTrace();

  // Creates a stacktrace from an existing array of instruction
  // pointers (such as returned by Addresses()).  |count| will be
  // trimmed to |kMaxTraces|.
  StackTrace(const void* const* trace, size_t count);

#if defined(MINI_CHROMIUM_OS_WIN)
  // Creates a stacktrace for an exception.
  // Note: this function will throw an import not found (StackWalk64) exception
  // on system without dbghelp 5.1.
  StackTrace(_EXCEPTION_POINTERS* exception_pointers);
  StackTrace(const _CONTEXT* context);
#endif

  // Copying and assignment are allowed with the default functions.

  ~StackTrace();

  // Gets an array of instruction pointer values. |*count| will be set to the
  // number of elements in the returned array.
  const void* const* Addresses(size_t* count) const;

  // Prints the stack trace to stderr.
  void Print() const;

///#if !defined(__UCLIBC__)
  // Resolves backtrace to symbols and write to stream.
  void OutputToStream(std::ostream* os) const;
///#endif

  // Resolves backtrace to symbols and returns as string.
  std::string ToString() const;

 private:
#if defined(MINI_CHROMIUM_OS_WIN)
  void InitTrace(const _CONTEXT* context_record);
#endif

  // From http://msdn.microsoft.com/en-us/library/bb204633.aspx,
  // the sum of FramesToSkip and FramesToCapture must be less than 63,
  // so set it to 62. Even if on POSIX it could be a larger value, it usually
  // doesn't give much more information.
  static const int kMaxTraces = 62;

  void* trace_[kMaxTraces];

  // The number of valid frames in |trace_|.
  size_t count_;
};

namespace internal {

#if defined(MINI_CHROMIUM_OS_POSIX)
// POSIX doesn't define any async-signal safe function for converting
// an integer to ASCII. We'll have to define our own version.
// itoa_r() converts a (signed) integer to ASCII. It returns "buf", if the
// conversion was successful or NULL otherwise. It never writes more than "sz"
// bytes. Output will be truncated as needed, and a NUL character is always
// appended.
CRBASE_EXPORT char *itoa_r(intptr_t i,
                           char *buf,
                           size_t sz,
                           int base,
                           size_t padding);
#endif  // defined(MINI_CHROMIUM_OS_POSIX)

}  // namespace internal

}  // namespace debug
}  // namespace cr

#endif  // MINI_CHROMIUM_CRBASE_DEBUG_STACK_TRACE_H_