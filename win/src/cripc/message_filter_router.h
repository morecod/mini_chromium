// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRIPC_MESSAGE_FILTER_ROUTER_H_
#define MINI_CHROMIUM_SRC_CRIPC_MESSAGE_FILTER_ROUTER_H_

#include <vector>

///#include "cripc/ipc_message_start.h"

namespace cripc {

class Message;
class MessageFilter;

class MessageFilterRouter {
 public:
  typedef std::vector<MessageFilter*> MessageFilters;

  MessageFilterRouter();
  ~MessageFilterRouter();

  void AddFilter(MessageFilter* filter);
  void RemoveFilter(MessageFilter* filter);
  bool TryFilters(const Message& message);
  void Clear();

 private:
  // List of global and selective filters; a given filter will exist in either
  // |message_global_filters_| OR |message_class_filters_|, but not both.
  // Note that |message_global_filters_| will be given first offering of any
  // given message.  It's the filter implementer and installer's
  // responsibility to ensure that a filter is either global or selective to
  // ensure proper message filtering order.
  MessageFilters global_filters_;
  ///MessageFilters message_class_filters_[/*LastIPCMsgStart*/];
};

}  // namespace cripc

#endif  // MINI_CHROMIUM_SRC_CRIPC_MESSAGE_FILTER_ROUTER_H_