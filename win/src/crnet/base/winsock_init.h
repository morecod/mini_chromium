// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Winsock initialization must happen before any Winsock calls are made.  The
// EnsureWinsockInit method will make sure that WSAStartup has been called.

#ifndef MINI_CHROMIUM_SRC_CRNET_BASE_WINSOCK_INIT_H_
#define MINI_CHROMIUM_SRC_CRNET_BASE_WINSOCK_INIT_H_

#include "crnet/base/net_export.h"

namespace crnet {

// Make sure that Winsock is initialized, calling WSAStartup if needed.
CRNET_EXPORT void EnsureWinsockInit();

}  // namespace crnet

#endif  // MINI_CHROMIUM_SRC_CRNET_BASE_WINSOCK_INIT_H_