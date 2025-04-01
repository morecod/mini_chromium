// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crnet/socket/client_socket_factory.h"

#include <utility>

#include "crbase/lazy_instance.h"
#include "crbase/build_config.h"
#include "crnet/socket/tcp_client_socket.h"
#include "crnet/udp/udp_client_socket.h"

namespace crnet {

namespace {

class DefaultClientSocketFactory : public ClientSocketFactory {
 public:
  DefaultClientSocketFactory() {
  }

  ~DefaultClientSocketFactory() override {
    // Note: This code never runs, as the factory is defined as a Leaky
    // singleton.
  }

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      const RandIntCallback& rand_int_cb) override {
    return std::unique_ptr<DatagramClientSocket>(
        new UDPClientSocket(bind_type, rand_int_cb));
  }

  std::unique_ptr<StreamSocket> CreateTransportClientSocket(
      const AddressList& addresses) override {
    return std::unique_ptr<StreamSocket>(new TCPClientSocket(
        addresses));
  }
};

static crbase::LazyInstance<DefaultClientSocketFactory>::Leaky
    g_default_client_socket_factory = CR_LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ClientSocketFactory* ClientSocketFactory::GetDefaultFactory() {
  return g_default_client_socket_factory.Pointer();
}

}  // namespace crnet
