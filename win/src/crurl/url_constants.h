// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRURL_URL_CONSTANTS_H_
#define MINI_CHROMIUM_SRC_CRURL_URL_CONSTANTS_H_

#include "crurl/url_export.h"

namespace crurl {

CRURL_EXPORT extern const char kAboutBlankURL[];

CRURL_EXPORT extern const char kAboutScheme[];
CRURL_EXPORT extern const char kBlobScheme[];
// The content scheme is specific to Android for identifying a stored file.
CRURL_EXPORT extern const char kContentScheme[];
CRURL_EXPORT extern const char kDataScheme[];
CRURL_EXPORT extern const char kFileScheme[];
CRURL_EXPORT extern const char kFileSystemScheme[];
CRURL_EXPORT extern const char kFtpScheme[];
CRURL_EXPORT extern const char kGopherScheme[];
CRURL_EXPORT extern const char kHttpScheme[];
CRURL_EXPORT extern const char kHttpsScheme[];
CRURL_EXPORT extern const char kJavaScriptScheme[];
CRURL_EXPORT extern const char kMailToScheme[];
CRURL_EXPORT extern const char kWsScheme[];
CRURL_EXPORT extern const char kWssScheme[];

// Used to separate a standard scheme and the hostname: "://".
CRURL_EXPORT extern const char kStandardSchemeSeparator[];

}  // namespace crurl

#endif  // MINI_CHROMIUM_SRC_CRURL_URL_CONSTANTS_H_