/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "dada_def.h"
#include "futils.h"

#include "spip/SPEADReceiverMerge.h"

#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <cstdio>
#include <cstring>
#include <iostream>

#define MEERKAT_DEFAULT_SPEAD_PORT 8888

void usage();
void * stats_thread (void * arg);
void signal_handler (int signal_value);
char quit_threads = 0;

spip::SPEADReceiverMerge * speadrecvmerge;

using namespace std;

int main(int argc, char *argv[])
{
  char * config_file = 0;

  // Host/IP to receive packets on 
  char * host;

  // udp port to send data to
  int port1 = MEERKAT_DEFAULT_SPEAD_PORT;
  int port2 = MEERKAT_DEFAULT_SPEAD_PORT + 1;

  // core on which to bind thread operations
  int core1 = -1;
  int core2 = -1;

  int verbose = 0;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "b:c:hnp:q:v")) != EOF) 
  {
    switch(c) 
    {
      case 'b':
        core1 = atoi(optarg);
        break;

      case 'c':
        core2 = atoi(optarg);
        break;

      case 'h':
        cerr << "Usage: " << endl;
        usage();
        exit(EXIT_SUCCESS);
        break;

      case 'p':
        port1 = atoi(optarg);
        break;

      case 'q':
        port2 = atoi(optarg);
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

  // create a UDP Receiver
  speadrecvmerge = new spip::SPEADReceiverMerge();

  // Check arguments
  if ((argc - optind) != 2) 
  {
    fprintf(stderr,"ERROR: 2 command line arguments expected\n");
    usage();
    return EXIT_FAILURE;
  }

  signal(SIGINT, signal_handler);
 
  // config file for this data stream
  config_file = strdup (argv[optind]);

  // local address/host to listen on
  host = strdup(argv[optind+1]);

  char * config = (char *) malloc (sizeof(char) * DADA_DEFAULT_HEADER_SIZE);
  if (config_file == NULL)
  {
    fprintf (stderr, "ERROR: could not allocate memory for config buffer\n");
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "meerkat_speadrecvmerge: reading config from " << config_file << endl;
  if (fileread (config_file, config, DADA_DEFAULT_HEADER_SIZE) < 0)
  {
    free (config);
    fprintf (stderr, "ERROR: could not read ASCII config from %s\n", config_file);
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "meerkat_speadrecvmerge: configuring using fixed config" << endl;
  speadrecvmerge->configure (config);

  if (verbose)
    cerr << "meerkat_speadrecvmerge: listening for packets on " << host << ":" 
         << port1 << " and  " << host << ":" << port2 << endl;
  speadrecvmerge->prepare (std::string(host), port1, port2);

  if (verbose)
    cerr << "meerkat_speadrecvmerge: receiving" << endl;
  speadrecvmerge->start_recv_threads (core1, core2);

  speadrecvmerge->join_recv_threads ();

  quit_threads = 1;

  delete speadrecvmerge;

  return 0;
}

void usage() 
{
  cout << "meerkat_speadrecvmerge [options] header host\n"
    "  header      ascii file contain header\n"
    "  host        ip to listen on\n"
    "  -b core     bind pol1 to specified CPU core\n"
    "  -c core     bind pol2 to specified CPU core\n"
    "  -h          print this help text\n"
    "  -p port     udp port pol1 [default " << MEERKAT_DEFAULT_SPEAD_PORT << "]\n"
    "  -q port     udp port pol2 [default " << ( MEERKAT_DEFAULT_SPEAD_PORT + 1) << "]\n"
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
