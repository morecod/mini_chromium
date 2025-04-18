// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CR_SRC_MEMORY_SCOPED_VECTOR_H_
#define CR_SRC_MEMORY_SCOPED_VECTOR_H_

#include <stddef.h>

#include <vector>
#include <memory>

#include "crbase/stl_util.h"

namespace cr {

// ScopedVector wraps a vector deleting the elements from its
// destructor.
//
// TODO(http://crbug.com/554289): DEPRECATED: Use std::vector instead (now that
// we have support for moveable types inside containers).
template <class T>
class ScopedVector {
 public:
  ScopedVector(const ScopedVector<T>&) = delete;
  ScopedVector<T>& operator=(const ScopedVector<T>&) = delete;
  ScopedVector<T> &&Pass() { return std::move(*this); };
  typedef void MoveOnlyTypeForCPP03;

public:
  typedef typename std::vector<T*>::allocator_type allocator_type;
  typedef typename std::vector<T*>::size_type size_type;
  typedef typename std::vector<T*>::difference_type difference_type;
  typedef typename std::vector<T*>::pointer pointer;
  typedef typename std::vector<T*>::const_pointer const_pointer;
  typedef typename std::vector<T*>::reference reference;
  typedef typename std::vector<T*>::const_reference const_reference;
  typedef typename std::vector<T*>::value_type value_type;
  typedef typename std::vector<T*>::iterator iterator;
  typedef typename std::vector<T*>::const_iterator const_iterator;
  typedef typename std::vector<T*>::reverse_iterator reverse_iterator;
  typedef typename std::vector<T*>::const_reverse_iterator
      const_reverse_iterator;

  ScopedVector() {}
  ~ScopedVector() { clear(); }
  ScopedVector(ScopedVector&& other) { swap(other); }

  ScopedVector& operator=(ScopedVector&& rhs) {
    swap(rhs);
    return *this;
  }

  reference operator[](size_t index) { return v_[index]; }
  const_reference operator[](size_t index) const { return v_[index]; }

  bool empty() const { return v_.empty(); }
  size_t size() const { return v_.size(); }

  reverse_iterator rbegin() { return v_.rbegin(); }
  const_reverse_iterator rbegin() const { return v_.rbegin(); }
  reverse_iterator rend() { return v_.rend(); }
  const_reverse_iterator rend() const { return v_.rend(); }

  iterator begin() { return v_.begin(); }
  const_iterator begin() const { return v_.begin(); }
  iterator end() { return v_.end(); }
  const_iterator end() const { return v_.end(); }

  const_reference front() const { return v_.front(); }
  reference front() { return v_.front(); }
  const_reference back() const { return v_.back(); }
  reference back() { return v_.back(); }

  void push_back(T* elem) { v_.push_back(elem); }
  void push_back(std::unique_ptr<T> elem) { v_.push_back(elem.release()); }

  void pop_back() {
    delete v_.back();
    v_.pop_back();
  }

  std::vector<T*>& get() { return v_; }
  const std::vector<T*>& get() const { return v_; }
  void swap(std::vector<T*>& other) { v_.swap(other); }
  void swap(ScopedVector<T>& other) { v_.swap(other.v_); }
  void release(std::vector<T*>* out) {
    out->swap(v_);
    v_.clear();
  }

  void reserve(size_t capacity) { v_.reserve(capacity); }

  // Resize, deleting elements in the disappearing range if we are shrinking.
  void resize(size_t new_size) {
    if (v_.size() > new_size)
      STLDeleteContainerPointers(v_.begin() + new_size, v_.end());
    v_.resize(new_size);
  }

  template<typename InputIterator>
  void assign(InputIterator begin, InputIterator end) {
    v_.assign(begin, end);
  }

  void clear() { STLDeleteElements(&v_); }

  // Like |clear()|, but doesn't delete any elements.
  void weak_clear() { v_.clear(); }

  // Lets the ScopedVector take ownership of |x|.
  iterator insert(iterator position, T* x) {
    return v_.insert(position, x);
  }

  iterator insert(iterator position, std::unique_ptr<T> x) {
    return v_.insert(position, x.release());
  }

  // Lets the ScopedVector take ownership of elements in [first,last).
  template<typename InputIterator>
  void insert(iterator position, InputIterator first, InputIterator last) {
    v_.insert(position, first, last);
  }

  iterator erase(iterator position) {
    delete *position;
    return v_.erase(position);
  }

  iterator erase(iterator first, iterator last) {
    STLDeleteContainerPointers(first, last);
    return v_.erase(first, last);
  }

  // Like |erase()|, but doesn't delete the element at |position|.
  iterator weak_erase(iterator position) {
    return v_.erase(position);
  }

  // Like |erase()|, but doesn't delete the elements in [first, last).
  iterator weak_erase(iterator first, iterator last) {
    return v_.erase(first, last);
  }

 private:
  std::vector<T*> v_;
};

}  //  namesapce crbase

#endif  // CR_SRC_MEMORY_SCOPED_VECTOR_H_