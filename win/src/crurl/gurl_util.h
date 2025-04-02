// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRURL_GURL_UTIL_H_
#define MINI_CHROMIUM_SRC_CRURL_GURL_UTIL_H_

#include <string>

#include "crbase/macros.h"
#include "crbase/strings/string16.h"
#include "crbase/strings/string_piece.h"

#include "crurl/url_export.h"
#include "crurl/third_party/mozilla/url_parse.h"
#include "crurl/gurl.h"

namespace crurl {

// Returns a new GURL by appending the given query parameter name and the
// value. Unsafe characters in the name and the value are escaped like
// %XX%XX. The original query component is preserved if it's present.
//
// Examples:
//
// AppendQueryParameter(GURL("http://example.com"), "name", "value").spec()
// => "http://example.com?name=value"
// AppendQueryParameter(GURL("http://example.com?x=y"), "name", "value").spec()
// => "http://example.com?x=y&name=value"
CRURL_EXPORT GURL AppendQueryParameter(const GURL& url,
                                       const std::string& name,
                                       const std::string& value);

// Returns a new GURL by appending or replacing the given query parameter name
// and the value. If |name| appears more than once, only the first name-value
// pair is replaced. Unsafe characters in the name and the value are escaped
// like %XX%XX. The original query component is preserved if it's present.
//
// Examples:
//
// AppendOrReplaceQueryParameter(
//     GURL("http://example.com"), "name", "new").spec()
// => "http://example.com?name=value"
// AppendOrReplaceQueryParameter(
//     GURL("http://example.com?x=y&name=old"), "name", "new").spec()
// => "http://example.com?x=y&name=new"
CRURL_EXPORT GURL AppendOrReplaceQueryParameter(const GURL& url,
                                                const std::string& name,
                                                const std::string& value);

// Iterates over the key-value pairs in the query portion of |url|.
class CRURL_EXPORT QueryIterator {
 public:
  QueryIterator(const QueryIterator&) = delete;
  QueryIterator& operator=(const QueryIterator&) = delete;

  explicit QueryIterator(const GURL& url);
  ~QueryIterator();

  std::string GetKey() const;
  std::string GetValue() const;
  const std::string& GetUnescapedValue();

  bool IsAtEnd() const;
  void Advance();

 private:
  const GURL& url_;
  crurl::Component query_;
  bool at_end_;
  crurl::Component key_;
  crurl::Component value_;
  std::string unescaped_value_;
};

// Looks for |search_key| in the query portion of |url|. Returns true if the
// key is found and sets |out_value| to the unescaped value for the key.
// Returns false if the key is not found.
CRURL_EXPORT bool GetValueForKeyInQuery(const GURL& url,
                                        const std::string& search_key,
                                        std::string* out_value);


// Returns the hostname by trimming the ending dot, if one exists.
CRURL_EXPORT std::string TrimEndingDot(const cr::StringPiece& host);

// Extracts the unescaped username/password from |url|, saving the results
// into |*username| and |*password|.
CRURL_EXPORT void GetIdentityFromURL(const GURL& url,
                                     cr::string16* username,
                                     cr::string16* password);

}  // namespace crurl

#endif  // MINI_CHROMIUM_SRC_CRURL_GURL_UTIL_H_