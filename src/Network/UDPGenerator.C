/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPGenerator.h"
#include "spip/Time.h"
#include "sys/time.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <new>

using namespace std;

spip::UDPGenerator::UDPGenerator()
{
  signal_buffer = 0;
  signal_buffer_size = 0;

  format = 0;
  keep_transmitting = true;
}

spip::UDPGenerator::~UDPGenerator()
{
  if (signal_buffer)
    free (signal_buffer);
  signal_buffer = 0;

  if (sock)
    delete sock;

  if (stats)
    delete stats;

  if (format)
    delete format;
}

int spip::UDPGenerator::configure (const char * config)
{
  // save the header for use later
  header.load_from_str (config);

  if (header.get ("NCHAN", "%u", &nchan) != 1)
    throw invalid_argument ("NCHAN did not exist in config");

  if (header.get ("NBIT", "%u", &nbit) != 1)
    throw invalid_argument ("NBIT did not exist in config");

  if (header.get ("NPOL", "%u", &npol) != 1)
    throw invalid_argument ("NPOL did not exist in config");

  if (header.get ("NDIM", "%u", &ndim) != 1)
    throw invalid_argument ("NDIM did not exist in config");

  if (header.get ("TSAMP", "%f", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in config");

  if (header.get ("BW", "%f", &bw) != 1)
    throw invalid_argument ("BW did not exist in config");

  char * buffer = (char *) malloc (128);
  if (header.get ("DATA_HOST", "%s", buffer) != 1)
    throw invalid_argument ("DATA_HOST did not exist in config");
  data_host = string (buffer);
  if (header.get ("DATA_PORT", "%d", &data_port) != 1)
    throw invalid_argument ("DATA_PORT did not exist in config");
  free (buffer);

  cerr << "UDP dest " << data_host << ":" << data_port << endl;

  bits_per_second  = (nchan * npol * ndim * nbit * 1000000) / tsamp;
  bytes_per_second = bits_per_second / 8;

  if (!format)
    throw runtime_error ("unable to configure format");
  format->configure (header, "");


}

// allocate memory for 1 second of data for use in packet generation
void spip::UDPGenerator::allocate_signal()
{
  if (bytes_per_second > signal_buffer_size)
  {
    if (signal_buffer_size)
      signal_buffer = malloc (bytes_per_second);
    else
      signal_buffer = realloc (signal_buffer, bytes_per_second);
    signal_buffer_size = bytes_per_second;

    if (!signal_buffer)
      throw invalid_argument ("unabled to allocate 1 second of data");

    memset (signal_buffer, 0, signal_buffer_size);
  }
}

void spip::UDPGenerator::set_format (UDPFormat * fmt)
{
  if (format)
    delete format;
  format = fmt;
}

void spip::UDPGenerator::prepare ()
{
  // create and open a UDP sending socket
  sock = new UDPSocketSend();
  sock->open (data_host, data_port);

  char * buffer = (char *) malloc (128);
  if (header.get ("UTC_START", "%s", buffer) == -1)
  {
    cerr << "spip::UDPGenerator::prepare no UTC_START in header" << endl;
    time_t now = time(0);
    spip::Time utc_start (now);
    utc_start.add_seconds (2);
    std::string utc_str = utc_start.get_gmtime();
    cerr << "spip::UDPGenerator::open UTC_START=" << utc_str  << endl;
    if (header.set ("UTC_START", "%s", utc_str.c_str()) < 0)
      throw invalid_argument ("failed to write UTC_START to header");
  }

  format->prepare (header, "");
  
  unsigned header_size = format->get_header_size();
  unsigned data_size   = format->get_data_size();

  cerr << "header_size=" << header_size << " data_size=" << data_size << endl;
  sock->resize (header_size + data_size);

  // initialize a stats class
  stats = new UDPStats(header_size, data_size);
}

// transmit UDP packets for the specified time at the specified data rate [b/s]
void spip::UDPGenerator::transmit (unsigned tobs, float data_rate) 
{
  cerr << "spip::UDPGenerator::transmit tobs=" << tobs << " data_rate=" 
       << data_rate << " bytes_per_second=" << bytes_per_second << endl;

  uint64_t bytes_to_send = tobs * 40e9;

  if (data_rate > 0)
    bytes_to_send = tobs * (data_rate/8);

  cerr << "spip::UDPGenerator::transmit bytes_to_send=" << bytes_to_send << endl;

  uint64_t packets_to_send = bytes_to_send / format->get_data_size();

  uint64_t packets_per_second = 0;
  double   sleep_time = 0;

  if (data_rate > 0)
  {
    packets_per_second = (uint64_t) ((data_rate/8) / (float) format->get_data_size());
    sleep_time = (1.0 / packets_per_second) * 1000000.0;
  }

  uint64_t packet_number = 0;

  char * buf = sock->get_buf();
  size_t bufsz = sock->get_bufsz();

  uint64_t total_bytes_sent = 0;

  struct timeval timestamp;
  time_t start_second;

  gettimeofday (&timestamp, 0);
  start_second = timestamp.tv_sec + 1;

  double micro_seconds = 0;
  double micro_seconds_elapsed = 0;

  // busy sleep until next 1pps tick
  while (timestamp.tv_sec < start_second)
    gettimeofday (&timestamp, 0);

  char wait = 1;

  const uint64_t payload_size = format->get_data_size();

  cerr << "spip::UDPGenerator::transmit bytes_per_second=" << bytes_per_second << endl;
  cerr << "spip::UDPGenerator::transmit bytes_to_send=" << bytes_to_send << endl;
  cerr << "spip::UDPGenerator::transmit packets_to_send=" << packets_to_send<< endl;
  cerr << "spip::UDPGenerator::transmit packets_per_second=" << packets_per_second<< endl;
  cerr << "spip::UDPGenerator::transmit sleep_time=" << sleep_time<< endl;
  cerr << "spip::UDPGenerator::transmit start_second=" << start_second<< endl;
  cerr << "spip::UDPGenerator::transmit payload_size=" << payload_size << endl;
  cerr << "spip::UDPGenerator::transmit bufsz=" << bufsz << endl;

  keep_transmitting = true;

  while (total_bytes_sent < bytes_to_send && keep_transmitting)
  {
    format->gen_packet (buf, bufsz);

    uint64_t bytes_sent = sock->send();

    stats->increment();

    // determine the desired time to wait un
    micro_seconds += sleep_time;

    if (data_rate)
    {
      wait = 1;
   
      while (wait && keep_transmitting)
      {
        gettimeofday (&timestamp, 0);
        micro_seconds_elapsed = (double) (((timestamp.tv_sec - start_second) * 1000000) + timestamp.tv_usec);

        if (micro_seconds_elapsed > micro_seconds)
          wait = 0;
      }
    }

    total_bytes_sent += payload_size;
  }

  cerr << "spip::UDPGenerator::transmit transmission done!" << endl;  

  cerr << "spip::UDPGenerator::transmit sending smaller packet!" << endl;
  format->gen_packet (buf, bufsz);
  sock->send (1024);
}

