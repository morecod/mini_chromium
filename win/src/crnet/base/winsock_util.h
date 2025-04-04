// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRNET_BASE_WINSOCK_UTIL_H_
#define MINI_CHROMIUM_SRC_CRNET_BASE_WINSOCK_UTIL_H_

#include <stddef.h>
///#include <winsock2.h>

typedef void* WSAEVENT;

namespace crnet {

// Bluetooth address size. Windows Bluetooth is supported via winsock.
static const size_t kBluetoothAddressSize = 6;

// Assert that the (manual-reset) event object is not signaled.
void AssertEventNotSignaled(WSAEVENT hEvent);

// If the (manual-reset) event object is signaled, resets it and returns true.
// Otherwise, does nothing and returns false.  Called after a Winsock function
// succeeds synchronously
//
// Our testing shows that except in rare cases (when running inside QEMU),
// the event object is already signaled at this point, so we call this method
// to avoid a context switch in common cases.  This is just a performance
// optimization.  The code still works if this function simply returns false.
bool ResetEventIfSignaled(WSAEVENT hEvent);

}  // namespace crnet

#endif  // MINI_CHROMIUM_SRC_CRNET_BASE_WINSOCK_UTIL_H_