/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/Channel.h"

using namespace std;

spip::Channel::Channel (unsigned _number, double _cfreq, double _bw)
{
  number = _number;
  cfreq = _cfreq;
  bw = _bw;
}
