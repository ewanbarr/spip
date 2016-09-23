/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "spip/SPEADBeamFormerConfig.h"
#include "spip/UDPFormatMeerKATSPEAD.h"
#include "spip/UDPSocketReceive.h"
#include "spip/Time.h"

#include "spead2/recv_packet.h"

#include <unistd.h>
#include <signal.h>

#include <cstdio>
#include <cstring>
#include <iostream>

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

  int sleep = 0;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "hp:s:v")) != EOF) 
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

      case 's':
        sleep = atoi(optarg);
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
 
  try 
  {

    string host = string(argv[optind]);

    spip::UDPSocketReceive * sock = new spip::UDPSocketReceive ();
    spip::UDPFormatMeerKATSPEAD * format = new spip::UDPFormatMeerKATSPEAD();

    if (num_args == 1)
    {
      if (verbose)
        cerr << "Listening on unicast " << host << ":" << port << endl;
      sock->open (host, port);
    }
    else
    {
      string mcast = string(argv[optind+1]);
      if (verbose)
        cerr << "Listening on multicast " << mcast << ":" << port << " via " << host << endl;
      sock->open_multicast (host, mcast, port);
    }

    sock->set_block ();
    sock->resize (4200);
    sock->resize_kernel_buffer (8192);
 
    int got = sock->recv ();
    if (verbose > 1)
      cerr << "Received a packet of " << got << " bytes" << endl;

    int64_t timestamp = 0;
    time_t adc_sync_time = 0;
    time_t now;

    if (got > 0)
    {
      format->decode_spead (sock->get_buf());
      if (verbose > 1)
        format->print_packet_header();
      timestamp = format->get_timestamp_fast();
      now = time(0);
    }

    usleep (sleep * 1e6);

    if (timestamp > 0)
    {
      uint64_t adc_sample_rate = 1712e6;
      uint64_t offset = timestamp / adc_sample_rate;
      adc_sync_time = now - offset;
      if (verbose)
      {
        spip::Time sync (adc_sync_time);
        cerr << "ADC was synced: " << sync.get_localtime() << endl; 
      }

      cerr << adc_sync_time << endl;
      return 0;
    }

    //delete sock;
  }
  catch (std::exception& exc)
  {
    cerr << "meerkat_speadtimestamp: ERROR: " << exc.what() << endl;
    return -1;
  }

  cerr << "0" << endl;
  return 1;
}

void usage() 
{
  cout << "meerkat_speadtimestamp [options] host [mcast]\n"
    "  host        local address to listen on\n"
    "  mcast       multicast address to listen on\n"
    "  -h          print this help text\n"
    "  -s sec      wait secs until closing socket\n"
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
    fprintf(stderr, "meerkat_speadtimestamp: received SIGINT\n");
  else if (signo == SIGTERM)
    fprintf(stderr, "meerkat_speadtimestamp: received SIGTERM\n");
  else if (signo == SIGPIPE)
    fprintf(stderr, "meerkat_speadtimestamp: received SIGPIPE\n");
  else if (signo == SIGKILL)
    fprintf(stderr, "meerkat_speadtimestamp: received SIGKILL\n");
  else
    fprintf(stderr, "meerkat_speadtimestamp: received SIGNAL %d\n", signo);
  if (quit_threads) 
  {
    exit(EXIT_FAILURE);
  }
  quit_threads = 1;
  if (stream_ptr)
    stream_ptr->stop();
}
