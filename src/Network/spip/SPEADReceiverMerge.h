
#ifndef __SPEADReceiverMerge_h
#define __SPEADReceiverMerge_h


#include <boost/asio.hpp>

#include "spead2/recv_udp.h"
#include "spead2/recv_live_heap.h"
#include "spead2/recv_ring_stream.h"

#include "spip/SPEADBeamFormerConfig.h"

#include <iostream>
#include <cstdlib>
#include <pthread.h>

namespace spip {

  class SPEADReceiverMerge {

    public:

      SPEADReceiverMerge ();

      ~SPEADReceiverMerge ();

      int configure (const char * config);

      void prepare (std::string ip_address, int port1, int port2);

      int open();

      void start_recv_threads (int core1, int core2);

      void join_recv_threads ();

      static void * recv_thread1_wrapper (void * obj)
      {
        ((SPEADReceiverMerge*) obj )->receive_thread (1);
      }
      static void * recv_thread2_wrapper (void * obj)
      {
        ((SPEADReceiverMerge*) obj )->receive_thread (2);
      }

      bool receive_thread (int pol);

      void parse_metadata (const spead2::recv::item &item);

      void close();

    protected:

      unsigned nchan;

      unsigned ndim;

      unsigned nbit;

      unsigned npol;

      uint64_t resolution;

      float bw;

      float channel_bw;

      float tsamp;

      unsigned bits_per_second;

      unsigned bytes_per_second;

      unsigned heap_size;

      bool verbose;

    private:

      std::string spead_ip;

      int spead_port1;
      int spead_port2;

      int core1;
      int core2;

      pthread_t recv_thread1_id;
      pthread_t recv_thread2_id;

      int64_t start_adc_sample;

  };

}

#endif
