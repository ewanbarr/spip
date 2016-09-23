/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/BlockFormatUWB.h"
#include <math.h>

#include <cstdlib>
#include <cstdio>
#include <iostream>

using namespace std;

spip::BlockFormatUWB::BlockFormatUWB()
{
  nchan = 1;
  npol = 2;
  ndim = 2;
  nbit = 16;
}

spip::BlockFormatUWB::~BlockFormatUWB()
{
}

void spip::BlockFormatUWB::unpack_hgft (char * buffer, uint64_t nbytes)
{
  const unsigned nsamp = nbytes / bytes_per_sample;
  const unsigned nsamp_per_time = nsamp / ntime;

  int16_t * in = (int16_t *) buffer;

  uint64_t isamp = 0;
  unsigned ichan = 0;
  unsigned ifreq = 0;
  unsigned ibin, itime, power;
  int re, im;

  for (unsigned isamp=0; isamp<nsamp; isamp++)
  {
    for (unsigned ipol=0; ipol<npol; ipol++)
    {
      re = (int) in[isamp];
      im = (int) in[isamp+1];

      sums[ipol*ndim + 0] += (float) re;
      sums[ipol*ndim + 1] += (float) im;

      ibin = re + 32768;
      hist[ipol][0][ifreq][ibin]++;

      ibin = im + 32768;
      hist[ipol][1][ifreq][ibin]++;

      // detect and average the timesamples into a NPOL sets of NCHAN * 512 waterfalls
      power = (unsigned) ((re * re) + (im * im));
      itime = isamp / nsamp_per_time;
      freq_time[ipol][ichan][itime] += power;

      isamp += 2;
    }
  }
}



void spip::BlockFormatUWB::unpack_ms(char * buffer, uint64_t nbytes)
{
  const unsigned nsamp = nbytes / bytes_per_sample;
  float ndat = (float) (nsamp * nchan);

  for (unsigned i=0; i<npol * ndim; i++)
    means[i] = sums[i] / ndat;

  int16_t * in = (int16_t *) buffer;
  uint64_t isamp = 0;
  int re, im;
  float diff;

#ifdef _DEBUG
  cerr << "spip::BlockFormatUWB::unpack_ms nsamp=" << nsamp << endl;
  cerr << "spip::BlockFormatUWB::unpack_ms nchan_per_freq=" << nchan_per_freq << " nblock=" << nblock << end
l;
#endif

  for (unsigned isamp=0; isamp<nsamp; isamp++)
  {
    for (unsigned ipol=0; ipol<npol; ipol++)
    {
      unsigned idx = ipol*ndim + 0;
      re = (int) in[isamp];
      diff = (float) re - means[idx];
      variances[idx] += diff * diff;

      idx = ipol*ndim + 1;
      im = (int) in[isamp+1];
      diff = (float) im - means[idx];
      variances[idx] += diff * diff;

      isamp += 2;
    }
  }

  for (unsigned i=0; i<npol * ndim; i++)
  {
    variances[i] /= ndat;
    stddevs[i] = sqrtf (variances[i]);
  }
}
