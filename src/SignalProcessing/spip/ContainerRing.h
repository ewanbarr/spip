/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/Container.h"

#ifndef __ContainerRing_h
#define __ContainerRing_h

namespace spip {

  class ContainerRing : public Container
  {
    public:

      //! Null constructor
      ContainerRing (uint64_t _size);

      ~ContainerRing();

      //! resize the buffer to match the input dimensions
      void resize ();

      //! zero the buffer 
      void zero ();

      //! set change the buffer pointer
      void set_buffer (unsigned char * buf);

      //! unset the buffer pointer
      void unset_buffer ();

    protected:

    private:

      bool buffer_valid;

  };
}

#endif
