
#ifndef __UDPFormatMeerKATSimple_h
#define __UDPFormatMeerKATSimple_h

#include "spip/meerkat_def.h"
#include "spip/UDPFormat.h"

#define UDP_FORMAT_SIMPLE_PACKET_NSAMP 1024
#define UDP_FORMAT_SIMPLE_NDIM 2
#define UDP_FORMAT_SIMPLE_NPOL 2

#include <cstring>

namespace spip {

  typedef struct {

    uint64_t seq_number;

    uint32_t integer_seconds;

    uint32_t fractional_seconds;

    uint16_t channel_number;

    uint16_t beam_number;

    uint16_t nsamp;

    uint8_t  cbf_version;

    uint8_t  reserved_01;

    uint16_t weights;

    uint16_t reserved_02;

    uint16_t reserved_03;

    uint16_t reserved_04;

  } meerkat_simple_udp_header_t;


  class UDPFormatMeerKATSimple : public UDPFormat {

    public:

      UDPFormatMeerKATSimple ();

      ~UDPFormatMeerKATSimple ();

      void configure (const spip::AsciiHeader& config, const char* suffix);

      void prepare (const spip::AsciiHeader& header, const char* suffix);

      void generate_signal ();

      uint64_t get_samples_for_bytes (uint64_t nbytes);

      uint64_t get_resolution ();

      void set_channel_range (unsigned start, unsigned end);

      static void encode_seq (char * buf, uint64_t seq)
      {
        memcpy (buf, (void *) &seq, sizeof(uint64_t));
      };

      static inline uint64_t decode_seq (char * buf)
      {
        return ((uint64_t *) buf)[0];  
      };

      inline void encode_header_seq (char * buf, uint64_t packet_number);
      inline void encode_header (char * buf);

      inline uint64_t decode_header_seq (char * buf);
      inline unsigned decode_header (char * buf);

      inline int64_t decode_packet (char * buf, unsigned *payload_size);
      inline int insert_last_packet (char * buf);

      inline int check_packet ();
      inline int insert_packet (char * buf, char * pkt, uint64_t start_samp, uint64_t next_samp);

      void print_packet_header ();

      inline void gen_packet (char * buf, size_t bufsz);

      // accessor methods for header params
      void set_seq_num (uint64_t seq_num) { header.seq_number = seq_num; };
      void set_int_sec (uint32_t int_sec) { header.integer_seconds = int_sec; };
      void set_fra_sec (uint32_t fra_sec) { header.fractional_seconds = fra_sec; };
      void set_chan_no (uint16_t chan_no) { header.channel_number = chan_no; };
      void set_beam_no (uint16_t beam_no) { header.beam_number = beam_no; };
      void set_nsamp   (uint16_t nsamp)   { header.nsamp = nsamp; };
      void set_weights (uint16_t weights) { header.weights = weights; };
      void set_cbf_ver (uint8_t  cbf_ver) { header.cbf_version = cbf_ver; };

      static unsigned get_samples_per_packet () { return UDP_FORMAT_SIMPLE_PACKET_NSAMP; };

    private:

      meerkat_simple_udp_header_t header;

      uint64_t nsamp_offset;

      uint64_t nsamp_per_sec;

      uint64_t prev_packet_number;

      uint64_t curr_packet_number;

  };

}

#endif
