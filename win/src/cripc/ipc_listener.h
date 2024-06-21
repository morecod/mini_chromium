// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRIPC_IPC_LISTENER_H_
#define MINI_CHROMIUM_SRC_CRIPC_IPC_LISTENER_H_

#include <stdint.h>

#include "cripc/ipc_export.h"

namespace cripc {

class Message;

// Implemented by consumers of a Channel to receive messages.
class CRIPC_EXPORT Listener {
 public:
  // Called when a message is received.  Returns true iff the message was
  // handled.
  virtual bool OnMessageReceived(const Message& message) = 0;

  // Called when the channel is connected and we have received the internal
  // Hello message from the peer.
  virtual void OnChannelConnected(int32_t peer_pid) {}

  // Called when an error is detected that causes the channel to close.
  // This method is not called when a channel is closed normally.
  virtual void OnChannelError() {}

  // Called when a message's deserialization failed.
  virtual void OnBadMessageReceived(const Message& message) {}

 protected:
  virtual ~Listener() {}
};

}  // namespace cripc

#endif  // MINI_CHROMIUM_SRC_CRIPC_IPC_LISTENER_H_