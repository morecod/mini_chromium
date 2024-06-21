// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRIPC_IPC_SENDER_H_
#define MINI_CHROMIUM_SRC_CRIPC_IPC_SENDER_H_

#include "crbase/process/process_handle.h"

#include "cripc/ipc_export.h"

namespace cripc {

class Message;

class CRIPC_EXPORT Sender {
 public:
  // Sends the given IPC message.  The implementor takes ownership of the
  // given Message regardless of whether or not this method succeeds.  This
  // is done to make this method easier to use.  Returns true on success and
  // false otherwise.
  virtual bool Send(Message* msg) = 0;

 protected:
  virtual ~Sender() {}
};

}  // namespace cripc

#endif  // MINI_CHROMIUM_SRC_CRIPC_IPC_SENDER_H_