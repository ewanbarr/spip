
#ifndef __UDPFormat_h
#define __UDPFormat_h

#include <cstdlib>
#include <inttypes.h>

#define UDP_FORMAT_PACKET_NSAMP 1024;

#define UDP_PACKET_TOO_LATE -1
#define UDP_PACKET_TOO_EARLY -2
#define UDP_PACKET_IGNORE -3

namespace spip {

  class UDPFormat {

    public:

      UDPFormat();

      ~UDPFormat();

      void generate_signal ();

      virtual void set_channel_range (unsigned start, unsigned end) = 0;

      void set_nsamp_per_block (unsigned _nsamp);

      virtual void gen_packet (char * buf, size_t bufsz) = 0;

      virtual uint64_t get_samples_for_bytes (uint64_t nbytes) = 0;

      virtual void encode_header_seq (char * buf, uint64_t packet_number) = 0;

      virtual void encode_header (char * buf) = 0;

      virtual uint64_t decode_header_seq (char * buf) = 0;

      virtual unsigned decode_header (char * buf) = 0;

      virtual int check_packet () = 0;

      virtual int insert_packet (char * buf, char * pkt, uint64_t start_samp, uint64_t next_samp) = 0;

      virtual void print_packet_header () = 0;

      virtual uint64_t get_resolution () = 0;

      unsigned get_samples_per_packet () { return UDP_FORMAT_PACKET_NSAMP; };

      unsigned get_header_size () { return packet_header_size; } ;

      unsigned get_data_size () { return packet_data_size; } ;

      double rand_normal (double mean, double stddev);

      void generate_noise_buffer (int nbits);

      void fill_noise (char * buf, size_t nbytes);

    protected:

      unsigned packet_header_size;

      unsigned packet_data_size;

      unsigned ndim;

      unsigned npol;

      unsigned nbit;

      unsigned nsamp_per_block;

      unsigned start_channel;

      unsigned end_channel;

      unsigned nchan;

      unsigned channel_stride;

      unsigned chanpol_stride;

      double n2;
      
      double n2_cached;
        
      char * noise_buffer;

      size_t noise_buffer_size;

    private:

  };

}

#endif
