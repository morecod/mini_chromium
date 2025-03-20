// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <winsock2.h>

#include "crnet/base/winsock_init.h"

#include "crbase/lazy_instance.h"
#include "crbase/logging.h"

namespace {

class WinsockInitSingleton {
 public:
  WinsockInitSingleton() {
    WORD winsock_ver = MAKEWORD(2, 2);
    WSAData wsa_data;
    bool did_init = (WSAStartup(winsock_ver, &wsa_data) == 0);
    if (did_init) {
      CR_DCHECK(wsa_data.wVersion == winsock_ver);

      // The first time WSAGetLastError is called, the delay load helper will
      // resolve the address with GetProcAddress and fixup the import.  If a
      // third party application hooks system functions without correctly
      // restoring the error code, it is possible that the error code will be
      // overwritten during delay load resolution.  The result of the first
      // call may be incorrect, so make sure the function is bound and future
      // results will be correct.
      WSAGetLastError();
    }
  }

  ~WinsockInitSingleton() {
    // Don't call WSACleanup() since the worker pool threads can continue to
    // call getaddrinfo() after Winsock has shutdown, which can lead to crashes.
  }
};

static crbase::LazyInstance<WinsockInitSingleton> g_winsock_init_singleton =
    CR_LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace crnet {

void EnsureWinsockInit() {
  g_winsock_init_singleton.Get();
}

}  // namespace crnet