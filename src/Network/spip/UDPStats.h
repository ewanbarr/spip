
#ifndef __UDPStats_h
#define __UDPStats_h

#include <cstddef>
#include <inttypes.h>

#include "spip/UDPStats.h"

namespace spip {

  class UDPStats {

    public:

      UDPStats (unsigned header, unsigned data);

      ~UDPStats ();

      void increment ();

      void dropped ();

      void dropped (uint64_t ndropped);

      void sleeps (uint64_t nsleeps);

      void reset ();

      uint64_t get_data_transmitted () { return pkts_transmitted * data; };

      uint64_t get_payload_transmitted () { return pkts_transmitted * payload; };

      uint64_t get_packets_transmitted () { return pkts_transmitted; };

      uint64_t get_packets_dropped () { return pkts_dropped; };

      uint64_t get_data_dropped () { return pkts_dropped * data; };

      uint64_t get_payload_dropped () { return pkts_dropped * payload; };

      uint64_t get_nsleeps() { return nsleeps; };

    private:

      unsigned data;

      unsigned payload;

      uint64_t pkts_transmitted;

      uint64_t pkts_dropped;

      uint64_t nsleeps;

  };

}

#endif
