// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_CRIPC_IPC_SYNC_MESSAGE_H_
#define MINI_CHROMIUM_CRIPC_IPC_SYNC_MESSAGE_H_

#include <stdint.h>

#if defined(MINI_CHROMIUM_OS_WIN)
#include <windows.h>
#endif

#include <memory>
#include <string>

#include "crbase/build_config.h"
#include "cripc/ipc_message.h"

namespace cr {
class WaitableEvent;
}  // namespace cr

namespace cripc {

class MessageReplyDeserializer;

class CRIPC_EXPORT SyncMessage : public Message {
 public:
  SyncMessage(int32_t routing_id,
              uint32_t type,
              PriorityValue priority,
              MessageReplyDeserializer* deserializer);
  ~SyncMessage() override;

  // Call this to get a deserializer for the output parameters.
  // Note that this can only be called once, and the caller is responsible
  // for deleting the deserializer when they're done.
  MessageReplyDeserializer* GetReplyDeserializer();

  // If this message can cause the receiver to block while waiting for user
  // input (i.e. by calling MessageBox), then the caller needs to pump window
  // messages and dispatch asynchronous messages while waiting for the reply.
  // This call enables message pumping behavior while waiting for a reply to
  // this message.
  void EnableMessagePumping() {
    header()->flags |= PUMPING_MSGS_BIT;
  }

  // Indicates whether window messages should be pumped while waiting for a
  // reply to this message.
  bool ShouldPumpMessages() const {
    return (header()->flags & PUMPING_MSGS_BIT) != 0;
  }

  // Returns true if the message is a reply to the given request id.
  static bool IsMessageReplyTo(const Message& msg, int request_id);

  // Given a reply message, returns an iterator to the beginning of the data
  // (i.e. skips over the synchronous specific data).
  static cr::PickleIterator GetDataIterator(const Message* msg);

  // Given a synchronous message (or its reply), returns its id.
  static int GetMessageId(const Message& msg);

  // Generates a reply message to the given message.
  static Message* GenerateReply(const Message* msg);

 private:
  struct SyncHeader {
    // unique ID (unique per sender)
    int message_id;
  };

  static bool ReadSyncHeader(const Message& msg, SyncHeader* header);
  static bool WriteSyncHeader(Message* msg, const SyncHeader& header);

  std::unique_ptr<MessageReplyDeserializer> deserializer_;
};

// Used to deserialize parameters from a reply to a synchronous message
class CRIPC_EXPORT MessageReplyDeserializer {
 public:
  virtual ~MessageReplyDeserializer() {}
  bool SerializeOutputParameters(const Message& msg);
 private:
  // Derived classes need to implement this, using the given iterator (which
  // is skipped past the header for synchronous messages).
  virtual bool SerializeOutputParameters(const Message& msg,
                                         cr::PickleIterator iter) = 0;
};

// When sending a synchronous message, this structure contains an object
// that knows how to deserialize the response.
struct PendingSyncMsg {
  PendingSyncMsg(int id, MessageReplyDeserializer* d, cr::WaitableEvent* e)
      : id(id), deserializer(d), done_event(e), send_result(false) {}

  int id;
  MessageReplyDeserializer* deserializer;
  cr::WaitableEvent* done_event;
  bool send_result;
};

}  // namespace cripc

#endif  // MINI_CHROMIUM_CRIPC_IPC_SYNC_MESSAGE_H_