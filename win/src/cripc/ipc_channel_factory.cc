// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cripc/ipc_channel_factory.h"

#include "crbase/macros.h"

namespace cripc {

namespace {

class PlatformChannelFactory : public ChannelFactory {
 public:
  PlatformChannelFactory(const PlatformChannelFactory&) = delete;
  PlatformChannelFactory& operator=(const PlatformChannelFactory&) = delete;

  PlatformChannelFactory(ChannelHandle handle, Channel::Mode mode)
      : handle_(handle), mode_(mode) {}

  std::string GetName() const override {
    return handle_.name;
  }

  std::unique_ptr<Channel> BuildChannel(Listener* listener) override {
    return Channel::Create(handle_, mode_, listener);
  }

 private:
  ChannelHandle handle_;
  Channel::Mode mode_;
};

} // namespace

// static
std::unique_ptr<ChannelFactory> ChannelFactory::Create(
    const ChannelHandle& handle,
    Channel::Mode mode) {
  return std::unique_ptr<ChannelFactory>(
      new PlatformChannelFactory(handle, mode));
}

}  // namespace cripc