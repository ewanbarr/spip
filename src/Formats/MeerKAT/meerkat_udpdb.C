/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "spip/HardwareAffinity.h"
#include "spip/UDPReceiveDB.h"
#include "spip/UDPFormatMeerKATSimple.h"
#ifdef HAVE_SPEAD2
#include "spip/UDPFormatMeerKATSPEAD.h"
#endif
#include "spip/TCPSocketServer.h"

#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>

void usage();
void signal_handler (int signal_value);

spip::UDPReceiveDB * udpdb;
char quit_threads = 0;

using namespace std;

int main(int argc, char *argv[]) try
{
  string key = "dada";

  string * format = new string("simple");

  spip::AsciiHeader config;

  // tcp control port to receive configuration
  int control_port = -1;

  // control socket for the control port
  spip::TCPSocketServer * ctrl_sock = 0;

  spip::HardwareAffinity hw_affinity;

  int verbose = 0;

  opterr = 0;
  int c;

  int core;

  while ((c = getopt(argc, argv, "b:c:f:hk:v")) != EOF) 
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

      case 'f':
        format = new string(optarg);
        break;

      case 'k':
        key = optarg;
        break;

      case 'h':
        cerr << "Usage: " << endl;
        usage();
        exit(EXIT_SUCCESS);
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

  // create a UDP recevier that writes to a data block
  udpdb = new spip::UDPReceiveDB (key.c_str());

  if (format->compare("simple") == 0)
    udpdb->set_format (new spip::UDPFormatMeerKATSimple());
#ifdef HAVE_SPEAD2
  else if (format->compare("spead") == 0)
    udpdb->set_format (new spip::UDPFormatMeerKATSPEAD());
#endif
  else
  {
    cerr << "ERROR: unrecognized UDP format [" << format << "]" << endl;
    delete udpdb;
    return (EXIT_FAILURE);
  }

  // Check arguments
  if ((argc - optind) != 1) 
  {
    fprintf(stderr,"ERROR: 1 command line argument expected\n");
    usage();
    return EXIT_FAILURE;
  }
 
  signal(SIGINT, signal_handler);

  // config for the this data stream
  if (config.load_from_file (argv[optind]) < 0)
  {
    cerr << "ERROR: could not read ASCII header from " << argv[optind] << endl;
    return (EXIT_FAILURE);
  }

  uint64_t data_bufsz = udpdb->get_data_bufsz();
  if (config.set("RESOLUTION", "%lu", data_bufsz) < 0)
  {
    fprintf (stderr, "ERROR: could not write RESOLUTION=%lu to config\n", data_bufsz);
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "meerkat_udpdb: configuring using fixed config" << endl;
  udpdb->configure (config.raw());

  if (verbose)
    cerr << "meerkat_udpdb: preparing runtime resources" << endl;
  udpdb->prepare ();

  // prepare a header which combines config with observation parameters
  spip::AsciiHeader header;
  header.load_from_str (config.raw());

  udpdb->start_stats_thread ();

  if (control_port > 0)
  {
    // open a listening socket for observation parameters
    cerr << "meerkat_udpdb: start_control_thread (" << control_port << ")" << endl;
    udpdb->start_control_thread (control_port);

    bool keep_receiving = true;
    while (keep_receiving)
    {
      //if (verbose)
      cerr << "meerkat_udpdb: receiving" << endl;
      keep_receiving = udpdb->receive ();
      cerr << "meerkat_udpdb: receive returned" << endl;
    }
  }
  else
  {
    if (verbose)
      cerr << "meerkat_udpdb: writing header to data block" << endl;
    udpdb->open ();

    cerr << "meerkat_udpdb: issuing start command" << endl;
    udpdb->start_capture ();

    cerr << "meerkat_udpdb: calling receive" << endl;
    udpdb->receive ();
  }

  udpdb->stop_stats_thread ();
  udpdb->close();

  delete udpdb;
}
catch (std::exception& exc)
{
  cerr << "ERROR: " << exc.what() << endl;
  return -1;
  return 0;
}

void usage() 
{
  cout << "meerkat_udpdb [options] config\n"
    "  config      ascii file containing fixed configuration\n"
    "  host        hostname/ip of UDP receiver\n"
    "  -b core     bind computation to specified CPU core\n"
    "  -c port     control port for dynamic configuration\n"
#ifdef HAVE_SPEAD2
    "  -f format   UDP data format [simple spead]\n"
#else
    "  -f format   UDP data format [simple]\n"
#endif
    "  -h          print this help text\n"
    "  -k key      PSRDada shared memory key to write to [default " << std::hex << DADA_DEFAULT_BLOCK_KEY << "]\n"
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

  udpdb->stop_capture();
}

