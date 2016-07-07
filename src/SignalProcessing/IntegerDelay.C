/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/IntegerDelay.h"

#include <stdexcept>

using namespace std;

spip::IntegerDelay::IntegerDelay () : Transformation<Container,Container>("IntegerDelay", outofplace)
{
  prev_delays = new spip::ContainerRAM ();
  curr_delays = new spip::ContainerRAM ();
  delta_delays = new spip::ContainerRAM ();

  have_buffered_output = false;
}

spip::IntegerDelay::~IntegerDelay ()
{
  delete prev_delays;
  delete curr_delays;
  delete delta_delays;
  delete buffered;
}

void spip::IntegerDelay::prepare (unsigned nsignal)
{
  prev_delays->set_nchan (1);
  prev_delays->set_ndim (1);
  prev_delays->set_npol (1);
  prev_delays->set_nbit (sizeof(unsigned));
  prev_delays->set_ndat (1);
  prev_delays->set_nsignal (nsignal);
  prev_delays->zero ();

  curr_delays->set_nchan (1);
  curr_delays->set_ndim (1);
  curr_delays->set_npol (1);
  curr_delays->set_nbit (sizeof(unsigned));
  curr_delays->set_ndat (1);
  curr_delays->set_nsignal (nsignal);
  curr_delays->zero ();

  ndat = input->get_ndat ();
  nchan = input->get_nchan ();
  npol  = input->get_npol ();
  nbit  = input->get_nbit ();
  ndim  = input->get_ndim ();
  nsignal = input->get_nsignal ();

  buffered = new spip::ContainerRAM ();
  buffered->set_nchan (nchan);
  buffered->set_npol (npol);
  buffered->set_nbit (nbit);
  buffered->set_ndim (ndim);
  buffered->set_nsignal (nsignal);
  buffered->set_ndat (ndat);
  buffered->resize ();
}

void spip::IntegerDelay::set_delay (unsigned isig, unsigned delay)
{
  if (isig >= curr_delays->get_nsignal())
    throw invalid_argument ("IntegerDelay::set_delay isig > nsignal");

  unsigned * buffer = (unsigned *) curr_delays->get_buffer();
  buffer[isig] = delay;
}

void spip::IntegerDelay::compute_delta_delays ()
{
  unsigned * prev_buffer = (unsigned *) prev_delays->get_buffer();
  unsigned * curr_buffer = (unsigned *) curr_delays->get_buffer();
  int * delta = (int *) delta_delays->get_buffer();

  for (unsigned isig=0; isig<nsignal; isig++)
    delta[isig] = (int) curr_buffer[isig] - (int) prev_buffer[isig];
}

//! simply copy input buffer to output buffer
void spip::IntegerDelay::transformation ()
{
  compute_delta_delays();

  // the delays to apply to the data stream
  int * delta = (int *) delta_delays->get_buffer();

  unsigned nbit_per_dat = ndim * nbit;

  if (nbit_per_dat == 8)
    transform ((int8_t *) input->get_buffer(), 
               (int8_t *) buffered->get_buffer(),
               (int8_t *) output->get_buffer());
  else if (nbit_per_dat == 16)
    transform ((int16_t *) input->get_buffer(), 
               (int16_t *) buffered->get_buffer(), 
               (int16_t *) output->get_buffer());
  else if (nbit_per_dat == 32)
    transform ((int32_t *) input->get_buffer(), 
               (int32_t *) buffered->get_buffer(), 
               (int32_t *) output->get_buffer());
  else if (nbit_per_dat == 64)
    transform ((int64_t *) input->get_buffer(), 
               (int64_t *) buffered->get_buffer(), 
               (int64_t *) output->get_buffer());
  else
    throw runtime_error ("IntegerDelay::transformation unsupported bit-rate");

  // double buffer the delays
  spip::ContainerRAM * tmp = prev_delays;
  prev_delays = curr_delays;
  curr_delays = tmp;

  // double buffer the output
  if (have_buffered_output)
  {
    tmp = dynamic_cast<spip::ContainerRAM *>(output);
    output = buffered;
    buffered = tmp;
    output->set_ndat (input->get_ndat());
  }
  else
  {
    have_buffered_output = true;
    output->set_ndat (0);
  }
}

