// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_CRNET_SOCKET_UDP_CLIENT_SOCKET_H_
#define MINI_CHROMIUM_CRNET_SOCKET_UDP_CLIENT_SOCKET_H_

#include <stdint.h>

#include "crbase/macros.h"
#include "crnet/base/rand_callback.h"
///#include "crnet/log/net_log.h"
#include "crnet/udp/datagram_client_socket.h"
#include "crnet/udp/udp_socket.h"

namespace crnet {

///class BoundNetLog;

// A client socket that uses UDP as the transport layer.
class CRNET_EXPORT_PRIVATE UDPClientSocket : public DatagramClientSocket {
 public:
  ///UDPClientSocket(DatagramSocket::BindType bind_type,
  ///                const RandIntCallback& rand_int_cb,
  ///                net::NetLog* net_log,
  ///                const net::NetLog::Source& source);
  UDPClientSocket(const UDPClientSocket&) = delete;
  UDPClientSocket& operator=(const UDPClientSocket&) = delete;

  UDPClientSocket(DatagramSocket::BindType bind_type,
                  const RandIntCallback& rand_int_cb);
  ~UDPClientSocket() override;

  // DatagramClientSocket implementation.
  ///int BindToNetwork(NetworkChangeNotifier::NetworkHandle network) override;
  ///int BindToDefaultNetwork() override;
  ///NetworkChangeNotifier::NetworkHandle GetBoundNetwork() const override;
  int Connect(const IPEndPoint& address) override;
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override;
  void Close() override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  ///const BoundNetLog& NetLog() const override;

#if defined(MINI_CHROMIUM_OS_WIN)
  // Switch to use non-blocking IO. Must be called right after construction and
  // before other calls.
  void UseNonBlockingIO();
#endif

 private:
  UDPSocket socket_;
  ///NetworkChangeNotifier::NetworkHandle network_;
  ///DISALLOW_COPY_AND_ASSIGN(UDPClientSocket);
};

}  // namespace crnet

#endif  // MINI_CHROMIUM_CRNET_SOCKET_UDP_CLIENT_SOCKET_H_