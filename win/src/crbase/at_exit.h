// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_AT_EXIT_H_
#define MINI_CHROMIUM_SRC_CRBASE_AT_EXIT_H_

#include <stack>

#include "crbase/base_export.h"
#include "crbase/functional/callback.h"
#include "crbase/macros.h"
#include "crbase/synchronization/lock.h"

namespace crbase {

// This class provides a facility similar to the CRT atexit(), except that
// we control when the callbacks are executed. Under Windows for a DLL they
// happen at a really bad time and under the loader lock. This facility is
// mostly used by crbase::Singleton.
//
// The usage is simple. Early in the main() or WinMain() scope create an
// AtExitManager object on the stack:
// int main(...) {
//    crbase::AtExitManager exit_manager;
//
// }
// When the exit_manager object goes out of scope, all the registered
// callbacks and singleton destructors will be called.

class CRBASE_EXPORT AtExitManager {
 public:
  typedef void (*AtExitCallbackType)(void*);

  AtExitManager(const AtExitManager&) = delete;
  AtExitManager& operator=(const AtExitManager&) = delete;

  AtExitManager();

  // The dtor calls all the registered callbacks. Do not try to register more
  // callbacks after this point.
  ~AtExitManager();

  // Registers the specified function to be called at exit. The prototype of
  // the callback function is void func(void*).
  static void RegisterCallback(AtExitCallbackType func, void* param);

  // Registers the specified task to be called at exit.
  static void RegisterTask(crbase::OnceClosure task);

  // Calls the functions registered with RegisterCallback in LIFO order. It
  // is possible to register new callbacks after calling this function.
  static void ProcessCallbacksNow();

 protected:
  // This constructor will allow this instance of AtExitManager to be created
  // even if one already exists.  This should only be used for testing!
  // AtExitManagers are kept on a global stack, and it will be removed during
  // destruction.  This allows you to shadow another AtExitManager.
  explicit AtExitManager(bool shadow);

 private:
  crbase::Lock lock_;
  std::stack<crbase::OnceClosure> stack_;
  AtExitManager* next_manager_;  // Stack of managers to allow shadowing.
};
}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_AT_EXIT_H_