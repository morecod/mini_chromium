// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crurl/gurl_util.h"

#include "crbase/logging.h"
#include "crbase/strings/escape.h"

namespace crurl {

GURL AppendQueryParameter(const GURL& url,
                          const std::string& name,
                          const std::string& value) {
  std::string query(url.query());

  if (!query.empty())
    query += "&";

  query += (cr::EscapeQueryParamValue(name, true) + "=" +
            cr::EscapeQueryParamValue(value, true));
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

GURL AppendOrReplaceQueryParameter(const GURL& url,
                                   const std::string& name,
                                   const std::string& value) {
  bool replaced = false;
  std::string param_name = cr::EscapeQueryParamValue(name, true);
  std::string param_value = cr::EscapeQueryParamValue(value, true);

  const std::string input = url.query();
  crurl::Component cursor(0, static_cast<int>(input.size()));
  std::string output;
  crurl::Component key_range, value_range;
  while (crurl::ExtractQueryKeyValue(input.data(), &cursor, &key_range,
                                     &value_range)) {
    const cr::StringPiece key(
        input.data() + key_range.begin, key_range.len);
    std::string key_value_pair;
    // Check |replaced| as only the first pair should be replaced.
    if (!replaced && key == param_name) {
      replaced = true;
      key_value_pair = (param_name + "=" + param_value);
    } else {
      key_value_pair.assign(input.data(),
                            key_range.begin,
                            value_range.end() - key_range.begin);
    }
    if (!output.empty())
      output += "&";

    output += key_value_pair;
  }
  if (!replaced) {
    if (!output.empty())
      output += "&";

    output += (param_name + "=" + param_value);
  }
  GURL::Replacements replacements;
  replacements.SetQueryStr(output);
  return url.ReplaceComponents(replacements);
}

QueryIterator::QueryIterator(const GURL& url)
    : url_(url),
      at_end_(!url.is_valid()) {
  if (!at_end_) {
    query_ = url.parsed_for_possibly_invalid_spec().query;
    Advance();
  }
}

QueryIterator::~QueryIterator() {
}

std::string QueryIterator::GetKey() const {
  CR_DCHECK(!at_end_);
  if (key_.is_nonempty())
    return url_.spec().substr(key_.begin, key_.len);
  return std::string();
}

std::string QueryIterator::GetValue() const {
  CR_DCHECK(!at_end_);
  if (value_.is_nonempty())
    return url_.spec().substr(value_.begin, value_.len);
  return std::string();
}

const std::string& QueryIterator::GetUnescapedValue() {
  CR_DCHECK(!at_end_);
  if (value_.is_nonempty() && unescaped_value_.empty()) {
    unescaped_value_ = cr::UnescapeURLComponent(
        GetValue(),
        cr::UnescapeRule::SPACES |
        cr::UnescapeRule::URL_SPECIAL_CHARS |
        cr::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
  }
  return unescaped_value_;
}

bool QueryIterator::IsAtEnd() const {
  return at_end_;
}

void QueryIterator::Advance() {
  CR_DCHECK(!at_end_);
  key_.reset();
  value_.reset();
  unescaped_value_.clear();
  at_end_ =
      !crurl::ExtractQueryKeyValue(
          url_.spec().c_str(), &query_, &key_, &value_);
}

bool GetValueForKeyInQuery(const GURL& url,
                           const std::string& search_key,
                           std::string* out_value) {
  for (QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == search_key) {
      *out_value = it.GetUnescapedValue();
      return true;
    }
  }
  return false;
}

std::string TrimEndingDot(const cr::StringPiece& host) {
  cr::StringPiece host_trimmed = host;
  size_t len = host_trimmed.length();
  if (len > 1 && host_trimmed[len - 1] == '.') {
    host_trimmed.remove_suffix(1);
  }
  return host_trimmed.as_string();
}

void GetIdentityFromURL(const GURL& url,
                        cr::string16* username,
                        cr::string16* password) {
  cr::UnescapeRule::Type flags =
      cr::UnescapeRule::SPACES | 
      cr::UnescapeRule::URL_SPECIAL_CHARS;
  *username = cr::UnescapeAndDecodeUTF8URLComponent(
      url.username(), flags);
  *password = cr::UnescapeAndDecodeUTF8URLComponent(
      url.password(), flags);
}

}  // namespace crurl