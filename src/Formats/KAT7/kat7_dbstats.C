/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "spip/AsciiHeader.h"
#include "spip/HardwareAffinity.h"
#include "spip/DataBlockStats.h"
#include "spip/TCPSocketServer.h"
#include "spip/BlockFormatKAT7.h"

#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>

void usage();
void * dbstats_thread (void * arg);
void signal_handler (int signal_value);

spip::DataBlockStats * dbstats;
char quit_threads = 0;

using namespace std;

int main(int argc, char *argv[]) try
{
  string key = "dada";

  spip::AsciiHeader config;

  // tcp control port to receive configuration
  int control_port = -1;

  // core on which to bind thread operations
  int core = -1;
  spip::HardwareAffinity hw_affinity;

  string stats_dir = "";

  int verbose = 0;

  int stream = 0;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "b:c:D:hk:n:s:v")) != EOF) 
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

      case 'D':
        stats_dir = string(optarg);
        break;

      case 'k':
        key = optarg;
        break;

      case 'h':
        cerr << "Usage: " << endl;
        usage();
        exit(EXIT_SUCCESS);
        break;

      case 's':
        stream = atoi(optarg);
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

  // create a DataBlockStats processor to read from the DB
  if (verbose)
    cerr << "kat7_dbstats: creating DataBlockStats with " << key << endl;
  dbstats = new spip::DataBlockStats (key.c_str());

  // Check arguments
  if ((argc - optind) != 1) 
  {
    fprintf(stderr,"ERROR: 1 command line arguments expected\n");
    usage();
    return EXIT_FAILURE;
  }

  dbstats->set_block_format (new spip::BlockFormatKAT7());
 
  signal(SIGINT, signal_handler);

  // config the this data stream
  if (config.load_from_file (argv[optind]) < 0)
  {
    cerr << "ERROR: could not read ASCII config from " << argv[optind] << endl;
    return (EXIT_FAILURE);
  }

  uint64_t data_bufsz = dbstats->get_data_bufsz();
  if (config.set ("RESOLUTION", "%lu", data_bufsz) < 0)
  {
    fprintf (stderr, "ERROR: could not write RESOLUTION=%lu to config\n", data_bufsz);
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "kat7_dbstats: configuring using fixed config" << endl;
  dbstats->configure (config.raw());

  dbstats->prepare();

  // open a listening socket for observation parameters
  if (control_port > 0)
  {
    cerr << "kat7_dbstats: start_control_thread (" << control_port << ")" << endl;
    dbstats->start_control_thread (control_port);

    bool keep_monitoring = true;
    while (keep_monitoring)
    {
      if (verbose)
        cerr << "kat7_dbstats: monitor()" << endl;
      keep_monitoring = dbstats->monitor(stats_dir, stream);
      if (verbose)
        cerr << "kat7_dbstats: monitor returned" << endl;
    }
  }
  else
  {
    cerr << "kat7_dbstats: calling monitor" << endl;
    dbstats->monitor(stats_dir, stream);
  }


  delete dbstats;
}
catch (std::exception& exc)
{
  cerr << "ERROR: " << exc.what() << endl;
  return -1;
  return 0;
}

void usage() 
{
  cout << "kat7_dbstats [options] config\n"
    "  config      ascii file containing fixed configuration\n"
    "  -b core     bind computation to specified CPU core\n"
    "  -c port     control port for dynamic configuration\n"
    "  -D dir      dump HG and FT files to dir [default cwd]\n"
    "  -h          print this help text\n"
    "  -k key      PSRDada shared memory key to read from [default " << std::hex << DADA_DEFAULT_BLOCK_KEY << "]\n"
    "  -s stream   dump HG and FT files with this stream id [default 0]\n"
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
  dbstats->stop_monitoring();
}
