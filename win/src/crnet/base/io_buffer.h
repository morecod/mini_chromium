// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRNET_BASE_IO_BUFFER_H_
#define MINI_CHROMIUM_SRC_CRNET_BASE_IO_BUFFER_H_

#include <stddef.h>

#include <string>
#include <memory>
#include <vector>

#include "crbase/containers/array_view.h"
#include "crbase/memory/ref_counted.h"
#include "crbase/memory/free_deleter.h"
#include "crnet/base/net_export.h"

namespace crnet {

// IOBuffers are reference counted data buffers used for easier asynchronous
// IO handling.
//
// They are often used as the destination buffers for Read() operations, or as
// the source buffers for Write() operations.
//
// IMPORTANT: Never re-use an IOBuffer after cancelling the IO operation that
//            was using it, since this may lead to memory corruption!
//
// -----------------------
// Ownership of IOBuffers:
// -----------------------
//
// Although IOBuffers are RefCountedThreadSafe, they are not intended to be
// used as a shared buffer, nor should they be used simultaneously across
// threads. The fact that they are reference counted is an implementation
// detail for allowing them to outlive cancellation of asynchronous
// operations.
//
// Instead, think of the underlying |char*| buffer contained by the IOBuffer
// as having exactly one owner at a time.
//
// Whenever you call an asynchronous operation that takes an IOBuffer,
// ownership is implicitly transferred to the called function, until the
// operation has completed (at which point it transfers back to the caller).
//
//     ==> The IOBuffer's data should NOT be manipulated, destroyed, or read
//         until the operation has completed.
//
//     ==> Cancellation does NOT count as completion. If an operation using
//         an IOBuffer is cancelled, the caller should release their
//         reference to this IOBuffer at the time of cancellation since
//         they can no longer use it.
//
// For instance, if you were to call a Read() operation on some class which
// takes an IOBuffer, and then delete that class (which generally will
// trigger cancellation), the IOBuffer which had been passed to Read() should
// never be re-used.
//
// This usage contract is assumed by any API which takes an IOBuffer, even
// though it may not be explicitly mentioned in the function's comments.
//
// -----------------------
// Motivation
// -----------------------
//
// The motivation for transferring ownership during cancellation is
// to make it easier to work with un-cancellable operations.
//
// For instance, let's say under the hood your API called out to the
// operating system's synchronous ReadFile() function on a worker thread.
// When cancelling through our asynchronous interface, we have no way of
// actually aborting the in progress ReadFile(). We must let it keep running,
// and hence the buffer it was reading into must remain alive. Using
// reference counting we can add a reference to the IOBuffer and make sure
// it is not destroyed until after the synchronous operation has completed.

// Base class, never instantiated, does not own the buffer.
class CRNET_EXPORT IOBuffer : public cr::RefCountedThreadSafe<IOBuffer> {
 public:
  // Returns the length from bytes() to the end of the buffer. Many methods that
  // take an IOBuffer also take a size indicated the number of IOBuffer bytes to
  // use from the start of bytes(). That number must be no more than the size()
  // of the passed in IOBuffer.
  size_t size() const {
    // SetBytesView() ensures this fits in an int.
    return bytes_view_.size();
  }

  char* data() { return reinterpret_cast<char*>(bytes()); }
  const char* data() const { return reinterpret_cast<const char*>(bytes()); }

  uint8_t* bytes() { return bytes_view_.data(); }
  const uint8_t* bytes() const { return bytes_view_.data();}

  cr::BytesView first(size_t size) {
    CR_CHECK_LE(size, bytes_view_.size());
    return bytes_view_.subview(0, size);
  }

  cr::BytesView bytes_view() { return bytes_view_; }

 protected:
  friend class cr::RefCountedThreadSafe<IOBuffer>;

  static void AssertValidBufferSize(size_t size);

  IOBuffer() = default;
  explicit IOBuffer(cr::BytesView bytes_view);

  virtual ~IOBuffer() = default;

  // Sets `array_view_` to `array_view`. CR_CHECKs if its size is too big to fit
  // in an int.
  void SetBytesView(cr::BytesView array_view);

  // Like SetArrayView(cr::ArrayView<uint8_t>()), but without a size check. 
  // Particularly useful to call in the destructor of subclasses, to avoid 
  // failing raw reference checks.
  void ClearBytesView();

 private:
  cr::BytesView bytes_view_;
};


// Class which owns its buffer and manages its destruction.
class CRNET_EXPORT IOBufferWithSize : public IOBuffer {
 public:
  IOBufferWithSize();
  explicit IOBufferWithSize(size_t size);

 protected:
  ~IOBufferWithSize() override;

 private:
  std::unique_ptr<uint8_t[]> data_;
};

// This is like IOBufferWithSize, except its constructor takes a vector.
// IOBufferWithSize uses a HeapArray instead of a vector so that it can avoid
// initializing its data. VectorIOBuffer is primarily useful useful for writing
// data, while IOBufferWithSize is primarily useful for reading data.
class CRNET_EXPORT VectorIOBuffer : public IOBuffer {
public:
  explicit VectorIOBuffer(std::vector<uint8_t> vector);
  explicit VectorIOBuffer(cr::BytesView bytes_view);

private:
  ~VectorIOBuffer() override;

  std::vector<uint8_t> vector_;
};

// This is a read only IOBuffer.  The data is stored in a string and
// the IOBuffer interface does not provide a proper way to modify it.
class CRNET_EXPORT StringIOBuffer : public IOBuffer {
public:
  explicit StringIOBuffer(std::string s);

private:
  ~StringIOBuffer() override;

  std::string string_data_;
};

// This version wraps an existing IOBuffer and provides convenient functions
// to progressively read all the data. The values returned by size() and bytes()
// are updated as bytes are consumed from the buffer.
//
// DrainableIOBuffer is useful when you have an IOBuffer that contains data
// to be written progressively, and Write() function takes an IOBuffer rather
// than char*. DrainableIOBuffer can be used as follows:
//
// // payload is the IOBuffer containing the data to be written.
// buf = cr::MakeRefCounted<DrainableIOBuffer>(payload, payload_size);
//
// while (buf->BytesRemaining() > 0) {
//   // Write() takes an IOBuffer. If it takes char*, we could
//   // simply use the regular IOBuffer like payload->data() + offset.
//   int bytes_written = Write(buf, buf->BytesRemaining());
//   buf->DidConsume(bytes_written);
// }
//
class CRNET_EXPORT DrainableIOBuffer : public IOBuffer {
 public:
  // `base` should be treated as exclusively owned by the DrainableIOBuffer as
  // long as the latter exists. Specifically, the span pointed to by `base`,
  // including its size, must not change, as the `DrainableIOBuffer` maintains a
  // copy of them internally.
  DrainableIOBuffer(cr::scoped_refptr<IOBuffer> base, size_t size);

  // DidConsume() changes the |data_| pointer so that |data_| always points
  // to the first unconsumed byte.
  void DidConsume(size_t bytes);

  // Returns the number of unconsumed bytes.
  size_t BytesRemaining() const;

  // Returns the number of consumed bytes.
  size_t BytesConsumed() const;

  // Seeks to an arbitrary point in the buffer. The notion of bytes consumed
  // and remaining are updated appropriately.
  void SetOffset(size_t bytes);

 private:
  ~DrainableIOBuffer() override;

  cr::scoped_refptr<IOBuffer> base_;
  size_t used_ = 0;
};

// This version provides a resizable buffer and a changeable offset. The values
// returned by size() and bytes() are updated whenever the offset of the buffer
// is set, or the buffer's capacity is changed.
//
// GrowableIOBuffer is useful when you read data progressively without
// knowing the total size in advance. GrowableIOBuffer can be used as
// follows:
//
// buf = cr::MakeRefCounted<GrowableIOBuffer>();
// buf->SetCapacity(1024);  // Initial capacity.
//
// while (!some_stream->IsEOF()) {
//   // Double the capacity if the remaining capacity is empty.
//   if (buf->RemainingCapacity() == 0)
//     buf->SetCapacity(buf->capacity() * 2);
//   int bytes_read = some_stream->Read(buf, buf->RemainingCapacity());
//   buf->set_offset(buf->offset() + bytes_read);
// }
//
class CRNET_EXPORT GrowableIOBuffer : public IOBuffer {
 public:
  GrowableIOBuffer() = default;

  // realloc memory to the specified capacity.
  void SetCapacity(size_t capacity);
  size_t capacity() { return capacity_; }

  // `offset` moves the `data_` pointer, allowing "seeking" in the data.
  void set_offset(size_t offset);
  size_t offset() { return offset_; }

  // Advances the offset by `bytes`. It's equivalent to `set_offset(offset() +
  // bytes)`, though does not accept negative values, as they likely indicate a
  // bug.
  void DidConsume(size_t bytes);

  size_t RemainingCapacity();

  // Returns the entire buffer, including the bytes before the `offset()`.
  //
  // The `array_view()` method in the base class only gives the part of the buffer
  // after `offset()`.
  cr::BytesView everything();

  // Return a ArrayView before the `offset()`.
  cr::BytesView bytes_view_before_offset();

 private:
  ~GrowableIOBuffer() override;

  // TODO(329476354): Convert to std::vector, use reserve()+resize() to make
  // exact reallocs, and remove `capacity_`. Possibly with an allocator the
  // default-initializes, if it's important to not initialize the new memory?
  std::unique_ptr<uint8_t, cr::FreeDeleter> real_data_;
  size_t capacity_ = 0;
  size_t offset_ = 0;
};

}  // namespace crnet

#endif  // MINI_CHROMIUM_SRC_CRNET_BASE_IO_BUFFER_H_