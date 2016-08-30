
#ifndef __BlockFormatCASPSR_h
#define __BlockFormatCASPSR_h

#include "spip/BlockFormat.h"

#include <cstring>

namespace spip {

  class BlockFormatCASPSR : public BlockFormat {

    public:

      BlockFormatCASPSR ();

      ~BlockFormatCASPSR ();

      void unpack_hgft (char * buffer, uint64_t nbytes);

      void unpack_ms (char * buffer, uint64_t nbytes);

    private:

  };

}

#endif
