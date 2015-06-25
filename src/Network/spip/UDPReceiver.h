
#ifndef __UDPReceiver_h
#define __UDPReceiver_h

#include "spip/UDPSocketReceive.h"
#include "spip/UDPFormat.h"
#include "spip/UDPStats.h"

#include <cstdlib>

namespace spip {

  class UDPReceiver {

    public:

      UDPReceiver ();

      ~UDPReceiver ();

      int configure (const char * header);

      void prepare (std::string ip_address, int port);

      void set_format (UDPFormat * fmt);

      // transmission thread
      void receive ();

      UDPStats * get_stats () { return stats; };

    protected:

      UDPSocketReceive * sock;

      UDPFormat * format;

      UDPStats * stats;

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
