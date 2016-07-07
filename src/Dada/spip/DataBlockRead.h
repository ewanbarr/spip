
#ifndef __DataBlockRead_h
#define __DataBlockRead_h

#include "spip/DataBlock.h"

namespace spip {

  class DataBlockRead : public DataBlock
  {
    public:

      DataBlockRead (const char * key);

      ~DataBlockRead ();

      void lock ();

      void unlock ();

      char * read_header ();

      void close ();

      void * open_block ();

      ssize_t close_block (uint64_t nbytes);

      size_t read (void * ptr, size_t bytes);

      size_t seek (int64_t offset, int whence);

    protected:

    private:

  };

}

#endif
