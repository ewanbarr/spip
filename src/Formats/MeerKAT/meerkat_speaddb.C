/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "dada_def.h"
#include "futils.h"

#include "spip/HardwareAffinity.h"
#include "spip/SPEADReceiveDB.h"

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

spip::SPEADReceiveDB * speaddb;

using namespace std;

int main(int argc, char *argv[])
{
  string key = "dada";

  char * config_file = 0;

  // Host/IP to receive packets on 
  char * host;

  // udp port to send data to
  int port = MEERKAT_DEFAULT_SPEAD_PORT;

  // for cpu and memory binding
  spip::HardwareAffinity hw_affinity;
  int core = -1;

  int verbose = 0;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "b:hk:p:v")) != EOF) 
  {
    switch(c) 
    {
      case 'b':
        core = atoi(optarg);
        hw_affinity.bind_process_to_cpu_core (core);
        hw_affinity.bind_to_memory (core);
        break;

      case 'h':
        cerr << "Usage: " << endl;
        usage();
        exit(EXIT_SUCCESS);
        break;

      case 'k':
        key = optarg;
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

  // create a UDP Receiver
  speaddb = new spip::SPEADReceiveDB(key.c_str());

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
    cerr << "meerkat_speaddb: reading config from " << config_file << endl;
  if (fileread (config_file, config, DADA_DEFAULT_HEADER_SIZE) < 0)
  {
    free (config);
    fprintf (stderr, "ERROR: could not read ASCII config from %s\n", config_file);
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "meerkat_speaddb: configuring using fixed config" << endl;
  speaddb->configure (config);

  if (verbose)
    cerr << "meerkat_speaddb: listening for packets on " << host << ":" 
         << port << endl;
  speaddb->prepare (std::string(host), port);

  if (verbose)
    cerr << "meerkat_speaddb: receiving" << endl;
  speaddb->receive ();

  quit_threads = 1;

  delete speaddb;

  return 0;
}

void usage() 
{
  cout << "meerkat_speaddb [options] header host\n"
    "  header      ascii file contain header\n"
    "  host        ip to listen on\n"
    "  -b core     bind computation to specified CPU core\n"
    "  -k key      write to SHM with key\n"
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
  speaddb->stop_capture();
  if (quit_threads) 
  {
    fprintf(stderr, "received signal %d twice, hard exit\n", signalValue);
    exit(EXIT_FAILURE);
  }
  quit_threads = 1;

}
