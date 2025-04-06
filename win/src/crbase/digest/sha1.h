// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_CRYPTO_SHA1_H_
#define MINI_CHROMIUM_SRC_CRBASE_CRYPTO_SHA1_H_

#include <stddef.h>

#include <string>

#include "crbase/base_export.h"
#include "crbase/strings/string_piece.h"

namespace cr {

// These functions perform SHA1 operations. The simplest call is SHA1Sum() to
// generate the SHA1 sum of the given data.
//
// You can also compute the SHA1 sum of data incrementally by making multiple
// calls to SHA1Update():
//   SHA15Context ctx; // intermediate SHA1 data: do not use
//   SHA1Init(&ctx);
//   SHA1Update(&ctx, data1, length1);
//   SHA1Update(&ctx, data2, length2);
//   ...
//
//   SHA1Digest digest; // the result of the computation
//   SHA1Final(&digest, &ctx);
//
// You can call SHA1DigestToBase16() to generate a string of the digest.

static constexpr size_t kSHA1Length = 20;  // Length in bytes of a SHA-1 hash.

// The output of an SHA1 operation
struct SHA1Digest {
  uint8_t a[kSHA1Length];
};

struct SHA1Context {
  char ctx[376];
};

// Initializes the given SHA1 context structure for subsequent calls to
// SHA1Update().
CRBASE_EXPORT void SHA1Init(SHA1Context* context);

// For the given buffer of |data| as a StringPiece, updates the given MD5
// context with the sum of the data. You can call this any number of times
// during the computation, except that MD5Init() must have been called first.
CRBASE_EXPORT void SHA1Update(SHA1Context* context, const StringPiece& data);

inline void SHA1Update(SHA1Context* context, const void* data, size_t len) {
  SHA1Update(context, StringPiece(reinterpret_cast<const char*>(data), len));
}

// Finalizes the SHA1 operation and fills the buffer with the digest.
CRBASE_EXPORT void SHA1Final(SHA1Context* context, SHA1Digest* digest);

// Converts a digest into human-readable hexadecimal.
CRBASE_EXPORT std::string SHA1DigestToBase16(const SHA1Digest& digest);

// Computes the SHA-1 hash of the input string |str| and returns the full
// hash.
CRBASE_EXPORT std::string SHA1String(const std::string& str);

// Computes the SHA-1 hash of the |len| bytes in |data| and puts the hash
// in |hash|. |hash| must be kSHA1Length bytes long.
CRBASE_EXPORT void SHA1Sum(const void* data, size_t len, SHA1Digest* digest);

// Compute a RFC 2104 HMAC: H(K XOR opad, H(K XOR ipad, text))
CRBASE_EXPORT void SHA1Hmac(const StringPiece& key, 
                            const StringPiece& input,
                            SHA1Digest* digest);

}  // namespace cr

#endif  // #define MINI_CHROMIUM_SRC_CRBASE_CRYPTO_SHA1_H_
