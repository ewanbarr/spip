/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/FractionalDelay.h"

#include <stdexcept>
#include <cmath>

using namespace std;

spip::FractionalDelay::FractionalDelay () : Transformation<Container,Container>("FractionalDelay", outofplace)
{
  delays = new spip::ContainerRAM ();
  phases = new spip::ContainerRAM ();
  firs = new spip::ContainerRAM ();

  ntap = 0;
}

void spip::FractionalDelay::prepare (unsigned _ntap)
{
  ndat  = input->get_ndat ();
  nchan = input->get_nchan ();
  npol  = input->get_npol ();
  nbit  = input->get_nbit ();
  ndim  = input->get_ndim ();
  nsignal = input->get_nsignal ();

  delays->set_nbit (sizeof(float));
  delays->set_nsignal (nsignal);
  delays->zero ();

  ntap = _ntap;
  half_ntap = ntap / 2;

  firs->set_nbit (sizeof(float));
  firs->set_ndat (ntap);
  firs->set_nsignal (nsignal);
  firs->zero ();

  phases->set_nchan (nchan);
  phases->set_ndim (1);
  phases->set_npol (1);
  phases->set_nbit (sizeof(float));
  phases->set_ndat (1);
  phases->set_nsignal (nsignal);
  phases->zero ();
}

void spip::FractionalDelay::set_delay (unsigned isig, float delay)
{
  if (isig >= delays->get_nsignal())
    throw invalid_argument ("FractionalDelay::set_delay isig > nsignal");

  float * buffer = (float *) delays->get_buffer();
  buffer[isig] = delay;
}

void spip::FractionalDelay::set_phase (unsigned isig, unsigned ichan, float phase)
{
  if (isig >= phases->get_nsignal())
    throw invalid_argument ("FractionalDelay::set_phase isig > nsignal");
  if (ichan >= phases->get_nchan())
    throw invalid_argument ("FractionalDelay::set_phase isig > nsignal");

  float * buffer = (float *) phases->get_buffer();
  buffer[isig*nchan+ichan] = phase;
}

//! simply copy input buffer to output buffer
void spip::FractionalDelay::transformation ()
{
  // each iteration the FIR coefficients must be recomputed since the 
  // geometric delay will have changed
  compute_fir_coeffs();

  // convert the input bits rate to floating point
  unpack_input ();

  // perform FIR delay
  transform ();

  repack_output ();
}

void spip::FractionalDelay::compute_fir_coeffs ()
{
  unsigned half_ntap = ntap / 2;
  float x, window, sinc;

  float * ds = (float *) delays->get_buffer();
  float * fs = (float *) firs->get_buffer();

  for (unsigned isig=0; isig<nsignal; isig++)
  {
    for (unsigned itap=0; itap<ntap; itap++)
    {
      x = (float) itap - (float) ds[isig];
      window = 0.54 - 0.46 * cos (2.0 * M_PI * (x+0.5) / (float) ntap);
      sinc   = 1.0f;

      if (x != half_ntap)
      {
        x -= half_ntap;
        x *= M_PI;
        sinc = sinf(x) / x;
      }
      fs[isig*ntap + itap] = sinc * window;
    }
  } 
}

void spip::FractionalDelay::unpack_input ()
{
  if (nbit == 8)
    unpack ((int8_t *) input->get_buffer(), (float *) unpacked->get_buffer());
  else if (nbit == 16)
    unpack ((int16_t *) input->get_buffer(), (float *) unpacked->get_buffer());
  else if (nbit == 32)
    unpack ((float *) input->get_buffer(), (float *) unpacked->get_buffer());
  else
    throw runtime_error ("FractionalDelay::unpack unsupported bit-rate");
}

void spip::FractionalDelay::transform ()
{
  float * un    = (float *) unpacked->get_buffer();
  float * out   = (float *) output->get_buffer();

  float * phasors = (float *) phases->get_buffer();
  float phasor_re, phasor_im;

  const uint64_t un_ndat = 2 * unpacked->get_ndat();
  const uint64_t out_ndat = 2 * output->get_ndat();

  for (unsigned ichan=0; ichan<nchan; ichan++)
  {
    for (unsigned ipol=0; ipol<npol; ipol++)
    {
      for (unsigned isig=0; isig<nsignal; isig++)
      {
        // fir coeffs for the signal
        float * fs = (float *) firs->get_buffer() + (isig * ntap);

        // compute the phase rotator from the phase angle
        sincosf (phasors[isig*nchan+ichan], &phasor_re, &phasor_im);

        for (uint64_t idat=half_ntap; idat<(ndat-half_ntap); idat++)
        {
          register float sum_re = 0;
          register float sum_im = 0;

          unsigned offset_re = 2*(idat-half_ntap);
          unsigned offset_im = 2*(idat-half_ntap) + 1;
 
          // compute the FIR interpolated value
          for (int64_t itap=0; itap<ntap; itap++)
          {
            sum_re += un[offset_re + itap] * fs[itap];
            sum_im += un[offset_im + itap] * fs[itap];
          }
         
          // multiply by phase angle 
          out[offset_re] = sum_re * phasor_re - sum_im * phasor_im;
          out[offset_im] = sum_im * phasor_re + sum_re * phasor_im;
        }

        un  += un_ndat;
        out += out_ndat;
      }
    }
  }
  return;
}

void spip::FractionalDelay::repack_output()
{
  if (nbit == 8)
    repack ((float *) unpacked->get_buffer(), (int8_t *) output->get_buffer());
  else if (nbit == 16)
    repack ((float *) unpacked->get_buffer(), (int16_t *) output->get_buffer());
  else if (nbit == 32)
    repack ((float *) unpacked->get_buffer(), (float *) output->get_buffer());
  else
    throw runtime_error ("FractionalDelay::repack unsupported bit-rate");
}

