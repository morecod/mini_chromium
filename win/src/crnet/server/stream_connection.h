// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRNET_SERVER_STREAM_CONNECTION_H_
#define MINI_CHROMIUM_SRC_CRNET_SERVER_STREAM_CONNECTION_H_

#include <queue>
#include <string>
#include <memory>

#include "crbase/macros.h"
#include "crbase/memory/ref_counted.h"
#include "crnet/base/io_buffer.h"

namespace crnet {

class StreamSocket;

// A container which has all information of an stream connection. It includes
// id, underlying socket, and pending read/write data.
class StreamConnection {
 public:
  // IOBuffer for data read.  It's a wrapper around GrowableIOBuffer, with more
  // functions for buffer management.  It moves unconsumed data to the start of
  // buffer.
  class ReadIOBuffer : public IOBuffer {
   public:
    static const size_t kInitialBufSize = 1024;
    static const size_t kMinimumBufSize = 128;
    static const size_t kCapacityIncreaseFactor = 2;
    static const size_t kDefaultMaxBufferSize = 1 * 1024 * 1024;  // 1 Mbytes.
    
    ReadIOBuffer(const ReadIOBuffer&) = delete;
    ReadIOBuffer& operator=(const ReadIOBuffer&) = delete;

    ReadIOBuffer();

    // Capacity.
    size_t GetCapacity() const;
    void SetCapacity(size_t capacity);
    // Increases capacity and returns true if capacity is not beyond the limit.
    bool IncreaseCapacity();

    // Returns a span containing bytes that have been written to, and thus are
    // available to be read from.
    cr::ArrayView<const uint8_t> readable_bytes() const;

    // More read data was appended. Increases the size of span_before_offset().
    void DidRead(size_t bytes);
    // Capacity for which more read data can be appended. Decreases size of
    // span_before_offset().
    size_t RemainingCapacity() const;

    // Removes consumed data and moves unconsumed data to the start of buffer.
    void DidConsume(size_t bytes);

    // Limit of how much internal capacity can increase.
    size_t max_buffer_size() const { return max_buffer_size_; }
    void set_max_buffer_size(size_t max_buffer_size) {
      max_buffer_size_ = max_buffer_size;
    }

   private:
    ~ReadIOBuffer() override;

    cr::scoped_refptr<GrowableIOBuffer> base_;
    size_t max_buffer_size_ = kDefaultMaxBufferSize;
  };

  // IOBuffer of pending data to write which has a queue of pending data. Each
  // pending data is stored in std::string.  data() is the data of first
  // std::string stored.
  class QueuedWriteIOBuffer : public IOBuffer {
   public:
    static const int kDefaultMaxBufferSize = 1 * 1024 * 1024;  // 1 Mbytes.

    QueuedWriteIOBuffer(const QueuedWriteIOBuffer&) = delete;
    QueuedWriteIOBuffer& operator=(const QueuedWriteIOBuffer&) = delete;

    QueuedWriteIOBuffer();

    // Whether or not pending data exists.
    bool IsEmpty() const;

    // Appends new pending data and returns true if total size doesn't exceed
    // the limit, |total_size_limit_|.  It would change data() if new data is
    // the first pending data.
    bool Append(const std::string& data);
    bool Append(const char* data, size_t len);

    // Consumes data and changes data() accordingly.  It cannot be more than
    // GetSizeToWrite().
    void DidConsume(size_t size);

    // Gets size of data to write this time. It is NOT total data size.
    size_t GetSizeToWrite() const;

    // Total size of all pending data.
    size_t total_size() const { return total_size_; }

    // Limit of how much data can be pending.
    size_t max_buffer_size() const { return max_buffer_size_; }
    void set_max_buffer_size(size_t max_buffer_size) {
      max_buffer_size_ = max_buffer_size;
    }

   private:
    ~QueuedWriteIOBuffer() override;

    std::queue<std::string> pending_data_;
    size_t total_size_ = 0;
    size_t max_buffer_size_ = kDefaultMaxBufferSize;
  };

  StreamConnection(const StreamConnection&) = delete;
  StreamConnection& operator=(const StreamConnection&) = delete;

  StreamConnection(int id, std::unique_ptr<StreamSocket> socket);
  ~StreamConnection();

  uint32_t id() const { return id_; }
  StreamSocket* socket() const { return socket_.get(); }
  ReadIOBuffer* read_buf() const { return read_buf_.get(); }
  QueuedWriteIOBuffer* write_buf() const { return write_buf_.get(); }

 private:
  const uint32_t id_;
  const std::unique_ptr<StreamSocket> socket_;
  const cr::scoped_refptr<ReadIOBuffer> read_buf_;
  const cr::scoped_refptr<QueuedWriteIOBuffer> write_buf_;
};

}  // namespace crnet

#endif  // MINI_CHROMIUM_SRC_CRNET_SERVER_STREAM_CONNECTION_H_