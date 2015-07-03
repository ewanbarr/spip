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
  //format = new UDPFormat();
  format = 0;
}

spip::UDPReceiver::~UDPReceiver()
{
  delete format;
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
  
  sock->set_block ();

  sock->resize (format->get_header_size() + format->get_data_size());

  // this should not be required when using VMA offloading
  //sock->resize_kernel_buffer (4*1024*1024);

  stats = new UDPStats (format->get_header_size(), format->get_data_size());
}

void spip::UDPReceiver::set_format (spip::UDPFormat * fmt)
{
  if (format)
    delete format;
  format = fmt;
}

// receive UDP packets for the specified time at the specified data rate
void spip::UDPReceiver::receive ()
{
  uint64_t packet_number = 0;
  int64_t prev_packet_number = -1;

  int fd = sock->get_fd();
  char * buf = sock->get_buf();
  size_t bufsz = sock->get_bufsz();

  uint64_t total_bytes_recvd = 0;

  char have_packet;
  size_t got;
  char cont = 1;
  uint64_t nsleeps;

  socklen_t size = sizeof(struct sockaddr);
  struct sockaddr_in client_addr;

  while (cont)
  {
    have_packet = 0;
    nsleeps = 0;
    while (!have_packet && cont)
    {
      got = recvfrom (fd, buf, bufsz, 0, (struct sockaddr *)&client_addr, &size);
      if (got == bufsz)
        have_packet = 1;
      else if (got == -1)
      {
        nsleeps++;
        if (nsleeps > 1000)
        {
          stats->sleeps(nsleeps);
          nsleeps = 0;
        }
      }
      else
      {
        cerr << "spip::UDPReceiver::receive recvfrom failed got=" << got << endl;
        cont = 0;
      }
    }

    format->decode_header (buf, bufsz, &packet_number);

    if (packet_number == (1 + prev_packet_number))
      stats->increment();
    else
    {
      stats->dropped();
    }

    stats->sleeps(nsleeps);

    prev_packet_number = packet_number;
    total_bytes_recvd += format->get_data_size();
  }
}

