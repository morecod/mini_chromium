// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_CRNET_BASE_SOCKADDR_STORAGE_H_
#define MINI_CHROMIUM_CRNET_BASE_SOCKADDR_STORAGE_H_

#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_OS_POSIX)
#include <sys/socket.h>
#include <sys/types.h>
#elif defined(MINI_CHROMIUM_OS_WIN)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "crnet/base/net_export.h"

namespace crnet {

// Convenience struct for when you need a |struct sockaddr|.
struct CRNET_EXPORT SockaddrStorage {
  SockaddrStorage();
  SockaddrStorage(const SockaddrStorage& other);
  void operator=(const SockaddrStorage& other);

  struct sockaddr_storage addr_storage;
  socklen_t addr_len;
  struct sockaddr* const addr;
};

}  // namespace crnet

#endif  // MINI_CHROMIUM_CRNET_BASE_SOCKADDR_STORAGE_H_