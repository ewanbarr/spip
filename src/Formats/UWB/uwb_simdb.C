/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "spip/AsciiHeader.h"
#include "spip/HardwareAffinity.h"
#include "spip/SimReceiveDB.h"
#include "spip/UDPFormatVDIF.h"

#include <pthread.h>
#include <unistd.h>
#include <signal.h> 
#include <cstdio>
#include <cstring>

#include <iostream>

void usage();
void * stats_thread (void * arg);
void signal_handler (int signal_value);

spip::SimReceiveDB * simdb = 0;
char quit_threads = 0;

using namespace std;

int main(int argc, char *argv[])
{
  string key = "dada";

  spip::AsciiHeader config;

  // TCP control port to recieve configuration
  int control_port = -1;

  // total time to transmit for
  int transmission_time = -1;

  // core on which to bind thread operations
  int core = -1;
  spip::HardwareAffinity hw_affinity;

  int verbose = 0;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "b:c:hk:t:v")) != EOF) 
  {
    switch(c) 
    {
      case 'b':
        core = atoi(optarg);
        hw_affinity.bind_process_to_cpu_core (core);
        hw_affinity.bind_to_memory (core);
        break;

      case 'c':
        control_port = atoi(optarg);
        break;

      case 'h':
        usage();
        return (EXIT_SUCCESS);
        break;

      case 'k':
        key = optarg;
        break;

      case 't':
        transmission_time = atoi(optarg);
        break;

      case 'v':
        verbose++;
        break;

      default:
        cerr << "ERROR: did not expect option " << c << endl;
        usage();
        return EXIT_FAILURE;
        break;
    }
  }

  // Check arguments
  if ((argc - optind) != 1) 
  {
    cerr << "ERROR: 1 command line argument expected" << endl;
    usage();
    return EXIT_FAILURE;
  }

  simdb = new spip::SimReceiveDB(key.c_str());
  simdb->set_format (new spip::UDPFormatVDIF());
  simdb->set_verbosity (verbose);

  signal(SIGINT, signal_handler);

  if (config.load_from_file (argv[optind]) < 0)
  {
    cerr << "ERROR: could not read ASCII config from " << argv[optind] << endl;
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "uwb_simdb: configuring based on header" << endl;
  simdb->configure (config.raw());

  if (verbose)
    cerr << "uwb_simdb: preparing" << endl;
  simdb->prepare ();


  if (control_port > 0)
  {
    // open a listening socket for observation parameters
    cerr << "uwb_dbsim: start_control_thread (" << control_port << ")" << endl;
    simdb->start_control_thread (control_port);

    bool keep_receiving = true;
    while (keep_receiving)
    {
      simdb->generate (-1);
    }
  }
  else
  {
    if (verbose)
      cerr << "uwb_simdb: starting stats thread" << endl;
    simdb->start_stats_thread();

    if (verbose)
      cerr << "uwb_simdb: writing header to data block" << endl;
    simdb->open (config.raw());

    if (verbose)
      cerr << "uwb_simdb: issuing start command" << endl;
    simdb->start_transmit();

    if (verbose)
      cerr << "uwb_simdb: calling receive" << endl;
    simdb->generate (transmission_time);

    if (verbose)
      cerr << "uwb_simdb: stopping stats thread" << endl;
    simdb->stop_stats_thread();
  }
  quit_threads = 1;

  delete simdb;

  return 0;
}

void usage() 
{
  cout << "uwb_simdb [options] config_file\n"
    "  config_file ascii file contain fixed configuration\n"
    "  -b core     bind computation to specified CPU core\n"
    "  -c port     control port for dynamic configuration\n"
    "  -k key      PSRDada shared memory key to write to [default " << std::hex << DADA_DEFAULT_BLOCK_KEY << "]\n"
    "  -h          print this help text\n"
    "  -t secs     number of seconds to transmit [default 5]\n"
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

  simdb->stop_transmit();
}
