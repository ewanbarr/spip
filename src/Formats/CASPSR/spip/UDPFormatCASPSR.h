
#ifndef __UDPFormatCASPSR_h
#define __UDPFormatCASPSR_h

#include "spip/caspsr_def.h"
#include "spip/UDPFormat.h"
#include "spip/AsciiHeader.h"

#include <cstring>

#define NO_1PPS_RESET

namespace spip {

  class UDPFormatCASPSR : public UDPFormat {

    public:

      UDPFormatCASPSR ();

      ~UDPFormatCASPSR ();

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

      //! correction to raw sequence number so that it is modulo 8192 bytes
      int64_t global_offset;

      int64_t adc_sync_time;

      // 
      uint64_t seq_no;

      uint64_t ch_id;

      uint64_t seq_inc;

      uint64_t seq_offset;

      uint64_t start_seq_no;

      uint64_t seq_to_byte;

      unsigned offset;

      double tsamp;

      bool started;
  };


}

#endif
