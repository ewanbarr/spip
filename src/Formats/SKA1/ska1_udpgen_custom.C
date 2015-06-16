/***************************************************************************
 *
 *    Copyright (C) 2015 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include <stdlib.h> 
#include <stdio.h> 
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <netdb.h> 
#include <sys/socket.h> 
#include <sys/wait.h> 
#include <sys/timeb.h> 
#include <math.h>
#include <pthread.h>
#include <assert.h>

#include "daemon.h"
#include "multilog.h"
#include "ska1_def.h"
#include "ska1_udp.h"

#include "spip/UDPGenerator.h"

void signal_handler(int signalValue);
void usage();

using namespace std;

static char* args = "b:B:cI:no:prt:f:hxk:vVD:";

int main(int argc, char *argv[])
{
  UDPGenerator * udpgen 

  char * header_file = 0;

  char * src_host = 0;

  // udp port to send data to
  int dest_port = SKA1_DEFAULT_UDPDB_PORT;

  // UDP socket struct
  struct sockaddr_in dagram;

  // total time to transmit for
  uint64_t transmission_time = 5;   

  // DADA logger
  multilog_t *log = 0;

  // Hostname to send UDP packets to
  char * dest_host;

  // udp file descriptor
  int udpfd;

  // UDP packet for transmission [contents irrelevant]
  char packet[UDP_PAYLOAD];

  // data rate
  float data_rate_mbits = 10;

  // number of packets to send every second
  uint64_t packets_ps = 0;

  // packet sequence number
  uint64_t seq_no = 0;

  // antenna Identifier
  uint16_t ant_id;

  int core = -1;

  opterr = 0;
  int c;
  while ((c = getopt(argc, argv, "b:hf:n:p:r:v")) != EOF) 
  {
    switch(c) 
    {
      case 'b':
        core = atoi(optarg);
        break;

      case 'h':
        usage();
        exit(EXIT_SUCCESS);
        break;

      case 'n':
        transmission_time = atoi(optarg);
        break;

      case 'f':
        src_host = strdup (optarg);
        fprintf (stderr, "optarg=%s\n", optarg);
        fprintf (stderr, "src_host=%s\n", src_host);
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

  // Check arguments
  if ((argc - optind) != 2) 
  {
    fprintf(stderr,"ERROR: 2 command line arguments expected\n");
    usage();
    return EXIT_FAILURE;
  }

  // header the this data stream
  header_file = stdrup (argv[optind]);

  // destination host
  dest_host = strdup(argv[optind+1]);

  char * header = (char *) malloc (sizeof(char) * DADA_DEFAULT_HEADER_SIZE);
  if (header_file == NULL)
  {
    fprintf (stderr, "ERROR: could not allocate memory for header buffer\n");
    return (EXIT_FAILURE);
  }

  if (fileread (header_file, header, DADA_DEFAULT_HEADER_SIZE) < 0)
  {
    free (header);
    fprintf (stderr, "ERROR: could not read ASCII header from %s\n", header_file);
    return (EXIT_FAILURE);
  }

  parse_header (ctx, header);

  // create the UDP transmitting socket
  UDPSocketSend socket = new UDPSocketSend();
  socket.open (std::string(dest_host, dest_port);

  // 

  seq_no = 0;
  ant_id = 0;

  signal(SIGINT, signal_handler);

  // do not use the syslog facility
  log = multilog_open ("ska1_udpgen", 0);
  multilog_add(log, stderr);

  double data_rate_bytes_ps = (double) (data_rate_mbits / 8) * 1000000;
  multilog(log, LOG_INFO, "Rate: %f Mib/s -> %lf B/s\n", data_rate_mbits, data_rate_bytes_ps);

  if (core >= 0)
   if (dada_bind_thread_to_core(core) < 0)
     multilog(log, LOG_INFO, "Error binding to core: %d\n", core);


  if (verbose)
  {
     multilog(log, LOG_INFO, "sending UDP data from %s to %s:%d\n", src_host, dest_host, dest_port);
     if (data_rate_bytes_ps)
       multilog (log, LOG_INFO, "data rate: %5.2lf Mib/s \n", data_rate_mbits);
     else
      multilog(log, LOG_INFO, "data_rate: fast as possible\n");
    multilog(log, LOG_INFO, "transmission length: %d seconds\n", transmission_time);
  }

  // create the socket for outgoing UDP data
  dada_udp_sock_out (&udpfd, &dagram, dest_host, dest_port, 0, src_host);

  uint64_t data_counter = 0;

  // initialise data rate timing library 
  StopWatch wait_sw;
  RealTime_Initialise(1);
  StopWatch_Initialise(1);

  // If we have a desired data rate, then we need to adjust our sleep time
  // accordingly
  if (data_rate_bytes_ps > 0)
  {
    packets_ps = floor(((double) data_rate_bytes_ps) / ((double) UDP_DATA));
    sleep_time = (1.0 / packets_ps) * 1000000.0;

    if (verbose)
    {
      multilog(log, LOG_INFO, "packets/sec %"PRIu64"\n",packets_ps);
      multilog(log, LOG_INFO, "sleep_time %f us\n",sleep_time);
    }
  }

  // seed the random number generator with current time
  srand ( time(NULL) );

  uint64_t total_bytes_to_send = data_rate_bytes_ps * transmission_time;
  multilog(log, LOG_INFO, "bytes_to_send=%"PRIu64"\n", total_bytes_to_send);

  // assume 10GbE speeds
  if (data_rate_bytes_ps == 0)
  {
    total_bytes_to_send = 10 * transmission_time;
    total_bytes_to_send *= 1024*1024*1024;
  }

  size_t bytes_sent = 0;
  uint64_t total_bytes_sent = 0;

  uint64_t bytes_sent_thistime = 0;
  uint64_t prev_bytes_sent = 0;
  
  time_t current_time = time(0);
  time_t prev_time = time(0);

  multilog(log,LOG_INFO,"Total bytes to send = %"PRIu64"\n",total_bytes_to_send);
  multilog(log,LOG_INFO,"UDP payload = %"PRIu64" bytes\n",UDP_PAYLOAD);
  multilog(log,LOG_INFO,"UDP data size = %"PRIu64" bytes\n",UDP_DATA);
  multilog(log,LOG_INFO,"Wire Rate\tUseful Rate\tPacket\tSleep Time\n");

  unsigned int s_off = 0;

  size_t numbytes;
  size_t socksize = sizeof(struct sockaddr);
  struct sockaddr * addr = (struct sockaddr *) &dagram;

  while (total_bytes_sent < total_bytes_to_send) 
  {
    if (data_rate_bytes_ps)
      StopWatch_Start(&wait_sw);

    // write the custom header into the packet
    ska1_encode_header (packet, seq_no, ant_id);

    // aj inlineing stuff for performance
    bytes_sent = sendto (udpfd, packet, UDP_PAYLOAD, 0, addr, socksize);

    if (bytes_sent != UDP_PAYLOAD) 
      multilog(log,LOG_ERR,"Error. Attempted to send %d bytes, but only "
                           "%"PRIu64" bytes were sent\n",UDP_PAYLOAD,
                           bytes_sent);

    // this is how much useful data we actaully sent
    total_bytes_sent += (bytes_sent - UDP_HEADER);

    data_counter++;
    prev_time = current_time;
    current_time = time(0);
    
    if (prev_time != current_time) 
    {
      double complete_udp_packet = (double) bytes_sent;
      double useful_data_only = (double) (bytes_sent - UDP_HEADER);
      double complete_packet = 28.0 + complete_udp_packet;

      double wire_ratio = complete_packet / complete_udp_packet;
      double useful_ratio = useful_data_only / complete_udp_packet;
        
      uint64_t bytes_per_second = total_bytes_sent - prev_bytes_sent;
      prev_bytes_sent = total_bytes_sent;

      double rate = ((double) bytes_per_second * 8) / 1000000;
      double wire_rate = rate * wire_ratio;
      double useful_rate = rate * useful_ratio;
             
      multilog(log,LOG_INFO,"%5.2f Mib/s  %5.2f Mib/s  %"PRIu64
                            "  %5.2f, %"PRIu64"\n",
                            wire_rate, useful_rate, data_counter, sleep_time,
                            bytes_sent);
    }

    seq_no++;

    if (data_rate_bytes_ps)
      StopWatch_Delay(&wait_sw, sleep_time);
  }

  uint64_t packets_sent = seq_no;

  multilog(log, LOG_INFO, "Sent %"PRIu64" bytes\n",total_bytes_sent);
  multilog(log, LOG_INFO, "Sent %"PRIu64" packets\n",packets_sent);

  close(udpfd);
  free (dest_host);

  return 0;
}


void signal_handler(int signalValue) 
{
  exit(EXIT_SUCCESS);
}

void usage() 
{
  cout << "ska1_udpgen_custom [options] header host\n"
    "  header      ascii file contain header\n"
    "  host        hostname/ip of UDP receiver\n"
    "  -b core     bind computation to specified CPU core\n"
    "  -h          print this help text\n"
    "  -n secs     number of seconds to transmit [default 5]\n"
    "  -p port     destination udp port [default " << SKA1_DEFAULT_UDP_PORT << "]\n"
    "  -r rate     transmit at rate Mib/s [default 10]\n"
    "  -v          verbose output\n"
    << endl;
}

