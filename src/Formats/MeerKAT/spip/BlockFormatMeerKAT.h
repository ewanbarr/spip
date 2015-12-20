
#ifndef __BlockFormatMeerKAT_h
#define __BlockFormatMeerKAT_h

#include "spip/BlockFormat.h"

#include <cstring>

namespace spip {

  class BlockFormatMeerKAT : public BlockFormat {

    public:

      BlockFormatMeerKAT ();

      ~BlockFormatMeerKAT ();

      void unpack_hgft (char * buffer, uint64_t nbytes);

      void unpack_ms (char * buffer, uint64_t nbytes);

    private:

      unsigned nsamp_block;

  };

}

#endif
