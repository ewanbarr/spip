/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "config.h"

#ifdef HAVE_HWLOC
#include "spip/HardwareAffinity.h"
#endif

#include "spip/TCPSocketServer.h"
#include "spip/UDPReceiveMergeDB.h"
#include "spip/Time.h"

#include <cstring>
#include <stdexcept>
#include <new>

using namespace std;

spip::UDPReceiveMergeDB::UDPReceiveMergeDB (const char * key_string)
{
  db = new DataBlockWrite (key_string);

  db->connect();
  db->lock();
  db->page();

  control_port = -1;

  control_cmd = None;
  control_state = Idle;

  for (unsigned i=0; i<2; i++)
  {
    control_states[i] = Idle;

    pthread_cond_init( &(cond_recvs[i]), NULL);
    pthread_mutex_init( &(mutex_recvs[i]), NULL);

    data_mcasts[i] = string ();
    formats[i] = NULL;
    stats[i] = NULL;
  }

  pthread_cond_init( &cond_db, NULL);
  pthread_mutex_init( &mutex_db, NULL);

  chunk_size = 0;
  overflow = NULL;

  verbose = 1;
}

spip::UDPReceiveMergeDB::~UDPReceiveMergeDB()
{
#ifdef _DEBUG
  cerr << "spip::UDPReceiveMergeDB::~UDPReceiveMergeDB" << endl;
#endif

  for (unsigned i=0; i<2; i++)
  {
    delete stats[i];
    delete formats[i];
  }

  if (overflow)
    free (overflow);
  overflow = 0;

  db->unlock();
  db->disconnect();

  delete db;
}

int spip::UDPReceiveMergeDB::configure (const char * config_str)
{
  // save the config for use on the first open block
  config.load_from_str (config_str);

  if (config.get ("NCHAN", "%u", &nchan) != 1)
    throw invalid_argument ("NCHAN did not exist in config");

  if (config.get ("NBIT", "%u", &nbit) != 1)
    throw invalid_argument ("NBIT did not exist in config");

  if (config.get ("NPOL", "%u", &npol) != 1)
    throw invalid_argument ("NPOL did not exist in config");

  if (config.get ("NDIM", "%u", &ndim) != 1)
    throw invalid_argument ("NDIM did not exist in config");

  if (config.get ("TSAMP", "%f", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in config");

  if (config.get ("BW", "%f", &bw) != 1)
    throw invalid_argument ("BW did not exist in config");

  char * buffer = (char *) malloc (128);

  if (config.get ("DATA_HOST_0", "%s", buffer) != 1)
    throw invalid_argument ("DATA_HOST_0 did not exist in config");
  data_hosts[0] = string (buffer);
  if (config.get ("DATA_HOST_1", "%s", buffer) != 1)
    throw invalid_argument ("DATA_HOST_1 did not exist in config");
  data_hosts[1] = string (buffer);

  if (config.get ("DATA_PORT_0", "%d", &data_ports[0]) != 1)
    throw invalid_argument ("DATA_PORT_0 did not exist in config");
  if (config.get ("DATA_PORT_1", "%d", &data_ports[1]) != 1)
    throw invalid_argument ("DATA_PORT_1 did not exist in config");

  if (config.get ("DATA_MCAST_0", "%s", buffer) == 1)
    data_mcasts[0] = string (buffer);
  if (config.get ("DATA_MCAST_1", "%s", buffer) == 1)
    data_mcasts[1] = string (buffer);

  free (buffer);

  bits_per_second  = (unsigned) ((nchan * npol * ndim * nbit * 1000000) / tsamp);
  bytes_per_second = bits_per_second / 8;

  formats[0]->configure(config, "_0");
  formats[1]->configure(config, "_1");

  npol = 2;
  if (config.set("NPOL", "%u", npol) < 0)
    throw invalid_argument ("failed to write NPOL to config"); 

  // get the resolution from the two formats
  chunk_size = formats[0]->get_resolution() + formats[1]->get_resolution();
  overflow = (char *) malloc (chunk_size);

  for (unsigned i=0; i<2; i++)
  {
    stats[i] = new UDPStats (formats[i]->get_header_size(), formats[i]->get_data_size());
  }
  return 0;
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
  void * result;
  pthread_join (control_thread_id, &result);
}

void spip::UDPReceiveMergeDB::set_control_cmd (spip::ControlCmd cmd)
{
  pthread_mutex_lock (&mutex_db);
  control_cmd = cmd;
  pthread_cond_signal (&cond_db);
  pthread_mutex_unlock (&mutex_db);
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

#ifdef _DEBUG
  cerr << "spip::UDPReceiveMergeDB::control_thread creating TCPSocketServer" << endl;
#endif
  spip::TCPSocketServer * control_sock = new spip::TCPSocketServer();

  // open a listen sock on all interfaces for the control port
  if (verbose)
  cerr << "opened control socket on port=" << control_port << endl;
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

      if (verbose > 1)
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
        header.clone (config);
        header.append_from_str (cmds);
        if (header.del ("COMMAND") < 0)
          throw runtime_error ("Could not remove COMMAND from header");

        if (verbose)
          cerr << "control_thread: open()" << endl;
        open ();

        if (verbose)
          cerr << "control_thread: control_cmd = Start" << endl;
        set_control_cmd (Start);
      }
      else if (strcmp (cmd, "STOP") == 0)
      {
        set_control_cmd (Stop);
      }
      else if (strcmp (cmd, "QUIT") == 0)
      {
        set_control_cmd (Quit);
      }
    }
  }
#ifdef _DEBUG
  cerr << "spip::UDPReceiveMergeDB::control_thread exiting" << endl;
#endif
}

bool spip::UDPReceiveMergeDB::open ()
{
  if (verbose > 1)
    cerr << "spip::UDPReceiveMergeDB::open()" << endl;

  if (control_cmd == Stop)
  {
    return false;
  } 

  if (header.get_header_length() == 0)
    header.clone(config);
 
  // check if UTC_START has been set
  char * buffer = (char *) malloc (128);
  if (header.get ("UTC_START", "%s", buffer) == -1)
  {
    time_t now = time(0);
    spip::Time utc_start (now);
    utc_start.add_seconds (2);
    std::string utc_str = utc_start.get_gmtime();
    cerr << "Generated UTC_START=" << utc_str  << endl;
    if (header.set ("UTC_START", "%s", utc_str.c_str()) < 0)
      throw invalid_argument ("failed to write UTC_START to header");
  }
  else
    cerr << "spip::UDPReceiveMergeDB::open UTC_START=" << buffer << endl;

  uint64_t obs_offset;
  if (header.get("OBS_OFFSET", "%lu", &obs_offset) == -1)
  {
    obs_offset = 0;
    if (header.set ("OBS_OFFSET", "%lu", obs_offset) < 0)
      throw invalid_argument ("failed to write OBS_OFFSET=0 to header");
  }

  if (header.get ("SOURCE", "%s", buffer) == -1)
    throw invalid_argument ("no SOURCE specified in header");

  if (verbose > 1)
    cerr << "spip::UDPReceiveMergeDB::open preparing formats" << endl;
  formats[0]->prepare (header, "_0");
  formats[1]->prepare (header, "_1");

  free (buffer);
  open (header.raw());
  return true;
}

// write the ascii header to the datablock, then
void spip::UDPReceiveMergeDB::open (const char * header_str)
{
  // open the data block for writing  
  db->open();

  // write the header
  db->write_header (header_str);
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

  header.reset();
}

void spip::UDPReceiveMergeDB::start_threads (int c1, int c2)
{
  // cpu cores on which to bind each recv thread
  cores[0] = c1;
  cores[1] = c2;

  // flag for whether the recv thread has filled the current buffer
  full[0] = true;
  full[1] = true;

  control_state = Idle;
  control_states[0] = Idle;
  control_states[1] = Idle;

  formats[0]->reset();
  formats[1]->reset();

  stats[0]->reset();
  stats[1]->reset();

  pthread_create (&datablock_thread_id, NULL, datablock_thread_wrapper, this);
  pthread_create (&recv_thread1_id, NULL, recv_thread1_wrapper, this);
  pthread_create (&recv_thread2_id, NULL, recv_thread2_wrapper, this);
  pthread_create (&stats_thread_id, NULL, stats_thread_wrapper, this);
}

void spip::UDPReceiveMergeDB::join_threads ()
{
  void * result;
  pthread_join (datablock_thread_id, &result);
  pthread_join (recv_thread1_id, &result);
  pthread_join (recv_thread2_id, &result);
  pthread_join (stats_thread_id, &result);
}

bool spip::UDPReceiveMergeDB::datablock_thread ()
{
  pthread_mutex_lock (&mutex_db);
  pthread_mutex_lock (&(mutex_recvs[0]));
  pthread_mutex_lock (&(mutex_recvs[1]));

  uint64_t ibuf = 0;
  int64_t overflow_lastbyte;

  // wait for the starting command from the control_thread
  while (control_cmd == None)
    pthread_cond_wait (&cond_db, &mutex_db);

  // if we have a start command then we can continue
  if (control_cmd == Start)
  {
   // open the data block for writing
    block = (char *) (db->open_block());

    // state of this thread
    control_state = Active;

    full[0] = full[1] = false;
    overflow_lastbytes[0] = overflow_lastbytes[1] = 0;

#ifdef _DEBUG
    cerr << "spip::UDPReceiveMergeDB::datablock opened buffer " << ibuf << endl;
#endif
  }
  else if (control_cmd == Stop || control_cmd == Quit)
  {
    control_state = Stopping;
#ifdef _DEBUG
    cerr << "spip::UDPReceiveMergeDB::datablock_thread received "
         <<  "a Stop command prior to starting" << endl;
#endif
  }
  else
  {
    throw invalid_argument ("datathread encounter an unexpected control_cmd");
  }

  pthread_mutex_unlock (&mutex_db);

  // signal receive threads to wake up and inspect control_state
  control_states[0] = control_states[1] = control_state;
  pthread_cond_signal (&(cond_recvs[0]));
  pthread_cond_signal (&(cond_recvs[1]));
  pthread_mutex_unlock (&(mutex_recvs[0]));
  pthread_mutex_unlock (&(mutex_recvs[1]));

#ifdef _DEBUG
  cerr << "spip::UDPReceiveMergeDB::datablock signaled recv " << ibuf << endl;
#endif

  // while the receiving state is Active
  while (control_state == Active)
  {
    // zero the next buffer that DB would provide
    //db->zero_next_block();

    // wait for RECV threads to fill buffer
    pthread_mutex_lock (&mutex_db);

    // if the current buffer has been filled  by both receive threads
    while (!full[0] || !full[1])
    {
#ifdef _DEBUG
      cerr << "spip::UDPReceiveMergeDB::datablock checking buffer " << ibuf 
           << " [" << full[0] << "," << full[1] << "]" << endl;
#endif
      pthread_cond_wait (&cond_db, &mutex_db);
    }
    
#ifdef _DEBUG
    cerr << "spip::UDPReceiveMergeDB::datablock filled buffer " << ibuf << endl;
#endif
    
    // close data block
    db->close_block(db->get_data_bufsz());

    // acquire both recv threads' mutexes
    pthread_mutex_lock (&(mutex_recvs[0]));
    pthread_mutex_lock (&(mutex_recvs[1]));

    ibuf++;

    // check for state changes
    if (control_cmd == Stop || control_cmd == Quit)
    {
      cerr << "STATE=Idle" << endl;
      control_state = Idle;
      control_states[0] = control_states[1] = control_state;
    }
    else
    {
      block = (char *) (db->open_block());
      full[0] = full[1] = false;
     
      // TODO make this a linked list!

      // copy any overflowed data into place
      overflow_lastbyte = std::max (overflow_lastbytes[0], overflow_lastbytes[1]);
      if (overflow_lastbyte > 0)
      {
#ifdef _DEBUG
        cerr << "spip::UDPReceiveMergeDB::data_block overflow saved " << overflow_lastbyte << " bytes" << endl;
#endif
        memcpy (block, overflow, overflow_lastbyte);
      }
    }

    // release the DB mutex now that we have the recv threads locked
    pthread_mutex_unlock (&mutex_db);
    pthread_cond_signal (&(cond_recvs[0]));
    pthread_cond_signal (&(cond_recvs[1]));
    pthread_mutex_unlock (&(mutex_recvs[0]));
    pthread_mutex_unlock (&(mutex_recvs[1]));
  }

  close ();

#ifdef _DEBUG
  cerr << "spip::UDPReceiveMergeDB::data_block exiting" << endl;
#endif

  return true;
}

bool spip::UDPReceiveMergeDB::receive_thread (int p)
{
#ifdef HAVE_HWLOC
  spip::HardwareAffinity hw_affinity;
  hw_affinity.bind_thread_to_cpu_core (cores[p]);
  hw_affinity.bind_to_memory (cores[p]);
#endif

#ifdef HAVE_VMA
  struct vma_api_t * vma_api = vma_get_api();
  struct vma_packets_t* pkt = NULL;
  if (!vma_api)
  {
    cerr << "WARNING: VMA support compiled, but VMA not available" << endl;
  }
#else
  char vma_api = 0;
#endif

  // allocated and configured in main threasd
  UDPFormat * format = formats[p];
  UDPStats * stat = stats[p];

  // open socket within the context of this thread 
  UDPSocketReceive * sock = new UDPSocketReceive;
  if (data_mcasts[p].size() > 0)
    sock->open_multicast (data_hosts[p], data_mcasts[p], data_ports[p]);
  else
    sock->open (data_hosts[p], data_ports[p]); 

  if (!vma_api)
    sock->set_nonblock ();

  size_t sock_bufsz = format->get_header_size() + format->get_data_size();
  sock->resize (sock_bufsz);
  sock->resize_kernel_buffer (64*1024*1024);

  pthread_cond_t * cond_recv = &(cond_recvs[p]);
  pthread_mutex_t * mutex_recv = &(mutex_recvs[p]);

  bool keep_receiving = true;
  bool have_packet = false;
  bool obs_started = false;

  struct sockaddr_in client_addr;
  struct sockaddr * addr = (struct sockaddr *) &client_addr;
  socklen_t addr_size = sizeof(struct sockaddr);

  int fd = sock->get_fd();
  char * buf = sock->get_buf();
  char * buf_ptr = buf;

  // block accounting 
  const int64_t data_bufsz = db->get_data_bufsz();
  const int64_t pol_bufsz = data_bufsz / 2;
  int64_t curr_byte_offset;
  int64_t next_byte_offset = 0;

  // overflow buffer
  const int64_t overflow_bufsz = chunk_size;
  int64_t overflow_maxbyte = 0;
  int64_t overflowed_bytes = 0;

  uint64_t bytes_this_buf = 0;
  int64_t byte_offset;
  uint64_t pol_offset = p * (chunk_size / 2);

  bool filled_this_buffer = false;
  unsigned bytes_received, bytes_dropped;
  int flags, got;
  uint64_t nsleeps;
  uint64_t ibuf = 0;

  // wait for datablock thread to change state to Active
  pthread_mutex_lock (mutex_recv);

  // wait for start command
  while (control_states[p] == Idle)
    pthread_cond_wait (cond_recv, mutex_recv);
#ifdef _DEBUG
  cerr << "spip::UDPReceiveMergeDB::receive["<<p<<"] control_states[" << p << "] now != Idle" << endl;
#endif

  pthread_mutex_unlock (mutex_recv);

  // main data acquisition loop
  while (control_states[p] == Active)
  {
    // wait until the datablock thread sets the state of this buffer
    // to empty (i.e. when it has provided a new buffer to fill
    pthread_mutex_lock (mutex_recv);

    while (full[p] && control_states[p] == Active)
    {
#ifdef _DEBUG
      cerr << "spip::UDPReceiveMergeDB::receive["<<p<<"] waiting for empty " << ibuf << " [" << full[0] << ", " << full[1] << "]" << endl;
#endif
      pthread_cond_wait (cond_recv, mutex_recv);
    }

    if (control_states[p] == Active)
    {
      curr_byte_offset = next_byte_offset;
      next_byte_offset += data_bufsz;
      overflow_maxbyte = next_byte_offset + overflow_bufsz;

#ifdef _DEBUG
      if (p == 1)
        cerr << "spip::UDPReceiveMergeDB::receive["<<p<<"] filling buffer " 
             << ibuf << " [" <<  curr_byte_offset << " - " << next_byte_offset
             << " - " << overflow_maxbyte << "] overflow=" << overflow_lastbytes[p] 
             << " overflow_bufsz=" << overflow_bufsz << endl;
#endif

      // signal other threads waiting on the condition
      pthread_mutex_unlock (mutex_recv);
      filled_this_buffer = false;
      
      // overflow handler
      if (overflow_lastbytes[p] > 0)
      {
        overflow_lastbytes[p] = 0;
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
            if (control_cmd == Stop || control_cmd == Quit)
              keep_receiving = false;
            flags = 0;
            got = (int) vma_api->recvfrom_zcopy(fd, buf, sock_bufsz, &flags, addr, &addr_size);
            if (got  > 32)
            {
              if (flags == MSG_VMA_ZCOPY)
              {
                pkt = (vma_packets_t*) buf;
                buf_ptr = (char *) pkt->pkts[0].iov[0].iov_base;
              }
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
              cerr << "spip::UDPReceiveMergeDB::receive error expected " << sock_bufsz
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
              cerr << "spip::UDPReceiveMergeDB::receive error expected " << sock_bufsz
                   << " B, received " << got << " B" <<  endl;
              set_control_cmd (Stop);
              keep_receiving = false;
            }
          }
        }

        byte_offset = format->decode_packet (buf_ptr, &bytes_received);

        if (byte_offset < 0)
        {
          // ignore if byte_offset is -ve
          have_packet = false;

          // data stream is finished
          if (byte_offset == -2)
          {
            set_control_cmd(Stop);
            keep_receiving = false;
          } 
        }
        // packet that is part of this observation
        else
        {
          // adjust byte offset from single pol to dual pol
          byte_offset += pol_offset;

          // packet belongs in current buffer
          if ((byte_offset >= curr_byte_offset) && (byte_offset < next_byte_offset))
          {
            bytes_this_buf += bytes_received;
            stat->increment_bytes (bytes_received);
            format->insert_last_packet (block + (byte_offset - curr_byte_offset));
            have_packet = false;
          }
          // packet fits in the overflow buffer
          else if ((byte_offset >= next_byte_offset) && (byte_offset < overflow_maxbyte))
          {
            format->insert_last_packet (overflow + (byte_offset - next_byte_offset));

            overflow_lastbytes[p] = std::max((byte_offset - next_byte_offset) + bytes_received, overflow_lastbytes[p]);
            overflowed_bytes += bytes_received;
            have_packet = false;
          }
          // packet belong to a previous buffer (this is a drop that has already been counted)
          else if (byte_offset < curr_byte_offset)
          {
            have_packet = false;
          }
          // packet belongs to a future buffer, that is beyond the overflowr
          else
          {
            filled_this_buffer = true;
            have_packet = true;
          }
        }

        // close open data block buffer if is is now full
        if (bytes_this_buf >= pol_bufsz || filled_this_buffer)
        {
#ifdef _DEBUG
          if (p == 1)
            cerr << "spip::UDPReceiveMergeDB::receive["<<p<<"] close_block "
                 << " bytes_this_buf=" << bytes_this_buf 
                 << " pol_bufsz=" << pol_bufsz 
                 << " overflow_lastbytes=" << overflow_lastbytes[p]
                 << " filled_this_buffer=" << filled_this_buffer << endl;
#endif
          stat->dropped_bytes (pol_bufsz - bytes_this_buf);
          filled_this_buffer = true;
        }
      }

      pthread_mutex_lock (&mutex_db);
      full[p] = true;

  #ifdef _DEBUG
      cerr << "spip::UDPReceiveMergeDB::receive["<<p<<"] filled buffer " << ibuf << endl; 
  #endif
      ibuf++;
      pthread_cond_signal (&cond_db);
      pthread_mutex_unlock (&mutex_db);
    }
    else
      pthread_mutex_unlock (mutex_recv);
  }

#ifdef _DEBUG
  cerr << "spip::UDPReceiveMergeDB::receive["<<p<<"] exiting" << endl;
#endif

  delete sock;

  if (control_states[p] == Idle)
    return true;
  else
    return false;
}

/* 
 *  Thread to print simple capture statistics
 */
void spip::UDPReceiveMergeDB::stats_thread()
{
  uint64_t b_recv_total[2] = {0, 0};
  uint64_t b_recv_curr[2];
  uint64_t b_recv_1sec;

  uint64_t s_total[2] = {0, 0};
  uint64_t s_curr[2];
  uint64_t s_1sec;

  uint64_t b_drop_total[2] = {0,0};
  uint64_t b_drop_curr[2];
  uint64_t b_drop_1sec;

  float gb_recv_ps[2] = {0, 0};
  float mb_recv_ps[2] = {0, 0};
  float gb_drop_ps[2] = {0, 0};

#ifdef _DEBUG
  cerr << "spip::UDPReceiveMergeDB::stats_thread starting polling" << endl;
#endif

  while (control_cmd != Stop && control_cmd != Quit)
  {
    while (control_state == Active)
    {
      for (unsigned i=0; i<2; i++)
      {
        // get a snapshot of the data as quickly as possible
        b_recv_curr[i] = stats[i]->get_data_transmitted();
        b_drop_curr[i] = stats[i]->get_data_dropped();
        s_curr[i] = stats[i]->get_nsleeps();

        // calc the values for the last second
        b_drop_1sec = b_drop_curr[i] - b_drop_total[i];
        b_recv_1sec = b_recv_curr[i] - b_recv_total[i];
        s_1sec = s_curr[i] - s_total[i];

        // update the totals
        b_drop_total[i] = b_drop_curr[i];
        b_recv_total[i] = b_recv_curr[i];
        s_total[i] = s_curr[i];

        gb_drop_ps[i] = (double) (b_drop_1sec * 8) / 1000000000;
        mb_recv_ps[i] = (double) b_recv_1sec / 1000000;
        gb_recv_ps[i] = (mb_recv_ps[i] * 8)/1000;
      }

      // determine how much memory is free in the receivers
      fprintf (stderr,"Recv %6.3f (%6.3f, %6.3f) [Gb/s] Dropped %6.3f (%6.3f + %6.3f) [Gb/s]\n", 
               gb_recv_ps[0] + gb_recv_ps[1], gb_recv_ps[0], gb_recv_ps[1],
               gb_drop_ps[0] + gb_drop_ps[1], gb_drop_ps[0], gb_drop_ps[1]);
      sleep (1);
    }
    sleep(1);
  }
#ifdef _DEBUG
  cerr << "spip::UDPReceiveMergeDB::stats_thread exiting";
#endif
}

