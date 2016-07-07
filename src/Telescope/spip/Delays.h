/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#ifndef __Delays_h
#define __Delays_h

#include "spip/Antenna.h"
#include "spip/Channel.h"
#include "spip/ContainerRAM.h"

#include <inttypes.h>
#include <cstdlib>
#include <vector>

namespace spip {

  class Delays
  {
    public:

      //! Null constructor
      Delays ();

      ~Delays();

      void set_nchan (unsigned n) { nchan = n; }
      unsigned get_nchan () { return nchan; }
      unsigned get_nchan () const { return nchan; }

      void set_nant (unsigned n) { nant = n; }
      unsigned get_nant() { return nant; }
      unsigned get_nant() const { return nant; }

      void compute (double projected_delay,
                    ContainerRAM * int_delays,
                    ContainerRAM * frac_delays,
                    ContainerRAM * phases);


    protected:

    private:

      unsigned nant;

      unsigned nchan;

      double C;
      double twopi;

      double fixed_delay;
      double sampling_period;

      std::vector<Antenna> antennae;

      std::vector<Channel> channels;

      std::vector<double> instrumental_delays;

      std::vector<double> geometric_delays;

  };
}

#endif
