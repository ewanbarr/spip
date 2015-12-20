/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "dada_def.h"
#include "futils.h"
#include "dada_affinity.h"

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
#ifdef HAVE_SPEAD2
  string * format = new string("spead");
#else
  string * format = new string("simple");
#endif

  char * config_file = 0;

  // Host/IP to receive packets on 
  char * host;

  // udp port to send data to
  int port = MEERKAT_DEFAULT_UDP_PORT;

  // core on which to bind thread operations
  int core = -1;

  int verbose = 0;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "b:f:hp:v")) != EOF) 
  {
    switch(c) 
    {
      case 'b':
        core = atoi(optarg);
        break;

      case 'f':
        format = new string(optarg);
        break;

      case 'h':
        cerr << "Usage: " << endl;
        usage();
        exit(EXIT_SUCCESS);
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

  // bind CPU computation to specific core
  if (core >= 0)
  {
    dada_bind_thread_to_core (core);

#ifdef HAVE_HWLOC
    hwloc_topology_t topology;
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);
    hwloc_obj_t obj = hwloc_get_obj_by_depth (topology, core_depth, core);
    if (obj)
    {
      // Get a copy of its cpuset that we may modify.
      hwloc_cpuset_t cpuset = hwloc_bitmap_dup (obj->cpuset);

      // Get only one logical processor (in case the core is SMT/hyperthreaded)
      hwloc_bitmap_singlify (cpuset);

      hwloc_membind_policy_t policy = HWLOC_MEMBIND_BIND;
      hwloc_membind_flags_t flags = 0;

      int result = hwloc_set_membind (topology, cpuset, policy, flags);
      if (result < 0)
      {
        fprintf (stderr, "dada_db: failed to set memory binding policy: %s\n",
                 strerror(errno));
        return -1;
      }

      // Free our cpuset copy
      hwloc_bitmap_free(cpuset);
    }
#endif

  }

  // create a UDP Receiver
  udprecv = new spip::UDPReceiver();

  if (format->compare("simple") == 0)
    udprecv->set_format (new spip::UDPFormatMeerKATSimple());
#ifdef HAVE_SPEAD2
  else if (format->compare("spead") == 0)
  {
    cerr << "spip::UDPReceiver set_format (spip::UDPFormatMeerKATSPEAD)" << endl;
    udprecv->set_format (new spip::UDPFormatMeerKATSPEAD());
  }
#endif
  else
  {
    cerr << "ERROR: unrecognized UDP format [" << format << "]" << endl;
    delete udprecv;
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
    cerr << "meerkat_udprecv: reading config from " << config_file << endl;
  if (fileread (config_file, config, DADA_DEFAULT_HEADER_SIZE) < 0)
  {
    free (config);
    fprintf (stderr, "ERROR: could not read ASCII config from %s\n", config_file);
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "meerkat_udprecv: configuring using fixed config" << endl;
  udprecv->configure (config);

  if (verbose)
    cerr << "meerkat_udprecv: listening for packets on " << host << ":" 
         << port << endl;
  udprecv->prepare (std::string(host), port);

  if (verbose)
    cerr << "meerkat_udprecv: starting stats thread" << endl;
  pthread_t stats_thread_id;
  int rval = pthread_create (&stats_thread_id, 0, stats_thread, (void *) recv);
  if (rval != 0)
  {
    cerr << "meerkat_udprecv: failed to start stats thread" << endl;
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "meerkat_udprecv: receiving" << endl;
  udprecv->receive ();

  quit_threads = 1;

  if (verbose)
    cerr << "meerkat_udprecv: joining stats_thread" << endl;
  void * result;
  pthread_join (stats_thread_id, &result);

  delete udprecv;

#ifdef HAVE_HWLOC
  hwloc_topology_destroy(topology);
#endif

  return 0;
}

void usage() 
{
  cout << "meerkat_udprecv [options] header host\n"
    "  header      ascii file contain header\n"
    "  host        hostname/ip of UDP receiver\n"
#ifdef HAVE_SPEAD2
    "  -f format   recverate UDP data of format [simple spead]\n"
#else
    "  -f format   recverate UDP data of format [simple spead]\n"
#endif
    "  -b core     bind computation to specified CPU core\n"
    "  -h          print this help text\n"
    "  -n secs     number of seconds to transmit [default 5]\n"
    "  -p port     destination udp port [default " << MEERKAT_DEFAULT_UDP_PORT << "]\n"
    "  -r rate     transmit at rate Mib/s [default 10]\n"
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

  uint64_t p_drop_curr = 0;

  float gb_recv_ps = 0;
  float mb_recv_ps = 0;

  while (!quit_threads)
  {
    // get a snapshot of the data as quickly as possible
    b_recv_curr = udprecv->get_stats()->get_data_transmitted();
    p_drop_curr = udprecv->get_stats()->get_packets_dropped();
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
    fprintf (stderr,"Recv %6.3f [Gb/s] Sleeps %lu Dropped %lu packets\n", gb_recv_ps, s_1sec, p_drop_curr);

    sleep(1);
  }
}

