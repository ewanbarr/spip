
#ifndef __UDPFormatVDIF_h
#define __UDPFormatVDIF_h

#include "spip/ska1_def.h"
#include "spip/UDPFormat.h"

#include "vdif_header.h"

#include <cstring>

namespace spip {

  class UDPFormatVDIF : public UDPFormat {

    public:

      UDPFormatVDIF (int pps);

      ~UDPFormatVDIF ();

      static unsigned get_samples_per_packet () { return UDP_FORMAT_VDIF_PACKET_NSAMP; };

      static void encode_seq (char * buf, uint64_t seq)
      {
        memcpy (buf, (void *) &seq, sizeof(uint64_t));
      };

      static inline uint64_t decode_seq (char * buf)
      {
        return ((uint64_t *) buf)[0];
      };

      void generate_signal ();

      uint64_t get_samples_for_bytes (uint64_t nbytes);

      uint64_t get_resolution ();

      void set_channel_range (unsigned start, unsigned end);


      inline void encode_header_seq (char * buf, uint64_t packet_number);
      inline void encode_header (char * buf);

      inline uint64_t decode_header_seq (char * buf);
      inline void decode_header (char * buf);

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


    private:

      vdif_header header;

      uint64_t nsamp_offset;

      int nsamp_per_sec;

      int packets_per_sec;

  };

}

#endif
