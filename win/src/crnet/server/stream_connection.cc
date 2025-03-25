// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crnet/server/stream_connection.h"

#include <utility>

#include "crbase/logging.h"
#include "crnet/socket/stream_socket.h"

namespace crnet {

StreamConnection::ReadIOBuffer::ReadIOBuffer()
    : base_(new GrowableIOBuffer()),
      max_buffer_size_(kDefaultMaxBufferSize) {
  SetCapacity(kInitialBufSize);
}

StreamConnection::ReadIOBuffer::~ReadIOBuffer() {
  data_ = NULL;  // base_ owns data_.
}

int StreamConnection::ReadIOBuffer::GetCapacity() const {
  return base_->capacity();
}

void StreamConnection::ReadIOBuffer::SetCapacity(int capacity) {
  CR_DCHECK_LE(GetSize(), capacity);
  base_->SetCapacity(capacity);
  data_ = base_->data();
}

bool StreamConnection::ReadIOBuffer::IncreaseCapacity() {
  if (GetCapacity() >= max_buffer_size_) {
    CR_LOG(ERROR) << "Too large read data is pending: capacity=" 
                  << GetCapacity()
                  << ", max_buffer_size=" << max_buffer_size_
                  << ", read=" << GetSize();
    return false;
  }

  int new_capacity = GetCapacity() * kCapacityIncreaseFactor;
  if (new_capacity > max_buffer_size_)
    new_capacity = max_buffer_size_;
  SetCapacity(new_capacity);
  return true;
}

char* StreamConnection::ReadIOBuffer::StartOfBuffer() const {
  return base_->StartOfBuffer();
}

int StreamConnection::ReadIOBuffer::GetSize() const {
  return base_->offset();
}

void StreamConnection::ReadIOBuffer::DidRead(int bytes) {
  CR_DCHECK_GE(RemainingCapacity(), bytes);
  base_->set_offset(base_->offset() + bytes);
  data_ = base_->data();
}

int StreamConnection::ReadIOBuffer::RemainingCapacity() const {
  return base_->RemainingCapacity();
}

void StreamConnection::ReadIOBuffer::DidConsume(int bytes) {
  int previous_size = GetSize();
  int unconsumed_size = previous_size - bytes;
  CR_DCHECK_LE(0, unconsumed_size);
  if (unconsumed_size > 0) {
    // Move unconsumed data to the start of buffer.
    memmove(StartOfBuffer(), StartOfBuffer() + bytes, unconsumed_size);
  }
  base_->set_offset(unconsumed_size);
  data_ = base_->data();

  // If capacity is too big, reduce it.
  if (GetCapacity() > kMinimumBufSize &&
      GetCapacity() > previous_size * kCapacityIncreaseFactor) {
    int new_capacity = GetCapacity() / kCapacityIncreaseFactor;
    if (new_capacity < kMinimumBufSize)
      new_capacity = kMinimumBufSize;
    // realloc() within GrowableIOBuffer::SetCapacity() could move data even
    // when size is reduced. If unconsumed_size == 0, i.e. no data exists in
    // the buffer, free internal buffer first to guarantee no data move.
    if (!unconsumed_size)
      base_->SetCapacity(0);
    SetCapacity(new_capacity);
  }
}

StreamConnection::QueuedWriteIOBuffer::QueuedWriteIOBuffer()
    : total_size_(0),
      max_buffer_size_(kDefaultMaxBufferSize) {
}

StreamConnection::QueuedWriteIOBuffer::~QueuedWriteIOBuffer() {
  data_ = NULL;  // pending_data_ owns data_.
}

bool StreamConnection::QueuedWriteIOBuffer::IsEmpty() const {
  return pending_data_.empty();
}

bool StreamConnection::QueuedWriteIOBuffer::Append(const std::string& data) {
  if (data.empty())
    return true;

  if (total_size_ + static_cast<int>(data.size()) > max_buffer_size_) {
    CR_LOG(ERROR) << "Too large write data is pending: size="
                  << total_size_ + data.size()
                  << ", max_buffer_size=" << max_buffer_size_;
    return false;
  }

  pending_data_.push(data);
  total_size_ += static_cast<int>(data.size());

  // If new data is the first pending data, updates data_.
  if (pending_data_.size() == 1)
    data_ = const_cast<char*>(pending_data_.front().data());
  return true;
}

void StreamConnection::QueuedWriteIOBuffer::DidConsume(int size) {
  CR_DCHECK_GE(total_size_, size);
  CR_DCHECK_GE(GetSizeToWrite(), size);
  if (size == 0)
    return;

  if (size < GetSizeToWrite()) {
    data_ += size;
  } else {  // size == GetSizeToWrite(). Updates data_ to next pending data.
    pending_data_.pop();
    data_ = IsEmpty() ? NULL : const_cast<char*>(pending_data_.front().data());
  }
  total_size_ -= size;
}

int StreamConnection::QueuedWriteIOBuffer::GetSizeToWrite() const {
  if (IsEmpty()) {
    CR_DCHECK_EQ(0, total_size_);
    return 0;
  }
  CR_DCHECK_GE(data_, pending_data_.front().data());
  int consumed = static_cast<int>(data_ - pending_data_.front().data());
  CR_DCHECK_GT(static_cast<int>(pending_data_.front().size()), consumed);
  return static_cast<int>(pending_data_.front().size()) - consumed;
}

StreamConnection::StreamConnection(int id, std::unique_ptr<StreamSocket> socket)
    : id_(id),
      socket_(std::move(socket)),
      read_buf_(new ReadIOBuffer()),
      write_buf_(new QueuedWriteIOBuffer()) {}

StreamConnection::~StreamConnection() {
}

}  // namespace crnet