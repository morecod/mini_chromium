// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINLIB_WINBASE_THREADING_THREAD_ID_NAME_MANAGER_H_
#define WINLIB_WINBASE_THREADING_THREAD_ID_NAME_MANAGER_H_

#include <map>
#include <string>

#include "winbase\base_export.h"
#include "winbase\functional\callback.h"
#include "winbase\macros.h"
#include "winbase\synchronization/lock.h"
#include "winbase\threading\platform_thread.h"

namespace winbase {

template <typename T>
struct DefaultSingletonTraits;

class WINBASE_EXPORT ThreadIdNameManager {
 public:
  static ThreadIdNameManager* GetInstance();

  static const char* GetDefaultInternedString();

  ThreadIdNameManager(const ThreadIdNameManager&) = delete;
  ThreadIdNameManager& operator=(const ThreadIdNameManager&) = delete;

  // Register the mapping between a thread |id| and |handle|.
  void RegisterThread(PlatformThreadHandle::Handle handle, PlatformThreadId id);

  // The callback is called on the thread, immediately after the name is set.
  // |name| is a pointer to a C string that is guaranteed to remain valid for
  // the duration of the process.
  using SetNameCallback = winbase::RepeatingCallback<void(const char* name)>;
  void InstallSetNameCallback(SetNameCallback callback);

  // Set the name for the current thread.
  void SetName(const std::string& name);

  // Get the name for the given id.
  const char* GetName(PlatformThreadId id);

  // Unlike |GetName|, this method using TLS and avoids touching |lock_|.
  const char* GetNameForCurrentThread();

  // Remove the name for the given id.
  void RemoveName(PlatformThreadHandle::Handle handle, PlatformThreadId id);

 private:
  friend struct DefaultSingletonTraits<ThreadIdNameManager>;

  typedef std::map<PlatformThreadId, PlatformThreadHandle::Handle>
      ThreadIdToHandleMap;
  typedef std::map<PlatformThreadHandle::Handle, std::string*>
      ThreadHandleToInternedNameMap;
  typedef std::map<std::string, std::string*> NameToInternedNameMap;

  ThreadIdNameManager();
  ~ThreadIdNameManager();

  // lock_ protects the name_to_interned_name_, thread_id_to_handle_ and
  // thread_handle_to_interned_name_ maps.
  Lock lock_;

  NameToInternedNameMap name_to_interned_name_;
  ThreadIdToHandleMap thread_id_to_handle_;
  ThreadHandleToInternedNameMap thread_handle_to_interned_name_;

  // Treat the main process specially as there is no PlatformThreadHandle.
  std::string* main_process_name_;
  PlatformThreadId main_process_id_;

  SetNameCallback set_name_callback_;
};

}  // namespace winbase

#endif  // WINLIB_WINBASE_THREADING_THREAD_ID_NAME_MANAGER_H_