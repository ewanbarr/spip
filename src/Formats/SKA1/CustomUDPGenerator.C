/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/CustomUDPGenerator.h"

#define __STDC_CONSTANT_MACROS
#include <stdint.h>
#include <iostream>

using namespace std;

spip::CustomUDPGenerator::CustomUDPGenerator()
{
  packet_header_size = 8;
  packet_data_size   = 8192;
}

spip::CustomUDPGenerator::~CustomUDPGenerator()
{
  cerr << "spip::CustomUDPGenerator::~CustomUDPGenerator()" << endl;
}

void spip::CustomUDPGenerator::generate_signal ()
{
}

void spip::CustomUDPGenerator::encode_header (void * buf, size_t bufsz, uint64_t packet_number)
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

void spip::CustomUDPGenerator::decode_header (void * buf, size_t bufsz, uint64_t * packet_number)
{
  char * b = (char *) buf;
  uint64_t tmp = 0;
  unsigned i = 0;
  *packet_number = 0;
  for (i = 0; i < 8; i++ )
  {
    tmp = 0;
    tmp = b[8 - i - 1];
    *packet_number |= (tmp << ((i & 7) << 3));
  }
}

