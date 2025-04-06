// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/digest/sha1.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>

#include "crbase/strings/string_piece.h"
#include "crbase/files/file_path.h"
#include "crbase/files/file.h"

namespace cr {

namespace {

inline uint32_t f(uint32_t t, uint32_t B, uint32_t C, uint32_t D) {
  if (t < 20) {
    return (B & C) | ((~B) & D);
  } else if (t < 40) {
    return B ^ C ^ D;
  } else if (t < 60) {
    return (B & C) | (B & D) | (C & D);
  } else {
    return B ^ C ^ D;
  }
}

inline uint32_t S(uint32_t n, uint32_t X) {
  return (X << n) | (X >> (32-n));
}

inline uint32_t K(uint32_t t) {
  if (t < 20) {
    return 0x5a827999;
  } else if (t < 40) {
    return 0x6ed9eba1;
  } else if (t < 60) {
    return 0x8f1bbcdc;
  } else {
    return 0xca62c1d6;
  }
}

inline void swapends(uint32_t* t) {
  *t = (*t >> 24) | ((*t >> 8) & 0xff00) | ((*t & 0xff00) << 8) | (*t << 24);
}

struct Context {
  uint32_t A, B, C, D, E;
  uint32_t H[5];
  union {
    uint32_t W[80];
    uint8_t M[64];
  };
  uint32_t cursor;
  uint64_t l;
};

void SHA1Process(struct Context* ctx) {
  uint32_t t;

  // Each a...e corresponds to a section in the FIPS 180-3 algorithm.

  // a.
  //
  // W and M are in a union, so no need to memcpy.
  // memcpy(W, M, sizeof(M));
  for (t = 0; t < 16; ++t)
    swapends(&ctx->W[t]);

  // b.
  for (t = 16; t < 80; ++t)
    ctx->W[t] = S(1, ctx->W[t - 3] ^ ctx->W[t - 8] ^ 
                ctx->W[t - 14] ^ ctx->W[t - 16]);

  // c.
  ctx->A = ctx->H[0];
  ctx->B = ctx->H[1];
  ctx->C = ctx->H[2];
  ctx->D = ctx->H[3];
  ctx->E = ctx->H[4];

  // d.
  for (t = 0; t < 80; ++t) {
    uint32_t TEMP = S(5, ctx->A) + f(t, ctx->B, ctx->C, ctx->D) + 
                    ctx->E + ctx->W[t] + K(t);
    ctx->E = ctx->D;
    ctx->D = ctx->C;
    ctx->C = S(30, ctx->B);
    ctx->B = ctx->A;
    ctx->A = TEMP;
  }

  // e.
  ctx->H[0] += ctx->A;
  ctx->H[1] += ctx->B;
  ctx->H[2] += ctx->C;
  ctx->H[3] += ctx->D;
  ctx->H[4] += ctx->E;

  ctx->cursor = 0;
}

void SHA1Pad(struct Context* ctx) {
  ctx->M[ctx->cursor++] = 0x80;

  if (ctx->cursor > 64 - 8) {
    // pad out to next block
    while (ctx->cursor < 64)
      ctx->M[ctx->cursor++] = 0;

    SHA1Process(ctx);
  }

  while (ctx->cursor < 64 - 8)
    ctx->M[ctx->cursor++] = 0;

  ctx->M[ctx->cursor++] = (ctx->l >> 56) & 0xff;
  ctx->M[ctx->cursor++] = (ctx->l >> 48) & 0xff;
  ctx->M[ctx->cursor++] = (ctx->l >> 40) & 0xff;
  ctx->M[ctx->cursor++] = (ctx->l >> 32) & 0xff;
  ctx->M[ctx->cursor++] = (ctx->l >> 24) & 0xff;
  ctx->M[ctx->cursor++] = (ctx->l >> 16) & 0xff;
  ctx->M[ctx->cursor++] = (ctx->l >> 8) & 0xff;
  ctx->M[ctx->cursor++] = ctx->l & 0xff;
}

}  // namespace

void SHA1Init(SHA1Context* context) {
  struct Context* ctx = reinterpret_cast<struct Context*>(context);
  ctx->A = 0;
  ctx->B = 0;
  ctx->C = 0;
  ctx->D = 0;
  ctx->E = 0;
  ctx->cursor = 0;
  ctx->l = 0;
  ctx->H[0] = 0x67452301;
  ctx->H[1] = 0xefcdab89;
  ctx->H[2] = 0x98badcfe;
  ctx->H[3] = 0x10325476;
  ctx->H[4] = 0xc3d2e1f0;
}

// For the given buffer of |data| as a StringPiece, updates the given MD5
// context with the sum of the data. You can call this any number of times
// during the computation, except that MD5Init() must have been called first.
void SHA1Update(SHA1Context* context, const StringPiece& data) {
  struct Context* ctx = reinterpret_cast<struct Context*>(context);
  const uint8_t* d = reinterpret_cast<const uint8_t*>(data.data());
  size_t nbytes = data.length();
  while (nbytes--) {
    ctx->M[ctx->cursor++] = *d++;
    if (ctx->cursor >= 64)
      SHA1Process(ctx);
    ctx->l += 8;
  }
}

// Finalizes the SHA1 operation and fills the buffer with the digest.
void SHA1Final(SHA1Context* context, SHA1Digest* digest) {
  struct Context* ctx = reinterpret_cast<struct Context*>(context);
  SHA1Pad(ctx);
  SHA1Process(ctx);

  for (int t = 0; t < 5; ++t)
    swapends(&ctx->H[t]);

  memcpy(digest, ctx->H, sizeof(ctx->H));
}

std::string SHA1DigestToBase16(const SHA1Digest& digest) {
  static char const zEncode[] = "0123456789abcdef";

  std::string ret;
  ret.resize(kSHA1Length * 2);

  for (int i = 0, j = 0; i < kSHA1Length; i++, j += 2) {
    uint8_t a = digest.a[i];
    ret[j] = zEncode[(a >> 4) & 0xf];
    ret[j + 1] = zEncode[a & 0xf];
  }
  return ret;
}

std::string SHA1String(const std::string& str) {
  SHA1Digest digest;

  SHA1Context context;
  SHA1Init(&context);
  SHA1Update(&context, str);
  SHA1Final(&context, &digest);
  return SHA1DigestToBase16(digest);
}

void SHA1Sum(const void* data, size_t len, SHA1Digest* digest) {
  SHA1Context context;
  SHA1Init(&context);
  SHA1Update(&context, StringPiece(reinterpret_cast<const char*>(data), len));
  SHA1Final(&context, digest);
}

void SHA1Hmac(const StringPiece& key, 
              const StringPiece& input, 
              SHA1Digest* digest) {
  constexpr size_t block_len = 64;

  // Copy the key to a block-sized buffer to simplify padding.
  // If the key is longer than a block, hash it and use the result instead.
  uint8_t new_key[block_len];
  if (key.length() > block_len) {
    SHA1Sum(key.data(), key.length(), reinterpret_cast<SHA1Digest*>(new_key));
    memset(new_key + kSHA1Length, 0, block_len - kSHA1Length);
  } else {
    memcpy(new_key, key.data(), key.length());
    memset(new_key + key.length(), 0, block_len - key.length());
  }
  // Set up the padding from the key, salting appropriately for each padding.
  uint8_t o_pad[block_len];
  uint8_t i_pad[block_len];
  for (size_t i = 0; i < block_len; ++i) {
    o_pad[i] = 0x5c ^ new_key[i];
    i_pad[i] = 0x36 ^ new_key[i];
  }

  // Inner hash; hash the inner padding, and then the input buffer.

  std::unique_ptr<SHA1Context> context(new SHA1Context);
  SHA1Init(context.get());
  SHA1Update(context.get(), i_pad, block_len);
  SHA1Update(context.get(), input);
  SHA1Final(context.get(), digest);

  SHA1Init(context.get());
  SHA1Update(context.get(), o_pad, block_len);
  SHA1Update(context.get(), digest, sizeof(*digest));
  SHA1Final(context.get(), digest);
}


}  // namespace cr
