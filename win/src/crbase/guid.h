// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_GUID_H_
#define MINI_CHROMIUM_SRC_CRBASE_GUID_H_

#include <stdint.h>

#include <string>

#include "crbase/base_export.h"
#include "crbase/build_config.h"

namespace crbase {

// Generate a 128-bit random GUID of the form: "%08X-%04X-%04X-%04X-%012llX".
// If GUID generation fails an empty string is returned.
// The POSIX implementation uses pseudo random number generation to create
// the GUID.  The Windows implementation uses system services.
CRBASE_EXPORT std::string GenerateGUID();

// Returns true if the input string conforms to the GUID format.
CRBASE_EXPORT bool IsValidGUID(const std::string& guid);

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_GUID_H_