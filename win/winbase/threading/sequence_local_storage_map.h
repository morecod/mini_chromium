// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINLIB_WINBASE_THREADING_SEQUENCE_LOCAL_STORAGE_MAP_H_
#define WINLIB_WINBASE_THREADING_SEQUENCE_LOCAL_STORAGE_MAP_H_

#include "winbase\base_export.h"
#include "winbase\containers\flat_map.h"
#include "winbase\macros.h"

namespace winbase {
namespace internal {

// A SequenceLocalStorageMap holds (slot_id) -> (value, destructor) items for a
// sequence. When a task runs, it is expected that a pointer to its sequence's
// SequenceLocalStorageMap is set in TLS using
// ScopedSetSequenceMapLocalStorageForCurrentThread. When a
// SequenceLocalStorageMap is destroyed, it invokes the destructors associated
// with values stored within it.
// The Get() and Set() methods should not be accessed directly.
// Use SequenceLocalStorageSlot to Get() and Set() values in the current
// sequence's SequenceLocalStorageMap.
class WINBASE_EXPORT SequenceLocalStorageMap {
 public:
  SequenceLocalStorageMap();
  SequenceLocalStorageMap(const SequenceLocalStorageMap&) = delete;
  SequenceLocalStorageMap& operator=(const SequenceLocalStorageMap&) = delete;

  ~SequenceLocalStorageMap();

  // Returns the SequenceLocalStorage bound to the current thread. It is invalid
  // to call this outside the scope of a
  // ScopedSetSequenceLocalStorageForCurrentThread.
  static SequenceLocalStorageMap& GetForCurrentThread();

  // Holds a pointer to a value alongside a destructor for this pointer.
  // Calls the destructor on the value upon destruction.
  class WINBASE_EXPORT ValueDestructorPair {
   public:
    using DestructorFunc = void(void*);

    ValueDestructorPair(void* value, DestructorFunc* destructor);
    ValueDestructorPair(const ValueDestructorPair&) = delete;
    ValueDestructorPair& operator=(const ValueDestructorPair&) = delete;
    ~ValueDestructorPair();

    ValueDestructorPair(ValueDestructorPair&& value_destructor_pair);

    ValueDestructorPair& operator=(ValueDestructorPair&& value_destructor_pair);

    void* value() const { return value_; }

   private:
    void* value_;
    DestructorFunc* destructor_;
  };

  // Returns the value stored in |slot_id| or nullptr if no value was stored.
  void* Get(int slot_id);

  // Stores |value_destructor_pair| in |slot_id|. Overwrites and destroys any
  // previously stored value.
  void Set(int slot_id, ValueDestructorPair value_destructor_pair);

 private:
  // Map from slot id to ValueDestructorPair.
  // flat_map was chosen because there are expected to be relatively few entries
  // in the map. For low number of entries, flat_map is known to perform better
  // than other map implementations.
  winbase::flat_map<int, ValueDestructorPair> sls_map_;
};

// Within the scope of this object,
// SequenceLocalStorageMap::GetForCurrentThread() will return a reference to the
// SequenceLocalStorageMap object passed to the constructor. There can be only
// one ScopedSetSequenceLocalStorageMapForCurrentThread instance per scope.
class WINBASE_EXPORT ScopedSetSequenceLocalStorageMapForCurrentThread {
 public:
  ScopedSetSequenceLocalStorageMapForCurrentThread(
      SequenceLocalStorageMap* sequence_local_storage);
  ScopedSetSequenceLocalStorageMapForCurrentThread(
      const ScopedSetSequenceLocalStorageMapForCurrentThread&) = delete;
  ScopedSetSequenceLocalStorageMapForCurrentThread& operator=(
      const ScopedSetSequenceLocalStorageMapForCurrentThread&) = delete;
  ~ScopedSetSequenceLocalStorageMapForCurrentThread();
};
}  // namespace internal
}  // namespace winbase

#endif  // WINLIB_WINBASE_THREADING_SEQUENCE_LOCAL_STORAGE_MAP_H_