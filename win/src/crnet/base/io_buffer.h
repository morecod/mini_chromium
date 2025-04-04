// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRNET_BASE_IO_BUFFER_H_
#define MINI_CHROMIUM_SRC_CRNET_BASE_IO_BUFFER_H_

#include <stddef.h>

#include <string>
#include <memory>

#include "crbase/memory/free_deleter.h"
#include "crbase/memory/ref_counted.h"
#include "crbase/pickle.h"
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
class CRNET_EXPORT IOBuffer : public cr::RefCountedThreadSafe<IOBuffer> {
 public:
  IOBuffer();

  // TODO(eroman): Deprecated. Use the size_t flavor instead. crbug.com/488553
  ///explicit IOBuffer(int buffer_size);
  explicit IOBuffer(size_t buffer_size);

  char* data() { return data_; }

 protected:
  friend class cr::RefCountedThreadSafe<IOBuffer>;

  // Only allow derived classes to specify data_.
  // In all other cases, we own data_, and must delete it at destruction time.
  explicit IOBuffer(char* data);

  virtual ~IOBuffer();

  char* data_;
};

// This version stores the size of the buffer so that the creator of the object
// doesn't have to keep track of that value.
// NOTE: This doesn't mean that we want to stop sending the size as an explicit
// argument to IO functions. Please keep using IOBuffer* for API declarations.
class CRNET_EXPORT IOBufferWithSize : public IOBuffer {
 public:
  // TODO(eroman): Deprecated. Use the size_t flavor instead. crbug.com/488553
  ///explicit IOBufferWithSize(int size);
  explicit IOBufferWithSize(size_t size);

  size_t size() const { return size_; }

 protected:
  // TODO(eroman): Deprecated. Use the size_t flavor instead. crbug.com/488553
  ///IOBufferWithSize(char* data, int size);

  // Purpose of this constructor is to give a subclass access to the base class
  // constructor IOBuffer(char*) thus allowing subclass to use underlying
  // memory it does not own.
  IOBufferWithSize(char* data, size_t size);
  ~IOBufferWithSize() override;

  size_t size_;
};

// This is a read only IOBuffer.  The data is stored in a string and
// the IOBuffer interface does not provide a proper way to modify it.
class CRNET_EXPORT StringIOBuffer : public IOBuffer {
 public:
  explicit StringIOBuffer(const std::string& s);
  explicit StringIOBuffer(std::unique_ptr<std::string> s);

  size_t size() const { return string_data_.size(); }

 private:
  ~StringIOBuffer() override;

  std::string string_data_;
};

// This version wraps an existing IOBuffer and provides convenient functions
// to progressively read all the data.
//
// DrainableIOBuffer is useful when you have an IOBuffer that contains data
// to be written progressively, and Write() function takes an IOBuffer rather
// than char*. DrainableIOBuffer can be used as follows:
//
// // payload is the IOBuffer containing the data to be written.
// buf = new DrainableIOBuffer(payload, payload_size);
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
  // TODO(eroman): Deprecated. Use the size_t flavor instead. crbug.com/488553
  ///DrainableIOBuffer(IOBuffer* base, int size);
  DrainableIOBuffer(IOBuffer* base, size_t size);

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

  size_t size() const { return size_; }

 private:
  ~DrainableIOBuffer() override;

  cr::scoped_refptr<IOBuffer> base_;
  size_t size_;
  size_t used_;
};

// This version provides a resizable buffer and a changeable offset.
//
// GrowableIOBuffer is useful when you read data progressively without
// knowing the total size in advance. GrowableIOBuffer can be used as
// follows:
//
// buf = new GrowableIOBuffer;
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
  GrowableIOBuffer();

  // realloc memory to the specified capacity.
  void SetCapacity(int capacity);
  int capacity() { return capacity_; }

  // |offset| moves the |data_| pointer, allowing "seeking" in the data.
  void set_offset(int offset);
  int offset() { return offset_; }

  int RemainingCapacity();
  char* StartOfBuffer();

 private:
  ~GrowableIOBuffer() override;

  std::unique_ptr<char, cr::FreeDeleter> real_data_;
  int capacity_;
  int offset_;
};

// This versions allows a pickle to be used as the storage for a write-style
// operation, avoiding an extra data copy.
class CRNET_EXPORT PickledIOBuffer : public IOBuffer {
 public:
  PickledIOBuffer();

  cr::Pickle* pickle() { return &pickle_; }

  // Signals that we are done writing to the pickle and we can use it for a
  // write-style IO operation.
  void Done();

 private:
  ~PickledIOBuffer() override;

  cr::Pickle pickle_;
};

// This class allows the creation of a temporary IOBuffer that doesn't really
// own the underlying buffer. Please use this class only as a last resort.
// A good example is the buffer for a synchronous operation, where we can be
// sure that nobody is keeping an extra reference to this object so the lifetime
// of the buffer can be completely managed by its intended owner.
class CRNET_EXPORT WrappedIOBuffer : public IOBuffer {
 public:
  explicit WrappedIOBuffer(const char* data);

 protected:
  ~WrappedIOBuffer() override;
};

}  // namespace crnet

#endif  // MINI_CHROMIUM_SRC_CRNET_BASE_IO_BUFFER_H_