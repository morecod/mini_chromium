// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRIPC_IPC_CHANNEL_READER_H_
#define MINI_CHROMIUM_SRC_CRIPC_IPC_CHANNEL_READER_H_

#include <stddef.h>

#include <set>

#include "crbase/macros.h"
#include "crbase/memory/scoped_vector.h"
#include "cripc/ipc_channel.h"
#include "cripc/ipc_export.h"

namespace cripc {
namespace internal {

// This class provides common pipe reading functionality for the
// platform-specific IPC channel implementations.
//
// It does the common input buffer management and message dispatch, while the
// platform-specific parts provide the pipe management through a virtual
// interface implemented on a per-platform basis.
//
// Note that there is no "writer" corresponding to this because the code for
// writing to the channel is much simpler and has very little common
// functionality that would benefit from being factored out. If we add
// something like that in the future, it would be more appropriate to add it
// here (and rename appropriately) rather than writing a different class.
class CRIPC_EXPORT ChannelReader {
 public:
  explicit ChannelReader(Listener* listener);
  ChannelReader(const ChannelReader&) = delete;
  ChannelReader& operator=(const ChannelReader&) = delete;
  virtual ~ChannelReader();

  void set_listener(Listener* listener) { listener_ = listener; }

  // This type is returned by ProcessIncomingMessages to indicate the effect of
  // the method.
  enum DispatchState {
    // All messages were successfully dispatched, or there were no messages to
    // dispatch.
    DISPATCH_FINISHED,
    // There was a channel error.
    DISPATCH_ERROR,
    // Dispatching messages is blocked on receiving more information from the
    // broker.
    DISPATCH_WAITING_ON_BROKER,
  };

  // Call to process messages received from the IPC connection and dispatch
  // them.
  DispatchState ProcessIncomingMessages();

  // Handles asynchronously read data.
  //
  // Optionally call this after returning READ_PENDING from ReadData to
  // indicate that buffer was filled with the given number of bytes of
  // data. See ReadData for more.
  DispatchState AsyncReadComplete(int bytes_read);

  // Returns true if the given message is internal to the IPC implementation,
  // like the "hello" message sent on channel set-up.
  bool IsInternalMessage(const Message& m);

  // Returns true if the given message is an Hello message
  // sent on channel set-up.
  bool IsHelloMessage(const Message& m);

 protected:
  enum ReadState { READ_SUCCEEDED, READ_FAILED, READ_PENDING };

  Listener* listener() const { return listener_; }

  // Subclasses should call this method in their destructor to give this class a
  // chance to clean up state that might be dependent on subclass members.
  void CleanUp();

  // Populates the given buffer with data from the pipe.
  //
  // Returns the state of the read. On READ_SUCCESS, the number of bytes
  // read will be placed into |*bytes_read| (which can be less than the
  // buffer size). On READ_FAILED, the channel will be closed.
  //
  // If the return value is READ_PENDING, it means that there was no data
  // ready for reading. The implementation is then responsible for either
  // calling AsyncReadComplete with the number of bytes read into the
  // buffer, or ProcessIncomingMessages to try the read again (depending
  // on whether the platform's async I/O is "try again" or "write
  // asynchronously into your buffer").
  virtual ReadState ReadData(char* buffer, int buffer_len, int* bytes_read) = 0;

  // Loads the required file desciptors into the given message. Returns true
  // on success. False means a fatal channel error.
  //
  // This will read from the input_fds_ and read more handles from the FD
  // pipe if necessary.
  virtual bool ShouldDispatchInputMessage(Message* msg) = 0;

  // Performs post-dispatch checks. Called when all input buffers are empty,
  // though there could be more data ready to be read from the OS.
  virtual bool DidEmptyInputBuffers() = 0;

  // Handles internal messages, like the hello message sent on channel startup.
  virtual void HandleInternalMessage(const Message& msg) = 0;

  // Exposed for testing purposes only.
  cr::ScopedVector<Message>* get_queued_messages() {
    return &queued_messages_;
  }

  // Exposed for testing purposes only.
  virtual void DispatchMessage(Message* m);

  // Get the process ID for the sender of the message.
  virtual cr::ProcessId GetSenderPID() = 0;

 private:
  // Takes the data received from the IPC channel and translates it into
  // Messages. Complete messages are passed to HandleTranslatedMessage().
  // Returns |false| on unrecoverable error.
  bool TranslateInputData(const char* input_data, int input_data_len);

  // Internal messages and messages bound for the attachment broker are
  // immediately dispatched. Other messages are passed to
  // HandleExternalMessage().
  // Returns |false| on unrecoverable error.
  bool HandleTranslatedMessage(Message* translated_message);

  // a deep copy of the message is added to |queued_messages_|.
  bool HandleExternalMessage(Message* external_message);

  // If there was a dispatch error, informs |listener_|.
  void HandleDispatchError(const Message& message);

  // Emits logging associated with a Message that is about to be dispatched.
  void EmitLogBeforeDispatch(const Message& message);

  // Dispatches messages from queued_messages_ to listeners. Successfully
  // dispatched messages are removed from queued_messages_.
  DispatchState DispatchMessages();

  // Checks that |size| is a valid message size. Has side effects if it's not.
  bool CheckMessageSize(size_t size);

  Listener* listener_;

  // We read from the pipe into this buffer. Managed by DispatchInputData, do
  // not access directly outside that function.
  char input_buf_[Channel::kReadBufferSize];

  // Large messages that span multiple pipe buffers, get built-up using
  // this buffer.
  std::string input_overflow_buf_;

  // Maximum overflow buffer size, see Channel::kMaximumReadBufferSize.
  // This is not a constant because we update it to reflect the reality
  // of std::string::reserve() implementation.
  size_t max_input_buffer_size_;

  // These messages are waiting to be dispatched. If this vector is non-empty,
  // then the front Message must be blocked on receiving an attachment from the
  // AttachmentBroker.
  cr::ScopedVector<Message> queued_messages_;
};

}  // namespace internal
}  // namespace cripc

#endif  // MINI_CHROMIUM_SRC_CRIPC_IPC_CHANNEL_READER_H_