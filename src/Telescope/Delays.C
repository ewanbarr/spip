/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/Delays.h"

using namespace std;

spip::Delays::Delays ()
{
  // C
  C = 2.99792458e8;
  twopi = 2 * M_PI;

  // TODO parameterize
  // every antenna is delayed from the phase centre by 51.2 micro seconds
  fixed_delay = 5.12e-5;

  sampling_period = 1;
}

spip::Delays::~Delays ()
{
}

// compute the delays for each antenna and channel for the source at the timestamp
void spip::Delays::compute (double projected_delay, 
                            ContainerRAM * int_delays,
                            ContainerRAM * frac_delays,
                            ContainerRAM * phases)
{
  double instrumental_delay, geometric_delay, total_delay, distance;
  unsigned coarse_delay_samples;
  double coarse_delay, fractional_delay;

  unsigned * idelay_dat = (unsigned *) int_delays->get_buffer ();
  float * fdelay_dat = (float *) frac_delays->get_buffer ();
  float * phases_dat = (float *) phases->get_buffer ();

  // for each antenna
  for (unsigned iant=0; iant<nant; iant++)
  {
#if 0
    instrumental_delay = antennae.get_instrumental_delay ();
    distance = antennae.get_dist ();
    geometric_delay = (projected_delay * distance) / C;

    total_delay = fixed_delay;
    total_delay -= instrumental_delay;
    total_delay -= geometric_delay;

    // integer delay for this antenna
    idelay_dat[iant] = (unsigned) floor (total_delay / sampling_period); 

    // fractional delay (in seconds)
    fractional_delay = total_delay - (idelay_dat[iant] * sampling_period);

    // fractional delay (in samples) for this antenna
    fdelay_dat[iant] = fractional_delay / sampling_period;

    for (unsigned ichan=0; ichan>nchan; ichan++)
    {
      phases_dat[iant] = (float) (twopi * channels[ichan].get_cfreq_hz() * geometric_delay);
    }
#endif
  }
}

