/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/TCPSocketServer.h"
#include "spip/UDPReceiveDB.h"
#include "sys/time.h"

#include "ascii_header.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <new>
#include <pthread.h>

#ifdef  HAVE_VMA
#include <mellanox/vma_extra.h>
#endif

#define UDPReceiveDB_CMD_NONE 0
#define UDPReceiveDB_CMD_START 1
#define UDPReceiveDB_CMD_STOP 2
#define UDPReceiveDB_CMD_QUIT 3

using namespace std;

spip::UDPReceiveDB::UDPReceiveDB(const char * key_string)
{
  db = new DataBlockWrite (key_string);

  db->connect();
  db->lock();

  format = 0;
  control_port = -1;
  header = (char *) malloc (DADA_DEFAULT_HEADER_SIZE);

#ifdef HAVE_VMA
  vma_api = vma_get_api(); 
  if (!vma_api)
    cerr << "spip::UDPReceiveDB::UDPReceiveDB VMA support compiled, but VMA not available" << endl;
  pkts = NULL;
#else
  vma_api = 0;
#endif

  control_cmd = None;
  control_state = Idle;
}

spip::UDPReceiveDB::~UDPReceiveDB()
{
  db->unlock();
  db->disconnect();

  delete db;
}

int spip::UDPReceiveDB::configure (const char * config)
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

  if (!format)
    throw runtime_error ("unable for prepare format");
  format->set_channel_range (start_chan, end_chan);

  uint64_t block_size = db->get_data_bufsz();
  unsigned nsamp_per_block = block_size / (nchan * npol * ndim * nbit / 8);
  format->set_nsamp_per_block (nsamp_per_block);

  // save the header for use on the first open block
  strncpy (header, config, strlen(config)+1);
}

void spip::UDPReceiveDB::prepare (std::string ip_address, int port)
{
  // create and open a UDP receiving socket
  sock = new UDPSocketReceive ();

  sock->open (ip_address, port);
  
  if (!vma_api)
    sock->set_nonblock ();

  cerr << "spip::UDPReceiveDB::prepare resize()" << endl;
  sock->resize (format->get_header_size() + format->get_data_size());

  // this should not be required when using VMA offloading
  cerr << "spip::UDPReceiveDB::prepare resize_kernel_buffer()" << endl;
  sock->resize_kernel_buffer (32*1024*1024);

  stats = new UDPStats (format->get_header_size(), format->get_data_size());
}

void spip::UDPReceiveDB::set_format (spip::UDPFormat * fmt)
{
  if (format)
    delete format;
  format = fmt;
}

void spip::UDPReceiveDB::start_control_thread (int port)
{
  control_port = port;

   pthread_create (&control_thread_id, 0, control_thread_wrapper, this);
}

// wrapper method to start control thread
void * spip::UDPReceiveDB::control_thread_wrapper (void * ptr)
{
  reinterpret_cast<UDPReceiveDB*>( ptr )->control_thread ();
  return 0;
}

void spip::UDPReceiveDB::stop_control_thread ()
{
  control_cmd = Quit;
}

// start a control thread that will receive commands from the TCS/LMC
void spip::UDPReceiveDB::control_thread()
{
#ifdef _DEBUG
  cerr << "spip::UDPReceiveDB::control_thread starting" << endl;
#endif

  if (control_port < 0)
  {
    cerr << "ERROR: no control port specified" << endl;
    return;
  }

  cerr << "spip::UDPReceiveDB::control_thread creating TCPSocketServer" << endl;
  spip::TCPSocketServer * control_sock = new spip::TCPSocketServer();

  // open a listen sock on all interfaces for the control port
  cerr << "spip::UDPReceiveDB::control_thread open socket on port=" 
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

    // update the stats
    update_stats ();
  }
#ifdef _DEBUG
  cerr << "spip::UDPReceiveDB::control_thread exiting" << endl;
#endif
}

// compute the data statisics since the last update
void spip::UDPReceiveDB::update_stats()
{
  b_recv_curr = get_stats()->get_data_transmitted();
  p_drop_curr = get_stats()->get_packets_dropped();
  s_curr      = get_stats()->get_nsleeps();

  struct timeval curr;
  gettimeofday (&curr, 0);

  if (prev.tv_sec > 0)
  {
    double seconds_elapsed = (curr.tv_sec - prev.tv_sec);
    seconds_elapsed += (curr.tv_usec - prev.tv_usec) / 1000000;

    // calc the values since prev update
    bytes_recv_ps = (double) (b_recv_curr - b_recv_total) / seconds_elapsed;
    packets_dropped_ps = (double) (p_drop_curr - p_drop_total) / seconds_elapsed;
    sleeps_ps = (double) (s_curr - s_total) / seconds_elapsed;

    double mb_recv_ps = (double) bytes_recv_ps / 1000000;
    double gb_recv_ps = (mb_recv_ps * 8)/1000;

    if (control_state == Active)
      cerr << "In: " << mb_recv_ps << "MB/s\tDropped:" << packets_dropped_ps << " pkt/sec " << endl;
  }

  // update the totals
  b_recv_total = b_recv_curr;
  p_drop_total = p_drop_curr;
  s_total = s_curr;
  prev.tv_sec = curr.tv_sec;
  prev.tv_usec = curr.tv_usec;
}

void spip::UDPReceiveDB::open ()
{
  open (header);
}

// write the ascii header to the datablock, then
void spip::UDPReceiveDB::open (const char * header)
{
  // open the data block for writing  
  db->open();

  // write the header
  db->write_header (header);
}

void spip::UDPReceiveDB::close ()
{
  cerr << "spip::UDPReceiveDB::close()" << endl;
  if (db->is_block_open())
  {
    cerr << "spip::UDPReceiveDB::close db->close_block(" << db->get_data_bufsz() << ")" << endl;
    db->close_block(db->get_data_bufsz());
  }

  // close the data block, ending the observation
  db->close();
}

// receive UDP packets for the specified time at the specified data rate
bool spip::UDPReceiveDB::receive ()
{
  cerr << "spip::UDPReceiveDB::receive ()" << endl;

  keep_receiving = true;
  prev.tv_sec = 0;
  prev.tv_usec = 0;

  control_state = Idle;

  uint64_t packet_number = 0;
  uint64_t total_bytes_recvd = 0;

  size_t got;
  uint64_t nsleeps = 0;

  struct sockaddr_in client_addr;
  struct sockaddr * addr = (struct sockaddr *) &client_addr;
  socklen_t addr_size = sizeof(struct sockaddr);

  bool have_packet = false;
  bool obs_started = false;

  // block control logic
  char * block;
  bool need_next_block = false;

  const unsigned header_size = format->get_header_size();
  const unsigned data_size   = format->get_data_size();

  const uint64_t samples_per_buf = format->get_samples_for_bytes (db->get_data_bufsz());

  int fd = sock->get_fd();
  char * buf = sock->get_buf();
  char * payload = buf + header_size;
  size_t bufsz = sock->get_bufsz();
  int result;

  // block accounting 
  
  const uint64_t packets_per_buf = db->get_data_bufsz() / data_size;
  uint64_t curr_sample = 0;
  uint64_t next_sample  = 0;
  uint64_t packets_this_buf = 0;
  uint64_t offset;

#ifdef _DEBUG
  cerr << "spip::UDPReceiveDB::receive db->get_data_bufsz()=" << db->get_data_bufsz() << endl;
  cerr << "spip::UDPReceiveDB::receive data_size=" << data_size << endl;
  cerr << "spip::UDPReceiveDB::receive packets_per_buf=" << packets_per_buf << endl;
#endif

#ifdef HAVE_VMA
  int flags;
  cerr << "spip::UDPReceiveDB::receive beginning acquisition loop" << endl;
#endif

  control_state = Idle;
  keep_receiving = true;

  while (keep_receiving)
  {
    if (vma_api)
    {
#ifdef HAVE_VMA
      if (pkts)
      {
        vma_api->free_packets(fd, pkts->pkts, pkts->n_packet_num);
        pkts = NULL;
      }
      while (!have_packet && keep_receiving)
      {
        flags = 0;
        got = vma_api->recvfrom_zcopy(fd, buf, bufsz, &flags, addr, &addr_size);
        if (got == bufsz)
        {
          if (flags & MSG_VMA_ZCOPY) 
          {
            pkts = (struct vma_packets_t*) buf;
            struct vma_packet_t *pkt = &pkts->pkts[0];
            packet_number = format->decode_header_seq ((char *) pkt->iov[0].iov_base);
          }
          else
          {
            packet_number = format->decode_header_seq (buf);  
          }
          have_packet = true;
        }
        else if (got == -1)
        {
          nsleeps++;
          if (nsleeps > 1000)
          {
            stats->sleeps(1000);
            nsleeps -= 1000;
          }
        }
        else
        {
          cerr << "spip::UDPReceiveDB::receive error expected " << bufsz  
               << " B, received " << got << " B" <<  endl;
          control_cmd = Stop;
          cerr << "control_cmd = Stop VMA" << endl;
          control_state = Stopping;
          cerr << "control_state = Stopping VMA" << endl;
          keep_receiving = false;
        }
      }
#endif
    }
    else
    {
      while (!have_packet && keep_receiving)
      {
        got = recvfrom (fd, buf, bufsz, 0, addr, &addr_size);
        if (got == bufsz)
          have_packet = true;
        else if (got == -1)
        {
          nsleeps++;
          if (nsleeps > 1000)
          {
            stats->sleeps(1000);
            nsleeps -= 1000;
          }
        }
        else
        {
          cerr << "spip::UDPReceiveDB::receive error expected " << bufsz  
               << " B, received " << got << " B" <<  endl;
          control_cmd = Stop;
          control_state = Stopping;
          keep_receiving = false;
        }
      }
      if (have_packet)
        format->decode_header (buf);
    }

    if (control_cmd == Stop)
      cerr << "spip::UDPReceiveDB::receive control_cmd == Stop" << endl;

    stats->sleeps(nsleeps);
    nsleeps = 0;

    // check for start of observation, signified by receiving a packet with a 
    // low seqeunce number
    if (control_state == Idle && control_cmd == Start)
    {
      cerr << "packet_number=" << packet_number << endl;
      if (packet_number < 1000)
      {
        control_state = Active;
      }
    }

    if (control_state == Active)
    {
      // open a new data block buffer if necessary
      if (!db->is_block_open())
      {
        block = (char *) db->open_block();
        need_next_block = false;

        if (packets_this_buf == 0 && next_sample > 0)
        {
          cerr << "spip::UDPReceiveDB::receive received 0 packets this buf" << endl;
          keep_receiving = false;
        }

        // number is first packet due in block to first packet of next block
        curr_sample = next_sample;
        next_sample += samples_per_buf;

#ifdef _DEBUG
        cerr << "spip::UDPReceiveDB::receive [" << curr_sample << " - " 
             << next_sample << "] (" << packets_this_buf << ")" << endl;
#endif

        packets_this_buf = 0;
      }

      // copy the current packet into the appropriate place in the buffer
      result = format->insert_packet (block, payload, curr_sample, next_sample);

      if (result == 0)
      {
        have_packet = false;
        packets_this_buf++;
        stats->increment();
      }
      else if (result == UDP_PACKET_TOO_EARLY)
      {
#ifdef _DEBUG
        cerr << "result == UDP_PACKET_TOO_EARLY" << endl;
#endif
        need_next_block = true;
        stats->dropped (packets_per_buf - packets_this_buf);
      }
      else if (result == UDP_PACKET_TOO_LATE)
      {
//#ifdef _DEBUG
        cerr << "result == UDP_PACKET_TOO_LATE" << endl;
//#endif
        format->print_packet_header();
        have_packet = false;
        keep_receiving = false;
        stats->dropped();
      }
      else
      {
        ;
      }

      // close open data block buffer if is is now full
      if (packets_this_buf == packets_per_buf || need_next_block)
      {
  #ifdef _DEBUG
        cerr << "spip::UDPReceiveDB::receive close_block packets_this_buf=" 
             << packets_this_buf << " packets_per_buf=" << packets_per_buf 
             << " need_next_block=" << need_next_block
             << " have_packet=" << have_packet << endl;
        format->print_packet_header();
  #endif
        db->close_block(db->get_data_bufsz());
      }
    }

    if (control_cmd == Stop || control_cmd == Quit)
    {
#ifdef _DEBUG
      cerr << "spip::UDPReceiveDB::receive control_cmd=" << control_cmd 
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
  cerr << "spip::UDPReceiveDB::receive exiting" << endl;
#endif

  // close the data block
  close();

  if (control_state == Idle)
    return true;
  else
    return false;
}
