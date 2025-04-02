// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crnet/server/stream_server.h"

#include <utility>

#include "crbase/functional/bind.h"
#include "crbase/compiler_specific.h"
#include "crbase/tracing/location.h"
#include "crbase/logging.h"
#include "crbase/stl_util.h"
#include "crbase/strings/string_number_conversions.h"
#include "crbase/strings/string_util.h"
#include "crbase/strings/stringprintf.h"
#include "crbase/threading/single_thread_task_runner.h"
#include "crbase/threading/thread_task_runner_handle.h"
#include "crnet/base/sys_byteorder.h"
#include "crnet/base/net_errors.h"
#include "crnet/server/stream_connection.h"
#include "crnet/socket/server_socket.h"
#include "crnet/socket/stream_socket.h"
#include "crnet/socket/tcp_server_socket.h"
#include "crbase/build_config.h"

namespace crnet {

StreamServer::StreamServer(std::unique_ptr<ServerSocket> server_socket,
                           StreamServer::Delegate* delegate)
    : server_socket_(std::move(server_socket)),
      delegate_(delegate),
      last_id_(0),
      weak_ptr_factory_(this) {
  CR_DCHECK(server_socket_);
  CR_DCHECK(delegate);
  // Start accepting connections in next run loop in case when delegate is not
  // ready to get callbacks.
  cr::ThreadTaskRunnerHandle::Get()->PostTask(
      CR_FROM_HERE,
      cr::BindOnce(&StreamServer::DoAcceptLoop, 
                   weak_ptr_factory_.GetWeakPtr()));
}

StreamServer::~StreamServer() {
  cr::STLDeleteContainerPairSecondPointers(
      id_to_connection_.begin(), id_to_connection_.end());
}

void StreamServer::SendData(uint32_t connection_id, const std::string& data) {
  StreamConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  bool writing_in_progress = !connection->write_buf()->IsEmpty();
  if (connection->write_buf()->Append(data) && !writing_in_progress)
    DoWriteLoop(connection);
}

void StreamServer::SendData(uint32_t connection_id, const char* data,
                            size_t data_len) {
  StreamConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  bool writing_in_progress = !connection->write_buf()->IsEmpty();
  if (connection->write_buf()->Append(data, data_len) && !writing_in_progress)
    DoWriteLoop(connection);
}

void StreamServer::Close(uint32_t connection_id) {
  StreamConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  id_to_connection_.erase(connection_id);
  delegate_->OnConnectionClose(connection_id);

  // The call stack might have callbacks which still have the pointer of
  // connection. Instead of referencing connection with ID all the time,
  // destroys the connection in next run loop to make sure any pending
  // callbacks in the call stack return.
  cr::ThreadTaskRunnerHandle::Get()->DeleteSoon(CR_FROM_HERE, connection);
}

int StreamServer::GetLocalAddress(IPEndPoint* address) {
  return server_socket_->GetLocalAddress(address);
}

void StreamServer::SetReceiveBufferSize(uint32_t connection_id, int32_t size) {
  StreamConnection* connection = FindConnection(connection_id);
  if (connection)
    connection->read_buf()->set_max_buffer_size(size);
}

void StreamServer::SetSendBufferSize(uint32_t connection_id, int32_t size) {
  StreamConnection* connection = FindConnection(connection_id);
  if (connection)
    connection->write_buf()->set_max_buffer_size(size);
}

void StreamServer::DoAcceptLoop() {
  int rv;
  do {
    rv = server_socket_->Accept(
        &accepted_socket_,
        cr::BindOnce(&StreamServer::OnAcceptCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
    if (rv == ERR_IO_PENDING)
      return;
    rv = HandleAcceptResult(rv);
  } while (rv == OK);
}

void StreamServer::OnAcceptCompleted(int rv) {
  if (HandleAcceptResult(rv) == OK)
    DoAcceptLoop();
}

int StreamServer::HandleAcceptResult(int rv) {
  if (rv < 0) {
    CR_LOG(ERROR) << "Accept error: rv=" << rv;
    return rv;
  }

  StreamConnection* connection =
      new StreamConnection(++last_id_, std::move(accepted_socket_));
  id_to_connection_[connection->id()] = connection;
  delegate_->OnConnectionCreate(connection->id());
  if (!HasClosedConnection(connection))
    DoReadLoop(connection);
  return OK;
}

void StreamServer::DoReadLoop(StreamConnection* connection) {
  int rv;
  do {
    StreamConnection::ReadIOBuffer* read_buf = connection->read_buf();
    // Increases read buffer size if necessary.
    if (read_buf->RemainingCapacity() == 0 && !read_buf->IncreaseCapacity()) {
      Close(connection->id());
      return;
    }

    rv = connection->socket()->Read(
        read_buf,
        read_buf->RemainingCapacity(),
        cr::BindOnce(&StreamServer::OnReadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), connection->id()));
    if (rv == ERR_IO_PENDING)
      return;
    rv = HandleReadResult(connection, rv);
  } while (rv == OK);
}

void StreamServer::OnReadCompleted(uint32_t connection_id, int rv) {
  StreamConnection* connection = FindConnection(connection_id);
  if (!connection)  // It might be closed right before by write error.
    return;

  if (HandleReadResult(connection, rv) == OK)
    DoReadLoop(connection);
}

int StreamServer::HandleReadResult(StreamConnection* connection, int rv) {
  if (rv <= 0) {
    Close(connection->id());
    return rv == 0 ? ERR_CONNECTION_CLOSED : rv;
  }

  StreamConnection::ReadIOBuffer* read_buf = connection->read_buf();
  read_buf->DidRead(rv);

  // Handles stream.
  while (read_buf->GetSize() > 0) {
    int handled = delegate_->OnConnectionData(
        connection->id(), read_buf->StartOfBuffer(),  read_buf->GetSize());
    if (handled == 0) {
      break;
    }
    else if (handled < 0) {
      // An error has occured. Close the connection.
      Close(connection->id());
      return ERR_CONNECTION_CLOSED;
    }

    read_buf->DidConsume(handled);
    if (HasClosedConnection(connection))
      return ERR_CONNECTION_CLOSED;
  }

  return OK;
}

void StreamServer::DoWriteLoop(StreamConnection* connection) {
  int rv = OK;
  StreamConnection::QueuedWriteIOBuffer* write_buf = connection->write_buf();
  while (rv == OK && write_buf->GetSizeToWrite() > 0) {
    rv = connection->socket()->Write(
        write_buf,
        write_buf->GetSizeToWrite(),
        cr::BindOnce(&StreamServer::OnWriteCompleted,
                     weak_ptr_factory_.GetWeakPtr(), connection->id()));
    if (rv == ERR_IO_PENDING || rv == OK)
      return;
    rv = HandleWriteResult(connection, rv);
  }
}

void StreamServer::OnWriteCompleted(uint32_t connection_id, int rv) {
  StreamConnection* connection = FindConnection(connection_id);
  if (!connection)  // It might be closed right before by read error.
    return;

  if (HandleWriteResult(connection, rv) == OK)
    DoWriteLoop(connection);
}

int StreamServer::HandleWriteResult(StreamConnection* connection, int rv) {
  if (rv < 0) {
    Close(connection->id());
    return rv;
  }

  connection->write_buf()->DidConsume(rv);
  return OK;
}

StreamConnection* StreamServer::FindConnection(uint32_t connection_id) {
  IdToConnectionMap::iterator it = id_to_connection_.find(connection_id);
  if (it == id_to_connection_.end())
    return NULL;
  return it->second;
}

// This is called after any delegate callbacks are called to check if Close()
// has been called during callback processing. Using the pointer of connection,
// |connection| is safe here because Close() deletes the connection in next run
// loop.
bool StreamServer::HasClosedConnection(StreamConnection* connection) {
  return FindConnection(connection->id()) != connection;
}

}  // namespace crnet