
#ifndef __SPEADReceiveDB_h
#define __SPEADReceiveDB_h

#include "dada_def.h"

#include "spip/DataBlockWrite.h"

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

      void stop_control_thread ();

      void open ();

      void open (const char * header);

      void close ();

      bool receive ();

      void start_capture () { control_cmd = Start; };

      void stop_capture () { control_cmd = Quit; };

      uint64_t get_data_bufsz () { return db->get_data_bufsz(); };

    protected:

      static void * control_thread_wrapper (void *);

      void control_thread ();

      DataBlockWrite * db;

      pthread_t control_thread_id;

      int control_port;

      ControlCmd control_cmd;

      ControlState control_state;

      char * header;

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

    private:

      boost::asio::ip::udp::endpoint enpoint;

      spead2::thread_pool worker;

      std::shared_ptr<spead2::memory_pool> pool;

      spead2::recv::ring_stream stream;
  };

}

#endif
