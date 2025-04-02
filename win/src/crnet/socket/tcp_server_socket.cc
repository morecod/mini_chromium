// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crnet/socket/tcp_server_socket.h"

#include <utility>

#include "crbase/functional/bind.h"
#include "crbase/functional/bind_helpers.h"
#include "crbase/logging.h"
#include "crnet/base/net_errors.h"
#include "crnet/socket/tcp_client_socket.h"

namespace crnet {

///TCPServerSocket::TCPServerSocket(NetLog* net_log, const NetLog::Source& source)
///    : socket_(net_log, source),
///      pending_accept_(false) {
//}

TCPServerSocket:: TCPServerSocket() : pending_accept_(false) {
}

TCPServerSocket::~TCPServerSocket() {
}

int TCPServerSocket::Listen(const IPEndPoint& address, int backlog) {
  int result = socket_.Open(address.GetFamily());
  if (result != OK)
    return result;

  result = socket_.SetDefaultOptionsForServer();
  if (result != OK) {
    socket_.Close();
    return result;
  }

  result = socket_.Bind(address);
  if (result != OK) {
    socket_.Close();
    return result;
  }

  result = socket_.Listen(backlog);
  if (result != OK) {
    socket_.Close();
    return result;
  }

  return OK;
}

int TCPServerSocket::GetLocalAddress(IPEndPoint* address) const {
  return socket_.GetLocalAddress(address);
}

int TCPServerSocket::Accept(std::unique_ptr<StreamSocket>* socket,
                            CompletionOnceCallback callback) {
  CR_DCHECK(socket);
  CR_DCHECK(!callback.is_null());

  if (pending_accept_) {
    CR_NOTREACHED();
    return ERR_UNEXPECTED;
  }

  // It is safe to use base::Unretained(this). |socket_| is owned by this class,
  // and the callback won't be run after |socket_| is destroyed.
  CompletionOnceCallback accept_callback =
      cr::BindOnce(&TCPServerSocket::OnAcceptCompleted, 
                       cr::Unretained(this), socket, std::move(callback));
  int result = socket_.Accept(&accepted_socket_, &accepted_address_,
                              std::move(accept_callback));
  if (result != ERR_IO_PENDING) {
    // |accept_callback| won't be called so we need to run
    // ConvertAcceptedSocket() ourselves in order to do the conversion from
    // |accepted_socket_| to |socket|.
    result = ConvertAcceptedSocket(result, socket);
  } else {
    pending_accept_ = true;
  }

  return result;
}

void TCPServerSocket::DetachFromThread() {
  socket_.DetachFromThread();
}

int TCPServerSocket::ConvertAcceptedSocket(
    int result,
    std::unique_ptr<StreamSocket>* output_accepted_socket) {
  // Make sure the TCPSocket object is destroyed in any case.
  std::unique_ptr<TCPSocket> temp_accepted_socket(std::move(accepted_socket_));
  if (result != OK)
    return result;

  output_accepted_socket->reset(
      new TCPClientSocket(std::move(temp_accepted_socket), accepted_address_));

  return OK;
}

void TCPServerSocket::OnAcceptCompleted(
    std::unique_ptr<StreamSocket>* output_accepted_socket,
    CompletionOnceCallback forward_callback,
    int result) {
  result = ConvertAcceptedSocket(result, output_accepted_socket);
  pending_accept_ = false;
  std::move(forward_callback).Run(result);
}

}  // namespace crnet