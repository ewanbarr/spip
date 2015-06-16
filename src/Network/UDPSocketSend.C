/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPSocketSend.h"

#include <arpa/inet.h>

#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <stdexcept>

using namespace std;

spip::UDPSocketSend::UDPSocketSend ()
{
  sock_addr = (struct sockaddr *) &udp_sock;
  sock_size = sizeof(struct sockaddr);
}

spip::UDPSocketSend::~UDPSocketSend ()
{
}

void spip::UDPSocketSend::open (string ip_address, int port)
{
  // open the socket FD
  spip::UDPSocket::open (port);

  // transmitting sockets must have an IP specified
  udp_sock.sin_addr.s_addr = inet_addr (ip_address.c_str());

  resize (9000);
}

size_t spip::UDPSocketSend::send () 
{
  size_t bytes_sent = sendto(fd, buf, bufsz, 0, sock_addr, sock_size);
  return bytes_sent;
}

