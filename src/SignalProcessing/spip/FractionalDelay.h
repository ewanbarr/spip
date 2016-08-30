//-*-C++-*-
/***************************************************************************
 *
 *   Copyright (C) 2016 by Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#ifndef __FractionalDelay_h
#define __FractionalDelay_h

#include "spip/ContainerRAM.h"
#include "spip/Transformation.h"
#include "math.h"

namespace spip {

  class FractionalDelay: public Transformation <Container, Container>
  {
    public:
     
      FractionalDelay ();

      void prepare (unsigned _ntap);

      void reserve ();

      void set_delay (unsigned isig, float delay);

      void set_phase (unsigned isig, unsigned ichan, float phase);

      //! Get the container of delays
      ContainerRAM * get_delays () { return delays; }

      //! Get the container of phases
      ContainerRAM * get_phases () { return phases; }

      void compute_fir_coeffs ();

      //! Perform the integer delay transformation from input to output
      void transformation ();

    protected:

      template <typename T>
      void unpack (T * in, float * un)
      {
        uint64_t nval = nchan * npol * nsignal * ndat * ndim;
        for (uint64_t ival=0; ival<nval; ival++)
          un[ival] = float (in[ival]);
      }
#if 0
      {
        for (unsigned ichan=0; ichan<nchan; ichan++)
        {
          for (unsigned ipol=0; ipol<npol; ipol++)
          {
            for (unsigned isig=0; isig<nsignal; isig++)
            {
              for (uint64_t ival=0; ival<ndat*ndim; ival++)
              {
                un[ival] = float (in[ival]);
              }
              in += ndat * ndim;
              unpacked += ndat * ndim;
            }
          }
        }
      }
#endif

      template <typename T>
      void repack (float * in, T * re)
      {
        uint64_t nval = nchan * npol * nsignal * ndat * ndim;
        for (uint64_t ival=0; ival<nval; ival++)
          re[ival] = (T) rintf (in[ival]);
      }
#if 0
      {
        for (unsigned ichan=0; ichan<nchan; ichan++)
        {
          for (unsigned ipol=0; ipol<npol; ipol++)
          {
            for (unsigned isig=0; isig<nsignal; isig++)
            {
              for (uint64_t ival=0; ival<ndat*ndim; ival++)
              {
                // downstream processing will assume a digitized mean of 0
                re[ival] = (T) rintf (in[ival]);
              }
              in += ndat * ndim;
              unpacked += ndat * ndim;
            }
          }
        }
      }
#endif


    private:

      void unpack_input ();

      void transform ();

      void repack_output ();

      //! fractional delays in samples
      ContainerRAM * delays;

      //! phase offsets in radians
      ContainerRAM * phases;

      //! phase offsets in radians
      ContainerRAM * firs;

      //! unpacked input 
      ContainerRAM * unpacked;

      unsigned nchan;

      unsigned npol;

      unsigned ndim;

      unsigned nbit;

      unsigned nsignal;

      uint64_t ndat;

      unsigned ntap;

      unsigned half_ntap;

  };

}

#endif
