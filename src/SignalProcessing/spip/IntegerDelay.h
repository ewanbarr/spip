//-*-C++-*-
/***************************************************************************
 *
 *   Copyright (C) 2016 by Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#ifndef __IntegerDelay_h
#define __IntegerDelay_h

#include "spip/ContainerRAM.h"
#include "spip/Transformation.h"

namespace spip {

  class IntegerDelay: public Transformation <Container, Container>
  {
    public:
     
      IntegerDelay ();

      ~IntegerDelay ();

      void prepare (unsigned _nsignal);

      void reserve ();

      //! Set the integer delay for a specific channel
      void set_delay (unsigned isig, unsigned delay);

      //! Get the container of delays
      ContainerRAM * get_delays () { return curr_delays; }

      void compute_delta_delays ();

      template <typename T>
      void transform (T * in, T* buf, T * out)
      {
        int * delta = (int *) delta_delays->get_buffer();
        uint64_t idat, odat;

        for (unsigned ichan=0; ichan<nchan; ichan++)
        {
          for (unsigned ipol=0; ipol<npol; ipol++)
          {
            // TODO fix the integer transition

            for (unsigned isig=0; isig<nsignal; isig++)
            {
              int delay = delta[isig];
              // copy buffered data to the output
              if (have_buffered_output)
              {
                for (odat=ndat-delay,idat=0; idat<ndat; idat++,odat++)
                  out[odat] = buf[idat];
              }

              // copy input to the buffer
              for (odat=0,idat=delay; idat<ndat; idat++,odat++)
                buf[odat] = in[idat];

              in += ndat;
              out += ndat;

            }
          }
        }
      }

      //! Perform the integer delay transformation from input to output
      void transformation ();

      bool have_output () { return have_buffered_output; }

    protected:

    private:

      //! integer delays applied to previous block
      ContainerRAM * prev_delays;

      //! integer delays to be applied to current block
      ContainerRAM * curr_delays;

      //! difference between prev and curr delays
      ContainerRAM * delta_delays;

      //! buffered output
      ContainerRAM * buffered;

      bool have_buffered_output;

      unsigned nchan;

      unsigned npol;

      unsigned ndim;

      unsigned nbit;

      unsigned nsignal;

      uint64_t ndat;

  };

}

#endif
