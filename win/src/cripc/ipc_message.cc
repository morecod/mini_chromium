// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cripc/ipc_message.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "crbase/logging.h"
#include "crbase/atomic/atomic_sequence_num.h"

namespace cripc {

//------------------------------------------------------------------------------

Message::~Message() {
}

Message::Message() : crbase::Pickle(sizeof(Header)) {
  header()->routing = header()->type = 0;
  header()->flags = 0;
  Init();
}

Message::Message(int32_t routing_id, uint32_t type, PriorityValue priority)
    : crbase::Pickle(sizeof(Header)) {
  header()->routing = routing_id;
  header()->type = type;
  CR_DCHECK((priority & 0xffffff00) == 0);
  header()->flags = priority;
  Init();
}

Message::Message(const char* data, int data_len)
    : crbase::Pickle(data, data_len) {
  Init();
}

Message::Message(const Message& other) : crbase::Pickle(other) {
  Init();
  sender_pid_ = other.sender_pid_;
}

void Message::Init() {
  dispatch_error_ = false;
  sender_pid_ = crbase::kNullProcessId;
#ifdef ENABLE_CRIPC_MESSAGE_LOG
  received_time_ = 0;
  dont_log_ = false;
  log_data_ = NULL;
#endif
}

Message& Message::operator=(const Message& other) {
  *static_cast<crbase::Pickle*>(this) = other;
  sender_pid_ = other.sender_pid_;
  return *this;
}

void Message::SetHeaderValues(int32_t routing, uint32_t type, uint32_t flags) {
  // This should only be called when the message is already empty.
  CR_DCHECK(payload_size() == 0);

  header()->routing = routing;
  header()->type = type;
  header()->flags = flags;
}

#ifdef ENABLE_CRIPC_MESSAGE_LOG
void Message::set_sent_time(int64_t time) {
  CR_DCHECK((header()->flags & HAS_SENT_TIME_BIT) == 0);
  header()->flags |= HAS_SENT_TIME_BIT;
  WriteInt64(time);
}

int64_t Message::sent_time() const {
  if ((header()->flags & HAS_SENT_TIME_BIT) == 0)
    return 0;

  const char* data = end_of_payload();
  data -= sizeof(int64_t);
  return *(reinterpret_cast<const int64_t*>(data));
}

void Message::set_received_time(int64_t time) const {
  received_time_ = time;
}
#endif

Message::NextMessageInfo::NextMessageInfo()
    : message_size(0), message_found(false), pickle_end(nullptr),
      message_end(nullptr) {}
Message::NextMessageInfo::~NextMessageInfo() {}

// static
void Message::FindNext(const char* range_start,
                       const char* range_end,
                       NextMessageInfo* info) {
  CR_DCHECK(info);
  info->message_found = false;
  info->message_size = 0;

  size_t pickle_size = 0;
  if (!crbase::Pickle::PeekNext(sizeof(Header),
                                range_start, range_end, &pickle_size))
    return;

  bool have_entire_pickle =
      static_cast<size_t>(range_end - range_start) >= pickle_size;

  info->message_size = pickle_size;

  if (!have_entire_pickle)
    return;

  const char* pickle_end = range_start + pickle_size;

  info->message_end = pickle_end;

  info->pickle_end = pickle_end;
  info->message_found = true;
}

}  // namespace cripc
