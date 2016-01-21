
#ifndef __SPEADReceiver_h
#define __SPEADReceiver_h


#include <boost/asio.hpp>

#include "spead2/recv_udp.h"
#include "spead2/recv_live_heap.h"
#include "spead2/recv_ring_stream.h"

#include "spip/SPEADBeamFormerConfig.h"

#include <iostream>
#include <cstdlib>
#include <pthread.h>

namespace spip {

  class SPEADReceiver {

    public:

      SPEADReceiver ();

      ~SPEADReceiver ();

      int configure (const char * config);

      void prepare (std::string ip_address, int port);

      int open();

      bool receive ();

      void parse_metadata (const spead2::recv::item &item);

      uint64_t item_ptr_48u (const unsigned char * ptr);
      double item_ptr_64f (const unsigned char * ptr);

      void show_heap(const spead2::recv::heap &fheap);

      void close();

    protected:

      bool keep_receiving;

      unsigned nchan;

      unsigned ndim;

      unsigned nbit;

      unsigned npol;

      float bw;

      float channel_bw;

      float tsamp;

      unsigned bits_per_second;

      unsigned bytes_per_second;

      unsigned heap_size;

      bool verbose;

    private:

      std::string spead_ip;

      int spead_port;

      //boost::asio::ip::udp::endpoint endpoint;

      spead2::thread_pool worker;

      std::shared_ptr<spead2::memory_pool> pool;

      SPEADBeamFormerConfig bf_config;

      int64_t start_adc_sample;

  };

}

#endif
