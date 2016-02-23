
#ifndef __DataBlockStats_h
#define __DataBlockStats_h

#include "dada_def.h"

#include "spip/BlockFormat.h"
#include "spip/DataBlockView.h"

#include <iostream>
#include <cstdlib>
#include <pthread.h>


namespace spip {

  enum ControlCmd   { None, Start, Stop, Quit };
  enum ControlState { Idle, Active, Stopping };

  class DataBlockStats {

    public:

      DataBlockStats (const char * key_string);

      ~DataBlockStats ();

      int configure (const char * config);

      void prepare ();

      void set_block_format (BlockFormat * fmt);

      void start_control_thread (int port);

      void stop_control_thread ();

      void stop_monitoring () { control_cmd = Quit; };

      void open ();

      void open (const char * header);

      void close ();

      bool monitor ( std::string stats_dir, unsigned stream_id);

      uint64_t get_data_bufsz () { return db->get_data_bufsz(); };

    protected:

      static void * control_thread_wrapper (void *);

      void control_thread ();

      void update_stats();

      BlockFormat * block_format;

      DataBlockView * db;

      char * buffer;

      pthread_t control_thread_id;

      int control_port;

      ControlCmd control_cmd;

      ControlState control_state;

      char * header;

      bool keep_monitoring;

      uint64_t bufsz;

      unsigned nchan;

      unsigned ndim;

      unsigned nbit;

      unsigned npol;

      unsigned start_chan;

      unsigned end_chan;

      float bw;

      float channel_bw;

      float tsamp;

      unsigned bits_per_second;

      unsigned bytes_per_second;

      struct timeval curr;

      struct timeval prev;

      bool verbose;

      unsigned poll_time;

      char utc_start[20];
      
  };

}

#endif
