// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _SCL_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif

#include "crbase/logging.h"

#include <limits.h>
#include <stdint.h>

#include "crbase/macros.h"
#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_OS_WIN)
#include <io.h>
#include <windows.h>

#include "crbase/files/file_path.h"
#include "crbase/files/file_util.h"

typedef HANDLE FileHandle;
typedef HANDLE MutexHandle;
// Windows warns on using write().  It prefers _write().
#define write(fd, buf, count) _write(fd, buf, static_cast<unsigned int>(count))
// Windows doesn't define STDERR_FILENO.  Define it here.
#define STDERR_FILENO 2

#elif defined(MINI_CHROMIUM_OS_POSIX)
#include <sys/syscall.h>
#include <time.h>
#endif


#if defined(MINI_CHROMIUM_OS_POSIX)
#include <errno.h>
#include <paths.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define MAX_PATH PATH_MAX
typedef FILE* FileHandle;
typedef pthread_mutex_t* MutexHandle;
#endif

#include <algorithm>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <ostream>
#include <string>

#include "crbase/debug/alias.h"
#include "crbase/debug/debugger.h"
#include "crbase/debug/stack_trace.h"
#include "crbase/strings/string_piece.h"
#include "crbase/strings/string_util.h"
#include "crbase/strings/stringprintf.h"
#include "crbase/strings/sys_string_conversions.h"
#include "crbase/strings/utf_string_conversions.h"
#include "crbase/synchronization/lock_impl.h"
#include "crbase/threading/platform_thread.h"
#include "crbase/posix/eintr_wrapper.h"
#include "crbase/immediate_crash.h"

#if defined(MINI_CHROMIUM_OS_POSIX)
#include "crbase/posix/safe_strerror.h"
#endif

namespace cr_logging {

namespace {

const char* const log_severity_names[LOG_NUM_SEVERITIES] = {
  "INFO", "WARNING", "ERROR", "FATAL" };

const char* log_severity_name(int severity) {
  if (severity >= 0 && severity < LOG_NUM_SEVERITIES)
    return log_severity_names[severity];
  return "UNKNOWN";
}

int g_min_log_level = 0;

LoggingDestination g_logging_destination = LOG_DEFAULT;

// For LOG_ERROR and above, always print to stderr.
const int kAlwaysPrintErrorLevel = LOG_ERROR;

// Which log file to use? This is initialized by InitLogging or
// will be lazily initialized to the default value when it is
// first needed.
#if defined(MINI_CHROMIUM_OS_WIN)
typedef std::wstring PathString;
#else
typedef std::string PathString;
#endif
PathString* g_log_file_name = nullptr;

// This file is lazily opened and the handle may be nullptr
FileHandle g_log_file = nullptr;

// What should be prepended to each message?
bool g_log_process_id = false;
bool g_log_thread_id = false;
bool g_log_timestamp = true;
bool g_log_tickcount = false;

// Should we pop up fatal debug messages in a dialog?
bool show_error_dialogs = false;

// An assert handler override specified by the client to be called instead of
// the debug message dialog and process termination.
LogAssertHandlerFunction log_assert_handler = nullptr;
// A log message handler that gets notified of every log message we process.
LogMessageHandlerFunction log_message_handler = nullptr;

// Helper functions to wrap platform differences.

int32_t CurrentProcessId() {
#if defined(MINI_CHROMIUM_OS_WIN)
  return GetCurrentProcessId();
#elif defined(MINI_CHROMIUM_OS_POSIX)
  return getpid();
#endif
}

uint64_t TickCount() {
#if defined(MINI_CHROMIUM_OS_WIN)
  return GetTickCount();
#elif defined(MINI_CHROMIUM_OS_POSIX)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  uint64_t absolute_micro = static_cast<int64_t>(ts.tv_sec) * 1000000 +
                            static_cast<int64_t>(ts.tv_nsec) / 1000;

  return absolute_micro;
#endif
}

void DeleteFilePath(const PathString& log_name) {
#if defined(MINI_CHROMIUM_OS_WIN)
  DeleteFileW(log_name.c_str());
#else
  unlink(log_name.c_str());
#endif
}

PathString GetDefaultLogFile() {
#if defined(MINI_CHROMIUM_OS_WIN)
  // On Windows we use the same path as the exe.
  wchar_t module_name[MAX_PATH];
  ::GetModuleFileNameW(nullptr, module_name, MAX_PATH);

  PathString log_name = module_name;
  PathString::size_type last_backslash = log_name.rfind('\\', log_name.size());
  if (last_backslash != PathString::npos)
    log_name.erase(last_backslash + 1);
  log_name += L"debug.log";
  return log_name;
#elif defined(MINI_CHROMIUM_OS_POSIX)
  // On other platforms we just use the current directory.
  return PathString("debug.log");
#endif
}

#if !defined(MINI_CHROMIUM_OS_WIN)
// This class acts as a wrapper for locking the logging files.
// LoggingLock::Init() should be called from the main thread before any logging
// is done. Then whenever logging, be sure to have a local LoggingLock
// instance on the stack. This will ensure that the lock is unlocked upon
// exiting the frame.
// LoggingLocks can not be nested.
class LoggingLock {
 public:
  LoggingLock() {
    LockLogging();
  }

  ~LoggingLock() {
    UnlockLogging();
  }

  static void Init(LogLockingState lock_log, const PathChar* new_log_file) {
    if (initialized)
      return;
    lock_log_file = lock_log;

    if (lock_log_file != LOCK_LOG_FILE)
      log_lock = new base::internal::LockImpl();

    initialized = true;
  }

 private:
  static void LockLogging() {
    if (lock_log_file == LOCK_LOG_FILE) {
#if defined(MINI_CHROMIUM_OS_POSIX)
      pthread_mutex_lock(&log_mutex);
#endif
    } else {
      // use the lock
      log_lock->Lock();
    }
  }

  static void UnlockLogging() {
    if (lock_log_file == LOCK_LOG_FILE) {
#if defined(MINI_CHROMIUM_OS_POSIX)
      pthread_mutex_unlock(&log_mutex);
#endif
    } else {
      log_lock->Unlock();
    }
  }

  // The lock is used if log file locking is false. It helps us avoid problems
  // with multiple threads writing to the log file at the same time.  Use
  // LockImpl directly instead of using Lock, because Lock makes logging calls.
  static cr::internal::LockImpl* log_lock;

  // When we don't use a lock, we are using a global mutex. We need to do this
  // because LockFileEx is not thread safe.
#if defined(MINI_CHROMIUM_OS_POSIX)
  static pthread_mutex_t log_mutex;
#endif

  static bool initialized;
  static LogLockingState lock_log_file;
};

// static
bool LoggingLock::initialized = false;
// static
cr::internal::LockImpl* LoggingLock::log_lock = nullptr;
// static
LogLockingState LoggingLock::lock_log_file = LOCK_LOG_FILE;

#if defined(MINI_CHROMIUM_OS_POSIX)
pthread_mutex_t LoggingLock::log_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#endif  // MINI_CHROMIUM_OS_WIN


// Called by logging functions to ensure that |g_log_file| is initialized
// and can be used for writing. Returns false if the file could not be
// initialized. |g_log_file| will be nullptr in this case.
bool InitializeLogFileHandle() {
  if (g_log_file)
    return true;

  if (!g_log_file_name) {
    // Nobody has called InitLogging to specify a debug log file, so here we
    // initialize the log file name to a default.
    g_log_file_name = new PathString(GetDefaultLogFile());
  }

  if ((g_logging_destination & LOG_TO_FILE) != 0) {
#if defined(MINI_CHROMIUM_OS_WIN)
    // The FILE_APPEND_DATA access mask ensures that the file is atomically
    // appended to across accesses from multiple threads.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa364399(v=vs.85).aspx
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa363858(v=vs.85).aspx
    g_log_file = ::CreateFileW(g_log_file_name->c_str(), FILE_APPEND_DATA,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_log_file == INVALID_HANDLE_VALUE || g_log_file == nullptr) {
      // try the current directory
      cr::FilePath file_path;
      if (!cr::GetCurrentDirectory(&file_path))
        return false;

      *g_log_file_name = file_path.Append(
          FILE_PATH_LITERAL("debug.log")).value();

      g_log_file = ::CreateFileW(g_log_file_name->c_str(), FILE_APPEND_DATA,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                 OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (g_log_file == INVALID_HANDLE_VALUE || g_log_file == nullptr) {
        g_log_file = nullptr;
        return false;
      }
    }
#elif defined(MINI_CHROMIUM_OS_POSIX)
    g_log_file = fopen(g_log_file_name->c_str(), "a");
    if (g_log_file == nullptr)
      return false;
#endif
  }

  return true;
}

void CloseFile(FileHandle log) {
#if defined(MINI_CHROMIUM_OS_WIN)
  CloseHandle(log);
#else
  fclose(log);
#endif
}

void CloseLogFileUnlocked() {
  if (!g_log_file)
    return;

#if defined(MINI_CHROMIUM_OS_WIN)
  CloseFile(g_log_file);
#else
  fclose(log);
#endif
  g_log_file = nullptr;
}

void WriteToFd(int fd, const char* data, size_t length) {
  size_t bytes_written = 0;
  int rv;
  while (bytes_written < length) {
    rv = write(fd, data + bytes_written, length - bytes_written);
    if (rv < 0) {
      // Give up, nothing we can do now.
      break;
    }
    bytes_written += rv;
  }
}

}  // namespace

LoggingSettings::LoggingSettings()
    : logging_dest(LOG_DEFAULT),
      log_file(nullptr),
      lock_log(LOCK_LOG_FILE),
      delete_old(APPEND_TO_OLD_LOG_FILE) {}

bool CrInitLoggingImpl(const LoggingSettings& settings) {
  g_logging_destination = settings.logging_dest;

  // ignore file options unless logging to file is set.
  if ((g_logging_destination & LOG_TO_FILE) == 0)
    return true;

#if !defined(MINI_CHROMIUM_OS_WIN)
  LoggingLock::Init(settings.lock_log, settings.log_file);
  LoggingLock logging_lock;
#endif

  // Calling InitLogging twice or after some log call has already opened the
  // default log file will re-initialize to the new options.
  CloseLogFileUnlocked();

  if (!g_log_file_name)
    g_log_file_name = new PathString();
  *g_log_file_name = settings.log_file;
  if (settings.delete_old == DELETE_OLD_LOG_FILE)
    DeleteFilePath(*g_log_file_name);

  return InitializeLogFileHandle();
}

void SetMinLogLevel(int level) {
  g_min_log_level = std::min(LOG_FATAL, level);
}

int GetMinLogLevel() {
  return g_min_log_level;
}

bool ShouldCreateLogMessage(int severity) {
  if (severity < g_min_log_level)
    return false;

  // Return true here unless we know ~LogMessage won't do anything. Note that
  // ~LogMessage writes to stderr if severity_ >= kAlwaysPrintErrorLevel, even
  // when g_logging_destination is LOG_NONE.
  return g_logging_destination != LOG_NONE || log_message_handler ||
         severity >= kAlwaysPrintErrorLevel;
}

// Returns true when LOG_TO_STDERR flag is set, or |severity| is high.
// If |severity| is high then true will be returned when no log destinations are
// set, or only LOG_TO_FILE is set, since that is useful for local development
// and debugging.
bool ShouldLogToStderr(int severity) {
  if (g_logging_destination & LOG_TO_STDERR)
    return true;
  return false;
}

void SetLogItems(bool enable_process_id, bool enable_thread_id,
                 bool enable_timestamp, bool enable_tickcount) {
  g_log_process_id = enable_process_id;
  g_log_thread_id = enable_thread_id;
  g_log_timestamp = enable_timestamp;
  g_log_tickcount = enable_tickcount;
}

void SetShowErrorDialogs(bool enable_dialogs) {
  show_error_dialogs = enable_dialogs;
}

void SetLogAssertHandler(LogAssertHandlerFunction handler) {
  log_assert_handler = handler;
}

void SetLogMessageHandler(LogMessageHandlerFunction handler) {
  log_message_handler = handler;
}

LogMessageHandlerFunction GetLogMessageHandler() {
  return log_message_handler;
}

// Explicit instantiations for commonly used comparisons.
template std::string* MakeCheckOpString<int, int>(
    const int&, const int&, const char* names);
template std::string* MakeCheckOpString<unsigned long, unsigned long>(
    const unsigned long&, const unsigned long&, const char* names);
template std::string* MakeCheckOpString<unsigned long, unsigned int>(
    const unsigned long&, const unsigned int&, const char* names);
template std::string* MakeCheckOpString<unsigned int, unsigned long>(
    const unsigned int&, const unsigned long&, const char* names);
template std::string* MakeCheckOpString<std::string, std::string>(
    const std::string&, const std::string&, const char* name);

#if !defined(NDEBUG)
// Displays a message box to the user with the error message in it.
// Used for fatal messages, where we close the app simultaneously.
// This is for developers only; we don't use this in circumstances
// (like release builds) where users could see it, since users don't
// understand these messages anyway.
void DisplayDebugMessageInDialog(const std::string& str) {
  if (str.empty())
    return;

  if (!show_error_dialogs)
    return;

#if defined(MINI_CHROMIUM_OS_WIN)
  MessageBoxW(nullptr, cr::UTF8ToUTF16(str).c_str(), L"Fatal error",
              MB_OK | MB_ICONHAND | MB_TOPMOST);
#else
  // We intentionally don't implement a dialog on other platforms.
  // You can just look at stderr.
#endif  // defined(MINI_CHROMIUM_OS_WIN)
}

#endif  // !defined(NDEBUG)

#if defined(MINI_CHROMIUM_OS_WIN)
LogMessage::SaveLastError::SaveLastError() : last_error_(::GetLastError()) {
}

LogMessage::SaveLastError::~SaveLastError() {
  ::SetLastError(last_error_);
}
#endif

LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : severity_(severity), file_(file), line_(line) {
  Init(file, line);
}

LogMessage::LogMessage(const char* file, int line, const char* condition)
    : severity_(LOG_FATAL), file_(file), line_(line) {
  Init(file, line);
  stream_ << "Check failed: " << condition << ". ";
}

LogMessage::LogMessage(const char* file, int line, std::string* result)
    : severity_(LOG_FATAL), file_(file), line_(line) {
  Init(file, line);
  stream_ << "Check failed: " << *result;
  delete result;
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity,
                       std::string* result)
    : severity_(severity), file_(file), line_(line) {
  Init(file, line);
  stream_ << "Check failed: " << *result;
  delete result;
}

LogMessage::~LogMessage() {
#if !defined(NDEBUG)
  if (severity_ == LOG_FATAL && !cr::debug::BeingDebugged()) {
    // Include a stack trace on a fatal, unless a debugger is attached.
    cr::debug::StackTrace trace;
    stream_ << std::endl;  // Newline to separate from log message.
    trace.OutputToStream(&stream_);
  }
#endif  // NDEBUG

  stream_ << std::endl;
  std::string str_newline(stream_.str());

  // Give any log message handler first dibs on the message.
  if (log_message_handler &&
      log_message_handler(severity_, file_, line_,
                          message_start_, str_newline)) {
    // The handler took care of it, no further processing.
    return;
  }

  if ((g_logging_destination & LOG_TO_SYSTEM_DEBUG_LOG) != 0) {
#if defined(MINI_CHROMIUM_OS_WIN)
    OutputDebugStringA(str_newline.c_str());
#else
    cr_ignore_result(
        fwrite(str_newline.data(), str_newline.size(), 1, stderr));
    fflush(stderr);
#endif
  } else if (severity_ >= kAlwaysPrintErrorLevel &&
             !(g_logging_destination & LOG_TO_STDERR)) {
    // When we're only outputting to a log file, above a certain log level, we
    // should still output to stderr so that we can better detect and diagnose
    // problems with unit tests, especially on the buildbots.
    cr_ignore_result(
        fwrite(str_newline.data(), str_newline.size(), 1, stderr));
    fflush(stderr);
  }

  if (ShouldLogToStderr(severity_)) {
    // Not using fwrite() here, as there are crashes on Windows when CRT calls
    // malloc() internally, triggering an OOM crash. This likely means that the
    // process is close to OOM, but at least get the proper error message out,
    // and give the caller a chance to free() up some resources. For instance if
    // the calling code is:
    //
    // allocate_something();
    // if (!TryToDoSomething()) {
    //   LOG(ERROR) << "Something went wrong";
    //   free_something();
    // }
    WriteToFd(STDERR_FILENO, str_newline.data(), str_newline.size());
  }

  // write to log file
  if ((g_logging_destination & LOG_TO_FILE) != 0) {
    // We can have multiple threads and/or processes, so try to prevent them
    // from clobbering each other's writes.
    // If the client app did not call InitLogging, and the lock has not
    // been created do it now. We do this on demand, but if two threads try
    // to do this at the same time, there will be a race condition to create
    // the lock. This is why InitLogging should be called from the main
    // thread at the beginning of execution.
#if !defined(MINI_CHROMIUM_OS_WIN)
    LoggingLock::Init(LOCK_LOG_FILE, nullptr);
    LoggingLock logging_lock;
#endif
    if (/*InitializeLogFileHandle()*/g_log_file) {
#if defined(MINI_CHROMIUM_OS_WIN)
      DWORD num_written;
      WriteFile(g_log_file,
                static_cast<const void*>(str_newline.c_str()),
                static_cast<DWORD>(str_newline.length()),
                &num_written,
                nullptr);
#else
      ignore_result(fwrite(
          str_newline.data(), str_newline.size(), 1, g_log_file));
      fflush(g_log_file);
#endif
    }
  }

  if (severity_ == LOG_FATAL) {
    // Ensure the first characters of the string are on the stack so they
    // are contained in minidumps for diagnostic purposes.
    char str_stack[1024];
    str_newline.copy(str_stack, cr_arraysize(str_stack));
    cr::debug::Alias(str_stack);

    if (log_assert_handler) {
      // Make a copy of the string for the handler out of paranoia.
      log_assert_handler(std::string(stream_.str()));
    } else {
      // Don't use the string with the newline, get a fresh version to send to
      // the debug message process. We also don't display assertions to the
      // user in release mode. The enduser can't do anything with this
      // information, and displaying message boxes when the application is
      // hosed can cause additional problems.
#ifndef NDEBUG
      if (!cr::debug::BeingDebugged()) {
        // Displaying a dialog is unnecessary when debugging and can complicate
        // debugging.
        DisplayDebugMessageInDialog(stream_.str());
      }
#endif
      // Crash the process to generate a dump.
      ///cr::debug::BreakDebugger();
      CR_IMMEDIATE_CRASH();
    }
  }
}

// writes the common header info to the stream
void LogMessage::Init(const char* file, int line) {
  cr::StringPiece filename(file);
  size_t last_slash_pos = filename.find_last_of("\\/");
  if (last_slash_pos != cr::StringPiece::npos)
    filename.remove_prefix(last_slash_pos + 1);

  // TODO(darin): It might be nice if the columns were fixed width.

  stream_ <<  '[';
  if (g_log_process_id)
    stream_ << CurrentProcessId() << ':';
  if (g_log_thread_id)
    stream_ << cr::PlatformThread::CurrentId() << ':';
  if (g_log_timestamp) {
    time_t t = time(nullptr);
    struct tm local_time = {0};
#ifdef _MSC_VER
    localtime_s(&local_time, &t);
#else
    localtime_r(&t, &local_time);
#endif
    struct tm* tm_time = &local_time;
    stream_ << std::setfill('0')
            << std::setw(2) << 1 + tm_time->tm_mon
            << std::setw(2) << tm_time->tm_mday
            << '/'
            << std::setw(2) << tm_time->tm_hour
            << std::setw(2) << tm_time->tm_min
            << std::setw(2) << tm_time->tm_sec
            << ':';
  }
  if (g_log_tickcount)
    stream_ << TickCount() << ':';
  if (severity_ >= 0)
    stream_ << log_severity_name(severity_);
  else
    stream_ << "VERBOSE" << -severity_;

  stream_ << ":" << filename << "(" << line << ")] ";

  message_start_ = stream_.str().length();
}

#if defined(MINI_CHROMIUM_OS_WIN)
// This has already been defined in the header, but defining it again as DWORD
// ensures that the type used in the header is equivalent to DWORD. If not,
// the redefinition is a compile error.
typedef DWORD SystemErrorCode;
#endif

SystemErrorCode GetLastSystemErrorCode() {
#if defined(MINI_CHROMIUM_OS_WIN)
  return ::GetLastError();
#elif defined(MINI_CHROMIUM_OS_POSIX)
  return errno;
#else
#error Not implemented
#endif
}

#if defined(MINI_CHROMIUM_OS_WIN)
CRBASE_EXPORT std::string SystemErrorCodeToString(SystemErrorCode error_code) {
  const int kErrorMessageBufferSize = 256;
  char msgbuf[kErrorMessageBufferSize];
  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageA(flags, nullptr, error_code, 0, msgbuf,
                             cr_arraysize(msgbuf), nullptr);
  if (len) {
    // Messages returned by system end with line breaks.
    return cr::CollapseWhitespaceASCII(msgbuf, true) +
        cr::StringPrintf(" (0x%X)", error_code);
  }
  return cr::StringPrintf("Error (0x%X) while retrieving error. (0x%X)",
                            GetLastError(), error_code);
}
#elif defined(MINI_CHROMIUM_OS_POSIX)
CRBASE_EXPORT std::string SystemErrorCodeToString(SystemErrorCode error_code) {
  return cr::safe_strerror(error_code);
}
#else
#error Not implemented
#endif

#if defined(MINI_CHROMIUM_OS_WIN)
Win32ErrorLogMessage::Win32ErrorLogMessage(const char* file,
                                           int line,
                                           LogSeverity severity,
                                           SystemErrorCode err)
    : err_(err),
      log_message_(file, line, severity) {
}

Win32ErrorLogMessage::~Win32ErrorLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
  // We're about to crash (CR_CHECK). Put |err_| on the stack (
  // by placing it in a field) and use Alias in hopes that it makes
  // it into crash dumps.
  DWORD last_error = err_;
  cr::debug::Alias(&last_error);
}
#elif defined(MINI_CHROMIUM_OS_POSIX)
ErrnoLogMessage::ErrnoLogMessage(const char* file,
                                 int line,
                                 LogSeverity severity,
                                 SystemErrorCode err)
    : err_(err),
      log_message_(file, line, severity) {
}

ErrnoLogMessage::~ErrnoLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
}
#endif  // defined(MINI_CHROMIUM_OS_WIN)

void CloseLogFile() {
#if !defined(MINI_CHROMIUM_OS_WIN)
  LoggingLock logging_lock;
#endif
  CloseLogFileUnlocked();
}

void RawLog(int level, const char* message) {
  if (level >= g_min_log_level) {
    size_t bytes_written = 0;
    const size_t message_len = strlen(message);
    int rv;
    while (bytes_written < message_len) {
      rv = HANDLE_EINTR(
          write(STDERR_FILENO, message + bytes_written,
                message_len - bytes_written));
      if (rv < 0) {
        // Give up, nothing we can do now.
        break;
      }
      bytes_written += rv;
    }

    if (message_len > 0 && message[message_len - 1] != '\n') {
      do {
        rv = HANDLE_EINTR(write(STDERR_FILENO, "\n", 1));
        if (rv < 0) {
          // Give up, nothing we can do now.
          break;
        }
      } while (rv != 1);
    }
  }

  if (level == LOG_FATAL)
    cr::debug::BreakDebugger();
}

// This was defined at the beginning of this file.
#undef write

#if defined(MINI_CHROMIUM_OS_WIN)
bool IsLoggingToFileEnabled() {
  return g_logging_destination & LOG_TO_FILE;
}

std::wstring GetLogFileFullPath() {
  if (g_log_file_name)
    return *g_log_file_name;
  return std::wstring();
}
#endif

CRBASE_EXPORT void LogErrorNotReached(const char* file, int line) {
  LogMessage(file, line, LOG_ERROR).stream()
      << "NOTREACHED() hit.";
}

}  // namespace cr_logging

//std::ostream& std::operator<<(std::ostream& out, const wchar_t* wstr) {
//  return out << cr::WideToUTF8(wstr);
//}
