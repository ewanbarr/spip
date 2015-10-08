
#ifndef __TCPSocket_h
#define __TCPSocket_h

#include "spip/Socket.h"

#include <netinet/in.h>

namespace spip {

  class TCPSocket : public Socket {

    public:

      TCPSocket ();

      ~TCPSocket ();

      // open the socket
      void open (int);

      void close_me ();

    protected:

      struct sockaddr_in server;

  };

}

#endif
