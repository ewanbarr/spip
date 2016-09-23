
#ifndef __UDPReceiver_h
#define __UDPReceiver_h

#include "spip/AsciiHeader.h"
#include "spip/UDPSocketReceive.h"
#include "spip/UDPFormat.h"
#include "spip/UDPStats.h"

#include <cstdlib>

#ifdef  HAVE_VMA
#include <mellanox/vma_extra.h>
#endif

namespace spip {

  class UDPReceiver {

    public:

      UDPReceiver ();

      ~UDPReceiver ();

      void configure (const char * header_str);

      void prepare ();

      void set_format (UDPFormat * fmt);

      void stop_receiving ();

      // transmission thread
      void receive ();

      UDPStats * get_stats () { return stats; };

      char verbose;

    protected:

      UDPSocketReceive * sock;

      UDPFormat * format;

      UDPStats * stats;

      std::string data_host;

      std::string data_mcast;

      int data_port;

      AsciiHeader header;

#ifdef HAVE_VMA
      struct vma_api_t *vma_api;

      struct vma_packets_t* pkts;
#else
      char vma_api;
#endif

      unsigned nchan;

      unsigned ndim;

      unsigned nbit;

      unsigned npol;

      float bw;

      float channel_bw;

      double tsamp;

      uint64_t bits_per_second;

      uint64_t bytes_per_second;

      bool keep_receiving;

      bool have_utc_start;
  };

}

#endif
