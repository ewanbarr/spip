
#ifndef __UDPReceiveDB_h
#define __UDPReceiveDB_h

#include "spip/UDPSocketReceive.h"
#include "spip/UDPFormat.h"
#include "spip/UDPStats.h"
#include "spip/DataBlockWrite.h"

#include <cstdlib>

#define USING_VMA_EXTRA_API

#ifdef  USING_VMA_EXTRA_API
#include <mellanox/vma_extra.h>
#endif

namespace spip {

  class UDPReceiveDB {

    public:

      UDPReceiveDB (const char * key_string);

      ~UDPReceiveDB ();

      int configure (const char * header);

      void prepare (std::string ip_address, int port);

      void set_format (UDPFormat * fmt);

      void open (const char * header);

      void close ();

      // transmission thread
      void receive ();

      void stop_capture () { keep_receiving = false; };

      UDPStats * get_stats () { return stats; };

    protected:

      UDPSocketReceive * sock;

      UDPFormat * format;

      UDPStats * stats;

      DataBlockWrite * db;

#ifdef USING_VMA_EXTRA_API
      struct vma_api_t *vma_api;

      struct vma_packets_t* pkts;
#else
      char vma_api;
#endif

      bool keep_receiving;

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
