// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRNET_SOCKET_UDP_UDP_SERVER_SOCKET_H_
#define MINI_CHROMIUM_SRC_CRNET_SOCKET_UDP_UDP_SERVER_SOCKET_H_

#include <stdint.h>

#include "crbase/macros.h"
#include "crnet/base/completion_once_callback.h"
#include "crnet/socket/udp/datagram_server_socket.h"
#include "crnet/socket/udp/udp_socket.h"

namespace crnet {

class IPAddress;
class IPEndPoint;
///class BoundNetLog;

// A client socket that uses UDP as the transport layer.
class CRNET_EXPORT UDPServerSocket : public DatagramServerSocket {
 public:
  ///UDPServerSocket(net::NetLog* net_log, const net::NetLog::Source& source);
  UDPServerSocket(const UDPServerSocket&) = delete;
  UDPServerSocket& operator=(const UDPServerSocket&) = delete;

  UDPServerSocket();
  ~UDPServerSocket() override;

  // Implement DatagramServerSocket:
  int Listen(const IPEndPoint& address) override;
  int RecvFrom(IOBuffer* buf,
               int buf_len,
               IPEndPoint* address,
               CompletionOnceCallback callback) override;
  int SendTo(IOBuffer* buf,
             int buf_len,
             const IPEndPoint& address,
             CompletionOnceCallback callback) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int SetDoNotFragment() override;
  void SetMsgConfirm(bool confirm) override;
  void Close() override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  void UseNonBlockingIO() override;
  ///const BoundNetLog& NetLog() const override;
  void AllowAddressReuse() override;
  void AllowBroadcast() override;
  int JoinGroup(const IPAddress& group_address) const override;
  int LeaveGroup(const IPAddress& group_address) const override;
  int SetMulticastInterface(uint32_t interface_index) override;
  int SetMulticastTimeToLive(int time_to_live) override;
  int SetMulticastLoopbackMode(bool loopback) override;
  int SetDiffServCodePoint(DiffServCodePoint dscp) override;
  void DetachFromThread() override;


 private:
  UDPSocket socket_;
  bool allow_address_reuse_;
  bool allow_broadcast_;
};

}  // namespace crnet

#endif  // MINI_CHROMIUM_SRC_CRNET_SOCKET_UDP_UDP_SERVER_SOCKET_H_