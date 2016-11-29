
#ifndef __UDPFormatMeerKATSPEADSPEAD_h
#define __UDPFormatMeerKATSPEADSPEAD_h

#include "spip/meerkat_def.h"
#include "spip/UDPFormat.h"
#include "spip/AsciiHeader.h"

#include "spead2/recv_packet.h"
#include "spead2/recv_udp.h"

#define UDP_FORMAT_MEERKAT_SPEAD_NDIM 2
#define UDP_FORMAT_MEERKAT_SPEAD_NPOL 1

#include <cstring>

static uint16_t magic_version = 0x5304;  // 0x53 is the magic, 4 is the version

namespace spip {

  class UDPFormatMeerKATSPEAD : public UDPFormat {

    public:

      UDPFormatMeerKATSPEAD ();

      ~UDPFormatMeerKATSPEAD ();

      void configure (const spip::AsciiHeader& config, const char* suffix);

      void prepare (spip::AsciiHeader& header, const char* suffix);

      void conclude ();

      void generate_signal ();

      uint64_t get_samples_for_bytes (uint64_t nbytes);

      uint64_t get_resolution ();

      void set_channel_range (unsigned start, unsigned end);

      int64_t get_timestamp_fast ();

      static void encode_seq (char * buf, uint64_t seq)
      {
        memcpy (buf, (void *) &seq, sizeof(uint64_t));
      };

      inline void encode_header_seq (char * buf, uint64_t packet_number);
      inline void encode_header (char * buf);

      void decode_spead (char * buf);
      inline int64_t decode_packet (char * buf, unsigned *payload_size);
      inline int insert_last_packet (char * buf);

      void print_packet_header ();
      void print_packet_timestamp ();
      bool check_stream_stop ();


      inline void gen_packet (char * buf, size_t bufsz);

      // accessor methods for header params
      void set_heap_num (int64_t heap_num ) { header.heap_cnt = heap_num * 8192; };
      void set_chan_no (int16_t chan_no)    { ; };
      void set_beam_no (int16_t beam_no)    { ; };

      static unsigned get_samples_per_packet () { return 1; };

    private:

      spead2::recv::packet_header header;

      time_t adc_sync_time;

      uint64_t adc_sample_rate;

      uint64_t adc_samples_per_heap;

      int64_t obs_start_sample;

      double samples_to_byte_offset;

      double bw;

      double tsamp;

      double adc_to_cbf;

      uint64_t nsamp_per_sec;

      unsigned nsamp_per_heap;

      unsigned nbytes_per_samp;

      unsigned avg_pkt_size;

      unsigned heap_size;

      unsigned pkts_per_heap;

      int64_t curr_heap_cnt;

      uint64_t curr_sample_number;

      uint64_t curr_heap_offset;

      uint64_t curr_heap_number;

      uint64_t curr_heap_bytes;

      unsigned nbytes_per_heap;

      unsigned timestamp_to_samples;

      bool first_heap;

      bool first_packet;

      unsigned header_npol;

      int offset ;

  };


}

#endif
