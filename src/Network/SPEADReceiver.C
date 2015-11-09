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

#include "ascii_header.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <new>
#include <pthread.h>

typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;

using namespace std;

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

  pool = std::make_shared<spead2::memory_pool>(16384, 26214400, 12, 8);
  spead2::recv::ring_stream<> stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2);
  stream.set_memory_pool(pool);
  boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address_v4::any(), 8888);
  stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 8 * 1024 * 1024);

  keep_receiving = true;

  while (keep_receiving)
  {
    // receive a single head from the ring-buffered stream
    try
    {
      spead2::recv::heap fh = stream.pop();
      show_heap(fh);
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
