// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crnet/udp/datagram_server_socket.h"

#include "crnet/base/ip_endpoint.h"
#include "crnet/base/net_errors.h"
#include "crnet/base/net_util.h"

namespace crnet {

int DatagramServerSocket::ListenWithAddressAndPort(
    const std::string& address_string,
    uint16_t port) {
  IPAddressNumber address_number;
  if (!ParseIPLiteralToNumber(address_string, &address_number)) {
    return ERR_ADDRESS_INVALID;
  }

  return Listen(IPEndPoint(address_number, port));
}

}  // namespace crnet