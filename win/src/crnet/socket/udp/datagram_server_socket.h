// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRNET_SOCKET_UDP_DATAGRAM_SERVER_SOCKET_H_
#define MINI_CHROMIUM_SRC_CRNET_SOCKET_UDP_DATAGRAM_SERVER_SOCKET_H_

#include <stdint.h>

#include "crnet/base/completion_once_callback.h"
#include "crnet/socket/udp/datagram_socket.h"
#include "crnet/socket/udp/diff_serv_code_point.h"

namespace crnet {

class IPAddress;
class IPEndPoint;
class IOBuffer;

// A UDP Socket.
class CRNET_EXPORT DatagramServerSocket : public DatagramSocket {
 public:
  ~DatagramServerSocket() override {}

  // Initialize this socket as a server socket listening at |address|.
  // Returns a network error code.
  virtual int Listen(const IPEndPoint& address) = 0;

  
  // Binds the socket with address and port, and starts listening. It expects
  // a valid IPv4 or IPv6 address. Otherwise, it returns ERR_ADDRESS_INVALID.
  // Subclasses may override this function if |address_string| is in a different
  // format, for example, unix domain socket path.
  virtual int ListenWithAddressAndPort(const std::string& address_string,
                                       uint16_t port);

  // Read from a socket and receive sender address information.
  // |buf| is the buffer to read data into.
  // |buf_len| is the maximum amount of data to read.
  // |address| is a buffer provided by the caller for receiving the sender
  //   address information about the received data.  This buffer must be kept
  //   alive by the caller until the callback is placed.
  // |address_length| is a ptr to the length of the |address| buffer.  This
  //   is an input parameter containing the maximum size |address| can hold
  //   and also an output parameter for the size of |address| upon completion.
  // |callback| is the callback on completion of the RecvFrom.
  // Returns a net error code, or ERR_IO_PENDING if the IO is in progress.
  // If ERR_IO_PENDING is returned, the caller must keep |buf|, |address|,
  // and |address_length| alive until the callback is called.
  virtual int RecvFrom(IOBuffer* buf,
                       int buf_len,
                       IPEndPoint* address,
                       CompletionOnceCallback callback) = 0;

  // Send to a socket with a particular destination.
  // |buf| is the buffer to send
  // |buf_len| is the number of bytes to send
  // |address| is the recipient address.
  // |address_length| is the size of the recipient address
  // |callback| is the user callback function to call on complete.
  // Returns a net error code, or ERR_IO_PENDING if the IO is in progress.
  // If ERR_IO_PENDING is returned, the caller must keep |buf| and |address|
  // alive until the callback is called.
  virtual int SendTo(IOBuffer* buf,
                     int buf_len,
                     const IPEndPoint& address,
                     CompletionOnceCallback callback) = 0;

  // Set the receive buffer size (in bytes) for the socket.
  // Returns a net error code.
  virtual int SetReceiveBufferSize(int32_t size) = 0;

  // Set the send buffer size (in bytes) for the socket.
  // Returns a net error code.
  virtual int SetSendBufferSize(int32_t size) = 0;

  // Allow the socket to share the local address to which the socket will
  // be bound with other processes. Should be called before Listen().
  virtual void AllowAddressReuse() = 0;

  // Allow sending and receiving packets to and from broadcast addresses.
  // Should be called before Listen().
  virtual void AllowBroadcast() = 0;

  // Join the multicast group with address |group_address|.
  // Returns a network error code.
  virtual int JoinGroup(const IPAddress& group_address) const = 0;

  // Leave the multicast group with address |group_address|.
  // If the socket hasn't joined the group, it will be ignored.
  // It's optional to leave the multicast group before destroying
  // the socket. It will be done by the OS.
  // Returns a network error code.
  virtual int LeaveGroup(const IPAddress& group_address) const = 0;

  // Set interface to use for multicast. If |interface_index| set to 0, default
  // interface is used.
  // Should be called before Bind().
  // Returns a network error code.
  virtual int SetMulticastInterface(uint32_t interface_index) = 0;

  // Set the time-to-live option for UDP packets sent to the multicast
  // group address. The default value of this option is 1.
  // Cannot be negative or more than 255.
  // Should be called before Bind().
  // Returns a network error code.
  virtual int SetMulticastTimeToLive(int time_to_live) = 0;

  // Set the loopback flag for UDP socket. If this flag is true, the host
  // will receive packets sent to the joined group from itself.
  // The default value of this option is true.
  // Should be called before Bind().
  // Returns a network error code.
  virtual int SetMulticastLoopbackMode(bool loopback) = 0;

  // Set the Differentiated Services Code Point. May do nothing on
  // some platforms. Returns a network error code.
  virtual int SetDiffServCodePoint(DiffServCodePoint dscp) = 0;

  // Resets the thread to be used for thread-safety checks.
  virtual void DetachFromThread() = 0;
};

}  // namespace crnet

#endif  // MINI_CHROMIUM_SRC_CRNET_SOCKET_UDP_DATAGRAM_SERVER_SOCKET_H_