// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/process/process.h"

#include <memory>

#include "crbase/logging.h"
#include "crbase/numerics/safe_conversions.h"
#include "crbase/process/kill.h"
#include "crbase/win/windows_version.h"
#include "crbase/win/native_api_wrapper.h"
#include "crbase/win/native_internal.h"

namespace {

DWORD kBasicProcessAccess = PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION |
                            SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION;

// query UNICODE_STRING only!!
HRESULT GetProcessInformationString(HANDLE process,
                                    NT_PROCESS_INFORMATION_CLASS info_class,
                                    crbase::string16* str) {
  CR_DCHECK(str);

  if (info_class != kProcessCommandLineInformation &&
      info_class != kProcessImageFileName &&
      info_class != kProcessImageFileNameWin32)
    return E_FAIL; // unsupport infor class..

  ULONG buf_len = 0;
  HRESULT hr =
      NtQueryProcessInformation(process, info_class, NULL, 0, &buf_len);
  if (buf_len >= (MAXWORD - sizeof(NT_UNICODE_STRING)))
    return hr; // buf too long!

  std::unique_ptr<char[]> buf(new char[buf_len]);
  hr = NtQueryProcessInformation(
      process, info_class, buf.get(), buf_len, &buf_len);
  if (SUCCEEDED(hr)) {
    NT_UNICODE_STRING* ustr = reinterpret_cast<NT_UNICODE_STRING*>(buf.get());
    *str = reinterpret_cast<crbase::string16::value_type*>(ustr->Buffer);
    return S_OK;
  }

  return hr;
}

HRESULT GetProcessUserCreateParams(HANDLE process,
                                   NT_RTL_USER_PROCESS_PARAMETERS* params) {
  NT_PROCESS_BASIC_INFORMATION pbi;
  HRESULT hr = NtQueryProcessInformation(
      process, kProcessBasicInformation, &pbi, sizeof(pbi), NULL);
  if (FAILED(hr))
    return hr;

  SIZE_T buf_len = 0;
  NT_PEB peb;
  hr = NtReadVirtualMemory(process,
                          reinterpret_cast<PVOID>(pbi.PebBaseAddress),
                          &peb, sizeof(peb), &buf_len);
  if (FAILED(hr))
    return hr;

  hr = NtReadVirtualMemory(process,
                           reinterpret_cast<PVOID>(peb.ProcessParameters),
                           params, sizeof(*params) , &buf_len);
  return hr;
}

HRESULT GetProcessUnicodeStringBuffer(HANDLE process,
                                     const NT_UNICODE_STRING& ustr,
                                     crbase::string16* buf) {
  CR_DCHECK(buf);

  SIZE_T buf_len = ustr.BufferLength;
  if (buf_len == 0 || ustr.BufferLength > ustr.MaximumBufferLength) {
    return E_FAIL;  // invalid string
  }

  crbase::string16 r;
  r.resize(ustr.BufferLength / sizeof(r[0]));

  HRESULT hr = NtReadVirtualMemory(
      process, reinterpret_cast<PVOID>(ustr.Buffer), &r[0], buf_len, &buf_len);
  if (SUCCEEDED(hr))
    buf->swap(r);

  return hr;
}

#if defined(MINI_CHROMIUM_ARCH_CPU_X86)

HRESULT GetProcessUserCreateParams64(HANDLE process,
                                     NT_RTL_USER_PROCESS_PARAMETERS64* params) {
  NT_PROCESS_BASIC_INFORMATION64 pbi64;
  HRESULT hr = NtQueryProcessInformationWow64(
      process, kProcessBasicInformation, &pbi64, sizeof(pbi64), NULL);
  if (FAILED(hr))
    return hr;

  ULONG64 buf_len = 0;
  NT_PEB64 peb64;
  hr = NtReadVirtualMemoryWow64(
      process, pbi64.PebBaseAddress, &peb64, sizeof(peb64), &buf_len);
  if (FAILED(hr))
    return false;

  hr = NtReadVirtualMemoryWow64(process, peb64.ProcessParameters, params,
                                sizeof(*params), &buf_len);
  return hr;
}

HRESULT GetProcessUnicodeStringBuffer64(HANDLE process,
                                        const NT_UNICODE_STRING64& ustr,
                                        crbase::string16* buf) {
  CR_DCHECK(buf);

  ULONG64 buf_len = ustr.BufferLength;
  if (buf_len == 0 || ustr.BufferLength > ustr.MaximumBufferLength) {
    return E_FAIL;  // invalid string
  }

  crbase::string16 r;
  r.resize(ustr.BufferLength / sizeof(r[0]));

  HRESULT hr = NtReadVirtualMemoryWow64(
      process, ustr.Buffer, &r[0], buf_len, &buf_len);
  if (SUCCEEDED(hr))
    buf->swap(r);

  return hr;
}

#endif  // MINI_CHROMIUM_ARCH_CPU_X86

} // namespace

////////////////////////////////////////////////////////////////////////////////

namespace crbase {

Process::Process(ProcessHandle handle)
    : is_current_process_(false),
      process_(handle) {
  CR_CHECK_NE(handle, ::GetCurrentProcess());
}

Process::Process(Process&& other)
    : is_current_process_(other.is_current_process_),
      process_(other.process_.Take()) {
  other.Close();
}

Process::~Process() {
}

Process& Process::operator=(Process&& other) {
  CR_DCHECK_NE(this, &other);
  process_.Set(other.process_.Take());
  is_current_process_ = other.is_current_process_;
  other.Close();
  return *this;
}

// static
Process Process::Current() {
  Process process;
  process.is_current_process_ = true;
  return process.Pass();
}

// static
Process Process::Open(ProcessId pid) {
  return Process(::OpenProcess(kBasicProcessAccess, FALSE, pid));
}

// static
Process Process::OpenWithExtraPrivileges(ProcessId pid) {
  DWORD access = kBasicProcessAccess | PROCESS_DUP_HANDLE | PROCESS_VM_READ;
  return Process(::OpenProcess(access, FALSE, pid));
}

// static
Process Process::OpenWithAccess(ProcessId pid, DWORD desired_access) {
  return Process(::OpenProcess(desired_access, FALSE, pid));
}

// static
Process Process::DeprecatedGetProcessFromHandle(ProcessHandle handle) {
  CR_DCHECK_NE(handle, ::GetCurrentProcess());
  ProcessHandle out_handle;
  if (!::DuplicateHandle(GetCurrentProcess(), handle,
                         GetCurrentProcess(), &out_handle,
                         0, FALSE, DUPLICATE_SAME_ACCESS)) {
    return Process();
  }
  return Process(out_handle);
}

// static
bool Process::CanBackgroundProcesses() {
  return true;
}

bool Process::IsValid() const {
  return process_.IsValid() || is_current();
}

ProcessHandle Process::Handle() const {
  return is_current_process_ ? GetCurrentProcess() : process_.Get();
}

Process Process::Duplicate() const {
  if (is_current())
    return Current();

  ProcessHandle out_handle;
  if (!IsValid() || !::DuplicateHandle(GetCurrentProcess(),
                                       Handle(),
                                       GetCurrentProcess(),
                                       &out_handle,
                                       0,
                                       FALSE,
                                       DUPLICATE_SAME_ACCESS)) {
    return Process();
  }
  return Process(out_handle);
}

ProcessId Process::Pid() const {
  CR_DCHECK(IsValid());
  return GetProcId(Handle());
}

bool Process::is_current() const {
  return is_current_process_;
}

void Process::Close() {
  is_current_process_ = false;
  if (!process_.IsValid())
    return;

  process_.Close();
}

bool Process::Terminate(int exit_code, bool wait) const {
  CR_DCHECK(IsValid());
  bool result = (::TerminateProcess(Handle(), exit_code) != FALSE);
  if (result && wait) {
    // The process may not end immediately due to pending I/O
    if (::WaitForSingleObject(Handle(), 60 * 1000) != WAIT_OBJECT_0)
      CR_DPLOG(ERROR) << "Error waiting for process exit";
  } else if (!result) {
    CR_DPLOG(ERROR) << "Unable to terminate process";
  }
  return result;
}

bool Process::WaitForExit(int* exit_code) {
  return WaitForExitWithTimeout(TimeDelta::FromMilliseconds(INFINITE),
                                exit_code);
}

bool Process::WaitForExitWithTimeout(TimeDelta timeout, int* exit_code) {
  // Limit timeout to INFINITE.
  DWORD timeout_ms = saturated_cast<DWORD>(timeout.InMilliseconds());
  if (::WaitForSingleObject(Handle(), timeout_ms) != WAIT_OBJECT_0)
    return false;

  DWORD temp_code;  // Don't clobber out-parameters in case of failure.
  if (!::GetExitCodeProcess(Handle(), &temp_code))
    return false;

  if (exit_code)
    *exit_code = temp_code;
  return true;
}

bool Process::IsProcessBackgrounded() const {
  CR_DCHECK(IsValid());
  DWORD priority = GetPriority();
  if (priority == 0)
    return false;  // Failure case.
  return ((priority == BELOW_NORMAL_PRIORITY_CLASS) ||
          (priority == IDLE_PRIORITY_CLASS));
}

bool Process::AdjustTokenPrivileges(LPCTSTR privilege_name,
                                    bool enable) {
  CR_DCHECK(IsValid());

  win::ScopedHandle token;
  if (!OpenProcessToken(Handle(), TOKEN_ADJUST_PRIVILEGES, token.Receive())) {
    CR_DPLOG(ERROR) << "Unable to open process token. ";
    return false;
  }

  LUID luid;
  if (!::LookupPrivilegeValue(NULL, privilege_name, &luid)) {
    CR_DPLOG(ERROR) << "LookupPrivilegeValue failed.";
    return false;
  }

  TOKEN_PRIVILEGES token_privileges;
  token_privileges.PrivilegeCount = 1;
  token_privileges.Privileges[0].Luid = luid;
  token_privileges.Privileges[0].Attributes =
      enable ? SE_PRIVILEGE_ENABLED : SE_PRIVILEGE_ENABLED_BY_DEFAULT;
  return !!::AdjustTokenPrivileges(
      token.Get(),
      FALSE,                   // DisableAllPrivileges
      &token_privileges,       // NewState Buffer
      sizeof(token_privileges),// BufferLength
      NULL,                    // PreviousState
      NULL);                   // ReturnLength
}

Process::ElevationStatus Process::GetElevationStatus() const {
  CR_DCHECK(IsValid());

  // Returns ELEVATION_YES as default pre vista.
  if (win::GetVersion() < win::Version::VISTA)
    return ELEVATION_YES;

  win::ScopedHandle token;
  if (!OpenProcessToken(Handle(), TOKEN_QUERY, token.Receive())) {
    CR_DPLOG(ERROR) << "Unable to open process token. ";
    return ELEVATION_UNKNOW;
  }

  // A nonzero value if the token has elevated privileges;
  // otherwise, a zero value.
  TOKEN_ELEVATION token_elevation;
  DWORD token_elevation_len = sizeof(token_elevation);
  if (!GetTokenInformation(token.Get(), TokenElevation, &token_elevation,
                           token_elevation_len, &token_elevation_len)) {
    CR_DPLOG(ERROR) << "GetTokenInformation failed." ;
    return ELEVATION_UNKNOW;
  }

  return !!token_elevation.TokenIsElevated ? ELEVATION_YES : ELEVATION_NO;
}

bool Process::GetImageFilePath(FilePath* file_path) const {
  CR_DCHECK(file_path);
  CR_DCHECK(IsValid());

  HRESULT hr = S_OK;
  if (win::GetVersion() >= win::Version::VISTA) {
    string16 value;
    hr = GetProcessInformationString(
        Handle(), kProcessImageFileNameWin32, &value);
    if (SUCCEEDED(hr)) {
      *file_path = FilePath(value);
      return true;
    }
    return false;
  }

  // on xp 64bits?
#if defined(MINI_CHROMIUM_ARCH_CPU_X86)
  BOOL is_wow64 = FALSE;

  bool successed = !!::IsWow64Process(Handle(), &is_wow64);
  CR_DPLOG_IF(ERROR, !successed) << "Faild to query wow64 info.";

  win::OSInfo::WindowsArchitecture
      win_arch =  win::OSInfo::GetArchitecture();
  if (win_arch == win::OSInfo::X64_ARCHITECTURE && !is_wow64) {
    // 32bits process read 64bit process memory..
    NT_RTL_USER_PROCESS_PARAMETERS64 upp64;
    hr = GetProcessUserCreateParams64(Handle(), &upp64);
    if (FAILED(hr))
      return false;

    string16 ret;
    hr = GetProcessUnicodeStringBuffer64(Handle(), upp64.ImagePathName, &ret);
    if (FAILED(hr))
      return false;

    *file_path = FilePath(ret);
    return true;
  }
#endif

  // 32bits process read 32bits process | 64bits process read 64bits
  NT_RTL_USER_PROCESS_PARAMETERS upp;
  hr = GetProcessUserCreateParams(Handle(), &upp);
  if (FAILED(hr))
    return false;

  string16 ret;
  hr = GetProcessUnicodeStringBuffer(Handle(), upp.ImagePathName, &ret);
  if (FAILED(hr))
      return false;

  *file_path = FilePath(ret);
  return true;
}

bool Process::GetCommandlineString(string16* cmd_line) const {
  CR_DCHECK(cmd_line);
  CR_DCHECK(IsValid());

  HRESULT hr = S_OK;
  if (win::GetVersion() >= win::Version::VISTA) {
    hr = GetProcessInformationString(
        Handle(), kProcessCommandLineInformation, cmd_line);
    return SUCCEEDED(hr);
  }

  // by reading memory.. pre visita
#if defined(MINI_CHROMIUM_ARCH_CPU_X86)
  BOOL is_wow64 = FALSE;

  bool successed = !!::IsWow64Process(Handle(), &is_wow64);
  CR_DPLOG_IF(ERROR, !successed) << "Faild to query wow64 info.";

  win::OSInfo::WindowsArchitecture
      win_arch = win::OSInfo::GetArchitecture();
  if (win_arch == win::OSInfo::X64_ARCHITECTURE && !is_wow64) {
    // 32bits process read 64bit process memory..
    NT_RTL_USER_PROCESS_PARAMETERS64 upp64;
    hr = GetProcessUserCreateParams64(Handle(), &upp64);
    if (FAILED(hr))
        return false;

    hr = GetProcessUnicodeStringBuffer64(Handle(), upp64.CommandLine, cmd_line);
    return SUCCEEDED(hr);
  }
#endif

  // 32bits process read 32bits process | 64bits process read 64bits
  NT_RTL_USER_PROCESS_PARAMETERS upp;
  hr = GetProcessUserCreateParams(Handle(), &upp);
  if (FAILED(hr))
    return false;

  hr = GetProcessUnicodeStringBuffer(Handle(), upp.CommandLine, cmd_line);
  return SUCCEEDED(hr);
}

bool Process::SetProcessBackgrounded(bool value) {
  CR_DCHECK(IsValid());
  // Vista and above introduce a real background mode, which not only
  // sets the priority class on the threads but also on the IO generated
  // by it. Unfortunately it can only be set for the calling process.
  DWORD priority;
  if ((win::GetVersion() >= win::Version::VISTA) && (is_current())) {
    priority = value ? PROCESS_MODE_BACKGROUND_BEGIN :
                       PROCESS_MODE_BACKGROUND_END;
  } else {
    priority = value ? IDLE_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS;
  }

  return (::SetPriorityClass(Handle(), priority) != 0);
}

int Process::GetPriority() const {
  CR_DCHECK(IsValid());
  return ::GetPriorityClass(Handle());
}

}  // namespace crbase
