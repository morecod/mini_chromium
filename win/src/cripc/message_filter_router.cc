// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cripc/message_filter_router.h"

#include <stddef.h>
#include <stdint.h>

#include "crbase/macros.h"

///#include "cripc/ipc_message_macros.h"
#include "cripc/ipc_message_utils.h"
#include "cripc/message_filter.h"

namespace cripc {

namespace {

bool TryFiltersImpl(MessageFilterRouter::MessageFilters& filters,
                    const Message& message) {
  for (size_t i = 0; i < filters.size(); ++i) {
    if (filters[i]->OnMessageReceived(message)) {
      return true;
    }
  }
  return false;
}

bool RemoveFilterImpl(MessageFilterRouter::MessageFilters& filters,
                      MessageFilter* filter) {
  MessageFilterRouter::MessageFilters::iterator it =
      std::remove(filters.begin(), filters.end(), filter);
  if (it == filters.end())
    return false;

  filters.erase(it, filters.end());
  return true;
}

///bool ValidMessageClass(int message_class) {
///  return message_class >= 0 && message_class < LastIPCMsgStart;
///}

}  // namespace

MessageFilterRouter::MessageFilterRouter() {}
MessageFilterRouter::~MessageFilterRouter() {}

void MessageFilterRouter::AddFilter(MessageFilter* filter) {
  // Determine if the filter should be applied to all messages, or only
  // messages of a certain class.
  ///std::vector<uint32_t> supported_message_classes;
  ///if (filter->GetSupportedMessageClasses(&supported_message_classes)) {
  ///  CR_DCHECK(!supported_message_classes.empty());
  ///  for (size_t i = 0; i < supported_message_classes.size(); ++i) {
  ///    const int message_class = supported_message_classes[i];
  ///    CR_DCHECK(ValidMessageClass(message_class));
  ///    // Safely ignore repeated subscriptions to a given message class for the
  ///    // current filter being added.
  ///    if (!message_class_filters_[message_class].empty() &&
  ///        message_class_filters_[message_class].back() == filter) {
  ///      continue;
  ///    }
  ///    message_class_filters_[message_class].push_back(filter);
  ///  }
  ///} else {
  ///  global_filters_.push_back(filter);
  ///}

  global_filters_.push_back(filter);
}

void MessageFilterRouter::RemoveFilter(MessageFilter* filter) {
  if (RemoveFilterImpl(global_filters_, filter))
    return;

  ///for (size_t i = 0; i < crbase_arraysize(message_class_filters_); ++i)
  ///  RemoveFilterImpl(message_class_filters_[i], filter);
}

bool MessageFilterRouter::TryFilters(const Message& message) {
  if (TryFiltersImpl(global_filters_, message))
    return true;

  return false;
  ///const int message_class = IPC_MESSAGE_CLASS(message);
  ///if (!ValidMessageClass(message_class))
  ///  return false;
  ///
  ///return TryFiltersImpl(message_class_filters_[message_class], message);
}

void MessageFilterRouter::Clear() {
  global_filters_.clear();
  ///for (size_t i = 0; i < crbase_arraysize(message_class_filters_); ++i)
  ///  message_class_filters_[i].clear();
}

}  // namespace cripc