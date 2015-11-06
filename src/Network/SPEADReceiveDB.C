/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/TCPSocketServer.h"
#include "spip/SPEADReceiveDB.h"
#include "sys/time.h"

#include "ascii_header.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <new>
#include <pthread.h>

#define SPEADReceiveDB_CMD_NONE 0
#define SPEADReceiveDB_CMD_START 1
#define SPEADReceiveDB_CMD_STOP 2
#define SPEADReceiveDB_CMD_QUIT 3

using namespace std;

spip::SPEADReceiveDB::SPEADReceiveDB(const char * key_string)
{
  db = new DataBlockWrite (key_string);

  db->connect();
  db->lock();

  control_port = -1;
  header = (char *) malloc (DADA_DEFAULT_HEADER_SIZE);

  control_cmd = None;
  control_state = Idle;
}

spip::SPEADReceiveDB::~SPEADReceiveDB()
{
  db->unlock();
  db->disconnect();

  delete db;
}

int spip::SPEADReceiveDB::configure (const char * config)
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

  unsigned start_chan, end_chan;
  if (ascii_header_get (config, "START_CHANNEL", "%u", &start_chan) != 1)
    throw invalid_argument ("START_CHANNEL did not exist in header");
  if (ascii_header_get (config, "END_CHANNEL", "%u", &end_chan) != 1)
    throw invalid_argument ("END_CHANNEL did not exist in header");

  // TODO parameterize this
  heap_size = 262144;

  // save the header for use on the first open block
  strncpy (header, config, strlen(config)+1);
}

void spip::SPEADReceiveDB::prepare (std::string ip_address, int port)
{
  // make a shared pool
  pool = std::make_shared<spead2::memory_pool>(16384, 26214400, 12, 8);

  stream = stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2);

  stream.set_memory_pool(pool);

  endpoint = endpoint (boost::asio::ip::address_v4::from_string(ip_address), port);

  stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 8 * 1024 * 1024);
}

void spip::SPEADReceiveDB::start_control_thread (int port)
{
  control_port = port;

  int errno = pthread_create (&control_thread_id, 0, control_thread_wrapper, this);
  if (errno != 0)
    throw runtime_error ("pthread_create");
}

// wrapper method to start control thread
void * spip::SPEADReceiveDB::control_thread_wrapper (void * ptr)
{
  reinterpret_cast<SPEADReceiveDB*>( ptr )->control_thread ();
  return 0;
}

void spip::SPEADReceiveDB::stop_control_thread ()
{
  control_cmd = Quit;
}

// start a control thread that will receive commands from the TCS/LMC
void spip::SPEADReceiveDB::control_thread()
{
#ifdef _DEBUG
  cerr << "spip::SPEADReceiveDB::control_thread starting" << endl;
#endif

  if (control_port < 0)
  {
    cerr << "ERROR: no control port specified" << endl;
    return;
  }

  cerr << "spip::SPEADReceiveDB::control_thread creating TCPSocketServer" << endl;
  spip::TCPSocketServer * control_sock = new spip::TCPSocketServer();

  // open a listen sock on all interfaces for the control port
  cerr << "spip::SPEADReceiveDB::control_thread open socket on port=" 
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

        if (verbose)
          cerr << "control_thread: open()" << endl;
        open ();

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
  cerr << "spip::SPEADReceiveDB::control_thread exiting" << endl;
#endif
}

void spip::SPEADReceiveDB::open ()
{
  open (header);
}

// write the ascii header to the datablock, then
void spip::SPEADReceiveDB::open (const char * header)
{
  // open the data block for writing  
  db->open();

  // write the header
  db->write_header (header);
}

void spip::SPEADReceiveDB::close ()
{
  cerr << "spip::SPEADReceiveDB::close()" << endl;
  if (db->is_block_open())
  {
    cerr << "spip::SPEADReceiveDB::close db->close_block(" << db->get_data_bufsz() << ")" << endl;
    db->close_block(db->get_data_bufsz());
  }

  // close the data block, ending the observation
  db->close();
}

// receive SPEAD heaps for the specified time at the specified data rate
bool spip::SPEADReceiveDB::receive ()
{
  cerr << "spip::SPEADReceiveDB::receive ()" << endl;

  control_state = Idle;

  uint64_t total_bytes_recvd = 0;
  bool obs_started = false;

  // block control logic
  char * block;
  bool need_next_block = false;

  const uint64_t samples_per_buf = format->get_samples_for_bytes (db->get_data_bufsz());

  // block accounting 
  const uint64_t heaps_per_buf = db->get_data_bufsz() / heap_size;
  uint64_t curr_sample = 0;
  uint64_t next_sample  = 0;
  uint64_t heaps_this_buf = 0;
  uint64_t offset;

#ifdef _DEBUG
  cerr << "spip::SPEADReceiveDB::receive db->get_data_bufsz()=" << db->get_data_bufsz() << endl;
  cerr << "spip::SPEADReceiveDB::receive data_size=" << data_size << endl;
  cerr << "spip::SPEADReceiveDB::receive heaps_per_buf=" << heaps_per_buf << endl;
#endif

  control_state = Idle;
  keep_receiving = true;

  while (keep_receiving)
  {
    if (control_state == Idle && control_cmd == Start)
    {
      cerr << "spip::SPEADReceiveDB::receive: cmd==Start" << endl;
      control_state = Active;
    }

    if (control_state == Active)
    {
      // open a new data block buffer if necessary
      if (!db->is_block_open())
      {
        block = (char *) db->open_block();
        need_next_block = false;

        if (heaps_this_buf == 0 && next_sample > 0)
        {
          cerr << "spip::SPEADReceiveDB::receive received 0 packets this buf" << endl;
          keep_receiving = false;
        }

        // number is first packet due in block to first packet of next block
        curr_sample = next_sample;
        next_sample += samples_per_buf;

#ifdef _DEBUG
        cerr << "spip::SPEADReceiveDB::receive [" << curr_sample << " - " 
             << next_sample << "] (" << heaps_this_buf << ")" << endl;
#endif
        heaps_this_buf = 0;
      }

      // receive a single head from the ring-buffered stream
      try
      {
        spead2::recv::heap fh = stream.pop();
        n_complete++;
        show_heap(fh);
      }
      catch (spead2::ringbuffer_stopped &e)
      {
        keep_receiving = false;
      }

      // copy the heap into the DADA block
      // TODO

      // determine if this data buffer is now full

      // close open data block buffer if is is now full
      if (heaps_this_buf == heaps_per_buf || need_next_block)
      {
  #ifdef _DEBUG
        cerr << "spip::SPEADReceiveDB::receive close_block heaps_this_buf=" 
             << heaps_this_buf << " heaps_per_buf=" << heaps_per_buf 
             << " need_next_block=" << need_next_block << endl;
  #endif
        db->close_block(db->get_data_bufsz());
      }
    }

    if (control_cmd == Stop || control_cmd == Quit)
    {
#ifdef _DEBUG
      cerr << "spip::SPEADReceiveDB::receive control_cmd=" << control_cmd 
           << endl; 
#endif
      cerr << "Stopping acquisition" << endl;
      keep_receiving = false;
      control_state = Idle;
      cerr << "control_state = Idle" << endl;
      control_cmd = None;
      cerr << "control_cmd = None" << endl;
    }
  }

  cerr << "Closing datablock" << endl;

#ifdef _DEBUG
  cerr << "spip::SPEADReceiveDB::receive exiting" << endl;
#endif

  // close the data block
  close();

  if (control_state == Idle)
    return true;
  else
    return false;
}
