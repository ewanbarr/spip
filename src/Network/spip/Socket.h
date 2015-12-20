
#ifndef __Socket_h
#define __Socket_h

#include "config.h"

#ifdef HAVE_HWLOC
#include <hwloc.h>
#endif

#include <cstddef>
#include <string>

#include <netinet/in.h>

namespace spip {

  class Socket {

    public:

      Socket ();

      ~Socket ();

      // open the socket
      virtual void open (int port) = 0;

      // close the socket
      void close_me ();

      // resize the socket buffer
      void resize (size_t new_bufsz);

      int set_nonblock();

      int set_block();

      struct in_addr * atoaddr (const char *address) ;

      char get_blocking () { return ! non_block; };

      int get_fd () { return fd; };

      char * get_buf () { return buf; };

      size_t get_bufsz () { return bufsz; };


    protected:

      // socket file descriptor
      int fd;

      // socket buffer
      char * buf;

      // size of socket buffer
      size_t bufsz;

    private:

#ifdef HAVE_HWLOC
      hwloc_topology_t topology;
#endif

      // IP address of UDP interface
      std::string interface;

      // UDP port number
      int port;

      // flag for blocking I/O state on socket
      char non_block;

  };

}

#endif
