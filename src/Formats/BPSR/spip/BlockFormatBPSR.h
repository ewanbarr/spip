
#ifndef __BlockFormatBPSR_h
#define __BlockFormatBPSR_h

#include "spip/BlockFormat.h"

#include <cstring>

namespace spip {

  class BlockFormatBPSR : public BlockFormat {

    public:

      BlockFormatBPSR ();

      ~BlockFormatBPSR ();

      void unpack_hgft (char * buffer, uint64_t nbytes);

      void unpack_ms (char * buffer, uint64_t nbytes);

    private:

  };

}

#endif
