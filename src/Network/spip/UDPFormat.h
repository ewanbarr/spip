
#ifndef __UDPFormat_h
#define __UDPFormat_h

#include <cstdlib>
#include <inttypes.h>

#define UDP_FORMAT_PACKET_NSAMP 1024;

namespace spip {

  class UDPFormat {

    public:

      UDPFormat();

      ~UDPFormat();

      void generate_signal ();

      void set_channel_range (unsigned start, unsigned end);

      void set_nsamp_per_block (unsigned _nsamp);

      virtual void gen_packet (char * buf, size_t bufsz) = 0;

      void encode_header (char * buf, size_t bufsz, uint64_t packet_number);

      void decode_header (char * buf, size_t bufsz, uint64_t *packet_number);

      void insert_packet (char * buf, uint64_t block_sample, char * pkt);

      unsigned get_samples_per_packet () { return UDP_FORMAT_PACKET_NSAMP; };

      unsigned get_header_size () { return packet_header_size; } ;

      unsigned get_data_size () { return packet_data_size; } ;

    protected:

      unsigned packet_header_size;

      unsigned packet_data_size;

      unsigned ndim;

      unsigned nsamp_per_block;

      unsigned start_channel;

      unsigned end_channel;

      unsigned nchan;

      unsigned chanpol_stride;

    private:

  };

}

#endif
