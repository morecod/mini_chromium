// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header defines cross-platform ByteSwap() implementations for 16, 32 and
// 64-bit values.

#ifndef MINI_CHROMIUM_SRC_CRBASE_BYTES_SWAP_H_
#define MINI_CHROMIUM_SRC_CRBASE_BYTES_SWAP_H_

#include <stdint.h>

#include "crbase/build_config.h"

namespace cr {

// Returns a value with all bytes in |x| swapped, i.e. reverses the endianness.
inline uint16_t ByteSwap(uint16_t x) {
  return ((x & 0x00ff) << 8) | ((x & 0xff00) >> 8);
}

inline uint32_t ByteSwap(uint32_t x) {
  return ((x & 0x000000fful) << 24) | ((x & 0x0000ff00ul) << 8) |
      ((x & 0x00ff0000ul) >> 8) | ((x & 0xff000000ul) >> 24);
}

inline uint64_t ByteSwap(uint64_t x) {
  return ((x & 0x00000000000000ffull) << 56) |
      ((x & 0x000000000000ff00ull) << 40) |
      ((x & 0x0000000000ff0000ull) << 24) |
      ((x & 0x00000000ff000000ull) << 8) |
      ((x & 0x000000ff00000000ull) >> 8) |
      ((x & 0x0000ff0000000000ull) >> 24) |
      ((x & 0x00ff000000000000ull) >> 40) |
      ((x & 0xff00000000000000ull) >> 56);
}

// Converts the bytes in |x| from host order (endianness) to little endian, and
// returns the result.
inline uint16_t ByteSwapToLE16(uint16_t x) {
#if defined(MINI_CHROMIUM_ARCH_CPU_LITTLE_ENDIAN)
  return x;
#else
  return ByteSwap(x);
#endif
}
inline uint32_t ByteSwapToLE32(uint32_t x) {
#if defined(MINI_CHROMIUM_ARCH_CPU_LITTLE_ENDIAN)
  return x;
#else
  return ByteSwap(x);
#endif
}
inline uint64_t ByteSwapToLE64(uint64_t x) {
#if defined(MINI_CHROMIUM_ARCH_CPU_LITTLE_ENDIAN)
  return x;
#else
  return ByteSwap(x);
#endif
}

inline uint16_t ByteSwapToBE16(uint16_t x) {
#if defined(MINI_CHROMIUM_ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}
inline uint32_t ByteSwapToBE32(uint32_t x) {
#if defined(MINI_CHROMIUM_ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}
inline uint64_t ByteSwapToBE64(uint64_t x) {
#if defined(MINI_CHROMIUM_ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}


}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_BYTES_SWAP_H_