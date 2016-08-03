/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "config.h" 

#include "spip/AsciiHeader.h"
#include "spip/HardwareAffinity.h"
#include "spip/UDPGenerator.h"
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

spip::UDPGenerator * gen = 0;
char quit_threads = 0;

using namespace std;

int main(int argc, char *argv[])
{
  spip::AsciiHeader config;

  char * src_host = 0;

  // total time to transmit for
  unsigned transmission_time = 5;   

  // data rate at which to transmit
  float data_rate_gbits = 0.5;

  // core on which to bind thread operations
  int core = -1;
  spip::HardwareAffinity hw_affinity;

  int verbose = 0;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "b:f:r:t:v")) != EOF) 
  {
    switch(c) 
    {
      case 'b':
        core = atoi(optarg);
        hw_affinity.bind_process_to_cpu_core (core);
        hw_affinity.bind_to_memory (core);
        break;

      case 'h':
        usage();
        exit(EXIT_SUCCESS);
        break;

      case 't':
        transmission_time = atoi(optarg);
        break;

      case 'r':
        data_rate_gbits = atof(optarg);
        break;

      case 'v':
        verbose++;
        break;

      default:
        usage();
        return EXIT_FAILURE;
        break;
    }
  }

  // check arguments
  if ((argc - optind) != 1) 
  {
    fprintf(stderr,"ERROR: 1 command line argument expected\n");
    usage();
    return EXIT_FAILURE;
  }

  gen = new spip::UDPGenerator();
  spip::UDPFormatVDIF * format = new spip::UDPFormatVDIF();
  format->set_self_start (false);
  gen->set_format (format);

  signal(SIGINT, signal_handler);

  // header the this data stream
  if (config.load_from_file (argv[optind]) < 0)
  {
    cerr << "ERROR: could not read ASCII header from " << argv[optind] << endl;
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "vdif_udpgen: configuring based on header" << endl;
  gen->configure (config.raw());

  if (verbose)
    cerr << "vdif_udpgen: allocating signal" << endl;
  gen->allocate_signal ();

  if (verbose)
    cerr << "vdif_udpgen: computing VDIF header" << endl;
  format->compute_header ();

  if (verbose)
    cerr << "vdif_udpgen: allocating resources" << endl;
  gen->prepare ();

  if (verbose)
    cerr << "vdif_udpgen: starting stats thread" << endl;
  pthread_t stats_thread_id;
  int rval = pthread_create (&stats_thread_id, 0, stats_thread, (void *) gen);
  if (rval != 0)
  {
    cerr << "vdif_udpgen: failed to start stats thread" << endl;
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "vdif_udpgen: transmitting for " << transmission_time << " seconds at " << data_rate_gbits << " Gib/s" << endl;
  gen->transmit (transmission_time, data_rate_gbits * 1e9);

  quit_threads = 1;

  if (verbose)
    cerr << "vdif_udpgen: joining stats_thread" << endl;
  void * result;
  pthread_join (stats_thread_id, &result);

  delete gen;

  return 0;
}

void usage() 
{
  cout << "vdif_udpgen [options] header\n"
    "  header      ascii file contain header\n"
    "  -b core     bind computation to specified CPU core\n"
    "  -h          print this help text\n"
    "  -t secs     number of seconds to transmit [default 5]\n"
    "  -r rate     transmit at rate Gib/s [default 0.5]\n"
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

  gen->stop_transmit();
}


/* 
 *  Thread to print simple capture statistics
 */
void * stats_thread (void * arg) 
{
  spip::UDPGenerator * gen = (spip::UDPGenerator *) arg;

  uint64_t b_sent_total = 0;
  uint64_t b_sent_1sec = 0;
  uint64_t b_sent_curr = 0;

  uint64_t s_sent_total = 0;
  uint64_t s_sent_1sec = 0;

  float gb_sent_ps = 0;
  float mb_sent_ps = 0;

  while (!quit_threads)
  {
    // get a snapshot of the data as quickly as possible
    b_sent_curr = gen->get_stats()->get_data_transmitted();

    // calc the values for the last second
    b_sent_1sec = b_sent_curr - b_sent_total;

    // update the totals
    b_sent_total = b_sent_curr;

    mb_sent_ps = (double) b_sent_1sec / 1000000;
    gb_sent_ps = (mb_sent_ps * 8)/1000;

    // determine how much memory is free in the receivers
    fprintf (stderr,"Rate=%6.3f [Gb/s]\n", gb_sent_ps);

    sleep(1);
  }
}

