// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crurl/gurl.h"

#include <stddef.h>

#include <algorithm>
#include <ostream>

#include "crbase/build_config.h"
#include "crbase/logging.h"
#include "crbase/strings/string_piece.h"
#include "crbase/strings/string_util.h"
#include "crurl/url_canon_stdstring.h"
#include "crurl/url_util.h"

#if defined(MINI_CHROMIUM_OS_WIN)
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace {

static std::string* empty_string = NULL;
static GURL* empty_gurl = NULL;

#if defined(MINI_CHROMIUM_OS_WIN)

// Returns a static reference to an empty string for returning a reference
// when there is no underlying string.
const std::string& EmptyStringForGURL() {
  // Avoid static object construction/destruction on startup/shutdown.
  if (!empty_string) {
    // Create the string. Be careful that we don't break in the case that this
    // is being called from multiple threads. Statics are not threadsafe.
    std::string* new_empty_string = new std::string;
    if (InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID*>(&empty_string), new_empty_string, NULL)) {
      // The old value was non-NULL, so no replacement was done. Another
      // thread did the initialization out from under us.
      delete new_empty_string;
    }
  }
  return *empty_string;
}

#else

static pthread_once_t empty_string_once = PTHREAD_ONCE_INIT;
static pthread_once_t empty_gurl_once = PTHREAD_ONCE_INIT;

void EmptyStringForGURLOnce(void) {
  empty_string = new std::string;
}

const std::string& EmptyStringForGURL() {
  // Avoid static object construction/destruction on startup/shutdown.
  pthread_once(&empty_string_once, EmptyStringForGURLOnce);
  return *empty_string;
}

#endif  // defined(MINI_CHROMIUM_OS_WIN)

}  // namespace

GURL::GURL() : is_valid_(false) {
}

GURL::GURL(const GURL& other)
    : spec_(other.spec_),
      is_valid_(other.is_valid_),
      parsed_(other.parsed_) {
  if (other.inner_url_)
    inner_url_.reset(new GURL(*other.inner_url_));
  // Valid filesystem urls should always have an inner_url_.
  CR_DCHECK(!is_valid_ || !SchemeIsFileSystem() || inner_url_);
}

GURL::GURL(const std::string& url_string) {
  InitCanonical(url_string, true);
}

GURL::GURL(const cr::string16& url_string) {
  InitCanonical(url_string, true);
}

GURL::GURL(const std::string& url_string, RetainWhiteSpaceSelector) {
  InitCanonical(url_string, false);
}

GURL::GURL(const char* canonical_spec,
           size_t canonical_spec_len,
           const crurl::Parsed& parsed,
           bool is_valid)
    : spec_(canonical_spec, canonical_spec_len),
      is_valid_(is_valid),
      parsed_(parsed) {
  InitializeFromCanonicalSpec();
}

GURL::GURL(std::string canonical_spec, const crurl::Parsed& parsed, 
           bool is_valid)
    : is_valid_(is_valid),
      parsed_(parsed) {
  spec_.swap(canonical_spec);
  InitializeFromCanonicalSpec();
}

template<typename STR>
void GURL::InitCanonical(const STR& input_spec, bool trim_path_end) {
  // Reserve enough room in the output for the input, plus some extra so that
  // we have room if we have to escape a few things without reallocating.
  spec_.reserve(input_spec.size() + 32);
  crurl::StdStringCanonOutput output(&spec_);
  is_valid_ = crurl::Canonicalize(
      input_spec.data(), static_cast<int>(input_spec.length()), trim_path_end,
      NULL, &output, &parsed_);

  output.Complete();  // Must be done before using string.
  if (is_valid_ && SchemeIsFileSystem()) {
    inner_url_.reset(new GURL(spec_.data(), parsed_.Length(),
                              *parsed_.inner_parsed(), true));
  }
}

void GURL::InitializeFromCanonicalSpec() {
  if (is_valid_ && SchemeIsFileSystem()) {
    inner_url_.reset(
        new GURL(spec_.data(), parsed_.Length(),
                 *parsed_.inner_parsed(), true));
  }

#ifndef NDEBUG
  // For testing purposes, check that the parsed canonical URL is identical to
  // what we would have produced. Skip checking for invalid URLs have no meaning
  // and we can't always canonicalize then reproducibly.
  if (is_valid_) {
    crurl::Component scheme;
    // We can't do this check on the inner_url of a filesystem URL, as
    // canonical_spec actually points to the start of the outer URL, so we'd
    // end up with infinite recursion in this constructor.
    if (!crurl::FindAndCompareScheme(
            spec_.data(), static_cast<int>(spec_.length()),
            crurl::kFileSystemScheme, &scheme) ||
        scheme.begin == parsed_.scheme.begin) {
      // We need to retain trailing whitespace on path URLs, as the |parsed_|
      // spec we originally received may legitimately contain trailing white-
      // space on the path or  components e.g. if the #ref has been
      // removed from a "foo:hello #ref" URL (see http://crbug.com/291747).
      GURL test_url(spec_, RETAIN_TRAILING_PATH_WHITEPACE);

      CR_DCHECK(test_url.is_valid_ == is_valid_);
      CR_DCHECK(test_url.spec_ == spec_);

      CR_DCHECK(test_url.parsed_.scheme == parsed_.scheme);
      CR_DCHECK(test_url.parsed_.username == parsed_.username);
      CR_DCHECK(test_url.parsed_.password == parsed_.password);
      CR_DCHECK(test_url.parsed_.host == parsed_.host);
      CR_DCHECK(test_url.parsed_.port == parsed_.port);
      CR_DCHECK(test_url.parsed_.path == parsed_.path);
      CR_DCHECK(test_url.parsed_.query == parsed_.query);
      CR_DCHECK(test_url.parsed_.ref == parsed_.ref);
    }
  }
#endif
}

GURL::~GURL() {
}

GURL& GURL::operator=(GURL other) {
  Swap(&other);
  return *this;
}

const std::string& GURL::spec() const {
  if (is_valid_ || spec_.empty())
    return spec_;

  CR_DCHECK(false) << "Trying to get the spec of an invalid URL!";
  return EmptyStringForGURL();
}

bool GURL::operator==(const GURL& other) const {
  return spec_ == other.spec_;
}

bool GURL::operator!=(const GURL& other) const {
  return spec_ != other.spec_;
}

bool GURL::operator<(const GURL& other) const {
  return spec_ < other.spec_;
}

bool GURL::operator>(const GURL& other) const {
  return spec_ > other.spec_;
}

// Note: code duplicated below (it's inconvenient to use a template here).
GURL GURL::Resolve(const std::string& relative) const {
  // Not allowed for invalid URLs.
  if (!is_valid_)
    return GURL();

  GURL result;

  // Reserve enough room in the output for the input, plus some extra so that
  // we have room if we have to escape a few things without reallocating.
  result.spec_.reserve(spec_.size() + 32);
  crurl::StdStringCanonOutput output(&result.spec_);

  if (!crurl::ResolveRelative(spec_.data(), 
                              static_cast<int>(spec_.length()),
                              parsed_, relative.data(),
                              static_cast<int>(relative.length()),
                              nullptr, &output, &result.parsed_)) {
    // Error resolving, return an empty URL.
    return GURL();
  }

  output.Complete();
  result.is_valid_ = true;
  if (result.SchemeIsFileSystem()) {
    result.inner_url_.reset(
        new GURL(result.spec_.data(), result.parsed_.Length(),
                 *result.parsed_.inner_parsed(), true));
  }
  return result;
}

// Note: code duplicated above (it's inconvenient to use a template here).
GURL GURL::Resolve(const cr::string16& relative) const {
  // Not allowed for invalid URLs.
  if (!is_valid_)
    return GURL();

  GURL result;

  // Reserve enough room in the output for the input, plus some extra so that
  // we have room if we have to escape a few things without reallocating.
  result.spec_.reserve(spec_.size() + 32);
  crurl::StdStringCanonOutput output(&result.spec_);

  if (!crurl::ResolveRelative(spec_.data(), 
                              static_cast<int>(spec_.length()),
                              parsed_, relative.data(),
                              static_cast<int>(relative.length()),
                              nullptr, &output, &result.parsed_)) {
    // Error resolving, return an empty URL.
    return GURL();
  }

  output.Complete();
  result.is_valid_ = true;
  if (result.SchemeIsFileSystem()) {
    result.inner_url_.reset(
        new GURL(result.spec_.data(), result.parsed_.Length(),
                 *result.parsed_.inner_parsed(), true));
  }
  return result;
}

// Note: code duplicated below (it's inconvenient to use a template here).
GURL GURL::ReplaceComponents(
    const crurl::Replacements<char>& replacements) const {
  GURL result;

  // Not allowed for invalid URLs.
  if (!is_valid_)
    return GURL();

  // Reserve enough room in the output for the input, plus some extra so that
  // we have room if we have to escape a few things without reallocating.
  result.spec_.reserve(spec_.size() + 32);
  crurl::StdStringCanonOutput output(&result.spec_);

  result.is_valid_ = crurl::ReplaceComponents(
      spec_.data(), static_cast<int>(spec_.length()), parsed_, replacements,
      NULL, &output, &result.parsed_);

  output.Complete();
  if (result.is_valid_ && result.SchemeIsFileSystem()) {
    result.inner_url_.reset(new GURL(spec_.data(), result.parsed_.Length(),
                                     *result.parsed_.inner_parsed(), true));
  }
  return result;
}

// Note: code duplicated above (it's inconvenient to use a template here).
GURL GURL::ReplaceComponents(
    const crurl::Replacements<cr::char16>& replacements) const {
  GURL result;

  // Not allowed for invalid URLs.
  if (!is_valid_)
    return GURL();

  // Reserve enough room in the output for the input, plus some extra so that
  // we have room if we have to escape a few things without reallocating.
  result.spec_.reserve(spec_.size() + 32);
  crurl::StdStringCanonOutput output(&result.spec_);

  result.is_valid_ = crurl::ReplaceComponents(
      spec_.data(), static_cast<int>(spec_.length()), parsed_, replacements,
      NULL, &output, &result.parsed_);

  output.Complete();
  if (result.is_valid_ && result.SchemeIsFileSystem()) {
    result.inner_url_.reset(new GURL(spec_.data(), result.parsed_.Length(),
                                     *result.parsed_.inner_parsed(), true));
  }
  return result;
}

GURL GURL::GetOrigin() const {
  // This doesn't make sense for invalid or nonstandard URLs, so return
  // the empty URL.
  if (!is_valid_ || !IsStandard())
    return GURL();

  if (SchemeIsFileSystem())
    return inner_url_->GetOrigin();

  crurl::Replacements<char> replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearPath();
  replacements.ClearQuery();
  replacements.ClearRef();

  return ReplaceComponents(replacements);
}

GURL GURL::GetAsReferrer() const {
  if (!is_valid_ || !SchemeIsHTTPOrHTTPS())
    return GURL();

  if (!has_ref() && !has_username() && !has_password())
    return GURL(*this);

  crurl::Replacements<char> replacements;
  replacements.ClearRef();
  replacements.ClearUsername();
  replacements.ClearPassword();
  return ReplaceComponents(replacements);
}

GURL GURL::GetWithEmptyPath() const {
  // This doesn't make sense for invalid or nonstandard URLs, so return
  // the empty URL.
  if (!is_valid_ || !IsStandard())
    return GURL();

  // We could optimize this since we know that the URL is canonical, and we are
  // appending a canonical path, so avoiding re-parsing.
  GURL other(*this);
  if (parsed_.path.len == 0)
    return other;

  // Clear everything after the path.
  other.parsed_.query.reset();
  other.parsed_.ref.reset();

  // Set the path, since the path is longer than one, we can just set the
  // first character and resize.
  other.spec_[other.parsed_.path.begin] = '/';
  other.parsed_.path.len = 1;
  other.spec_.resize(other.parsed_.path.begin + 1);
  return other;
}

bool GURL::IsStandard() const {
  return crurl::IsStandard(spec_.data(), parsed_.scheme);
}

bool GURL::SchemeIs(cr::StringPiece lower_ascii_scheme) const {
  CR_DCHECK(cr::IsStringASCII(lower_ascii_scheme));
  CR_DCHECK(cr::ToLowerASCII(lower_ascii_scheme) == lower_ascii_scheme);

  if (parsed_.scheme.len <= 0)
    return lower_ascii_scheme.empty();
  return scheme_piece() == lower_ascii_scheme;
}

bool GURL::SchemeIsHTTPOrHTTPS() const {
  return SchemeIs(crurl::kHttpScheme) || 
         SchemeIs(crurl::kHttpsScheme);
}

bool GURL::SchemeIsWSOrWSS() const {
  return SchemeIs(crurl::kWsScheme) || 
         SchemeIs(crurl::kWssScheme);
}

int GURL::IntPort() const {
  if (parsed_.port.is_nonempty())
    return crurl::ParsePort(spec_.data(), parsed_.port);
  return crurl::PORT_UNSPECIFIED;
}

int GURL::EffectiveIntPort() const {
  int int_port = IntPort();
  if (int_port == crurl::PORT_UNSPECIFIED && IsStandard())
    return crurl::DefaultPortForScheme(spec_.data() + parsed_.scheme.begin,
                                       parsed_.scheme.len);
  return int_port;
}

std::string GURL::ExtractFileName() const {
  crurl::Component file_component;
  crurl::ExtractFileName(spec_.data(), parsed_.path, &file_component);
  return ComponentString(file_component);
}

std::string GURL::PathForRequest() const {
  CR_DCHECK(parsed_.path.len > 0)
      << "Canonical path for requests should be non-empty";
  if (parsed_.ref.len >= 0) {
    // Clip off the reference when it exists. The reference starts after the
    // #-sign, so we have to subtract one to also remove it.
    return std::string(spec_, parsed_.path.begin,
                       parsed_.ref.begin - parsed_.path.begin - 1);
  }
  // Compute the actual path length, rather than depending on the spec's
  // terminator. If we're an inner_url, our spec continues on into our outer
  // URL's path/query/ref.
  int path_len = parsed_.path.len;
  if (parsed_.query.is_valid())
    path_len = parsed_.query.end() - parsed_.path.begin;

  return std::string(spec_, parsed_.path.begin, path_len);
}

std::string GURL::HostNoBrackets() const {
  // If host looks like an IPv6 literal, strip the square brackets.
  crurl::Component h(parsed_.host);
  if (h.len >= 2 && spec_[h.begin] == '[' && spec_[h.end() - 1] == ']') {
    h.begin++;
    h.len -= 2;
  }
  return ComponentString(h);
}

std::string GURL::GetContent() const {
  return is_valid_ ? ComponentString(parsed_.GetContent()) : std::string();
}

bool GURL::HostIsIPAddress() const {
  if (!is_valid_ || spec_.empty())
     return false;

  crurl::RawCanonOutputT<char, 128> ignored_output;
  crurl::CanonHostInfo host_info;
  crurl::CanonicalizeIPAddress(spec_.c_str(), parsed_.host, 
                               &ignored_output, &host_info);
  return host_info.IsIPAddress();
}

#if defined(MINI_CHROMIUM_OS_WIN)

const GURL& GURL::EmptyGURL() {
  // Avoid static object construction/destruction on startup/shutdown.
  if (!empty_gurl) {
    // Create the string. Be careful that we don't break in the case that this
    // is being called from multiple threads.
    GURL* new_empty_gurl = new GURL;
    if (InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID*>(&empty_gurl), new_empty_gurl, NULL)) {
      // The old value was non-NULL, so no replacement was done. Another
      // thread did the initialization out from under us.
      delete new_empty_gurl;
    }
  }
  return *empty_gurl;
}

#else

void EmptyGURLOnce(void) {
  empty_gurl = new GURL;
}

const GURL& GURL::EmptyGURL() {
  // Avoid static object construction/destruction on startup/shutdown.
  pthread_once(&empty_gurl_once, EmptyGURLOnce);
  return *empty_gurl;
}

#endif  // defined(MINI_CHROMIUM_OS_WIN)

bool GURL::DomainIs(cr::StringPiece lower_ascii_domain) const {
  if (!is_valid_ || lower_ascii_domain.empty())
    return false;

  // FileSystem URLs have empty parsed_.host, so check this first.
  if (SchemeIsFileSystem() && inner_url_)
    return inner_url_->DomainIs(lower_ascii_domain);

  if (!parsed_.host.is_nonempty())
    return false;

  // If the host name ends with a dot but the input domain doesn't,
  // then we ignore the dot in the host name.
  const char* host_last_pos = spec_.data() + parsed_.host.end() - 1;
  int host_len = parsed_.host.len;
  int domain_len = static_cast<int>(lower_ascii_domain.length());
  if ('.' == *host_last_pos && '.' != lower_ascii_domain[domain_len - 1]) {
    host_last_pos--;
    host_len--;
  }

  if (host_len < domain_len)
    return false;

  // |host_first_pos| is the start of the compared part of the host name, not
  // start of the whole host name.
  const char* host_first_pos = spec_.data() + parsed_.host.begin +
                               host_len - domain_len;

  if (!cr::LowerCaseEqualsASCII(
           cr::StringPiece(host_first_pos, domain_len), lower_ascii_domain))
    return false;

  // Make sure there aren't extra characters in host before the compared part;
  // if the host name is longer than the input domain name, then the character
  // immediately before the compared part should be a dot. For example,
  // www.google.com has domain "google.com", but www.iamnotgoogle.com does not.
  if ('.' != lower_ascii_domain[0] && host_len > domain_len &&
      '.' != *(host_first_pos - 1))
    return false;

  return true;
}

void GURL::Swap(GURL* other) {
  spec_.swap(other->spec_);
  std::swap(is_valid_, other->is_valid_);
  std::swap(parsed_, other->parsed_);
  inner_url_.swap(other->inner_url_);
}

std::ostream& operator<<(std::ostream& out, const GURL& url) {
  return out << url.possibly_invalid_spec();
}