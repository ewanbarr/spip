
#ifndef __UDPFormatSKA1_h
#define __UDPFormatSKA1_h

#include "spip/ska1_def.h"
#include "spip/UDPFormat.h"

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

  } ska1_udp_header_t;


  class UDPFormatSKA1 : public UDPFormat {

    public:

      UDPFormatSKA1 ();

      ~UDPFormatSKA1 ();

      void generate_signal ();

      static inline void encode_header (void * buf, uint64_t packet_number);

      static inline void decode_header (void * buf, uint64_t * packet_number);

      uint64_t get_resolution ();


    private:

      ska1_udp_header_t header;

  };

}

#endif
