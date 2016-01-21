
#ifndef __SPEADReceiveDB_h
#define __SPEADReceiveDB_h

#include "dada_def.h"

#include <boost/asio.hpp>

#include "spead2/recv_udp.h"
#include "spead2/recv_live_heap.h"
#include "spead2/recv_ring_stream.h"

#include "spip/DataBlockWrite.h"
#include "spip/SPEADBeamFormerConfig.h"

#include <iostream>
#include <cstdlib>
#include <pthread.h>

namespace spip {

  enum ControlCmd   { None, Start, Stop, Quit };
  enum ControlState { Idle, Active, Stopping };

  class SPEADReceiveDB {

    public:

      SPEADReceiveDB (const char * key_string);

      ~SPEADReceiveDB ();

      int configure (const char * config);

      void prepare (std::string ip_address, int port);

      void start_control_thread (int port);

      static void * control_thread_wrapper (void * obj)
      {
        ((SPEADReceiveDB*) obj )->control_thread ();
      }

      void stop_control_thread ();

      void open ();

      void open (const char * header);

      void close ();

      bool receive ();

      void start_capture () { control_cmd = Start; };

      void stop_capture () { control_cmd = Quit; };

      uint64_t get_data_bufsz () { return db->get_data_bufsz(); };

    protected:

      void control_thread ();

      DataBlockWrite * db;

      pthread_t control_thread_id;

      int control_port;

      ControlCmd control_cmd;

      ControlState control_state;

      char * header;

      unsigned nchan;

      unsigned ndim;

      unsigned nbit;

      unsigned npol;

      float bw;

      float channel_bw;

      float tsamp;

      uint64_t resolution;

      unsigned bits_per_second;

      unsigned bytes_per_second;

      int64_t start_adc_sample;

      char verbose;

    private:

      bool have_metadata;

      bool keep_receiving;

      //boost::asio::ip::udp::endpoint * endpoint;

      //std::shared_ptr<spead2::memory_pool> pool;
      //
      std::string spead_ip;

      int spead_port;

      unsigned heap_size;

      SPEADBeamFormerConfig bf_config;

      uint64_t timestamp;
  };

}

#endif
