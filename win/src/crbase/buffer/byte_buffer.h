/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MINI_CHROMIUM_SRC_CRBASE_BUFFER_BYTE_BUFFER_H_
#define MINI_CHROMIUM_SRC_CRBASE_BUFFER_BYTE_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "crbase/byte_order.h"
#include "crbase/buffer/buffer.h"

// Reads/Writes from/to buffer using network byte order (big endian)
namespace cr {

template <class BufferClassT>
class ByteBufferWriterT {
 public:
  ByteBufferWriterT(const ByteBufferWriterT<BufferClassT>&) = delete;
  ByteBufferWriterT<BufferClassT>& operator=(
      const ByteBufferWriterT<BufferClassT>&) = delete;

  ByteBufferWriterT() { Construct(nullptr, kDefaultCapacity); }
  ByteBufferWriterT(const char* bytes, size_t len) { Construct(bytes, len); }

  const char* Data() const { return buffer_.data(); }
  size_t Length() const { return buffer_.size(); }
  size_t Capacity() const { return buffer_.capacity(); }

  // Write value to the buffer. Resizes the buffer when it is
  // neccessary.
  void WriteUInt8(uint8_t val) {
    WriteBytes(reinterpret_cast<const char*>(&val), 1);
  }

  void WriteUIntBE16(uint16_t val) {
    uint16_t v = cr::ByteSwapToBE16(val);
    WriteBytes(reinterpret_cast<const char*>(&v), 2);
  }

  void WriteUIntBE24(uint32_t val) {
    uint32_t v = cr::ByteSwapToBE32(val);
    char* start = reinterpret_cast<char*>(&v);
    ++start;
    WriteBytes(start, 3);
  }

  void WriteUIntBE32(uint32_t val) {
    uint32_t v = cr::ByteSwapToBE32(val);
    WriteBytes(reinterpret_cast<const char*>(&v), 4);
  }

  void WriteUIntBE64(uint64_t val) {
    uint64_t v = cr::ByteSwapToBE64(val);
    WriteBytes(reinterpret_cast<const char*>(&v), 8);
  }

  void WriteUIntLE16(uint16_t val) {
    uint16_t v = cr::ByteSwapToLE16(val);
    WriteBytes(reinterpret_cast<const char*>(&v), 2);
  }

  void WriteUIntLE24(uint32_t val) {
    uint32_t v = cr::ByteSwapToLE32(val);
    char* start = reinterpret_cast<char*>(&v);
    ++start;
    WriteBytes(start, 3);
  }

  void WriteUIntLE32(uint32_t val) {
    uint32_t v = cr::ByteSwapToLE32(val);
    WriteBytes(reinterpret_cast<const char*>(&v), 4);
  }

  void WriteUIntLE64(uint64_t val) {
    uint64_t v = cr::ByteSwapToLE64(val);
    WriteBytes(reinterpret_cast<const char*>(&v), 8);
  }

  // Serializes an unsigned varint in the format described by
  // https://developers.google.com/protocol-buffers/docs/encoding#varints
  // with the caveat that integers are 64-bit, not 128-bit.
  void WriteUVarint(uint64_t val) {
    while (val >= 0x80) {
      // Write 7 bits at a time, then set the msb to a continuation byte
      // (msb=1).
      char byte = static_cast<char>(val) | 0x80;
      WriteBytes(&byte, 1);
      val >>= 7;
    }
    char last_byte = static_cast<char>(val);
    WriteBytes(&last_byte, 1);
  }

  void WriteString(const std::string& val) {
    WriteBytes(val.c_str(), val.size());
  }

  void WriteBytes(const char* val, size_t len) { buffer_.AppendData(val, len); }

  // Reserves the given number of bytes and returns a char* that can be written
  // into. Useful for functions that require a char* buffer and not a
  // ByteBufferWriter.
  char* ReserveWriteBuffer(size_t len) {
    buffer_.SetSize(buffer_.size() + len);
    return buffer_.data();
  }

  // Resize the buffer to the specified |size|.
  void Resize(size_t size) { buffer_.SetSize(size); }

  // Clears the contents of the buffer. After this, Length() will be 0.
  void Clear() { buffer_.Clear(); }

 private:
  static constexpr size_t kDefaultCapacity = 4096;

  void Construct(const char* bytes, size_t size) {
    if (bytes) {
      buffer_.AppendData(bytes, size);
    } else {
      buffer_.EnsureCapacity(size);
    }
  }

  BufferClassT buffer_;
};

class ByteBufferWriter : public ByteBufferWriterT<BufferT<char>> {
 public:
  ByteBufferWriter(const ByteBufferWriter&) = delete;
  ByteBufferWriter& operator=(const ByteBufferWriter&) = delete;

  ByteBufferWriter();
  ByteBufferWriter(const char* bytes, size_t len);
};

// The ByteBufferReader references the passed data, i.e. the pointer must be
// valid during the lifetime of the reader.
class ByteBufferReader {
 public:
  ByteBufferReader(const ByteBufferReader&) = delete;
  ByteBufferReader& operator=(const ByteBufferReader&) = delete;

  ByteBufferReader(const char* bytes, size_t len);

  // Initializes buffer from a zero-terminated string.
  explicit ByteBufferReader(const char* bytes);

  explicit ByteBufferReader(const Buffer& buf);

  explicit ByteBufferReader(const ByteBufferWriter& buf);

  // Returns start of unprocessed data.
  const char* Data() const { return bytes_ + start_; }
  // Returns number of unprocessed bytes.
  size_t Length() const { return end_ - start_; }

  // Read a next value from the buffer. Return false if there isn't
  // enough data left for the specified type.
  bool ReadUInt8(uint8_t* val);
  bool ReadUIntBE16(uint16_t* val);
  bool ReadUIntBE24(uint32_t* val);
  bool ReadUIntBE32(uint32_t* val);
  bool ReadUIntBE64(uint64_t* val);

  bool ReadUIntLE16(uint16_t* val);
  bool ReadUIntLE24(uint32_t* val);
  bool ReadUIntLE32(uint32_t* val);
  bool ReadUIntLE64(uint64_t* val);

  bool ReadUVarint(uint64_t* val);
  bool ReadBytes(char* val, size_t len);

  // Appends next |len| bytes from the buffer to |val|. Returns false
  // if there is less than |len| bytes left.
  bool ReadString(std::string* val, size_t len);

  // Moves current position |size| bytes forward. Returns false if
  // there is less than |size| bytes left in the buffer. Consume doesn't
  // permanently remove data, so remembered read positions are still valid
  // after this call.
  bool Consume(size_t size);

 protected:
  void Construct(const char* bytes, size_t size);

  const char* bytes_;
  size_t size_;
  size_t start_;
  size_t end_;
};

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_BUFFER_BYTE_BUFFER_H_
