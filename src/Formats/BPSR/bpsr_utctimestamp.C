/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "spip/UDPFormatBPSR.h"
#include "spip/UDPSocketReceive.h"
#include "spip/Time.h"

#include <unistd.h>
#include <signal.h>

#include <cstdio>
#include <cstring>
#include <iostream>

void usage();
void signal_handler (int signal_value);
int quit_threads = 0;

using namespace std;

int main(int argc, char *argv[])
{
  // udp port to receive meta-data stream on
  int port = 4001;

  int verbose = 0;

  int sleep = 0;

  // by default assume 400 MHz
  double tsamp = 0.00125;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "hp:s:t:v")) != EOF) 
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

      case 't':
        tsamp = (double) atof(optarg);
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
    spip::UDPFormatBPSR * format = new spip::UDPFormatBPSR();

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
    sock->resize (8208);
    sock->resize_kernel_buffer (8192);
 
    // busy wait until next 1s "tick"
    time_t now = time(0);
    time_t prev = now;
    while (now == prev)
      now = time(0);

    if (verbose)
      cerr << "getting a packet" << endl;

    // get a packet
    int got = sock->recv ();
    int64_t timestamp = 0;

    if (verbose)
      cerr << "Got " << got << " bytes in packet" << endl;

    if (got > 0)
      timestamp = format->decode_packet_seq (sock->get_buf());

    if (verbose)
      cerr << "timestamp=" << timestamp << endl;

    // samples per second
    uint64_t sample_rate = (uint64_t) 1e6 / tsamp;
    uint64_t seq_rate = sample_rate / 4;
    uint64_t offset = timestamp / seq_rate;

    if (verbose > 1)
      cerr << "sample_rate=" << sample_rate << " seq_rate=" << seq_rate <<  " offset=" << offset << endl;

    time_t adc_sync_time = now - offset;

    if (verbose)
      cerr << "adc_sync_time=" << adc_sync_time << endl;
    if (verbose)
    {
      spip::Time sync (adc_sync_time);
      cerr << "ADC was synced: " << sync.get_localtime() << endl;
    }

    cerr << adc_sync_time << endl;
    return 0;
  }
  catch (std::exception& exc)
  {
    cerr << "bpsr_udptimestamp: ERROR: " << exc.what() << endl;
    return -1;
  }

  cerr << "0" << endl;
  return 1;
}

void usage() 
{
  cout << "bpsr_udptimestamp [options] host [mcast]\n"
    "  host        local address to listen on\n"
    "  mcast       multicast address to listen on\n"
    "  -h          print this help text\n"
    "  -s sec      wait secs until closing socket\n"
    "  -t usec     sampling time [microseconds]\n"
    "  -p port     udp port [default " << 4001 << "]\n"
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
}
