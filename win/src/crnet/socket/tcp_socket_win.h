// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_NET_SOCKET_TCP_SOCKET_WIN_H_
#define MINI_CHROMIUM_NET_SOCKET_TCP_SOCKET_WIN_H_

#include <stdint.h>
#include <winsock2.h>

#include <memory>

#include "crbase/compiler_specific.h"
#include "crbase/macros.h"
#include "crbase/memory/ref_counted.h"
#include "crbase/threading/non_thread_safe.h"
#include "crbase/win/object_watcher.h"
#include "crnet/base/address_family.h"
#include "crnet/base/completion_once_callback.h"
#include "crnet/base/net_export.h"
///#include "net/log/net_log.h"

namespace crnet {

class AddressList;
class IOBuffer;
class IPEndPoint;

class CRNET_EXPORT TCPSocketWin 
    : MSVC_NON_EXPORTED_BASE(public crbase::NonThreadSafe),
                             public crbase::win::ObjectWatcher::Delegate  {
 public:
  ///TCPSocketWin(NetLog* net_log, const NetLog::Source& source);
  // Added by johao.
  TCPSocketWin(const TCPSocketWin&) = delete;
  TCPSocketWin& operator=(const TCPSocketWin&) = delete;
  TCPSocketWin();

   ~TCPSocketWin() override;

  int Open(AddressFamily family);

  // Both AdoptConnectedSocket and AdoptListenSocket take ownership of an
  // existing socket. AdoptConnectedSocket takes an already connected
  // socket. AdoptListenSocket takes a socket that is intended to accept
  // connection. In some sense, AdoptListenSocket is more similar to Open.
  int AdoptConnectedSocket(SOCKET socket, const IPEndPoint& peer_address);
  int AdoptListenSocket(SOCKET socket);

  int Bind(const IPEndPoint& address);

  int Listen(int backlog);
  int Accept(std::unique_ptr<TCPSocketWin>* socket,
             IPEndPoint* address,
             CompletionOnceCallback callback);

  int Connect(const IPEndPoint& address, 
              CompletionOnceCallback callback);
  bool IsConnected() const;
  bool IsConnectedAndIdle() const;

  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported.
  int Read(IOBuffer* buf, int buf_len, CompletionOnceCallback callback);
  int Write(IOBuffer* buf, int buf_len, CompletionOnceCallback callback);

  int GetLocalAddress(IPEndPoint* address) const;
  int GetPeerAddress(IPEndPoint* address) const;

  // Sets various socket options.
  // The commonly used options for server listening sockets:
  // - SetExclusiveAddrUse().
  int SetDefaultOptionsForServer();
  // The commonly used options for client sockets and accepted sockets:
  // - Increase the socket buffer sizes for WinXP;
  // - SetNoDelay(true);
  // - SetKeepAlive(true, 45).
  void SetDefaultOptionsForClient();
  int SetExclusiveAddrUse();
  int SetReceiveBufferSize(int32_t size);
  int SetSendBufferSize(int32_t size);
  bool SetKeepAlive(bool enable, int delay);
  bool SetNoDelay(bool no_delay);

  // Gets the estimated RTT. Returns false if the RTT is
  // unavailable. May also return false when estimated RTT is 0.
  bool GetEstimatedRoundTripTime(crbase::TimeDelta* out_rtt) const
      CR_WARN_UNUSED_RESULT;

  void Close();

  // Setter/Getter methods for TCP FastOpen socket option.
  // NOOPs since TCP FastOpen is not implemented in Windows.
  bool UsingTCPFastOpen() const { return false; }
  void EnableTCPFastOpenIfSupported() {}

  bool IsValid() const { return socket_ != INVALID_SOCKET; }

  // Detachs from the current thread, to allow the socket to be transferred to
  // a new thread. Should only be called when the object is no longer used by
  // the old thread.
  void DetachFromThread();

  // Marks the start/end of a series of connect attempts for logging purpose.
  //
  // TCPClientSocket may attempt to connect to multiple addresses until it
  // succeeds in establishing a connection. The corresponding log will have
  // multiple NetLog::TYPE_TCP_CONNECT_ATTEMPT entries nested within a
  // NetLog::TYPE_TCP_CONNECT. These methods set the start/end of
  // NetLog::TYPE_TCP_CONNECT.
  //
  // TODO(yzshen): Change logging format and let TCPClientSocket log the
  // start/end of a series of connect attempts itself.
  ///void StartLoggingMultipleConnectAttempts(const AddressList& addresses);
  ///void EndLoggingMultipleConnectAttempts(int net_error);

  ///const BoundNetLog& net_log() const { return net_log_; }

 private:
  class Core;

  // crbase::ObjectWatcher::Delegate implementation.
  void OnObjectSignaled(HANDLE object) override;

  int AcceptInternal(std::unique_ptr<TCPSocketWin>* socket,
                     IPEndPoint* address);

  int DoConnect();
  void DoConnectComplete(int result);

  ///void LogConnectBegin(const AddressList& addresses);
  ///void LogConnectEnd(int net_error);

  int DoRead(IOBuffer* buf, int buf_len, CompletionOnceCallback callback);
  void DidCompleteConnect();
  void DidCompleteWrite();
  void DidSignalRead();

  SOCKET socket_;

  HANDLE accept_event_;
  crbase::win::ObjectWatcher accept_watcher_;

  std::unique_ptr<TCPSocketWin>* accept_socket_;
  IPEndPoint* accept_address_;
  CompletionOnceCallback accept_callback_;

  // The various states that the socket could be in.
  bool waiting_connect_;
  bool waiting_read_;
  bool waiting_write_;

  // The core of the socket that can live longer than the socket itself. We pass
  // resources to the Windows async IO functions and we have to make sure that
  // they are not destroyed while the OS still references them.
  crbase::scoped_refptr<Core> core_;

  // External callback; called when connect or read is complete.
  CompletionOnceCallback read_callback_;

  // External callback; called when write is complete.
  CompletionOnceCallback write_callback_;

  std::unique_ptr<IPEndPoint> peer_address_;
  // The OS error that a connect attempt last completed with.
  int connect_os_error_;

  ///bool logging_multiple_connect_attempts_;

  ///BoundNetLog net_log_;

  ///DISALLOW_COPY_AND_ASSIGN(TCPSocketWin);
};

}  // namespace crnet

#endif  // MINI_CHROMIUM_NET_SOCKET_TCP_SOCKET_WIN_H_