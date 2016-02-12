/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/HardwareAffinity.h"
#include "spip/TCPSocketServer.h"
#include "spip/SPEADReceiveMergeDB.h"
#include "sys/time.h"

#include "ascii_header.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <new>

#define SPEADReceiveMergeDB_CMD_NONE 0
#define SPEADReceiveMergeDB_CMD_START 1
#define SPEADReceiveMergeDB_CMD_STOP 2
#define SPEADReceiveMergeDB_CMD_QUIT 3

using namespace std;

spip::SPEADReceiveMergeDB::SPEADReceiveMergeDB (const char * key_string)
{
  db = new DataBlockWrite (key_string);

  db->connect();
  db->lock();

  control_port = -1;
  header = (char *) malloc (DADA_DEFAULT_HEADER_SIZE);

  control_cmd = None;
  control_state = Idle;

  cond = PTHREAD_COND_INITIALIZER;
  mutex = PTHREAD_MUTEX_INITIALIZER;

  pthread_cond_init( &cond, NULL);
  pthread_mutex_init( &mutex, NULL);

  verbose = 1;
}

spip::SPEADReceiveMergeDB::~SPEADReceiveMergeDB()
{
  db->unlock();
  db->disconnect();

  delete db;
}

int spip::SPEADReceiveMergeDB::configure (const char * config)
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

  // the RESOLUTION should match the heap size exactly
  if (ascii_header_get (config, "RESOLUTION", "%lu", &resolution) != 1)
    throw invalid_argument ("RESOLUTION did not exist in header");

  if (ascii_header_get (config, "START_ADC_SAMPLE", "%ld", &start_adc_sample) != 1)
    start_adc_sample = -1;  

  channel_bw = bw / nchan;

  bits_per_second  = (nchan * npol * ndim * nbit * 1000000) / tsamp;
  bytes_per_second = bits_per_second / 8;

  unsigned start_chan, end_chan;
  if (ascii_header_get (config, "START_CHANNEL", "%u", &start_chan) != 1)
    throw invalid_argument ("START_CHANNEL did not exist in header");
  if (ascii_header_get (config, "END_CHANNEL", "%u", &end_chan) != 1)
    throw invalid_argument ("END_CHANNEL did not exist in header");

  // save the header for use on the first open block
  strncpy (header, config, strlen(config)+1);

  return 0;
}

void spip::SPEADReceiveMergeDB::prepare (std::string ip_address1, int port1, std::string ip_address2, int port2)
{
  spead_ips[0] = ip_address1;
  spead_ips[1] = ip_address2;

  spead_ports[0] = port1;
  spead_ports[1] = port2;
}

void spip::SPEADReceiveMergeDB::start_control_thread (int port)
{
  control_port = port;
  pthread_create (&control_thread_id, NULL, control_thread_wrapper, this);
}

void spip::SPEADReceiveMergeDB::stop_control_thread ()
{
  control_cmd = Quit;
}

// start a control thread that will receive commands from the TCS/LMC
void spip::SPEADReceiveMergeDB::control_thread()
{
#ifdef _DEBUG
  cerr << "spip::SPEADReceiveMergeDB::control_thread starting" << endl;
#endif

  if (control_port < 0)
  {
    cerr << "ERROR: no control port specified" << endl;
    return;
  }

  cerr << "spip::SPEADReceiveMergeDB::control_thread creating TCPSocketServer" << endl;
  spip::TCPSocketServer * control_sock = new spip::TCPSocketServer();

  // open a listen sock on all interfaces for the control port
  cerr << "spip::SPEADReceiveMergeDB::control_thread open socket on port=" 
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
  cerr << "spip::SPEADReceiveMergeDB::control_thread exiting" << endl;
#endif
}

void spip::SPEADReceiveMergeDB::open ()
{
  open (header);
}

// write the ascii header to the datablock, then
void spip::SPEADReceiveMergeDB::open (const char * header)
{
  cerr << "spip::SPEADReceiveMergeDB::open()" << endl;

  // open the data block for writing  
  db->open();

  // write the header
  db->write_header (header);

  cerr << "spip::SPEADReceiveMergeDB::open db=" << (void *) db << endl;
}

void spip::SPEADReceiveMergeDB::close ()
{
  if (verbose)
    cerr << "spip::SPEADReceiveMergeDB::close()" << endl;
  if (db->is_block_open())
  {
    if (verbose)
      cerr << "spip::SPEADReceiveMergeDB::close db->close_block(" << db->get_data_bufsz() << ")" << endl;
    db->close_block(db->get_data_bufsz());
  }

  // close the data block, ending the observation
  db->close();
}

void spip::SPEADReceiveMergeDB::start_threads (int c1, int c2)
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

void spip::SPEADReceiveMergeDB::join_threads ()
{
  void * result;
  pthread_join (datablock_thread_id, &result);
  pthread_join (recv_thread1_id, &result);
  pthread_join (recv_thread2_id, &result);
}

bool spip::SPEADReceiveMergeDB::datablock_thread ()
{
  pthread_mutex_lock (&mutex);

  // open the data block for writing
  block = (char *) (db->open_block());

  uint64_t heaps_per_buf = db->get_data_bufsz() / resolution;

  // setup the heap ranges for the first block
  curr_heap = 0;
  next_heap = heaps_per_buf;

  // wait for the starting command from the control_thread
  while (control_cmd == None)
    pthread_cond_wait (&cond, &mutex);

  // if we have a start command then we can continue
  if (control_cmd == Start)
  {
    control_state = Active;
    // single receive threads to commence
    pthread_cond_broadcast (&cond);
    pthread_mutex_unlock(&mutex);
  }
  else if (control_cmd == Stop)
  {
    cerr << "spip::SPEADReceiveMergeDB::datablock_thread received "
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
    // if the current buffer has been filled  by both receive threads
    while (!full[0] && !full[1])
      pthread_cond_wait (&cond, &mutex);
    // when this returns we have a lock on mutex
    
    // close data block
    db->close_block(db->get_data_bufsz());

    // if the control thread is asking us to stop Acquisition
    if (control_cmd == Stop)
    {
      cerr << "control_state = Idle" << endl;
      control_state = Idle;
    }
    else
    {
      block = (char *) (db->open_block());
      curr_heap = next_heap;
      next_heap = curr_heap + heaps_per_buf;

      full[0] = full[1] = false;
    }
  }

  cerr << "spip::SPEADReceiveMergeDB::datablock_thread pthread_unlock (mutex)" << endl;
  pthread_mutex_unlock (&mutex);

  cerr << "spip::SPEADReceiveMergeDB::datablock_thread pthread_exit()" << endl;
  pthread_exit (NULL);
}

// receive SPEAD heaps for the specified time at the specified data rate
bool spip::SPEADReceiveMergeDB::receive_thread (int p)
{
  if (verbose)
    cerr << "spip::SPEADReceiveMergeDB::receive[" << p << "] ()" << endl;

  spip::HardwareAffinity hw_affinity;
  cerr << "spip::SPEADReceiveMergeDB::receive["<<p<<"] binding to core " << cores[p] << endl;
  hw_affinity.bind_thread_to_cpu_core (cores[p]);
  hw_affinity.bind_to_memory (cores[p]);

  // Bruce advised the lower limit should be the expected heap size
  // Upper limit should be  + 4K for headers etc
  const int lower = (resolution / npol);
  const int upper = (resolution / npol) + 4096;

  spead2::thread_pool worker;
  std::shared_ptr<spead2::memory_pool> pool = std::make_shared<spead2::memory_pool>(lower, upper, 12, 8);
  spead2::recv::ring_stream<> stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2);
  stream.set_memory_pool(pool);
  boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address_v4::any(), spead_ports[p]);
  stream.emplace_reader<spead2::recv::udp_reader>( endpoint, spead2::recv::udp_reader::default_max_size, 64 * 1024 * 1024);

  bool keep_receiving = true;
  bool have_metadata = false;

  cerr << "receive_thread["<<p<<"] waiting for meta-data" << endl;

  // read the meta-data from the spead stream
  while (keep_receiving && !have_metadata)
  {
    try
    {
#ifdef _DEBUG
      cerr << "spip::SPEADReceiveMergeDB::receive waiting for meta-data heap" << endl;
#endif
      spead2::recv::heap fh = stream.pop();
#ifdef _DEBUG
      cerr << "spip::SPEADReceiveMergeDB::receive received meta-data heap with ID=" << fh.get_cnt() << endl;
#endif

      const auto &items = fh.get_items();
      for (const auto &item : items)
      {
        if (item.id == SPEAD_CBF_RAW_SAMPLES || item.id == SPEAD_CBF_RAW_TIMESTAMP)
        {
          // just ignore raw CBF packets until header is received
        }
        else
        {
          bf_config.parse_item (item);
        }
      }

      vector<spead2::descriptor> descriptors = fh.get_descriptors();
      for (const auto &descriptor : descriptors)
      {
        bf_config.parse_descriptor (descriptor);
      }
      bf_config.print_config();
      have_metadata = bf_config.valid();
    }

    catch (spead2::ringbuffer_stopped &e)
    {
      keep_receiving = false;
    }
  }

  cerr << "receive_thread["<<p<<"] have meta-data" << endl;

  // block accounting 
  const unsigned bytes_per_heap = bf_config.get_bytes_per_heap();
  const unsigned samples_per_heap = bf_config.get_samples_per_heap();
  const uint64_t heaps_per_buf = db->get_data_bufsz() / bytes_per_heap;
  const double adc_to_bf_sampling_ratio = bf_config.get_adc_to_bf_sampling_ratio ();

  if (bytes_per_heap != resolution / npol)
  {
    cerr << "receive_thread["<<p<<"] bytes_per_heap != resolution/npol" << endl;
    // TODO make this an error condition
  }

  cerr << "receive_thread["<<p<<"] bytes_per_heap=" << bytes_per_heap << endl;
  cerr << "receive_thread["<<p<<"] heaps_per_buf=" << heaps_per_buf << endl;
  cerr << "receive_thread["<<p<<"] adc_to_bf_sampling_ratio=" << adc_to_bf_sampling_ratio << endl;

  // get a heap of raw beam former data
  if (db->get_data_bufsz() % bytes_per_heap)
  {
    cerr << "spip::SPEADReceiveMergeDB::receive there were not an integer number of heaps per buffer" << endl;
    return -1;
  }

  // block control logic
  uint64_t heaps_this_buf;

#ifdef _DEBUG
  cerr << "receive_thread["<<p<<"] db->get_data_bufsz()=" << db->get_data_bufsz() << endl;
  cerr << "receive_thread["<<p<<"] data_size=" << data_size << endl;
  //cerr << "receive_thread["<<p<<"] heaps_per_buf=" << heaps_per_buf << endl;
#endif

  // wait for datablock thread to change state to Active
  pthread_mutex_lock (&mutex);
  while (control_state == Idle)
    pthread_cond_wait (&cond, &mutex);
  pthread_mutex_unlock (&mutex);

  // now we are within the main loop
  while (control_state == Active)
  {
    // wait until the datablock thread sets the state of this buffer
    // to empty (i.e. when it has provided a new buffer to fill
    pthread_mutex_lock (&mutex);
    while (full[p])
      pthread_cond_wait (&cond, &mutex);
    pthread_mutex_unlock (&mutex);

    heaps_this_buf = 0;

    // while we have not filled this buffer with data from
    // this polarisation
    while (!full[p])
    {
      try
      {
        spead2::recv::heap fh = stream.pop();

        const auto &items = fh.get_items();
        int raw_id = -1;
        timestamp = -1;

        for (unsigned i=0; i<items.size(); i++)
        {
          if (items[i].id == SPEAD_CBF_RAW_SAMPLES)
          {
            raw_id = i;
            timestamp = fh.get_cnt();
          }
          else if (items[i].id == SPEAD_CBF_RAW_TIMESTAMP)
          {
            timestamp = SPEADBeamFormerConfig::item_ptr_48u (items[i].ptr);
          }
          else if (items[i].id == 0xe8)
          {
            ;
          }
          else
          {
            // for now ignore all non CBF RAW heaps after the start
          }
        }
      
        //cerr << "raw_id=" << raw_id << " timestamp=" << timestamp << " start_adc_sample=" << start_adc_sample << endl;

        // if a starting ADC sample was provided not provided in the configuration
        if (start_adc_sample == -1)
          start_adc_sample = timestamp;

        // if a RAW CBF heap has been received and is valid
        if (raw_id >= 0 && timestamp >= 0 && timestamp >= start_adc_sample)
        {
          // the number of ADC samples since the start of this observation
          uint64_t adc_sample = (uint64_t) (timestamp - start_adc_sample);

          // TODO check the implications of non integer adc_to_bf ratios
          uint64_t bf_sample = adc_sample / adc_to_bf_sampling_ratio;
          uint64_t heap = bf_sample / samples_per_heap;

          //cerr << "adc_sample=" <<adc_sample << " bf_sample=" << bf_sample << " heap=" << heap << " [" << curr_heap << " - " << next_heap << "]" << endl;

          // if this heap belongs in the current block
          if (heap >= curr_heap && heap < next_heap)
          {
            uint64_t byte_offset = (heap - curr_heap) * bytes_per_heap;
            memcpy (block + byte_offset, items[raw_id].ptr, items[raw_id].length);
            heaps_this_buf++;
          }
          else if (heap < curr_heap)
          {
            cerr << "ERROR: heap=" << heap << " curr_heap=" << curr_heap << endl;
          }
          else if (heap >= next_heap)
          {
            full[p] = true;
            cerr << "WARN : heap=" << heap << " next_heap=" << next_heap << endl;
            // TODO we should keep this heap if possible
          }
          else
            cerr << "WARNING else case!" << endl;
        }
      }
      catch (spead2::ringbuffer_stopped &e)
      {
        cerr << "ERROR: spead2::ringbuffer_stopped exception" << endl;
        keep_receiving = false;
      }

      // close open data block buffer if is is now full
      if (heaps_this_buf == heaps_per_buf || full[p])
      {
#ifdef _DEBUG
        cerr << "spip::SPEADReceiveMergeDB::receive close_block heaps_this_buf=" 
             << heaps_this_buf << " heaps_per_buf=" << heaps_per_buf 
             << " full["<<p<<"]=" << full[p] << endl;
#endif
        full[p] = true;
      }
    }
  }

  cerr << "spip::SPEADReceiveMergeDB::receive["<<p<<"] exiting" << endl;
  if (control_state == Idle)
    return true;
  else
    return false;
}
