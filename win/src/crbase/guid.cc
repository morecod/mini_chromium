// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/guid.h"

#include <stddef.h>
#include <objbase.h>
#include <windows.h>

#include "crbase/strings/string_util.h"
#include "crbase/strings/utf_string_conversions.h"
#include "crbase/logging.h"

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
  assert(result == kGUIDSize);
  if (result != kGUIDSize)
    return std::string();

  return WideToUTF8(guid_string.substr(1, guid_string.length() - 2));
}


}  // namespace crbase