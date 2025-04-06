// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRNET_SOCKET_CLIENT_SOCKET_FACTORY_H_
#define MINI_CHROMIUM_SRC_CRNET_SOCKET_CLIENT_SOCKET_FACTORY_H_

#include <memory>
#include <string>

#include "crnet/base/net_export.h"
#include "crnet/base/rand_callback.h"
#include "crnet/socket/udp/datagram_socket.h"

namespace crnet {

class AddressList;
class DatagramClientSocket;
class StreamSocket;

// An interface used to instantiate StreamSocket objects.  Used to facilitate
// testing code with mock socket implementations.
class CRNET_EXPORT ClientSocketFactory {
 public:
  virtual ~ClientSocketFactory() {}

  // |source| is the NetLogSource for the entity trying to create the socket,
  // if it has one.
  virtual std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type) = 0;

  virtual std::unique_ptr<StreamSocket> CreateTransportClientSocket(
      const AddressList& addresses) = 0;

  // Returns the default ClientSocketFactory.
  static ClientSocketFactory* GetDefaultFactory();
};

}  // namespace crnet

#endif  // MINI_CHROMIUM_SRC_CRNET_SOCKET_CLIENT_SOCKET_FACTORY_H_
