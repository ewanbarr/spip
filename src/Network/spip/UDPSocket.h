
#ifndef __UDPSocket_h
#define __UDPSocket_h

#include "spip/Socket.h"

#include <netinet/in.h>

namespace spip {

  class UDPSocket : public Socket {

    public:

      UDPSocket ();

      ~UDPSocket ();

      // open the socket
      void open (int);

    protected:

      // structure for UDP socket
      struct sockaddr_in udp_sock;

  };

}

#endif
