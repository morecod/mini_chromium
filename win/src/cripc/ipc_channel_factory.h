// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRIPC_IPC_CHANNEL_FACTORY_H_
#define MINI_CHROMIUM_SRC_CRIPC_IPC_CHANNEL_FACTORY_H_

#include <string>
#include <vector>
#include <memory>

#include "cripc/ipc_channel.h"

namespace cripc {

// Encapsulates how a Channel is created. A ChannelFactory can be
// passed to the constructor of ChannelProxy or SyncChannel to tell them
// how to create underlying channel.
class CRIPC_EXPORT ChannelFactory {
 public:
  // Creates a factory for "native" channel built through
  // IPC::Channel::Create().
  static std::unique_ptr<ChannelFactory> Create(const ChannelHandle& handle,
                                                Channel::Mode mode);

  virtual ~ChannelFactory() { }
  virtual std::string GetName() const = 0;
  virtual std::unique_ptr<Channel> BuildChannel(Listener* listener) = 0;
};

}  // namespace cripc

#endif  // MINI_CHROMIUM_SRC_CRIPC_IPC_CHANNEL_FACTORY_H_