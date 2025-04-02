// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRIPC_IPC_SYNC_MESSAGE_FILTER_H_
#define MINI_CHROMIUM_SRC_CRIPC_IPC_SYNC_MESSAGE_FILTER_H_

#include <set>

#include "crbase/macros.h"
#include "crbase/memory/ref_counted.h"
#include "crbase/memory/scoped_vector.h"
#include "crbase/synchronization/lock.h"

#include "cripc/ipc_sender.h"
#include "cripc/ipc_sync_message.h"
#include "cripc/message_filter.h"

namespace cr {
class SingleThreadTaskRunner;
class WaitableEvent;
}  // namespace cr

namespace cripc {
class SyncChannel;

// This MessageFilter allows sending synchronous IPC messages from a thread
// other than the listener thread associated with the SyncChannel.  It does not
// support fancy features that SyncChannel does, such as handling recursion or
// receiving messages while waiting for a response.  Note that this object can
// be used to send simultaneous synchronous messages from different threads.
class CRIPC_EXPORT SyncMessageFilter : public MessageFilter, public Sender {
 public:
  SyncMessageFilter(const SyncMessageFilter&) = delete;
  SyncMessageFilter& operator=(const SyncMessageFilter&) = delete;

  // MessageSender implementation.
  bool Send(Message* message) override;

  // MessageFilter implementation.
  void OnFilterAdded(Sender* sender) override;
  void OnChannelError() override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const Message& message) override;

 protected:
  SyncMessageFilter(cr::WaitableEvent* shutdown_event,
                    bool is_channel_send_thread_safe);

  ~SyncMessageFilter() override;

 private:
  friend class SyncChannel;

  void set_is_channel_send_thread_safe(bool is_channel_send_thread_safe) {
    is_channel_send_thread_safe_ = is_channel_send_thread_safe;
  }

  void SendOnIOThread(Message* message);
  // Signal all the pending sends as done, used in an error condition.
  void SignalAllEvents();

  // The channel to which this filter was added.
  Sender* sender_;

  // Indicates if |sender_|'s Send method is thread-safe.
  bool is_channel_send_thread_safe_;

  // The process's main thread.
  cr::scoped_refptr<cr::SingleThreadTaskRunner> listener_task_runner_;

  // The message loop where the Channel lives.
  cr::scoped_refptr<cr::SingleThreadTaskRunner> io_task_runner_;

  typedef std::set<PendingSyncMsg*> PendingSyncMessages;
  PendingSyncMessages pending_sync_messages_;

  // Messages waiting to be delivered after IO initialization.
  cr::ScopedVector<Message> pending_messages_;

  // Locks data members above.
  cr::Lock lock_;

  cr::WaitableEvent* shutdown_event_;

  ///CR_DISALLOW_COPY_AND_ASSIGN(SyncMessageFilter)
};

}  // namespace cripc

#endif  // MINI_CHROMIUM_SRC_CRIPC_IPC_SYNC_MESSAGE_FILTER_H_