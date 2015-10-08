/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPFormatCustom.h"

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <stdint.h>
#endif
#include <iostream>

using namespace std;

spip::UDPFormatCustom::UDPFormatCustom()
{
  packet_header_size = sizeof(ska1_udp_header_t);
  packet_data_size   = 4096;
  header
}

spip::UDPFormatCustom::~UDPFormatCustom()
{
  cerr << "spip::UDPFormatCustom::~UDPFormatCustom()" << endl;
}

void spip::UDPFormatCustom::generate_signal ()
{
}

static inline void spip::UDPFormatCustom::encode_seq_number (void * buf, size_t bufsz, uint64_t seq)
{
  memcpy (buf, (void *) &seq, sizeof(uint64_t));
}


inline void spip::UDPFormatCustom::encode_header (void * buf, size_t bufsz)
{
  encode_seq_number (buf, bufsz, packet_seq_number);

}



static inline uint64_t spip::UDPFormatCustom::decode_seq_number (void * buf, size_t bufsz)
{
  return ((uint64_t *) buf)[0];
}

void spip::UDPFormatCustom::decode_header (void * buf, size_t bufsz)
{
  packet_seq_number = decode_seq_number (buf, bufsz);

  uint32_t * buf32 = (uint32_t *) buf;
  integer_seconds    = buf32[2];
  fractional_seconds = buf32[3];

  uint16_t * buf16 = (uint16_t *) buf;
  channel_number = buf16[8];
  beam_number    = buf16[9];
  nsamp          = buf16[10];
  weights        = buf16[12];

  uint8_t * buf8 = (uint8_t *) buf;
  cbf_version    = buf8[22];
}
