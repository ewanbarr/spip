/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "spip/UDPReceiveMergeDB.h"
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

#define MEERKAT_DEFAULT_SPEAD_PORT 8888

void usage();
void signal_handler (int signal_value);

spip::UDPReceiveMergeDB * udpmergedb;
char quit_threads = 0;

using namespace std;

int main(int argc, char *argv[])
{
  string key = "dada";

  string * format = new string("simple");

  spip::AsciiHeader config;

  char * config_file = 0;

  // Host/IP to receive packets on 
  char * host;

  // udp port to send data to
  int port1 = MEERKAT_DEFAULT_SPEAD_PORT;
  int port2 = MEERKAT_DEFAULT_SPEAD_PORT + 1;

  // core on which to bind thread operations
  int core1 = -1;
  int core2 = -1;

  int control_port = -1;

  // control socket for the control port
  spip::TCPSocketServer * ctrl_sock = 0;

  int verbose = 0;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "b:c:f:hk:p:q:v")) != EOF) 
  {
    switch(c) 
    {
      case 'b':
        core1 = atoi(optarg);
        break;

      case 'c':
        core2 = atoi(optarg);
        break;

      case 'f':
        format = new string(optarg);
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
  udpmergedb = new spip::UDPReceiveMergeDB(key.c_str());

  if (format->compare("simple") == 0)
    udpmergedb->set_formats (new spip::UDPFormatMeerKATSimple(), new spip::UDPFormatMeerKATSimple());
#ifdef HAVE_SPEAD2
  else if (format->compare("spead") == 0)
    udpmergedb->set_formats (new spip::UDPFormatMeerKATSPEAD(), new spip::UDPFormatMeerKATSPEAD());
#endif
  else
  {
    cerr << "ERROR: unrecognized UDP format [" << format << "]" << endl;
    delete udpmergedb;
    return (EXIT_FAILURE);
  }

  // Check arguments
  if ((argc - optind) != 2) 
  {
    fprintf(stderr,"ERROR: 2 command line arguments expected\n");
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

  // local address/host to listen on
  host = strdup(argv[optind+1]);

  uint64_t data_bufsz = udpmergedb->get_data_bufsz();
  if (config.set("RESOLUTION", "%lu", data_bufsz) < 0)
  {
    fprintf (stderr, "ERROR: could not write RESOLUTION=%lu to config\n", data_bufsz);
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "meerkat_udpmergedb: configuring using fixed config" << endl;
  udpmergedb->configure (config.raw());

  if (verbose)
    cerr << "meerkat_udpmergedb: listening for packets on " << host << ":" 
         << port1 << " and  " << host << ":" << port2 << endl;
  udpmergedb->prepare (std::string(host), port1, std::string(host), port2);

  if (control_port > 0)
  {
    // open a listening socket for observation parameters
    cerr << "meerkat_udpmergedb: start_control_thread (" << control_port << ")" << endl;
    udpmergedb->start_control_thread (control_port);

    bool keep_receiving = true;
    while (keep_receiving)
    {
      if (verbose)
        cerr << "meerkat_udpmergedb: receiving" << endl;
      udpmergedb->start_threads (core1, core2);
      udpmergedb->join_threads ();
      cerr << "meerkat_udpmergedb: receive returned" << endl;
    }
  }
  else
  {
    if (verbose)
      cerr << "meerkat_udpmergedb: writing header to data block" << endl;
    udpmergedb->open (config.raw());

    cerr << "meerkat_udpmergedb: calling receive" << endl;
    if (verbose)
      cerr << "meerkat_udpmergedb: receiving" << endl;
    udpmergedb->start_threads (core1, core2);

    cerr << "meerkat_udpmergedb: issuing start command" << endl;
    udpmergedb->set_control_cmd (spip::Start);

    cerr << "meerkat_udpmergedb: waiting for threads to terminate" << endl;
    udpmergedb->join_threads ();
    cerr << "meerkat_udpmergedb: receive returned" << endl;
  }

  //udpmergedb->stop_stats_thread ();
  udpmergedb->close();

  quit_threads = 1;
  delete udpmergedb;

  return 0;
}

void usage() 
{
  cout << "meerkat_udpmergedb [options] header host\n"
    "  header      ascii file contain header\n"
    "  host        ip to listen on\n"
    "  -b core     bind pol1 to specified CPU core\n"
    "  -c core     bind pol2 to specified CPU core\n"
#ifdef HAVE_SPEAD2
    "  -f format   UDP data format [simple spead]\n"
#else
    "  -f format   UDP data format [simple]\n"
#endif
    "  -k key      shared memory key to write to [default " << std::hex << DADA_DEFAULT_BLOCK_KEY << std::dec << "]\n"
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
