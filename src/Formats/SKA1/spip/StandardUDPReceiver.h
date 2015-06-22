
#ifndef __StandardUDPReceiver_h
#define __StandardUDPReceiver_h

#include "spip/ska1_def.h"
#include "spip/UDPReceiver.h"

namespace spip {

  class StandardUDPReceiver : public UDPReceiver {

    public:

      StandardUDPReceiver ();

      ~StandardUDPReceiver ();

      void decode_header (void * buf, size_t bufsz, uint64_t * packet_number);

    private:

      int version;

  };

}

#endif
