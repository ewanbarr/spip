
#ifndef __UDPSocketReceive_h
#define __UDPSocketReceive_h

#include "spip/UDPSocket.h"

#include <netinet/in.h>

namespace spip {

  class UDPSocketReceive : public UDPSocket {

    public:

      UDPSocketReceive ();

      ~UDPSocketReceive ();

      // open the socket
      void open (std::string, int);

      // open the socket and bind to a multicast group
      void open_multicast (std::string, std::string, int port);

      // leave a multicast group on socket
      void leave_multicast ();

      size_t resize_kernel_buffer (size_t);

      size_t clear_buffered_packets ();

      size_t recv ();

    private:

      // size of the kernel socket buffer;
      size_t kernel_bufsz;

      // flag for whether bufsz contains a packet
      char have_packet;

      bool multicast;

      struct ip_mreq mreq;

  };

}

#endif
