// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "winbase/run_loop.h"

#include "winbase\functional\bind.h"
#include "winbase\functional\callback.h"
#include "winbase\lazy_instance.h"
#include "winbase\message_loop\message_loop.h"
#include "winbase\single_thread_task_runner.h"
#include "winbase\threading\thread_local.h"
#include "winbase\threading\thread_task_runner_handle.h"

namespace winbase {

namespace {

LazyInstance<ThreadLocalPointer<RunLoop::Delegate>>::Leaky tls_delegate =
    WINBASE_LAZY_INSTANCE_INITIALIZER;

// Runs |closure| immediately if this is called on |task_runner|, otherwise
// forwards |closure| to it.
void ProxyToTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner,
                       OnceClosure closure) {
  if (task_runner->RunsTasksInCurrentSequence()) {
    std::move(closure).Run();
    return;
  }
  task_runner->PostTask(WINBASE_FROM_HERE, std::move(closure));
}

}  // namespace

RunLoop::Delegate::Delegate() {
  // The Delegate can be created on another thread. It is only bound in
  // RegisterDelegateForCurrentThread().
  WINBASE_DETACH_FROM_THREAD(bound_thread_checker_);
}

RunLoop::Delegate::~Delegate() {
  WINBASE_DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  // A RunLoop::Delegate may be destroyed before it is bound, if so it may still
  // be on its creation thread (e.g. a Thread that fails to start) and
  // shouldn't disrupt that thread's state.
  if (bound_)
    tls_delegate.Get().Set(nullptr);
}

bool RunLoop::Delegate::ShouldQuitWhenIdle() {
  return active_run_loops_.top()->quit_when_idle_received_;
}

// static
void RunLoop::RegisterDelegateForCurrentThread(Delegate* delegate) {
  // Bind |delegate| to this thread.
  WINBASE_DCHECK(!delegate->bound_);
  WINBASE_DCHECK_CALLED_ON_VALID_THREAD(delegate->bound_thread_checker_);

  // There can only be one RunLoop::Delegate per thread.
  WINBASE_DCHECK(!tls_delegate.Get().Get())
      << "Error: Multiple RunLoop::Delegates registered on the same thread.\n\n"
         "Hint: You perhaps instantiated a second "
         "MessageLoop/ScopedTaskEnvironment on a thread that already had one?";
  tls_delegate.Get().Set(delegate);
  delegate->bound_ = true;
}

RunLoop::RunLoop(Type type)
    : delegate_(tls_delegate.Get().Get()),
      type_(type),
      origin_task_runner_(ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  WINBASE_DCHECK(delegate_)
      << "A RunLoop::Delegate must be bound to this thread prior "
         "to using RunLoop.";
  WINBASE_DCHECK(origin_task_runner_);
}

RunLoop::~RunLoop() {
  // TODO(gab): Fix bad usage and enable this check, http://crbug.com/715235.
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RunLoop::Run() {
  WINBASE_DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!BeforeRun())
    return;

  // It is okay to access this RunLoop from another sequence while Run() is
  // active as this RunLoop won't touch its state until after that returns (if
  // the RunLoop's state is accessed while processing Run(), it will be re-bound
  // to the accessing sequence for the remainder of that Run() -- accessing from
  // multiple sequences is still disallowed).
  WINBASE_DETACH_FROM_SEQUENCE(sequence_checker_);

  WINBASE_DCHECK_EQ(this, delegate_->active_run_loops_.top());
  const bool application_tasks_allowed =
      delegate_->active_run_loops_.size() == 1U ||
      type_ == Type::kNestableTasksAllowed;
  delegate_->Run(application_tasks_allowed);

  // Rebind this RunLoop to the current thread after Run().
  WINBASE_DETACH_FROM_SEQUENCE(sequence_checker_);
  WINBASE_DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AfterRun();
}

void RunLoop::RunUntilIdle() {
  WINBASE_DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  quit_when_idle_received_ = true;
  Run();
}

void RunLoop::Quit() {
  // Thread-safe.

  // This can only be hit if run_loop->Quit() is called directly (QuitClosure()
  // proxies through ProxyToTaskRunner() as it can only deref its WeakPtr on
  // |origin_task_runner_|).
  if (!origin_task_runner_->RunsTasksInCurrentSequence()) {
    origin_task_runner_->PostTask(
        WINBASE_FROM_HERE, winbase::BindOnce(&RunLoop::Quit, Unretained(this)));
    return;
  }

  quit_called_ = true;
  if (running_ && delegate_->active_run_loops_.top() == this) {
    // This is the inner-most RunLoop, so quit now.
    delegate_->Quit();
  }
}

void RunLoop::QuitWhenIdle() {
  // Thread-safe.

  // This can only be hit if run_loop->QuitWhenIdle() is called directly
  // (QuitWhenIdleClosure() proxies through ProxyToTaskRunner() as it can only
  // deref its WeakPtr on |origin_task_runner_|).
  if (!origin_task_runner_->RunsTasksInCurrentSequence()) {
    origin_task_runner_->PostTask(
        WINBASE_FROM_HERE,
        winbase::BindOnce(&RunLoop::QuitWhenIdle, Unretained(this)));
    return;
  }

  quit_when_idle_received_ = true;
}

winbase::Closure RunLoop::QuitClosure() {
  // TODO(gab): Fix bad usage and enable this check, http://crbug.com/715235.
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  allow_quit_current_deprecated_ = false;

  // Need to use ProxyToTaskRunner() as WeakPtrs vended from
  // |weak_factory_| may only be accessed on |origin_task_runner_|.
  // TODO(gab): It feels wrong that QuitClosure() is bound to a WeakPtr.
  return winbase::Bind(&ProxyToTaskRunner, origin_task_runner_,
                       winbase::Bind(&RunLoop::Quit, weak_factory_.GetWeakPtr()));
}

winbase::Closure RunLoop::QuitWhenIdleClosure() {
  // TODO(gab): Fix bad usage and enable this check, http://crbug.com/715235.
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  allow_quit_current_deprecated_ = false;

  // Need to use ProxyToTaskRunner() as WeakPtrs vended from
  // |weak_factory_| may only be accessed on |origin_task_runner_|.
  // TODO(gab): It feels wrong that QuitWhenIdleClosure() is bound to a WeakPtr.
  return winbase::Bind(
      &ProxyToTaskRunner, origin_task_runner_,
      winbase::Bind(&RunLoop::QuitWhenIdle, weak_factory_.GetWeakPtr()));
}

// static
bool RunLoop::IsRunningOnCurrentThread() {
  Delegate* delegate = tls_delegate.Get().Get();
  return delegate && !delegate->active_run_loops_.empty();
}

// static
bool RunLoop::IsNestedOnCurrentThread() {
  Delegate* delegate = tls_delegate.Get().Get();
  return delegate && delegate->active_run_loops_.size() > 1;
}

// static
void RunLoop::AddNestingObserverOnCurrentThread(NestingObserver* observer) {
  Delegate* delegate = tls_delegate.Get().Get();
  WINBASE_DCHECK(delegate);
  delegate->nesting_observers_.AddObserver(observer);
}

// static
void RunLoop::RemoveNestingObserverOnCurrentThread(NestingObserver* observer) {
  Delegate* delegate = tls_delegate.Get().Get();
  WINBASE_DCHECK(delegate);
  delegate->nesting_observers_.RemoveObserver(observer);
}

// static
void RunLoop::QuitCurrentDeprecated() {
  WINBASE_DCHECK(IsRunningOnCurrentThread());
  Delegate* delegate = tls_delegate.Get().Get();
  WINBASE_DCHECK(
      delegate->active_run_loops_.top()->allow_quit_current_deprecated_)
          << "Please migrate off QuitCurrentDeprecated(), "
             "e.g. to QuitClosure().";
  delegate->active_run_loops_.top()->Quit();
}

// static
void RunLoop::QuitCurrentWhenIdleDeprecated() {
  WINBASE_DCHECK(IsRunningOnCurrentThread());
  Delegate* delegate = tls_delegate.Get().Get();
  WINBASE_DCHECK(
      delegate->active_run_loops_.top()->allow_quit_current_deprecated_)
          << "Please migrate off QuitCurrentWhenIdleDeprecated(), e.g. to "
             "QuitWhenIdleClosure().";
  delegate->active_run_loops_.top()->QuitWhenIdle();
}

// static
Closure RunLoop::QuitCurrentWhenIdleClosureDeprecated() {
  // TODO(844016): Fix callsites and enable this check, or remove the API.
  Delegate* delegate = tls_delegate.Get().Get();
  WINBASE_DCHECK(delegate->active_run_loops_.top()->allow_quit_current_deprecated_)
      << "Please migrate off QuitCurrentWhenIdleClosureDeprecated(), e.g to "
         "QuitWhenIdleClosure().";
  return Bind(&RunLoop::QuitCurrentWhenIdleDeprecated);
}

bool RunLoop::BeforeRun() {
  WINBASE_DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if WINBASE_DCHECK_IS_ON()
  WINBASE_DCHECK(delegate_->allow_running_for_testing_)
      << "RunLoop::Run() isn't allowed in the scope of a "
         "ScopedDisallowRunningForTesting. Hint: if mixing "
         "TestMockTimeTaskRunners on same thread, use TestMockTimeTaskRunner's "
         "API instead of RunLoop to drive individual task runners.";
  WINBASE_DCHECK(!run_called_);
  run_called_ = true;
#endif  // WINBASE_DCHECK_IS_ON()

  // Allow Quit to be called before Run.
  if (quit_called_)
    return false;

  auto& active_run_loops_ = delegate_->active_run_loops_;
  active_run_loops_.push(this);

  const bool is_nested = active_run_loops_.size() > 1;

  if (is_nested) {
    for (auto& observer : delegate_->nesting_observers_)
      observer.OnBeginNestedRunLoop();
    if (type_ == Type::kNestableTasksAllowed)
      delegate_->EnsureWorkScheduled();
  }

  running_ = true;
  return true;
}

void RunLoop::AfterRun() {
  WINBASE_DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  running_ = false;

  auto& active_run_loops_ = delegate_->active_run_loops_;
  WINBASE_DCHECK_EQ(active_run_loops_.top(), this);
  active_run_loops_.pop();

  RunLoop* previous_run_loop =
      active_run_loops_.empty() ? nullptr : active_run_loops_.top();

  if (previous_run_loop) {
    for (auto& observer : delegate_->nesting_observers_)
      observer.OnExitNestedRunLoop();
  }

  // Execute deferred Quit, if any:
  if (previous_run_loop && previous_run_loop->quit_called_)
    delegate_->Quit();
}

}  // namespace winbase