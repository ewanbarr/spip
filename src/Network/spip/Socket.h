
#ifndef __Socket_h
#define __Socket_h

#include <cstddef>
#include <string>

#include "spip/Socket.h"

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

      char get_blocking () { return ! non_block; };

      void * get_buf () { return buf; };

      size_t get_bufsz () { return bufsz; };

    protected:

      // socket file descriptor
      int fd;

      // socket buffer
      void * buf;

      // size of socket buffer
      size_t bufsz;

    private:

      // IP address of UDP interface
      std::string interface;

      // UDP port number
      int port;

      // flag for blocking I/O state on socket
      char non_block;

  };

}

#endif
