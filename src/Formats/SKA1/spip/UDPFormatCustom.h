
#ifndef __UDPFormatCustom_h
#define __UDPFormatCustom_h

#include "spip/ska1_def.h"
#include "spip/UDPFormat.h"

#define UDP_FORMAT_CUSTOM_PACKET_NSAMP 1024
#define UDP_FORMAT_CUSTOM_NDIM 2
#define UDP_FORMAT_CUSTOM_NPOL 2

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

  } ska1_custom_udp_header_t;


  class UDPFormatCustom : public UDPFormat {

    public:

      UDPFormatCustom ();

      ~UDPFormatCustom ();

      void generate_signal ();

      uint64_t get_samples_for_bytes (uint64_t nbytes);

      static void encode_seq (char * buf, size_t bufsz, uint64_t seq)
      {
        memcpy (buf, (void *) &seq, sizeof(uint64_t));
      };

      static inline uint64_t decode_seq (char * buf, size_t bufsz)
      {
        return ((uint64_t *) buf)[0];  
      };

      inline void encode_header_seq (char * buf, size_t bufsz, uint64_t packet_number);
      inline void encode_header (char * buf, size_t bufsz);

      inline uint64_t decode_header_seq (char * buf, size_t bufsz);
      inline void decode_header (char * buf, size_t bufsz);

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

      static unsigned get_samples_per_packet () { return UDP_FORMAT_CUSTOM_PACKET_NSAMP; };

    private:

      ska1_custom_udp_header_t header;

      uint64_t nsamp_offset;

      uint64_t nsamp_per_sec;

      unsigned nsamp_per_block;

      unsigned start_channel;

      unsigned end_channel;

      unsigned nchan;

      unsigned chanpol_stride;

  };

}

#endif
