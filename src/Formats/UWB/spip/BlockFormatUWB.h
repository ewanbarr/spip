
#ifndef __BlockFormatUWB_h
#define __BlockFormatUWB_h

#include "spip/BlockFormat.h"

#include <cstring>

namespace spip {

  class BlockFormatUWB : public BlockFormat {

    public:

      BlockFormatUWB ();

      ~BlockFormatUWB ();

      void unpack_hgft (char * buffer, uint64_t nbytes);

      void unpack_ms (char * buffer, uint64_t nbytes);

    private:

  };

}

#endif
