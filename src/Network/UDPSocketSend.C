/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPSocketSend.h"

#include <arpa/inet.h>
#include <netdb.h>

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
  struct in_addr *addr;
  addr = atoaddr (ip_address.c_str());
  udp_sock.sin_addr.s_addr = addr->s_addr;
}

size_t spip::UDPSocketSend::send (size_t nbytes)
{
  if (nbytes > bufsz)
    throw runtime_error ("cannot send more bytes than socket size");
  return sendto(fd, buf, nbytes, 0, sock_addr, sock_size); 
}

