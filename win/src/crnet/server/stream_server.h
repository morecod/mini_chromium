// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRNET_SERVER_STREAM_SERVER_H_
#define MINI_CHROMIUM_SRC_CRNET_SERVER_STREAM_SERVER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <memory>

#include "crbase/macros.h"
#include "crbase/memory/weak_ptr.h"
#include "crnet/server/stream_connection.h"

namespace crnet {

class IPEndPoint;
class ServerSocket;
class StreamSocket;

class StreamServer {
 public:
  // Delegate to handle stream events. Beware that it is not safe to
  // destroy the StreamServer in any of these callbacks.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnConnectionCreate(uint32_t connection_id) = 0;

    // Returns the number of bytes handled. if an error occurs then return a
    // net error code(defines in crnet/base/net_errors.h i.g:ERROR_FAILED)
    virtual int OnConnectionData(uint32_t connection_id, const char* data,
                                 int data_len) = 0;
    virtual void OnConnectionClose(uint32_t connection_id) = 0;
  };

  StreamServer(const StreamServer&) = delete;
  StreamServer& operator=(const StreamServer&) = delete;

  // Instantiates a stream server with |server_socket| which already started
  // listening, but not accepting.  This constructor schedules accepting
  // connections asynchronously in case when |delegate| is not ready to get
  // callbacks yet.
  StreamServer(std::unique_ptr<ServerSocket> server_socket,
               StreamServer::Delegate* delegate);
  ~StreamServer();

  // Sends the provided data directly to the given connection. No validation is
  // performed that data constitutes a valid Stream response. A valid Stream
  // response may be split across multiple calls to SendData.
  void SendData(uint32_t connection_id, const std::string& data);
  void SendData(uint32_t connection_id, const char* data, size_t data_len);

  void Close(uint32_t connection_id);

  void SetReceiveBufferSize(uint32_t connection_id, int32_t size);
  void SetSendBufferSize(uint32_t connection_id, int32_t size);

  // Copies the local address to |address|. Returns a network error code.
  int GetLocalAddress(IPEndPoint* address);

 private:

  typedef std::map<uint32_t, StreamConnection*> IdToConnectionMap;

  void DoAcceptLoop();
  void OnAcceptCompleted(int rv);
  int HandleAcceptResult(int rv);

  void DoReadLoop(StreamConnection* connection);
  void OnReadCompleted(uint32_t connection_id, int rv);
  int HandleReadResult(StreamConnection* connection, int rv);

  void DoWriteLoop(StreamConnection* connection);
  void OnWriteCompleted(uint32_t connection_id, int rv);
  int HandleWriteResult(StreamConnection* connection, int rv);

  StreamConnection* FindConnection(uint32_t connection_id);

  // Whether or not Close() has been called during delegate callback processing.
  bool HasClosedConnection(StreamConnection* connection);

  const std::unique_ptr<ServerSocket> server_socket_;

  // currently accepted socket from client.
  std::unique_ptr<StreamSocket> accepted_socket_;

  StreamServer::Delegate* const delegate_;

  uint32_t last_id_;
  IdToConnectionMap id_to_connection_;

  cr::WeakPtrFactory<StreamServer> weak_ptr_factory_;
};

}  // namespace crnet

#endif // MINI_CHROMIUM_SRC_CRNET_SERVER_STREAM_SERVER_H_