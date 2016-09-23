/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/


#include "spip/AsciiHeader.h"
#include "spip/HardwareAffinity.h"
#include "spip/UDPReceiver.h"
#include "spip/UDPFormatMeerKATSimple.h"

#ifdef HAVE_SPEAD2
#include "spip/UDPFormatMeerKATSPEAD.h"
#endif

#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <cstdio>
#include <cstring>
#include <iostream>

void usage();
void * stats_thread (void * arg);
void signal_handler (int signal_value);
char quit_threads = 0;

spip::UDPReceiver * udprecv;

using namespace std;

int main(int argc, char *argv[])
{
  string * format = new string("simple");

  spip::AsciiHeader config;

  // core on which to bind thread operations
  spip::HardwareAffinity hw_affinity;

  int core = -1;

  char verbose = 0;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "b:f:hv")) != EOF) 
  {
    switch(c) 
    {
      case 'b':
        core = atoi(optarg);
        hw_affinity.bind_process_to_cpu_core (core);
        hw_affinity.bind_to_memory (core);
        break;

      case 'f':
        format = new string(optarg);
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

  // Check arguments
  if ((argc - optind) != 1) 
  {
    fprintf(stderr,"ERROR: 1 command line argument expected\n");
    usage();
    return EXIT_FAILURE;
  }

  signal(SIGINT, signal_handler);

  try 
  {
    // create a UDP Receiver
    udprecv = new spip::UDPReceiver();
    udprecv->verbose = verbose;

    if (verbose)
      cerr << "meerkat_udprecv: configuring format to be " << format << endl;

    if (format->compare("simple") == 0)
      udprecv->set_format (new spip::UDPFormatMeerKATSimple());
#ifdef HAVE_SPEAD2
    else if (format->compare("spead") == 0)
    {
      udprecv->set_format (new spip::UDPFormatMeerKATSPEAD());
    }
#endif
    else
    {
      cerr << "ERROR: unrecognized UDP format [" << format << "]" << endl;
      delete udprecv;
      return (EXIT_FAILURE);
    }

    if (verbose)
      cerr << "meerkat_udprecv: Loading configuration from " << argv[optind] << endl;

    // config file for this data stream
    if (config.load_from_file (argv[optind]) < 0)
    {
      cerr << "ERROR: could not read ASCII header from " << argv[optind] << endl;
      return (EXIT_FAILURE);
    }

    if (udprecv->verbose)
      cerr << "meerkat_udprecv: configuring using fixed config" << endl;
    udprecv->configure (config.raw());

    if (udprecv->verbose)
      cerr << "meerkat_udprecv: allocating runtime resources" << endl;
    udprecv->prepare ();

    if (udprecv->verbose)
      cerr << "meerkat_udprecv: starting stats thread" << endl;
    pthread_t stats_thread_id;
    int rval = pthread_create (&stats_thread_id, 0, stats_thread, (void *) recv);
    if (rval != 0)
    {
      cerr << "meerkat_udprecv: failed to start stats thread" << endl;
      return (EXIT_FAILURE);
    }

    if (udprecv->verbose)
      cerr << "meerkat_udprecv: receiving" << endl;
    udprecv->receive ();

    quit_threads = 1;

    if (udprecv->verbose)
      cerr << "meerkat_udprecv: joining stats_thread" << endl;
    void * result;
    pthread_join (stats_thread_id, &result);
  
    delete udprecv;
  }
  catch (std::exception& exc)
  {
    cerr << "meerkat_udprecv: ERROR: " << exc.what() << endl;
    return -1;
  }
  cerr << "meerkat_udprecv: exiting" << endl;

  return 0;
}

void usage() 
{
  cout << "meerkat_udprecv [options] config\n"
    "  header      ascii file contain config and header\n"
#ifdef HAVE_SPEAD2
    "  -f format   receive UDP data of format [simple spead]\n"
#else
    "  -f format   receive UDP data of format [simple]\n"
#endif
    "  -b core     bind computation to specified CPU core\n"
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
  udprecv->stop_receiving();
}


/* 
 *  Thread to print simple capture statistics
 */
void * stats_thread (void * arg)
{
  spip::UDPReceiver * recv = (spip::UDPReceiver *) arg;

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
    if (!quit_threads)
    {
      // get a snapshot of the data as quickly as possible
      b_recv_curr = udprecv->get_stats()->get_data_transmitted();
      b_drop_curr = udprecv->get_stats()->get_data_dropped();
      s_curr = udprecv->get_stats()->get_nsleeps();

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
}

