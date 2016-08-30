/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#ifndef __Container_h
#define __Container_h

#include <inttypes.h>
#include <cstdlib>

namespace spip {

  //! All Data Containers have a sample ordering 
  typedef enum { TFSP, FSTP, Custom } Ordering;

  class Container
  {
    public:

      //! Null constructor
      Container ();

      ~Container();

      void set_nchan (unsigned n) { nchan = n; }
      unsigned get_nchan () { return nchan; }
      unsigned get_nchan () const { return nchan; }

      void set_nsignal (unsigned n) { nsignal = n; }
      unsigned get_nsignal() { return nsignal; }
      unsigned get_nsignal() const { return nsignal; }

      void set_ndim (unsigned n) { ndim = n; }
      unsigned get_ndim() { return ndim; }
      unsigned get_ndim() const { return ndim; }

      void set_npol (unsigned n) { npol = n; }
      unsigned get_npol() { return npol; }
      unsigned get_npol() const { return npol; }

      void set_nbit (unsigned n) { nbit = n; }
      unsigned get_nbit() { return nbit; }
      unsigned get_nbit() const { return nbit; }

      void set_ndat (uint64_t n) { ndat = n; }
      uint64_t get_ndat () { return ndat; }
      uint64_t get_ndat () const { return ndat; }

      size_t calculate_buffer_size ();

      //! resize the buffer to match the input dimensions
      virtual void resize () = 0;

      //! return a pointer to the data buffer
      virtual unsigned char * get_buffer() { return buffer; }
      virtual unsigned char * get_buffer() const { return buffer; }

    protected:

      //! The data buffer 
      unsigned char * buffer;

      //! Size of the data buffer (in bytes)
      uint64_t size;

    private:

      //! Ordering of data within the buffer
      Ordering order;

      //! Number of time samples
      uint64_t ndat;

      //! Number of frequnecy channels
      unsigned nchan;

      //! Number of indepdent signals (e.g. antenna, beams)
      unsigned nsignal;

      //! Number of polarisations
      unsigned npol;

      //! Number of dimensions to each datum
      unsigned ndim;

      //! Number of bits per value
      unsigned nbit;
  };
}

#endif
