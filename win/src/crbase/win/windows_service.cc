// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/win/windows_service.h"

#include "crbase/command_line.h"
#include "crbase/logging.h"
#include "crbase/strings/sys_string_conversions.h"

namespace cr {
namespace win {

namespace {
const unsigned int kServiceQueryWaitTimeMs = 100;
// The number of iterations to poll if a service is stopped correctly.
const unsigned int kMaxServiceQueryIterations = 100;
}  // namespace

WindowsService::WindowsService(const cr::StringPiece16& service_name) {
  service_name_ = service_name.as_string();
}

WindowsService::~WindowsService() {}

DWORD WindowsService::InstallService(
    const cr::FilePath& service_binary_path,
    const cr::StringPiece16& display_name,
    DWORD service_type,
    DWORD start_type,
    DWORD error_control,
    ScopedScHandle* sc_handle) {
  ScopedScHandle scm_handle(
      ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE));
  if (!scm_handle.IsValid())
    return ::GetLastError();

  cr::CommandLine command_line(cr::CommandLine::NO_PROGRAM);
  command_line.SetProgram(service_binary_path);

  *sc_handle = ScopedScHandle(::CreateServiceW(
      scm_handle.Get(),                             // SCM database
      service_name_.data(),                         // name of service
      display_name.data(),                          // service name to display
      SERVICE_ALL_ACCESS,                           // desired access
      service_type,                                 // service type
      start_type,                                   // start type
      error_control,                                // error control type
      command_line.GetCommandLineString().c_str(),  // path to service's binary
      nullptr,                                      // no load ordering group
      nullptr,                                      // no tag identifier
      nullptr,                                      // no dependencies
      nullptr,                                      // LocalSystem account
      nullptr));

  if (!sc_handle->IsValid())
    return ::GetLastError();

  return ERROR_SUCCESS;
}

DWORD WindowsService::GetServiceStatus(SERVICE_STATUS* service_status) {
  CR_DCHECK(service_status);

  ScopedScHandle scm_handle(
      ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
  if (!scm_handle.IsValid())
    return ::GetLastError();

  ScopedScHandle sc_handle(::OpenServiceW(
      scm_handle.Get(), service_name_.data(), SERVICE_QUERY_STATUS));
  if (!sc_handle.IsValid())
    return ::GetLastError();

  if (!::QueryServiceStatus(sc_handle.Get(), service_status)) {
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

DWORD WindowsService::DeleteService() {
  ScopedScHandle scm_handle(
      ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!scm_handle.IsValid())
    return ::GetLastError();

  ScopedScHandle sc_handle(
      ::OpenServiceW(scm_handle.Get(), service_name_.data(), DELETE));
  if (!sc_handle.IsValid())
    return ::GetLastError();

  // The DeleteService function marks a service for deletion from the service
  // control manager database. The database entry is not removed until all open
  // handles to the service have been closed by calls to the CloseServiceHandle
  // function, and the service is not running.
  if (!::DeleteService(sc_handle.Get()))
    return ::GetLastError();

  return ERROR_SUCCESS;
}

DWORD WindowsService::StartService() {
  ScopedScHandle scm_handle(
      ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!scm_handle.IsValid())
    return ::GetLastError();

  ScopedScHandle sc_handle(::OpenServiceW(
      scm_handle.Get(), service_name_.data(), SERVICE_START));
  if (!sc_handle.IsValid())
    return ::GetLastError();

  if (!::StartServiceW(sc_handle.Get(), 0, nullptr))
    return ::GetLastError();

  ///CR_LOG(INFO) << "service started successfully.";
  return ERROR_SUCCESS;
}

DWORD WindowsService::WaitForServiceStarted(DWORD wait_sec,
                                            HANDLE cancel_event) {
  ScopedScHandle scm_handle(
      ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
  if (scm_handle.Get() == nullptr)
    return ::GetLastError();

  ScopedScHandle s_handle(::OpenServiceW(
      scm_handle.Get(), service_name_.data(), SERVICE_QUERY_STATUS));
  if (s_handle.Get() == nullptr)
    return ::GetLastError();

  int wait_ms = wait_sec * 1000;
  // Wait until the service is completely started.
  for (;;) {
    SERVICE_STATUS service_status;
    if (!QueryServiceStatus(s_handle.Get(), &service_status)) {
      DWORD error = ::GetLastError();
      CR_LOG(ERROR) << "QueryServiceStatus failed error=" << error;
      return error;
    }
    if (service_status.dwCurrentState == SERVICE_RUNNING)
      return ERROR_SUCCESS;

    if (service_status.dwCurrentState != SERVICE_START_PENDING) {
      CR_LOG(ERROR) << "Cannot start service state="
                    << service_status.dwCurrentState;
      return E_FAIL;
    }

    if (HandleTraits::IsHandleValid(cancel_event)) {
       DWORD wait_err = 
           ::WaitForSingleObject(cancel_event, kServiceQueryWaitTimeMs);
      if (wait_err == WAIT_OBJECT_0) {
         CR_LOG(ERROR) << "User gives up waiting for starting";
         return E_FAIL;
      }

      if (wait_err == WAIT_TIMEOUT) {
        wait_ms -= kServiceQueryWaitTimeMs;
        if (wait_ms <= 0) break;
      } else {
        DWORD error = ::GetLastError();
        CR_LOG(ERROR) << "WaitForSingleObject failed ret=" << wait_err
                      << ", error=" << error;
        return E_FAIL;
      }
    } else {
      ::Sleep(kServiceQueryWaitTimeMs);
      wait_ms -= kServiceQueryWaitTimeMs;
      if (wait_ms <= 0) break;
    }
  }

  // The service didn't terminate.
  CR_LOG(ERROR) << "Starting service timed out";
  return E_FAIL;
}

DWORD WindowsService::WaitForServiceStopped(DWORD wait_sec,
                                            HANDLE cancel_event) {
  ScopedScHandle scm_handle(
      ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
  if (scm_handle.Get() == nullptr)
    return ::GetLastError();

  ScopedScHandle s_handle(::OpenServiceW(
      scm_handle.Get(), service_name_.data(), SERVICE_QUERY_STATUS));
  if (s_handle.Get() == nullptr)
    return ::GetLastError();

  int wait_ms = wait_sec * 1000;
  // Wait until the service is completely stopped.
  for (;;) {
    SERVICE_STATUS service_status;
    if (!QueryServiceStatus(s_handle.Get(), &service_status)) {
      DWORD error = ::GetLastError();
      CR_LOG(ERROR) << "QueryServiceStatus failed error=" << error;
      return error;
    }
    if (service_status.dwCurrentState == SERVICE_STOPPED)
      return ERROR_SUCCESS;

    if (service_status.dwCurrentState != SERVICE_STOP_PENDING &&
        service_status.dwCurrentState != SERVICE_RUNNING) {
      CR_LOG(ERROR) << "Cannot stop service state="
                    << service_status.dwCurrentState;
      return E_FAIL;
    }

    if (HandleTraits::IsHandleValid(cancel_event)) {
       DWORD wait_err = 
           ::WaitForSingleObject(cancel_event, kServiceQueryWaitTimeMs);
      if (wait_err == WAIT_OBJECT_0) {
         CR_LOG(ERROR) << "User gives up waiting for stopping";
         return E_FAIL;
      }

      if (wait_err == WAIT_TIMEOUT) {
        wait_ms -= kServiceQueryWaitTimeMs;
        if (wait_ms <= 0) break;
      } else {
        DWORD error = ::GetLastError();
        CR_LOG(ERROR) << "WaitForSingleObject failed ret=" << wait_err
                      << ", error=" << error;
        return E_FAIL;
      }
    } else {
      ::Sleep(kServiceQueryWaitTimeMs);
      wait_ms -= kServiceQueryWaitTimeMs;
      if (wait_ms <= 0) break;
    }
  }

  // The service didn't terminate.
  CR_LOG(ERROR) << "Stopping service timed out";
  return E_FAIL;
}

DWORD WindowsService::ControlService(DWORD control) {
  ScopedScHandle scm_handle(
      ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!scm_handle.IsValid())
    return ::GetLastError();

  // TODO(crbug.com/40141510): More granular access rights corresponding to the
  // controls can be specified.
  ScopedScHandle s_handle(::OpenServiceW(
      scm_handle.Get(), service_name_.data(), SERVICE_ALL_ACCESS));
  if (!s_handle.IsValid())
    return ::GetLastError();

  SERVICE_STATUS service_status;
  if (!::ControlService(s_handle.Get(), control, &service_status)) {
    DWORD error = ::GetLastError();
    CR_LOG(ERROR) << "ControlService failed with error=" << error;
    return error;
  }

  return ERROR_SUCCESS;
}

DWORD WindowsService::ChangeServiceConfig(DWORD dwServiceType,
                                          DWORD dwStartType,
                                          DWORD dwErrorControl) {
  ScopedScHandle scm_handle(
      ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!scm_handle.IsValid())
    return ::GetLastError();

  ScopedScHandle s_handle(::OpenServiceW(
      scm_handle.Get(), service_name_.data(), SERVICE_CHANGE_CONFIG));
  if (!s_handle.IsValid())
    return ::GetLastError();

  if (!::ChangeServiceConfigW(s_handle.Get(), dwServiceType, dwStartType,
                              dwErrorControl, nullptr, nullptr, nullptr,
                              nullptr, nullptr, nullptr, nullptr)) {
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

DWORD WindowsService::StartServiceCtrlDispatcher(
    LPSERVICE_MAIN_FUNCTIONW service_main) {
  SERVICE_TABLE_ENTRYW dispatch_table[] = {
      {(LPWSTR)service_name_.data(), service_main}, {nullptr, nullptr}};

  if (!::StartServiceCtrlDispatcherW(dispatch_table))
    return ::GetLastError();

  return ERROR_SUCCESS;
}

DWORD WindowsService::RegisterCtrlHandler(
    LPHANDLER_FUNCTION handler_proc,
    SERVICE_STATUS_HANDLE* service_status_handle) {
  CR_DCHECK(handler_proc);
  CR_DCHECK(service_status_handle);

  SERVICE_STATUS_HANDLE sc_status_handle =
      ::RegisterServiceCtrlHandlerW(service_name_.data(), handler_proc);
  if (!sc_status_handle)
    return ::GetLastError();

  *service_status_handle = sc_status_handle;
  return ERROR_SUCCESS;
}

DWORD WindowsService::SetServiceStatus(
    SERVICE_STATUS_HANDLE service_status_handle,
    SERVICE_STATUS service) {
  if (!::SetServiceStatus(service_status_handle, &service))
    return ::GetLastError();
  return ERROR_SUCCESS;
}

}  // namespace win
}  // namespace cr
