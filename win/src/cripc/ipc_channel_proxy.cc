// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cripc/ipc_channel_proxy.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <memory>

#include "crbase/functional/bind.h"
#include "crbase/tracing/location.h"
#include "crbase/memory/ptr_util.h"
#include "crbase/memory/ref_counted.h"
#include "crbase/threading/single_thread_task_runner.h"
#include "crbase/threading/thread_task_runner_handle.h"
#include "crbase/build_config.h"

#include "cripc/ipc_channel_factory.h"
#include "cripc/ipc_listener.h"
#include "cripc/ipc_logging.h"
///#include "cripc/ipc_message_macros.h"
#include "cripc/message_filter.h"
#include "cripc/message_filter_router.h"

namespace cripc {

//------------------------------------------------------------------------------

ChannelProxy::Context::Context(
    Listener* listener,
    const crbase::scoped_refptr<crbase::SingleThreadTaskRunner>& ipc_task_runner)
    : listener_task_runner_(crbase::ThreadTaskRunnerHandle::Get()),
      listener_(listener),
      ipc_task_runner_(ipc_task_runner),
      channel_connected_called_(false),
      channel_send_thread_safe_(false),
      message_filter_router_(new MessageFilterRouter()),
      peer_pid_(crbase::kNullProcessId) /*,
      attachment_broker_endpoint_(false)*/ {
  CR_DCHECK(ipc_task_runner_.get());
  // The Listener thread where Messages are handled must be a separate thread
  // to avoid oversubscribing the IO thread. If you trigger this error, you
  // need to either:
  // 1) Create the ChannelProxy on a different thread, or
  // 2) Just use Channel
  // Note, we currently make an exception for a NULL listener. That usage
  // basically works, but is outside the intent of ChannelProxy. This support
  // will disappear, so please don't rely on it. See crbug.com/364241
  CR_DCHECK(
      !listener || (ipc_task_runner_.get() != listener_task_runner_.get()));
}

ChannelProxy::Context::~Context() {
}

void ChannelProxy::Context::ClearIPCTaskRunner() {
  ipc_task_runner_ = NULL;
}

void ChannelProxy::Context::CreateChannel(
    std::unique_ptr<ChannelFactory> factory) {
  crbase::AutoLock l(channel_lifetime_lock_);
  CR_DCHECK(!channel_);
  channel_id_ = factory->GetName();
  channel_ = factory->BuildChannel(this);
  channel_send_thread_safe_ = channel_->IsSendThreadSafe();
  ///channel_->SetAttachmentBrokerEndpoint(attachment_broker_endpoint_);
}

bool ChannelProxy::Context::TryFilters(const Message& message) {
  CR_DCHECK(message_filter_router_);
#ifdef ENABLE_IPC_MESSAGE_LOG
  Logging* logger = Logging::GetInstance();
  if (logger->Enabled())
    logger->OnPreDispatchMessage(message);
#endif

  if (message_filter_router_->TryFilters(message)) {
    if (message.dispatch_error()) {
      listener_task_runner_->PostTask(
          CR_FROM_HERE, 
          crbase::BindOnce(&Context::OnDispatchBadMessage, this, message));
    }
#ifdef ENABLE_IPC_MESSAGE_LOG
    if (logger->Enabled())
      logger->OnPostDispatchMessage(message, channel_id_);
#endif
    return true;
  }
  return false;
}

// Called on the IPC::Channel thread
bool ChannelProxy::Context::OnMessageReceived(const Message& message) {
  // First give a chance to the filters to process this message.
  if (!TryFilters(message))
    OnMessageReceivedNoFilter(message);
  return true;
}

// Called on the IPC::Channel thread
bool ChannelProxy::Context::OnMessageReceivedNoFilter(const Message& message) {
  listener_task_runner_->PostTask(
      CR_FROM_HERE, 
      crbase::BindOnce(&Context::OnDispatchMessage, this, message));
  return true;
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelConnected(int32_t peer_pid) {
  // We cache off the peer_pid so it can be safely accessed from both threads.
  peer_pid_ = channel_->GetPeerPID();

  // Add any pending filters.  This avoids a race condition where someone
  // creates a ChannelProxy, calls AddFilter, and then right after starts the
  // peer process.  The IO thread could receive a message before the task to add
  // the filter is run on the IO thread.
  OnAddFilter();

  // See above comment about using listener_task_runner_ here.
  listener_task_runner_->PostTask(
      CR_FROM_HERE, crbase::BindOnce(&Context::OnDispatchConnected, this));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelError() {
  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelError();

  // See above comment about using listener_task_runner_ here.
  listener_task_runner_->PostTask(
      CR_FROM_HERE, crbase::BindOnce(&Context::OnDispatchError, this));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelOpened() {
  CR_DCHECK(channel_ != NULL);

  // Assume a reference to ourselves on behalf of this thread.  This reference
  // will be released when we are closed.
  AddRef();

  if (!channel_->Connect()) {
    OnChannelError();
    return;
  }

  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnFilterAdded(channel_.get());
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelClosed() {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/477117 is fixed.
  ///tracked_objects::ScopedTracker tracking_profile(
  ///    FROM_HERE_WITH_EXPLICIT_FUNCTION(
  ///        "477117 ChannelProxy::Context::OnChannelClosed"));
  // It's okay for IPC::ChannelProxy::Close to be called more than once, which
  // would result in this branch being taken.
  if (!channel_)
    return;

  for (size_t i = 0; i < filters_.size(); ++i) {
    filters_[i]->OnChannelClosing();
    filters_[i]->OnFilterRemoved();
  }

  // We don't need the filters anymore.
  message_filter_router_->Clear();
  filters_.clear();
  // We don't need the lock, because at this point, the listener thread can't
  // access it any more.
  pending_filters_.clear();

  ClearChannel();

  // Balance with the reference taken during startup.  This may result in
  // self-destruction.
  Release();
}

void ChannelProxy::Context::Clear() {
  listener_ = NULL;
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnSendMessage(std::unique_ptr<Message> message) {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/477117 is fixed.
  ///tracked_objects::ScopedTracker tracking_profile(
  ///    FROM_HERE_WITH_EXPLICIT_FUNCTION(
  ///        "477117 ChannelProxy::Context::OnSendMessage"));
  if (!channel_) {
    OnChannelClosed();
    return;
  }

  if (!channel_->Send(message.release()))
    OnChannelError();
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnAddFilter() {
  // Our OnChannelConnected method has not yet been called, so we can't be
  // sure that channel_ is valid yet. When OnChannelConnected *is* called,
  // it invokes OnAddFilter, so any pending filter(s) will be added at that
  // time.
  if (peer_pid_ == crbase::kNullProcessId)
    return;

  std::vector<crbase::scoped_refptr<MessageFilter> > new_filters;
  {
    crbase::AutoLock auto_lock(pending_filters_lock_);
    new_filters.swap(pending_filters_);
  }

  for (size_t i = 0; i < new_filters.size(); ++i) {
    filters_.push_back(new_filters[i]);

    message_filter_router_->AddFilter(new_filters[i].get());

    // The channel has already been created and connected, so we need to
    // inform the filters right now.
    new_filters[i]->OnFilterAdded(channel_.get());
    new_filters[i]->OnChannelConnected(peer_pid_);
  }
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnRemoveFilter(MessageFilter* filter) {
  if (peer_pid_ == crbase::kNullProcessId) {
    // The channel is not yet connected, so any filters are still pending.
    crbase::AutoLock auto_lock(pending_filters_lock_);
    for (size_t i = 0; i < pending_filters_.size(); ++i) {
      if (pending_filters_[i].get() == filter) {
        filter->OnFilterRemoved();
        pending_filters_.erase(pending_filters_.begin() + i);
        return;
      }
    }
    return;
  }
  if (!channel_)
    return;  // The filters have already been deleted.

  message_filter_router_->RemoveFilter(filter);

  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i].get() == filter) {
      filter->OnFilterRemoved();
      filters_.erase(filters_.begin() + i);
      return;
    }
  }

  CR_NOTREACHED() << "filter to be removed not found";
}

// Called on the listener's thread
void ChannelProxy::Context::AddFilter(MessageFilter* filter) {
  crbase::AutoLock auto_lock(pending_filters_lock_);
  pending_filters_.push_back(crbase::make_scoped_refptr(filter));
  ipc_task_runner_->PostTask(
      CR_FROM_HERE, crbase::BindOnce(&Context::OnAddFilter, this));
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchMessage(const Message& message) {
#if defined(ENABLE_CRIPC_MESSAGE_LOG)
  Logging* logger = Logging::GetInstance();
  std::string name;
  logger->GetMessageText(message.type(), &name, &message, NULL);
  ///TRACE_EVENT1("ipc", "ChannelProxy::Context::OnDispatchMessage",
  ///             "name", name);
#else
  ///TRACE_EVENT2("ipc", "ChannelProxy::Context::OnDispatchMessage",
  ///             "class", IPC_MESSAGE_ID_CLASS(message.type()),
  ///             "line", IPC_MESSAGE_ID_LINE(message.type()));
#endif

  if (!listener_)
    return;

  OnDispatchConnected();

#ifdef ENABLE_CRIPC_MESSAGE_LOG
  if (message.type() == CRIPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(message);
    return;
  }

  if (logger->Enabled())
    logger->OnPreDispatchMessage(message);
#endif

  listener_->OnMessageReceived(message);
  if (message.dispatch_error())
    listener_->OnBadMessageReceived(message);

#ifdef ENABLE_CRIPC_MESSAGE_LOG
  if (logger->Enabled())
    logger->OnPostDispatchMessage(message, channel_id_);
#endif
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchConnected() {
  if (channel_connected_called_)
    return;

  channel_connected_called_ = true;
  if (listener_)
    listener_->OnChannelConnected(peer_pid_);
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchError() {
  if (listener_)
    listener_->OnChannelError();
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchBadMessage(const Message& message) {
  if (listener_)
    listener_->OnBadMessageReceived(message);
}

void ChannelProxy::Context::ClearChannel() {
  crbase::AutoLock l(channel_lifetime_lock_);
  channel_.reset();
}

void ChannelProxy::Context::SendFromThisThread(Message* message) {
  crbase::AutoLock l(channel_lifetime_lock_);
  if (!channel_)
    return;
  CR_DCHECK(channel_->IsSendThreadSafe());
  channel_->Send(message);
}

void ChannelProxy::Context::Send(Message* message) {
  if (channel_send_thread_safe_) {
    SendFromThisThread(message);
    return;
  }

  ipc_task_runner()->PostTask(
      CR_FROM_HERE, 
      crbase::BindOnce(&ChannelProxy::Context::OnSendMessage, this,
                       crbase::Passed(crbase::WrapUnique(message))));
}

bool ChannelProxy::Context::IsChannelSendThreadSafe() const {
  return channel_send_thread_safe_;
}

//-----------------------------------------------------------------------------

// static
std::unique_ptr<ChannelProxy> ChannelProxy::Create(
    const ChannelHandle& channel_handle,
    Channel::Mode mode,
    Listener* listener,
    const crbase::scoped_refptr<crbase::SingleThreadTaskRunner>& 
        ipc_task_runner) {
  std::unique_ptr<ChannelProxy> channel(
      new ChannelProxy(listener, ipc_task_runner));
  channel->Init(channel_handle, mode, true);
  return channel;
}

// static
std::unique_ptr<ChannelProxy> ChannelProxy::Create(
    std::unique_ptr<ChannelFactory> factory,
    Listener* listener,
    const crbase::scoped_refptr<crbase::SingleThreadTaskRunner>& 
        ipc_task_runner) {
  std::unique_ptr<ChannelProxy> channel(
      new ChannelProxy(listener, ipc_task_runner));
  channel->Init(std::move(factory), true);
  return channel;
}

ChannelProxy::ChannelProxy(Context* context)
    : context_(context),
      did_init_(false) {
}

ChannelProxy::ChannelProxy(
    Listener* listener,
    const crbase::scoped_refptr<crbase::SingleThreadTaskRunner>& 
        ipc_task_runner)
    : context_(new Context(listener, ipc_task_runner)), did_init_(false) {
}

ChannelProxy::~ChannelProxy() {
  CR_DCHECK(CalledOnValidThread());

  Close();
}

void ChannelProxy::Init(const ChannelHandle& channel_handle,
                        Channel::Mode mode,
                        bool create_pipe_now) {
  Init(ChannelFactory::Create(channel_handle, mode), create_pipe_now);
}

void ChannelProxy::Init(std::unique_ptr<ChannelFactory> factory,
                        bool create_pipe_now) {
  CR_DCHECK(CalledOnValidThread());
  CR_DCHECK(!did_init_);

  if (create_pipe_now) {
    // Create the channel immediately.  This effectively sets up the
    // low-level pipe so that the client can connect.  Without creating
    // the pipe immediately, it is possible for a listener to attempt
    // to connect and get an error since the pipe doesn't exist yet.
    context_->CreateChannel(std::move(factory));
  } else {
    context_->ipc_task_runner()->PostTask(
        CR_FROM_HERE, 
        crbase::BindOnce(&Context::CreateChannel, context_,
                         crbase::Passed(&factory)));
  }

  // complete initialization on the background thread
  context_->ipc_task_runner()->PostTask(
      CR_FROM_HERE, crbase::BindOnce(&Context::OnChannelOpened, context_));

  did_init_ = true;
  OnChannelInit();
}

void ChannelProxy::Close() {
  CR_DCHECK(CalledOnValidThread());

  // Clear the backpointer to the listener so that any pending calls to
  // Context::OnDispatchMessage or OnDispatchError will be ignored.  It is
  // possible that the channel could be closed while it is receiving messages!
  context_->Clear();

  if (context_->ipc_task_runner()) {
    context_->ipc_task_runner()->PostTask(
        CR_FROM_HERE, crbase::BindOnce(&Context::OnChannelClosed, context_));
  }
}

bool ChannelProxy::Send(Message* message) {
  CR_DCHECK(did_init_);

#ifdef ENABLE_CRIPC_MESSAGE_LOG
  Logging::GetInstance()->OnSendMessage(message, context_->channel_id());
#endif

  context_->Send(message);
  return true;
}

void ChannelProxy::AddFilter(MessageFilter* filter) {
  CR_DCHECK(CalledOnValidThread());

  context_->AddFilter(filter);
}

void ChannelProxy::RemoveFilter(MessageFilter* filter) {
  CR_DCHECK(CalledOnValidThread());

  context_->ipc_task_runner()->PostTask(
      CR_FROM_HERE, 
      crbase::BindOnce(&Context::OnRemoveFilter, context_, 
                       crbase::RetainedRef(filter)));
}

void ChannelProxy::ClearIPCTaskRunner() {
  CR_DCHECK(CalledOnValidThread());

  context()->ClearIPCTaskRunner();
}

crbase::ProcessId ChannelProxy::GetPeerPID() const {
  return context_->peer_pid_;
}

///void ChannelProxy::OnSetAttachmentBrokerEndpoint() {
///  context()->set_attachment_broker_endpoint(is_attachment_broker_endpoint());
///}

void ChannelProxy::OnChannelInit() {
}

//-----------------------------------------------------------------------------

}  // namespace cripc