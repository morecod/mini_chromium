// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_CRBASE_MEMORY_REF_COUNTED_H_
#define MINI_CHROMIUM_CRBASE_MEMORY_REF_COUNTED_H_

#include <stddef.h>

#include <cassert>
#include <iosfwd>
#include <type_traits>

#include "crbase/atomic/atomic_ref_count.h"
#include "crbase/base_export.h"
#include "crbase/compiler_specific.h"
#include "crbase/logging.h"
#include "crbase/macros.h"
///#include "base/sequence_checker.h"
///#include "base/threading/thread_collision_warner.h"
#include "crbase/build_config.h"

namespace cr {

template <class T>
class scoped_refptr;

template <typename T>
scoped_refptr<T> AdoptRef(T* t);

namespace subtle {

enum AdoptRefTag { kAdoptRefTag };
enum StartRefCountFromZeroTag { kStartRefCountFromZeroTag };
enum StartRefCountFromOneTag { kStartRefCountFromOneTag };

class CRBASE_EXPORT RefCountedBase {
 public:
  bool HasOneRef() const { return ref_count_ == 1; }

  RefCountedBase(const RefCountedBase&) = delete;
  RefCountedBase& operator=(const RefCountedBase&) = delete;

 protected:
  explicit RefCountedBase(StartRefCountFromZeroTag) {
///#if DCHECK_IS_ON()
///    sequence_checker_.DetachFromSequence();
///#endif
  }

  explicit RefCountedBase(StartRefCountFromOneTag) : ref_count_(1) {
#if CR_DCHECK_IS_ON()
    needs_adopt_ref_ = true;
///    sequence_checker_.DetachFromSequence();
#endif
  }

  ~RefCountedBase() {
#if CR_DCHECK_IS_ON()
    CR_DCHECK(in_dtor_) 
        << "RefCounted object deleted without calling Release()";
#endif
  }

  void AddRef() const {
    // TODO(maruel): Add back once it doesn't assert 500 times/sec.
    // Current thread books the critical section "AddRelease"
    // without release it.
    // DFAKE_SCOPED_LOCK_THREAD_LOCKED(add_release_);
#if CR_DCHECK_IS_ON()
    CR_DCHECK(!in_dtor_);
    CR_DCHECK(!needs_adopt_ref_)
        << "This RefCounted object is created with non-zero reference count."
        << " The first reference to such a object has to be made by AdoptRef or"
        << " MakeRefCounted.";
    ///if (ref_count_ >= 1) {
    ///  DCHECK(CalledOnValidSequence());
    ///}
#endif

    ++ref_count_;
  }

  // Returns true if the object should self-delete.
  bool Release() const {
    --ref_count_;

    // TODO(maruel): Add back once it doesn't assert 500 times/sec.
    // Current thread books the critical section "AddRelease"
    // without release it.
    // DFAKE_SCOPED_LOCK_THREAD_LOCKED(add_release_);

#if CR_DCHECK_IS_ON()
    CR_DCHECK(!in_dtor_);
    if (ref_count_ == 0)
      in_dtor_ = true;

    ///if (ref_count_ >= 1)
    ///  DCHECK(CalledOnValidSequence());
    ///if (ref_count_ == 1)
    ///  sequence_checker_.DetachFromSequence();
#endif

    return ref_count_ == 0;
  }

 private:
  template <typename U>
  friend scoped_refptr<U> cr::AdoptRef(U*);

  void Adopted() const {
#if CR_DCHECK_IS_ON()
    CR_DCHECK(needs_adopt_ref_);
    needs_adopt_ref_ = false;
#endif
  }

///#if DCHECK_IS_ON()
///  bool CalledOnValidSequence() const;
///#endif

  mutable size_t ref_count_ = 0;

#if CR_DCHECK_IS_ON()
  mutable bool needs_adopt_ref_ = false;
  mutable bool in_dtor_ = false;
  ///mutable SequenceChecker sequence_checker_;
#endif

  ///DFAKE_MUTEX(add_release_);

  ///DISALLOW_COPY_AND_ASSIGN(RefCountedBase);
};

class CRBASE_EXPORT RefCountedThreadSafeBase {
 public:
  bool HasOneRef() const;

  RefCountedThreadSafeBase(const RefCountedThreadSafeBase&) = delete;
  RefCountedThreadSafeBase& operator=(const RefCountedThreadSafeBase&) = delete;

 protected:
  explicit RefCountedThreadSafeBase(StartRefCountFromZeroTag) {}
  explicit RefCountedThreadSafeBase(StartRefCountFromOneTag) : ref_count_(1) {
#if CR_DCHECK_IS_ON()
    needs_adopt_ref_ = true;
#endif
  }

  ~RefCountedThreadSafeBase();

  void AddRef() const;

  // Returns true if the object should self-delete.
  bool Release() const;

 private:
  template <typename U>
  friend scoped_refptr<U> cr::AdoptRef(U*);

  void Adopted() const {
#if CR_DCHECK_IS_ON()
    CR_DCHECK(needs_adopt_ref_);
    needs_adopt_ref_ = false;
#endif
  }

  mutable AtomicRefCount ref_count_ = 0;
#if CR_DCHECK_IS_ON()
  mutable bool needs_adopt_ref_ = false;
  mutable bool in_dtor_ = false;
#endif

  ///DISALLOW_COPY_AND_ASSIGN(RefCountedThreadSafeBase);
};

}  // namespace subtle

//
// A base class for reference counted classes.  Otherwise, known as a cheap
// knock-off of WebKit's RefCounted<T> class.  To use this, just extend your
// class from it like so:
//
//   class MyFoo : public cr::RefCounted<MyFoo> {
//    ...
//    private:
//     friend class cr::RefCounted<MyFoo>;
//     ~MyFoo();
//   };
//
// You should always make your destructor non-public, to avoid any code deleting
// the object accidently while there are references to it.
//
//
// The ref count manipulation to RefCounted is NOT thread safe and has DCHECKs
// to trap unsafe cross thread usage. A subclass instance of RefCounted can be
// passed to another execution sequence only when its ref count is 1. If the ref
// count is more than 1, the RefCounted class verifies the ref updates are made
// on the same execution sequence as the previous ones.
//
//
// The reference count starts from zero by default, and we intended to migrate
// to start-from-one ref count. Put REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE() to
// the ref counted class to opt-in.
//
// If an object has start-from-one ref count, the first scoped_refptr need to be
// created by base::AdoptRef() or cr::MakeRefCounted(). We can use
// cr::MakeRefCounted() to create create both type of ref counted object.
//
// The motivations to use start-from-one ref count are:
//  - Start-from-one ref count doesn't need the ref count increment for the
//    first reference.
//  - It can detect an invalid object acquisition for a being-deleted object
//    that has zero ref count. That tends to happen on custom deleter that
//    delays the deletion.
//    TODO(tzik): Implement invalid acquisition detection.
//  - Behavior parity to Blink's WTF::RefCounted, whose count starts from one.
//    And start-from-one ref count is a step to merge WTF::RefCounted into
//    cr::RefCounted.
//
#define REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE()               \
  static constexpr ::cr::subtle::StartRefCountFromOneTag \
      kRefCountPreference = ::cr::subtle::kStartRefCountFromOneTag;

template <class T>
class RefCounted : public subtle::RefCountedBase {
 public:
  static constexpr subtle::StartRefCountFromZeroTag kRefCountPreference =
      subtle::kStartRefCountFromZeroTag;

  RefCounted() : subtle::RefCountedBase(T::kRefCountPreference) {}

  RefCounted(const RefCounted&) = delete;
  RefCounted& operator=(const RefCounted&) = delete;

  void AddRef() const {
    subtle::RefCountedBase::AddRef();
  }

  void Release() const {
    if (subtle::RefCountedBase::Release()) {
      delete static_cast<const T*>(this);
    }
  }

 protected:
  ~RefCounted() = default;

 private:
  ///DISALLOW_COPY_AND_ASSIGN(RefCounted);
};

// Forward declaration.
template <class T, typename Traits> class RefCountedThreadSafe;

// Default traits for RefCountedThreadSafe<T>.  Deletes the object when its ref
// count reaches 0.  Overload to delete it on a different thread etc.
template<typename T>
struct DefaultRefCountedThreadSafeTraits {
  static void Destruct(const T* x) {
    // Delete through RefCountedThreadSafe to make child classes only need to be
    // friend with RefCountedThreadSafe instead of this struct, which is an
    // implementation detail.
    RefCountedThreadSafe<T,
                         DefaultRefCountedThreadSafeTraits>::DeleteInternal(x);
  }
};

//
// A thread-safe variant of RefCounted<T>
//
//   class MyFoo : public cr::RefCountedThreadSafe<MyFoo> {
//    ...
//   };
//
// If you're using the default trait, then you should add compile time
// asserts that no one else is deleting your object.  i.e.
//    private:
//     friend class cr::RefCountedThreadSafe<MyFoo>;
//     ~MyFoo();
//
// We can use REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE() with RefCountedThreadSafe
// too. See the comment above the RefCounted definition for details.
template <class T, typename Traits = DefaultRefCountedThreadSafeTraits<T> >
class RefCountedThreadSafe : public subtle::RefCountedThreadSafeBase {
 public:
  static constexpr subtle::StartRefCountFromZeroTag kRefCountPreference =
      subtle::kStartRefCountFromZeroTag;

  explicit RefCountedThreadSafe()
      : subtle::RefCountedThreadSafeBase(T::kRefCountPreference) {}

  RefCountedThreadSafe(const RefCountedThreadSafe&) = delete;
  RefCountedThreadSafe& operator=(const RefCountedThreadSafe&) = delete;

  void AddRef() const {
    subtle::RefCountedThreadSafeBase::AddRef();
  }

  void Release() const {
    if (subtle::RefCountedThreadSafeBase::Release()) {
      Traits::Destruct(static_cast<const T*>(this));
    }
  }

 protected:
  ~RefCountedThreadSafe() = default;

 private:
  friend struct DefaultRefCountedThreadSafeTraits<T>;
  static void DeleteInternal(const T* x) { delete x; }

  ///DISALLOW_COPY_AND_ASSIGN(RefCountedThreadSafe);
};

//
// A thread-safe wrapper for some piece of data so we can place other
// things in scoped_refptrs<>.
//
template<typename T>
class RefCountedData
    : public cr::RefCountedThreadSafe< cr::RefCountedData<T> > {
 public:
  RefCountedData() : data() {}
  RefCountedData(const T& in_value) : data(in_value) {}

  T data;

 private:
  friend class cr::RefCountedThreadSafe<cr::RefCountedData<T> >;
  ~RefCountedData() = default;
};

// Creates a scoped_refptr from a raw pointer without incrementing the reference
// count. Use this only for a newly created object whose reference count starts
// from 1 instead of 0.
template <typename T>
scoped_refptr<T> AdoptRef(T* obj) {
  using Tag = typename std::decay<decltype(T::kRefCountPreference)>::type;
  static_assert(std::is_same<subtle::StartRefCountFromOneTag, Tag>::value,
                "Use AdoptRef only for the reference count starts from one.");

  CR_DCHECK(obj);
  CR_DCHECK(obj->HasOneRef());
  obj->Adopted();
  return scoped_refptr<T>(obj, subtle::kAdoptRefTag);
}

namespace subtle {

template <typename T>
scoped_refptr<T> AdoptRefIfNeeded(T* obj, StartRefCountFromZeroTag) {
  return scoped_refptr<T>(obj);
}

template <typename T>
scoped_refptr<T> AdoptRefIfNeeded(T* obj, StartRefCountFromOneTag) {
  return AdoptRef(obj);
}

}  // namespace subtle

// Constructs an instance of T, which is a ref counted type, and wraps the
// object into a scoped_refptr.
template <typename T, typename... Args>
scoped_refptr<T> MakeRefCounted(Args&&... args) {
  T* obj = new T(std::forward<Args>(args)...);
  return subtle::AdoptRefIfNeeded(obj, T::kRefCountPreference);
}

//
// A smart pointer class for reference counted objects.  Use this class instead
// of calling AddRef and Release manually on a reference counted object to
// avoid common memory leaks caused by forgetting to Release an object
// reference.  Sample usage:
//
//   class MyFoo : public RefCounted<MyFoo> {
//    ...
//    private:
//     friend class RefCounted<MyFoo>;  // Allow destruction by RefCounted<>.
//     ~MyFoo();                        // Destructor must be private/protected.
//   };
//
//   void some_function() {
//     scoped_refptr<MyFoo> foo = new MyFoo();
//     foo->Method(param);
//     // |foo| is released when this function returns
//   }
//
//   void some_other_function() {
//     scoped_refptr<MyFoo> foo = new MyFoo();
//     ...
//     foo = nullptr;  // explicitly releases |foo|
//     ...
//     if (foo)
//       foo->Method(param);
//   }
//
// The above examples show how scoped_refptr<T> acts like a pointer to T.
// Given two scoped_refptr<T> classes, it is also possible to exchange
// references between the two objects, like so:
//
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b.swap(a);
//     // now, |b| references the MyFoo object, and |a| references nullptr.
//   }
//
// To make both |a| and |b| in the above example reference the same MyFoo
// object, simply use the assignment operator:
//
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b = a;
//     // now, |a| and |b| each own a reference to the same MyFoo object.
//   }
//
template <class T>
class scoped_refptr {
 public:
  typedef T element_type;

  scoped_refptr() {}

  scoped_refptr(T* p) : ptr_(p) {
    if (ptr_)
      AddRef(ptr_);
  }

  // Copy constructor.
  scoped_refptr(const scoped_refptr<T>& r) : ptr_(r.ptr_) {
    if (ptr_)
      AddRef(ptr_);
  }

  // Copy conversion constructor.
  template <typename U,
            typename = typename std::enable_if<
                std::is_convertible<U*, T*>::value>::type>
  scoped_refptr(const scoped_refptr<U>& r) : ptr_(r.get()) {
    if (ptr_)
      AddRef(ptr_);
  }

  // Move constructor. This is required in addition to the conversion
  // constructor below in order for clang to warn about pessimizing moves.
  scoped_refptr(scoped_refptr&& r) : ptr_(r.get()) { r.ptr_ = nullptr; }

  // Move conversion constructor.
  template <typename U,
            typename = typename std::enable_if<
                std::is_convertible<U*, T*>::value>::type>
  scoped_refptr(scoped_refptr<U>&& r) : ptr_(r.get()) {
    r.ptr_ = nullptr;
  }

  ~scoped_refptr() {
    if (ptr_)
      Release(ptr_);
  }

  T* get() const { return ptr_; }

  T& operator*() const {
    assert(ptr_ != nullptr);
    return *ptr_;
  }

  T* operator->() const {
    assert(ptr_ != nullptr);
    return ptr_;
  }

  scoped_refptr<T>& operator=(T* p) {
    // AddRef first so that self assignment should work
    if (p)
      AddRef(p);
    T* old_ptr = ptr_;
    ptr_ = p;
    if (old_ptr)
      Release(old_ptr);
    return *this;
  }

  scoped_refptr<T>& operator=(const scoped_refptr<T>& r) {
    return *this = r.ptr_;
  }

  template <typename U>
  scoped_refptr<T>& operator=(const scoped_refptr<U>& r) {
    return *this = r.get();
  }

  scoped_refptr<T>& operator=(scoped_refptr<T>&& r) {
    scoped_refptr<T>(std::move(r)).swap(*this);
    return *this;
  }

  template <typename U>
  scoped_refptr<T>& operator=(scoped_refptr<U>&& r) {
    scoped_refptr<T>(std::move(r)).swap(*this);
    return *this;
  }

  void swap(scoped_refptr<T>& r) {
    T* tmp = ptr_;
    ptr_ = r.ptr_;
    r.ptr_ = tmp;
  }

  explicit operator bool() const { return ptr_ != nullptr; }

  template <typename U>
  bool operator==(const scoped_refptr<U>& rhs) const {
    return ptr_ == rhs.get();
  }

  template <typename U>
  bool operator!=(const scoped_refptr<U>& rhs) const {
    return !operator==(rhs);
  }

  template <typename U>
  bool operator<(const scoped_refptr<U>& rhs) const {
    return ptr_ < rhs.get();
  }

 protected:
  T* ptr_ = nullptr;

 private:
  template <typename U>
  friend scoped_refptr<U> cr::AdoptRef(U*);

  scoped_refptr(T* p, cr::subtle::AdoptRefTag) : ptr_(p) {}

  // Friend required for move constructors that set r.ptr_ to null.
  template <typename U>
  friend class scoped_refptr;

  // Non-inline helpers to allow:
  //     class Opaque;
  //     extern template class scoped_refptr<Opaque>;
  // Otherwise the compiler will complain that Opaque is an incomplete type.
  static void AddRef(T* ptr);
  static void Release(T* ptr);
};

// static
template <typename T>
void scoped_refptr<T>::AddRef(T* ptr) {
  ptr->AddRef();
}

// static
template <typename T>
void scoped_refptr<T>::Release(T* ptr) {
  ptr->Release();
}

// Handy utility for creating a scoped_refptr<T> out of a T* explicitly without
// having to retype all the template arguments
template <typename T>
scoped_refptr<T> make_scoped_refptr(T* t) {
  return scoped_refptr<T>(t);
}

template <typename T, typename U>
bool operator==(const scoped_refptr<T>& lhs, const U* rhs) {
  return lhs.get() == rhs;
}

template <typename T, typename U>
bool operator==(const T* lhs, const scoped_refptr<U>& rhs) {
  return lhs == rhs.get();
}

template <typename T>
bool operator==(const scoped_refptr<T>& lhs, std::nullptr_t null) {
  return !static_cast<bool>(lhs);
}

template <typename T>
bool operator==(std::nullptr_t null, const scoped_refptr<T>& rhs) {
  return !static_cast<bool>(rhs);
}

template <typename T, typename U>
bool operator!=(const scoped_refptr<T>& lhs, const U* rhs) {
  return !operator==(lhs, rhs);
}

template <typename T, typename U>
bool operator!=(const T* lhs, const scoped_refptr<U>& rhs) {
  return !operator==(lhs, rhs);
}

template <typename T>
bool operator!=(const scoped_refptr<T>& lhs, std::nullptr_t null) {
  return !operator==(lhs, null);
}

template <typename T>
bool operator!=(std::nullptr_t null, const scoped_refptr<T>& rhs) {
  return !operator==(null, rhs);
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const scoped_refptr<T>& p) {
  return out << p.get();
}

}  // namespace cr

#endif  // MINI_CHROMIUM_CRBASE_MEMORY_REF_COUNTED_H_