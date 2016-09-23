
#ifndef __DataBlockWrite_h
#define __DataBlockWrite_h

#include "config.h"
#include "spip/DataBlock.h"

#ifdef HAVE_CUDA
#include <cuda_runtime.h>
#endif

namespace spip {

  class DataBlockWrite : public DataBlock
  {
    public:

      DataBlockWrite (const char * key);

      ~DataBlockWrite ();

#ifdef HAVE_CUDA
      void set_device (int id);
#endif

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

#ifdef HAVE_CUDA
      int dev_id;

      char * dev_ptr;

      size_t dev_bytes;

      cudaStream_t stream;
#endif

  };

}

#endif
