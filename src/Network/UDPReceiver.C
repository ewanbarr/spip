/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/AsciiHeader.h"
#include "spip/UDPReceiver.h"
#include "sys/time.h"


#include <iostream>
#include <stdexcept>
#include <new>

using namespace std;

spip::UDPReceiver::UDPReceiver()
{
  format = 0;
  verbose = 0;

#ifdef HAVE_VMA
  vma_api = vma_get_api();
  if (!vma_api)
    cerr << "spip::UDPReceiver::UDPReceiver VMA support compiled, but VMA not available" << endl;
  pkts = NULL;
#else
  vma_api = 0;
#endif

}

spip::UDPReceiver::~UDPReceiver()
{
  delete format;
}

int spip::UDPReceiver::configure (const char * header)
{
  if (spip::AsciiHeader::header_get (header, "NCHAN", "%u", &nchan) != 1)
    throw invalid_argument ("NCHAN did not exist in header");

  if (spip::AsciiHeader::header_get (header, "NBIT", "%u", &nbit) != 1)
    throw invalid_argument ("NBIT did not exist in header");

  if (spip::AsciiHeader::header_get (header, "NPOL", "%u", &npol) != 1)
    throw invalid_argument ("NPOL did not exist in header");

  if (spip::AsciiHeader::header_get (header, "NDIM", "%u", &ndim) != 1)
    throw invalid_argument ("NDIM did not exist in header");

  if (spip::AsciiHeader::header_get (header, "TSAMP", "%f", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in header");

  if (spip::AsciiHeader::header_get (header, "BW", "%f", &bw) != 1)
    throw invalid_argument ("BW did not exist in header");

  channel_bw = bw / nchan;

  bits_per_second  = (nchan * npol * ndim * nbit * 1000000) / tsamp;
  bytes_per_second = bits_per_second / 8;
}

void spip::UDPReceiver::prepare (std::string ip_address, int port)
{
  if (verbose)
    cerr << "spip::UDPReceiver::prepare()" << endl;

  // create and open a UDP receiving socket
  sock = new UDPSocketReceive ();

  sock->open (ip_address, port);
  
  sock->set_block ();

  sock->resize (format->get_header_size() + format->get_data_size());

  // this should not be required when using VMA offloading
  sock->resize_kernel_buffer (64*1024*1024);

  stats = new UDPStats (format->get_header_size(), format->get_data_size());
  if (verbose)
    cerr << "spip::UDPReceiver::prepare finished" << endl;
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
  if (verbose)
    cerr << "spip::UDPReceiver::receive()" << endl;

  int fd = sock->get_fd();
  char * buf = sock->get_buf();
  size_t sock_bufsz = sock->get_bufsz();

  int bytes_received, bytes_dropped;

  bool keep_receiving = true;
  bool have_packet = false;
  size_t got;
  uint64_t nsleeps = 0;

  struct sockaddr_in client_addr;
  struct sockaddr * addr = (struct sockaddr *) &client_addr;
  socklen_t addr_size = sizeof(struct sockaddr);

#ifdef HAVE_VMA
  int flags;
#endif

  while (keep_receiving)
  {
    if (vma_api)
    {
#ifdef HAVE_VMA
      if (pkts)
      {
        vma_api->free_packets(fd, pkts->pkts, pkts->n_packet_num);
        pkts = NULL;
      }
      while (!have_packet && keep_receiving)
      {
        flags = 0;
        got = (int) vma_api->recvfrom_zcopy(fd, buf, sock_bufsz, &flags, addr, &addr_size);
        if (got  > 32)
        {
          if (flags & MSG_VMA_ZCOPY)
          {
            pkts = (struct vma_packets_t*) buf;
            struct vma_packet_t *pkt = &pkts->pkts[0];
            buf = (char *) pkt->iov[0].iov_base;
          }
          have_packet = true;
        }
        else if (got == -1)
        {
          nsleeps++;
          if (nsleeps > 1000)
          {
            stats->sleeps(1000);
            nsleeps -= 1000;
          }
        }
        else
        {
          cerr << "control_state = Stopping VMA" << endl;
          keep_receiving = false;
        }
      }
#endif
    }
    else
    {
      while (!have_packet && keep_receiving)
      {
        got = (int) recvfrom (fd, buf, sock_bufsz, 0, addr, &addr_size);
        if (got > 32)
        {
          have_packet = true;
        }
        else if (got == -1)
        {
          nsleeps++;
          if (nsleeps > 1000)
          {
            stats->sleeps(1000);
            nsleeps -= 1000;
          }
        }
        else
        {
          cerr << "spip::UDPReceiver::receive error expected " << sock_bufsz
               << " B, received " << got << " B" <<  endl;
          keep_receiving = false;
        }
      }
    }

    if (have_packet)
    {
      // decode the header so that the format knows what to do with the packet
      bytes_received = format->decode_header (buf);
      
      // check to see if any bytes have been dropped, based on the reception of this packet
      bytes_dropped = format->check_packet ();
      
      stats->increment_bytes (bytes_received);
      stats->dropped_bytes (bytes_dropped);

      if (bytes_dropped && verbose)
      {
        cerr << "DROP: " << bytes_dropped << " B" << endl;
        if (verbose > 1)
          format->print_packet_header ();
      }

      stats->sleeps(nsleeps);
      have_packet = false;
    }
  }
}

