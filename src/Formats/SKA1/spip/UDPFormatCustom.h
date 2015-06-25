
#ifndef __UDPFormatCustom_h
#define __UDPFormatCustom_h

#include "spip/ska1_def.h"
#include "spip/UDPFormat.h"

namespace spip {

  class UDPFormatCustom : public UDPFormat {

    public:

      UDPFormatCustom ();

      ~UDPFormatCustom ();

      void generate_signal ();

      inline void encode_header (void * buf, size_t bufsz, uint64_t packet_number);

      inline void decode_header (void * buf, size_t bufsz, uint64_t * packet_number);

    private:

  };

}

#endif
