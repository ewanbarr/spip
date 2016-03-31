/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPGenerator.h"
#include "spip/AsciiHeader.h"
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
  if (spip::AsciiHeader::header_get (config, "NCHAN", "%u", &nchan) != 1)
    throw invalid_argument ("NCHAN did not exist in header");

  if (spip::AsciiHeader::header_get (config, "NBIT", "%u", &nbit) != 1)
    throw invalid_argument ("NBIT did not exist in header");

  if (spip::AsciiHeader::header_get (config, "NPOL", "%u", &npol) != 1)
    throw invalid_argument ("NPOL did not exist in header");

  if (spip::AsciiHeader::header_get (config, "NDIM", "%u", &ndim) != 1)
    throw invalid_argument ("NDIM did not exist in header");

  if (spip::AsciiHeader::header_get (config, "TSAMP", "%f", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in header");

  if (spip::AsciiHeader::header_get (config, "BW", "%f", &bw) != 1)
    throw invalid_argument ("BW did not exist in header");

  channel_bw = bw / nchan;

  header.load_from_str (config);

  if (!format)
    throw runtime_error ("unable for prepare format");
  format->configure (header, "");
}

// allocate memory for 1 second of data for use in packet generation
void spip::UDPGenerator::allocate_signal()
{
  bits_per_second  = (nchan * npol * ndim * nbit * 1000000) / tsamp;
  bytes_per_second = bits_per_second / 8;

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

void spip::UDPGenerator::prepare (std::string ip_address, int port)
{
  // create and open a UDP sending socket
  sock = new UDPSocketSend();
  sock->open (ip_address, port);
  
  unsigned header_size = format->get_header_size();
  unsigned data_size   = format->get_data_size();

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

  cerr << "spip::UDPGenerator::transmit bytes_per_second=" << bytes_per_second << endl;
  cerr << "spip::UDPGenerator::transmit bytes_to_send=" << bytes_to_send << endl;
  cerr << "spip::UDPGenerator::transmit packets_to_send=" << packets_to_send<< endl;
  cerr << "spip::UDPGenerator::transmit packets_per_second=" << packets_per_second<< endl;
  cerr << "spip::UDPGenerator::transmit sleep_time=" << sleep_time<< endl;
  cerr << "spip::UDPGenerator::transmit start_second=" << start_second<< endl;

  keep_transmitting = true;

  const uint64_t payload_size = format->get_data_size();

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

