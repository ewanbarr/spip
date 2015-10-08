/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/TCPSocket.h"

#include <arpa/inet.h>

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <iostream>

#include <stdexcept>

using namespace std;

spip::TCPSocket::TCPSocket ()
{
  bzero(&(server.sin_zero), 8);
}

spip::TCPSocket::~TCPSocket ()
{
}

void spip::TCPSocket::open (int port)
{
  fd = socket (PF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    throw runtime_error("could not create socket");

  // ensure the socket is reuseable without the painful timeout
  int on = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0)
    throw runtime_error ("Could not set SO_REUSEADDR socket option");

  // fill in the udp socket struct with class, listening IP, port
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
}

void spip::TCPSocket::close_me ()
{
  spip::Socket::close_me();
}

