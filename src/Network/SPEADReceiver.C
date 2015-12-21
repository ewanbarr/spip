/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/SPEADReceiver.h"
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

typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;
using namespace std;

template<typename T>
static inline T extract_bits(T value, int first, int cnt)
{
  assert(0 <= first && first + cnt <= 8 * sizeof(T));
  assert(cnt > 0 && cnt < 8 * sizeof(T));
  return (value >> first) & ((T(1) << cnt) - 1);
}


static time_point start = std::chrono::high_resolution_clock::now();
spip::SPEADReceiver::SPEADReceiver()
{
  verbose = 0;
}

spip::SPEADReceiver::~SPEADReceiver()
{
}

int spip::SPEADReceiver::configure (const char * config)
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

  if (ascii_header_get (config, "START_ADC_SAMPLE", "%lu", &start_adc_sample) != 1)
    start_adc_sample = 0;

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

void spip::SPEADReceiver::prepare (std::string ip_address, int port)
{
  spead_ip = ip_address;
  spead_port = port;
  //endpoint = endpoint (boost::asio::ip::address_v4::from_string(ip_address), port);
}

int spip::SPEADReceiver::open ()
{
  cerr << "spip::SPEADReceiver::open()" << endl;
  return 0;
}

void spip::SPEADReceiver::close ()
{
  cerr << "spip::SPEADReceiver::close()" << endl;
}

// receive SPEAD heaps for the specified time at the specified data rate
bool spip::SPEADReceiver::receive ()
{
  cerr << "spip::SPEADReceiver::receive ()" << endl;

  uint64_t total_bytes_recvd = 0;
  bool obs_started = false;

  int lower = 4194304 * 4;
  int upper = lower + 4096;

  pool = std::make_shared<spead2::memory_pool>(lower, upper, 12, 8);
  spead2::recv::ring_stream<> stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2);
  stream.set_memory_pool(pool);
  boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address_v4::any(), spead_port);
  stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 128 * 1024 * 1024);

  keep_receiving = true;
  bool have_metadata = true;

  // retrieve the meta-data from the stream first
  while (keep_receiving && have_metadata)
  {
    try
    {
#ifdef _DEBUG
      cerr << "spip::SPEADReceiver::receive waiting for meta-data heap" << endl;
#endif
      spead2::recv::heap fh = stream.pop();
#ifdef _DEBUG
      cerr << "spip::SPEADReceiver::receive received meta-data heap with ID=" << fh.get_cnt() << endl;
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

      have_metadata = bf_config.valid();

    }
    catch (spead2::ringbuffer_stopped &e)
    {
      keep_receiving = false;
    }
  }

  bf_config.print_config();

/*
  // block accounting
  const unsigned bytes_per_heap = bf_config.get_bytes_per_heap();
  const unsigned samples_per_heap = bf_config.get_samples_per_heap();
  const double adc_to_bf_sampling_ratio = bf_config.get_adc_to_bf_sampling_ratio ();

  cerr << "spip::SPEADReceiver::receive bytes_per_heap=" << bytes_per_heap << endl;
  cerr << "spip::SPEADReceiver::receive samples_per_heap=" << samples_per_heap << endl;
  cerr << "spip::SPEADReceiver::receive adc_to_bf_sampling_ratio=" << adc_to_bf_sampling_ratio << endl;
*/
  int64_t heap = 0;
  int64_t prev_heap = -1;

  int raw_id;
  unsigned char * ptr;
  uint64_t timestamp;
  // now receive RAW CBF heaps

  uint64_t adc_to_bf_sampling_ratio = 8192;
  uint64_t samples_per_heap = 256;

  uint64_t nreceived = 0;
  uint64_t ndropped = 0;
  while (keep_receiving)
  {
    try
    {
#ifdef _DEBUG
      cerr << "spip::SPEADReceiver::receive waiting for data heap" << endl;
#endif
      spead2::recv::heap fh = stream.pop();
#ifdef _DEBUG
      cerr << "spip::SPEADReceiver::receive received data heap with ID=" << fh.get_cnt() << " size=" << iteendl;
#endif

      const auto &items = fh.get_items();

      //cerr << "heap ID=" << fh.get_cnt() << " size=" << items.size() << endl;
      
      ptr = 0;
      timestamp = 0;
      raw_id = -1;
/*
      for (const auto &item : items)
      {
        if (item.id == SPEAD_CBF_RAW_SAMPLES)
          ptr = item.ptr;
        else if (item.id == SPEAD_CBF_RAW_TIMESTAMP)
          timestamp = SPEADBeamFormerConfig::item_ptr_48u (item.ptr);
        else
          ;
      }
 */
      for (unsigned i=0; i<items.size(); i++)
      {
        cerr << "spip::SPEADReceiver::receive item[" << i << "] ID 0x" << std::hex << items[i].id << std::dec << " length=" << items[i].length << endl;

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
        if (start_adc_sample == 0)
          start_adc_sample = timestamp;

        // the number of ADC samples since the start of this observation
        uint64_t adc_sample = timestamp - start_adc_sample;
        uint64_t bf_sample = adc_sample / adc_to_bf_sampling_ratio;
        heap = (int64_t) bf_sample / samples_per_heap;

//#ifdef _DEBUG
        cerr << "timestamp=" << timestamp << " adc_sample=" << adc_sample << " bf_sample=" << bf_sample << " heap=" << heap << endl;
//#endif

        if (heap != prev_heap + 1)
        {
          ndropped += (heap - prev_heap);
          float percent_dropped = ((float) ndropped / (float) (ndropped + nreceived)) * 100;
          cerr << "DROPPED: " << (heap - prev_heap) << " [" << ndropped << ", " << nreceived << " => " << percent_dropped << "]" << endl;
        }
        else
        {
          nreceived++;
        }
        prev_heap = heap;
      }
    }
    catch (spead2::ringbuffer_stopped &e)
    {
      keep_receiving = false;
    }
  }

#ifdef _DEBUG
  cerr << "spip::SPEADReceiver::receive exiting" << endl;
#endif

  // close the data block
  close();
}

void spip::SPEADReceiver::show_heap(const spead2::recv::heap &fheap)
{
    std::cout << "Received heap with CNT " << fheap.get_cnt() << '\n';
    const auto &items = fheap.get_items();
    std::cout << items.size() << " item(s)\n";
    for (const auto &item : items)
    {
        std::cout << "    ID: 0x" << std::hex << item.id << std::dec << ' ';
        std::cout << "[" << item.length << " bytes]";
        std::cout << '\n';
    }
    std::vector<spead2::descriptor> descriptors = fheap.get_descriptors();
    for (const auto &descriptor : descriptors)
    {
        std::cout
            << "    0x" << std::hex << descriptor.id << std::dec << ":\n"
            << "        NAME:  " << descriptor.name << "\n"
            << "        DESC:  " << descriptor.description << "\n";
        if (descriptor.numpy_header.empty())
        {
            std::cout << "        TYPE:  ";
            for (const auto &field : descriptor.format)
                std::cout << field.first << field.second << ",";
            std::cout << "\n";
            std::cout << "        SHAPE: ";
            for (const auto &size : descriptor.shape)
                if (size == -1)
                    std::cout << "?,";
                else
                    std::cout << size << ",";
            std::cout << "\n";
        }
        else
            std::cout << "        DTYPE: " << descriptor.numpy_header << "\n";
    }
    time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = now - start;
    std::cout << elapsed.count() << "\n";
    std::cout << std::flush;
}
