
#ifndef __UDPFormat_h
#define __UDPFormat_h

#include <cstdlib>
#include <inttypes.h>

namespace spip {

  class UDPFormat {

    public:

      UDPFormat();

      ~UDPFormat();

      void generate_signal ();

      void encode_header (void * buf, size_t bufsz, uint64_t packet_number);

      void decode_header (void * buf, size_t bufsz, uint64_t *packet_number);

      unsigned get_header_size () { return packet_header_size; } ;

      unsigned get_data_size () { return packet_data_size; } ;

    protected:

      unsigned packet_header_size;

      unsigned packet_data_size;

    private:

  };

}

#endif
