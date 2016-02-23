/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPStats.h"

#include <stdexcept>
#include <cstdlib>
#include <iostream>

using namespace std;

spip::UDPStats::UDPStats (unsigned _hdr, unsigned _data)
{
  data = _data;
  payload = _hdr + _data;
  reset ();
}

spip::UDPStats::~UDPStats ()
{
}

void spip::UDPStats::reset ()
{
  bytes_transmitted = 0;
  bytes_dropped = 0;
  nsleeps = 0;
}

void spip::UDPStats::increment ()
{
  increment_bytes (data);
}

void spip::UDPStats::increment_bytes (uint64_t nbytes)
{
  bytes_transmitted += nbytes;
}

void spip::UDPStats::dropped ()
{
  dropped_bytes (data);
}

void spip::UDPStats::dropped_bytes (uint64_t nbytes)
{
  bytes_dropped += nbytes;
}

void spip::UDPStats::dropped (uint64_t ndropped)
{
  dropped_bytes (ndropped * data);
}

void spip::UDPStats::sleeps (uint64_t to_add)
{
  nsleeps += to_add;
}
