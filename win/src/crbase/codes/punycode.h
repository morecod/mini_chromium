// Copyright 2016 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_CODES_PUNYCODE_H_
#define MINI_CHROMIUM_SRC_CRBASE_CODES_PUNYCODE_H_

#include <string>

#include "crbase/base_export.h"
#include "crbase/strings/string16.h"
#include "crbase/strings/string_piece.h"

namespace cr {

// Encodes an UTF16 string into Punycode. Returns true if successful and false
// otherwise.
CRBASE_EXPORT bool EncodePunycode(const StringPiece16& input, std::string &out);
CRBASE_EXPORT bool EncodePunycode(const StringPiece16& input, string16 &out);

// Decodes a Punycode string into a sequence of Unicode scalars. Return true if
// successful and false otherwise.
CRBASE_EXPORT bool DecodePunycode(StringPiece input, string16& output);

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_CODES_PUNYCODE_H_
