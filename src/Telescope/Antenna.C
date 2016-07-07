/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/Antenna.h"

using namespace std;

spip::Antenna::Antenna ()
{
  scale = 1;
}

spip::Antenna::Antenna (std::string _name, double _x, double _y, double _delay, double _phase, double _scale)
{
  name = _name;
  x = _x;
  y = _y;
  instrumental_delay = _delay;
  phase_offset = _phase;
  scale = _scale;
}

spip::Antenna::~Antenna ()
{
}

double spip::Antenna::get_distance ()
{
  return sqrt (x*x + y*y);
}
