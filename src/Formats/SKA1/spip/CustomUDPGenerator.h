
#ifndef __CustomUDPGenerator_h
#define __CustomUDPGenerator_h

#include "spip/ska1_def.h"
#include "spip/UDPGenerator.h"

namespace spip {

  class CustomUDPGenerator : public UDPGenerator {

    public:

      CustomUDPGenerator ();

      ~CustomUDPGenerator ();

      void generate_signal ();

      void encode_header (void * buf, size_t bufsz, uint64_t packet_number);

    private:

      int version;

  };

}

#endif
