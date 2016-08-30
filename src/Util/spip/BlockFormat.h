
#ifndef __BlockFormat_h
#define __BlockFormat_h

#include <cstdlib>
#include <inttypes.h>

#include <vector>
#include <string>

namespace spip {

  class BlockFormat {

    public:

      BlockFormat();

      ~BlockFormat();

      void prepare (unsigned _nbin, unsigned _ntime, unsigned _nfreq);

      void reset();

      void write_histograms(std::string hg_filename);

      void write_freq_times(std::string ft_filename);

      void write_mean_stddevs(std::string ms_filename);

      virtual void unpack_hgft (char * buffer, uint64_t nbytes) = 0;

      virtual void unpack_ms (char * buffer, uint64_t nbytes) = 0;

      void set_resolution (uint64_t _resolution) { resolution = _resolution; };

    protected:

      unsigned ndim;

      unsigned npol;

      unsigned nchan;

      unsigned nbit;

      unsigned nbin;

      unsigned ntime;

      unsigned nfreq;

      unsigned bits_per_sample;

      unsigned bytes_per_sample;

      uint64_t resolution;

      std::vector <float>sums;

      std::vector <float>means;

      std::vector <float>variances;

      std::vector <float> stddevs;

      std::vector <std::vector <std::vector <std::vector <unsigned> > > > hist;

      std::vector <std::vector <std::vector <float> > > freq_time;

    private:

  };

}

#endif
