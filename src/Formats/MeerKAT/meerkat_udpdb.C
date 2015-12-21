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

#include "spip/HardwareAffinity.h"
#include "spip/UDPReceiveDB.h"
#include "spip/UDPFormatMeerKATSimple.h"
#include "spip/TCPSocketServer.h"

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

  string * format = new string("simple");

  char * config_file = 0;

  // Host/IP to receive packets on 
  char * host;

  // udp port to send data to
  int port = MEERKAT_DEFAULT_UDP_PORT;

  // tcp control port to receive configuration
  int control_port = -1;

  // control socket for the control port
  spip::TCPSocketServer * ctrl_sock = 0;

  spip::HardwareAffinity hw_affinity;

  int verbose = 0;

  opterr = 0;
  int c;

  int core;

  while ((c = getopt(argc, argv, "b:c:f:hk:p:v")) != EOF) 
  {
    switch(c) 
    {
      case 'b':
        core = atoi(optarg);
        hw_affinity.bind_to_cpu_core (core);
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

  // create a UDP recevier that writes to a data block
  udpdb = new spip::UDPReceiveDB (key.c_str());

  if (format->compare("simple") == 0)
    udpdb->set_format (new spip::UDPFormatMeerKATSimple());
  else
  {
    cerr << "ERROR: unrecognized UDP format [" << format << "]" << endl;
    delete udpdb;
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

  // header the this data stream
  config_file = strdup (argv[optind]);

  // local address/host to listen on
  host = strdup(argv[optind+1]);

  char * config = (char *) malloc (DADA_DEFAULT_HEADER_SIZE);
  if (config == NULL)
  {
    fprintf (stderr, "ERROR: could not allocate memory for config buffer\n");
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "meerkat_udpdb: reading config from " << config_file << endl;
  if (fileread (config_file, config, DADA_DEFAULT_HEADER_SIZE) < 0)
  {
    free (config);
    fprintf (stderr, "ERROR: could not read config from %s\n", config_file);
    return (EXIT_FAILURE);
  }

  uint64_t data_bufsz = udpdb->get_data_bufsz();
  if (ascii_header_set (config, "RESOLUTION", "%lu", data_bufsz) < 0)
  {
    free (config);
    fprintf (stderr, "ERROR: could not write RESOLUTION=%lu to config\n", data_bufsz);
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "meerkat_udpdb: configuring using fixed config" << endl;
  udpdb->configure (config);

  if (verbose)
    cerr << "meerkat_udpdb: listening for UDP packets on " 
         << host << ":" << port << endl;
  udpdb->prepare (std::string(host), port);

  // prepare a header which combines config with observation parameters
  char * header = (char *) malloc (DADA_DEFAULT_HEADER_SIZE);
  if (header == NULL)
  {
    fprintf (stderr, "ERROR: could not allocate memory for header buffer\n");
    return (EXIT_FAILURE);
  }

  // assume that the config includes a header param
  strncpy (header, config, strlen(config) + 1);

  // open a listening socket for observation parameters
  if (control_port > 0)
  {
    cerr << "meerkat_udpdb: start_control_thread (" << control_port << ")" << endl;
    udpdb->start_control_thread (control_port);

/*
    ctrl_sock = new spip::TCPSocketServer();

    // open a listen sock on all interfaces for the control port
    if (verbose)
      cerr << "meerkat_udpdb: opening control socket on any:" << control_port << endl;
    ctrl_sock->open ("any", control_port, 1);

    int fd = -1;

    // wait for a connection
    while (!quit_threads && fd < 0)
    {
      if (verbose)
        cerr << "meerkat_udpdb: ctrl_sock->accept(1)" << endl;
      // try to accept with a 1 second timeout
      fd = ctrl_sock->accept_client (1);
    }

    if (!quit_threads && fd > 0 )
    {
      char * obs = (char *) malloc (DADA_DEFAULT_HEADER_SIZE);
      ssize_t bytes_read = read (fd, obs, DADA_DEFAULT_HEADER_SIZE);
      if (verbose)
        cerr << "meerkat_udpdb: bytes_read=" << bytes_read << endl;
      cerr << "meerkat_udpdb: obs=" << obs << endl;
      ctrl_sock->close_me ();
      strcat (header, obs);
    }
*/
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
    udpdb->open (header);

    cerr << "meerkat_udpdb: issuing start command" << endl;
    udpdb->start_capture ();

    cerr << "meerkat_udpdb: calling receive" << endl;
    udpdb->receive ();
    
  }


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
  cout << "meerkat_udpdb [options] config host\n"
    "  config      ascii file containing fixed configuration\n"
    "  host        hostname/ip of UDP receiver\n"
    "  -b core     bind computation to specified CPU core\n"
    "  -c port     control port for dynamic configuration\n"
    "  -f format   UDP data format [meerkat]\n"
    "  -h          print this help text\n"
    "  -k key      PSRDada shared memory key to write to [default " << std::hex << DADA_DEFAULT_BLOCK_KEY << "]\n"
    "  -p port     incoming udp port [default " << std::dec << MEERKAT_DEFAULT_UDP_PORT << "]\n"
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
void * stats_thread (void * arg)
{
  spip::UDPReceiveDB * recv = (spip::UDPReceiveDB *) arg;

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
    b_recv_curr = recv->get_stats()->get_data_transmitted();
    p_drop_curr = recv->get_stats()->get_packets_dropped();
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
    fprintf (stderr,"Recv %6.3f [Gb/s] Sleeps %lu Dropped %lu packets\n", gb_recv_ps, s_1sec, p_drop_curr);

    sleep(1);
  }
}
 */

