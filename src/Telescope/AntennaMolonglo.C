/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/AntennaMolonglo.h"

#include <sstream>

using namespace std;

spip::AntennaMolonglo::AntennaMolonglo ()
{
}

spip::AntennaMolonglo::AntennaMolonglo (const char * config_line)
{
  string line = config_line;
  stringstream ss(line);

  ss >> name;
  ss >> dist;
  ss >> instrumental_delay;
  ss >> scale;
  ss >> phase_offset;
}


spip::AntennaMolonglo::AntennaMolonglo (std::string _name, 
    double _dist, double _delay, double _phase, double _scale)
{
  name = _name;
  instrumental_delay = _delay;
  phase_offset = _phase;
  scale = _scale;
  dist = _dist;

  if (name[0] == 'E' || name[0] == 'e')
    dist *= -1; 

  // ultimately we should convert to something independent of array
  // slope and with azimuth correction
  x = dist;
  y = 0;
}

void spip::AntennaMolonglo::set_bay (string _bay)
{
  bay = _bay; 
  if (bay[0] == 'R')
    bay_idx = 0;
  if (bay[0] == 'Y')
    bay_idx = 1;
  if (bay[0] == 'G')
    bay_idx = 2;
  if (bay[0] == 'B')
    bay_idx = 3;
}

double spip::AntennaMolonglo::get_dist ()
{
  return dist; 
}
