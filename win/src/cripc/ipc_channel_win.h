// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRIPC_IPC_CHANNEL_WIN_H_
#define MINI_CHROMIUM_SRC_CRIPC_IPC_CHANNEL_WIN_H_

#include "cripc/ipc_channel.h"

#include <stdint.h>

#include <queue>
#include <string>
#include <memory>

#include "crbase/macros.h"
#include "crbase/memory/weak_ptr.h"
#include "crbase/message_loop/message_loop.h"
#include "crbase/win/scoped_handle.h"

#include "cripc/ipc_channel_reader.h"

namespace cripc {

class ChannelWin : public Channel,
                   public internal::ChannelReader,
                   public cr::MessageLoopForIO::IOHandler {
 public:
  // Mirror methods of Channel, see ipc_channel.h for description.
  // |broker| must outlive the newly created object.
  ChannelWin(const ChannelHandle& channel_handle,
             Mode mode,
             Listener* listener);
  ChannelWin(const ChannelWin&) = delete;
  ChannelWin& operator=(const ChannelWin&) = delete;
  ~ChannelWin() override;

  // Channel implementation
  bool Connect() override;
  void Close() override;
  bool Send(Message* message) override;
  cr::ProcessId GetPeerPID() const override;
  cr::ProcessId GetSelfPID() const override;

  static bool IsNamedServerInitialized(const std::string& channel_id);

 private:
  // ChannelReader implementation.
  ReadState ReadData(char* buffer, int buffer_len, int* bytes_read) override;
  bool ShouldDispatchInputMessage(Message* msg) override;
  bool DidEmptyInputBuffers() override;
  void HandleInternalMessage(const Message& msg) override;
  cr::ProcessId GetSenderPID() override;

  static const cr::string16 PipeName(const std::string& channel_id,
                                     uint32_t* secret);
  bool CreatePipe(const ChannelHandle &channel_handle, Mode mode);

  bool ProcessConnection();
  bool ProcessOutgoingMessages(cr::MessageLoopForIO::IOContext* context,
                               DWORD bytes_written);

  // Returns |false| on channel error.
  // If |message| has brokerable attachments, those attachments are passed to
  // the AttachmentBroker (which in turn invokes Send()), so this method must
  // be re-entrant.
  // Adds |message| to |output_queue_| and calls ProcessOutgoingMessages().
  bool ProcessMessageForDelivery(Message* message);

  // Moves all messages from |prelim_queue_| to |output_queue_| by calling
  // ProcessMessageForDelivery().
  void FlushPrelimQueue();

  // MessageLoop::IOHandler implementation.
  void OnIOCompleted(cr::MessageLoopForIO::IOContext* context,
                     DWORD bytes_transfered,
                     DWORD error) override;

 private:
  struct State {
    explicit State(ChannelWin* channel);
    ~State();
    cr::MessageLoopForIO::IOContext context;
    bool is_pending;
  };

  State input_state_;
  State output_state_;

  cr::win::ScopedHandle pipe_;

  cr::ProcessId peer_pid_;

  // Messages not yet ready to be sent are queued here. Messages removed from
  // this queue are placed in the output_queue_. The double queue is
  // unfortunate, but is necessary because messages with brokerable attachments
  // can generate multiple messages to be sent (possibly from other channels).
  // Some of these generated messages cannot be sent until |peer_pid_| has been
  // configured.
  // As soon as |peer_pid| has been configured, there is no longer any need for
  // |prelim_queue_|. All messages are flushed, and no new messages are added.
  std::queue<Message*> prelim_queue_;

  // Messages to be sent are queued here.
  std::queue<OutputElement*> output_queue_;

  // In server-mode, we have to wait for the client to connect before we
  // can begin reading.  We make use of the input_state_ when performing
  // the connect operation in overlapped mode.
  bool waiting_connect_;

  // This flag is set when processing incoming messages.  It is used to
  // avoid recursing through ProcessIncomingMessages, which could cause
  // problems.  TODO(darin): make this unnecessary
  bool processing_incoming_;

  // Determines if we should validate a client's secret on connection.
  bool validate_client_;

  // Tracks the lifetime of this object, for debugging purposes.
  uint32_t debug_flags_;

  // This is a unique per-channel value used to authenticate the client end of
  // a connection. If the value is non-zero, the client passes it in the hello
  // and the host validates. (We don't send the zero value fto preserve IPC
  // compatability with existing clients that don't validate the channel.)
  uint32_t client_secret_;

  cr::WeakPtrFactory<ChannelWin> weak_factory_;
};

}  // namespace cripc

#endif  // MINI_CHROMIUM_SRC_CRIPC_IPC_CHANNEL_WIN_H_