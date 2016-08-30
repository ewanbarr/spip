/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/ContainerRAM.h"

#include <cstring>

using namespace std;

spip::ContainerRAM::ContainerRAM ()
{
}

spip::ContainerRAM::~ContainerRAM ()
{
  // deallocate any buffer
  if (buffer)
    free (buffer);
}

void spip::ContainerRAM::resize ()
{
  uint64_t required_size = calculate_buffer_size ();
  if (required_size > size)
  {
    if (buffer)
      free (buffer);
    buffer = (unsigned char *) malloc (required_size);
  }
}

void spip::ContainerRAM::zero ()
{
  bzero (buffer, size);
}
