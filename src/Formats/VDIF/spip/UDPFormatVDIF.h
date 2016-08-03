/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#ifndef __UDPFormatVDIF_h
#define __UDPFormatVDIF_h

#include "spip/vdifio.h"
#include "spip/UDPFormat.h"

#include <cstring>

namespace spip {

  class UDPFormatVDIF : public UDPFormat {

    public:

      UDPFormatVDIF (int pps = -1);

      ~UDPFormatVDIF ();

      void configure (const AsciiHeader& config, const char* suffix);

      void prepare (AsciiHeader& header, const char* suffix);


      void conclude ();

      unsigned get_samples_per_packet () { return nsamp_per_packet; };

      void generate_signal ();

      uint64_t get_samples_for_bytes (uint64_t nbytes);

      uint64_t get_resolution ();

      void set_channel_range (unsigned start, unsigned end);

      inline void encode_header_seq (char * buf, uint64_t packet_number);
      inline void encode_header (char * buf);

      inline int64_t decode_packet (char * buf, unsigned * payload_size);

      inline uint64_t decode_header_seq (char * buf);
      inline void decode_header (char * buf);

      inline int insert_last_packet (char * buf);

      void print_packet_header ();

      inline void gen_packet (char * buf, size_t bufsz);
      
      //! encode header with config data
      void compute_header ();

    private:

      vdif_header header;

      char * payload;

      uint64_t nsamp_offset;

      unsigned nsamp_per_packet;

      unsigned packets_per_second;

      uint64_t bytes_per_second;

      double bw;

      double tsamp;

      //! second (since VDIF epoch) that the observation began
      int start_second;

      bool configured_stream;
  };

}

#endif
