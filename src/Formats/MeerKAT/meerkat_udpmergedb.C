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
#include <sstream>

void usage();
void signal_handler (int signal_value);

spip::UDPReceiveMergeDB * udpmergedb;
char quit_threads = 0;

using namespace std;

int main(int argc, char *argv[])
{
  string key = "dada";

  string format = "simple";

  spip::AsciiHeader config;

  char * config_file = 0;

  // core on which to bind thread operations
  string cores = "-1,-1";

  int control_port = -1;

  int nsecs = -1;

  // control socket for the control port
  spip::TCPSocketServer * ctrl_sock = 0;

  int verbose = 0;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "b:c:f:hk:t:v")) != EOF) 
  {
    switch(c) 
    {
      case 'b':
        cores = string(optarg);
        break;

      case 'c':
        control_port = atoi(optarg);
        break;

      case 'f':
        format = optarg;
        break;

      case 'h':
        cerr << "Usage: " << endl;
        usage();
        exit(EXIT_SUCCESS);
        break;

      case 'k':
        key = optarg;
        break;

      case 't':
        nsecs = atoi(optarg);
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

  try
  {

    udpmergedb = new spip::UDPReceiveMergeDB(key.c_str());

    if (format.compare("simple") == 0)
      udpmergedb->set_formats (new spip::UDPFormatMeerKATSimple(), new spip::UDPFormatMeerKATSimple());
  #ifdef HAVE_SPEAD2
    else if (format.compare("spead") == 0)
      udpmergedb->set_formats (new spip::UDPFormatMeerKATSPEAD(), new spip::UDPFormatMeerKATSPEAD());
  #endif
    else
    {
      cerr << "ERROR: unrecognized UDP format [" << format << "]" << endl;
      delete udpmergedb;
      return (EXIT_FAILURE);
    }

    // Check arguments
    if ((argc - optind) != 1) 
    {
      fprintf(stderr,"ERROR: 1 command line argument expected\n");
      usage();
      return EXIT_FAILURE;
    }

    int core1, core2;
    string delimited = ",";
    size_t pos = cores.find(delimited);
    string str1 = cores.substr(0, pos);
    string str2 = cores.substr(pos+1, cores.length());
    istringstream(str1) >> core1;
    istringstream(str2) >> core2;

    signal(SIGINT, signal_handler);
   
    // config for the this data stream
    if (config.load_from_file (argv[optind]) < 0)
    {
      cerr << "ERROR: could not read ASCII header from " << argv[optind] << endl;
      return (EXIT_FAILURE);
    }

    if (verbose)
      cerr << "meerkat_udpmergedb: configuring using fixed config" << endl;
    udpmergedb->configure (config.raw());

    if (control_port > 0)
    {
      // open a listening socket for observation parameters
      cerr << "meerkat_udpmergedb: start_control_thread (" << control_port << ")" << endl;
      udpmergedb->start_control_thread (control_port);

      while (!quit_threads)
      {
        if (verbose)
          cerr << "meerkat_udpmergedb: starting threads" << endl;
        udpmergedb->start_threads (core1, core2);
        udpmergedb->join_threads ();
        if (verbose)
          cerr << "meerkat_udpmergedb: threads ended" << endl;
        udpmergedb->set_control_cmd (spip::None);
      }
      udpmergedb->stop_control_thread ();
    }
    else
    {
      if (verbose)
        cerr << "meerkat_udpmergedb: receiving" << endl;
      udpmergedb->start_threads (core1, core2);

      if (verbose)
        cerr << "meerkat_udpmergedb: opening data block" << endl;
      if (udpmergedb->open ())
      {
        cerr << "meerkat_udpmergedb: issuing start command" << endl;
        udpmergedb->set_control_cmd (spip::Start);
      }
      else
      {
        if (verbose)
          cerr << "meerkat_udpmergedb: failed to open data stream" << endl;
      }
      udpmergedb->join_threads ();
    }

    quit_threads = 1;
    delete udpmergedb;
  }
  catch (std::exception& exc)
  {
    cerr << "meerkat_udpmergedb: ERROR: " << exc.what() << endl;
    return -1;
  }


  return 0;
}

void usage() 
{
  cout << "meerkat_udpmergedb [options] header\n"
      "  header      ascii file contain header\n"
      "  -b c1,c2    bind pols 1 and 2 to cores c1 and c2\n"
      "  -c port     listen for control commands on port\n"
  #ifdef HAVE_SPEAD2
      "  -f format   UDP data format [simple spead]\n"
  #else
      "  -f format   UDP data format [simple]\n"
  #endif
      "  -t sec      Receive data for sec seconds\n"
      "  -k key      shared memory key to write to [default " << std::hex << DADA_DEFAULT_BLOCK_KEY << std::dec << "]\n"
      "  -h          print this help text\n"
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
  udpmergedb->set_control_cmd (spip::Quit);
}
