
#ifndef __TCPSocketServer_h
#define __TCPSocketServer_h

#include "spip/TCPSocket.h"

#include <netinet/in.h>

namespace spip {

  class TCPSocketServer : public TCPSocket {

    public:

      TCPSocketServer ();

      ~TCPSocketServer ();

      // open the listening socket
      void open (std::string, int, int);

      // close the listening socket
      void close_me ();

      // enter blocking accept call
      int accept_client ();

      // try to accept a connection within a timeout
      int accept_client (int timeout);

      void close_client ();

    private:

      int client_fd;

      //FILE * sock_in;

      //FILE * sock_out;

  };

}

#endif
