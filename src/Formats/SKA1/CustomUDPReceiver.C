/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/CustomUDPReceiver.h"

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <stdint.h>
#endif
#include <stdlib.h>
#include <iostream>

using namespace std;

spip::CustomUDPReceiver::CustomUDPReceiver()
{
  packet_header_size = 8;
  packet_data_size   = 8192;
}

spip::CustomUDPReceiver::~CustomUDPReceiver()
{
  cerr << "spip::CustomUDPReceiver::~CustomUDPReceiver()" << endl;
}

void spip::CustomUDPReceiver::decode_header (void * buf, size_t bufsz, uint64_t * packet_number)
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

