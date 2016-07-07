/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#ifndef __Channel_h
#define __Channel_h

#include <string>
#include <cmath>

namespace spip {

  class Channel
  {
    public:

      //! Default constructor
      Channel (unsigned _number, double _cfreq, double _bw);

      //! Return distance from phase centre
      double get_cfreq_hz () { return cfreq * 1e6; }

      double get_cfreq () { return cfreq; }

      double get_bw () { return bw; }

    protected:

    private:

      //! centre frequnecy of the channel in MHz
      double cfreq;

      //! bandwidth of the channel in MHz
      double bw;

      //! PFB channel index
      double number;


  };
}

#endif
