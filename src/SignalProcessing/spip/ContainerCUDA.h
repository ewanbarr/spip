/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/Container.h"

namespace spip {

  class ContainerCUDADevice : public Container
  {
    public:

      //! Null constructor
      ContainerCUDADevice();

      ~ContainerCUDADevice();

      //! resize the buffer to match the input dimensions
      void resize ();

    protected:

    private:

      //! device upon which memory is to be allocated
      int device;

      //! stream upon which memory access should be used 
      cudaStream_t stream;
  };

  class ContainerCUDAPinned : public Container
  {
    public:

      //! Null constructor
      ContainerCUDAPinned();

      ~ContainerCUDAPinned();

      //! resize the buffer to match the input dimensions
      void resize ();

    protected:

    private:
  };
}
