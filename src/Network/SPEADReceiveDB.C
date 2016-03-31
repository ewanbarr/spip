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

  verbose = 1;
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

  if (verbose)
    cerr << "spip::SPEADReceiveDB::configure db->open()" << endl;
  db->open();

  if (verbose)
    cerr << "spip::SPEADReceiveDB::configure db->write_header()" << endl;
  db->write_header (header);
}

void spip::SPEADReceiveDB::prepare (std::string ip_address, int port)
{
  spead_ip = ip_address;
  spead_port = port;

/*
  // make a shared pool
  pool = std::make_shared<spead2::memory_pool>(16384, 26214400, 12, 8);

  //endpoint = endpoint (boost::asio::ip::address_v4::from_string(ip_address), port);
  endpoint = new boost::asio::ip::udp::endpoint (boost::asio::ip::address_v4::any(), port);
*/
}

void spip::SPEADReceiveDB::start_control_thread (int port)
{
  control_port = port;
  pthread_create (&control_thread_id, NULL, control_thread_wrapper, this);
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
  cerr << "spip::SPEADReceiveDB::open()" << endl;

  // open the data block for writing  
  db->open();

  // write the header
  db->write_header (header);

  cerr << "spip::SPEADReceiveDB::open db=" << (void *) db << endl;
}

void spip::SPEADReceiveDB::close ()
{
  if (verbose)
    cerr << "spip::SPEADReceiveDB::close()" << endl;
  if (db->is_block_open())
  {
    if (verbose)
      cerr << "spip::SPEADReceiveDB::close db->close_block(" << db->get_data_bufsz() << ")" << endl;
    db->close_block(db->get_data_bufsz());
  }

  // close the data block, ending the observation
  db->close();
}

// receive SPEAD heaps for the specified time at the specified data rate
bool spip::SPEADReceiveDB::receive ()
{
  if (verbose)
    cerr << "spip::SPEADReceiveDB::receive ()" << endl;

  // Bruce advised the lower limit should be the expected heap size
  // Upper limit should be  + 4K for headers etc
  const int lower = resolution;
  const int upper = resolution + 4096;

  spead2::thread_pool worker;
  std::shared_ptr<spead2::memory_pool> pool = std::make_shared<spead2::memory_pool>(lower, upper, 12, 8);
  spead2::recv::ring_stream<> stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2);
  stream.set_memory_pool(pool);
  boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address_v4::any(), spead_port);
  stream.emplace_reader<spead2::recv::udp_reader>( endpoint, spead2::recv::udp_reader::default_max_size, 128 * 1024 * 1024);

  control_state = Idle;
  keep_receiving = true;
  have_metadata = false;

  // read the meta-data from the spead stream
  while (keep_receiving && !have_metadata)
  {
    try
    {
#ifdef _DEBUG
      cerr << "spip::SPEADReceiveDB::receive waiting for meta-data heap" << endl;
#endif
      spead2::recv::heap fh = stream.pop();
#ifdef _DEBUG
      cerr << "spip::SPEADReceiveDB::receive received meta-data heap with ID=" << fh.get_cnt() << endl;
#endif

      const auto &items = fh.get_items();
      for (const auto &item : items)
      {
        if (item.id >= SPEAD_CBF_RAW_SAMPLES || item.id == SPEAD_CBF_RAW_TIMESTAMP)
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
      have_metadata = bf_config.valid();
    }

    catch (spead2::ringbuffer_stopped &e)
    {
      keep_receiving = false;
    }
  }

  bf_config.print_config();

  // block accounting 
  const unsigned bytes_per_heap = bf_config.get_bytes_per_heap();
  const unsigned samples_per_heap = bf_config.get_samples_per_heap();
  const uint64_t heaps_per_buf = db->get_data_bufsz() / bytes_per_heap;

  const double adc_to_bf_sampling_ratio = bf_config.get_adc_to_bf_sampling_ratio ();

  cerr << "spip::SPEADReceiveDB::receive bytes_per_heap=" << bytes_per_heap << endl;
  cerr << "spip::SPEADReceiveDB::receive heaps_per_buf=" << heaps_per_buf << endl;
  cerr << "spip::SPEADReceiveDB::receive adc_to_bf_sampling_ratio=" << adc_to_bf_sampling_ratio << endl;

  // get a heap of raw beam former data
  if (db->get_data_bufsz() % bytes_per_heap)
  {
    cerr << "spip::SPEADReceiveDB::receive there were not an integer number of heaps per buffer" << endl;
    return -1;
  }

  // block control logic
  char * block;
  bool need_next_block = false;

  uint64_t curr_heap = 0;
  uint64_t next_heap  = 0;
  uint64_t heaps_this_buf = 0;

#ifdef _DEBUG
  cerr << "spip::SPEADReceiveDB::receive db->get_data_bufsz()=" << db->get_data_bufsz() << endl;
  cerr << "spip::SPEADReceiveDB::receive data_size=" << data_size << endl;
  cerr << "spip::SPEADReceiveDB::receive heaps_per_buf=" << heaps_per_buf << endl;
#endif

  control_state = Active;

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
        if (verbose > 1)
          cerr << "spip::SPEADReceiveDB::receive db=" << (void *) db << " open_block()" << endl;
        block = (char *) (db->open_block());
        if (verbose > 1)
          cerr << "spip::SPEADReceiveDB::receive db block opened!" << endl;
        need_next_block = false;

        if (heaps_this_buf == 0 && next_heap > 0)
        {
          cerr << "spip::SPEADReceiveDB::receive received 0 heaps this buf" << endl;
          keep_receiving = false;
        }

        // number is first packet due in block to first packet of next block
        curr_heap = next_heap;
        next_heap = curr_heap + heaps_per_buf;

#ifdef _DEBUG
        cerr << "spip::SPEADReceiveDB::receive [" << curr_heap << " - " 
             << next_heap << "] (" << heaps_this_buf << ")" << endl;
#endif
        heaps_this_buf = 0;
      }

      try
      {
#ifdef _DEBUG
        cerr << "spip::SPEADReceiver::receive stream.pop()" << endl;
#endif

        spead2::recv::heap fh = stream.pop();

        const auto &items = fh.get_items();
        int raw_id = -1;
        timestamp = 0;
#define PARSE_ALL_ITEMS
#ifdef PARSE_ALL_ITEMS
        for (unsigned i=0; i<items.size(); i++)
        {
          if (items[i].id >= SPEAD_CBF_RAW_SAMPLES)
          {
            raw_id = i;
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
#else
        if (items[0].id == SPEAD_CBF_RAW_SAMPLES)
        {
          raw_id = 0;
          timestamp = SPEADBeamFormerConfig::item_ptr_48u (items[1].ptr);
        }
        else
        {
          cerr << "items.size()=" << items.size() << " items[0].id=" << std::hex << items[0].id << std::dec << endl;
        }
          //cerr << "raw_id=" << raw_id << " timestamp=" << timestamp << " start_adc_sample=" << start_adc_sample << endl;
#endif


        // if a starting ADC sample was not provided in the configuration
        if (start_adc_sample == -1 && timestamp > 0)
          start_adc_sample = timestamp;

        // if a RAW CBF heap has been received and is valid
        if (raw_id >= 0 && timestamp > 0 && timestamp >= start_adc_sample)
        {
          // the number of ADC samples since the start of this observation
          uint64_t adc_sample = (uint64_t) (timestamp - start_adc_sample);

          // TODO check the implications of non integer adc_to_bf ratios
          uint64_t bf_sample = adc_sample / adc_to_bf_sampling_ratio;
          uint64_t heap = bf_sample / samples_per_heap;

#ifdef _DEBUG
          cerr << "adc_sample=" <<adc_sample << " bf_sample=" << bf_sample << " heap=" << heap << " [" << curr_heap << " - " << next_heap << "]" << endl;
#endif

          // if this heap belongs in the current block
          if (heap >= curr_heap && heap < next_heap)
          {
            uint64_t byte_offset = (heap - curr_heap) * bytes_per_heap;
            memcpy (block + byte_offset, items[raw_id].ptr, items[raw_id].length);
            heaps_this_buf++;

            if (heap + 1 == next_heap)
              need_next_block = true;
          }
          else if (heap < curr_heap)
          {
            cerr << "ERROR: heap=" << heap << " curr_heap=" << curr_heap << endl;
          }
          else if (heap >= next_heap)
          {
            need_next_block = true;
            cerr << "WARN : heap=" << heap << " >= next_heap=" << next_heap << endl;
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
