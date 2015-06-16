
#ifndef __UDPGenerator_h
#define __UDPGenerator_h

#include "spip/UDPSocketSend.h"

#include <cstdlib>

namespace spip {

  class UDPGenerator {

    public:

      UDPGenerator ();

      ~UDPGenerator ();

      int configure (const char * header);

      void allocate_signal ();

      // derived classes must implement this
      virtual void generate_signal () = 0;

      virtual void encode_header (void * buf, size_t bufsz, uint64_t packet_number) = 0;

      virtual void decode_header (void * buf, size_t bufsz, uint64_t * packet_number) = 0;

      void prepare (std::string ip_address, int port);

      // transmission thread
      void transmit (unsigned tobs, float data_rate);

    protected:

      UDPSocketSend * sock;

      size_t packet_header_size;

      size_t packet_data_size;

      unsigned nchan;

      unsigned ndim;

      unsigned nbit;

      unsigned npol;

      float bw;

      float channel_bw;

      float tsamp;

      void * signal_buffer;

      size_t signal_buffer_size;

      unsigned bits_per_second;

      unsigned bytes_per_second;
  };

}

#endif
