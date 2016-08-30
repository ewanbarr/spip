/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/Container.h"

using namespace std;

spip::Container::Container ()
{
  ndat = 1;
  nchan = 1;
  nsignal = 1;
  ndim = 1;
  npol = 1;
  nbit = 8;

  buffer = NULL;
}

spip::Container::~Container ()
{
}

size_t spip::Container::calculate_buffer_size ()
{
  return size_t (ndat * nchan * nsignal * ndim * npol * nbit) / 8;
}
