
#ifndef __UDPReceiveDB_h
#define __UDPReceiveDB_h

#include "dada_def.h"

#include "spip/UDPSocketReceive.h"
#include "spip/UDPFormat.h"
#include "spip/UDPStats.h"
#include "spip/DataBlockWrite.h"

#include <iostream>
#include <cstdlib>
#include <pthread.h>

#define USING_VMA_EXTRA_API
#ifdef  USING_VMA_EXTRA_API
#include <mellanox/vma_extra.h>
#endif

namespace spip {

  enum ControlCmd   { None, Start, Stop, Quit };
  enum ControlState { Idle, Active, Stopping };

  class UDPReceiveDB {

    public:

      UDPReceiveDB (const char * key_string);

      ~UDPReceiveDB ();

      int configure (const char * config);

      void prepare (std::string ip_address, int port);

      void set_format (UDPFormat * fmt);

      void start_control_thread (int port);

      void stop_control_thread ();

      void open ();

      void open (const char * header);

      void close ();

      bool receive ();

      void start_capture () { control_cmd = Start; };

      void stop_capture () { control_cmd = Quit; };

      UDPStats * get_stats () { return stats; };

      uint64_t get_data_bufsz () { return db->get_data_bufsz(); };

    protected:

      static void * control_thread_wrapper (void *);

      void control_thread ();

      void update_stats();

      UDPSocketReceive * sock;

      UDPFormat * format;

      UDPStats * stats;

      DataBlockWrite * db;

      pthread_t control_thread_id;

      int control_port;

      ControlCmd control_cmd;

      ControlState control_state;

      char * header;

#ifdef USING_VMA_EXTRA_API
      struct vma_api_t *vma_api;

      struct vma_packets_t* pkts;
#else
      char vma_api;
#endif

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

      uint64_t b_recv_curr;
      uint64_t p_drop_curr;
      uint64_t s_curr;
      uint64_t b_recv_total;
      uint64_t p_drop_total;
      uint64_t s_total;

      double bytes_recv_ps;
      double packets_dropped_ps;
      double sleeps_ps;

      struct timeval curr;
      struct timeval prev;
  };

}

#endif
