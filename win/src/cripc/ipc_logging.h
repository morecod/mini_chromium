// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRIPC_IPC_LOGGING_H_
#define MINI_CHROMIUM_SRC_CRIPC_IPC_LOGGING_H_

#include <stdint.h>

#include "cripc/ipc_message.h"  // For ENABLE_CRIPC_MESSAGE_LOG.

#ifdef ENABLE_CRIPC_MESSAGE_LOG

#include <vector>
#include <memory>
#include <unordered_map>

#include "crbase/memory/singleton.h"
#include "crbase/message_loop/message_loop.h"
#include "cripc/ipc_export.h"

// Logging function. |name| is a string in ASCII and |params| is a string in
// UTF-8.
typedef void (*LogFunction)(std::string* name,
                            const cripc::Message* msg,
                            std::string* params);

typedef std::unordered_map<uint32_t, LogFunction > LogFunctionMap;

namespace cripc {

class Message;
class Sender;

// One instance per process.  Needs to be created on the main thread (the UI
// thread in the browser) but OnPreDispatchMessage/OnPostDispatchMessage
// can be called on other threads.
class CRIPC_EXPORT Logging {
 public:
  // Implemented by consumers of log messages.
  class Consumer {
   public:
    virtual void Log(const LogData& data) = 0;

   protected:
    virtual ~Consumer() {}
  };

  void SetConsumer(Consumer* consumer);

  ~Logging();
  static Logging* GetInstance();

  // Enable and Disable are NOT cross-process; they only affect the
  // current thread/process.  If you want to modify the value for all
  // processes, perhaps your intent is to call
  // g_browser_process->SetIPCLoggingEnabled().
  void Enable();
  void Disable();
  bool Enabled() const { return enabled_; }

  // Called by child processes to give the logger object the channel to send
  // logging data to the browser process.
  void SetIPCSender(Sender* sender);

  // Called in the browser process when logging data from a child process is
  // received.
  void OnReceivedLoggingMessage(const Message& message);

  void OnSendMessage(Message* message, const std::string& channel_id);
  void OnPreDispatchMessage(const Message& message);
  void OnPostDispatchMessage(const Message& message,
                             const std::string& channel_id);

  // Like the *MsgLog functions declared for each message class, except this
  // calls the correct one based on the message type automatically.  Defined in
  // ipc_logging.cc.
  static void GetMessageText(uint32_t type, std::string* name,
                             const Message* message, std::string* params);

  static void set_log_function_map(LogFunctionMap* functions) {
    log_function_map_ = functions;
  }

  static LogFunctionMap* log_function_map() {
    return log_function_map_;
  }

 private:
  typedef enum {
    ANSI_COLOR_RESET = -1,
    ANSI_COLOR_BLACK,
    ANSI_COLOR_RED,
    ANSI_COLOR_GREEN,
    ANSI_COLOR_YELLOW,
    ANSI_COLOR_BLUE,
    ANSI_COLOR_MAGENTA,
    ANSI_COLOR_CYAN,
    ANSI_COLOR_WHITE
  } ANSIColor;
  const char* ANSIEscape(ANSIColor color);
  ANSIColor DelayColor(double delay);

  friend struct cr::DefaultSingletonTraits<Logging>;
  Logging();

  void OnSendLogs();
  void Log(const LogData& data);

  bool enabled_;
  bool enabled_on_stderr_;  // only used on POSIX for now
  bool enabled_color_; // only used on POSIX for now

  std::vector<LogData> queued_logs_;
  bool queue_invoke_later_pending_;

  Sender* sender_;
  cr::MessageLoop* main_thread_;

  Consumer* consumer_;

  static LogFunctionMap* log_function_map_;
};

}  // namespace cripc

#endif // ENABLE_CRIPC_MESSAGE_LOG

#endif  // MINI_CHROMIUM_SRC_CRIPC_IPC_LOGGING_H_