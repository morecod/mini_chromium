// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_DIGEST_MD5_H_
#define MINI_CHROMIUM_SRC_CRBASE_DIGEST_MD5_H_

#include <stddef.h>
#include <stdint.h>

#include "crbase/base_export.h"
#include "crbase/strings/string_piece.h"

namespace cr {

// MD5 stands for Message Digest algorithm 5.
// MD5 is a robust hash function, designed for cyptography, but often used
// for file checksums.  The code is complex and slow, but has few
// collisions.
// See Also:
//   http://en.wikipedia.org/wiki/MD5

// These functions perform MD5 operations. The simplest call is MD5Sum() to
// generate the MD5 sum of the given data.
//
// You can also compute the MD5 sum of data incrementally by making multiple
// calls to MD5Update():
//   MD5Context ctx; // intermediate MD5 data: do not use
//   MD5Init(&ctx);
//   MD5Update(&ctx, data1, length1);
//   MD5Update(&ctx, data2, length2);
//   ...
//
//   MD5Digest digest; // the result of the computation
//   MD5Final(&digest, &ctx);
//
// You can call MD5DigestToBase16() to generate a string of the digest.

static constexpr size_t kMD5Length = 16;  // Length in bytes of a SHA-1 hash.

// The output of an MD5 operation.
struct MD5Digest {
  uint8_t a[kMD5Length];
};

// Used for storing intermediate data during an MD5 computation. Callers
// should not access the data.
struct MD5Context {
  char ctx[88];
};

// Initializes the given MD5 context structure for subsequent calls to
// MD5Update().
CRBASE_EXPORT void MD5Init(MD5Context* context);

// For the given buffer of |data| as a StringPiece, updates the given MD5
// context with the sum of the data. You can call this any number of times
// during the computation, except that MD5Init() must have been called first.
CRBASE_EXPORT void MD5Update(MD5Context* context, const StringPiece& data);

inline void MD5Update(MD5Context* context, const void* data, size_t len) {
  MD5Update(context, StringPiece(reinterpret_cast<const char*>(data), len));
}

// Finalizes the MD5 operation and fills the buffer with the digest.
CRBASE_EXPORT void MD5Final(MD5Context* context, MD5Digest* digest);

// MD5IntermediateFinal() generates a digest without finalizing the MD5
// operation.  Can be used to generate digests for the input seen thus far,
// without affecting the digest generated for the entire input.
CRBASE_EXPORT void MD5IntermediateFinal(const MD5Context* context, 
                                        MD5Digest* digest);

// Converts a digest into human-readable hexadecimal.
CRBASE_EXPORT std::string MD5DigestToBase16(const MD5Digest& digest);

// Computes the MD5 sum of the given data buffer with the given length.
// The given 'digest' structure will be filled with the result data.
CRBASE_EXPORT void MD5Sum(const void* data, size_t length, MD5Digest* digest);

// Returns the MD5 (in hexadecimal) of a string.
CRBASE_EXPORT std::string MD5String(const StringPiece& str);

// Compute a RFC 2104 HMAC: H(K XOR opad, H(K XOR ipad, text))
CRBASE_EXPORT void MD5Hmac(const StringPiece& key, 
                           const StringPiece& input,
                           MD5Digest* digest);
}  // namespace cr

#endif  //  MINI_CHROMIUM_SRC_CRBASE_DIGEST_MD5_H_