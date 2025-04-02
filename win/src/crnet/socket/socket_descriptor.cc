// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crnet/socket/socket_descriptor.h"

#if defined(MINI_CHROMIUM_OS_POSIX)
#include <sys/types.h>
#include <sys/socket.h>
#endif

#if defined(MINI_CHROMIUM_OS_WIN)
#include <ws2tcpip.h>
#include "crbase/win/windows_version.h"
#include "crnet/base/winsock_init.h"
#endif

namespace crnet {

SocketDescriptor CreatePlatformSocket(int family, int type, int protocol) {
#if defined(MINI_CHROMIUM_OS_WIN)
  EnsureWinsockInit();
  SocketDescriptor result = ::WSASocket(family, type, protocol, nullptr, 0,
                                        WSA_FLAG_OVERLAPPED);
  if (result != kInvalidSocket && family == AF_INET6 &&
      cr::win::OSInfo::GetInstance()->version() 
          >= cr::win::Version::VISTA) {
    DWORD value = 0;
    if (setsockopt(result, IPPROTO_IPV6, IPV6_V6ONLY,
                   reinterpret_cast<const char*>(&value), sizeof(value))) {
      closesocket(result);
      return kInvalidSocket;
    }
  }
  return result;
#else  // MINI_CHROMIUM_OS_WIN
  return ::socket(family, type, protocol);
#endif  // MINI_CHROMIUM_OS_WIN

}

}  // namespace crnet