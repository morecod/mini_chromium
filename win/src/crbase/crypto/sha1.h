// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_CRYPTO_SHA1_H_
#define MINI_CHROMIUM_SRC_CRBASE_CRYPTO_SHA1_H_

#include <stddef.h>

#include <string>

#include "crbase/base_export.h"

namespace crbase {

class FilePath;

// These functions perform SHA-1 operations.
static const size_t kSHA1Length = 20;  // Length in bytes of a SHA-1 hash.

// Computes the SHA-1 hash of the input string |str| and returns the full
// hash.
CRBASE_EXPORT std::string SHA1HashString(const std::string& str);

// Computes the SHA-1 hash of the |len| bytes in |data| and puts the hash
// in |hash|. |hash| must be kSHA1Length bytes long.
CRBASE_EXPORT void SHA1HashBytes(const unsigned char* data, size_t len,
                                 unsigned char* hash);

// Computes the SHA1-hash from the file  and puts the hash
// in |hash|. |hash| must be kSHA1Length bytes long.
CRBASE_EXPORT bool SHA1HashFile(const FilePath& file_name,
                                unsigned char* hash);

}  // namespace crbase

#endif  // #define MINI_CHROMIUM_SRC_CRBASE_CRYPTO_SHA1_H_
