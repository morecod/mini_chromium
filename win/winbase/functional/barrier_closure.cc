// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "winbase\functional\barrier_closure.h"

#include <utility>

#include "winbase\atomic\atomic_ref_count.h"
#include "winbase\functional\bind.h"
#include "winbase\memory\ptr_util.h"

namespace winbase {
namespace {

// Maintains state for a BarrierClosure.
class BarrierInfo {
 public:
  BarrierInfo(int num_callbacks_left, OnceClosure done_closure);
  void Run();

 private:
  AtomicRefCount num_callbacks_left_;
  OnceClosure done_closure_;
};

BarrierInfo::BarrierInfo(int num_callbacks, OnceClosure done_closure)
    : num_callbacks_left_(num_callbacks),
      done_closure_(std::move(done_closure)) {}

void BarrierInfo::Run() {
  WINBASE_DCHECK(!num_callbacks_left_.IsZero());
  if (!num_callbacks_left_.Decrement())
    std::move(done_closure_).Run();
}

}  // namespace

RepeatingClosure BarrierClosure(int num_callbacks_left,
                                OnceClosure done_closure) {
  WINBASE_DCHECK_GE(num_callbacks_left, 0);

  if (num_callbacks_left == 0)
    std::move(done_closure).Run();

  return BindRepeating(
      &BarrierInfo::Run,
      Owned(new BarrierInfo(num_callbacks_left, std::move(done_closure))));
}

}  // namespace winbase