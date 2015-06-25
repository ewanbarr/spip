/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPFormat.h"

#include <iostream>

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <stdint.h>
#endif

using namespace std;

spip::UDPFormat::UDPFormat()
{
  packet_header_size = 8;
  packet_data_size   = 1464;
}

spip::UDPFormat::~UDPFormat()
{
  cerr << "spip::UDPFormat::~UDPFormat()" << endl;
}

void spip::UDPFormat::generate_signal ()
{

}

void spip::UDPFormat::encode_header (void * buf, size_t bufsz, uint64_t packet_number)
{
  char * b = (char *) buf;
  b[0] = (uint8_t) (packet_number>>56);
  b[1] = (uint8_t) (packet_number>>48);
  b[2] = (uint8_t) (packet_number>>40);
  b[3] = (uint8_t) (packet_number>>32);
  b[4] = (uint8_t) (packet_number>>24);
  b[5] = (uint8_t) (packet_number>>16);
  b[6] = (uint8_t) (packet_number>>8);
  b[7] = (uint8_t) (packet_number);
}

void spip::UDPFormat::decode_header (void * buf, size_t bufsz, uint64_t * packet_number)
{
  unsigned char * b = (unsigned char *) buf;
  uint64_t tmp = 0;
  unsigned i = 0;
  *packet_number = UINT64_C (0);
  for (i = 0; i < 8; i++ )
  {
    tmp = UINT64_C (0);
    tmp = b[8 - i - 1];
    *packet_number |= (tmp << ((i & 7) << 3));
  }
}


