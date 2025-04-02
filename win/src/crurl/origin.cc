// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crurl/origin.h"

#include <stdint.h>
#include <string.h>

#include "crbase/logging.h"
#include "crbase/strings/string_number_conversions.h"
#include "crurl/gurl.h"
#include "crurl/url_canon.h"
#include "crurl/url_canon_stdstring.h"
#include "crurl/url_constants.h"
#include "crurl/url_util.h"

namespace crurl {

Origin::Origin() : unique_(true) {
}

Origin::Origin(const GURL& url) : unique_(true) {
  if (!url.is_valid() || (!url.IsStandard() && !url.SchemeIsBlob()))
    return;

  if (url.SchemeIsFileSystem()) {
    tuple_ = SchemeHostPort(*url.inner_url());
  } else if (url.SchemeIsBlob()) {
    // If we're dealing with a 'blob:' URL, https://url.spec.whatwg.org/#origin
    // defines the origin as the origin of the URL which results from parsing
    // the "path", which boils down to everything after the scheme. GURL's
    // 'GetContent()' gives us exactly that.
    tuple_ = SchemeHostPort(GURL(url.GetContent()));
  } else {
    tuple_ = SchemeHostPort(url);
  }

  unique_ = tuple_.IsInvalid();
}

Origin::Origin(cr::StringPiece scheme, cr::StringPiece host, 
               uint16_t port)
    : tuple_(scheme, host, port) {
  unique_ = tuple_.IsInvalid();
}

Origin::~Origin() {
}

// static
Origin Origin::UnsafelyCreateOriginWithoutNormalization(
    cr::StringPiece scheme,
    cr::StringPiece host,
    uint16_t port) {
  return Origin(scheme, host, port);
}

std::string Origin::Serialize() const {
  if (unique())
    return "null";

  if (scheme() == kFileScheme)
    return "file://";

  return tuple_.Serialize();
}

bool Origin::IsSameOriginWith(const Origin& other) const {
  if (unique_ || other.unique_)
    return false;

  return tuple_.Equals(other.tuple_);
}

bool Origin::operator<(const Origin& other) const {
  return tuple_ < other.tuple_;
}

std::ostream& operator<<(std::ostream& out, const crurl::Origin& origin) {
  return out << origin.Serialize();
}

}  // namespace crurl