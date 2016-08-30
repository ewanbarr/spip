/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/DelayPipeline.h"

#include <signal.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <new>

//#define _DEBUG

using namespace std;

spip::DelayPipeline::DelayPipeline (const char * in_key_string, const char * out_key_string)
{
  in_db  = new DataBlockRead (in_key_string);
  out_db = new DataBlockWrite (out_key_string);

  // smallest number of taps
  ntap = 3;

  in_db->connect();
  in_db->lock();

  out_db->connect();
  out_db->lock();
}

spip::DelayPipeline::~DelayPipeline()
{
  in_db->unlock();
  in_db->disconnect();
  delete in_db;

  out_db->unlock();
  out_db->disconnect();
  delete out_db;
}

int spip::DelayPipeline::configure ()
{
  char * header_str = in_db->read_header();

  // save the header for use on the first open block
  header.load_from_str (header_str);

  if (header.get ("NANT", "%u", &nsignal) != 1)
    throw invalid_argument ("NSIGNAL did not exist in header");

  if (header.get ("NCHAN", "%u", &nchan) != 1)
    throw invalid_argument ("NCHAN did not exist in header");

  if (header.get ("NBIT", "%u", &nbit) != 1)
    throw invalid_argument ("NBIT did not exist in header");

  if (header.get ("NPOL", "%u", &npol) != 1)
    throw invalid_argument ("NPOL did not exist in header");

  if (header.get ("NDIM", "%u", &ndim) != 1)
    throw invalid_argument ("NDIM did not exist in header");

  if (header.get ("TSAMP", "%f", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in header");

  if (header.get ("BW", "%f", &bw) != 1)
    throw invalid_argument ("BW did not exist in header");

  // check if UTC_START has been set
  char * buffer = (char *) malloc (128);
  if (header.get ("UTC_START", "%s", buffer) == -1)
    throw invalid_argument ("failed to read UTC_START from header");

  // parse UTC_START into spip::Time
  utc_start = new spip::Time(buffer);

  uint64_t obs_offset;
  if (header.get("OBS_OFFSET", "%lu", &obs_offset) == -1)
  {
    obs_offset = 0;
    if (header.set ("OBS_OFFSET", "%lu", obs_offset) < 0)
      throw invalid_argument ("failed to write OBS_OFFSET=0 to header");
  }

  free (buffer);

  bits_per_second  = (nsignal * nchan * npol * ndim * nbit * 1000000) / tsamp;
  bytes_per_second = bits_per_second / 8;

  return 0;
}

void spip::DelayPipeline::prepare ()
{
  // input container
  uint64_t in_bufsz = in_db->get_data_bufsz();
  input = new spip::ContainerRing (in_bufsz);
  input->set_nchan (nchan);
  input->set_nsignal (nsignal);
  input->set_nbit (nbit);
  input->set_npol (npol);
  input->set_ndim (ndim);

  uint64_t ndat = in_bufsz / ((nsignal * nchan * npol * ndim * nbit) / 8);
  input->set_ndat (ndat);

  // configure the intermediary 
  buffered = new spip::ContainerRAM ();
  buffered->set_nchan (nchan);
  buffered->set_nsignal (nsignal);
  buffered->set_nbit (nbit);
  buffered->set_npol (npol);
  buffered->set_ndim (ndim);
  buffered->set_ndat (ndat);
  buffered->resize();
  buffered->zero();

  integer_delay = new spip::IntegerDelay ();
  integer_delay->set_input (input);
  integer_delay->set_output (buffered);
  integer_delay->prepare (nsignal);

  uint64_t out_bufsz = out_db->get_data_bufsz ();
  output = new spip::ContainerRing (out_db->get_data_bufsz());
  output->set_nchan (nchan);
  output->set_nsignal (nsignal);
  output->set_nbit (nbit);
  output->set_npol (npol);
  output->set_ndim (ndim);
  uint64_t out_ndat = out_bufsz / ((nsignal * nchan * npol * ndim * nbit) / 8);
  output->set_ndat (ndat);

  fractional_delay = new spip::FractionalDelay ();
  fractional_delay->set_input (buffered);
  fractional_delay->set_output (output);
  fractional_delay->prepare (ntap);
  
}

void spip::DelayPipeline::open ()
{
  open (header.raw());
}

// write the ascii header to the output datablock
void spip::DelayPipeline::open (const char * header_str)
{
  // open the data block for writing  
  out_db->open();

  // write the header
  out_db->write_header (header_str);
}

void spip::DelayPipeline::close ()
{
#ifdef _DEBUG
  cerr << "spip::DelayPipeline::close()" << endl;
#endif

  if (out_db->is_block_open())
  {
    cerr << "spip::DelayPipeline::close out_db->close_block(" << out_db->get_data_bufsz() << ")" << endl;
    out_db->close_block (out_db->get_data_bufsz());
  }

  if (in_db->is_block_open())
  {
    cerr << "spip::DelayPipeline::close in_db->close_block(" << in_db->get_data_bufsz() << ")" << endl;
    in_db->close_block (in_db->get_data_bufsz());
  }

  // close the data blocks, ending the observation
  in_db->close();
  out_db->close();
}

void spip::DelayPipeline::compute_delays ( spip::ContainerRAM * int_delays,
                                           spip::ContainerRAM * frac_delays,
                                           spip::ContainerRAM * phases )
{

}

// process blocks of input data until the end of the data stream
bool spip::DelayPipeline::process ()
{
  cerr << "spip::DelayPipeline::process ()" << endl;

  bool keep_processing = true;

  void * in_block = NULL;
  void * out_block = NULL;
  uint64_t block_size = in_db->get_data_bufsz();

  while (keep_processing)
  {
    // read a block of input data
    in_block = in_db->open_block (); 

    // set the input buffer to be pointing to in_block
    input->set_buffer ((unsigned char *) in_block);

    {
      // compute all delays
      compute_delays (integer_delay->get_delays(),
                      fractional_delay->get_delays(),
                      fractional_delay->get_phases());

      // perform DSP operation
      integer_delay->transformation ();

      if (integer_delay->have_output())
      {
        // update the FIR coefficients
        fractional_delay->compute_fir_coeffs ();

        // integer delay double buffers the output internally
        fractional_delay->set_input (integer_delay->get_output());

        out_block = out_db->open_block ();
        output->set_buffer ((unsigned char *) out_block);

        // perform fractional delay
        fractional_delay->transformation ();

        output->unset_buffer ();
        out_db->close_block (block_size);
        out_block = NULL;
      }
    }

    input->unset_buffer();
    in_db->close_block (block_size);
    in_block = NULL;
  }

  // close the data block
  close();

  return true;
}
