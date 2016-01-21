
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

      void increment_bytes (uint64_t nbytes);

      void dropped ();

      void dropped_bytes (uint64_t nbytes);

      void dropped (uint64_t ndropped);

      void sleeps (uint64_t nsleeps);

      void reset ();

      uint64_t get_data_transmitted () { return bytes_transmitted; };

      uint64_t get_data_dropped () { return bytes_dropped; };

      uint64_t get_nsleeps() { return nsleeps; };

    private:

      unsigned data;

      unsigned payload;

      uint64_t bytes_transmitted;

      uint64_t bytes_dropped;

      uint64_t nsleeps;

  };

}

#endif
