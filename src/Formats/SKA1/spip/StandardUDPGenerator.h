
#ifndef __StandardUDPGenerator_h
#define __StandardUDPGenerator_h

#include "spip/ska1_def.h"
#include "spip/UDPGenerator.h"

namespace spip {

  class StandardUDPGenerator : public UDPGenerator {

    public:

      StandardUDPGenerator ();

      ~StandardUDPGenerator ();

      void generate_signal ();

      void encode_header (void * buf, size_t bufsz, uint64_t packet_number);

    private:

      int version;

  };

}

#endif
