// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crnet/server/stream_connection.h"

#include <utility>

#include "crbase/logging.h"
#include "crnet/socket/tcp/stream_socket.h"

namespace crnet {

StreamConnection::ReadIOBuffer::ReadIOBuffer()
    : base_(cr::MakeRefCounted<GrowableIOBuffer>()) {
  SetCapacity(kInitialBufSize);
}

StreamConnection::ReadIOBuffer::~ReadIOBuffer() {
  // Avoid dangling ptr when `base_` is destroyed.
  ClearBytesView();
}

size_t StreamConnection::ReadIOBuffer::GetCapacity() const {
  return base_->capacity();
}

void StreamConnection::ReadIOBuffer::SetCapacity(size_t capacity) {
  CR_DCHECK_LE(base_->offset(), capacity);
  base_->SetCapacity(capacity);
  // Clear current span to avoid warning about dangling pointer, as
  // SetCapacity() may destroy the old buffer.
  ClearBytesView();
  base_->SetCapacity(capacity);
  SetBytesView(base_->bytes_view());
}

bool StreamConnection::ReadIOBuffer::IncreaseCapacity() {
  if (GetCapacity() >= max_buffer_size_) {
    CR_LOG(ERROR) << "Too large read data is pending: capacity=" 
                  << GetCapacity()
                  << ", max_buffer_size=" << max_buffer_size_
                  << ", read=" << base_->offset();
    return false;
  }

  size_t new_capacity = GetCapacity() * kCapacityIncreaseFactor;
  if (new_capacity > max_buffer_size_)
    new_capacity = max_buffer_size_;
  SetCapacity(new_capacity);
  return true;
}

cr::ArrayView<const uint8_t> 
StreamConnection::ReadIOBuffer::readable_bytes() const {
  return base_->bytes_view_before_offset();
}

void StreamConnection::ReadIOBuffer::DidRead(size_t bytes) {
  CR_DCHECK_GE(RemainingCapacity(), bytes);
  base_->set_offset(base_->offset() + bytes);
  SetBytesView(base_->bytes_view());
}

size_t StreamConnection::ReadIOBuffer::RemainingCapacity() const {
  return base_->RemainingCapacity();
}

void StreamConnection::ReadIOBuffer::DidConsume(size_t bytes) {
  size_t previous_size = base_->offset();
  CR_DCHECK_LE(bytes, previous_size);

  bytes = bytes > previous_size ? previous_size : bytes;
  size_t unconsumed_size = previous_size - bytes;
  if (unconsumed_size != 0) {
    // Move unconsumed data to the start of buffer.
    cr::BytesView buffer = base_->bytes_view_before_offset();
    memmove(buffer.begin(), buffer.subview(bytes).data(), unconsumed_size);
  }
  base_->set_offset(unconsumed_size);
  SetBytesView(base_->bytes_view());

  // If capacity is too big, reduce it.
  if (GetCapacity() > kMinimumBufSize &&
      GetCapacity() > previous_size * kCapacityIncreaseFactor) {
    size_t new_capacity = GetCapacity() / kCapacityIncreaseFactor;
    if (new_capacity < kMinimumBufSize)
      new_capacity = kMinimumBufSize;

    // this avoids the pointer to dangle until `SetCapacity` gets called.
    ClearBytesView();

    // realloc() within GrowableIOBuffer::SetCapacity() could move data even
    // when size is reduced. If unconsumed_size == 0, i.e. no data exists in
    // the buffer, free internal buffer first to guarantee no data move.
    if (unconsumed_size == 0)
      base_->SetCapacity(0);
    SetCapacity(new_capacity);
  }
}

StreamConnection::QueuedWriteIOBuffer::QueuedWriteIOBuffer() = default;

StreamConnection::QueuedWriteIOBuffer::~QueuedWriteIOBuffer() {
  // `pending_data_` owns the underlying data.
  ClearBytesView();
}

bool StreamConnection::QueuedWriteIOBuffer::IsEmpty() const {
  return pending_data_.empty();
}

bool StreamConnection::QueuedWriteIOBuffer::Append(const std::string& data) {
  if (data.empty())
    return true;

  if (total_size_ + data.size() > max_buffer_size_) {
    CR_LOG(ERROR) << "Too large write data is pending: size="
                  << total_size_ + data.size()
                  << ", max_buffer_size=" << max_buffer_size_;
    return false;
  }

  pending_data_.push(data);
  total_size_ += data.size();

  // If new data is the first pending data, updates data_.
  if (pending_data_.size() == 1) {
    SetBytesView(cr::MakeBytesView(pending_data_.front()));
  }
  return true;
}

bool StreamConnection::QueuedWriteIOBuffer::Append(const char* data, 
                                                   size_t data_len) {
  if (data == nullptr || !data_len)
    return true;

  if (total_size_ + data_len > max_buffer_size_) {
    CR_LOG(ERROR) << "Too large write data is pending: size="
                  << total_size_ + data_len
                  << ", max_buffer_size=" << max_buffer_size_;
    return false;
  }

  pending_data_.push(std::string(data, data_len));
  total_size_ += data_len;

  // If new data is the first pending data, updates data_.
  if (pending_data_.size() == 1) {
    SetBytesView(cr::MakeBytesView(pending_data_.front()));
  }
  return true;
}

void StreamConnection::QueuedWriteIOBuffer::DidConsume(size_t size) {
  CR_DCHECK_GE(total_size_, size);
  CR_DCHECK_GE(GetSizeToWrite(), size);
  if (size == 0)
    return;

  if (size < GetSizeToWrite()) {
    SetBytesView(bytes_view().subview(size));
  }
  else {
    // size == GetSizeToWrite(). Updates data_ to next pending data.
    ClearBytesView();
    pending_data_.pop();
    if (!IsEmpty()) {
      SetBytesView(cr::MakeBytesView(pending_data_.front()));
    }
  }
  total_size_ -= size;
}

size_t StreamConnection::QueuedWriteIOBuffer::GetSizeToWrite() const {
  if (IsEmpty()) {
    CR_DCHECK_EQ(0, total_size_);
    return 0;
  }
  // Return the unconsumed size of the current pending write.
  return size();
}

StreamConnection::StreamConnection(int id, std::unique_ptr<StreamSocket> socket)
    : id_(id),
      socket_(std::move(socket)),
      read_buf_(cr::MakeRefCounted<ReadIOBuffer>()),
      write_buf_(cr::MakeRefCounted<QueuedWriteIOBuffer>()) {}

StreamConnection::~StreamConnection() {
}

}  // namespace crnet