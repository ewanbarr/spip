
#ifndef __UDPReceiver_h
#define __UDPReceiver_h

#include "spip/UDPSocketReceive.h"
#include "spip/UDPStats.h"

#include <cstdlib>

namespace spip {

  class UDPReceiver {

    public:

      UDPReceiver ();

      ~UDPReceiver ();

      int configure (const char * header);

      // derived classes must implement this
      virtual void decode_header (void * buf, size_t bufsz, uint64_t * packet_number) = 0;

      void prepare (std::string ip_address, int port);

      // transmission thread
      void receive ();

      UDPStats * get_stats () { return stats; };

    protected:

      UDPSocketReceive * sock;

      UDPStats * stats;

      size_t packet_header_size;

      size_t packet_data_size;

      unsigned nchan;

      unsigned ndim;

      unsigned nbit;

      unsigned npol;

      float bw;

      float channel_bw;

      float tsamp;

      unsigned bits_per_second;

      unsigned bytes_per_second;
  };

}

#endif
