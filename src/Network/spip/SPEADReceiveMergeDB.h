
#ifndef __SPEADReceiveMergeDB_h
#define __SPEADReceiveMergeDB_h

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

  class SPEADReceiveMergeDB {

    public:

      SPEADReceiveMergeDB (const char * key_string);

      ~SPEADReceiveMergeDB ();

      int configure (const char * config);

      void prepare (std::string ip_address1, int port1, std::string ip_address2, int port2);

      void start_control_thread (int port);

      static void * control_thread_wrapper (void * obj)
      {
        ((SPEADReceiveMergeDB*) obj )->control_thread ();
      }

      void stop_control_thread ();

      void start_threads (int core1, int core2);

      void join_threads ();

      static void * datablock_thread_wrapper (void * obj)
      {
        ((SPEADReceiveMergeDB*) obj )->datablock_thread ();
      }

      bool datablock_thread ();

      static void * recv_thread1_wrapper (void * obj)
      {
        ((SPEADReceiveMergeDB*) obj )->receive_thread (0);
      }

      static void * recv_thread2_wrapper (void * obj)
      {
        ((SPEADReceiveMergeDB*) obj )->receive_thread (1);
      }

      bool receive_thread (int pol);

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
  
      char * block;
          
      pthread_t control_thread_id;

      pthread_t datablock_thread_id;

      pthread_t recv_thread1_id;

      pthread_t recv_thread2_id;

      pthread_cond_t cond;

      pthread_mutex_t mutex;

      uint64_t curr_heap;

      uint64_t next_heap;

      std::string spead_ips[2];

      int spead_ports[2];

      int cores[2];

      bool full[2];

      unsigned heap_size;

      SPEADBeamFormerConfig bf_config;

      uint64_t timestamp;
  };

}

#endif
