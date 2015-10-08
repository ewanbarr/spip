/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/TCPSocketServer.h"

#include <arpa/inet.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>


#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <iostream>

#include <stdexcept>
#include <sstream>

using namespace std;

spip::TCPSocketServer::TCPSocketServer ()
{
  client_fd = 0;
  //sock_in = sock_out = 0;
}

spip::TCPSocketServer::~TCPSocketServer ()
{
  close_client();
}

// open a TCP socket and listen for up to nqueued connections
void spip::TCPSocketServer::open (std::string ip_addr, int port, int nqueued)
{
  spip::TCPSocket::open (port);

  // determine if the listening socket will use a specific or any IP address
  if (ip_addr == "any")
  {
    server.sin_addr.s_addr = INADDR_ANY;
  }
  else
  {
    struct in_addr *addr;
    addr = spip::Socket::atoaddr (ip_addr.c_str());
    server.sin_addr.s_addr = addr->s_addr;
  }

  // bind socket to file descriptor
  size_t length = sizeof(struct sockaddr_in);
  if (bind(fd, (struct sockaddr *)&server, length) == -1)
  {
    stringstream ss;
    ss << "could not bind TCPSocketServer to " << ip_addr << ":" <<  port;
    throw runtime_error (ss.str());
  }

  // by default all sockets are blocking
  set_block();

  // listen on the file descriptor
  if (listen(fd, nqueued) < 0)
    throw runtime_error ("Could not listen on socket");

  cerr << "spip::TCPSocketServer::open DONE!" << endl;
}

void spip::TCPSocketServer::close_me ()
{
/*
  if (sock_in)
    fclose(sock_in);
  if (sock_out)
    fclose(sock_out);
  if (client_fd)
    close(client_fd);
*/

  spip::TCPSocket::close_me ();
}

// enter blocking accept call
int spip::TCPSocketServer::accept_client ()
{
  // close any active client connection (if there is one)
  close_client();

  client_fd = accept (fd, (struct sockaddr *)NULL, NULL);

  if (client_fd < 0)
  {
    throw runtime_error ("could not accept connection");
  }

  //sock_in  = fdopen (client_fd, "r");
  //sock_out = fdopen (client_fd, "w");

  //setbuf (sock_in, 0);
  //setbuf (sock_out, 0);

  return client_fd;
}

// enter non-blocking accept call
int spip::TCPSocketServer::accept_client (int timeout)
{

  // add listening socket to the FD_SET
  fd_set socks;
  FD_ZERO (&socks);
  FD_SET(fd, &socks);

  struct timeval tmout;
  tmout.tv_sec = timeout;
  tmout.tv_usec = 0;

  int readsocks = select(fd+1, &socks, (fd_set *) 0, (fd_set *) 0, &tmout);

  if (readsocks < 0)
    throw runtime_error ("select failed");
  else if (readsocks == 0)
  {
    return -1;
  }
  else
    return accept_client ();
}

void spip::TCPSocketServer::close_client ()
{
/*
  if (sock_in)
    fclose(sock_in);
  sock_in = 0;
  if (sock_out)
    fclose(sock_out);
  sock_out = 0;
*/
  
  if (client_fd)
    close(client_fd);
  client_fd = 0;
}
