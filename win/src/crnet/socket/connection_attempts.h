// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRNET_SOCKET_CONNECTION_ATTEMPTS_H_
#define MINI_CHROMIUM_SRC_CRNET_SOCKET_CONNECTION_ATTEMPTS_H_

#include "crnet/base/ip_endpoint.h"

namespace crnet {

// A record of an connection attempt made to connect to a host. Includes TCP
// and SSL errors, but not proxy connections.
struct ConnectionAttempt {
  ConnectionAttempt(const IPEndPoint endpoint, int result)
      : endpoint(endpoint), result(result) {}

  // Address and port the socket layer attempted to connect to.
  IPEndPoint endpoint;

  // Net error indicating the result of that attempt.
  int result;
};

// Multiple connection attempts, as might be tracked in an HttpTransaction or a
// URLRequest. Order is insignificant.
typedef std::vector<ConnectionAttempt> ConnectionAttempts;

}  // namespace crnet

#endif  // MINI_CHROMIUM_SRC_CRNET_SOCKET_CONNECTION_ATTEMPTS_H_