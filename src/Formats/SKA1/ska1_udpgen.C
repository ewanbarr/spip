/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "dada_def.h"
#include "futils.h"

#include "spip/CustomUDPGenerator.h"

#include <pthread.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

#include <iostream>

void usage();
void * stats_thread (void * arg);
char quit_threads = 0;

using namespace std;

int main(int argc, char *argv[])
{
  spip::UDPGenerator * gen = 0;

  char * header_file = 0;

  char * src_host = 0;

  // Hostname to send UDP packets to
  char * dest_host;

  // udp port to send data to
  int dest_port = SKA1_DEFAULT_UDP_PORT;

  // total time to transmit for
  unsigned transmission_time = 5;   

  // data rate at which to transmit
  float data_rate_mbits = 64;

  // core on which to bind thread operations
  int core = -1;

  int verbose = 0;

  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "b:f:hn:p:r:v")) != EOF) 
  {
    switch(c) 
    {
      case 'b':
        core = atoi(optarg);
        break;

      case 'f':
        if (strcmp(optarg, "custom") == 0)
          gen = (spip::UDPGenerator *) new spip::CustomUDPGenerator;
        else
        {
          cerr << "ERROR: format " << optarg << " not supported" << endl;
          return (EXIT_FAILURE);
        }
        break;

      case 'h':
        usage();
        exit(EXIT_SUCCESS);
        break;

      case 'n':
        transmission_time = atoi(optarg);
        break;

      case 'p':
        dest_port = atoi(optarg);
        break;

      case 'r':
        data_rate_mbits = atof(optarg);
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

  if (verbose)
    cerr << "ska1_udpgen: parsed command line options" << endl;

  // Check arguments
  if ((argc - optind) != 2) 
  {
    fprintf(stderr,"ERROR: 2 command line arguments expected\n");
    usage();
    return EXIT_FAILURE;
  }

  // header the this data stream
  header_file = strdup (argv[optind]);

  // destination host
  dest_host = strdup(argv[optind+1]);

  char * header = (char *) malloc (sizeof(char) * DADA_DEFAULT_HEADER_SIZE);
  if (header_file == NULL)
  {
    cerr << "ERROR: could not allocate memory for header buffer" << endl;
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "ska1_udpgen: reading header from " << header_file << endl;
  if (fileread (header_file, header, DADA_DEFAULT_HEADER_SIZE) < 0)
  {
    free (header);
    cerr << "ERROR: could not read ASCII header from " << header_file << endl;
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "ska1_udpgen: configuring based on header" << endl;
  gen->configure (header);

  if (verbose)
    cerr << "ska1_udpgen: allocating signal" << endl;
  gen->allocate_signal ();

  if (verbose)
    cerr << "ska1_udpgen: preparing for transmission to " << dest_host << ":" << dest_port << endl;
  gen->prepare (std::string(dest_host), dest_port);

  if (verbose)
    cerr << "ska1_udpgen: starting stats thread" << endl;
  pthread_t stats_thread_id;
  int rval = pthread_create (&stats_thread_id, 0, stats_thread, (void *) gen);
  if (rval != 0)
  {
    cerr << "ska1_udpgen: failed to start stats thread" << endl;
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "ska1_udpgen: transmitting for " << transmission_time << " secoonds at " << data_rate_mbits << " mb/s" << endl;
  gen->transmit (transmission_time, data_rate_mbits * 1000000);

  quit_threads = 1;

  if (verbose)
    cerr << "ska1_udpgen: joining stats_thread" << endl;
  void * result;
  pthread_join (stats_thread_id, &result);

  delete gen;

  return 0;
}

void usage() 
{
  cout << "ska1_udpgen [options] header host\n"
    "  header      ascii file contain header\n"
    "  host        hostname/ip of UDP receiver\n"
    "  -f format   generate UDP data of format [custom spead vdif]\n"
    "  -b core     bind computation to specified CPU core\n"
    "  -h          print this help text\n"
    "  -n secs     number of seconds to transmit [default 5]\n"
    "  -p port     destination udp port [default " << SKA1_DEFAULT_UDP_PORT << "]\n"
    "  -r rate     transmit at rate Mib/s [default 10]\n"
    "  -v          verbose output\n"
    << endl;
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

