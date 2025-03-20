// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_CRNET_SOCKET_TCP_SOCKET_H_
#define MINI_CHROMIUM_CRNET_SOCKET_TCP_SOCKET_H_

#include "crbase/build_config.h"
#include "crnet/base/net_export.h"

#if defined(MINI_CHROMIUM_OS_WIN)
#include "crnet/socket/tcp_socket_win.h"
#elif defined(MINI_CHROMIUM_OS_POSIX)
#include "crnet/socket/tcp_socket_posix.h"
#endif

namespace crnet {

// TCPSocket provides a platform-independent interface for TCP sockets.
//
// It is recommended to use TCPClientSocket/TCPServerSocket instead of this
// class, unless a clear separation of client and server socket functionality is
// not suitable for your use case (e.g., a socket needs to be created and bound
// before you know whether it is a client or server socket).
#if defined(MINI_CHROMIUM_OS_WIN)
typedef TCPSocketWin TCPSocket;
#elif defined(MINI_CHROMIUM_OS_POSIX)
typedef TCPSocketPosix TCPSocket;
#endif

// Check if TCP FastOpen is supported by the OS.
bool IsTCPFastOpenSupported();

// Check if TCP FastOpen is enabled by the user.
bool IsTCPFastOpenUserEnabled();

// Checks if TCP FastOpen is supported by the kernel. Also enables TFO for all
// connections if indicated by user.
// Not thread safe.  Must be called during initialization/startup only.
CRNET_EXPORT void CheckSupportAndMaybeEnableTCPFastOpen(bool user_enabled);

}  // namespace crnet

#endif  // MINI_CHROMIUM_CRNET_SOCKET_TCP_SOCKET_H_