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

//#define _DEBUG

#define MEERKAT_DEFAULT_SPEAD_PORT 8888

void usage();
void signal_handler (int signal_value);
int quit_threads = 0;
spead2::recv::ring_stream<> *stream_ptr = 0;

using namespace std;

int main(int argc, char *argv[])
{
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
  int num_args = argc - optind;
  if ((num_args != 1) && (num_args != 2))
  {
    fprintf(stderr,"ERROR: at least 1 command line argument required\n");
    usage();
    return EXIT_FAILURE;
  }

  signal(SIGINT, signal_handler);
 
  // local address/host to listen on
  boost::asio::ip::address host = boost::asio::ip::address::from_string(string(argv[optind]));
  if (num_args == 2)
  {
    boost::asio::ip::address mcast = boost::asio::ip::address::from_string(string(argv[optind+1]));
  }

  int lower = 4 * 1048576;
  int upper = lower + 4096;

  std::shared_ptr<spead2::memory_pool> pool = std::make_shared<spead2::memory_pool>(lower, upper, 12, 8);
  spead2::thread_pool worker;
  spead2::recv::ring_stream<> stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2);
  stream.set_memory_pool(pool);

  size_t max_size = spead2::recv::udp_reader::default_max_size;
  size_t buffer_size = 262144;

  if (num_args == 1)
  {
    boost::asio::ip::udp::endpoint endpoint(host, port); 
    stream.emplace_reader<spead2::recv::udp_reader>(endpoint, max_size, buffer_size);
  }
  else 
  {
    boost::asio::ip::address mcast = boost::asio::ip::address::from_string(string(argv[optind+1]));
    boost::asio::ip::udp::endpoint endpoint(mcast, port);
    stream.emplace_reader<spead2::recv::udp_reader>(endpoint, max_size, buffer_size, host);
  }
  stream_ptr = &stream;

  bool keep_receiving = true;
  bool have_metadata = false;

  spip::SPEADBeamFormerConfig bf_config;

  // retrieve the meta-data from the stream first
  while (keep_receiving && !have_metadata)
  {
    try
    {
      spead2::recv::heap fh = stream.pop();
      const auto &items = fh.get_items();
      for (const auto &item : items)
        bf_config.parse_item (item);

      vector<spead2::descriptor> descriptors = fh.get_descriptors();
      for (const auto &descriptor : descriptors)
        bf_config.parse_descriptor (descriptor);
#ifdef _DEBUG
      cerr << "=== meta data now contains: ===========" << endl;
      bf_config.print_config();
#endif
      have_metadata = bf_config.valid();
    }
    catch (spead2::ringbuffer_stopped &e)
    {
      keep_receiving = false;
    }
  }

  if (have_metadata)
  {
    stream.stop();
    if (verbose)
    {
      cerr << "meerkat_speadmeta: have meta-data" << endl;
      bf_config.print_config();
    }

    time_t sync_time = bf_config.get_sync_time();

    if (verbose)
    {
      size_t buffer_size = 20;
      char * buffer = (char *) malloc (buffer_size);
      strftime (buffer, buffer_size, DADA_TIMESTR, gmtime (&sync_time));
      cerr << "ADC_UTC_START=" << buffer << endl;
      strftime (buffer, buffer_size, DADA_TIMESTR, localtime (&sync_time));
      cerr << "ADC_LOCAL_START=" << buffer << endl;
      free(buffer);
    }

    cerr << sync_time << endl;
    return 0;
  }
  else
  {
    cerr << "0" << endl;
    return 1;
  }
}

void usage() 
{
  cout << "meerkat_speadmeta [options] host [mcast]\n"
    "  host        local address to listen on\n"
    "  mcast       multicast address to listen on\n"
    "  -h          print this help text\n"
    "  -p port     udp port [default " << MEERKAT_DEFAULT_SPEAD_PORT << "]\n"
    "  -v          verbose output\n"
    << endl;
}

/*
 *  Simple signal handler to exit more gracefully
 */
void signal_handler (int signo)
{
  if (signo == SIGINT)
    fprintf(stderr, "meerkat_speadmeta: received SIGINT\n");
  else if (signo == SIGTERM)
    fprintf(stderr, "meerkat_speadmeta: received SIGTERM\n");
  else if (signo == SIGPIPE)
    fprintf(stderr, "meerkat_speadmeta: received SIGPIPE\n");
  else if (signo == SIGKILL)
    fprintf(stderr, "meerkat_speadmeta: received SIGKILL\n");
  else
    fprintf(stderr, "meerkat_speadmeta: received SIGNAL %d\n", signo);
  if (quit_threads) 
  {
    exit(EXIT_FAILURE);
  }
  quit_threads = 1;
  if (stream_ptr)
    stream_ptr->stop();
}
