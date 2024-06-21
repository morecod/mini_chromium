// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "crbase/process/launch.h"

#include <fcntl.h>
#include <io.h>
#include <shellapi.h>
#include <windows.h>
#include <userenv.h>
#include <psapi.h>

#include <ios>
#include <limits>
#include <memory>

#include "crbase/functional/bind.h"
#include "crbase/functional/bind_helpers.h"
#include "crbase/command_line.h"
#include "crbase/debug/stack_trace.h"
#include "crbase/logging.h"
#include "crbase/message_loop/message_loop.h"
///#include "crbase/metrics/histogram.h"
#include "crbase/process/kill.h"
#include "crbase/process/process_iterator.h"
#include "crbase/strings/utf_string_conversions.h"
#include "crbase/sys_info.h"
#include "crbase/win/object_watcher.h"
#include "crbase/win/scoped_handle.h"
#include "crbase/win/scoped_process_information.h"
#include "crbase/win/startup_information.h"
#include "crbase/win/windows_version.h"

// userenv.dll is required for CreateEnvironmentBlock().
///#pragma comment(lib, "userenv.lib")

namespace crbase {


namespace {

// This exit code is used by the Windows task manager when it kills a
// process.  It's value is obviously not that unique, and it's
// surprising to me that the task manager uses this value, but it
// seems to be common practice on Windows to test for it as an
// indication that the task manager has killed something if the
// process goes away.
const DWORD kProcessKilledExitCode = 1;

bool GetAppOutputInternal(const StringPiece16& cl,
                          bool include_stderr,
                          std::string* output) {
  HANDLE out_read = NULL;
  HANDLE out_write = NULL;

  SECURITY_ATTRIBUTES sa_attr;
  // Set the bInheritHandle flag so pipe handles are inherited.
  sa_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa_attr.bInheritHandle = TRUE;
  sa_attr.lpSecurityDescriptor = NULL;

  // Create the pipe for the child process's STDOUT.
  if (!CreatePipe(&out_read, &out_write, &sa_attr, 0)) {
    CR_NOTREACHED() << "Failed to create pipe";
    return false;
  }

  // Ensure we don't leak the handles.
  win::ScopedHandle scoped_out_read(out_read);
  win::ScopedHandle scoped_out_write(out_write);

  // Ensure the read handles to the pipes are not inherited.
  if (!SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0)) {
    CR_NOTREACHED() << "Failed to disabled pipe inheritance";
    return false;
  }

  FilePath::StringType writable_command_line_string;
  writable_command_line_string.assign(cl.data(), cl.size());

  STARTUPINFOW start_info = {};

  start_info.cb = sizeof(start_info);
  start_info.hStdOutput = out_write;
  // Keep the normal stdin.
  start_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  if (include_stderr) {
    start_info.hStdError = out_write;
  } else {
    start_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  }
  start_info.dwFlags |= STARTF_USESTDHANDLES;

  // Create the child process.
  PROCESS_INFORMATION temp_process_info = {};
  if (!CreateProcessW(NULL,
                      &writable_command_line_string[0],
                      NULL, NULL,
                      TRUE,  // Handles are inherited.
                      0, NULL, NULL, &start_info, &temp_process_info)) {
    CR_NOTREACHED() << "Failed to start process";
    return false;
  }
  win::ScopedProcessInformation proc_info(temp_process_info);

  // Close our writing end of pipe now. Otherwise later read would not be able
  // to detect end of child's output.
  scoped_out_write.Close();

  // Read output from the child process's pipe for STDOUT
  const int kBufferSize = 1024;
  char buffer[kBufferSize];

  for (;;) {
    DWORD bytes_read = 0;
    BOOL success = ReadFile(out_read, buffer, kBufferSize, &bytes_read, NULL);
    if (!success || bytes_read == 0)
      break;
    output->append(buffer, bytes_read);
  }

  // Let's wait for the process to finish.
  WaitForSingleObject(proc_info.process_handle(), INFINITE);

  int exit_code;
  crbase::TerminationStatus status = GetTerminationStatus(
      proc_info.process_handle(), &exit_code);
  return status != crbase::TERMINATION_STATUS_PROCESS_CRASHED &&
         status != crbase::TERMINATION_STATUS_ABNORMAL_TERMINATION;
}

// Launches a process with elevated privileges.  This does not behave exactly
// like LaunchProcess as it uses ShellExecuteEx instead of CreateProcess to
// create the process.  This means the process will have elevated privileges
// and thus some common operations like OpenProcess will fail.
Process LaunchElevatedProcess(const CommandLine& cmdline,
                              const FilePath& current_directory,
                              bool start_hidden,
                              bool wait) {
  const string16 file = cmdline.GetProgram().value();
  const string16 arguments = cmdline.GetArgumentsString();

  SHELLEXECUTEINFOW shex_info = {};
  shex_info.cbSize = sizeof(shex_info);
  shex_info.fMask = SEE_MASK_NOCLOSEPROCESS;
  shex_info.hwnd = GetActiveWindow();
  shex_info.lpVerb = L"runas";
  shex_info.lpFile = file.c_str();
  shex_info.lpParameters = arguments.c_str();
  shex_info.lpDirectory =
      current_directory.empty() ? NULL : current_directory.value().c_str();
  shex_info.nShow = start_hidden ? SW_HIDE : SW_SHOW;
  shex_info.hInstApp = NULL;

  if (!ShellExecuteExW(&shex_info)) {
    CR_DPLOG(ERROR);
    return Process();
  }

  if (wait)
    WaitForSingleObject(shex_info.hProcess, INFINITE);

  return Process(shex_info.hProcess);
}

}  // namespace

void RouteStdioToConsole(bool create_console_if_not_found) {
  // Don't change anything if stdout or stderr already point to a
  // valid stream.
  //
  // If we are running under Buildbot or under Cygwin's default
  // terminal (mintty), stderr and stderr will be pipe handles.  In
  // that case, we don't want to open CONOUT$, because its output
  // likely does not go anywhere.
  //
  // We don't use GetStdHandle() to check stdout/stderr here because
  // it can return dangling IDs of handles that were never inherited
  // by this process.  These IDs could have been reused by the time
  // this function is called.  The CRT checks the validity of
  // stdout/stderr on startup (before the handle IDs can be reused).
  // _fileno(stdout) will return -2 (_NO_CONSOLE_FILENO) if stdout was
  // invalid.
  if (_fileno(stdout) >= 0 || _fileno(stderr) >= 0) {
    // _fileno was broken for SUBSYSTEM:WINDOWS from VS2010 to VS2012/2013.
    // http://crbug.com/358267. Confirm that the underlying HANDLE is valid
    // before aborting.

    // This causes NaCl tests to hang on XP for reasons unclear, perhaps due
    // to not being able to inherit handles. Since it's only for debugging,
    // and redirecting still works, punt for now.
    if (crbase::win::GetVersion() < crbase::win::Version::VISTA)
      return;

    intptr_t stdout_handle = _get_osfhandle(_fileno(stdout));
    intptr_t stderr_handle = _get_osfhandle(_fileno(stderr));
    if (stdout_handle >= 0 || stderr_handle >= 0)
      return;
  }

  if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
    unsigned int result = GetLastError();
    // Was probably already attached.
    if (result == ERROR_ACCESS_DENIED)
      return;
    // Don't bother creating a new console for each child process if the
    // parent process is invalid (eg: crashed).
    if (result == ERROR_GEN_FAILURE)
      return;
    if (create_console_if_not_found) {
      // Make a new console if attaching to parent fails with any other error.
      // It should be ERROR_INVALID_HANDLE at this point, which means the
      // browser was likely not started from a console.
      AllocConsole();
    } else {
      return;
    }
  }

  // Arbitrary byte count to use when buffering output lines.  More
  // means potential waste, less means more risk of interleaved
  // log-lines in output.
  enum { kOutputBufferSize = 64 * 1024 };

  if (freopen("CONOUT$", "w", stdout)) {
    setvbuf(stdout, NULL, _IOLBF, kOutputBufferSize);
    // Overwrite FD 1 for the benefit of any code that uses this FD
    // directly.  This is safe because the CRT allocates FDs 0, 1 and
    // 2 at startup even if they don't have valid underlying Windows
    // handles.  This means we won't be overwriting an FD created by
    // _open() after startup.
    _dup2(_fileno(stdout), 1);
  }
  if (freopen("CONOUT$", "w", stderr)) {
    setvbuf(stderr, NULL, _IOLBF, kOutputBufferSize);
    _dup2(_fileno(stderr), 2);
  }

  // Fix all cout, wcout, cin, wcin, cerr, wcerr, clog and wclog.
  std::ios::sync_with_stdio();
}

Process LaunchProcess(const CommandLine& cmdline,
                      const LaunchOptions& options) {
  if (options.elevated) {
    return LaunchElevatedProcess(cmdline, options.current_directory,
                                 options.start_hidden, options.wait);
  }
  return LaunchProcess(cmdline.GetCommandLineString(), options);
}

Process LaunchProcess(const string16& cmdline,
                      const LaunchOptions& options) {
  if (options.elevated) {
    return LaunchElevatedProcess(CommandLine::FromString(cmdline),
                                 options.current_directory,
                                 options.start_hidden, options.wait);
  }

  win::StartupInformation startup_info_wrapper;
  STARTUPINFOW* startup_info = startup_info_wrapper.startup_info();

  bool inherit_handles = options.inherit_handles;
  DWORD flags = 0;
  if (options.handles_to_inherit) {
    if (options.handles_to_inherit->empty()) {
      inherit_handles = false;
    } else {
      if (win::GetVersion() < win::Version::VISTA) {
        CR_DLOG(ERROR) << "Specifying handles to inherit requires Vista or later.";
        return Process();
      }

      if (options.handles_to_inherit->size() >
              std::numeric_limits<DWORD>::max() / sizeof(HANDLE)) {
        CR_DLOG(ERROR) << "Too many handles to inherit.";
        return Process();
      }

      if (!startup_info_wrapper.InitializeProcThreadAttributeList(1)) {
        CR_DLOG(ERROR);
        return Process();
      }

      if (!startup_info_wrapper.UpdateProcThreadAttribute(
              PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
              const_cast<HANDLE*>(&options.handles_to_inherit->at(0)),
              static_cast<DWORD>(options.handles_to_inherit->size() *
                  sizeof(HANDLE)))) {
        CR_DLOG(ERROR);
        return Process();
      }

      inherit_handles = true;
      flags |= EXTENDED_STARTUPINFO_PRESENT;
    }
  }

  if (options.empty_desktop_name)
    startup_info->lpDesktop = const_cast<wchar_t*>(L"");
  startup_info->dwFlags = STARTF_USESHOWWINDOW;
  startup_info->wShowWindow = options.start_hidden ? SW_HIDE : SW_SHOW;

  if (options.stdin_handle || options.stdout_handle || options.stderr_handle) {
    CR_DCHECK(inherit_handles);
    CR_DCHECK(options.stdin_handle);
    CR_DCHECK(options.stdout_handle);
    CR_DCHECK(options.stderr_handle);
    startup_info->dwFlags |= STARTF_USESTDHANDLES;
    startup_info->hStdInput = options.stdin_handle;
    startup_info->hStdOutput = options.stdout_handle;
    startup_info->hStdError = options.stderr_handle;
  }

  if (options.job_handle) {
    flags |= CREATE_SUSPENDED;

    // If this code is run under a debugger, the launched process is
    // automatically associated with a job object created by the debugger.
    // The CREATE_BREAKAWAY_FROM_JOB flag is used to prevent this on Windows
    // releases that do not support nested jobs.
    if (win::GetVersion() < win::Version::WIN8)
      flags |= CREATE_BREAKAWAY_FROM_JOB;
  }

  if (options.force_breakaway_from_job_)
    flags |= CREATE_BREAKAWAY_FROM_JOB;

  PROCESS_INFORMATION temp_process_info = {};

  LPCWSTR current_directory = options.current_directory.empty()
                                  ? nullptr
                                  : options.current_directory.value().c_str();

  string16 writable_cmdline(cmdline);
  if (options.as_user) {
    flags |= CREATE_UNICODE_ENVIRONMENT;
    void* enviroment_block = NULL;

    if (!CreateEnvironmentBlock(&enviroment_block, options.as_user, FALSE)) {
      CR_DPLOG(ERROR);
      return Process();
    }

   // Environment options are not implemented for use with |as_user|.
    CR_DCHECK(!options.clear_environment);
    CR_DCHECK(options.environment.empty());

    BOOL launched =
        CreateProcessAsUserW(options.as_user, NULL,
                             &writable_cmdline[0],
                             NULL, NULL, inherit_handles, flags,
                             enviroment_block, current_directory, startup_info,
                             &temp_process_info);
    DestroyEnvironmentBlock(enviroment_block);
    if (!launched) {
      //DWORD error = ::GetLastError();
      CR_DPLOG(ERROR) << "Command line:" << std::endl
                      << UTF16ToUTF8(cmdline)
                      << std::endl;
      return Process();
    }
  } else {
    wchar_t* new_environment = nullptr;
    std::wstring env_storage;
    if (options.clear_environment || !options.environment.empty()) {
      if (options.clear_environment) {
        static const wchar_t kEmptyEnvironment[] = {0};
        env_storage = AlterEnvironment(kEmptyEnvironment, options.environment);
      } else {
        wchar_t* old_environment = ::GetEnvironmentStringsW();
        if (!old_environment) {
          CR_DPLOG(ERROR);
          return Process();
        }
        env_storage = AlterEnvironment(old_environment, options.environment);
        ::FreeEnvironmentStringsW(old_environment);
      }
      new_environment = const_cast<wchar_t*>(env_storage.data());
      flags |= CREATE_UNICODE_ENVIRONMENT;
    }

    if (!CreateProcessW(NULL,
                        &writable_cmdline[0], NULL, NULL, inherit_handles,
                        flags, new_environment, current_directory,
                        startup_info, &temp_process_info)) {
      CR_DPLOG(ERROR) << "Command line:" << std::endl
                      << UTF16ToUTF8(cmdline)
                      << std::endl;
      return Process();
    }
  }
  win::ScopedProcessInformation process_info(temp_process_info);

  if (options.job_handle) {
    if (0 == AssignProcessToJobObject(options.job_handle,
                                      process_info.process_handle())) {
      CR_DLOG(ERROR) << "Could not AssignProcessToObject.";
      Process scoped_process(process_info.TakeProcessHandle());
      scoped_process.Terminate(kProcessKilledExitCode, true);
      return Process();
    }

    ResumeThread(process_info.thread_handle());
  }

  if (options.grant_foreground_privilege &&
      !AllowSetForegroundWindow(GetProcId(process_info.process_handle()))) {
    CR_DPLOG(ERROR) << "Failed to grant foreground privilege to launched process";
  }

  if (options.wait)
    ::WaitForSingleObject(process_info.process_handle(), INFINITE);

  return Process(process_info.TakeProcessHandle());
}

bool SetJobObjectLimitFlags(HANDLE job_object, DWORD limit_flags) {
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_info = {};
  limit_info.BasicLimitInformation.LimitFlags = limit_flags;
  return 0 != SetInformationJobObject(
      job_object,
      JobObjectExtendedLimitInformation,
      &limit_info,
      sizeof(limit_info));
}

bool GetAppOutput(const CommandLine& cl, std::string* output) {
  return GetAppOutput(cl.GetCommandLineString(), output);
}

bool GetAppOutputAndError(const CommandLine& cl, std::string* output) {
  return GetAppOutputInternal(cl.GetCommandLineString(), true, output);
}

bool GetAppOutput(const StringPiece16& cl, std::string* output) {
  return GetAppOutputInternal(cl, false, output);
}

void RaiseProcessToHighPriority() {
  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
}

LaunchOptions::LaunchOptions()
    : wait(false),
      start_hidden(false),
      elevated(false),
      handles_to_inherit(NULL),
      inherit_handles(false),
      as_user(NULL),
      empty_desktop_name(false),
      job_handle(NULL),
      stdin_handle(NULL),
      stdout_handle(NULL),
      stderr_handle(NULL),
      force_breakaway_from_job_(false),
      grant_foreground_privilege(false),
      clear_environment(false){}

LaunchOptions::~LaunchOptions() {
}

}  // namespace crbase
