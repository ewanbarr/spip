
#ifndef __DataBlockWrite_h
#define __DataBlockWrite_h

#include "spip/DataBlock.h"

namespace spip {

  class DataBlockWrite : public DataBlock
  {
    public:

      DataBlockWrite (const char * key);

      ~DataBlockWrite ();

      void open ();

      void lock ();

      void page ();

      void close();

      void unlock ();

      void write_header (const char* header);

      ssize_t write_data (void * ptr, size_t bytes);

      void * open_block ();

      ssize_t close_block (uint64_t bytes);

      ssize_t update_block (uint64_t bytes);

      void zero_next_block ();

    protected:

    private:

  };

}

#endif
