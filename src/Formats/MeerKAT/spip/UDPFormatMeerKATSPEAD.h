
#ifndef __UDPFormatMeerKATSPEADSPEAD_h
#define __UDPFormatMeerKATSPEADSPEAD_h

#include "spip/meerkat_def.h"
#include "spip/UDPFormat.h"

#include "spead2/recv_packet.h"

#define UDP_FORMAT_MEERKAT_SPEAD_NDIM 2
#define UDP_FORMAT_MEERKAT_SPEAD_NPOL 1

#include <cstring>

static uint16_t magic_version = 0x5304;  // 0x53 is the magic, 4 is the version

namespace spip {

  typedef struct {

    uint16_t magic_version : 16;

    uint8_t item_pointer_width : 8;

    uint8_t heap_addr_width : 8;

    uint16_t reserved: 16;

    int16_t num_items;

  } spead_hdr_t;

  typedef struct {

    //char  item_address_mode : 1;

    uint16_t item_identifier : 16;

    uint64_t item_address : 48;

  } spead_item_pointer_t;

  typedef struct {

    spead2::recv::packet_header hdr;

    spead_hdr_t spead_hdr;

    spead_item_pointer_t heap_number;

    spead_item_pointer_t heap_length;

    spead_item_pointer_t payload_offset_in_heap;

    spead_item_pointer_t payload_length;

    spead_item_pointer_t timestamp;

    spead_item_pointer_t frequency_channel;

    spead_item_pointer_t f_engine_flags;

    spead_item_pointer_t beam_number;

  } meerkat_spead_udp_hdr_t;

  class UDPFormatMeerKATSPEAD : public UDPFormat {

    public:

      UDPFormatMeerKATSPEAD ();

      ~UDPFormatMeerKATSPEAD ();

      void generate_signal ();

      uint64_t get_samples_for_bytes (uint64_t nbytes);

      void set_channel_range (unsigned start, unsigned end);

      int64_t get_timestamp_fast ();

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
      inline void decode_header (char * buf);

      inline int insert_packet (char * buf, char * pkt, uint64_t start_samp, uint64_t next_samp);

      void print_item_pointer (spead_item_pointer_t item);
      void print_packet_header ();

      inline void gen_packet (char * buf, size_t bufsz);

      // accessor methods for header params
      void set_heap_num (int64_t heap_num ) { header.heap_cnt = heap_num * 8192; };
      void set_chan_no (int16_t chan_no)    { ; };
      void set_beam_no (int16_t beam_no)    { ; };

      static unsigned get_samples_per_packet () { return 1; };

    private:

      spead2::recv::packet_header header;

      uint64_t obs_start_sample;

      uint64_t nsamp_per_sec;

      unsigned nsamp_per_heap;

      unsigned nbytes_per_samp;

      unsigned avg_pkt_size;

      unsigned pkts_per_heap;

      int64_t curr_heap_cnt;

      uint64_t curr_sample_number;

      uint64_t curr_heap_offset;

      uint64_t curr_heap_number;

      unsigned nbytes_per_heap;

      unsigned timestamp_to_samples;

  };

}

#endif
