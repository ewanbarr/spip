
#ifndef __UDPFormatMeerKATSPEAD_h
#define __UDPFormatMeerKATSPEAD_h

#include "spip/meerkat_def.h"
#include "spip/UDPFormat.h"

#define UDP_FORMAT_MEERKAT_SPEAD_PACKET_NSAMP 1024
#define UDP_FORMAT_MEERKAT_SPEAD_NDIM 2
#define UDP_FORMAT_MEERKAT_SPEAD_NPOL 2

#include <cstring>

namespace spip {

  typedef struct {

    uint8_t magic;

    uint8_t version;

    uint8_t item_pointer_width;

    uint8_t heap_addr_width;

    uint16_t reserved;

    uint16_t num_items;

  } spead_pkt_hdr_t;

  typedef struct {

    uint32_t item_address_mode : 1;

    uint32_t item_identifier : 23;

    uint32_t item_address : 40;

  } spead_item_pointer_t;

  typedef struct {

    spead_pkt_hdr_t pkt_hdr;

    uint32_t heap_id;

    uint32_t heap_size;

    uint32_t heap_offset;

    uint32_t payload_length;

    uint32_t fid;

  } spead_heap_hdr_t;

  class UDPFormatMeerKAT : public UDPFormat {

    public:

      UDPFormatMeerKAT ();

      ~UDPFormatMeerKAT ();

      void generate_signal ();

      uint64_t get_samples_for_bytes (uint64_t nbytes);

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

      static unsigned get_samples_per_packet () { return UDP_FORMAT_MEERKAT_SPEAD_PACKET_NSAMP; };

    private:

      spead_heap_hdr_t header;

      uint64_t nsamp_offset;

      uint64_t nsamp_per_sec;

  };

}

#endif
