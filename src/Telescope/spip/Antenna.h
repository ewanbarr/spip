/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#ifndef __Antenna_h
#define __Antenna_h

#include <string>
#include <cmath>

namespace spip {

  class Antenna
  {
    public:

      //! Null constructor
      Antenna ();

      Antenna (std::string _name, double _x, double _y, double _delay, double _phase, double _scale);

      ~Antenna();

      //! Return distance from phase centre
      double get_distance ();

      double get_instrumental_delay () { return instrumental_delay; }

      double get_phase_offset () { return phase_offset; }

    protected:

      //! name of the antenna
      std::string name;

      //! x coordinate relative to phase centre in metres
      double x;

      //! y coordinate relative to phase centre in metres
      double y;

      //! delay in seconds
      double instrumental_delay;

      //! phase offset in radians
      float phase_offset;

      //! weighting of antenna, normalised
      float scale;

    private:


  };
}

#endif
