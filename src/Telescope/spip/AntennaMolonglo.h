/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#ifndef __AntennaMolonglo_h
#define __AntennaMolonglo_h

#include "spip/Antenna.h"

namespace spip {

  class AntennaMolonglo : public Antenna
  {
    public:

      //! Null constructor
      AntennaMolonglo ();

      AntennaMolonglo (const char * config_line);

      AntennaMolonglo (std::string _name, double _dist, double _delay, double _phase, double _scale);

      ~AntennaMolonglo();

      void set_bay (std::string _bay);

      double get_dist ();

    protected:

    private:

      double dist;

      std::string bay;

      unsigned bay_idx;

  };
}

#endif
