/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/SPEADReceiverMerge.h"
#include "spip/HardwareAffinity.h"

#include "sys/time.h"

#include "spead2/recv_udp.h"
#include "spead2/recv_live_heap.h"
#include "spead2/recv_ring_stream.h"
#include "spead2/common_endian.h"

#include "ascii_header.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <new>
#include <pthread.h>

//#define _DEBUG 1

using namespace std;

spip::SPEADReceiverMerge::SPEADReceiverMerge()
{
  verbose = 0;
}

spip::SPEADReceiverMerge::~SPEADReceiverMerge()
{
}

int spip::SPEADReceiverMerge::configure (const char * config)
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

  // TODO parameterize this
  heap_size = 262144;
}

void spip::SPEADReceiverMerge::prepare (std::string ip_address, int port1, int port2)
{
  spead_ip = ip_address;
  spead_port1 = port1;
  spead_port2 = port2;
}

int spip::SPEADReceiverMerge::open ()
{
  cerr << "spip::SPEADReceiverMerge::open()" << endl;
  return 0;
}

void spip::SPEADReceiverMerge::close ()
{
  cerr << "spip::SPEADReceiverMerge::close()" << endl;
}

void spip::SPEADReceiverMerge::start_recv_threads (int c1, int c2)
{
  core1 = c1;
  core2 = c2;
  pthread_create (&recv_thread1_id, NULL, recv_thread1_wrapper, this);
  usleep (10000);
  pthread_create (&recv_thread2_id, NULL, recv_thread2_wrapper, this);
}

void spip::SPEADReceiverMerge::join_recv_threads ()
{
  void * result;
  pthread_join (recv_thread1_id, &result);
  pthread_join (recv_thread2_id, &result);
}

// receive SPEAD heaps for the specified time at the specified data rate
bool spip::SPEADReceiverMerge::receive_thread (int pol)
{
  cerr << "spip::SPEADReceiverMerge::receive["<<pol<<"] ()" << endl;

  int spead_port;
  int core;
  if (pol == 1)
  {
    spead_port = spead_port1;
    core = core1;
  }
  else if (pol == 2)
  {
    core = core2;
    spead_port = spead_port2;
  }
  else
    throw invalid_argument ("receive_thread pol arg must be 1 or 2");

  spip::HardwareAffinity hw_affinity;
  cerr << "spip::SPEADReceiverMerge::receive["<<pol<<"] binding to core " << core << endl;
  hw_affinity.bind_thread_to_cpu_core (core);
  hw_affinity.bind_to_memory (core);

  uint64_t total_bytes_recvd = 0;
  bool obs_started = false;

  int lower = (resolution / npol);
  int upper = (resolution / npol) + 4096;

  spead2::thread_pool worker;
  std::shared_ptr<spead2::memory_pool> pool = std::make_shared<spead2::memory_pool>(lower, upper, 12, 8);
  spead2::recv::ring_stream<> stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2);
  stream.set_memory_pool(pool);
  boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address_v4::any(), spead_port);
  stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 128 * 1024 * 1024);

  bool keep_receiving = true;
  bool have_metadata = false;

  SPEADBeamFormerConfig bf_config;

  // retrieve the meta-data from the stream first
  while (keep_receiving && !have_metadata)
  {
    try
    {
//#ifdef _DEBUG
      cerr << "spip::SPEADReceiverMerge::receive["<<pol<<"] waiting for meta-data heap" << endl;
//#endif
      spead2::recv::heap fh = stream.pop();
//#ifdef _DEBUG
      cerr << "spip::SPEADReceiverMerge::receive["<<pol<<"] received meta-data heap with ID=" << fh.get_cnt() << endl;
//#endif

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
  const double adc_to_bf_sampling_ratio = bf_config.get_adc_to_bf_sampling_ratio ();

  cerr << "spip::SPEADReceiverMerge::receive["<<pol<<"] bytes_per_heap=" << bytes_per_heap << endl;
  cerr << "spip::SPEADReceiverMerge::receive["<<pol<<"] samples_per_heap=" << samples_per_heap << endl;
  cerr << "spip::SPEADReceiverMerge::receive["<<pol<<"] adc_to_bf_sampling_ratio=" << adc_to_bf_sampling_ratio << endl;

  int64_t prev_heap = -1;

  int raw_id;
  unsigned char * ptr;
  uint64_t timestamp;
  // now receive RAW CBF heaps

  int64_t nreceived = 0;
  int64_t ndropped = 0;
  int local_start_adc_sample = start_adc_sample;

  while (keep_receiving)
  {
    try
    {
#ifdef _DEBUG
      cerr << "spip::SPEADReceiverMerge::receive["<<pol<<"] waiting for data heap" << endl;
#endif
      spead2::recv::heap fh = stream.pop();
#ifdef _DEBUG
      cerr << "spip::SPEADReceiverMerge::receive["<<pol<<"] received data heap with ID=" << fh.get_cnt() << " size=" << iteendl;
#endif

      const auto &items = fh.get_items();
      ptr = 0;
      timestamp = 0;
      raw_id = -1;

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

     if (raw_id >= 0)
     {
        if (local_start_adc_sample == -1)
          local_start_adc_sample = timestamp;

        // the number of ADC samples since the start of this observation
        uint64_t adc_sample = timestamp - local_start_adc_sample;
        uint64_t bf_sample = adc_sample / adc_to_bf_sampling_ratio;
        int64_t heap = (int64_t) bf_sample / samples_per_heap;

#ifdef _DEBUG
        cerr << "start=" << local_start_adc_sample << " timestamp=" << timestamp << " adc_sample=" << adc_sample << " adc_to_bf_sampling_ratio=" << adc_to_bf_sampling_ratio << " bf_sample=" << bf_sample << " heap=" << heap << endl;
#endif

        nreceived++;

        if (heap != prev_heap + 1)
        {
          ndropped += (heap - (prev_heap+1));
          float percent_dropped = ((float) ndropped / (float) (ndropped + nreceived)) * 100;
          cerr << "DROPPED: " << (heap - prev_heap) << " heaps. Totals [" << ndropped << " dropped " << nreceived << "recvd  => " << percent_dropped << "% ]" << endl;

        }
        prev_heap = heap;

        if (nreceived % 1024 == 0)
          cerr << "[" << pol << "] received=" << nreceived << " dropped=" << ndropped << " curr=" << fh.get_cnt() << endl;
      }
    }
    catch (spead2::ringbuffer_stopped &e)
    {
      keep_receiving = false;
    }
  }

#ifdef _DEBUG
  cerr << "spip::SPEADReceiverMerge::receive["<<pol<<"] exiting" << endl;
#endif
}
