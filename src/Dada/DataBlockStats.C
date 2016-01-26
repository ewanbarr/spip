/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/TCPSocketServer.h"
#include "spip/DataBlockStats.h"
#include "spip/BlockFormat.h"
#include "sys/time.h"

#include "ascii_header.h"

#include <cstring>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <new>
#include <vector>
#include <pthread.h>


#define DataBlockStats_CMD_NONE 0
#define DataBlockStats_CMD_START 1
#define DataBlockStats_CMD_STOP 2
#define DataBlockStats_CMD_QUIT 3

using namespace std;

spip::DataBlockStats::DataBlockStats(const char * key_string)
{
  db = new DataBlockView (key_string);

  db->connect();
  db->lock();

  bufsz = db->get_data_bufsz();
  buffer = (char *) malloc(bufsz);

  control_port = -1;
  header = (char *) malloc (DADA_DEFAULT_HEADER_SIZE);

  control_cmd = None;
  control_state = Idle;
  verbose = false;
  poll_time = 5;

}

spip::DataBlockStats::~DataBlockStats()
{
  db->unlock();
  db->disconnect();

  delete db;

  free (header);
  free (buffer);
}

int spip::DataBlockStats::configure (const char * config)
{
  if (ascii_header_get (config, "NCHAN", "%u", &nchan) != 1)
    throw invalid_argument ("NCHAN did not exist in header");

  if (ascii_header_get (config, "NBIT", "%u", &nbit) != 1)
    throw invalid_argument ("NBIT did not exist in header");

  if (ascii_header_get (config, "NPOL", "%u", &npol) != 1)
    throw invalid_argument ("NPOL did not exist in header");

  if (ascii_header_get (config, "NDIM", "%u", &ndim) != 1)
    throw invalid_argument ("NDIM did not exist in header");

  if (ascii_header_get (config, "TSAMP", "%f", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in header");

  if (ascii_header_get (config, "BW", "%f", &bw) != 1)
    throw invalid_argument ("BW did not exist in header");

  channel_bw = bw / nchan;

  bits_per_second  = (nchan * npol * ndim * nbit * 1000000) / tsamp;
  bytes_per_second = bits_per_second / 8;

  if (ascii_header_get (config, "START_CHANNEL", "%u", &start_chan) != 1)
    throw invalid_argument ("START_CHANNEL did not exist in header");
  if (ascii_header_get (config, "END_CHANNEL", "%u", &end_chan) != 1)
    throw invalid_argument ("END_CHANNEL did not exist in header");

  // save the header for use on the first open block
  strncpy (header, config, strlen(config)+1);
}

void spip::DataBlockStats::prepare ()
{
  // allocate a private buffer to be used for stats analysis
  if (verbose)
    cerr << "spip::DataBlockStats::prepare db->read_header()" << endl; 
  db->read_header();

  if (ascii_header_get (db->get_header(), "UTC_START", "%s", utc_start) != 1)
    throw runtime_error ("could not read UTC_START from header");

  uint64_t resolution;
  if (ascii_header_get(db->get_header(), "RESOLUTION", "%lu", &resolution) != 1)
    throw runtime_error ("could not read RESOLUTION from header");
  block_format->set_resolution (resolution);

  //if (verbose)
    cerr << "spip::DataBlockStats::prepare UTC_START=" << utc_start
         << " RESOLUTION=" << resolution << endl;
}

void spip::DataBlockStats::set_block_format (BlockFormat * fmt)
{
  block_format = fmt;
}


void spip::DataBlockStats::start_control_thread (int port)
{
  control_port = port;
  pthread_create (&control_thread_id, 0, control_thread_wrapper, this);
}

// wrapper method to start control thread
void * spip::DataBlockStats::control_thread_wrapper (void * ptr)
{
  reinterpret_cast<DataBlockStats*>( ptr )->control_thread ();
  return 0;
}

void spip::DataBlockStats::stop_control_thread ()
{
  control_cmd = Quit;
}

// start a control thread that will monitor commands from the TCS/LMC
void spip::DataBlockStats::control_thread()
{
#ifdef _DEBUG
  cerr << "spip::DataBlockStats::control_thread starting" << endl;
#endif

  if (control_port < 0)
  {
    cerr << "ERROR: no control port specified" << endl;
    return;
  }

  if (verbose)
    cerr << "spip::DataBlockStats::control_thread creating TCPSocketServer" << endl;
  spip::TCPSocketServer * control_sock = new spip::TCPSocketServer();

  // open a listen sock on all interfaces for the control port
  if (verbose)
    cerr << "spip::DataBlockStats::control_thread open socket on port=" 
         << control_port << endl;
  control_sock->open ("any", control_port, 1);

  int fd = -1;
  int verbose = 1;

  char * cmds = (char *) malloc (DADA_DEFAULT_HEADER_SIZE);
  char * cmd  = (char *) malloc (32);

  control_cmd = None;

  // wait for a connection
  while (control_cmd != Quit && fd < 0)
  {
    // accept with a 1 second timeout
#ifdef _DEBUG
    cerr << "control_thread : ctrl_sock->accept_client(1)" << endl;
#endif
    fd = control_sock->accept_client (1);
#ifdef _DEBUG
    cerr << "control_thread : fd=" << fd << endl;
#endif
    if (fd >= 0 )
    {
      if (verbose > 1)
        cerr << "control_thread : reading data from socket" << endl;
      ssize_t bytes_read = read (fd, cmds, DADA_DEFAULT_HEADER_SIZE);

      if (verbose)
        cerr << "control_thread: bytes_read=" << bytes_read << endl;

      control_sock->close_client();
      fd = -1;

      // now check command in list of header commands
      if (ascii_header_get (cmds, "COMMAND", "%s", cmd) != 1)
        throw invalid_argument ("COMMAND did not exist in header");
      if (verbose)
        cerr << "control_thread: cmd=" << cmd << endl;
      if (strcmp (cmd, "START") == 0)
      {
        strcat (header, cmds);
        if (ascii_header_del (header, "COMMAND") < 0)
          throw runtime_error ("Could not remove COMMAND from header");

        // write header
        if (verbose)
          cerr << "control_thread: control_cmd = Start" << endl;
        control_cmd = Start;
      }
      else if (strcmp (cmd, "STOP") == 0)
      {
        if (verbose)
          cerr << "control_thread: control_cmd = Stop" << endl;
        control_cmd = Stop;
      }
      else if (strcmp (cmd, "QUIT") == 0)
      {
        if (verbose)
          cerr << "control_thread: control_cmd = Quit" << endl;
        control_cmd = Quit;
      }
    }
  }

#ifdef _DEBUG
  cerr << "spip::DataBlockStats::control_thread exiting" << endl;
#endif
}

// monitor the most recently data to the data block to produce
// statistics on the data stream
bool spip::DataBlockStats::monitor (std::string stats_dir, unsigned stream_id)
{
  if (verbose)
    cerr << "spip::DataBlockStats::monitor ()" << endl;

  if (stats_dir.length() > 0)
    stats_dir += "/";

  keep_monitoring = true;

  unsigned nbin = 256;
  unsigned ntime = 512;
  unsigned nfreq = 512;

  block_format->prepare (nbin, ntime, nfreq);

  //if (verbose)
    cerr << "spip::DataBlockStats::monitor: db->read (" << buffer << ", " << bufsz << ")" << endl;

  int64_t bytes_read = db->read (buffer, bufsz);

  char local_time[32];
  char command[128];

  stringstream ss;
  ss << stats_dir << utc_start;
  sprintf (command, "mkdir -p %s", ss.str().c_str());
  if (verbose)
    cerr << "spip::DataBlockStats::monitor " << command << endl;
  int rval = system (command);
  if (rval != 0)
  {
    cerr << "spip::DataBlockStats::monitor could not create stats dir" << endl;
    keep_monitoring = false;
  }

  while (keep_monitoring)
  {
    // if EOD, then stop monitoring
    if (db->eod ())
    {
      if (verbose)
        cerr << "spip::DataBlockStats::monitor encountered EOD, monitoring stopped " << endl;
      keep_monitoring = false;
    }
    // seek to the end of the current data block
    else if (db->view_eod (db->get_data_bufsz()) < 0)
    {
      if (verbose)
        cerr << "spip::DataBlockStats::monitor db->view_eod failed, monitoring stopped" << endl;
      keep_monitoring = false;
    }
    else
    {
      if (verbose)
        cerr << "spip::DataBlockStats::monitor reading data block" << endl;

      block_format->reset();

      bytes_read = db->read (buffer, bufsz);

      block_format->unpack_hgft (buffer, bufsz);
      block_format->unpack_ms (buffer, bufsz);

      // write the data files to disk 
      time_t now = time(0);
      strftime (local_time, 32, DADA_TIMESTR, localtime(&now));

      ss.str("");

      ss << stats_dir << utc_start << "/" << local_time << "." << stream_id << ".hg.stats";
      if (verbose)
        cerr << "spip::DataBlockStats::monitor creating HG stats file " << ss.str() << endl;

      block_format->write_histograms (ss.str());
      
      ss.str("");
      ss << stats_dir << utc_start << "/" << local_time << "." 
         << stream_id << ".ft.stats";
      if (verbose)
        cerr << "spip::DataBlockStats::monitor creating FT stats file " << ss.str() << endl;
      block_format->write_freq_times (ss.str());

      ss.str("");
      ss << stats_dir << utc_start << "/" << local_time << "." 
         << stream_id << ".ms.stats";
      if (verbose)
        cerr << "spip::DataBlockStats::monitor creating MS stats file " << ss.str() << endl;

      block_format->write_mean_stddevs (ss.str());

      if (verbose)
        cerr << "spip::DataBlockStats::monitor sleep(" << poll_time << ")" << endl;
      int to_sleep = poll_time;
      while (control_cmd != Quit && to_sleep > 0)
      {
        sleep (1);
        to_sleep--;
      }
    }
    if (control_cmd == Quit)
      keep_monitoring = false;
  }

  return true;
}
