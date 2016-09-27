/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/Time.h"
#include "spip/AsciiHeader.h"
#include "spip/UDPReceiver.h"
#include "sys/time.h"

#include <unistd.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <new>

using namespace std;

spip::UDPReceiver::UDPReceiver()
{
  keep_receiving = true;
  have_utc_start = false;
  format = NULL;
  verbose = 1;

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
  if (format)
  {
    format->conclude();
    delete format;
  }
}

void spip::UDPReceiver::configure (const char * config_str)
{
  header.load_from_str (config_str);

  if (header.get ( "NCHAN", "%u", &nchan) != 1)
    throw invalid_argument ("NCHAN did not exist in header");

  if (header.get ( "NBIT", "%u", &nbit) != 1)
    throw invalid_argument ("NBIT did not exist in header");

  if (header.get ( "NPOL", "%u", &npol) != 1)
    throw invalid_argument ("NPOL did not exist in header");

  if (header.get ( "NDIM", "%u", &ndim) != 1)
    throw invalid_argument ("NDIM did not exist in header");

  if (header.get ( "TSAMP", "%lf", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in header");

  if (header.get ( "BW", "%f", &bw) != 1)
    throw invalid_argument ("BW did not exist in header");

  char * buffer = (char *) malloc (128);
  if (header.get ("DATA_HOST", "%s", buffer) != 1)
    throw invalid_argument ("DATA_HOST did not exist in header");
  data_host = string (buffer);
  if (header.get ("DATA_MCAST", "%s", buffer) != 1)
    data_mcast = string ();
  else
    data_mcast = string (buffer);
  if (header.get ("DATA_PORT", "%d", &data_port) != 1)
    throw invalid_argument ("DATA_PORT did not exist in header");
  free (buffer);

  if (verbose)
    cerr << "spip::UDPReceiver::configure receiving on " 
         << data_host << ":" << data_port  << endl;

  uint64_t mega = 1e6;
  bits_per_second = (mega * nchan * npol * ndim * nbit) / tsamp;
  bytes_per_second = bits_per_second / 8;

  if (!format)
    throw runtime_error ("format was not allocated");
  format->configure (header, "");
}

void spip::UDPReceiver::prepare ()
{
  if (verbose)
    cerr << "spip::UDPReceiver::prepare()" << endl;

  // create and open a UDP receiving socket
  sock = new UDPSocketReceive ();

  if (data_mcast.size() > 0)
  {
    cerr << "spip::UDPReceiver::prepare sock->open_multicast (" << data_host << ", " << data_mcast << ", " << data_port << ")" << endl;
    sock->open_multicast (data_host, data_mcast, data_port);
  }
  else
  {
    if (verbose)
      cerr << "spip::UDPReceiver::prepare sock->open(" << data_host << ", " << data_port << ")" << endl;
    sock->open (data_host, data_port);
  }
  
  if (vma_api)
    sock->set_block ();
  else
    sock->set_nonblock ();

  size_t sock_size = format->get_header_size() + format->get_data_size();
  if (verbose)
    cerr << "spip::UDPReceiver::prepare sock->resize(" << sock_size << ")" << endl;
  sock->resize (sock_size);
  sock->resize_kernel_buffer (64*1024*1024);

  stats = new UDPStats (format->get_header_size(), format->get_data_size());

  // if this format is not self starting, check for the UTC_START
  if (!format->get_self_start ())
  {
    // check if UTC_START has been set
    char * buffer = (char *) malloc (128);
    if (header.get ("UTC_START", "%s", buffer) == -1)
    {
      cerr << "spip::UDPReceiver::open no UTC_START in header" << endl;
      time_t now = time(0);
      spip::Time utc_start (now);
      utc_start.add_seconds (2);
      std::string utc_str = utc_start.get_gmtime();
      cerr << "spip::UDPReceiver::open UTC_START=" << utc_str  << endl;
      if (header.set ("UTC_START", "%s", utc_str.c_str()) < 0)
        throw invalid_argument ("failed to write UTC_START to header");
    }
    have_utc_start = true;
  }

  format->prepare (header, "");
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
  char * buf_ptr = buf;
  size_t sock_bufsz = sock->get_bufsz();

  bool have_packet = false;
  int got;
  uint64_t nsleeps = 0;

  struct sockaddr_in client_addr;
  struct sockaddr * addr = (struct sockaddr *) &client_addr;
  socklen_t addr_size = sizeof(struct sockaddr);

  // virtual block
  size_t data_bufsz = 32768l * nchan * ndim * npol;
  char * block = (char *) malloc (data_bufsz);
  bool need_next_block = false;

  // block accounting 
  int64_t curr_byte_offset = 0;
  int64_t next_byte_offset = data_bufsz;

#ifdef _DEBUG
  cerr << "spip::UDPReceiver::receive [" << curr_byte_offset << " - "
       << next_byte_offset << "] (" << 0 << ")" << endl;
#endif

  // overflow buffer
  const int64_t overflow_bufsz = 2097152;
  int64_t overflow_lastbyte = 0;
  int64_t overflow_maxbyte = next_byte_offset + overflow_bufsz;
  int64_t overflowed_bytes = 0;
  char * overflow = (char *) malloc(overflow_bufsz);
  memset (overflow, 0, overflow_bufsz);

  int64_t bytes_this_buf = 0;
  int64_t byte_offset;
  unsigned bytes_received;

#ifdef HAVE_VMA
  int flags;
#endif

  if (verbose)
    cerr << "spip::UDPReceiver::receive starting main loop" << endl;

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
        got = vma_api->recvfrom_zcopy(fd, buf, sock_bufsz, &flags, addr, &addr_size);
        if (got  > 32)
        {
          if ((flags & MSG_VMA_ZCOPY) == MSG_VMA_ZCOPY)
          {
            pkts = (vma_packets_t*) buf;
            int npkts = pkts->n_packet_num;
            struct vma_packet_t *pkt = &pkts->pkts[0];
            buf_ptr = (char *) pkt->iov[0].iov_base;
          }
          have_packet = true;
        }
        // since we are blocking on UDP socket
        else if (got == -1)
        {
          keep_receiving = false;
        }
        else
        {
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
      if (need_next_block)
      {
        need_next_block = false;

        // update absolute limits
        curr_byte_offset = next_byte_offset;
        next_byte_offset += data_bufsz;

#ifdef _DEBUG
        cerr << "spip::UDPReceiver::receive [" << curr_byte_offset << " - "
             << next_byte_offset << "] (" << bytes_this_buf << ")" << endl;
#endif

        overflow_maxbyte = next_byte_offset + overflow_bufsz;

        if (overflow_lastbyte > 0)
        {
          memcpy (block, overflow, overflow_lastbyte);
          overflow_lastbyte = 0;
          bytes_this_buf = overflowed_bytes;
          stats->increment_bytes (overflowed_bytes);
          overflowed_bytes = 0;
        }
        else
          bytes_this_buf = 0;
      }

      // decode the header so that the format knows what to do with the packet
      byte_offset = format->decode_packet (buf_ptr, &bytes_received);

      // if we do not yet have a UTC start, get it from the format
      if (!have_utc_start)
      {
        Time utc_start = format->get_utc_start ();
        uint64_t pico_seconds = format->get_pico_seconds();
        have_utc_start = true;
      }

      // packet belongs in current buffer
      if ((byte_offset >= curr_byte_offset) && (byte_offset < next_byte_offset))
      {
        stats->increment_bytes (bytes_received);
        format->insert_last_packet (block + (byte_offset - curr_byte_offset));
        bytes_this_buf += bytes_received;
        have_packet = false;
      }
      else if ((byte_offset >= next_byte_offset) && (byte_offset < overflow_maxbyte))
      {
        format->insert_last_packet (overflow + (byte_offset - next_byte_offset));
        overflow_lastbyte = std::max((byte_offset - next_byte_offset) + bytes_received, overflow_lastbyte);
        overflowed_bytes += bytes_received;
        have_packet = false;
      }
      else if (byte_offset < curr_byte_offset)
      {
        // ignore
        have_packet = false;
      }
      else
      {
#ifdef _DEBUG
        cerr << "ELSE byte_offset=" << byte_offset << " [" << curr_byte_offset <<" - " << next_byte_offset << " - " << overflow_maxbyte << "] bytes_received=" << bytes_received << " bytes_this_buf=" << bytes_this_buf << endl; 
#endif
        need_next_block = true;
        have_packet = true;
      }

      if (bytes_this_buf >= data_bufsz || need_next_block)
      {
        stats->dropped_bytes (data_bufsz - bytes_this_buf);
        need_next_block = true;
      }
    }
  }
}

void spip::UDPReceiver::stop_receiving ()
{
  keep_receiving = false;
  if (format)
    format->conclude();
}
