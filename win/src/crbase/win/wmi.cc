// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/win/wmi.h"

#include <windows.h>

#include <objbase.h>
#include <stdint.h>
#include <utility>

#include "crbase/strings/string_util.h"
#include "crbase/strings/utf_string_conversions.h"
#include "crbase/win/scoped_bstr.h"
#include "crbase/win/scoped_variant.h"

namespace cr {
namespace win {

bool CreateLocalWmiConnection(bool set_blanket,
                              CR_SCOPED_COMPTR(IWbemServices)* wmi_services) {
  CR_SCOPED_COMPTR(IWbemLocator) wmi_locator;
  HRESULT hr =
      ::CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(wmi_locator.Receive()));
  if (FAILED(hr))
    return false;

  CR_SCOPED_COMPTR(IWbemServices) wmi_services_r;
  hr = wmi_locator->ConnectServer(ScopedBstr(L"ROOT\\CIMV2"),
                                  nullptr, nullptr, nullptr, 0, nullptr,
                                  nullptr, wmi_services_r.Receive());
  if (FAILED(hr))
    return false;

  if (set_blanket) {
    hr = ::CoSetProxyBlanket(wmi_services_r.get(), RPC_C_AUTHN_WINNT,
                             RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                             RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (FAILED(hr))
      return false;
  }

  wmi_services->swap(wmi_services_r);
  return true;
}

bool CreateWmiClassMethodObject(IWbemServices* wmi_services,
                                StringPiece16 class_name,
                                StringPiece16 method_name,
                                CR_SCOPED_COMPTR(IWbemClassObject)* class_instance) {
  // We attempt to instantiate a COM object that represents a WMI object plus
  // a method rolled into one entity.
  ScopedBstr b_class_name(class_name);
  ScopedBstr b_method_name(method_name);
  CR_SCOPED_COMPTR(IWbemClassObject) class_object;
  HRESULT hr;
  hr = wmi_services->GetObject(b_class_name, 0, nullptr, class_object.Receive(),
                               nullptr);
  if (FAILED(hr))
    return false;

  CR_SCOPED_COMPTR(IWbemClassObject) params_def;
  hr = class_object->GetMethod(b_method_name, 0, params_def.Receive(), nullptr);
  if (FAILED(hr))
    return false;

  if (!params_def.get()) {
    // You hit this special case if the WMI class is not a CIM class. MSDN
    // sometimes tells you this. Welcome to WMI hell.
    return false;
  }

  hr = params_def->SpawnInstance(0, class_instance->Receive());
  return SUCCEEDED(hr);
}

// The code in Launch() basically calls the Create Method of the Win32_Process
// CIM class is documented here:
// http://msdn2.microsoft.com/en-us/library/aa389388(VS.85).aspx
// NOTE: The documentation for the Create method suggests that the ProcessId
// parameter and return value are of type uint32_t, but when we call the method
// the values in the returned out_params, are VT_I4, which is int32_t.
bool WmiLaunchProcess(const string16& command_line, int* process_id) {
  CR_SCOPED_COMPTR(IWbemServices) wmi_local;
  if (!CreateLocalWmiConnection(true, &wmi_local))
    return false;

  static constexpr char16 class_name[] = L"Win32_Process";
  static constexpr char16 method_name[] = L"Create";
  CR_SCOPED_COMPTR(IWbemClassObject) process_create;
  if (!CreateWmiClassMethodObject(wmi_local.get(), class_name, method_name,
                                  &process_create)) {
    return false;
  }

  ScopedVariant b_command_line(command_line.data());

  if (FAILED(process_create->Put(L"CommandLine", 0, b_command_line.AsInput(),
                                 0))) {
    return false;
  }

  CR_SCOPED_COMPTR(IWbemClassObject) out_params;
  HRESULT hr = wmi_local->ExecMethod(
      ScopedBstr(class_name), ScopedBstr(method_name), 0, nullptr,
      process_create.get(), out_params.Receive(), nullptr);
  if (FAILED(hr))
    return false;

  // We're only expecting int32_t or uint32_t values, so no need for
  // ScopedVariant.
  VARIANT ret_value = {{{VT_EMPTY}}};
  hr = out_params->Get(L"ReturnValue", 0, &ret_value, nullptr, 0);
  if (FAILED(hr) || V_I4(&ret_value) != 0)
    return false;

  VARIANT pid = {{{VT_EMPTY}}};
  hr = out_params->Get(L"ProcessId", 0, &pid, nullptr, 0);
  if (FAILED(hr) || V_I4(&pid) == 0)
    return false;

  if (process_id)
    *process_id = V_I4(&pid);

  return true;
}

// static
WmiComputerSystemInfo WmiComputerSystemInfo::Get() {
  CR_SCOPED_COMPTR(IWbemServices) services;
  WmiComputerSystemInfo info;

  if (!CreateLocalWmiConnection(true, &services))
    return info;

  info.PopulateModelAndManufacturer(services);
  info.PopulateSerialNumber(services);
  return info;
}

WmiComputerSystemInfo::WmiComputerSystemInfo() {
}

void WmiComputerSystemInfo::PopulateModelAndManufacturer(
    const CR_SCOPED_COMPTR(IWbemServices)& services) {
  static StringPiece16 query_computer_system =
      L"SELECT Manufacturer,Model FROM Win32_ComputerSystem";

  CR_SCOPED_COMPTR(IEnumWbemClassObject) enumerator_computer_system;
  HRESULT hr = services->ExecQuery(
      ScopedBstr(L"WQL"), ScopedBstr(query_computer_system),
      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
      enumerator_computer_system.Receive());
  if (FAILED(hr) || !enumerator_computer_system.get())
    return;

  CR_SCOPED_COMPTR(IWbemClassObject) class_object;
  ULONG items_returned = 0;
  hr = enumerator_computer_system->Next(
      WBEM_INFINITE, 1, class_object.Receive(), &items_returned);
  if (FAILED(hr) || !items_returned)
    return;

  ScopedVariant manufacturer;
  hr = class_object->Get(L"Manufacturer", 0, manufacturer.Receive(), 0, 0);
  if (SUCCEEDED(hr) && manufacturer.type() == VT_BSTR) {
    WideToUTF16(V_BSTR(manufacturer.ptr()),
                ::SysStringLen(V_BSTR(manufacturer.ptr())), &manufacturer_);
  }
  ScopedVariant model;
  hr = class_object->Get(L"Model", 0, model.Receive(), 0, 0);
  if (SUCCEEDED(hr) && model.type() == VT_BSTR) {
    WideToUTF16(V_BSTR(model.ptr()), ::SysStringLen(V_BSTR(model.ptr())),
                &model_);
  }
}

void WmiComputerSystemInfo::PopulateSerialNumber(
    const CR_SCOPED_COMPTR(IWbemServices)& services) {
  static StringPiece16 query_bios =
      L"SELECT SerialNumber FROM Win32_Bios";

  CR_SCOPED_COMPTR(IEnumWbemClassObject) enumerator_bios;
  HRESULT hr = services->ExecQuery(
      ScopedBstr(L"WQL"), ScopedBstr(query_bios),
      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
      enumerator_bios.Receive());
  if (FAILED(hr) || !enumerator_bios.get())
    return;

  CR_SCOPED_COMPTR(IWbemClassObject) class_obj;
  ULONG items_returned = 0;
  hr = enumerator_bios->Next(WBEM_INFINITE, 1, class_obj.Receive(),
                             &items_returned);
  if (FAILED(hr) || !items_returned)
    return;

  ScopedVariant serial_number;
  hr = class_obj->Get(L"SerialNumber", 0, serial_number.Receive(), 0, 0);
  if (SUCCEEDED(hr) && serial_number.type() == VT_BSTR) {
    WideToUTF16(V_BSTR(serial_number.ptr()),
                ::SysStringLen(V_BSTR(serial_number.ptr())), &serial_number_);
  }
}

}  // namespace win
}  // namespace cr
