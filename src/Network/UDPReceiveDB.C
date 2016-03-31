/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/TCPSocketServer.h"
#include "spip/AsciiHeader.h"
#include "spip/UDPReceiveDB.h"
#include "spip/Time.h"
#include "sys/time.h"

#include <signal.h>
#include <unistd.h>
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

//#define _DEBUG

using namespace std;

spip::UDPReceiveDB::UDPReceiveDB(const char * key_string)
{
  db = new DataBlockWrite (key_string);

  db->connect();
  db->lock();

  format = 0;
  control_port = -1;

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
  // save the header for use on the first open block
  header.load_from_str (config);

  if (header.get ("NCHAN", "%u", &nchan) != 1)
    throw invalid_argument ("NCHAN did not exist in header");

  if (header.get ("NBIT", "%u", &nbit) != 1)
    throw invalid_argument ("NBIT did not exist in header");

  if (header.get ("NPOL", "%u", &npol) != 1)
    throw invalid_argument ("NPOL did not exist in header");

  if (header.get ("NDIM", "%u", &ndim) != 1)
    throw invalid_argument ("NDIM did not exist in header");

  if (header.get ("TSAMP", "%f", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in header");

  if (header.get ("BW", "%f", &bw) != 1)
    throw invalid_argument ("BW did not exist in header");

  char * buffer = (char *) malloc (128);
  if (header.get ("DATA_HOST", "%s", buffer) != 1)
    throw invalid_argument ("DATA_HOST did not exist in header");
  data_host = string (buffer);
  if (header.get ("DATA_PORT", "%d", &data_port) != 1)
    throw invalid_argument ("DATA_PORT did not exist in header");

  cerr << "UDP source " << data_host << ":" << data_port << endl;

  bits_per_second  = (nchan * npol * ndim * nbit * 1000000) / tsamp;
  bytes_per_second = bits_per_second / 8;

  if (!format)
    throw runtime_error ("unable for configure format");
  format->configure (header, "");

  uint64_t block_size = db->get_data_bufsz();
  unsigned nsamp_per_block = block_size / (nchan * npol * ndim * nbit / 8);
  format->set_nsamp_per_block (nsamp_per_block);

  // now write new params to header
  uint64_t resolution = format->get_resolution();
  cerr << "spip::UDPReceiveDB::configure resolution=" << resolution << endl;
  if (header.set("RESOLUTION", "%lu", resolution) < 0)
    throw invalid_argument ("failed to write RESOLUTION to header");

  free (buffer);
}

void spip::UDPReceiveDB::prepare ()
{
  // create and open a UDP receiving socket
  sock = new UDPSocketReceive ();
  sock->open (data_host, data_port);
  
  if (!vma_api)
  {
    cerr << "setting nonblock" << endl;
    sock->set_nonblock ();
  }

  size_t sock_bufsz = format->get_header_size() + format->get_data_size();
  cerr << "spip::UDPReceiveDB::prepare resize(" << sock_bufsz << ")" << endl;
  sock->resize (sock_bufsz);

  // this should not be required when using VMA offloading
  //if (!vma_api)
  {
    cerr << "spip::UDPReceiveDB::prepare resize_kernel_buffer()" << endl;
    sock->resize_kernel_buffer (32*1024*1024);
  }

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
    cerr << "WARNING: no control port, using 32132" << endl;
    control_port = 32132;
  }

  cerr << "spip::UDPReceiveDB::control_thread creating TCPSocketServer" << endl;
  spip::TCPSocketServer * control_sock = new spip::TCPSocketServer();

  // open a listen sock on all interfaces for the control port
  cerr << "spip::UDPReceiveDB::control_thread open socket on port=" 
       << control_port << endl;
  control_sock->open ("any", control_port, 1);

  int fd = -1;
  int verbose = 1;

  char * cmds = (char *) malloc (DEFAULT_HEADER_SIZE);
  char * cmd  = (char *) malloc (32);

  //control_cmd = None;

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
  cerr << "spip::UDPReceiveDB::control_thread exiting" << endl;
#endif
}


void spip::UDPReceiveDB::start_stats_thread ()
{
  pthread_create (&stats_thread_id, NULL, stats_thread_wrapper, this);
}

void spip::UDPReceiveDB::stop_stats_thread ()
{
  control_cmd = Stop;
  void * result;
  pthread_join (stats_thread_id, &result);
}

/* 
 *  Thread to print simple capture statistics
 */
void spip::UDPReceiveDB::stats_thread()
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

  cerr << "spip::UDPReceiveDB::stats_thread starting polling" << endl;

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
      fprintf (stderr,"Recv %6.3f [Gb/s] Sleeps %lu Dropped %lu B\n", gb_recv_ps, s_1sec, b_drop_curr);
      sleep (1);
    }
    sleep(1);
  }
}


// compute the data statisics since the last update
void spip::UDPReceiveDB::update_stats()
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

void spip::UDPReceiveDB::open ()
{
  cerr << "spip::UDPReceiveDB::open format->prepare()" << endl;

  // check if UTC_START has been set
  char * buffer = (char *) malloc (128);
  if (header.get ("UTC_START", "%s", buffer) == -1)
  {
    cerr << "spip::UDPReceiveDB::open no UTC_START in header" << endl;
    time_t now = time(0);
    spip::Time utc_start (now);
    utc_start.add_seconds (2);
    std::string utc_str = utc_start.get_gmtime();
    cerr << "spip::UDPReceiveDB::open UTC_START=" << utc_str  << endl;
    if (header.set ("UTC_START", "%s", utc_str.c_str()) < 0)
      throw invalid_argument ("failed to write UTC_START to header");
  }

  format->prepare(header, "");
  open (header.raw());
  free (buffer);
}

// write the ascii header to the datablock, then
void spip::UDPReceiveDB::open (const char * header_str)
{
  // open the data block for writing  
  db->open();

  // write the header
  db->write_header (header_str);
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

  //uint64_t packet_number = 0;
  uint64_t total_bytes_recvd = 0;

  int got;
  uint64_t nsleeps = 0;

  struct sockaddr_in client_addr;
  struct sockaddr * addr = (struct sockaddr *) &client_addr;
  socklen_t addr_size = sizeof(struct sockaddr);

  bool have_packet = false;
  bool obs_started = false;

  // block control logic
  char * block = (char *) db->open_block();
  bool need_next_block = false;

  const uint64_t data_bufsz = db->get_data_bufsz();
  const unsigned header_size = format->get_header_size();
  const unsigned data_size   = format->get_data_size();
  const uint64_t samples_per_buf = format->get_samples_for_bytes (data_bufsz);
  cerr << "spip::UDPReceiveDB::receive samples_per_buf=" << samples_per_buf << " data_bufsz=" << data_bufsz << endl;

  int fd = sock->get_fd();
  char * buf = sock->get_buf();
  char * payload = buf + header_size;
  size_t sock_bufsz = sock->get_bufsz();
  int result;

  // block accounting 
  uint64_t curr_byte_offset = 0;
  uint64_t next_byte_offset = data_bufsz;

  // overflow buffer
  const uint64_t overflow_bufsz = 2097152;
  uint64_t overflow_lastbyte = 0;
  uint64_t overflow_maxbyte = next_byte_offset + overflow_bufsz;
  uint64_t overflowed_bytes = 0;
  char * overflow = (char *) malloc(overflow_bufsz);
  memset (overflow, 0, overflow_bufsz);

  uint64_t bytes_this_buf = 0;
  int64_t byte_offset;

  unsigned bytes_received, bytes_dropped;

#ifdef _DEBUG
  cerr << "spip::UDPReceiveDB::receive sock_bufsz=" << sock_bufsz << endl;
  cerr << "spip::UDPReceiveDB::receive data_size=" << data_size << endl;
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
        got = (int) vma_api->recvfrom_zcopy(fd, buf, sock_bufsz, &flags, addr, &addr_size);
        if (got  > 32)
        {
          if (flags & MSG_VMA_ZCOPY) 
          {
            pkts = (struct vma_packets_t*) buf;
            struct vma_packet_t *pkt = &pkts->pkts[0];
            buf = (char *) (pkt->iov[0].iov_base);
          }
          have_packet = true;
        }
        else
        {
          cerr << "spip::UDPReceiveDB::receive error expected " << sock_bufsz  
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
        got = (int) recvfrom (fd, buf, sock_bufsz, 0, addr, &addr_size);
        if (got > 32)
        {
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
          cerr << "spip::UDPReceiveDB::receive error expected " << sock_bufsz  
               << " B, received " << got << " B" <<  endl;
          control_cmd = Stop;
          control_state = Stopping;
          keep_receiving = false;
        }
      }
    }

    if (control_cmd == Stop)
      cerr << "spip::UDPReceiveDB::receive control_cmd == Stop" << endl;

    stats->sleeps(nsleeps);
    nsleeps = 0;

    // check for start of observation, signified by receiving a packet with a 
    // low seqeunce number
    if (control_state == Idle && control_cmd == Start)
    {
      cerr << "control_state == Active" << endl;
      control_state = Active;
    }

    if (control_state == Active && have_packet)
    {
      // open a new data block buffer if necessary
      if (!db->is_block_open())
      {
        block = (char *) db->open_block();
        need_next_block = false;

        if (bytes_this_buf == 0 && curr_byte_offset > 0)
        {
          cerr << "spip::UDPReceiveDB::receive received 0 packets this buf" << endl;
          keep_receiving = false;
        }

        // update absolute limits
        curr_byte_offset = next_byte_offset;
        next_byte_offset += data_bufsz;
        overflow_maxbyte = next_byte_offset + overflow_bufsz;

//#ifdef _DEBUG
        cerr << "spip::UDPReceiveDB::receive [" << curr_byte_offset << " - " 
             << next_byte_offset << "] (" << bytes_this_buf << ")" << endl;
//#endif

        if (overflow_lastbyte > 0)
        {
          memcpy (block, overflow, overflow_lastbyte);
          overflow_lastbyte = 0;
          bytes_this_buf = overflowed_bytes;
          stats->increment_bytes (overflowed_bytes);
          overflowed_bytes = 0;
        }
        else
          bytes_this_buf = 0;
      }

      byte_offset = format->decode_packet (buf, &bytes_received);

      // packet belongs in current buffer
      if ((byte_offset >= curr_byte_offset) && (byte_offset < next_byte_offset))
      {
        bytes_this_buf += bytes_received;
        stats->increment_bytes (bytes_received); 
        format->insert_last_packet (block + (byte_offset - curr_byte_offset));
        have_packet = false;
      }
      else if ((byte_offset >= next_byte_offset) && (byte_offset < overflow_maxbyte))
      {
        format->insert_last_packet (overflow + (byte_offset - next_byte_offset));
        overflow_lastbyte = std::max((byte_offset - next_byte_offset) + bytes_received, overflow_lastbyte);
        overflowed_bytes += bytes_received;
        have_packet = false;
      }
      else if (byte_offset < 0)
      {
        // ignore
        have_packet = false;
      }
      else
      {
        need_next_block = true;
        have_packet = true;
      }


      // close open data block buffer if is is now full
      if (bytes_this_buf >= data_bufsz || need_next_block)
      {
#ifdef _DEBUG
          cerr << bytes_this_buf << " / " << data_bufsz << " => " << 
              ((float) bytes_this_buf / (float) data_bufsz) * 100 << endl;
        cerr << "spip::UDPReceiveDB::receive close_block bytes_this_buf=" 
             << bytes_this_buf << " bytes_per_buf=" << data_bufsz 
             << " need_next_block=" << need_next_block
             << " have_packet=" << have_packet << endl;
        format->print_packet_header();
#endif
        stats->dropped_bytes (data_bufsz - bytes_this_buf);
        db->close_block (data_bufsz);
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
