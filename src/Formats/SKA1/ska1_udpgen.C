/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "dada_def.h"
#include "futils.h"

#include "spip/CustomUDPGenerator.h"

#include <unistd.h>
#include <cstdio>
#include <cstring>

#include <iostream>

#define SKA1_DEFAULT_UDP_PORT 4003

void usage();

using namespace std;

int main(int argc, char *argv[])
{
  spip::CustomUDPGenerator * custom = 0;

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
          custom = new spip::CustomUDPGenerator;
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
    fprintf (stderr, "ERROR: could not allocate memory for header buffer\n");
    return (EXIT_FAILURE);
  }

  if (verbose)
    cerr << "ska1_udpgen: reading header from " << header_file << endl;
  if (fileread (header_file, header, DADA_DEFAULT_HEADER_SIZE) < 0)
  {
    free (header);
    fprintf (stderr, "ERROR: could not read ASCII header from %s\n", header_file);
    return (EXIT_FAILURE);
  }

  if (custom)
  {
    if (verbose)
      cerr << "ska1_udpgen: configuring based on header" << endl;
    custom->configure (header);
    if (verbose)
      cerr << "ska1_udpgen: allocating signal" << endl;
    custom->allocate_signal ();
    if (verbose)
      cerr << "ska1_udpgen: preparing for transmission to " << dest_host << ":" << dest_port << endl;
    custom->prepare (std::string(dest_host), dest_port);
    if (verbose)
      cerr << "ska1_udpgen: transmitting" << endl;
    custom->transmit (transmission_time, data_rate_mbits * 1000000);
  }

  cerr << "ska1_udpgen: deleting custom" << endl;

  delete custom;

  return 0;
}

void usage() 
{
  cout << "ska1_udpgen_custom [options] header host\n"
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

