// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cripc/ipc_sync_message.h"

#include <stdint.h>

#include <stack>

#include "crbase/build_config.h"
#include "crbase/lazy_instance.h"
#include "crbase/logging.h"
#include "crbase/atomic/atomic_sequence_num.h"
#include "crbase/synchronization/waitable_event.h"

namespace {

struct WaitableEventLazyInstanceTraits
    : public crbase::DefaultLazyInstanceTraits<crbase::WaitableEvent> {
  static crbase::WaitableEvent* New(void* instance) {
    // Use placement new to initialize our instance in our preallocated space.
    return new (instance) crbase::WaitableEvent(true, true);
  }
};

crbase::LazyInstance<crbase::WaitableEvent, WaitableEventLazyInstanceTraits>
    dummy_event = CR_LAZY_INSTANCE_INITIALIZER;

crbase::StaticAtomicSequenceNumber g_next_id;

}  // namespace

namespace cripc {

#define kSyncMessageHeaderSize 4

SyncMessage::SyncMessage(int32_t routing_id,
                         uint32_t type,
                         PriorityValue priority,
                         MessageReplyDeserializer* deserializer)
    : Message(routing_id, type, priority),
      deserializer_(deserializer),
      pump_messages_event_(NULL) {
  set_sync();
  set_unblock(true);

  // Add synchronous message data before the message payload.
  SyncHeader header;
  header.message_id = g_next_id.GetNext();
  WriteSyncHeader(this, header);
}

SyncMessage::~SyncMessage() {
}

MessageReplyDeserializer* SyncMessage::GetReplyDeserializer() {
  CR_DCHECK(deserializer_.get());
  return deserializer_.release();
}

void SyncMessage::EnableMessagePumping() {
  CR_DCHECK(!pump_messages_event_);
  set_pump_messages_event(dummy_event.Pointer());
}

bool SyncMessage::IsMessageReplyTo(const Message& msg, int request_id) {
  if (!msg.is_reply())
    return false;

  return GetMessageId(msg) == request_id;
}

crbase::PickleIterator SyncMessage::GetDataIterator(const Message* msg) {
  crbase::PickleIterator iter(*msg);
  if (!iter.SkipBytes(kSyncMessageHeaderSize))
    return crbase::PickleIterator();
  else
    return iter;
}

int SyncMessage::GetMessageId(const Message& msg) {
  if (!msg.is_sync() && !msg.is_reply())
    return 0;

  SyncHeader header;
  if (!ReadSyncHeader(msg, &header))
    return 0;

  return header.message_id;
}

Message* SyncMessage::GenerateReply(const Message* msg) {
  CR_DCHECK(msg->is_sync());

  Message* reply = new Message(msg->routing_id(), CRIPC_REPLY_ID,
                               msg->priority());
  reply->set_reply();

  SyncHeader header;

  // use the same message id, but this time reply bit is set
  header.message_id = GetMessageId(*msg);
  WriteSyncHeader(reply, header);

  return reply;
}

bool SyncMessage::ReadSyncHeader(const Message& msg, SyncHeader* header) {
  CR_DCHECK(msg.is_sync() || msg.is_reply());

  crbase::PickleIterator iter(msg);
  bool result = iter.ReadInt(&header->message_id);
  if (!result) {
    CR_NOTREACHED();
    return false;
  }

  return true;
}

bool SyncMessage::WriteSyncHeader(Message* msg, const SyncHeader& header) {
  CR_DCHECK(msg->is_sync() || msg->is_reply());
  CR_DCHECK(msg->payload_size() == 0);
  bool result = msg->WriteInt(header.message_id);
  if (!result) {
    CR_NOTREACHED();
    return false;
  }

  // Note: if you add anything here, you need to update kSyncMessageHeaderSize.
  CR_DCHECK(kSyncMessageHeaderSize == msg->payload_size());

  return true;
}


bool MessageReplyDeserializer::SerializeOutputParameters(const Message& msg) {
  return SerializeOutputParameters(msg, SyncMessage::GetDataIterator(&msg));
}

}  // namespace cripc