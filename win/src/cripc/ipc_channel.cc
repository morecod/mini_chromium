// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cripc/ipc_channel.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "crbase/rand_util.h"
#include "crbase/strings/stringprintf.h"
#include "crbase/atomic/atomic_sequence_num.h"

namespace {

// Global atomic used to guarantee channel IDs are unique.
cr::StaticAtomicSequenceNumber g_last_id;

}  // namespace

namespace cripc {

// static
std::string Channel::GenerateUniqueRandomChannelID() {
  // Note: the string must start with the current process id, this is how
  // some child processes determine the pid of the parent.
  //
  // This is composed of a unique incremental identifier, the process ID of
  // the creator, an identifier for the child instance, and a strong random
  // component. The strong random component prevents other processes from
  // hijacking or squatting on predictable channel names.
  int process_id = cr::GetCurrentProcId();
  return cr::StringPrintf("%d.%u.%d",
      process_id,
      g_last_id.GetNext(),
      cr::RandInt(0, std::numeric_limits<int32_t>::max()));
}

Channel::OutputElement::OutputElement(Message* message)
    : message_(message), buffer_(nullptr), length_(0) {}

Channel::OutputElement::OutputElement(void* buffer, size_t length)
    : message_(nullptr), buffer_(buffer), length_(length) {}

Channel::OutputElement::~OutputElement() {
  free(buffer_);
}

}  // namespace cripc
