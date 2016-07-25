/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/TCPSocketServer.h"
#include "spip/AsciiHeader.h"
#include "spip/SimReceiveDB.h"
#include "sys/time.h"

#include <unistd.h>
#include <cstring>
#include <iostream>
#include <iomanip>      // std::setprecision
#include <stdexcept>
#include <new>
#include <pthread.h>

#define DELTA_START 5

using namespace std;

spip::SimReceiveDB::SimReceiveDB(const char * key_string)
{
  db = new DataBlockWrite (key_string);

  db->connect();
  db->lock();

  verbose = 0;
  format = 0;
  control_port = -1;

  control_cmd = None;
  control_state = Idle;
}

spip::SimReceiveDB::~SimReceiveDB()
{
  db->unlock();
  db->disconnect();

  delete db;
}

int spip::SimReceiveDB::configure (const char * config)
{
  if (spip::AsciiHeader::header_get (config, "NCHAN", "%u", &nchan) != 1)
    throw invalid_argument ("NCHAN did not exist in header");

  if (spip::AsciiHeader::header_get (config, "NBIT", "%u", &nbit) != 1)
    throw invalid_argument ("NBIT did not exist in header");

  if (spip::AsciiHeader::header_get (config, "NPOL", "%u", &npol) != 1)
    throw invalid_argument ("NPOL did not exist in header");

  if (spip::AsciiHeader::header_get (config, "NDIM", "%u", &ndim) != 1)
    throw invalid_argument ("NDIM did not exist in header");

  if (spip::AsciiHeader::header_get (config, "TSAMP", "%f", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in header");

  if (spip::AsciiHeader::header_get (config, "BW", "%f", &bw) != 1)
    throw invalid_argument ("BW did not exist in header");

  channel_bw = bw / nchan;

  bits_per_second = (double) (nchan * npol * ndim * nbit) * (1000000.0f / tsamp);
  bytes_per_second = bits_per_second / 8.0;

  // save the header for use on the first open block
  header.load_from_str (config);

  if (!format)
    throw runtime_error ("unable for prepare format");
  format->configure (header, "");

  // now write new params to header
  uint64_t resolution = format->get_resolution();
  if (header.set("RESOLUTION", "%lu", resolution) < 0)
    throw invalid_argument ("failed to write RESOLUTION to header");

}

void spip::SimReceiveDB::prepare ()
{
  stats = new UDPStats (format->get_header_size(), format->get_data_size());
}

void spip::SimReceiveDB::set_format (spip::UDPFormat * fmt)
{
  if (format)
    delete format;
  format = fmt;
}

void spip::SimReceiveDB::start_control_thread (int port)
{
  control_port = port;
  pthread_create (&control_thread_id, 0, control_thread_wrapper, this);
}

// wrapper method to start control thread
void * spip::SimReceiveDB::control_thread_wrapper (void * ptr)
{
  reinterpret_cast<SimReceiveDB*>( ptr )->control_thread ();
  return 0;
}

void spip::SimReceiveDB::stop_control_thread ()
{
  control_cmd = Quit;
}

// start a control thread that will receive commands from the TCS/LMC
void spip::SimReceiveDB::control_thread()
{
#ifdef _DEBUG
  cerr << "spip::SimReceiveDB::control_thread starting" << endl;
#endif

  if (control_port < 0)
  {
    cerr << "WARNING: no control port, using 32132" << endl;
    control_port = 32132;
  }

  cerr << "spip::SimReceiveDB::control_thread creating TCPSocketServer" << endl;
  spip::TCPSocketServer * control_sock = new spip::TCPSocketServer();

  // open a listen sock on all interfaces for the control port
  cerr << "spip::SimReceiveDB::control_thread open socket on port=" 
       << control_port << endl;
  control_sock->open ("any", control_port, 1);

  int fd = -1;
  int verbose = 1;

  char * cmds = (char *) malloc (DEFAULT_HEADER_SIZE);
  char * cmd  = (char *) malloc (32);

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
      ssize_t bytes_read = read (fd, cmds, DEFAULT_HEADER_SIZE);

      if (verbose)
        cerr << "control_thread: bytes_read=" << bytes_read << endl;

      control_sock->close_client();
      fd = -1;

      // now check command in list of header commands
      if (spip::AsciiHeader::header_get (cmds, "COMMAND", "%s", cmd) != 1)
        throw invalid_argument ("COMMAND did not exist in header");
      //if (verbose)
        cerr << "control_thread: cmd=" << cmd << endl;
      if (strcmp (cmd, "START") == 0)
      {
        // append cmds to header
        header.append_from_str (cmds);
        if (header.del ("COMMAND") < 0)
          throw runtime_error ("Could not remove COMMAND from header");

        if (verbose)
          cerr << "control_thread: open()" << endl;
        open ();

        // write header
        //if (verbose)
          cerr << "control_thread: control_cmd = Start" << endl;
        control_cmd = Start;
      }
      else if (strcmp (cmd, "STOP") == 0)
      {
        //if (verbose)
          cerr << "control_thread: control_cmd = Stop" << endl;
        control_cmd = Stop;
      }
      else if (strcmp (cmd, "QUIT") == 0)
      {
        //if (verbose)
          cerr << "control_thread: control_cmd = Quit" << endl;
        control_cmd = Quit;
      }
    }

    // update the stats
    update_stats ();
  }
#ifdef _DEBUG
  cerr << "spip::SimReceiveDB::control_thread exiting" << endl;
#endif
}


void spip::SimReceiveDB::start_stats_thread ()
{
  pthread_create (&stats_thread_id, NULL, stats_thread_wrapper, this);
}

void spip::SimReceiveDB::stop_stats_thread ()
{
  control_cmd = Stop;
  void * result;
  pthread_join (stats_thread_id, &result);
}

/* 
 *  Thread to print simple capture statistics
 */
void spip::SimReceiveDB::stats_thread()
{
  uint64_t b_recv_total = 0;
  uint64_t b_recv_curr = 0;
  uint64_t b_recv_1sec;

  uint64_t s_curr = 0;
  uint64_t s_total = 0;
  uint64_t s_1sec;

  uint64_t b_drop_curr = 0;

  float gb_recv_ps = 0;
  float mb_recv_ps = 0;

  if (verbose)
  {
    cerr << "spip::SimReceiveDB::stats_thread starting polling" << endl;
    while (control_cmd != Stop)
    {
      while (control_state == Active)
      {
        // get a snapshot of the data as quickly as possible
        b_recv_curr = stats->get_data_transmitted();
        b_drop_curr = stats->get_data_dropped();
        s_curr = stats->get_nsleeps();

        // calc the values for the last second
        b_recv_1sec = b_recv_curr - b_recv_total;
        s_1sec = s_curr - s_total;

        // update the totals
        b_recv_total = b_recv_curr;
        s_total = s_curr;

        mb_recv_ps = (double) b_recv_1sec / 1000000;
        gb_recv_ps = (mb_recv_ps * 8)/1000;

        // determine how much memory is free in the receivers
        cerr << "Recv " << std::setprecision(3) << gb_recv_ps << " [Gb/s] "
             << "Sleeps " << s_1sec << " Dropped " << b_drop_curr << endl;
        sleep (1);
      }
    }
    sleep(1);
  }
}


// compute the data statisics since the last update
void spip::SimReceiveDB::update_stats()
{
  b_recv_curr = get_stats()->get_data_transmitted();
  b_drop_curr = get_stats()->get_data_dropped();
  s_curr      = get_stats()->get_nsleeps();

  struct timeval curr;
  gettimeofday (&curr, 0);

  if (prev.tv_sec > 0)
  {
    double seconds_elapsed = (curr.tv_sec - prev.tv_sec);
    seconds_elapsed += (curr.tv_usec - prev.tv_usec) / 1000000;

    // calc the values since prev update
    bytes_recv_ps = (double) (b_recv_curr - b_recv_total) / seconds_elapsed;
    bytes_drop_ps = (double) (b_drop_curr - b_drop_total) / seconds_elapsed;
    sleeps_ps = (double) (s_curr - s_total) / seconds_elapsed;

    double mb_recv_ps = (double) bytes_recv_ps / 1000000;
    double gb_recv_ps = (mb_recv_ps * 8)/1000;

    double mb_drop_ps = (double) bytes_drop_ps / 1000000;
    double gb_drop_ps = (mb_drop_ps * 8)/1000;

    if (control_state == Active)
      cerr << "In: " << gb_recv_ps << "Gb/s\tDropped:" << mb_drop_ps << " Mb/s" << endl;
  }

  // update the totals
  b_recv_total = b_recv_curr;
  b_drop_total = b_drop_curr;
  s_total = s_curr;
  prev.tv_sec = curr.tv_sec;
  prev.tv_usec = curr.tv_usec;
}

void spip::SimReceiveDB::open ()
{
  format->prepare (header, "");
  open (header.raw());
}

// write the ascii header to the datablock, then
void spip::SimReceiveDB::open (const char * header_str)
{
  if (verbose)
    cerr << "spip::SimReceiveDB::open()" << endl;
  // open the data block for writing  
  db->open();

  // write the header
  db->write_header (header_str);
}

void spip::SimReceiveDB::close ()
{
  if (verbose)
    cerr << "spip::SimReceiveDB::close()" << endl;
  if (db->is_block_open())
  {
    if (verbose)
      cerr << "spip::SimReceiveDB::close db->close_block(" << db->get_data_bufsz() << ")" << endl;
    db->close_block(db->get_data_bufsz());
  }

  // close the data block, ending the observation
  db->close();
}

// Generate Data into the DB at the specified data rate
bool spip::SimReceiveDB::generate (int tobs)
{
  if (verbose)
    cerr << "spip::SimReceiveDB::transmit tobs=" << tobs << " bytes_per_second=" << bytes_per_second << endl;

  uint64_t bytes_to_gen = (uint64_t) (tobs * bytes_per_second);
  uint64_t total_bytes_gend = 0;

  // block control logic
  char * block;
  const uint64_t data_bufsz = db->get_data_bufsz();
  bool wait;

  // time control
  double micro_seconds = 0;
  double micro_seconds_elapsed = 0;
  double sleep_time = ((double) data_bufsz / bytes_per_second)  * 1e6;

  // thread control
  control_state = Idle;
  keep_generating = true;

  // generate a buffer full of noise that is twice as large as out block size
  format->set_noise_buffer_size (2 * data_bufsz);
  format->generate_noise_buffer (nbit);

  struct timeval timestamp;
  time_t start_second = 0;

  // Main loop
  while (keep_generating)
  {
    // wait for start
    if (control_state == Idle && control_cmd == Start)
    {
      control_state = Active;
      gettimeofday (&timestamp, 0);
      start_second = timestamp.tv_sec + DELTA_START;
    }

    // if started
    if (control_state == Active)
    {
      // open a new buf
      block = (char *) db->open_block();

      // fill with gaussian noise
      format->fill_noise (block, data_bufsz);

      // close the full buf 
      db->close_block (data_bufsz);

      // increment stats by the data generated
      stats->increment_bytes (data_bufsz);

      // increment total bytes generated
      total_bytes_gend += data_bufsz;

      // determine the desired time to wait at the end of this buffer
      micro_seconds += sleep_time;

      // see how long to wait in a busy sleep
      wait = true;
      while (wait)
      {
        gettimeofday (&timestamp, 0);
        micro_seconds_elapsed = (double) (((timestamp.tv_sec - start_second) * 1000000) + timestamp.tv_usec);

        if (micro_seconds_elapsed > micro_seconds)
          wait = false;
      }
    }
    else
    {
      // wait a short time 
      usleep (sleep_time);
    }

    // manual override for TOBS
    if (bytes_to_gen > 0 && total_bytes_gend >= bytes_to_gen)
    {
      control_cmd = Stop;
    }

      // check for stop command
    if (control_cmd == Stop)
    {
      if (verbose)
        cerr << "spip::SimReceiveDB::receive control_cmd == Stop" << endl;
      keep_generating = false;
      control_state = Idle;
      control_cmd = None;
    }
  }

#ifdef _DEBUG
  cerr << "spip::SimReceiveDB::receive exiting" << endl;
#endif

  // close the data block
  close();

  if (control_state == Idle)
    return true;
  else
    return false;
}
