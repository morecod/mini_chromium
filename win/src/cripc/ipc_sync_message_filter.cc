// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cripc/ipc_sync_message_filter.h"

#include "crbase/functional/bind.h"
#include "crbase/tracing/location.h"
#include "crbase/logging.h"
#include "crbase/synchronization/waitable_event.h"
#include "crbase/threading/single_thread_task_runner.h"
#include "crbase/threading/thread_task_runner_handle.h"

#include "cripc/ipc_channel.h"
#include "cripc/ipc_sync_message.h"

namespace cripc {

bool SyncMessageFilter::Send(Message* message) {
  if (!message->is_sync()) {
    {
      cr::AutoLock auto_lock(lock_);
      if (sender_ && is_channel_send_thread_safe_) {
        sender_->Send(message);
        return true;
      } else if (!io_task_runner_.get()) {
        pending_messages_.push_back(message);
        return true;
      }
    }
    io_task_runner_->PostTask(
        CR_FROM_HERE,
        cr::BindOnce(&SyncMessageFilter::SendOnIOThread, this, message));
    return true;
  }

  cr::WaitableEvent done_event(true, false);
  PendingSyncMsg pending_message(
      SyncMessage::GetMessageId(*message),
      static_cast<SyncMessage*>(message)->GetReplyDeserializer(),
      &done_event);

  {
    cr::AutoLock auto_lock(lock_);
    // Can't use this class on the main thread or else it can lead to deadlocks.
    // Also by definition, can't use this on IO thread since we're blocking it.
    if (cr::ThreadTaskRunnerHandle::IsSet()) {
      CR_DCHECK(cr::ThreadTaskRunnerHandle::Get() != listener_task_runner_);
      CR_DCHECK(cr::ThreadTaskRunnerHandle::Get() != io_task_runner_);
    }
    pending_sync_messages_.insert(&pending_message);

    if (io_task_runner_.get()) {
      io_task_runner_->PostTask(
          CR_FROM_HERE,
          cr::BindOnce(&SyncMessageFilter::SendOnIOThread, this, message));
    } else {
      pending_messages_.push_back(message);
    }
  }

  cr::WaitableEvent* events[2] = { shutdown_event_, &done_event };
  cr::WaitableEvent::WaitMany(events, 2);

  {
    cr::AutoLock auto_lock(lock_);
    delete pending_message.deserializer;
    pending_sync_messages_.erase(&pending_message);
  }

  return pending_message.send_result;
}

void SyncMessageFilter::OnFilterAdded(Sender* sender) {
  std::vector<Message*> pending_messages;
  {
    cr::AutoLock auto_lock(lock_);
    sender_ = sender;
    io_task_runner_ = cr::ThreadTaskRunnerHandle::Get();
    pending_messages_.release(&pending_messages);
  }
  for (auto* msg : pending_messages)
    SendOnIOThread(msg);
}

void SyncMessageFilter::OnChannelError() {
  cr::AutoLock auto_lock(lock_);
  sender_ = NULL;
  SignalAllEvents();
}

void SyncMessageFilter::OnChannelClosing() {
  cr::AutoLock auto_lock(lock_);
  sender_ = NULL;
  SignalAllEvents();
}

bool SyncMessageFilter::OnMessageReceived(const Message& message) {
  cr::AutoLock auto_lock(lock_);
  for (PendingSyncMessages::iterator iter = pending_sync_messages_.begin();
       iter != pending_sync_messages_.end(); ++iter) {
    if (SyncMessage::IsMessageReplyTo(message, (*iter)->id)) {
      if (!message.is_reply_error()) {
        (*iter)->send_result =
            (*iter)->deserializer->SerializeOutputParameters(message);
      }
      (*iter)->done_event->Signal();
      return true;
    }
  }

  return false;
}

SyncMessageFilter::SyncMessageFilter(cr::WaitableEvent* shutdown_event,
                                     bool is_channel_send_thread_safe)
    : sender_(NULL),
      is_channel_send_thread_safe_(is_channel_send_thread_safe),
      listener_task_runner_(cr::ThreadTaskRunnerHandle::Get()),
      shutdown_event_(shutdown_event) {
}

SyncMessageFilter::~SyncMessageFilter() {
}

void SyncMessageFilter::SendOnIOThread(Message* message) {
  if (sender_) {
    sender_->Send(message);
    return;
  }

  if (message->is_sync()) {
    // We don't know which thread sent it, but it doesn't matter, just signal
    // them all.
    cr::AutoLock auto_lock(lock_);
    SignalAllEvents();
  }

  delete message;
}

void SyncMessageFilter::SignalAllEvents() {
  lock_.AssertAcquired();
  for (PendingSyncMessages::iterator iter = pending_sync_messages_.begin();
       iter != pending_sync_messages_.end(); ++iter) {
    (*iter)->done_event->Signal();
  }
}

}  // namespace cripc