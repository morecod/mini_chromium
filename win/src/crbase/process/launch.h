// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains functions for launching subprocesses.

#ifndef MINI_CHROMIUM_SRC_CRBASE_PROCESS_LAUNCH_H_
#define MINI_CHROMIUM_SRC_CRBASE_PROCESS_LAUNCH_H_

#include <windows.h>
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

namespace crbase {

class CommandLine;

typedef std::vector<HANDLE> HandlesToInheritVector;
// TODO(viettrungluu): Only define this on POSIX?
typedef std::vector<std::pair<int, int> > FileHandleMappingVector;

// Options for launching a subprocess that are passed to LaunchProcess().
// The default constructor constructs the object with default options.
struct CRBASE_EXPORT LaunchOptions {
  LaunchOptions();
  ~LaunchOptions();

  // If true, wait for the process to complete.
  bool wait;

  bool start_hidden;

  // If not empty, change to this directory before executing the new process.
  FilePath current_directory;
  
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

// Set |job_object|'s JOBOBJECT_EXTENDED_LIMIT_INFORMATION
// BasicLimitInformation.LimitFlags to |limit_flags|.
CRBASE_EXPORT bool SetJobObjectLimitFlags(HANDLE job_object, DWORD limit_flags);

// Output multi-process printf, cout, cerr, etc to the cmd.exe console that ran
// chrome. This is not thread-safe: only call from main thread.
CRBASE_EXPORT void RouteStdioToConsole(bool create_console_if_not_found);

// Executes the application specified by |cl| and wait for it to exit. Stores
// the output (stdout) in |output|. Redirects stderr to /dev/null. Returns true
// on success (application launched and exited cleanly, with exit code
// indicating success).
CRBASE_EXPORT bool GetAppOutput(const CommandLine& cl, std::string* output);

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

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_PROCESS_LAUNCH_H_