// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains functions for launching subprocesses.

#ifndef MINI_CHROMIUM_SRC_CRBASE_PROCESS_LAUNCH_H_
#define MINI_CHROMIUM_SRC_CRBASE_PROCESS_LAUNCH_H_

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "crbase/base_export.h"
#include "crbase/environment.h"
#include "crbase/macros.h"
#include "crbase/files/file_path.h"
#include "crbase/process/process.h"
#include "crbase/process/process_handle.h"
#include "crbase/strings/string_piece.h"
#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_OS_POSIX)
#include "base/posix/file_descriptor_shuffle.h"
#elif defined(MINI_CHROMIUM_OS_WIN)
#include <windows.h>
#endif

namespace cr {

class CommandLine;

#if defined(MINI_CHROMIUM_OS_WIN)
typedef std::vector<HANDLE> HandlesToInheritVector;
#endif

// TODO(viettrungluu): Only define this on POSIX?
typedef std::vector<std::pair<int, int> > FileHandleMappingVector;

// Options for launching a subprocess that are passed to LaunchProcess().
// The default constructor constructs the object with default options.
struct CRBASE_EXPORT LaunchOptions {
#if defined(MINI_CHROMIUM_OS_POSIX)
  // Delegate to be run in between fork and exec in the subprocess (see
  // pre_exec_delegate below)
  class CRBASE_EXPORT PreExecDelegate {
   public:
    PreExecDelegate(const PreExecDelegate&) = delete;
    PreExecDelegate& operator=(const PreExecDelegate&) = delete;
    PreExecDelegate() {}
    virtual ~PreExecDelegate() {}

    // Since this is to be run between fork and exec, and fork may have happened
    // while multiple threads were running, this function needs to be async
    // safe.
    virtual void RunAsyncSafe() = 0;
  };
#endif  // defined(MINI_CHROMIUM_OS_POSIX)

  LaunchOptions();
  ~LaunchOptions();

  // If true, wait for the process to complete.
  bool wait;

#if defined(MINI_CHROMIUM_OS_WIN)
  bool start_hidden;

  // Process will be started using ShellExecuteEx instead of CreateProcess so
  // that it is elevated. LaunchProcess with this flag will have different
  // behaviour due to ShellExecuteEx. Some common operations like OpenProcess
  // will fail. Currently the only other supported LaunchOptions are
  // |start_hidden| and |wait|.
  bool elevated;

  // If non-null, inherit exactly the list of handles in this vector (these
  // handles must be inheritable). This is only supported on Vista and higher.
  HandlesToInheritVector* handles_to_inherit;

  // If true, the new process inherits handles from the parent. In production
  // code this flag should be used only when running short-lived, trusted
  // binaries, because open handles from other libraries and subsystems will
  // leak to the child process, causing errors such as open socket hangs.
  // Note: If |handles_to_inherit| is non-null, this flag is ignored and only
  // those handles will be inherited (on Vista and higher).
  bool inherit_handles;

  // If non-null, runs as if the user represented by the token had launched it.
  // Whether the application is visible on the interactive desktop depends on
  // the token belonging to an interactive logon session.
  //
  // To avoid hard to diagnose problems, when specified this loads the
  // environment variables associated with the user and if this operation fails
  // the entire call fails as well.
  UserTokenHandle as_user;

  // If true, use an empty string for the desktop name.
  bool empty_desktop_name;

  // If non-null, launches the application in that job object. The process will
  // be terminated immediately and LaunchProcess() will fail if assignment to
  // the job object fails.
  HANDLE job_handle;

  // Handles for the redirection of stdin, stdout and stderr. The handles must
  // be inheritable. Caller should either set all three of them or none (i.e.
  // there is no way to redirect stderr without redirecting stdin). The
  // |inherit_handles| flag must be set to true when redirecting stdio stream.
  HANDLE stdin_handle;
  HANDLE stdout_handle;
  HANDLE stderr_handle;

  // If set to true, ensures that the child process is launched with the
  // CREATE_BREAKAWAY_FROM_JOB flag which allows it to breakout of the parent
  // job if any.
  bool force_breakaway_from_job_;

  // If set to true, permission to bring windows to the foreground is passed to
  // the launched process if the current process has such permission.
  bool grant_foreground_privilege;
#else

  // If non-null, remap file descriptors according to the mapping of
  // src fd->dest fd to propagate FDs into the child process.
  // This pointer is owned by the caller and must live through the
  // call to LaunchProcess().
  const FileHandleMappingVector* fds_to_remap;

  // Each element is an RLIMIT_* constant that should be raised to its
  // rlim_max.  This pointer is owned by the caller and must live through
  // the call to LaunchProcess().
  const std::vector<int>* maximize_rlimits;

  // If true, start the process in a new process group, instead of
  // inheriting the parent's process group.  The pgid of the child process
  // will be the same as its pid.
  bool new_process_group;

#if defined(MINI_CHROMIUM_OS_LINUX)
  // If non-zero, start the process using clone(), using flags as provided.
  // Unlike in clone, clone_flags may not contain a custom termination signal
  // that is sent to the parent when the child dies. The termination signal will
  // always be set to SIGCHLD.
  int clone_flags;

  // By default, child processes will have the PR_SET_NO_NEW_PRIVS bit set. If
  // true, then this bit will not be set in the new child process.
  bool allow_new_privs;

  // Sets parent process death signal to SIGKILL.
  bool kill_on_parent_death;
#endif  // defined(MINI_CHROMIUM_OS_LINUX)

#if defined(MINI_CHROMIUM_OS_POSIX)
  // If non-null, a delegate to be run immediately prior to executing the new
  // program in the child process.
  //
  // WARNING: If LaunchProcess is called in the presence of multiple threads,
  // code running in this delegate essentially needs to be async-signal safe
  // (see man 7 signal for a list of allowed functions).
  PreExecDelegate* pre_exec_delegate;
#endif  // defined(MINI_CHROMIUM_OS_POSIX)
#endif

  // If not empty, change to this directory before executing the new process.
  FilePath current_directory;

  // Set/unset environment variables. These are applied on top of the parent
  // process environment.  Empty (the default) means to inherit the same
  // environment. See internal::AlterEnvironment().
  EnvironmentMap environment;

  // Clear the environment for the new process before processing changes from
  // |environment|.
  bool clear_environment;
};

// Launch a process via the command line |cmdline|.
// See the documentation of LaunchOptions for details on |options|.
//
// Returns a valid Process upon success.
//
CRBASE_EXPORT Process LaunchProcess(const CommandLine& cmdline,
                                    const LaunchOptions& options);

#if defined(MINI_CHROMIUM_OS_WIN)
// Windows-specific LaunchProcess that takes the command line as a
// string.  Useful for situations where you need to control the
// command line arguments directly, but prefer the CommandLine version
// if launching Chrome itself.
//
// The first command line argument should be the path to the process,
// and don't forget to quote it.
//
// Example (including literal quotes)
//  cmdline = "c:\windows\explorer.exe" -foo "c:\bar\"
CRBASE_EXPORT Process LaunchProcess(const string16& cmdline,
                                    const LaunchOptions& options);
#elif defined(MINI_CHROMIUM_OS_POSIX)
// A POSIX-specific version of LaunchProcess that takes an argv array
// instead of a CommandLine.  Useful for situations where you need to
// control the command line arguments directly, but prefer the
// CommandLine version if launching Chrome itself.
CRBASE_EXPORT Process LaunchProcess(const std::vector<std::string>& argv,
                                    const LaunchOptions& options);

// Close all file descriptors, except those which are a destination in the
// given multimap. Only call this function in a child process where you know
// that there aren't any other threads.
CRBASE_EXPORT void CloseSuperfluousFds(const InjectiveMultimap& saved_map);
#endif

#if defined(MINI_CHROMIUM_OS_WIN)
// Set |job_object|'s JOBOBJECT_EXTENDED_LIMIT_INFORMATION
// BasicLimitInformation.LimitFlags to |limit_flags|.
CRBASE_EXPORT bool SetJobObjectLimitFlags(HANDLE job_object, DWORD limit_flags);

// Output multi-process printf, cout, cerr, etc to the cmd.exe console that ran
// chrome. This is not thread-safe: only call from main thread.
CRBASE_EXPORT void RouteStdioToConsole(bool create_console_if_not_found);
#endif

#if defined(MINI_CHROMIUM_OS_WIN)
// Executes the application specified by |cl| and wait for it to exit. Stores
// the output (stdout) in |output|. Redirects stderr to /dev/null. Returns true
// on success (application launched and exited cleanly, with exit code
// indicating success).
CRBASE_EXPORT bool GetAppOutput(const CommandLine& cl, std::string* output);
#endif

// Like GetAppOutput, but also includes stderr.
CRBASE_EXPORT bool GetAppOutputAndError(const CommandLine& cl,
                                        std::string* output);

// A Windows-specific version of GetAppOutput that takes a command line string
// instead of a CommandLine object. Useful for situations where you need to
// control the command line arguments directly.
CRBASE_EXPORT bool GetAppOutput(const StringPiece16& cl, std::string* output);

// If supported on the platform, and the user has sufficent rights, increase
// the current process's scheduling priority to a high priority.
CRBASE_EXPORT void RaiseProcessToHighPriority();

#if defined(MINI_CHROMIUM_OS_LINUX)
// A wrapper for clone with fork-like behavior, meaning that it returns the
// child's pid in the parent and 0 in the child. |flags|, |ptid|, and |ctid| are
// as in the clone system call (the CLONE_VM flag is not supported).
//
// This function uses the libc clone wrapper (which updates libc's pid cache)
// internally, so callers may expect things like getpid() to work correctly
// after in both the child and parent. An exception is when this code is run
// under Valgrind. Valgrind does not support the libc clone wrapper, so the libc
// pid cache may be incorrect after this function is called under Valgrind.
//
// As with fork(), callers should be extremely careful when calling this while
// multiple threads are running, since at the time the fork happened, the
// threads could have been in any state (potentially holding locks, etc.).
// Callers should most likely call execve() in the child soon after calling
// this.
CRBASE_EXPORT pid_t ForkWithFlags(unsigned long flags, pid_t* ptid, 
                                  pid_t* ctid);
#endif

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_PROCESS_LAUNCH_H_