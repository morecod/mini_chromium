// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crnet/socket/udp/datagram_server_socket.h"

#include "crnet/base/ip_endpoint.h"
#include "crnet/base/net_errors.h"

namespace crnet {

int DatagramServerSocket::ListenWithAddressAndPort(
    const std::string& address_string,
    uint16_t port) {
  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(address_string)) {
    return ERR_ADDRESS_INVALID;
  }

  return Listen(IPEndPoint(ip_address, port));
}

}  // namespace crnet