// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_CRURL_URL_UTIL_INTERNAL_H_
#define MINI_CHROMIUM_CRURL_URL_UTIL_INTERNAL_H_

#include <string>

#include "crbase/strings/string16.h"
#include "crurl/third_party/mozilla/url_parse.h"

namespace crurl {

// Given a string and a range inside the string, compares it to the given
// lower-case |compare_to| buffer.
bool CompareSchemeComponent(const char* spec,
                            const Component& component,
                            const char* compare_to);
bool CompareSchemeComponent(const crbase::char16* spec,
                            const Component& component,
                            const char* compare_to);

}  // namespace crurl

#endif  // MINI_CHROMIUM_CRURL_URL_UTIL_INTERNAL_H_