// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crnet/socket/tcp/tcp_client_socket.h"

#include <utility>

#include "crbase/functional/callback_helpers.h"
#include "crbase/logging.h"
///#include "crbase/metrics/histogram_macros.h"
///#include "crbase/profiler/scoped_tracker.h"
#include "crbase/time/time.h"
#include "crnet/base/io_buffer.h"
#include "crnet/base/ip_endpoint.h"
#include "crnet/base/net_errors.h"

namespace crnet {

///TCPClientSocket::TCPClientSocket(const AddressList& addresses,
///                                 net::NetLog* net_log,
///                                 const net::NetLog::Source& source)
///    : socket_(new TCPSocket(net_log, source)),
///      addresses_(addresses),
///      current_address_index_(-1),
///      next_connect_state_(CONNECT_STATE_NONE),
///      previously_disconnected_(false),
///      total_received_bytes_(0) {}
TCPClientSocket::TCPClientSocket(const AddressList& addresses)
    : socket_(new TCPSocket()),
      addresses_(addresses),
      current_address_index_(-1),
      next_connect_state_(CONNECT_STATE_NONE),
      previously_disconnected_(false),
      total_received_bytes_(0) {}

TCPClientSocket::TCPClientSocket(std::unique_ptr<TCPSocket> connected_socket,
                                 const IPEndPoint& peer_address)
    : socket_(std::move(connected_socket)),
      addresses_(AddressList(peer_address)),
      current_address_index_(0),
      next_connect_state_(CONNECT_STATE_NONE),
      previously_disconnected_(false),
      total_received_bytes_(0) {
  CR_DCHECK(socket_);

  socket_->SetDefaultOptionsForClient();
  use_history_.set_was_ever_connected();
}

TCPClientSocket::~TCPClientSocket() {
  Disconnect();
}

int TCPClientSocket::Bind(const IPEndPoint& address) {
  if (current_address_index_ >= 0 || bind_address_) {
    // Cannot bind the socket if we are already connected or connecting.
    CR_NOTREACHED();
    return ERR_UNEXPECTED;
  }

  int result = OK;
  if (!socket_->IsValid()) {
    result = OpenSocket(address.GetFamily());
    if (result != OK)
      return result;
  }

  result = socket_->Bind(address);
  if (result != OK)
    return result;

  bind_address_.reset(new IPEndPoint(address));
  return OK;
}

int TCPClientSocket::Connect(CompletionOnceCallback callback) {
  CR_DCHECK(!callback.is_null());

  // If connecting or already connected, then just return OK.
  if (socket_->IsValid() && current_address_index_ >= 0)
    return OK;

  ///socket_->StartLoggingMultipleConnectAttempts(addresses_);

  // We will try to connect to each address in addresses_. Start with the
  // first one in the list.
  next_connect_state_ = CONNECT_STATE_CONNECT;
  current_address_index_ = 0;

  int rv = DoConnectLoop(OK);
  if (rv == ERR_IO_PENDING) {
    connect_callback_ = std::move(callback);
  } /* else {
    socket_->EndLoggingMultipleConnectAttempts(rv);
  } */
  ///
  return rv;
}

int TCPClientSocket::DoConnectLoop(int result) {
  CR_DCHECK_NE(next_connect_state_, CONNECT_STATE_NONE);

  int rv = result;
  do {
    ConnectState state = next_connect_state_;
    next_connect_state_ = CONNECT_STATE_NONE;
    switch (state) {
      case CONNECT_STATE_CONNECT:
        CR_DCHECK_EQ(OK, rv);
        rv = DoConnect();
        break;
      case CONNECT_STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      default:
        CR_NOTREACHED() << "bad state " << state;
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_connect_state_ != CONNECT_STATE_NONE);

  return rv;
}

int TCPClientSocket::DoConnect() {
  CR_DCHECK_GE(current_address_index_, 0);
  CR_DCHECK_LT(current_address_index_, static_cast<int>(addresses_.size()));

  const IPEndPoint& endpoint = addresses_[current_address_index_];

  {
    // TODO(ricea): Remove ScopedTracker below once crbug.com/436634 is fixed.
    ///tracked_objects::ScopedTracker tracking_profile(
    ///    FROM_HERE_WITH_EXPLICIT_FUNCTION("436634 TCPClientSocket::DoConnect"));

    if (previously_disconnected_) {
      use_history_.Reset();
      connection_attempts_.clear();
      previously_disconnected_ = false;
    }

    next_connect_state_ = CONNECT_STATE_CONNECT_COMPLETE;

    if (socket_->IsValid()) {
      CR_DCHECK(bind_address_);
    } else {
      int result = OpenSocket(endpoint.GetFamily());
      if (result != OK)
        return result;

      if (bind_address_) {
        result = socket_->Bind(*bind_address_);
        if (result != OK) {
          socket_->Close();
          return result;
        }
      }
    }
  }

  // |socket_| is owned by this class and the callback won't be run once
  // |socket_| is gone. Therefore, it is safe to use base::Unretained() here.
  return socket_->Connect(endpoint,
                          cr::BindOnce(&TCPClientSocket::DidCompleteConnect,
                                           cr::Unretained(this)));
}

int TCPClientSocket::DoConnectComplete(int result) {
  if (result == OK) {
    use_history_.set_was_ever_connected();
    return OK;  // Done!
  }

  connection_attempts_.push_back(
      ConnectionAttempt(addresses_[current_address_index_], result));

  // Close whatever partially connected socket we currently have.
  DoDisconnect();

  // Try to fall back to the next address in the list.
  if (current_address_index_ + 1 < static_cast<int>(addresses_.size())) {
    next_connect_state_ = CONNECT_STATE_CONNECT;
    ++current_address_index_;
    return OK;
  }

  // Otherwise there is nothing to fall back to, so give up.
  return result;
}

void TCPClientSocket::Disconnect() {
  DoDisconnect();
  current_address_index_ = -1;
  bind_address_.reset();
}

void TCPClientSocket::DoDisconnect() {
  total_received_bytes_ = 0;
  ///EmitTCPMetricsHistogramsOnDisconnect();
  // If connecting or already connected, record that the socket has been
  // disconnected.
  previously_disconnected_ = socket_->IsValid() && current_address_index_ >= 0;
  socket_->Close();
}

bool TCPClientSocket::IsConnected() const {
  return socket_->IsConnected();
}

bool TCPClientSocket::IsConnectedAndIdle() const {
  return socket_->IsConnectedAndIdle();
}

int TCPClientSocket::GetPeerAddress(IPEndPoint* address) const {
  return socket_->GetPeerAddress(address);
}

int TCPClientSocket::GetLocalAddress(IPEndPoint* address) const {
  CR_DCHECK(address);

  if (!socket_->IsValid()) {
    if (bind_address_) {
      *address = *bind_address_;
      return OK;
    }
    return ERR_SOCKET_NOT_CONNECTED;
  }

  return socket_->GetLocalAddress(address);
}

///const BoundNetLog& TCPClientSocket::NetLog() const {
///  return socket_->net_log();
///}

void TCPClientSocket::SetSubresourceSpeculation() {
  use_history_.set_subresource_speculation();
}

void TCPClientSocket::SetOmniboxSpeculation() {
  use_history_.set_omnibox_speculation();
}

bool TCPClientSocket::WasEverUsed() const {
  return use_history_.was_used_to_convey_data();
}

bool TCPClientSocket::WasNpnNegotiated() const {
  return false;
}

///NextProto TCPClientSocket::GetNegotiatedProtocol() const {
///  return kProtoUnknown;
///}

///bool TCPClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
///  return false;
///}

int TCPClientSocket::Read(IOBuffer* buf,
                          int buf_len,
                          CompletionOnceCallback callback) {
  CR_DCHECK(!callback.is_null());

  // |socket_| is owned by this class and the callback won't be run once
  // |socket_| is gone. Therefore, it is safe to use base::Unretained() here.
  CompletionOnceCallback read_callback = cr::BindOnce(
      &TCPClientSocket::DidCompleteRead, cr::Unretained(this), 
      std::move(callback));
  int result = socket_->Read(buf, buf_len, std::move(read_callback));
  if (result > 0) {
    use_history_.set_was_used_to_convey_data();
    total_received_bytes_ += result;
  }

  return result;
}

int TCPClientSocket::Write(IOBuffer* buf,
                           int buf_len,
                           CompletionOnceCallback callback) {
  CR_DCHECK(!callback.is_null());

  // |socket_| is owned by this class and the callback won't be run once
  // |socket_| is gone. Therefore, it is safe to use base::Unretained() here.
  CompletionOnceCallback write_callback = cr::BindOnce(
      &TCPClientSocket::DidCompleteWrite, cr::Unretained(this), 
      std::move(callback));
  int result = socket_->Write(buf, buf_len, std::move(write_callback));
  if (result > 0)
    use_history_.set_was_used_to_convey_data();

  return result;
}

int TCPClientSocket::SetReceiveBufferSize(int32_t size) {
  return socket_->SetReceiveBufferSize(size);
}

int TCPClientSocket::SetSendBufferSize(int32_t size) {
    return socket_->SetSendBufferSize(size);
}

bool TCPClientSocket::SetKeepAlive(bool enable, int delay) {
  return socket_->SetKeepAlive(enable, delay);
}

bool TCPClientSocket::SetNoDelay(bool no_delay) {
  return socket_->SetNoDelay(no_delay);
}

void TCPClientSocket::GetConnectionAttempts(ConnectionAttempts* out) const {
  *out = connection_attempts_;
}

void TCPClientSocket::ClearConnectionAttempts() {
  connection_attempts_.clear();
}

void TCPClientSocket::AddConnectionAttempts(
    const ConnectionAttempts& attempts) {
  connection_attempts_.insert(connection_attempts_.begin(), attempts.begin(),
                              attempts.end());
}

int64_t TCPClientSocket::GetTotalReceivedBytes() const {
  return total_received_bytes_;
}

void TCPClientSocket::DidCompleteConnect(int result) {
  CR_DCHECK_EQ(next_connect_state_, CONNECT_STATE_CONNECT_COMPLETE);
  CR_DCHECK_NE(result, ERR_IO_PENDING);
  CR_DCHECK(!connect_callback_.is_null());

  result = DoConnectLoop(result);
  if (result != ERR_IO_PENDING) {
    ///socket_->EndLoggingMultipleConnectAttempts(result);
    cr::ResetAndReturn(&connect_callback_).Run(result);
  }
}

void TCPClientSocket::DidCompleteRead(CompletionOnceCallback callback,
                                      int result) {
  if (result > 0)
    total_received_bytes_ += result;

  DidCompleteReadWrite(std::move(callback), result);
}

void TCPClientSocket::DidCompleteWrite(CompletionOnceCallback callback,
                                       int result) {
  DidCompleteReadWrite(std::move(callback), result);
}

void TCPClientSocket::DidCompleteReadWrite(CompletionOnceCallback callback,
                                           int result) {
  if (result > 0)
    use_history_.set_was_used_to_convey_data();

  // TODO(pkasting): Remove ScopedTracker below once crbug.com/462780 is fixed.
  ///tracked_objects::ScopedTracker tracking_profile(
  ///    FROM_HERE_WITH_EXPLICIT_FUNCTION(
  ///        "462780 TCPClientSocket::DidCompleteReadWrite"));
  std::move(callback).Run(result);
}

int TCPClientSocket::OpenSocket(AddressFamily family) {
  CR_DCHECK(!socket_->IsValid());

  int result = socket_->Open(family);
  if (result != OK)
    return result;

  socket_->SetDefaultOptionsForClient();

  return OK;
}

///void TCPClientSocket::EmitTCPMetricsHistogramsOnDisconnect() {
///  base::TimeDelta rtt;
///  if (socket_->GetEstimatedRoundTripTime(&rtt)) {
///    UMA_HISTOGRAM_CUSTOM_TIMES("Net.TcpRtt.AtDisconnect", rtt,
///                               base::TimeDelta::FromMilliseconds(1),
///                               base::TimeDelta::FromMinutes(10), 100);
///  }
///}

}  // namespace crnet