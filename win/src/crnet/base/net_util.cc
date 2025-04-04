// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crnet/base/net_util.h"

#include <algorithm>
#include <string>

#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_OS_WIN)
#include <windows.h>
#include <iphlpapi.h>
#include <winsock2.h>
///#pragma comment(lib, "iphlpapi.lib")
#elif defined(MINI_CHROMIUM_OS_POSIX)
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#endif  // defined(OS_POSIX)

#include "crbase/logging.h"
#include "crbase/strings/string_util.h"
#include "crbase/strings/stringprintf.h"
#include "crbase/strings/utf_string_conversions.h"
#include "crnet/base/address_list.h"
#include "crnet/base/ip_address_number.h"
///#include "crnet/base/registry_controlled_domains/registry_controlled_domain.h"
#include "crnet/base/sys_byteorder.h"
#include "crurl/gurl.h"
#include "crurl/gurl_util.h"
#include "crurl/third_party/mozilla/url_parse.h"
#include "crurl/url_canon.h"
#include "crurl/url_canon_ip.h"

#if defined(MINI_CHROMIUM_OS_WIN)
#include "crnet/base/winsock_init.h"
#endif

namespace crnet {

namespace {

std::string NormalizeHostname(cr::StringPiece host) {
  std::string result = cr::ToLowerASCII(host);
  if (!result.empty() && *result.rbegin() == '.')
    result.resize(result.size() - 1);
  return result;
}

bool IsNormalizedLocalhostTLD(const std::string& host) {
  return cr::EndsWith(host, ".localhost", cr::CompareCase::SENSITIVE);
}

// |host| should be normalized.
bool IsLocalHostname(const std::string& host) {
  return host == "localhost" || host == "localhost.localdomain" ||
         IsNormalizedLocalhostTLD(host);
}

// |host| should be normalized.
bool IsLocal6Hostname(const std::string& host) {
  return host == "localhost6" || host == "localhost6.localdomain6";
}

}  // namespace

std::string CanonicalizeHost(const std::string& host,
                             crurl::CanonHostInfo* host_info) {
  // Try to canonicalize the host.
  const crurl::Component raw_host_component(0, static_cast<int>(host.length()));
  std::string canon_host;
  crurl::StdStringCanonOutput canon_host_output(&canon_host);
  crurl::CanonicalizeHostVerbose(host.c_str(), raw_host_component,
                                 &canon_host_output, host_info);

  if (host_info->out_host.is_nonempty() &&
      host_info->family != crurl::CanonHostInfo::BROKEN) {
    // Success!  Assert that there's no extra garbage.
    canon_host_output.Complete();
    CR_DCHECK_EQ(host_info->out_host.len, static_cast<int>(canon_host.length()));
  } else {
    // Empty host, or canonicalization failed.  We'll return empty.
    canon_host.clear();
  }

  return canon_host;
}

inline bool IsHostCharAlphanumeric(char c) {
  // We can just check lowercase because uppercase characters have already been
  // normalized.
  return ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9'));
}

bool IsCanonicalizedHostCompliant(const std::string& host) {
  if (host.empty())
    return false;

  bool in_component = false;
  bool most_recent_component_started_alphanumeric = false;

  for (std::string::const_iterator i(host.begin()); i != host.end(); ++i) {
    const char c = *i;
    if (!in_component) {
      most_recent_component_started_alphanumeric = IsHostCharAlphanumeric(c);
      if (!most_recent_component_started_alphanumeric && (c != '-') &&
          (c != '_')) {
        return false;
      }
      in_component = true;
    } else if (c == '.') {
      in_component = false;
    } else if (!IsHostCharAlphanumeric(c) && (c != '-') && (c != '_')) {
      return false;
    }
  }

  return most_recent_component_started_alphanumeric;
}

bool ParseHostAndPort(std::string::const_iterator host_and_port_begin,
                      std::string::const_iterator host_and_port_end,
                      std::string* host,
                      int* port) {
  if (host_and_port_begin >= host_and_port_end)
    return false;

  // When using url, we use char*.
  const char* auth_begin = &(*host_and_port_begin);
  int auth_len = static_cast<int>(host_and_port_end - host_and_port_begin);

  crurl::Component auth_component(0, auth_len);
  crurl::Component username_component;
  crurl::Component password_component;
  crurl::Component hostname_component;
  crurl::Component port_component;

  crurl::ParseAuthority(auth_begin, auth_component, &username_component,
      &password_component, &hostname_component, &port_component);

  // There shouldn't be a username/password.
  if (username_component.is_valid() || password_component.is_valid())
    return false;

  if (!hostname_component.is_nonempty())
    return false;  // Failed parsing.

  int parsed_port_number = -1;
  if (port_component.is_nonempty()) {
    parsed_port_number = crurl::ParsePort(auth_begin, port_component);

    // If parsing failed, port_number will be either PORT_INVALID or
    // PORT_UNSPECIFIED, both of which are negative.
    if (parsed_port_number < 0)
      return false;  // Failed parsing the port number.
  }

  if (port_component.len == 0)
    return false;  // Reject inputs like "foo:"

  unsigned char tmp_ipv6_addr[16];

  // If the hostname starts with a bracket, it is either an IPv6 literal or
  // invalid. If it is an IPv6 literal then strip the brackets.
  if (hostname_component.len > 0 &&
      auth_begin[hostname_component.begin] == '[') {
    if (auth_begin[hostname_component.end() - 1] == ']' &&
        crurl::IPv6AddressToNumber(
            auth_begin, hostname_component, tmp_ipv6_addr)) {
      // Strip the brackets.
      hostname_component.begin++;
      hostname_component.len -= 2;
    } else {
      return false;
    }
  }

  // Pass results back to caller.
  host->assign(auth_begin + hostname_component.begin, hostname_component.len);
  *port = parsed_port_number;

  return true;  // Success.
}

bool ParseHostAndPort(const std::string& host_and_port,
                      std::string* host,
                      int* port) {
  return ParseHostAndPort(
      host_and_port.begin(), host_and_port.end(), host, port);
}

std::string GetHostAndPort(const GURL& url) {
  // For IPv6 literals, GURL::host() already includes the brackets so it is
  // safe to just append a colon.
  return cr::StringPrintf("%s:%d", url.host().c_str(),
                              url.EffectiveIntPort());
}

std::string GetHostAndOptionalPort(const GURL& url) {
  // For IPv6 literals, GURL::host() already includes the brackets
  // so it is safe to just append a colon.
  if (url.has_port())
    return cr::StringPrintf(
        "%s:%s", url.host().c_str(), url.port().c_str());
  return url.host();
}

///bool IsHostnameNonUnique(const std::string& hostname) {
///  // CanonicalizeHost requires surrounding brackets to parse an IPv6 address.
///  const std::string host_or_ip = hostname.find(':') != std::string::npos ?
///      "[" + hostname + "]" : hostname;
///  crurl::CanonHostInfo host_info;
///  std::string canonical_name = CanonicalizeHost(host_or_ip, &host_info);
///
///  // If canonicalization fails, then the input is truly malformed. However,
///  // to avoid mis-reporting bad inputs as "non-unique", treat them as unique.
///  if (canonical_name.empty())
///    return false;
///
///  // If |hostname| is an IP address, check to see if it's in an IANA-reserved
///  // range.
///  if (host_info.IsIPAddress()) {
///    IPAddressNumber host_addr;
///    if (!ParseIPLiteralToNumber(hostname.substr(host_info.out_host.begin,
///                                                host_info.out_host.len),
///                                &host_addr)) {
///      return false;
///    }
///    switch (host_info.family) {
///      case crurl::CanonHostInfo::IPV4:
///      case crurl::CanonHostInfo::IPV6:
///        return IsIPAddressReserved(host_addr);
///      case crurl::CanonHostInfo::NEUTRAL:
///      case crurl::CanonHostInfo::BROKEN:
///        return false;
///    }
///  }
///
///  // Check for a registry controlled portion of |hostname|, ignoring private
///  // registries, as they already chain to ICANN-administered registries,
///  // and explicitly ignoring unknown registries.
///  //
///  // Note: This means that as new gTLDs are introduced on the Internet, they
///  // will be treated as non-unique until the registry controlled domain list
///  // is updated. However, because gTLDs are expected to provide significant
///  // advance notice to deprecate older versions of this code, this an
///  // acceptable tradeoff.
///  return 0 == registry_controlled_domains::GetRegistryLength(
///                  canonical_name,
///                  registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
///                  registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
///}

std::string GetHostName() {
#if defined(MINI_CHROMIUM_OS_WIN)
  EnsureWinsockInit();
#endif

  // Host names are limited to 255 bytes.
  char buffer[256];
  int result = gethostname(buffer, sizeof(buffer));
  if (result != 0) {
    CR_DLOG(WARNING) << "gethostname() failed with " << result;
    buffer[0] = '\0';
  }
  return std::string(buffer);
}

std::string GetHostOrSpecFromURL(const GURL& url) {
  return url.has_host() ? crurl::TrimEndingDot(url.host_piece()) : url.spec();
}

GURL SimplifyUrlForRequest(const GURL& url) {
  CR_DCHECK(url.is_valid());
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

bool ResolveLocalHostname(cr::StringPiece host,
                          uint16_t port,
                          AddressList* address_list) {
  static const unsigned char kLocalhostIPv4[] = {127, 0, 0, 1};
  static const unsigned char kLocalhostIPv6[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

  std::string normalized_host = NormalizeHostname(host);

  address_list->clear();

  bool is_local6 = IsLocal6Hostname(normalized_host);
  if (!is_local6 && !IsLocalHostname(normalized_host))
    return false;

  address_list->push_back(
      IPEndPoint(
          IPAddressNumber(kLocalhostIPv6,
                          kLocalhostIPv6 + cr_arraysize(kLocalhostIPv6)),
          port));
  if (!is_local6) {
    address_list->push_back(
        IPEndPoint(
            IPAddressNumber(kLocalhostIPv4,
                            kLocalhostIPv4 + cr_arraysize(kLocalhostIPv4)),
            port));
  }

  return true;
}

bool IsLocalhost(cr::StringPiece host) {
  std::string normalized_host = NormalizeHostname(host);
  if (IsLocalHostname(normalized_host) || IsLocal6Hostname(normalized_host))
    return true;

  IPAddressNumber ip_number;
  if (ParseIPLiteralToNumber(host, &ip_number)) {
    size_t size = ip_number.size();
    switch (size) {
      case kIPv4AddressSize: {
        IPAddressNumber localhost_prefix;
        localhost_prefix.push_back(127);
        for (int i = 0; i < 3; ++i) {
          localhost_prefix.push_back(0);
        }
        return IPNumberMatchesPrefix(ip_number, localhost_prefix, 8);
      }

      case kIPv6AddressSize: {
        struct in6_addr sin6_addr;
        memcpy(&sin6_addr, &ip_number[0], kIPv6AddressSize);
        return !!IN6_IS_ADDR_LOOPBACK(&sin6_addr);
      }

      default:
        CR_NOTREACHED();
    }
  }

  return false;
}

}  // namespace crnet