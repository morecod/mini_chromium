// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/threading/platform_thread.h"

#include <stddef.h>

#include "crbase/build_config.h"
#include "crbase/compiler_specific.h"
#include "crbase/debug/alias.h"
///#include "crbase/debug/profiler.h"
#include "crbase/logging.h"
#include "crbase/threading/thread_id_name_manager.h"
#include "crbase/threading/thread_restrictions.h"
#include "crbase/tracing/tracked_objects.h"
#include "crbase/win/scoped_handle.h"
#include "crbase/win/windows_version.h"

namespace cr {

namespace {

// The information on how to set the thread name comes from
// a MSDN article: http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
const DWORD kVCThreadNameException = 0x406D1388;

typedef struct tagTHREADNAME_INFO {
  DWORD dwType;  // Must be 0x1000.
  LPCSTR szName;  // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;  // Reserved for future use, must be zero.
} THREADNAME_INFO;

#if defined(MINI_CHROMIUM_COMPILER_GCC)
__attribute__ ((unused))
int VCThreadNameExceptionHandler(EXCEPTION_POINTERS* context) {
  CR_ALLOW_UNUSED_LOCAL(context);
  return EXCEPTION_CONTINUE_EXECUTION;
}
#endif

// This function has try handling, so it is separated out of its caller.
void SetNameInternal(PlatformThreadId thread_id, const char* name) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = thread_id;
  info.dwFlags = 0;

#if defined(MINI_CHROMIUM_COMPILER_MSVC)
  __try {
    RaiseException(kVCThreadNameException, 0, sizeof(info)/sizeof(DWORD),
                   reinterpret_cast<DWORD_PTR*>(&info));
  } __except(EXCEPTION_CONTINUE_EXECUTION) {
  }
#elif defined(MINI_CHROMIUM_COMPILER_GCC)
  __try1(VCThreadNameExceptionHandler) {
    RaiseException(kVCThreadNameException, 0, sizeof(info)/sizeof(DWORD),
                   reinterpret_cast<DWORD_PTR*>(&info));
  } __except1{
  }
#else
#error Unsupport platform.
#endif // defined(MINI_CHROMIUM_COMPILER_MSVC)

}

struct ThreadParams {
  PlatformThread::Delegate* delegate;
  bool joinable;
  ThreadPriority priority;
};

DWORD __stdcall ThreadFunc(void* params) {
  ThreadParams* thread_params = static_cast<ThreadParams*>(params);
  PlatformThread::Delegate* delegate = thread_params->delegate;
  if (!thread_params->joinable)
    cr::ThreadRestrictions::SetSingletonAllowed(false);

  if (thread_params->priority != ThreadPriority::NORMAL)
    PlatformThread::SetCurrentThreadPriority(thread_params->priority);

  // Retrieve a copy of the thread handle to use as the key in the
  // thread name mapping.
  PlatformThreadHandle::Handle platform_handle;
  BOOL did_dup = DuplicateHandle(GetCurrentProcess(),
                                 GetCurrentThread(),
                                 GetCurrentProcess(),
                                 &platform_handle,
                                 0,
                                 FALSE,
                                 DUPLICATE_SAME_ACCESS);

  win::ScopedHandle scoped_platform_handle;

  if (did_dup) {
    scoped_platform_handle.Set(platform_handle);
    ThreadIdNameManager::GetInstance()->RegisterThread(
        scoped_platform_handle.Get(),
        PlatformThread::CurrentId());
  }

  delete thread_params;
  delegate->ThreadMain();

  if (did_dup) {
    ThreadIdNameManager::GetInstance()->RemoveName(
        scoped_platform_handle.Get(),
        PlatformThread::CurrentId());
  }

  return 0;
}

// CreateThreadInternal() matches PlatformThread::CreateWithPriority(), except
// that |out_thread_handle| may be nullptr, in which case a non-joinable thread
// is created.
bool CreateThreadInternal(size_t stack_size,
                          PlatformThread::Delegate* delegate,
                          PlatformThreadHandle* out_thread_handle,
                          ThreadPriority priority) {
  unsigned int flags = 0;
  if (stack_size > 0 &&
      cr::win::GetVersion() >= cr::win::Version::XP) {
    flags = STACK_SIZE_PARAM_IS_A_RESERVATION;
  } else {
    stack_size = 0;
  }

  ThreadParams* params = new ThreadParams;
  params->delegate = delegate;
  params->joinable = out_thread_handle != nullptr;
  params->priority = priority;

  // Using CreateThread here vs _beginthreadex makes thread creation a bit
  // faster and doesn't require the loader lock to be available.  Our code will
  // have to work running on CreateThread() threads anyway, since we run code
  // on the Windows thread pool, etc.  For some background on the difference:
  //   http://www.microsoft.com/msj/1099/win32/win321099.aspx
  void* thread_handle =
      ::CreateThread(nullptr, stack_size, ThreadFunc, params, flags, nullptr);
  if (!thread_handle) {
    delete params;
    return false;
  }

  if (out_thread_handle)
    *out_thread_handle = PlatformThreadHandle(thread_handle);
  else
    CloseHandle(thread_handle);
  return true;
}

}  // namespace

// static
PlatformThreadId PlatformThread::CurrentId() {
  return ::GetCurrentThreadId();
}

// static
PlatformThreadRef PlatformThread::CurrentRef() {
  return PlatformThreadRef(::GetCurrentThreadId());
}

// static
PlatformThreadHandle PlatformThread::CurrentHandle() {
  return PlatformThreadHandle(::GetCurrentThread());
}

// static
void PlatformThread::YieldCurrentThread() {
  ::Sleep(0);
}

// static
void PlatformThread::Sleep(TimeDelta duration) {
  // When measured with a high resolution clock, Sleep() sometimes returns much
  // too early. We may need to call it repeatedly to get the desired duration.
  TimeTicks end = TimeTicks::Now() + duration;
  for (TimeTicks now = TimeTicks::Now(); now < end; now = TimeTicks::Now())
    ::Sleep(static_cast<DWORD>((end - now).InMillisecondsRoundedUp()));
}

// static
void PlatformThread::SetName(const std::string& name) {
  ThreadIdNameManager::GetInstance()->SetName(CurrentId(), name);

  // On Windows only, we don't need to tell the profiler about the "BrokerEvent"
  // thread, as it exists only in the chrome.exe image, and never spawns or runs
  // tasks (items which could be profiled).  This test avoids the notification,
  // which would also (as a side effect) initialize the profiler in this unused
  // context, including setting up thread local storage, etc.  The performance
  // impact is not terrible, but there is no reason to do initialize it.
  if (name != "BrokerEvent")
    tracked_objects::ThreadData::InitializeThreadContext(name);

  // The debugger needs to be around to catch the name in the exception.  If
  // there isn't a debugger, we are just needlessly throwing an exception.
  // If this image file is instrumented, we raise the exception anyway
  // to provide the profiler with human-readable thread names.
  if (!::IsDebuggerPresent() /*&& !cr::debug::IsBinaryInstrumented()*/)
    return;

  SetNameInternal(CurrentId(), name.c_str());
}

// static
const char* PlatformThread::GetName() {
  return ThreadIdNameManager::GetInstance()->GetName(CurrentId());
}

// static
bool PlatformThread::CreateWithPriority(size_t stack_size, Delegate* delegate,
                                        PlatformThreadHandle* thread_handle,
                                        ThreadPriority priority) {
  CR_DCHECK(thread_handle);
  return CreateThreadInternal(stack_size, delegate, thread_handle, priority);
}

// static
bool PlatformThread::CreateNonJoinable(size_t stack_size, Delegate* delegate) {
  return CreateThreadInternal(stack_size, delegate, nullptr,
                              ThreadPriority::NORMAL);
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  CR_DCHECK(thread_handle.platform_handle());

  // Wait for the thread to exit.  It should already have terminated but make
  // sure this assumption is valid.
  DWORD result = WaitForSingleObject(thread_handle.platform_handle(), INFINITE);
  if (result != WAIT_OBJECT_0) {
    // Debug info for bug 127931.
    DWORD error = GetLastError();
    debug::Alias(&error);
    debug::Alias(&result);
    CR_CHECK(false);
  }

  CloseHandle(thread_handle.platform_handle());
}

// static
void PlatformThread::SetCurrentThreadPriority(ThreadPriority priority) {
  int desired_priority = THREAD_PRIORITY_ERROR_RETURN;
  switch (priority) {
    case ThreadPriority::BACKGROUND:
      desired_priority = THREAD_PRIORITY_LOWEST;
      break;
    case ThreadPriority::NORMAL:
      desired_priority = THREAD_PRIORITY_NORMAL;
      break;
    case ThreadPriority::DISPLAY:
      desired_priority = THREAD_PRIORITY_ABOVE_NORMAL;
      break;
    case ThreadPriority::REALTIME_AUDIO:
      desired_priority = THREAD_PRIORITY_TIME_CRITICAL;
      break;
    default:
      CR_NOTREACHED() << "Unknown priority.";
      break;
  }
  CR_DCHECK_NE(desired_priority, THREAD_PRIORITY_ERROR_RETURN);

#ifndef NDEBUG
  const BOOL success =
#endif
      ::SetThreadPriority(PlatformThread::CurrentHandle().platform_handle(),
                          desired_priority);
  CR_DPLOG_IF(ERROR, !success) << "Failed to set thread priority to "
                               << desired_priority;
}

// static
ThreadPriority PlatformThread::GetCurrentThreadPriority() {
  int priority =
      ::GetThreadPriority(PlatformThread::CurrentHandle().platform_handle());
  switch (priority) {
    case THREAD_PRIORITY_LOWEST:
      return ThreadPriority::BACKGROUND;
    case THREAD_PRIORITY_NORMAL:
      return ThreadPriority::NORMAL;
    case THREAD_PRIORITY_ABOVE_NORMAL:
      return ThreadPriority::DISPLAY;
    case THREAD_PRIORITY_TIME_CRITICAL:
      return ThreadPriority::REALTIME_AUDIO;
    case THREAD_PRIORITY_ERROR_RETURN:
      CR_DPCHECK(false) << "GetThreadPriority error";  // Falls through.
    default:
      CR_NOTREACHED() << "Unexpected priority: " << priority;
      return ThreadPriority::NORMAL;
  }
}

}  // namespace cr
