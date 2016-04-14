
#ifndef __UDPReceiveMergeDB_h
#define __UDPReceiveMergeDB_h

#include "dada_def.h"

#include "spip/AsciiHeader.h"
#include "spip/UDPSocketReceive.h"
#include "spip/UDPFormat.h"
#include "spip/UDPStats.h"
#include "spip/DataBlockWrite.h"

#include <iostream>
#include <cstdlib>
#include <pthread.h>

#ifdef  HAVE_VMA
#include <mellanox/vma_extra.h>
#endif


namespace spip {

  enum ControlCmd   { None, Start, Stop, Quit };
  enum ControlState { Idle, Active, Stopping };

  class UDPReceiveMergeDB {

    public:

      UDPReceiveMergeDB (const char * key_string);

      ~UDPReceiveMergeDB ();

      int configure (const char * config);

      void prepare ();

      void set_formats (UDPFormat * fmt1, UDPFormat * fmt2);

      void set_control_cmd (ControlCmd cmd);

      void start_control_thread (int port);

      static void * control_thread_wrapper (void * obj)
      {
        ((UDPReceiveMergeDB*) obj )->control_thread ();
      }

      void stop_control_thread ();

      void start_threads (int core1, int core2);

      void join_threads ();

      static void * datablock_thread_wrapper (void * obj)
      {
        ((UDPReceiveMergeDB*) obj )->datablock_thread ();
      }

      bool datablock_thread ();

      static void * recv_thread1_wrapper (void * obj)
      {
        ((UDPReceiveMergeDB*) obj )->receive_thread (0);
      }

      static void * recv_thread2_wrapper (void * obj)
      {
        ((UDPReceiveMergeDB*) obj )->receive_thread (1);
      }

      bool receive_thread (int pol);

      void open ();

      void open (const char * header);

      void close ();

      bool receive ();

      uint64_t get_data_bufsz () { return db->get_data_bufsz(); };

    protected:

      void control_thread ();

      DataBlockWrite * db;

      int control_port;

      ControlCmd control_cmd;

      ControlState control_state;

      ControlState control_states[2];

      std::string data_hosts[2];

      int data_ports[2];

      std::string data_mcasts[2];

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

      UDPSocketReceive * socks[2];

      UDPFormat * formats[2];

      UDPStats * stats[2];

      int cores[2];

      bool full[2];

      unsigned heap_size;

      uint64_t timestamp;

      AsciiHeader header;


#ifdef HAVE_VMA
      struct vma_api_t *vma_apis[2];

      struct vma_packets_t* pkts[2];
#else
      char vma_apis[2];
#endif

  };

}

#endif
