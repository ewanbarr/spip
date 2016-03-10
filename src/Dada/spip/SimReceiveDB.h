
#ifndef __SimReceiveDB_h
#define __SimReceiveDB_h

#include "config.h"

#include "spip/AsciiHeader.h"
#include "spip/UDPFormat.h"
#include "spip/UDPStats.h"
#include "spip/DataBlockWrite.h"

#include <iostream>
#include <cstdlib>
#include <pthread.h>

namespace spip {

  enum ControlCmd   { None, Start, Stop, Quit };
  enum ControlState { Idle, Active, Stopping };

  class SimReceiveDB {

    public:

      SimReceiveDB (const char * key_string);

      ~SimReceiveDB ();

      int configure (const char * config);

      void prepare ();

      void set_format (UDPFormat * fmt);

      void start_control_thread (int port);

      void stop_control_thread ();

      void open ();

      void open (const char * header);

      void close ();

      bool generate(int tobs);

      void start_transmit() { control_cmd = Start; };

      void stop_transmit() { control_cmd = Quit; };

      static void * stats_thread_wrapper (void * obj)
      {
        ((SimReceiveDB*) obj )->stats_thread ();
      }

      void start_stats_thread ();

      void stop_stats_thread ();

      void stats_thread ();

      UDPStats * get_stats () { return stats; };

      uint64_t get_data_bufsz () { return db->get_data_bufsz(); };

      void set_verbosity (unsigned v) { verbose = v; };

    protected:

      static void * control_thread_wrapper (void *);

      void control_thread ();

      void update_stats();

      UDPFormat * format;

      UDPStats * stats;

      pthread_t stats_thread_id;

      DataBlockWrite * db;

      pthread_t control_thread_id;

      int control_port;

      ControlCmd control_cmd;

      ControlState control_state;

      AsciiHeader header;

      bool keep_generating;

      unsigned nchan;

      unsigned ndim;

      unsigned nbit;

      unsigned npol;

      float bw;

      float channel_bw;

      float tsamp;

      double bits_per_second;

      double bytes_per_second;

      uint64_t b_recv_curr;
      uint64_t b_drop_curr;
      uint64_t s_curr;
      uint64_t b_recv_total;
      uint64_t b_drop_total;
      uint64_t s_total;

      double bytes_recv_ps;
      double bytes_drop_ps;
      double sleeps_ps;

      struct timeval curr;
      struct timeval prev;

      char verbose;
  };

}

#endif
