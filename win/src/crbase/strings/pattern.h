// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_STRINGS_PATTERN_H_
#define MINI_CHROMIUM_SRC_CRBASE_STRINGS_PATTERN_H_

#include "crbase/base_export.h"
#include "crbase/strings/string_piece.h"

namespace cr {

// Returns true if the string passed in matches the pattern. The pattern
// string can contain wildcards like * and ?
//
// The backslash character (\) is an escape character for * and ?
// We limit the patterns to having a max of 16 * or ? characters.
// ? matches 0 or 1 character, while * matches 0 or more characters.
CRBASE_EXPORT bool MatchPattern(const StringPiece& string,
                              const StringPiece& pattern);
CRBASE_EXPORT bool MatchPattern(const StringPiece16& string,
                              const StringPiece16& pattern);

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_STRINGS_PATTERN_H_