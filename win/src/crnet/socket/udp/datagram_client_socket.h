// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRNET_SOCKET_UDP_DATAGRAM_CLIENT_SOCKET_H_
#define MINI_CHROMIUM_SRC_CRNET_SOCKET_UDP_DATAGRAM_CLIENT_SOCKET_H_

///#include "crnet/base/network_change_notifier.h"
#include "crnet/socket/socket.h"
#include "crnet/socket/udp/datagram_socket.h"

namespace crnet {

class IPEndPoint;

class CRNET_EXPORT_PRIVATE DatagramClientSocket : public DatagramSocket,
                                                  public Socket {
 public:
  ~DatagramClientSocket() override {}

  // Binds this socket to |network|. All data traffic on the socket will be sent
  // and received via |network|. Must be called before Connect(). This call will
  // fail if |network| has disconnected. Communication using this socket will
  // fail if |network| disconnects.
  // Returns a net error code.
  ///virtual int BindToNetwork(NetworkChangeNotifier::NetworkHandle network) = 0;

  // Same as BindToNetwork, except that the current default network is used.
  // Returns a net error code.
  ///virtual int BindToDefaultNetwork() = 0;

  // Returns the network that either BindToNetwork() or BindToDefaultNetwork()
  // bound this socket to. Returns NetworkChangeNotifier::kInvalidNetworkHandle
  // if not explicitly bound via BindToNetwork() or BindToDefaultNetwork().
  ///virtual NetworkChangeNotifier::NetworkHandle GetBoundNetwork() const = 0;

  // Initialize this socket as a client socket to server at |address|.
  // Returns a network error code.
  virtual int Connect(const IPEndPoint& address) = 0;
};

}  // namespace crnet

#endif  // MINI_CHROMIUM_SRC_CRNET_SOCKET_UDP_DATAGRAM_CLIENT_SOCKET_H_