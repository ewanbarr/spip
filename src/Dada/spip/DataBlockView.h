
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

      // get EOD flag on the data block
      int eod ();

      int view_eod (uint64_t byte_resolution);

      void * get_curr_buf ();

      void read_header ();

      int64_t read (void * ptr, uint64_t bytes);

      size_t seek (int64_t offset, int whence);

    protected:

    private:

  };

}

#endif
