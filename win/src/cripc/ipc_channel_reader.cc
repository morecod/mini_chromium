// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cripc/ipc_channel_reader.h"

#include <stddef.h>

#include <algorithm>

#include "crbase/message_loop/message_loop.h"

#include "cripc/ipc_listener.h"
#include "cripc/ipc_logging.h"
#include "cripc/ipc_message.h"

namespace cripc {
namespace internal {

ChannelReader::ChannelReader(Listener* listener)
  : listener_(listener),
    max_input_buffer_size_(Channel::kMaximumReadBufferSize) {
  memset(input_buf_, 0, sizeof(input_buf_));
}

ChannelReader::~ChannelReader() {
}

ChannelReader::DispatchState ChannelReader::ProcessIncomingMessages() {
  while (true) {
    int bytes_read = 0;
    ReadState read_state = ReadData(input_buf_, Channel::kReadBufferSize,
                                    &bytes_read);
    if (read_state == READ_FAILED)
      return DISPATCH_ERROR;
    if (read_state == READ_PENDING)
      return DISPATCH_FINISHED;

    CR_DCHECK(bytes_read > 0);
    if (!TranslateInputData(input_buf_, bytes_read))
      return DISPATCH_ERROR;

    DispatchState state = DispatchMessages();
    if (state != DISPATCH_FINISHED)
      return state;
  }
}

ChannelReader::DispatchState ChannelReader::AsyncReadComplete(int bytes_read) {
  if (!TranslateInputData(input_buf_, bytes_read))
    return DISPATCH_ERROR;
  return DispatchMessages();
}

bool ChannelReader::IsInternalMessage(const Message& m) {
  return m.routing_id() == MSG_ROUTING_NONE &&
      m.type() >= Channel::CLOSE_FD_MESSAGE_TYPE &&
      m.type() <= Channel::HELLO_MESSAGE_TYPE;
}

bool ChannelReader::IsHelloMessage(const Message& m) {
  return m.routing_id() == MSG_ROUTING_NONE &&
      m.type() == Channel::HELLO_MESSAGE_TYPE;
}

void ChannelReader::CleanUp() {
}

void ChannelReader::DispatchMessage(Message* m) {
  EmitLogBeforeDispatch(*m);
  listener_->OnMessageReceived(*m);
  HandleDispatchError(*m);
}

bool ChannelReader::TranslateInputData(const char* input_data,
                                       int input_data_len) {
  const char* p;
  const char* end;

  // Possibly combine with the overflow buffer to make a larger buffer.
  if (input_overflow_buf_.empty()) {
    p = input_data;
    end = input_data + input_data_len;
  } else {
    if (!CheckMessageSize(input_overflow_buf_.size() + input_data_len))
      return false;
    input_overflow_buf_.append(input_data, input_data_len);
    p = input_overflow_buf_.data();
    end = p + input_overflow_buf_.size();
  }

  size_t next_message_size = 0;

  // Dispatch all complete messages in the data buffer.
  while (p < end) {
    Message::NextMessageInfo info;
    Message::FindNext(p, end, &info);
    if (info.message_found) {
      int pickle_len = static_cast<int>(info.pickle_end - p);
      Message translated_message(p, pickle_len);

      if (!HandleTranslatedMessage(&translated_message))
        return false;

      p = info.message_end;
    } else {
      // Last message is partial.
      next_message_size = info.message_size;
      if (!CheckMessageSize(next_message_size))
        return false;
      break;
    }
  }

  // Account for the case where last message's byte is in the next data chunk.
  size_t next_message_buffer_size = next_message_size ?
      next_message_size + Channel::kReadBufferSize - 1:
      0;

  // Save any partial data in the overflow buffer.
  if (p != input_overflow_buf_.data())
    input_overflow_buf_.assign(p, end - p);

  if (!input_overflow_buf_.empty()) {
    // We have something in the overflow buffer, which means that we will
    // append the next data chunk (instead of parsing it directly). So we
    // resize the buffer to fit the next message, to avoid repeatedly
    // growing the buffer as we receive all message' data chunks.
    if (next_message_buffer_size > input_overflow_buf_.capacity()) {
      input_overflow_buf_.reserve(next_message_buffer_size);
    }
  }

  // Trim the buffer if we can
  if (next_message_buffer_size < max_input_buffer_size_ &&
      input_overflow_buf_.size() < max_input_buffer_size_ &&
      input_overflow_buf_.capacity() > max_input_buffer_size_) {
    // std::string doesn't really have a method to shrink capacity to
    // a specific value, so we have to swap with another string.
    std::string trimmed_buf;
    trimmed_buf.reserve(max_input_buffer_size_);
    if (trimmed_buf.capacity() > max_input_buffer_size_) {
      // Since we don't control how much space reserve() actually reserves,
      // we have to go other way around and change the max size to avoid
      // getting into the outer if() again.
      max_input_buffer_size_ = trimmed_buf.capacity();
    }
    trimmed_buf.assign(input_overflow_buf_.data(),
                       input_overflow_buf_.size());
    input_overflow_buf_.swap(trimmed_buf);
  }

  if (input_overflow_buf_.empty() && !DidEmptyInputBuffers())
    return false;
  return true;
}

bool ChannelReader::HandleTranslatedMessage(
    Message* translated_message) {
  // Immediately handle internal messages.
  if (IsInternalMessage(*translated_message)) {
    EmitLogBeforeDispatch(*translated_message);
    HandleInternalMessage(*translated_message);
    HandleDispatchError(*translated_message);
    return true;
  }

  translated_message->set_sender_pid(GetSenderPID());

  return HandleExternalMessage(translated_message);
}

bool ChannelReader::HandleExternalMessage(Message* external_message) {
  // Make a deep copy of |external_message| to add to the queue.
  std::unique_ptr<Message> m(new Message(*external_message));
  queued_messages_.push_back(m.release());
  return true;
}

void ChannelReader::HandleDispatchError(const Message& message) {
  if (message.dispatch_error())
    listener_->OnBadMessageReceived(message);
}

void ChannelReader::EmitLogBeforeDispatch(const Message& message) {
#ifdef ENABLE_CRIPC_MESSAGE_LOG
  std::string name;
  Logging::GetInstance()->GetMessageText(message.type(), &name, &message, NULL);
#endif
}

ChannelReader::DispatchState ChannelReader::DispatchMessages() {
  while (!queued_messages_.empty()) {
    Message* m = queued_messages_.front();

    DispatchMessage(m);
    queued_messages_.erase(queued_messages_.begin());
  }
  return DISPATCH_FINISHED;
}

bool ChannelReader::CheckMessageSize(size_t size) {
  if (size <= Channel::kMaximumMessageSize) {
    return true;
  }
  input_overflow_buf_.clear();
  CR_LOG(ERROR) << "IPC message is too big: " << size;
  return false;
}

}  // namespace internal
}  // namespace cripc
