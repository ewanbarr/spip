/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPStats.h"

#include <stdexcept>
#include <cstdlib>

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
  pkts_transmitted = 0;
  pkts_dropped = 0;
}

void spip::UDPStats::increment ()
{
  pkts_transmitted ++;
}

void spip::UDPStats::dropped ()
{
  pkts_dropped ++;
}
