
#ifndef __UDPFormatBPSR_h
#define __UDPFormatBPSR_h

#define NO_1PPS_RESET

#include "spip/bpsr_def.h"
#include "spip/UDPFormat.h"
#include "spip/AsciiHeader.h"

#include <cstring>

namespace spip {

  class UDPFormatBPSR : public UDPFormat {

    public:

      UDPFormatBPSR ();

      ~UDPFormatBPSR ();

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

      inline int64_t decode_packet (char * buf, unsigned *payload_size);
      inline int insert_last_packet (char * buf);

      int64_t decode_packet_seq (char* buf);

      void print_packet_header ();
      void print_packet_timestamp ();

      inline void gen_packet (char * buf, size_t bufsz);

      static unsigned get_samples_per_packet () { return 1; };

    private:

      //! pointer to most recently decoded packet payload
      char * payload;

      unsigned acc_len;

      unsigned seq_inc;

      uint64_t seq_no;

      uint64_t seq_offset;

      uint64_t seq_to_byte;

      unsigned offset;

      double tsamp;

      uint64_t adc_sync_time;

#ifdef NO_1PPS_RESET
      uint64_t start_seq_no;
#endif

      bool started;
  };


}

#endif
