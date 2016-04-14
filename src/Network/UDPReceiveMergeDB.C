/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/HardwareAffinity.h"
#include "spip/TCPSocketServer.h"
#include "spip/UDPReceiveMergeDB.h"
#include "spip/Time.h"
//#include "sys/time.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <new>

#define UDPReceiveMergeDB_CMD_NONE 0
#define UDPReceiveMergeDB_CMD_START 1
#define UDPReceiveMergeDB_CMD_STOP 2
#define UDPReceiveMergeDB_CMD_QUIT 3

//#define _DEBUG

using namespace std;

spip::UDPReceiveMergeDB::UDPReceiveMergeDB (const char * key_string)
{
  db = new DataBlockWrite (key_string);

  db->connect();
  db->lock();

  control_port = -1;

  control_cmd = None;
  control_state = Idle;
  control_states[0] = Idle;
  control_states[1] = Idle;

  cond = PTHREAD_COND_INITIALIZER;
  mutex = PTHREAD_MUTEX_INITIALIZER;

  pthread_cond_init( &cond, NULL);
  pthread_mutex_init( &mutex, NULL);

  data_mcasts[0] = string ();
  data_mcasts[1] = string ();

  verbose = 1;
}

spip::UDPReceiveMergeDB::~UDPReceiveMergeDB()
{
  db->unlock();
  db->disconnect();

  delete db;
}

int spip::UDPReceiveMergeDB::configure (const char * config)
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

  if (header.get ("DATA_HOST_0", "%s", buffer) != 1)
    throw invalid_argument ("DATA_HOST_0 did not exist in header");
  data_hosts[0] = string (buffer);
  if (header.get ("DATA_HOST_1", "%s", buffer) != 1)
    throw invalid_argument ("DATA_HOST_1 did not exist in header");
  data_hosts[1] = string (buffer);

  if (header.get ("DATA_PORT_0", "%d", &data_ports[0]) != 1)
    throw invalid_argument ("DATA_PORT_0 did not exist in header");
  if (header.get ("DATA_PORT_1", "%d", &data_ports[1]) != 1)
    throw invalid_argument ("DATA_PORT_1 did not exist in header");

  if (header.get ("DATA_MCAST_0", "%s", buffer) == 1)
    data_mcasts[0] = string (buffer);
  if (header.get ("DATA_MCAST_1", "%s", buffer) == 1)
    data_mcasts[1] = string (buffer);

  bits_per_second  = (nchan * npol * ndim * nbit * 1000000) / tsamp;
  bytes_per_second = bits_per_second / 8;

  if (!formats[0] or !formats[1])
    throw runtime_error ("unable for prepare format");

  formats[0]->configure (header, "_0");
  formats[1]->configure (header, "_1");

  // now write new params to header
  uint64_t resolution = formats[0]->get_resolution();
  cerr << "spip::UDPReceiveDB::configure resolution=" << resolution << endl;
  if (header.set("RESOLUTION", "%lu", resolution) < 0)
    throw invalid_argument ("failed to write RESOLUTION to header");

  return 0;
}

void spip::UDPReceiveMergeDB::prepare ()
{
  for (unsigned i=0; i<2; i++)
  {
    socks[i] = new UDPSocketReceive ();

    if (data_mcasts[i].size() > 0)
      socks[i]->open_multicast (data_hosts[i], data_mcasts[i], data_ports[i]);
    else
      socks[i]->open (data_hosts[i], data_ports[i]);

#ifdef HAVE_VMA
    vma_apis[i] = vma_get_api();
    if (!vma_apis[i])
    {
      cerr << "WARNING: VMA support compiled, but VMA not available" << endl;
      pkts[i] = NULL;
    }
#else
    vma_apis[i] = 0;
#endif

    size_t sock_bufsz = formats[i]->get_header_size() + formats[i]->get_data_size();
    if (!vma_apis[i])
      socks[i]->set_nonblock ();
    socks[i]->resize (sock_bufsz);
    if (!vma_apis[i])
      socks[i]->resize_kernel_buffer (32*1024*1024);
    stats[i] = new UDPStats (formats[i]->get_header_size(), formats[i]->get_data_size());
  }

      
}

void spip::UDPReceiveMergeDB::set_formats (spip::UDPFormat * fmt1, spip::UDPFormat * fmt2)
{
  if (formats[0])
    delete formats[0];
  formats[0] = fmt1;
  if (formats[1])
    delete formats[1];
  formats[1] = fmt2;
}

void spip::UDPReceiveMergeDB::start_control_thread (int port)
{
  control_port = port;
  pthread_create (&control_thread_id, NULL, control_thread_wrapper, this);
}

void spip::UDPReceiveMergeDB::stop_control_thread ()
{
  control_cmd = Quit;
}

void spip::UDPReceiveMergeDB::set_control_cmd (spip::ControlCmd cmd)
{
  pthread_mutex_lock (&mutex);  
  control_cmd = cmd;
  pthread_cond_signal (&cond);
  pthread_mutex_unlock (&mutex);
}

// start a control thread that will receive commands from the TCS/LMC
void spip::UDPReceiveMergeDB::control_thread()
{
#ifdef _DEBUG
  cerr << "spip::UDPReceiveMergeDB::control_thread starting" << endl;
#endif

  if (control_port < 0)
  {
    cerr << "ERROR: no control port specified" << endl;
    return;
  }

  cerr << "spip::UDPReceiveMergeDB::control_thread creating TCPSocketServer" << endl;
  spip::TCPSocketServer * control_sock = new spip::TCPSocketServer();

  // open a listen sock on all interfaces for the control port
  cerr << "spip::UDPReceiveMergeDB::control_thread open socket on port=" 
       << control_port << endl;
  control_sock->open ("any", control_port, 1);

  int fd = -1;
  int verbose = 1;

  char * cmds = (char *) malloc (DADA_DEFAULT_HEADER_SIZE);
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
      ssize_t bytes_read = read (fd, cmds, DADA_DEFAULT_HEADER_SIZE);

      if (verbose)
        cerr << "control_thread: bytes_read=" << bytes_read << endl;

      control_sock->close_client();
      fd = -1;

      // now check command in list of header commands
      if (spip::AsciiHeader::header_get (cmds, "COMMAND", "%s", cmd) != 1)
        throw invalid_argument ("COMMAND did not exist in header");
      if (verbose)
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
        if (verbose)
          cerr << "control_thread: control_cmd = Start" << endl;
        set_control_cmd (Start);
      }
      else if (strcmp (cmd, "STOP") == 0)
      {
        if (verbose)
          cerr << "control_thread: control_cmd = Stop" << endl;
        set_control_cmd (Stop);
      }
      else if (strcmp (cmd, "QUIT") == 0)
      {
        if (verbose)
          cerr << "control_thread: control_cmd = Quit" << endl;
        set_control_cmd (Quit);
      }
    }
  }
#ifdef _DEBUG
  cerr << "spip::UDPReceiveMergeDB::control_thread exiting" << endl;
#endif
}

void spip::UDPReceiveMergeDB::open ()
{

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

  // prepare formats based on run-time header
  formats[0]->prepare (header, "_0");
  formats[1]->prepare (header, "_1");

  open (header.raw());
}

// write the ascii header to the datablock, then
void spip::UDPReceiveMergeDB::open (const char * header_str)
{
  cerr << "spip::UDPReceiveMergeDB::open()" << endl;

  // open the data block for writing  
  db->open();

  // write the header
  db->write_header (header_str);

  cerr << "spip::UDPReceiveMergeDB::open db=" << (void *) db << endl;
}

void spip::UDPReceiveMergeDB::close ()
{
  if (verbose)
    cerr << "spip::UDPReceiveMergeDB::close()" << endl;
  if (db->is_block_open())
  {
    if (verbose)
      cerr << "spip::UDPReceiveMergeDB::close db->close_block(" << db->get_data_bufsz() << ")" << endl;
    db->close_block(db->get_data_bufsz());
  }

  // close the data block, ending the observation
  db->close();
}

void spip::UDPReceiveMergeDB::start_threads (int c1, int c2)
{
  // cpu cores on which to bind each recv thread
  cores[0] = c1;
  cores[1] = c2;

  // flag for whether the recv thread has filled the current buffer
  full[0] = true;
  full[1] = true;

  pthread_create (&datablock_thread_id, NULL, datablock_thread_wrapper, this);
  pthread_create (&recv_thread1_id, NULL, recv_thread1_wrapper, this);
  pthread_create (&recv_thread2_id, NULL, recv_thread2_wrapper, this);
}

void spip::UDPReceiveMergeDB::join_threads ()
{
  void * result;
  pthread_join (datablock_thread_id, &result);
  pthread_join (recv_thread1_id, &result);
  pthread_join (recv_thread2_id, &result);
}

bool spip::UDPReceiveMergeDB::datablock_thread ()
{
  pthread_mutex_lock (&mutex);

  // open the data block for writing
  block = (char *) (db->open_block());

#ifdef _DEBUG
  uint64_t ibuf = 0;
#endif

  // wait for the starting command from the control_thread
  while (control_cmd == None)
    pthread_cond_wait (&cond, &mutex);

  // if we have a start command then we can continue
  if (control_cmd == Start)
  {
    cerr << "spip::UDPReceiveMergeDB::datablock control_cmd == Start" << endl;
    // state of this thread
    control_state = Active;
    // signal both receive threads to begin
    control_states[0] = Active;
    control_states[1] = Active;
    full[0] = false;
    full[1] = false;
#ifdef _DEBUG
    cerr << "spip::UDPReceiveMergeDB::datablock opened buffer " << ibuf << endl;
#endif

    // single receive threads to commence
    pthread_cond_broadcast (&cond);
    pthread_mutex_unlock(&mutex);
  }
  else if (control_cmd == Stop)
  {
    cerr << "spip::UDPReceiveMergeDB::datablock_thread received "
         <<  "a Stop command prior to starting" << endl;
    control_state = Idle;
    pthread_cond_broadcast (&cond);
    pthread_mutex_unlock(&mutex);
    pthread_exit (NULL);
  }
  else
    throw invalid_argument ("datathread encounter an unexpected control_cmd");

  // while the receiving state is Active
  while (control_state == Active)
  {
    pthread_mutex_lock (&mutex);

    // if the current buffer has been filled  by both receive threads
    while (!full[0] || !full[1])
    {
#ifdef _DEBUG
      cerr << "spip::UDPReceiveMergeDB::datablock checking buffer " << ibuf 
           << " [" << full[0] << "," << full[1] << "]" << endl;
#endif
      pthread_cond_wait (&cond, &mutex);
    }
    
#ifdef _DEBUG
    cerr << "spip::UDPReceiveMergeDB::datablock filled buffer " << ibuf << endl;
#endif
    
    // close data block
    db->close_block(db->get_data_bufsz());
#ifdef _DEBUG
    ibuf++;
#endif

    // check for a control command whilst no buffers are open
    if (control_cmd == Stop)
    {
      cerr << "STATE=Idle" << endl;
      control_state = Idle;
    }
    else
    {
      block = (char *) (db->open_block());
      full[0] = full[1] = false;
    }
    pthread_cond_broadcast (&cond);
    pthread_mutex_unlock (&mutex);
  }

  pthread_exit (NULL);
}

bool spip::UDPReceiveMergeDB::receive_thread (int p)
{
  //if (verbose)
  //  cerr << "spip::UDPReceiveMergeDB::receive[" << p << "] ()" << endl;

  spip::HardwareAffinity hw_affinity;
  //cerr << "spip::UDPReceiveMergeDB::receive["<<p<<"] binding to core " << cores[p] << endl;
  hw_affinity.bind_thread_to_cpu_core (cores[p]);
  hw_affinity.bind_to_memory (cores[p]);

  UDPSocketReceive * sock = socks[p];
  UDPFormat * format = formats[p];
  UDPStats * stat = stats[p];
#if HAVE_VMA
  struct vma_api_t *vma_api = vma_apis[p];
  struct vma_packets_t* pkt = pkts[p];
#endif

  bool keep_receiving = true;
  bool have_packet = false;
  bool obs_started = false;

  struct sockaddr_in client_addr;
  struct sockaddr * addr = (struct sockaddr *) &client_addr;
  socklen_t addr_size = sizeof(struct sockaddr);


  int fd = sock->get_fd();
  char * buf = sock->get_buf();
  size_t sock_bufsz = sock->get_bufsz();
  int result;

  // block accounting 
  const int64_t data_bufsz = db->get_data_bufsz() / 2;
  int64_t curr_byte_offset = 0;
  int64_t next_byte_offset = 0;

  // overflow buffer
  const int64_t overflow_bufsz = 2097152;
  int64_t overflow_lastbyte = 0;
  int64_t overflow_maxbyte = 0;
  int64_t overflowed_bytes = 0;
  char * overflow = (char *) malloc(overflow_bufsz);
  memset (overflow, 0, overflow_bufsz);

  uint64_t bytes_this_buf = 0;
  int64_t byte_offset;

  bool filled_this_buffer = false;
  unsigned bytes_received, bytes_dropped;
  int flags, got;
  uint64_t nsleeps, ibuf;
  ibuf = 0;

  // wait for datablock thread to change state to Active
  pthread_mutex_lock (&mutex);
#ifdef _DEBUG
  cerr << "spip::UDPReceiveMergeDB::receive["<<p<<"] waiting for Active" << endl;
#endif
  while (control_states[p] == Idle)
    pthread_cond_wait (&cond, &mutex);
  pthread_mutex_unlock (&mutex);

  // main data acquisition loop
  while (control_states[p] == Active)
  {
    // wait until the datablock thread sets the state of this buffer
    // to empty (i.e. when it has provided a new buffer to fill
    pthread_mutex_lock (&mutex);
    while (full[p])
    {
#ifdef _DEBUG
      cerr << "spip::UDPReceiveMergeDB::receive["<<p<<"] waiting for empty " << ibuf << " [" << full[0] << ", " << full[1] << "]" << endl;
#endif
      pthread_cond_wait (&cond, &mutex);
    }

    curr_byte_offset = next_byte_offset;
    next_byte_offset += data_bufsz;
    overflow_maxbyte = next_byte_offset + overflow_bufsz;

#ifdef _DEBUG
    cerr << "spip::UDPReceiveMergeDB::receive["<<p<<"] filling buffer " << ibuf << " [" <<  curr_byte_offset << " - " << next_byte_offset << "] overflow=" << overflow_lastbyte << endl;
#endif

    // signal other threads waiting on the condition
    pthread_cond_broadcast (&cond);
    pthread_mutex_unlock (&mutex);
    filled_this_buffer = false;
    
    // pointer to polarisation block 
    char * pol_block = block + (data_bufsz * p);

    // overflow handler
    if (overflow_lastbyte > 0)
    {
      memcpy (pol_block, overflow, overflow_lastbyte);
      overflow_lastbyte = 0;
      bytes_this_buf = overflowed_bytes;
      stat->increment_bytes (overflowed_bytes);
      overflowed_bytes = 0;
    }
    else
      bytes_this_buf = 0;

    // while we have not filled this buffer with data from
    // this polarisation
    while (!filled_this_buffer && keep_receiving)
    {
      if (vma_api)
      {
  #ifdef HAVE_VMA
        if (pkt)
        {
          vma_api->free_packets(fd, pkt->pkts, pkt->n_packet_num);
          pkt = NULL;
        }
        while (!have_packet && keep_receiving)
        {
          flags = 0;
          got = (int) vma_api->recvfrom_zcopy(fd, buf, sock_bufsz, &flags, addr, &addr_size);
          if (got  > 32)
          {
            if (flags & MSG_VMA_ZCOPY)
            {
              pkt = (vma_packets_t*) buf;
              //struct vma_packet_t *vma_pkt = &pkt->pkts[0];
              //buf = (char *) (vma_pkt->iov[0].iov_base);
              buf = (char *) pkt->pkts[0].iov[0].iov_base;
            }
            have_packet = true;
          }
          else
          {
            cerr << "spip::UDPReceiveDB::receive error expected " << sock_bufsz
                 << " B, received " << got << " B" <<  endl;
            set_control_cmd (Stop);
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
              stat->sleeps(1000);
              nsleeps -= 1000;
            }
          }
          else
          {
            cerr << "spip::UDPReceiveDB::receive error expected " << sock_bufsz
                 << " B, received " << got << " B" <<  endl;
            set_control_cmd (Stop);
            keep_receiving = false;
          }
        }
      }


      byte_offset = format->decode_packet (buf, &bytes_received);

      // packet belongs in current buffer
      if ((byte_offset >= curr_byte_offset) && (byte_offset < next_byte_offset))
      {
        bytes_this_buf += bytes_received;
        stat->increment_bytes (bytes_received);
        format->insert_last_packet (pol_block + (byte_offset - curr_byte_offset));
        have_packet = false;
      }
      else if ((byte_offset >= next_byte_offset) && (byte_offset < overflow_maxbyte))
      {
        format->insert_last_packet (overflow + (byte_offset - next_byte_offset));
        overflow_lastbyte = std::max((byte_offset - next_byte_offset) + bytes_received, overflow_lastbyte);
        overflowed_bytes += bytes_received;
        have_packet = false;
      }
      else if (byte_offset < curr_byte_offset)
      {
        // ignore
        have_packet = false;
      }
      else
      {
        filled_this_buffer = true;
        have_packet = true;
      }

      // close open data block buffer if is is now full
      if (bytes_this_buf >= data_bufsz || filled_this_buffer)
      {
  #ifdef _DEBUG
        cerr << "spip::UDPReceiveMergeDB::receive["<<p<<"] close_block bytes_this_buf="
             << bytes_this_buf << " data_bufsz=" << data_bufsz
             << " filled_this_buffer=" << filled_this_buffer << endl;
  #endif
        filled_this_buffer = true;
      }
    }
    pthread_mutex_lock (&mutex);
    full[p] = true;

    if (control_cmd == Stop)
    {
      control_states[p] = Idle;
    }
#ifdef _DEBUG
    cerr << "spip::UDPReceiveMergeDB::receive["<<p<<"] filled buffer " << ibuf << endl; 
    ibuf++;
#endif
    pthread_cond_broadcast (&cond);
    pthread_mutex_unlock (&mutex);
  }

  cerr << "spip::UDPReceiveMergeDB::receive["<<p<<"] exiting" << endl;
  if (control_states[p] == Idle)
    return true;
  else
    return false;
}
