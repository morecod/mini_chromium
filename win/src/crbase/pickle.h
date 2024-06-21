// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_PICKLE_H_
#define MINI_CHROMIUM_SRC_CRBASE_PICKLE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "crbase/base_export.h"
#include "crbase/strings/string16.h"
#include "crbase/strings/string_piece.h"

namespace crbase {

class Pickle;

// PickleIterator reads data from a Pickle. The Pickle object must remain valid
// while the PickleIterator object is in use.
class CRBASE_EXPORT PickleIterator {
 public:
  PickleIterator() : payload_(NULL), read_index_(0), end_index_(0) {}
  explicit PickleIterator(const Pickle& pickle);

  // Methods for reading the payload of the Pickle. To read from the start of
  // the Pickle, create a PickleIterator from a Pickle. If successful, these
  // methods return true. Otherwise, false is returned to indicate that the
  // result could not be extracted. It is not possible to read from the iterator
  // after that.
  bool ReadBool(bool* result);
  bool ReadInt(int* result);
  bool ReadLong(long* result);
  bool ReadUInt16(uint16_t* result);
  bool ReadUInt32(uint32_t* result);
  bool ReadInt64(int64_t* result);
  bool ReadUInt64(uint64_t* result);
  bool ReadSizeT(size_t* result);
  bool ReadFloat(float* result);
  bool ReadDouble(double* result);
  bool ReadString(std::string* result);
  // The StringPiece data will only be valid for the lifetime of the message.
  bool ReadStringPiece(StringPiece* result);
  bool ReadString16(string16* result);
  // The StringPiece16 data will only be valid for the lifetime of the message.
  bool ReadStringPiece16(StringPiece16* result);

  // A pointer to the data will be placed in |*data|, and the length will be
  // placed in |*length|. The pointer placed into |*data| points into the
  // message's buffer so it will be scoped to the lifetime of the message (or
  // until the message data is mutated). Do not keep the pointer around!
  bool ReadData(const char** data, int* length);

  // A pointer to the data will be placed in |*data|. The caller specifies the
  // number of bytes to read, and ReadBytes will validate this length. The
  // pointer placed into |*data| points into the message's buffer so it will be
  // scoped to the lifetime of the message (or until the message data is
  // mutated). Do not keep the pointer around!
  bool ReadBytes(const char** data, int length);

  // A safer version of ReadInt() that checks for the result not being negative.
  // Use it for reading the object sizes.
  bool ReadLength(int* result) {
    return ReadInt(result) && *result >= 0;
  }

  // Skips bytes in the read buffer and returns true if there are at least
  // num_bytes available. Otherwise, does nothing and returns false.
  bool SkipBytes(int num_bytes) {
    return !!GetReadPointerAndAdvance(num_bytes);
  }

 private:
  // Read Type from Pickle.
  template <typename Type>
  bool ReadBuiltinType(Type* result);

  // Advance read_index_ but do not allow it to exceed end_index_.
  // Keeps read_index_ aligned.
  void Advance(size_t size);

  // Get read pointer for Type and advance read pointer.
  template<typename Type>
  const char* GetReadPointerAndAdvance();

  // Get read pointer for |num_bytes| and advance read pointer. This method
  // checks num_bytes for negativity and wrapping.
  const char* GetReadPointerAndAdvance(int num_bytes);

  // Get read pointer for (num_elements * size_element) bytes and advance read
  // pointer. This method checks for int overflow, negativity and wrapping.
  const char* GetReadPointerAndAdvance(int num_elements,
                                       size_t size_element);

  const char* payload_;  // Start of our pickle's payload.
  size_t read_index_;  // Offset of the next readable byte in payload.
  size_t end_index_;  // Payload size.
};

// This class provides facilities for basic binary value packing and unpacking.
//
// The Pickle class supports appending primitive values (ints, strings, etc.)
// to a pickle instance.  The Pickle instance grows its internal memory buffer
// dynamically to hold the sequence of primitive values.   The internal memory
// buffer is exposed as the "data" of the Pickle.  This "data" can be passed
// to a Pickle object to initialize it for reading.
//
// When reading from a Pickle object, it is important for the consumer to know
// what value types to read and in what order to read them as the Pickle does
// not keep track of the type of data written to it.
//
// The Pickle's data has a header which contains the size of the Pickle's
// payload.  It can optionally support additional space in the header.  That
// space is controlled by the header_size parameter passed to the Pickle
// constructor.
//
class CRBASE_EXPORT Pickle {
 public:
  // Initialize a Pickle object using the default header size.
  Pickle();

  // Initialize a Pickle object with the specified header size in bytes, which
  // must be greater-than-or-equal-to sizeof(Pickle::Header).  The header size
  // will be rounded up to ensure that the header size is 32bit-aligned.
  explicit Pickle(int header_size);

  // Initializes a Pickle from a const block of data.  The data is not copied;
  // instead the data is merely referenced by this Pickle.  Only const methods
  // should be used on the Pickle when initialized this way.  The header
  // padding size is deduced from the data length.
  Pickle(const char* data, int data_len);

  // Initializes a Pickle as a deep copy of another Pickle.
  Pickle(const Pickle& other);

  // Note: There are no virtual methods in this class.  This destructor is
  // virtual as an element of defensive coding.  Other classes have derived from
  // this class, and there is a *chance* that they will cast into this base
  // class before destruction.  At least one such class does have a virtual
  // destructor, suggesting at least some need to call more derived destructors.
  virtual ~Pickle();

  // Performs a deep copy.
  Pickle& operator=(const Pickle& other);

  // Returns the number of bytes written in the Pickle, including the header.
  size_t size() const { return header_size_ + header_->payload_size; }

  // Returns the data for this Pickle.
  const void* data() const { return header_; }

  // Returns the effective memory capacity of this Pickle, that is, the total
  // number of bytes currently dynamically allocated or 0 in the case of a
  // read-only Pickle. This should be used only for diagnostic / profiling
  // purposes.
  size_t GetTotalAllocatedSize() const;

  // Methods for adding to the payload of the Pickle.  These values are
  // appended to the end of the Pickle's payload.  When reading values from a
  // Pickle, it is important to read them in the order in which they were added
  // to the Pickle.

  bool WriteBool(bool value) {
    return WriteInt(value ? 1 : 0);
  }
  bool WriteInt(int value) {
    return WritePOD(value);
  }
  // WARNING: DO NOT USE THIS METHOD IF PICKLES ARE PERSISTED IN ANY WAY.
  // It will write whatever a "long" is on this architecture. On 32-bit
  // platforms, it is 32 bits. On 64-bit platforms, it is 64 bits. If persisted
  // pickles are still around after upgrading to 64-bit, or if they are copied
  // between dissimilar systems, YOUR PICKLES WILL HAVE GONE BAD.
  bool WriteLongUsingDangerousNonPortableLessPersistableForm(long value) {
    return WritePOD(value);
  }
  bool WriteUInt16(uint16_t value) { return WritePOD(value); }
  bool WriteUInt32(uint32_t value) { return WritePOD(value); }
  bool WriteInt64(int64_t value) { return WritePOD(value); }
  bool WriteUInt64(uint64_t value) { return WritePOD(value); }
  bool WriteSizeT(size_t value) {
    // Always write size_t as a 64-bit value to ensure compatibility between
    // 32-bit and 64-bit processes.
    return WritePOD(static_cast<uint64_t>(value));
  }
  bool WriteFloat(float value) {
    return WritePOD(value);
  }
  bool WriteDouble(double value) {
    return WritePOD(value);
  }
  bool WriteString(const StringPiece& value);
  bool WriteString16(const StringPiece16& value);
  // "Data" is a blob with a length. When you read it out you will be given the
  // length. See also WriteBytes.
  bool WriteData(const char* data, int length);
  // "Bytes" is a blob with no length. The caller must specify the length both
  // when reading and writing. It is normally used to serialize PoD types of a
  // known size. See also WriteData.
  bool WriteBytes(const void* data, int length);

  // Reserves space for upcoming writes when multiple writes will be made and
  // their sizes are computed in advance. It can be significantly faster to call
  // Reserve() before calling WriteFoo() multiple times.
  void Reserve(size_t additional_capacity);

  // Payload follows after allocation of Header (header size is customizable).
  struct Header {
    uint32_t payload_size;  // Specifies the size of the payload.
  };

  // Returns the header, cast to a user-specified type T.  The type T must be a
  // subclass of Header and its size must correspond to the header_size passed
  // to the Pickle constructor.
  template <class T>
  T* headerT() {
    return static_cast<T*>(header_);
  }
  template <class T>
  const T* headerT() const {
    return static_cast<const T*>(header_);
  }

  // The payload is the pickle data immediately following the header.
  size_t payload_size() const {
    return header_ ? header_->payload_size : 0;
  }

  const char* payload() const {
    return reinterpret_cast<const char*>(header_) + header_size_;
  }

  // Returns the address of the byte immediately following the currently valid
  // header + payload.
  const char* end_of_payload() const {
    // This object may be invalid.
    return header_ ? payload() + payload_size() : NULL;
  }

 protected:
  char* mutable_payload() {
    return reinterpret_cast<char*>(header_) + header_size_;
  }

  size_t capacity_after_header() const {
    return capacity_after_header_;
  }

  // Resize the capacity, note that the input value should not include the size
  // of the header.
  void Resize(size_t new_capacity);

  // Claims |num_bytes| bytes of payload. This is similar to Reserve() in that
  // it may grow the capacity, but it also advances the write offset of the
  // pickle by |num_bytes|. Claimed memory, including padding, is zeroed.
  //
  // Returns the address of the first byte claimed.
  void* ClaimBytes(size_t num_bytes);

  // Find the end of the pickled data that starts at range_start.  Returns NULL
  // if the entire Pickle is not found in the given data range.
  static const char* FindNext(size_t header_size,
                              const char* range_start,
                              const char* range_end);

  // Parse pickle header and return total size of the pickle. Data range
  // doesn't need to contain entire pickle.
  // Returns true if pickle header was found and parsed. Callers must check
  // returned |pickle_size| for sanity (against maximum message size, etc).
  // NOTE: when function successfully parses a header, but encounters an
  // overflow during pickle size calculation, it sets |pickle_size| to the
  // maximum size_t value and returns true.
  static bool PeekNext(size_t header_size,
                       const char* range_start,
                       const char* range_end,
                       size_t* pickle_size);

  // The allocation granularity of the payload.
  static const int kPayloadUnit;

 private:
  friend class PickleIterator;

  Header* header_;
  size_t header_size_;  // Supports extra data between header and payload.
  // Allocation size of payload (or -1 if allocation is const). Note: this
  // doesn't count the header.
  size_t capacity_after_header_;
  // The offset at which we will write the next field. Note: this doesn't count
  // the header.
  size_t write_offset_;

  // Just like WriteBytes, but with a compile-time size, for performance.
  template<size_t length> void CRBASE_EXPORT WriteBytesStatic(const void* data);

  // Writes a POD by copying its bytes.
  template <typename T> bool WritePOD(const T& data) {
    WriteBytesStatic<sizeof(data)>(&data);
    return true;
  }

  inline void* ClaimUninitializedBytesInternal(size_t num_bytes);
  inline void WriteBytesCommon(const void* data, size_t length);
};

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_PICKLE_H_