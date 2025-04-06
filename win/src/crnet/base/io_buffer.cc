// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crnet/base/io_buffer.h"

#include "crbase/logging.h"
#include "crbase/numerics/safe_math.h"

namespace crnet {

// TODO(eroman): IOBuffer is being converted to require buffer sizes and offsets
// be specified as "size_t" rather than "int" (crbug.com/488553). To facilitate
// this move (since LOTS of code needs to be updated), this function ensures
// that sizes can be safely converted to an "int" without truncation. The
// assert ensures calling this with an "int" argument is also safe.
void IOBuffer::AssertValidBufferSize(size_t size) {
  static_assert(sizeof(size_t) >= sizeof(int), "size overflowed.");
  cr::CheckedNumeric<int>(size).ValueOrDie();
}

IOBuffer::IOBuffer(cr::BytesView bytes_view)
    : bytes_view_(bytes_view) {
  AssertValidBufferSize(bytes_view.size());
}

void IOBuffer::SetBytesView(cr::BytesView bytes_view) {
  AssertValidBufferSize(bytes_view.size());
  bytes_view_ = bytes_view;
}

void IOBuffer::ClearBytesView() {
  bytes_view_ = cr::BytesView();
}

// -- IOBufferWithSize
IOBufferWithSize::IOBufferWithSize() = default;

IOBufferWithSize::IOBufferWithSize(size_t size) {
  AssertValidBufferSize(size);
  data_.reset(new uint8_t[size]);
  SetBytesView(cr::MakeBytesView(data_.get(), size));
}

IOBufferWithSize::~IOBufferWithSize() {
  // Clear pointer before this destructor makes it dangle.
  ClearBytesView();
  data_.reset();
}

// -- VectorIOBuffer
VectorIOBuffer::VectorIOBuffer(std::vector<uint8_t> vector) 
    : vector_(std::move(vector)) {
  SetBytesView(cr::MakeBytesView(vector_));
}

VectorIOBuffer::VectorIOBuffer(cr::BytesView bytes_view) {
  AssertValidBufferSize(bytes_view.size());
  vector_.assign(bytes_view.begin(), bytes_view.end());
  SetBytesView(bytes_view);
}

VectorIOBuffer::~VectorIOBuffer() {
  ClearBytesView();
}

// -- StringIOBuffer
StringIOBuffer::StringIOBuffer(std::string s) 
    : string_data_(std::move(s)) {
  SetBytesView(cr::MakeBytesView(string_data_));
}

StringIOBuffer::~StringIOBuffer() {
  ClearBytesView();
}

// -- DrainableIOBuffer 
DrainableIOBuffer::DrainableIOBuffer(cr::scoped_refptr<IOBuffer> base, 
                                     size_t size)
    : IOBuffer(base->first(size)), base_(std::move(base)) {}

void DrainableIOBuffer::DidConsume(size_t bytes) {
  SetOffset(used_ + bytes);
}

size_t DrainableIOBuffer::BytesRemaining() const {
  return size();
}

// Returns the number of consumed bytes.
size_t DrainableIOBuffer::BytesConsumed() const {
  return used_;
}

void DrainableIOBuffer::SetOffset(size_t bytes) {
  // Length from the start of `base_` to the end of the buffer passed in to the
  // constructor isn't stored anywhere, so need to calculate it.
  size_t length = size() + used_;
  CR_CHECK_LE(bytes, length);
  used_ = bytes;
  SetBytesView(cr::MakeBytesView(base_->bytes() + used_, length - bytes));
}

DrainableIOBuffer::~DrainableIOBuffer() {
  // Clear ptr before this destructor destroys the |base_| instance,
  // making it dangle.
  ClearBytesView();
}

// -- GrowableIOBuffer 
void GrowableIOBuffer::SetCapacity(size_t capacity) {
  CR_CHECK_LE(capacity, std::numeric_limits<int>::max());

  // The ArrayView will be set again in `set_offset()`. Need to clear raw 
  // pointers to the data before reallocating the buffer.
  ClearBytesView();

  // realloc will crash if it fails.
  real_data_.reset(
      static_cast<uint8_t*>(realloc(real_data_.release(), capacity)));

  capacity_ = capacity;
  if (offset_ > capacity)
    set_offset(capacity);
  else
    set_offset(offset_);  // The pointer may have changed.
}

void GrowableIOBuffer::set_offset(size_t offset) {
  CR_CHECK_LE(offset, capacity_);
  offset_ = offset;

  SetBytesView(cr::MakeBytesView(
      real_data_.get() + offset, capacity_ - offset));
}

void GrowableIOBuffer::DidConsume(size_t bytes) {
  CR_CHECK_LE(bytes, size());
  set_offset(offset_ + bytes);
}

size_t GrowableIOBuffer::RemainingCapacity() {
  return size();
}

cr::BytesView GrowableIOBuffer::everything() {
  return cr::MakeBytesView(real_data_.get(), capacity_);
}

cr::BytesView GrowableIOBuffer::bytes_view_before_offset() {
  return cr::MakeBytesView(real_data_.get(), offset_);
}

GrowableIOBuffer::~GrowableIOBuffer() {
  ClearBytesView();
}

}  // namespace crnet