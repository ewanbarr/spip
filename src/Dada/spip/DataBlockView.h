
#ifndef __DataBlockView_h
#define __DataBlockView_h

#include "spip/DataBlock.h"

namespace spip {

  class DataBlockView : public DataBlock
  {
    public:

      DataBlockView (const char * key);

      ~DataBlockView ();

      void lock ();

      void unlock ();

      void * open_block ();

      ssize_t close_block (uint64_t bytes);

      size_t read (void * ptr, size_t bytes);

      size_t seek (int64_t offset, int whence);

    protected:

    private:

  };

}

#endif
