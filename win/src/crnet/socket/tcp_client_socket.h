// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRNET_SOCKET_TCP_CLIENT_SOCKET_H_
#define MINI_CHROMIUM_SRC_CRNET_SOCKET_TCP_CLIENT_SOCKET_H_

#include <stdint.h>

#include <memory>

#include "crbase/compiler_specific.h"
#include "crbase/macros.h"
#include "crnet/base/address_list.h"
#include "crnet/base/completion_once_callback.h"
#include "crnet/base/net_export.h"
///#include "crnet/log/net_log.h"
#include "crnet/socket/connection_attempts.h"
#include "crnet/socket/stream_socket.h"
#include "crnet/socket/tcp_socket.h"

namespace crnet {

// A client socket that uses TCP as the transport layer.
class CRNET_EXPORT TCPClientSocket : public StreamSocket {
 public:
  // The IP address(es) and port number to connect to.  The TCP socket will try
  // each IP address in the list until it succeeds in establishing a
  // connection.
  ///TCPClientSocket(const AddressList& addresses,
  ///                net::NetLog* net_log,
  ///                const net::NetLog::Source& source);
  TCPClientSocket(const TCPClientSocket&) = delete;
  TCPClientSocket& operator=(const TCPClientSocket&) = delete;
  TCPClientSocket(const AddressList& addresses);

  // Adopts the given, connected socket and then acts as if Connect() had been
  // called. This function is used by TCPServerSocket and for testing.
  TCPClientSocket(std::unique_ptr<TCPSocket> connected_socket,
                  const IPEndPoint& peer_address);

  ~TCPClientSocket() override;

  // Binds the socket to a local IP address and port.
  int Bind(const IPEndPoint& address);

  // StreamSocket implementation.
  int Connect(CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  ///const BoundNetLog& NetLog() const override;
  void SetSubresourceSpeculation() override;
  void SetOmniboxSpeculation() override;
  bool WasEverUsed() const override;
  bool UsingTCPFastOpen() const override;
  void EnableTCPFastOpenIfSupported() override;
  bool WasNpnNegotiated() const override;
  ///NextProto GetNegotiatedProtocol() const override;
  ///bool GetSSLInfo(SSLInfo* ssl_info) override;

  // Socket implementation.
  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

  virtual bool SetKeepAlive(bool enable, int delay);
  virtual bool SetNoDelay(bool no_delay);

  void GetConnectionAttempts(ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override;
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override;
  int64_t GetTotalReceivedBytes() const override;

 private:
  // State machine for connecting the socket.
  enum ConnectState {
    CONNECT_STATE_CONNECT,
    CONNECT_STATE_CONNECT_COMPLETE,
    CONNECT_STATE_NONE,
  };

  // State machine used by Connect().
  int DoConnectLoop(int result);
  int DoConnect();
  int DoConnectComplete(int result);

  // Helper used by Disconnect(), which disconnects minus resetting
  // current_address_index_ and bind_address_.
  void DoDisconnect();

  void DidCompleteConnect(int result);
  void DidCompleteRead(CompletionOnceCallback callback, int result);
  void DidCompleteWrite(CompletionOnceCallback callback, int result);
  void DidCompleteReadWrite(CompletionOnceCallback callback, int result);

  int OpenSocket(AddressFamily family);

  // Emits histograms for TCP metrics, at the time the socket is
  // disconnected.
  ///void EmitTCPMetricsHistogramsOnDisconnect();

  std::unique_ptr<TCPSocket> socket_;

  // Local IP address and port we are bound to. Set to NULL if Bind()
  // wasn't called (in that case OS chooses address/port).
  std::unique_ptr<IPEndPoint> bind_address_;

  // The list of addresses we should try in order to establish a connection.
  AddressList addresses_;

  // Where we are in above list. Set to -1 if uninitialized.
  int current_address_index_;

  // External callback; called when connect is complete.
  CompletionOnceCallback connect_callback_;

  // The next state for the Connect() state machine.
  ConnectState next_connect_state_;

  // This socket was previously disconnected and has not been re-connected.
  bool previously_disconnected_;

  // Record of connectivity and transmissions, for use in speculative connection
  // histograms.
  UseHistory use_history_;

  // Failed connection attempts made while trying to connect this socket.
  ConnectionAttempts connection_attempts_;

  // Total number of bytes received by the socket.
  int64_t total_received_bytes_;
};

}  // namespace crnet

#endif  // MINI_CHROMIUM_SRC_CRNET_SOCKET_TCP_CLIENT_SOCKET_H_