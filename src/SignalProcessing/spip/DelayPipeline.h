/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#ifndef __DelayPipeline_h
#define __DelayPipeline_h

#include "spip/AsciiHeader.h"
#include "spip/Time.h"
#include "spip/DataBlockRead.h"
#include "spip/DataBlockWrite.h"
#include "spip/IntegerDelay.h"
#include "spip/FractionalDelay.h"
#include "spip/ContainerRing.h"
#include "spip/ContainerRAM.h"

#include <vector>

namespace spip {

  class DelayPipeline {

    public:

      DelayPipeline (const char * in_key_string, const char * out_key_string);

      ~DelayPipeline ();

      int configure ();

      void prepare ();

      void open ();

      void open (const char * header_str);

      void close ();

      void compute_delays (ContainerRAM * int_delays,
                           ContainerRAM * frac_delays,
                           ContainerRAM * phases);

      bool process ();

    protected:

      AsciiHeader header;

      DataBlockRead * in_db;

      DataBlockWrite * out_db;

      Time * utc_start;

      IntegerDelay * integer_delay;

      FractionalDelay * fractional_delay;

      ContainerRing * input;

      ContainerRAM * buffered;

      ContainerRing * output;

      unsigned ntap;

      unsigned nsignal;

      unsigned nchan;

      unsigned npol;

      unsigned ndim;

      unsigned nbit;

      float tsamp;

      float bw;

      float channel_bw;

      std::vector<float> channel_freqs;

      unsigned bits_per_second;

      unsigned bytes_per_second;

  };

}

#endif
