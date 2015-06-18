/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPReceiver.h"
#include "sys/time.h"

#include "ascii_header.h"

#include <iostream>
#include <stdexcept>
#include <new>

using namespace std;

spip::UDPReceiver::UDPReceiver()
{
}

spip::UDPReceiver::~UDPReceiver()
{
}

int spip::UDPReceiver::configure (const char * header)
{
  if (ascii_header_get (header, "NCHAN", "%u", &nchan) != 1)
    throw invalid_argument ("NCHAN did not exist in header");

  if (ascii_header_get (header, "NBIT", "%u", &nbit) != 1)
    throw invalid_argument ("NBIT did not exist in header");

  if (ascii_header_get (header, "NPOL", "%u", &npol) != 1)
    throw invalid_argument ("NPOL did not exist in header");

  if (ascii_header_get (header, "NDIM", "%u", &ndim) != 1)
    throw invalid_argument ("NDIM did not exist in header");

  if (ascii_header_get (header, "TSAMP", "%f", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in header");

  if (ascii_header_get (header, "BW", "%f", &bw) != 1)
    throw invalid_argument ("BW did not exist in header");

  channel_bw = bw / nchan;

  bits_per_second  = (nchan * npol * ndim * nbit * 1000000) / tsamp;
  bytes_per_second = bits_per_second / 8;
}

void spip::UDPReceiver::prepare (std::string ip_address, int port)
{
  // create and open a UDP receiving socket
  sock = new UDPSocketReceive ();
  sock->open (ip_address, port);
  sock->resize (packet_header_size + packet_data_size);

  stats = new UDPStats (packet_header_size, packet_data_size);
}

// receive UDP packets for the specified time at the specified data rate
void spip::UDPReceiver::receive ()
{
  uint64_t packet_number = 0;
  uint64_t prev_packet_number = 0;

  int fd = sock->get_fd();
  void * buf = sock->get_buf();
  size_t bufsz = sock->get_bufsz();

  cerr << "spip::UDPReceiver::receive buf=" << buf << " bufsz=" << bufsz << endl;

  uint64_t total_bytes_recvd = 0;

  char have_packet;
  size_t got;
  char cont = 1;
  uint64_t nsleeps = 0;

  while (cont)
  {
    have_packet = 0;
    while (!have_packet && cont)
    {
      got = recvfrom (fd, buf, bufsz, 0, NULL, NULL);
      if (got == bufsz)
        have_packet = 1;
      else if (got == -1)
        nsleeps++;
      else
      {
        cerr << "spip::UDPReceiver::receive recvfrom failed got=" << got << endl;
        cont = 0;
      }
    }

    decode_header (buf, bufsz, &packet_number);

    if (packet_number && packet_number == prev_packet_number + 1)
      stats->increment();
    else
    {
      //cerr << "expected " << prev_packet_number + 1 << " got " << packet_number << endl;
      stats->dropped();
    }
    prev_packet_number = packet_number;

    total_bytes_recvd += packet_data_size;
    packet_number++;
  }

  cerr << "spip::UDPReceiver::receive transmission done!" << endl;  
}

