/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "spip/AsciiHeader.h"
#include "spip/HardwareAffinity.h"
#include "spip/UDPReceiveDB.h"
#include "spip/UDPFormatCustom.h"

#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>

void usage();
void * stats_thread (void * arg);
void signal_handler (int signal_value);

spip::UDPReceiveDB * udpdb;
char quit_threads = 0;

using namespace std;

int main(int argc, char *argv[]) try
{
  string key = "dada";

  string format = "standard";

  spip::AsciiHeader config;

  // core on which to bind thread operations
  int core = -1;
  spip::HardwareAffinity hw_affinity;

  int verbose = 0;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "b:f:hk:v")) != EOF) 
  {
    switch(c) 
    {
      case 'b':
        core = atoi(optarg);
        hw_affinity.bind_process_to_cpu_core (core);
        hw_affinity.bind_to_memory (core);
        break;

      case 'f':
        format = optarg;
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

  if (format.compare("standard") == 0)
    ;
  else if (format.compare("custom") == 0)
    udpdb->set_format (new spip::UDPFormatCustom());
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

  if (config.load_from_file (argv[optind]) < 0)
  {
    cerr << "ERROR: could not read ASCII config from " << argv[optind] << endl;
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "ska1_udpdb: configuring" << endl;
  udpdb->configure (config.raw());

  if (verbose)
    cerr << "ska1_udpdb: allocating resources" << endl;
  udpdb->prepare ();

  if (verbose)
    cerr << "ska1_udpdb: writing header to data block" << endl;
  udpdb->open ();

  if (verbose)
    cerr << "ska1_udpdb: starting stats thread" << endl;
  pthread_t stats_thread_id;
  int rval = pthread_create (&stats_thread_id, 0, stats_thread, (void *) udpdb);
  if (rval != 0)
  {
    cerr << "ska1_udpdb: failed to start stats thread" << endl;
    return (EXIT_FAILURE);
  }

  cerr << "ska1_udpdb: issuing start command" << endl;
  udpdb->start_capture ();

  if (verbose)
    cerr << "ska1_udpdb: receiving" << endl;
  udpdb->receive ();
  quit_threads = 1;

  udpdb->close();

  if (verbose)
    cerr << "ska1_udpdb: joining stats_thread" << endl;
  void * result;
  pthread_join (stats_thread_id, &result);

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
  cout << "ska1_udpdb [options] config\n"
    "  config      ascii file containing config\n"
    "  -b core     bind computation to specified CPU core\n"
    "  -f format   UDP data format [standard custom]\n"
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


/* 
 *  Thread to print simple capture statistics
 */
void * stats_thread (void * arg)
{
  spip::UDPReceiveDB * recv = (spip::UDPReceiveDB *) arg;

  uint64_t b_recv_total = 0;
  uint64_t b_recv_curr = 0;
  uint64_t b_recv_1sec;

  uint64_t s_curr = 0;
  uint64_t s_total = 0;
  uint64_t s_1sec;

  uint64_t b_drop_curr = 0;

  float gb_recv_ps = 0;
  float mb_recv_ps = 0;

  while (!quit_threads)
  {
    // get a snapshot of the data as quickly as possible
    b_recv_curr = recv->get_stats()->get_data_transmitted();
    b_drop_curr = recv->get_stats()->get_data_dropped();
    s_curr = recv->get_stats()->get_nsleeps();

    // calc the values for the last second
    b_recv_1sec = b_recv_curr - b_recv_total;
    s_1sec = s_curr - s_total;

    // update the totals
    b_recv_total = b_recv_curr;
    s_total = s_curr;

    mb_recv_ps = (double) b_recv_1sec / 1000000;
    gb_recv_ps = (mb_recv_ps * 8)/1000;

    // determine how much memory is free in the receivers
    fprintf (stderr,"Recv %6.3f [Gb/s] Sleeps %lu Dropped %lu B\n", gb_recv_ps, s_1sec, b_drop_curr);

    sleep(1);
  }
}

