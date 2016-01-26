/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "dada_def.h"
#include "futils.h"
#include "dada_affinity.h"
#include "ascii_header.h"

#include "spip/BlockFormatMeerKAT.h"
#include "spip/DataBlockStats.h"
#include "spip/TCPSocketServer.h"

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

  char * config_file = 0;

  // tcp control port to receive configuration
  int control_port = -1;

  // control socket for the control port
  spip::TCPSocketServer * ctrl_sock = 0;

  // core on which to bind thread operations
  int core = -1;

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

  // bind CPU computation to specific core
  if (core >= 0)
    dada_bind_thread_to_core (core);
  
  // create a DataBlockStats processor to read from the DB
  if (verbose)
    cerr << "meerkat_dbstats: creating DataBlockStats with " << key << endl;
  dbstats = new spip::DataBlockStats (key.c_str());

  dbstats->set_block_format (new spip::BlockFormatMeerKAT());

  // Check arguments
  if ((argc - optind) != 1) 
  {
    fprintf(stderr,"ERROR: 1 command line arguments expected\n");
    usage();
    return EXIT_FAILURE;
  }
 
  signal(SIGINT, signal_handler);

  // header the this data stream
  config_file = strdup (argv[optind]);

  char * config = (char *) malloc (DADA_DEFAULT_HEADER_SIZE);
  if (config == NULL)
  {
    fprintf (stderr, "ERROR: could not allocate memory for config buffer\n");
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "meerkat_dbstats: reading config from " << config_file << endl;
  if (fileread (config_file, config, DADA_DEFAULT_HEADER_SIZE) < 0)
  {
    free (config);
    fprintf (stderr, "ERROR: could not read config from %s\n", config_file);
    return (EXIT_FAILURE);
  }

  uint64_t data_bufsz = dbstats->get_data_bufsz();
  if (ascii_header_set (config, "RESOLUTION", "%lu", data_bufsz) < 0)
  {
    free (config);
    fprintf (stderr, "ERROR: could not write RESOLUTION=%lu to config\n", data_bufsz);
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "meerkat_dbstats: configuring using fixed config" << endl;
  dbstats->configure (config);

  // prepare a header which combines config with observation parameters
  char * header = (char *) malloc (DADA_DEFAULT_HEADER_SIZE);
  if (header == NULL)
  {
    fprintf (stderr, "ERROR: could not allocate memory for header buffer\n");
    return (EXIT_FAILURE);
  }

  // assume that the config includes a header param
  strncpy (header, config, strlen(config) + 1);

  dbstats->prepare();

  // open a listening socket for observation parameters
  if (control_port > 0)
  {
    cerr << "meerkat_dbstats: start_control_thread (" << control_port << ")" << endl;
    dbstats->start_control_thread (control_port);

    bool keep_monitoring = true;
    while (keep_monitoring)
    {
      if (verbose)
        cerr << "meerkat_dbstats: monitor()" << endl;
      keep_monitoring = dbstats->monitor(stats_dir, stream);
      if (verbose)
        cerr << "meerkat_dbstats: monitor returned" << endl;
    }
  }
  else
  {
    cerr << "meerkat_dbstats: calling monitor" << endl;
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
  cout << "meerkat_dbstats [options] config\n"
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
