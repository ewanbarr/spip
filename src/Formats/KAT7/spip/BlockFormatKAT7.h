
#ifndef __BlockFormatKAT7_h
#define __BlockFormatKAT7_h

#include "spip/BlockFormat.h"

#include <cstring>

namespace spip {

  class BlockFormatKAT7 : public BlockFormat {

    public:

      BlockFormatKAT7 ();

      ~BlockFormatKAT7 ();

      void unpack_hgft (char * buffer, uint64_t nbytes);

      void unpack_ms (char * buffer, uint64_t nbytes);

    private:

      unsigned nsamp_block;

  };

}

#endif
