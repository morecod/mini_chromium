// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_RUN_LOOP_H_
#define MINI_CHROMIUM_SRC_CRBASE_RUN_LOOP_H_

#include "crbase/base_export.h"
#include "crbase/functional/callback.h"
#include "crbase/macros.h"
#include "crbase/memory/weak_ptr.h"
#include "crbase/message_loop/message_loop.h"

namespace cr {

class MessagePumpDispatcher;

// Helper class to Run a nested MessageLoop. Please do not use nested
// MessageLoops in production code! If you must, use this class instead of
// calling MessageLoop::Run/Quit directly. RunLoop::Run can only be called once
// per RunLoop lifetime. Create a RunLoop on the stack and call Run/Quit to run
// a nested MessageLoop.
class CRBASE_EXPORT RunLoop {
 public:
  RunLoop(const RunLoop&) = delete;
  RunLoop& operator=(const RunLoop&) = delete;

  RunLoop();
  explicit RunLoop(MessagePumpDispatcher* dispatcher);
  ~RunLoop();

  // Run the current MessageLoop. This blocks until Quit is called. Before
  // calling Run, be sure to grab an AsWeakPtr or the QuitClosure in order to
  // stop the MessageLoop asynchronously. MessageLoop::QuitWhenIdle and QuitNow
  // will also trigger a return from Run, but those are deprecated.
  void Run();

  // Run the current MessageLoop until it doesn't find any tasks or messages in
  // the queue (it goes idle). WARNING: This may never return! Only use this
  // when repeating tasks such as animated web pages have been shut down.
  void RunUntilIdle();

  bool running() const { return running_; }

  // Quit an earlier call to Run(). There can be other nested RunLoops servicing
  // the same task queue (MessageLoop); Quitting one RunLoop has no bearing on
  // the others. Quit can be called before, during or after Run. If called
  // before Run, Run will return immediately when called. Calling Quit after the
  // RunLoop has already finished running has no effect.
  //
  // WARNING: You must NEVER assume that a call to Quit will terminate the
  // targetted message loop. If a nested message loop continues running, the
  // target may NEVER terminate. It is very easy to livelock (run forever) in
  // such a case.
  void Quit();

  // Convenience method to get a closure that safely calls Quit (has no effect
  // if the RunLoop instance is gone).
  //
  // Example:
  //   RunLoop run_loop;
  //   PostTask(run_loop.QuitClosure());
  //   run_loop.Run();
  RepeatingClosure QuitClosure();

 private:
  friend class MessageLoop;

  // Return false to abort the Run.
  bool BeforeRun();
  void AfterRun();

  MessageLoop* loop_;

  // Parent RunLoop or NULL if this is the top-most RunLoop.
  RunLoop* previous_run_loop_;

  MessagePumpDispatcher* dispatcher_;

  // Used to count how many nested Run() invocations are on the stack.
  int run_depth_;

  bool run_called_;
  bool quit_called_;
  bool running_;

  // Used to record that QuitWhenIdle() was called on the MessageLoop, meaning
  // that we should quit Run once it becomes idle.
  bool quit_when_idle_received_;

  // WeakPtrFactory for QuitClosure safety.
  WeakPtrFactory<RunLoop> weak_factory_;
};

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_RUN_LOOP_H_