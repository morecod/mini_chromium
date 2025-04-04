// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cripc/ipc_sync_channel.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "crbase/functional/bind.h"
#include "crbase/lazy_instance.h"
#include "crbase/tracing/location.h"
#include "crbase/logging.h"
#include "crbase/macros.h"
#include "crbase/memory/ptr_util.h"
#include "crbase/message_loop/message_loop.h"
#include "crbase/run_loop.h"
#include "crbase/synchronization/waitable_event.h"
#include "crbase/threading/thread_local.h"
#include "crbase/threading/thread_task_runner_handle.h"
///#include "crbase/trace_event/trace_event.h"
#include "cripc/ipc_channel_factory.h"
#include "cripc/ipc_logging.h"
///#include "cripc/ipc_message_macros.h"
#include "cripc/ipc_sync_message.h"

using cr::WaitableEvent;

namespace cripc {

namespace {
///
///// A generic callback used when watching handles synchronously. Sets |*signal|
///// to true.
///void OnEventReady(bool* signal) {
///  *signal = true;
///}
///
cr::LazyInstance<std::unique_ptr<cr::WaitableEvent>>::Leaky
    g_pump_messages_event = CR_LAZY_INSTANCE_INITIALIZER;

}  // namespace

// When we're blocked in a Send(), we need to process incoming synchronous
// messages right away because it could be blocking our reply (either
// directly from the same object we're calling, or indirectly through one or
// more other channels).  That means that in SyncContext's OnMessageReceived,
// we need to process sync message right away if we're blocked.  However a
// simple check isn't sufficient, because the listener thread can be in the
// process of calling Send.
// To work around this, when SyncChannel filters a sync message, it sets
// an event that the listener thread waits on during its Send() call.  This
// allows us to dispatch incoming sync messages when blocked.  The race
// condition is handled because if Send is in the process of being called, it
// will check the event.  In case the listener thread isn't sending a message,
// we queue a task on the listener thread to dispatch the received messages.
// The messages are stored in this queue object that's shared among all
// SyncChannel objects on the same thread (since one object can receive a
// sync message while another one is blocked).

class SyncChannel::ReceivedSyncMsgQueue :
    public cr::RefCountedThreadSafe<ReceivedSyncMsgQueue> {
 public:
  // SyncChannel::WaitForReplyWithNestedMessageLoop may be re-entered, i.e. we
  // may nest waiting message loops arbitrarily deep on the SyncChannel's
  // thread. Every such operation has a corresponding WaitableEvent to be
  // watched which, when signalled for IPC completion, breaks out of the loop.
  // A reference to the innermost (i.e. topmost) watcher is held in
  // |ReceivedSyncMsgQueue::top_send_done_event_watcher_|.
  //
  // NestedSendDoneWatcher provides a simple scoper which is used by
  // WaitForReplyWithNestedMessageLoop to begin watching a new local "send done"
  // event, preserving the previous topmost state on the local stack until the
  // new inner loop is broken. If yet another subsequent nested loop is started
  // therein the process is repeated again in the new inner stack frame, and so
  // on.
  //
  // When this object is destroyed on stack unwind, the previous topmost state
  // is swapped back into |ReceivedSyncMsgQueue::top_send_done_event_watcher_|,
  // and its watch is resumed immediately.
  class NestedSendDoneWatcher {
   public:
    NestedSendDoneWatcher(const NestedSendDoneWatcher&) = delete;
    NestedSendDoneWatcher& operator=( const NestedSendDoneWatcher&) = delete;

    NestedSendDoneWatcher(SyncChannel::SyncContext* context,
                          cr::RunLoop* run_loop)
        : sync_msg_queue_(context->received_sync_msgs()),
          outer_state_(sync_msg_queue_->top_send_done_event_watcher_),
          event_(context->GetSendDoneEvent()),
          callback_(
              cr::BindOnce(
                  &SyncChannel::SyncContext::OnSendDoneEventSignaled,
                  context,
                  run_loop)) {
      sync_msg_queue_->top_send_done_event_watcher_ = this;
      if (outer_state_)
        outer_state_->StopWatching();
      StartWatching();
    }

    ~NestedSendDoneWatcher() {
      sync_msg_queue_->top_send_done_event_watcher_ = outer_state_;
      if (outer_state_)
        outer_state_->StartWatching();
    }

   private:
    void Run(WaitableEvent* event) {
      CR_DCHECK(callback_);
      std::move(callback_).Run(event);
    }

    void StartWatching() {
      watcher_.StartWatching(
          event_, cr::BindOnce(&NestedSendDoneWatcher::Run,
                                   cr::Unretained(this)));
    }

    void StopWatching() { watcher_.StopWatching(); }

    ReceivedSyncMsgQueue* const sync_msg_queue_;
    NestedSendDoneWatcher* const outer_state_;

    cr::WaitableEvent* const event_;
    cr::WaitableEventWatcher::EventCallback callback_;
    cr::WaitableEventWatcher watcher_;

    ///DISALLOW_COPY_AND_ASSIGN(NestedSendDoneWatcher);
  };

  // Returns the ReceivedSyncMsgQueue instance for this thread, creating one
  // if necessary.  Call RemoveContext on the same thread when done.
  static ReceivedSyncMsgQueue* AddContext() {
    // We want one ReceivedSyncMsgQueue per listener thread (i.e. since multiple
    // SyncChannel objects can block the same thread).
    ReceivedSyncMsgQueue* rv = lazy_tls_ptr_.Pointer()->Get();
    if (!rv) {
      rv = new ReceivedSyncMsgQueue();
      ReceivedSyncMsgQueue::lazy_tls_ptr_.Pointer()->Set(rv);
    }
    rv->listener_count_++;
    return rv;
  }

  // Prevents messages from being dispatched immediately when the dispatch event
  // is signaled. Instead, |*dispatch_flag| will be set.
  void BlockDispatch(bool* dispatch_flag) { dispatch_flag_ = dispatch_flag; }

  // Allows messages to be dispatched immediately when the dispatch event is
  // signaled.
  void UnblockDispatch() { dispatch_flag_ = nullptr; }

  // Called on IPC thread when a synchronous message or reply arrives.
  void QueueMessage(const Message& msg, SyncChannel::SyncContext* context) {
    bool was_task_pending;
    {
      cr::AutoLock auto_lock(message_lock_);

      was_task_pending = task_pending_;
      task_pending_ = true;

      // We set the event in case the listener thread is blocked (or is about
      // to). In case it's not, the PostTask dispatches the messages.
      message_queue_.push_back(QueuedMessage(new Message(msg), context));
      message_queue_version_++;
    }

    dispatch_event_.Signal();
    if (!was_task_pending) {
      listener_task_runner_->PostTask(
          CR_FROM_HERE, 
          cr::BindOnce(&ReceivedSyncMsgQueue::DispatchMessagesTask,
                           this, cr::RetainedRef(context)));
    }
  }

  void QueueReply(const Message &msg, SyncChannel::SyncContext* context) {
    received_replies_.push_back(QueuedMessage(new Message(msg), context));
  }

  // Called on the listener's thread to process any queues synchronous
  // messages.
  void DispatchMessagesTask(SyncContext* context) {
    {
      cr::AutoLock auto_lock(message_lock_);
      task_pending_ = false;
    }
    context->DispatchMessages();
  }

  // Dispatches any queued incoming sync messages. If |dispatching_context| is
  // not null, messages which target a restricted dispatch channel will only be
  // dispatched if |dispatching_context| belongs to the same restricted dispatch
  // group as that channel. If |dispatching_context| is null, all queued
  // messages are dispatched.
  void DispatchMessages(SyncContext* dispatching_context) {
    bool first_time = true;
    uint32_t expected_version = 0;
    SyncMessageQueue::iterator it;
    while (true) {
      Message* message = nullptr;
      cr::scoped_refptr<SyncChannel::SyncContext> context;
      {
        cr::AutoLock auto_lock(message_lock_);
        if (first_time || message_queue_version_ != expected_version) {
          it = message_queue_.begin();
          first_time = false;
        }
        for (; it != message_queue_.end(); it++) {
          int message_group = it->context->restrict_dispatch_group();
          if (!dispatching_context ||
              message_group == kRestrictDispatchGroup_None ||
              message_group == dispatching_context->restrict_dispatch_group()) {
            message = it->message;
            context = it->context;
            it = message_queue_.erase(it);
            message_queue_version_++;
            expected_version = message_queue_version_;
            break;
          }
        }
      }

      if (message == nullptr)
        break;
      context->OnDispatchMessage(*message);
      delete message;
    }
  }

  // SyncChannel calls this in its destructor.
  void RemoveContext(SyncContext* context) {
    cr::AutoLock auto_lock(message_lock_);

    SyncMessageQueue::iterator iter = message_queue_.begin();
    while (iter != message_queue_.end()) {
      if (iter->context.get() == context) {
        delete iter->message;
        iter = message_queue_.erase(iter);
        message_queue_version_++;
      } else {
        iter++;
      }
    }

    if (--listener_count_ == 0) {
      CR_DCHECK(lazy_tls_ptr_.Pointer()->Get());
      lazy_tls_ptr_.Pointer()->Set(nullptr);
      ///sync_dispatch_watcher_.reset();
    }
  }

  cr::WaitableEvent* dispatch_event() { return &dispatch_event_; }
  cr::SingleThreadTaskRunner* listener_task_runner() {
    return listener_task_runner_.get();
  }

  // Holds a pointer to the per-thread ReceivedSyncMsgQueue object.
  static cr::LazyInstance<cr::ThreadLocalPointer<ReceivedSyncMsgQueue>>::
      DestructorAtExit lazy_tls_ptr_;

  // Called on the ipc thread to check if we can unblock any current Send()
  // calls based on a queued reply.
  void DispatchReplies() {
    for (size_t i = 0; i < received_replies_.size(); ++i) {
      Message* message = received_replies_[i].message;
      if (received_replies_[i].context->TryToUnblockListener(message)) {
        delete message;
        received_replies_.erase(received_replies_.begin() + i);
        return;
      }
    }
  }

 private:
  friend class cr::RefCountedThreadSafe<ReceivedSyncMsgQueue>;

  // See the comment in SyncChannel::SyncChannel for why this event is created
  // as manual reset.
  ReceivedSyncMsgQueue()
      : message_queue_version_(0),
        dispatch_event_(true, false),
        listener_task_runner_(cr::ThreadTaskRunnerHandle::Get()) {
    ///sync_dispatch_watcher_->AllowWokenUpBySyncWatchOnSameThread();
  }

  ~ReceivedSyncMsgQueue() {}

  void OnDispatchEventReady() {
    if (dispatch_flag_) {
      *dispatch_flag_ = true;
      return;
    }

    // We were woken up during a sync wait, but no specific SyncChannel is
    // currently waiting. i.e., some other Mojo interface on this thread is
    // waiting for a response. Since we don't support anything analogous to
    // restricted dispatch on Mojo interfaces, in this case it's safe to
    // dispatch sync messages for any context.
    DispatchMessages(nullptr);
  }

  // Holds information about a queued synchronous message or reply.
  struct QueuedMessage {
    QueuedMessage(Message* m, SyncContext* c) : message(m), context(c) { }
    Message* message;
    cr::scoped_refptr<SyncChannel::SyncContext> context;
  };

  typedef std::list<QueuedMessage> SyncMessageQueue;
  SyncMessageQueue message_queue_;

  // Used to signal DispatchMessages to rescan
  uint32_t message_queue_version_ = 0;

  std::vector<QueuedMessage> received_replies_;

  // Signaled when we get a synchronous message that we must respond to, as the
  // sender needs its reply before it can reply to our original synchronous
  // message.
  cr::WaitableEvent dispatch_event_;
  cr::scoped_refptr<cr::SingleThreadTaskRunner> listener_task_runner_;
  cr::Lock message_lock_;
  bool task_pending_ = false;
  int listener_count_ = 0;

  // The current NestedSendDoneWatcher for this thread, if we're currently
  // in a SyncChannel::WaitForReplyWithNestedMessageLoop. See
  // NestedSendDoneWatcher comments for more details.
  NestedSendDoneWatcher* top_send_done_event_watcher_ = nullptr;

  // If not null, the address of a flag to set when the dispatch event signals,
  // in lieu of actually dispatching messages. This is used by
  // SyncChannel::WaitForReply to restrict the scope of queued messages we're
  // allowed to process while it's waiting.
  bool* dispatch_flag_ = nullptr;

  // Watches |dispatch_event_| during all sync handle watches on this thread.
  ///std::unique_ptr<mojo::SyncEventWatcher> sync_dispatch_watcher_;
};

cr::LazyInstance<cr::ThreadLocalPointer<
    SyncChannel::ReceivedSyncMsgQueue>>::DestructorAtExit
    SyncChannel::ReceivedSyncMsgQueue::lazy_tls_ptr_ =
        CR_LAZY_INSTANCE_INITIALIZER;

SyncChannel::SyncContext::SyncContext(
    Listener* listener,
    const cr::scoped_refptr<cr::SingleThreadTaskRunner>& 
        ipc_task_runner,
    WaitableEvent* shutdown_event)
    : ChannelProxy::Context(listener, ipc_task_runner),
      received_sync_msgs_(ReceivedSyncMsgQueue::AddContext()),
      shutdown_event_(shutdown_event),
      restrict_dispatch_group_(kRestrictDispatchGroup_None) {
}

void SyncChannel::SyncContext::OnSendDoneEventSignaled(
    cr::RunLoop* nested_loop,
    cr::WaitableEvent* event) {
  CR_DCHECK_EQ(GetSendDoneEvent(), event);
  nested_loop->Quit();
}

SyncChannel::SyncContext::~SyncContext() {
  while (!deserializers_.empty())
    Pop();
}

// Adds information about an outgoing sync message to the context so that
// we know how to deserialize the reply. Returns |true| if the message was added
// to the context or |false| if it was rejected (e.g. due to shutdown.)
bool SyncChannel::SyncContext::Push(SyncMessage* sync_msg) {
  // Create the tracking information for this message. This object is stored
  // by value since all members are pointers that are cheap to copy. These
  // pointers are cleaned up in the Pop() function.
  //
  // The event is created as manual reset because in between Signal and
  // OnObjectSignalled, another Send can happen which would stop the watcher
  // from being called.  The event would get watched later, when the nested
  // Send completes, so the event will need to remain set.
  cr::AutoLock auto_lock(deserializers_lock_);
  if (reject_new_deserializers_)
    return false;
  PendingSyncMsg pending(
      SyncMessage::GetMessageId(*sync_msg), sync_msg->GetReplyDeserializer(),
      new cr::WaitableEvent(true, false));
  deserializers_.push_back(pending);
  return true;
}

bool SyncChannel::SyncContext::Pop() {
  bool result;
  {
    cr::AutoLock auto_lock(deserializers_lock_);
    PendingSyncMsg msg = deserializers_.back();
    delete msg.deserializer;
    delete msg.done_event;
    msg.done_event = nullptr;
    deserializers_.pop_back();
    result = msg.send_result;
  }

  // We got a reply to a synchronous Send() call that's blocking the listener
  // thread.  However, further down the call stack there could be another
  // blocking Send() call, whose reply we received after we made this last
  // Send() call.  So check if we have any queued replies available that
  // can now unblock the listener thread.
  ipc_task_runner()->PostTask(
      CR_FROM_HERE, cr::BindOnce(&ReceivedSyncMsgQueue::DispatchReplies,
                                     received_sync_msgs_));

  return result;
}

cr::WaitableEvent* SyncChannel::SyncContext::GetSendDoneEvent() {
  cr::AutoLock auto_lock(deserializers_lock_);
  return deserializers_.back().done_event;
}

cr::WaitableEvent* SyncChannel::SyncContext::GetDispatchEvent() {
  return received_sync_msgs_->dispatch_event();
}

void SyncChannel::SyncContext::DispatchMessages() {
  received_sync_msgs_->DispatchMessages(this);
}

bool SyncChannel::SyncContext::TryToUnblockListener(const Message* msg) {
  cr::AutoLock auto_lock(deserializers_lock_);
  if (deserializers_.empty() ||
      !SyncMessage::IsMessageReplyTo(*msg, deserializers_.back().id)) {
    return false;
  }

  if (!msg->is_reply_error()) {
    bool send_result = deserializers_.back().deserializer->
        SerializeOutputParameters(*msg);
    deserializers_.back().send_result = send_result;
    CR_DLOG_IF(ERROR, !send_result) << "Couldn't deserialize reply message";
    ///DVLOG_IF(1, !send_result) << "Couldn't deserialize reply message";
  } else {
    CR_DLOG(ERROR) << "Received error reply";
    ///DVLOG(1) << "Received error reply";
  }

  cr::WaitableEvent* done_event = deserializers_.back().done_event;
  ///TRACE_EVENT_FLOW_BEGIN0(
  ///    TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
  ///    "SyncChannel::SyncContext::TryToUnblockListener", done_event);

  done_event->Signal();

  return true;
}

void SyncChannel::SyncContext::Clear() {
  CancelPendingSends();
  received_sync_msgs_->RemoveContext(this);
  Context::Clear();
}

bool SyncChannel::SyncContext::OnMessageReceived(const Message& msg) {
  // Give the filters a chance at processing this message.
  if (TryFilters(msg))
    return true;

  if (TryToUnblockListener(&msg))
    return true;

  if (msg.is_reply()) {
    received_sync_msgs_->QueueReply(msg, this);
    return true;
  }

  if (msg.should_unblock()) {
    received_sync_msgs_->QueueMessage(msg, this);
    return true;
  }

  return Context::OnMessageReceivedNoFilter(msg);
}

void SyncChannel::SyncContext::OnChannelError() {
  CancelPendingSends();
  shutdown_watcher_.StopWatching();
  Context::OnChannelError();
}

void SyncChannel::SyncContext::OnChannelOpened() {
  shutdown_watcher_.StartWatching(
      shutdown_event_,
      cr::BindOnce(
          &SyncChannel::SyncContext::OnShutdownEventSignaled,
          cr::Unretained(this)));
  Context::OnChannelOpened();
}

void SyncChannel::SyncContext::OnChannelClosed() {
  CancelPendingSends();
  shutdown_watcher_.StopWatching();
  Context::OnChannelClosed();
}

void SyncChannel::SyncContext::CancelPendingSends() {
  cr::AutoLock auto_lock(deserializers_lock_);
  reject_new_deserializers_ = true;
  PendingSyncMessageQueue::iterator iter;
  CR_DLOG(INFO) << "Canceling pending sends";

  ///DVLOG(1) << "Canceling pending sends";
  for (iter = deserializers_.begin(); iter != deserializers_.end(); iter++) {
    ///TRACE_EVENT_FLOW_BEGIN0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
    ///                        "SyncChannel::SyncContext::CancelPendingSends",
    ///                        iter->done_event);
    iter->done_event->Signal();
  }
}

void SyncChannel::SyncContext::OnShutdownEventSignaled(WaitableEvent* event) {
  CR_DCHECK_EQ(event, shutdown_event_);

  // Process shut down before we can get a reply to a synchronous message.
  // Cancel pending Send calls, which will end up setting the send done event.
  CancelPendingSends();
}

// static
std::unique_ptr<SyncChannel> SyncChannel::Create(
    const ChannelHandle& channel_handle,
    Channel::Mode mode,
    Listener* listener,
    const cr::scoped_refptr<cr::SingleThreadTaskRunner>& 
        ipc_task_runner,
    bool create_pipe_now,
    cr::WaitableEvent* shutdown_event) {
  std::unique_ptr<SyncChannel> channel =
      Create(listener, ipc_task_runner, shutdown_event);
  channel->Init(channel_handle, mode, create_pipe_now);
  return channel;
}

// static
std::unique_ptr<SyncChannel> SyncChannel::Create(
    std::unique_ptr<ChannelFactory> factory,
    Listener* listener,
    const cr::scoped_refptr<cr::SingleThreadTaskRunner>& 
        ipc_task_runner,
    bool create_pipe_now,
    cr::WaitableEvent* shutdown_event) {
  std::unique_ptr<SyncChannel> channel =
      Create(listener, ipc_task_runner, shutdown_event);
  channel->Init(std::move(factory), create_pipe_now);
  return channel;
}

// static
std::unique_ptr<SyncChannel> SyncChannel::Create(
    Listener* listener,
    const cr::scoped_refptr<cr::SingleThreadTaskRunner>& 
        ipc_task_runner,
    WaitableEvent* shutdown_event) {
  return cr::WrapUnique(
      new SyncChannel(listener, ipc_task_runner, shutdown_event));
}

SyncChannel::SyncChannel(
    Listener* listener,
    const cr::scoped_refptr<cr::SingleThreadTaskRunner>& 
        ipc_task_runner,
    WaitableEvent* shutdown_event)
    : ChannelProxy(new SyncContext(listener, ipc_task_runner, shutdown_event)) {
  // The current (listener) thread must be distinct from the IPC thread, or else
  // sending synchronous messages will deadlock.
  CR_DCHECK_NE(ipc_task_runner.get(), 
               cr::ThreadTaskRunnerHandle::Get().get());
  StartWatching();
}

SyncChannel::~SyncChannel() {
}

void SyncChannel::SetRestrictDispatchChannelGroup(int group) {
  sync_context()->set_restrict_dispatch_group(group);
}

cr::scoped_refptr<SyncMessageFilter> SyncChannel::CreateSyncMessageFilter() {
  cr::scoped_refptr<SyncMessageFilter> filter = new SyncMessageFilter(
      sync_context()->shutdown_event(),
      sync_context()->IsChannelSendThreadSafe());
  AddFilter(filter.get());
  if (!did_init())
    pre_init_sync_message_filters_.push_back(filter);
  return filter;
}

bool SyncChannel::Send(Message* message) {
///#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
///  std::string name;
///  Logging::GetInstance()->GetMessageText(
///      message->type(), &name, message, nullptr);
///  TRACE_EVENT1("ipc", "SyncChannel::Send", "name", name);
///#else
///  TRACE_EVENT2("ipc", "SyncChannel::Send",
///               "class", IPC_MESSAGE_ID_CLASS(message->type()),
///               "line", IPC_MESSAGE_ID_LINE(message->type()));
///#endif
  if (!message->is_sync()) {
    ChannelProxy::Send(message);
    return true;
  }

  SyncMessage* sync_msg = static_cast<SyncMessage*>(message);
  bool pump_messages = sync_msg->ShouldPumpMessages();

  // *this* might get deleted in WaitForReply.
  cr::scoped_refptr<SyncContext> context(sync_context());
  if (!context->Push(sync_msg)) {
    ///DVLOG(1) << "Channel is shutting down. Dropping sync message.";
    CR_DLOG(INFO) << "Channel is shutting down. Dropping sync message.";
    delete message;
    return false;
  }

  ChannelProxy::Send(message);

  // Wait for reply, or for any other incoming synchronous messages.
  // |this| might get deleted, so only call static functions at this point.
  WaitForReply(context.get(), pump_messages);

  ///TRACE_EVENT_FLOW_END0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
  ///                      "SyncChannel::Send", context->GetSendDoneEvent());

  return context->Pop();
}

void SyncChannel::WaitForReply(SyncContext* context,
                               bool pump_messages) {
  context->DispatchMessages();

  cr::WaitableEvent* pump_messages_event = nullptr;
  if (pump_messages) {
    if (!g_pump_messages_event.Get()) {
      g_pump_messages_event.Get() = cr::MakeUnique<cr::WaitableEvent>(
          true, true);
    }
    pump_messages_event = g_pump_messages_event.Get().get();
  }

  while (true) {    
    WaitableEvent* objects[] = {
      context->GetDispatchEvent(),
      context->GetSendDoneEvent(),
      pump_messages_event
    };

    size_t object_count = pump_messages_event ? 3 : 2;
    size_t result = WaitableEvent::WaitMany(objects, object_count);
    if (result == 0 /* dispatch event */) {
      // We're waiting for a reply, but we received a blocking synchronous
      // call.  We must process it or otherwise a deadlock might occur.
      context->GetDispatchEvent()->Reset();
      context->DispatchMessages();
      continue;
    }

    if (result == 2  /* pump_messages_event */)
      WaitForReplyWithNestedMessageLoop(context);  // Run a nested run loop.

    break;
  }
}

void SyncChannel::WaitForReplyWithNestedMessageLoop(SyncContext* context) {
  cr::MessageLoop::ScopedNestableTaskAllower allow(
      cr::MessageLoop::current());
  cr::RunLoop nested_loop;
  ReceivedSyncMsgQueue::NestedSendDoneWatcher watcher(context, &nested_loop);
  nested_loop.Run();
}

void SyncChannel::OnDispatchEventSignaled(cr::WaitableEvent* event) {
  CR_DCHECK_EQ(sync_context()->GetDispatchEvent(), event);
  sync_context()->GetDispatchEvent()->Reset();

  StartWatching();

  // NOTE: May delete |this|.
  sync_context()->DispatchMessages();
}

void SyncChannel::StartWatching() {
  // |dispatch_watcher_| watches the event asynchronously, only dispatching
  // messages once the listener thread is unblocked and pumping its task queue.
  // The ReceivedSyncMsgQueue also watches this event and may dispatch
  // immediately if woken up by a message which it's allowed to dispatch.
  dispatch_watcher_.StartWatching(
      sync_context()->GetDispatchEvent(),
      cr::BindOnce(&SyncChannel::OnDispatchEventSignaled,
                       cr::Unretained(this)));
}

void SyncChannel::OnChannelInit() {
  pre_init_sync_message_filters_.clear();
}

}  // namespace cripc