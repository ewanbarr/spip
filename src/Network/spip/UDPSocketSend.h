
#ifndef __UDPSocketSend_h
#define __UDPSocketSend_h

#include "spip/UDPSocket.h"

#include <netinet/in.h>

namespace spip {

  class UDPSocketSend : public UDPSocket {

    public:

      UDPSocketSend ();

      ~UDPSocketSend ();

      // open the socket
      void open (std::string, int);

      // send the contents of buf (bufsz bytes)
      inline size_t send () { sendto(fd, buf, bufsz, 0, sock_addr, sock_size); };

      struct in_addr * atoaddr (const char *address) ;

    private:

      struct sockaddr * sock_addr;

      size_t sock_size;

  };

}

#endif
