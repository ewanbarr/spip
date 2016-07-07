/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/ContainerRing.h"

#include <cstring>
#include <stdexcept>

using namespace std;

spip::ContainerRing::ContainerRing (uint64_t _size)
{
  buffer_valid = false;
  size = _size;
}

spip::ContainerRing::~ContainerRing ()
{
}

void spip::ContainerRing::resize ()
{
  uint64_t required_size = calculate_buffer_size ();
  if (required_size > size)
  {
    throw runtime_error ("required size for container not equal to ring buffer size");
  }
}

void spip::ContainerRing::zero ()
{
  if (buffer_valid)
    bzero (buffer, size);
  else
    throw runtime_error ("cannot zero an invalid buffer");
}

void spip::ContainerRing::set_buffer (unsigned char * buf)
{
  buffer = buf;
  buffer_valid = true;
}

void spip::ContainerRing::unset_buffer ()
{
  buffer = NULL;
  buffer_valid = false;
}
