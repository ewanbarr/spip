/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/Container.h"

#ifndef __ContainerRAM_h
#define __ContainerRAM_h

namespace spip {

  class ContainerRAM : public Container
  {
    public:

      //! Null constructor
      ContainerRAM ();

      ~ContainerRAM();

      //! resize the buffer to match the input dimensions
      void resize ();

      //! zero the buffer 
      void zero ();

    protected:

    private:

  };
}

#endif
