// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_CRBASE_FUNCTIONAL_CALLBACK_LIST_H_
#define MINI_CHROMIUM_CRBASE_FUNCTIONAL_CALLBACK_LIST_H_

#include <list>
#include <memory>

#include "crbase/functional/callback_forward.h"
#include "crbase/functional/callback.h"
#include "crbase/compiler_specific.h"
#include "crbase/logging.h"
#include "crbase/macros.h"

// OVERVIEW:
//
// A container for a list of callbacks.  Unlike a normal STL vector or list,
// this container can be modified during iteration without invalidating the
// iterator. It safely handles the case of a callback removing itself
// or another callback from the list while callbacks are being run.
//
// TYPICAL USAGE:
//
// class MyWidget {
//  public:
//   ...
//
//   MyWidget(const MyWidget&) = delete;
//   MyWidget& operator=(const MyWidget&) = delete;
//
//   typedef cr::Callback<void(const Foo&)> OnFooCallback;
//
//   std::unique_ptr<cr::CallbackList<void(const Foo&)>::Subscription>
//   RegisterCallback(const OnFooCallback& cb) {
//     return callback_list_.Add(cb);
//   }
//
//  private:
//   void NotifyFoo(const Foo& foo) {
//      callback_list_.Notify(foo);
//   }
//
//   cr::RepeatingCallbackList<void(const Foo&)> callback_list_;
// };
//
//
// class MyWidgetListener {
//  public:
//   MyWidgetListener(const MyWidgetListener&) = delete;
//   MyWidgetListener& operator=(const MyWidgetListener&) = delete;
//
//   MyWidgetListener::MyWidgetListener() {
//     foo_subscription_ = MyWidget::GetCurrent()->RegisterCallback(
//             cr::BindRepeating(&MyWidgetListener::OnFoo, this)));
//   }
//
//   MyWidgetListener::~MyWidgetListener() {
//      // Subscription gets deleted automatically and will deregister
//      // the callback in the process.
//   }
//
//  private:
//   void OnFoo(const Foo& foo) {
//     // Do something.
//   }
//
//   std::unique_ptr<cr::RepeatingCallbackList<void(const Foo&)>::
//                       Subscription>
//       foo_subscription_;
// };

namespace cr {

namespace internal {

template <typename CallbackType>
class CallbackListBase {
 public:
  class Subscription {
   public:
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    Subscription(CallbackListBase<CallbackType>* list,
                 typename std::list<CallbackType>::iterator iter)
        : list_(list),
          iter_(iter) {
    }

    ~Subscription() {
      if (list_->active_iterator_count_) {
        iter_->Reset();
      } else {
        list_->callbacks_.erase(iter_);
        if (!list_->removal_callback_.is_null())
          list_->removal_callback_.Run();
      }
    }

   private:
    CallbackListBase<CallbackType>* list_;
    typename std::list<CallbackType>::iterator iter_;
  };

  // Add a callback to the list. The callback will remain registered until the
  // returned Subscription is destroyed, which must occur before the
  // CallbackList is destroyed.
  std::unique_ptr<Subscription> Add(const CallbackType& cb) 
      CR_WARN_UNUSED_RESULT {
    CR_DCHECK(!cb.is_null());
    return std::unique_ptr<Subscription>(
        new Subscription(this, callbacks_.insert(callbacks_.end(), cb)));
  }

  // Sets a callback which will be run when a subscription list is changed.
  void set_removal_callback(const Closure& callback) {
    removal_callback_ = callback;
  }

  // Returns true if there are no subscriptions. This is only valid to call when
  // not looping through the list.
  bool empty() {
    CR_DCHECK_EQ(0, active_iterator_count_);
    return callbacks_.empty();
  }

 protected:
  // An iterator class that can be used to access the list of callbacks.
  class Iterator {
   public:
    explicit Iterator(CallbackListBase<CallbackType>* list)
        : list_(list),
          list_iter_(list_->callbacks_.begin()) {
      ++list_->active_iterator_count_;
    }

    Iterator(const Iterator& iter)
        : list_(iter.list_),
          list_iter_(iter.list_iter_) {
      ++list_->active_iterator_count_;
    }

    ~Iterator() {
      if (list_ && --list_->active_iterator_count_ == 0) {
        list_->Compact();
      }
    }

    CallbackType* GetNext() {
      while ((list_iter_ != list_->callbacks_.end()) && list_iter_->is_null())
        ++list_iter_;

      CallbackType* cb = NULL;
      if (list_iter_ != list_->callbacks_.end()) {
        cb = &(*list_iter_);
        ++list_iter_;
      }
      return cb;
    }

   private:
    CallbackListBase<CallbackType>* list_;
    typename std::list<CallbackType>::iterator list_iter_;
  };
  CallbackListBase(const CallbackListBase&) = delete;
  CallbackListBase& operator=(const CallbackListBase&) = delete;

  CallbackListBase() : active_iterator_count_(0) {}

  ~CallbackListBase() {
    CR_DCHECK_EQ(0, active_iterator_count_);
    CR_DCHECK_EQ(0U, callbacks_.size());
  }

  // Returns an instance of a CallbackListBase::Iterator which can be used
  // to run callbacks.
  Iterator GetIterator() {
    return Iterator(this);
  }

  // Compact the list: remove any entries which were NULLed out during
  // iteration.
  void Compact() {
    typename std::list<CallbackType>::iterator it = callbacks_.begin();
    bool updated = false;
    while (it != callbacks_.end()) {
      if ((*it).is_null()) {
        updated = true;
        it = callbacks_.erase(it);
      } else {
        ++it;
      }
    }

    if (updated && !removal_callback_.is_null())
      removal_callback_.Run();
  }

 private:
  std::list<CallbackType> callbacks_;
  int active_iterator_count_;
  RepeatingClosure removal_callback_;
};

}  // namespace internal

template <typename Sig> class RepeatingCallbackList;

template <typename... Args>
class RepeatingCallbackList<void(Args...)>
    : public internal::CallbackListBase<Callback<void(Args...)> > {
 public:
  typedef Callback<void(Args...)> CallbackType;

  RepeatingCallbackList(const RepeatingCallbackList&) = delete;
  RepeatingCallbackList& operator=(const RepeatingCallbackList&) = delete;

  RepeatingCallbackList() {}

  template <typename... RunArgs>
  void Notify(RunArgs&&... args) {
    typename internal::CallbackListBase<CallbackType>::Iterator it =
        this->GetIterator();
    CallbackType* cb;
    while ((cb = it.GetNext()) != NULL) {
      cb->Run(args...);
    }
  }
};

}  // namespace cr

#endif  // MINI_CHROMIUM_CRBASE_FUNCTIONAL_CALLBACK_LIST_H_