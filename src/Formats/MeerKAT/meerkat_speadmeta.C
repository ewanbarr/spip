/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "spip/SPEADBeamFormerConfig.h"

#include "spead2/recv_udp.h"
#include "spead2/recv_live_heap.h"
#include "spead2/recv_ring_stream.h"
#include "spead2/common_endian.h"

#include "futils.h"
#include "dada_def.h"

#include <unistd.h>
#include <signal.h>

#include <cstdio>
#include <cstring>
#include <iostream>

#define MEERKAT_DEFAULT_SPEAD_PORT 8888

void usage();
void signal_handler (int signal_value);
char quit_threads = 0;

using namespace std;

int main(int argc, char *argv[])
{
  // Host/IP to receive meta-data stream on
  char * host;

  // udp port to receive meta-data stream on
  int port = MEERKAT_DEFAULT_SPEAD_PORT;

  int verbose = 0;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "hp:v")) != EOF) 
  {
    switch(c) 
    {
      case 'h':
        cerr << "Usage: " << endl;
        usage();
        exit(EXIT_SUCCESS);
        break;

      case 'p':
        port = atoi(optarg);
        break;

      case 'v':
        verbose++;
        break;

      default:
        cerr << "Unrecognised option [" << c << "]" << endl;
        usage();
        return EXIT_FAILURE;
        break;
    }
  }

  // Check arguments
  if ((argc - optind) != 1) 
  {
    fprintf(stderr,"ERROR: 1 command line argument expected\n");
    usage();
    return EXIT_FAILURE;
  }

  signal(SIGINT, signal_handler);
 
  // local address/host to listen on
  host = strdup(argv[optind]);

  int lower = 4 * 1048576;
  int upper = lower + 4096;

  std::shared_ptr<spead2::memory_pool> pool = std::make_shared<spead2::memory_pool>(lower, upper, 12, 8);
  spead2::thread_pool worker;
  spead2::recv::ring_stream<> stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2);
  stream.set_memory_pool(pool);
  boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address_v4::any(), port);
  stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 2 * 1024 * 1024);

  bool keep_receiving = true;
  bool have_metadata = false;

  spip::SPEADBeamFormerConfig bf_config;

  // retrieve the meta-data from the stream first
  while (keep_receiving && !have_metadata)
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
        // ignore items with TIMESTAMPS or CBF_RAW ids
        if (item.id == SPEAD_CBF_RAW_TIMESTAMP || item.id >= 0x5000)
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
#ifdef _DEBUG
      cerr << "=== meta data now contains: ===========" << endl;
      bf_config.print_config();
#endif

    }
    catch (spead2::ringbuffer_stopped &e)
    {
      keep_receiving = false;
    }
  }

  cerr << "spip::SPEADReceiver::receive have meta-data" << endl;
  bf_config.print_config();

  time_t sync_time = bf_config.get_sync_time();
  size_t buffer_size = 20;
  char * buffer = (char *) malloc (buffer_size);
  strftime (buffer, buffer_size, DADA_TIMESTR, gmtime (&sync_time));
  cerr << "ADC_UTC_START=" << buffer << endl;
  strftime (buffer, buffer_size, DADA_TIMESTR, localtime (&sync_time));
  cerr << "ADC_LOCAL_START=" << buffer << endl;

  free(buffer);

  return 0;
}

void usage() 
{
  cout << "meerkat_speadmeta [options] host\n"
    "  host        ip to listen on\n"
    "  -h          print this help text\n"
    "  -p port     udp port [default " << MEERKAT_DEFAULT_SPEAD_PORT << "]\n"
    "  -v          verbose output\n"
    << endl;
}

/*
 *  Simple signal handler to exit more gracefully
 */
void signal_handler(int signalValue)
{
  fprintf(stderr, "received signal %d\n", signalValue);
  if (quit_threads) 
  {
    fprintf(stderr, "received signal %d twice, hard exit\n", signalValue);
    exit(EXIT_FAILURE);
  }
  quit_threads = 1;
}
