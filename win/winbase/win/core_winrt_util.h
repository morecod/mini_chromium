// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINLIB_WINBASE_WIN_CORE_WINRT_UTIL_H_
#define WINLIB_WINBASE_WIN_CORE_WINRT_UTIL_H_

#include <hstring.h>
#include <inspectable.h>
#include <roapi.h>
#include <windef.h>

#include "winbase\base_export.h"
#include "winbase\strings\string16.h"
#include "winbase\win\scoped_hstring.h"

namespace winbase {
namespace win {

// Provides access to Core WinRT functions which may not be available on
// Windows 7. Loads functions dynamically at runtime to prevent library
// dependencies.

WINBASE_EXPORT bool ResolveCoreWinRTDelayload();

// The following stubs are provided for when component build is enabled, in
// order to avoid the propagation of delay-loading CoreWinRT to other modules.

WINBASE_EXPORT HRESULT RoInitialize(RO_INIT_TYPE init_type);

WINBASE_EXPORT void RoUninitialize();

WINBASE_EXPORT HRESULT RoGetActivationFactory(HSTRING class_id,
                                              const IID& iid,
                                              void** out_factory);

WINBASE_EXPORT HRESULT RoActivateInstance(HSTRING class_id,
                                          IInspectable** instance);

// Retrieves an activation factory for the type specified.
template <typename InterfaceType, char16 const* runtime_class_id>
HRESULT GetActivationFactory(InterfaceType** factory) {
  ScopedHString class_id_hstring = ScopedHString::Create(runtime_class_id);
  if (!class_id_hstring.is_valid())
    return E_FAIL;

  return winbase::win::RoGetActivationFactory(class_id_hstring.get(),
                                              IID_PPV_ARGS(factory));
}

}  // namespace win
}  // namespace winbase

#endif  // WINLIB_WINBASE_WIN_CORE_WINRT_UTIL_H_