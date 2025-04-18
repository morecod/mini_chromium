// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRNET_BASE_ADDRESS_LIST_H_
#define MINI_CHROMIUM_SRC_CRNET_BASE_ADDRESS_LIST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "crbase/compiler_specific.h"
#include "crnet/base/ip_endpoint.h"
#include "crnet/base/ip_address.h"
#include "crnet/base/net_export.h"
///#include "crnet/log/net_log.h"

struct addrinfo;

namespace crnet {

class IPAddress;

class CRNET_EXPORT AddressList
    : MSVC_NON_EXPORTED_BASE(private std::vector<IPEndPoint>) {
 public:
  AddressList();
  ~AddressList();

  // Creates an address list for a single IP literal.
  explicit AddressList(const IPEndPoint& endpoint);

  static AddressList CreateFromIPAddress(const IPAddress& address,
                                         uint16_t port);

  static AddressList CreateFromIPAddressList(
      const IPAddressList& addresses,
      const std::string& canonical_name);

  // Copies the data from |head| and the chained list into an AddressList.
  static AddressList CreateFromAddrinfo(const struct addrinfo* head);

  // Returns a copy of |list| with port on each element set to |port|.
  static AddressList CopyWithPort(const AddressList& list, uint16_t port);

  // TODO(szym): Remove all three. http://crbug.com/126134
  const std::string& canonical_name() const {
    return canonical_name_;
  }

  void set_canonical_name(const std::string& canonical_name) {
    canonical_name_ = canonical_name;
  }

  // Sets canonical name to the literal of the first IP address on the list.
  void SetDefaultCanonicalName();

  // Creates a callback for use with the NetLog that returns a Value
  // representation of the address list.  The callback must be destroyed before
  // |this| is.
  ///NetLog::ParametersCallback CreateNetLogCallback() const;

  // Exposed methods from std::vector.
  using std::vector<IPEndPoint>::size;
  using std::vector<IPEndPoint>::empty;
  using std::vector<IPEndPoint>::clear;
  using std::vector<IPEndPoint>::reserve;
  using std::vector<IPEndPoint>::capacity;
  using std::vector<IPEndPoint>::operator[];
  using std::vector<IPEndPoint>::front;
  using std::vector<IPEndPoint>::back;
  using std::vector<IPEndPoint>::push_back;
  using std::vector<IPEndPoint>::insert;
  using std::vector<IPEndPoint>::erase;
  using std::vector<IPEndPoint>::iterator;
  using std::vector<IPEndPoint>::const_iterator;
  using std::vector<IPEndPoint>::begin;
  using std::vector<IPEndPoint>::end;
  using std::vector<IPEndPoint>::rbegin;
  using std::vector<IPEndPoint>::rend;

 private:
  // TODO(szym): Remove. http://crbug.com/126134
  std::string canonical_name_;
};

}  // namespace crnet

#endif  // MINI_CHROMIUM_SRC_CRNET_BASE_ADDRESS_LIST_H_