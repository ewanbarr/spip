/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPSocketReceive.h"

#include <arpa/inet.h>

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <stdexcept>

using namespace std;

spip::UDPSocketReceive::UDPSocketReceive ()
{
  have_packet = 0;
  kernel_bufsz = 131071;      // general default buffer size for linux kernels
  multicast = false;
}

spip::UDPSocketReceive::~UDPSocketReceive ()
{
  if (multicast)
    leave_multicast();
  if (buf)
    free (buf);
  buf = 0;
}

void spip::UDPSocketReceive::open (string ip_address, int port)
{
#ifdef _DEBUG
  cerr << "spip::UDPSocketReceive::open(" << ip_address << ", " << port << ")" << endl;
#endif

  // open the socket FD
  spip::UDPSocket::open (port);

  if (ip_address.compare("any") == 0)
  {
    udp_sock.sin_addr.s_addr = htonl (INADDR_ANY);
  }
  else
    udp_sock.sin_addr.s_addr = inet_addr (ip_address.c_str());

  // bind socket to file descriptor
  if (bind(fd, (struct sockaddr *)&udp_sock, sizeof(udp_sock)) == -1) 
  {
    throw runtime_error ("could not bind to UDP socket");
  }
}

void spip::UDPSocketReceive::open_multicast (string ip_address, string group, int port)
{
  // open the UDP socket on INADDR_ANY
  open ("any", port);

  // use setsockopt() to request that the kernel join a multicast group
  mreq.imr_multiaddr.s_addr=inet_addr(group.c_str());
  mreq.imr_interface.s_addr=inet_addr(ip_address.c_str());

#ifdef _DEBUG
  cerr << "spip::UDPSocketReceive::open_multicast mreq.imr_multiaddr.s_addr=inet_addr=" << group << ":" << port << endl;
  cerr << "spip::UDPSocketReceive::open_multicast mreq.imr_interface.s_addr=inet_addr=" << ip_address<< endl;
#endif

  if (setsockopt(fd, IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)) < 0)
  {
    throw runtime_error ("could not subscribe to multicast address");
  }
  multicast = true;
}

void spip::UDPSocketReceive::leave_multicast ()
{
  if (setsockopt(fd, IPPROTO_IP,IP_DROP_MEMBERSHIP,&mreq,sizeof(mreq)) < 0)
  {
    cerr << "could not unsubscribe from multicast address" << endl;
  }
}

size_t spip::UDPSocketReceive::resize_kernel_buffer (size_t pref_size)
{
  int value = 0;
  int len = 0;
  int retval = 0;

  // Attempt to set to the specified value
  value = pref_size;
  len = sizeof(value);
  retval = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &value, len);
  if (retval != 0)
    throw runtime_error("could not set SO_RCVBUF to default size");

  // now check if it worked
  len = sizeof(value);
  value = 0;
  retval = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &value, (socklen_t *) &len);
  if (retval != 0)
    throw runtime_error("could not get SO_RCVBUF size");

  // Check the size. n.b. linux actually sets the size to DOUBLE the value
  if (value*2 != pref_size && value/2 != pref_size)
  {
    len = sizeof(value);
    value = 131071;
    retval = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &value, len);
    if (retval != 0)
      throw runtime_error("could not set SO_RCVBUF size");

    // Now double check that the buffer size is at least correct here
    len = sizeof(value);
    value = 0;
    retval = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &value, (socklen_t *) &len);
    if (retval != 0)
      throw runtime_error("could not get SO_RCVBUF size");
  }

  return value;
}

size_t spip::UDPSocketReceive::clear_buffered_packets ()
{
  size_t bytes_cleared = 0;
  size_t bytes_read = 0;
  unsigned keep_reading = 1;
  int errsv;

  int was_blocking = get_blocking();

  if (was_blocking)
    set_nonblock();

  while ( keep_reading)
  {
    bytes_read = recvfrom (fd, buf, bufsz, 0, NULL, NULL);
    if (bytes_read == bufsz)
    {
      bytes_cleared += bytes_read;
    }
    else if (bytes_read == -1)
    {
      keep_reading = 0;
      errsv = errno;
      if (errsv != EAGAIN)
        throw runtime_error ("recvfrom failed");
    }
    else
      keep_reading = 0;
  }

  if (was_blocking)
    set_block();

  return bytes_cleared;
}


size_t spip::UDPSocketReceive::recv ()
{
  size_t received = recvfrom (fd, buf, bufsz, 0, NULL, NULL);
  if (received < 0)
  {
    //cerr << "sock_recv recvfrom" << endl;
    return -1;
  }

  return received;
}


