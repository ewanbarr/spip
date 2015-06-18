
#ifndef __CustomUDPReceiver_h
#define __CustomUDPReceiver_h

#include "spip/ska1_def.h"
#include "spip/UDPReceiver.h"

namespace spip {

  class CustomUDPReceiver : public UDPReceiver {

    public:

      CustomUDPReceiver ();

      ~CustomUDPReceiver ();

      void decode_header (void * buf, size_t bufsz, uint64_t * packet_number);

    private:

      int version;

  };

}

#endif
