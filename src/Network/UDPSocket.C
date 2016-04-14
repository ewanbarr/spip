/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPSocket.h"

#include <arpa/inet.h>

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <iostream>

#include <stdexcept>

using namespace std;

spip::UDPSocket::UDPSocket ()
{
}

spip::UDPSocket::~UDPSocket ()
{
}

void spip::UDPSocket::open (int port)
{
  fd = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd < 0)
    throw runtime_error("could not create socket");

  // ensure the socket is reuseable without the painful timeout
  int on = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0)
    throw runtime_error ("Could not set SO_REUSEADDR socket option");

  // fill in the udp socket struct with class, listening IP, port
  bzero(&(udp_sock.sin_zero), 8);
  udp_sock.sin_family = AF_INET;
  udp_sock.sin_port = htons(port);

  // by default all sockets are blocking
  set_block();
}
