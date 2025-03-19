// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/guid.h"

#include <stddef.h>
#include <objbase.h>

#include "crbase/strings/string_util.h"
#include "crbase/strings/utf_string_conversions.h"
#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_OS_WIN)
#include "crbase/logging.h"
#elif defined(MINI_CHROMIUM_OS_POSIX)
#include "crbase/rand_util.h"
#include "crbase/strings/stringprintf.h"
#endif

namespace crbase {

bool IsValidGUID(const std::string& guid) {
  const size_t kGUIDLength = 36U;
  if (guid.length() != kGUIDLength)
    return false;

  for (size_t i = 0; i < guid.length(); ++i) {
    char current = guid[i];
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (current != '-')
        return false;
    } else {
      if (!IsHexDigit(current))
        return false;
    }
  }

  return true;
}

#if defined(MINI_CHROMIUM_OS_WIN)

std::string GenerateGUID() {
  const int kGUIDSize = 39;

  GUID guid;
  HRESULT guid_result = CoCreateGuid(&guid);
  CR_DCHECK(SUCCEEDED(guid_result));
  if (!SUCCEEDED(guid_result))
    return std::string();

  std::wstring guid_string;
  int result = StringFromGUID2(guid,
                               WriteInto(&guid_string, kGUIDSize), kGUIDSize);
  CR_DCHECK(result == kGUIDSize);
  if (result != kGUIDSize)
    return std::string();

  return WideToUTF8(guid_string.substr(1, guid_string.length() - 2));
}

#elif defined(MINI_CHROMIUM_OS_POSIX)

// TODO(cmasone): Once we're comfortable this works, migrate Windows code to
// use this as well.
std::string RandomDataToGUIDString(const uint64_t bytes[2]) {
  return StringPrintf("%08X-%04X-%04X-%04X-%012llX",
                      static_cast<unsigned int>(bytes[0] >> 32),
                      static_cast<unsigned int>((bytes[0] >> 16) & 0x0000ffff),
                      static_cast<unsigned int>(bytes[0] & 0x0000ffff),
                      static_cast<unsigned int>(bytes[1] >> 48),
                      bytes[1] & 0x0000ffffffffffffULL);
}

std::string GenerateGUID() {
  uint64_t sixteen_bytes[2] = {crbase::RandUint64(), crbase::RandUint64()};

  // Set the GUID to version 4 as described in RFC 4122, section 4.4.
  // The format of GUID version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
  // where y is one of [8, 9, A, B].

  // Clear the version bits and set the version to 4:
  sixteen_bytes[0] &= 0xffffffffffff0fffULL;
  sixteen_bytes[0] |= 0x0000000000004000ULL;

  // Set the two most significant bits (bits 6 and 7) of the
  // clock_seq_hi_and_reserved to zero and one, respectively:
  sixteen_bytes[1] &= 0x3fffffffffffffffULL;
  sixteen_bytes[1] |= 0x8000000000000000ULL;

  return RandomDataToGUIDString(sixteen_bytes);
}

#endif

}  // namespace crbase