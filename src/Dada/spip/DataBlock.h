
#ifndef __DataBlock_h
#define __DataBlock_h

#include "ipcio.h"
#include "ipcbuf.h"

#include <cstddef>
#include <string>

namespace spip
{
  class DataBlock 
  {
    public:

      DataBlock (const char * key);

      ~DataBlock ();

      void connect ();

      void disconnect ();

      virtual void lock () = 0;

      virtual void unlock () = 0;

      virtual void * open_block () = 0;

      virtual ssize_t close_block (uint64_t bytes) = 0;

      const uint64_t get_data_bufsz () { return data_bufsz; };

      const uint64_t get_header_bufsz () { return header_bufsz; };

      inline const bool is_block_open () { return block_open; };

      inline const bool is_block_full () { return (curr_buf_bytes == data_bufsz); };

      const char * get_header() { return reinterpret_cast<const char *>(header); } ;

    protected:

      ipcbuf_t * header_block;

      ipcio_t * data_block;

      bool connected;

      bool locked;

      bool block_open;

      uint64_t header_bufsz;

      uint64_t data_bufsz;

      void * curr_buf;

      uint64_t curr_buf_id;

      uint64_t curr_buf_bytes;

      void * header;

      uint64_t header_size;

    private:

      key_t data_block_key;

      key_t header_block_key;


  };

}

#endif
