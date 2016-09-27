
#ifndef __UDPFormat_h
#define __UDPFormat_h

#include <cstdlib>
#include <inttypes.h>

#include "spip/AsciiHeader.h"
#include "spip/Time.h"

#define UDP_FORMAT_PACKET_NSAMP 1024;

#define UDP_PACKET_TOO_LATE -1
#define UDP_PACKET_TOO_EARLY -2
#define UDP_PACKET_IGNORE -3

namespace spip {

  class UDPFormat {

    public:

      UDPFormat();

      ~UDPFormat();

      virtual void configure (const AsciiHeader& config, const char* suffix) = 0;

      virtual void prepare (AsciiHeader& header, const char* suffix) = 0;

      virtual void conclude () = 0;

      void reset () { prepared = false; } ;

      bool is_configured() { return configured; } ;

      bool is_prepared() { return prepared; } ;
      
      void set_configured() { configured = true; };

      void set_prepared() { prepared = true; };

      virtual void gen_packet (char * buf, size_t bufsz) = 0;

      virtual uint64_t get_samples_for_bytes (uint64_t nbytes) = 0;

      virtual void encode_header_seq (char * buf, uint64_t packet_number) = 0;

      virtual void encode_header (char * buf) = 0;

      virtual int64_t decode_packet (char * buf, unsigned * payload_size) = 0;

      virtual int insert_last_packet (char * buf) = 0;

      virtual void print_packet_header () = 0;

      virtual uint64_t get_resolution () = 0;

      unsigned get_samples_per_packet () { return UDP_FORMAT_PACKET_NSAMP; };

      unsigned get_header_size () { return packet_header_size; } ;

      unsigned get_data_size () { return packet_data_size; } ;

      double rand_normal (double mean, double stddev);

      void generate_noise_buffer (int nbits);

      void fill_noise (char * buf, size_t nbytes);

      void set_noise_buffer_size (unsigned nbytes);

      //! set whether this format will determine the UTC_START itself
      void set_self_start (bool ss) { self_start = ss; };

      //! get whether this format is self starting
      bool get_self_start () { return self_start; };

      //! return the UTC second start time for this format
      Time get_utc_start () { return utc_start; };

      //! return the offset from the utc_startin pico seconds
      uint64_t get_pico_seconds () { return pico_seconds; };

      //void set_start_sample (uint64_t s) { start_sample = s; };

    protected:

      //! start time of the observation
      Time utc_start;

      //! offset from start time in pico seconds
      uint64_t pico_seconds;

      //! size of the header in the UDP packet payload
      unsigned packet_header_size;

      //! size of the data in the UDP packet payload
      unsigned packet_data_size;

      unsigned ndim;

      unsigned npol;

      unsigned nbit;

      unsigned nsamp_per_block;

      unsigned start_channel;

      unsigned end_channel;

      unsigned nchan;

      double n2;
      
      double n2_cached;
        
      char * noise_buffer;

      size_t noise_buffer_size;

      bool configured;

      bool prepared;

      bool self_start;

    private:

  };

}

#endif
