/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPReceiveDB.h"
#include "sys/time.h"

#include "ascii_header.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <new>

#ifdef  USING_VMA_EXTRA_API
#include <mellanox/vma_extra.h>
#endif

using namespace std;

spip::UDPReceiveDB::UDPReceiveDB(const char * key_string)
{
  db = new DataBlockWrite (key_string);

  db->connect();
  db->lock();

  keep_receiving = true;
  format = 0;

#ifdef USING_VMA_EXTRA_API
  vma_api = vma_get_api(); 
  if (!vma_api)
    cerr << "spip::UDPReceiveDB::UDPReceiveDB VMA support compiled, but VMA not available" << endl;
  pkts = NULL;
#else
  vma_api = 0;
#endif
}

spip::UDPReceiveDB::~UDPReceiveDB()
{
  db->unlock();
  db->disconnect();

  delete db;
}

int spip::UDPReceiveDB::configure (const char * header)
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

  unsigned start_chan, end_chan;
  if (ascii_header_get (header, "START_CHANNEL", "%u", &start_chan) != 1)
    throw invalid_argument ("START_CHANNEL did not exist in header");
  if (ascii_header_get (header, "END_CHANNEL", "%u", &end_chan) != 1)
    throw invalid_argument ("END_CHANNEL did not exist in header");

  if (!format)
    throw runtime_error ("unable for prepare format");
  format->set_channel_range (start_chan, end_chan);
}

void spip::UDPReceiveDB::prepare (std::string ip_address, int port)
{
  // create and open a UDP receiving socket
  sock = new UDPSocketReceive ();

  sock->open (ip_address, port);
  
  //sock->set_nonblock ();

  sock->resize (format->get_header_size() + format->get_data_size());

  // this should not be required when using VMA offloading
  sock->resize_kernel_buffer (32*1024*1024);

  stats = new UDPStats (format->get_header_size(), format->get_data_size());
}

void spip::UDPReceiveDB::set_format (spip::UDPFormat * fmt)
{
  if (format)
    delete format;
  format = fmt;
}

// write the ascii header to the datablock, then
void spip::UDPReceiveDB::open (const char * header)
{
  db->write_header (header);
}

void spip::UDPReceiveDB::close ()
{
  if (db->is_block_open())
  {
    db->close_block(db->get_data_bufsz());
  }
}

// receive UDP packets for the specified time at the specified data rate
void spip::UDPReceiveDB::receive ()
{
  uint64_t packet_number = 0;
  int64_t prev_packet_number = -1;
  uint64_t total_bytes_recvd = 0;

  size_t got;
  uint64_t nsleeps = 0;

  struct sockaddr_in client_addr;
  struct sockaddr * addr = (struct sockaddr *) &client_addr;
  socklen_t addr_size = sizeof(struct sockaddr);

  bool have_packet = false;

  // block control logic
  char * block = (char *) db->open_block();
  bool need_next_block = false;

  const unsigned header_size = format->get_header_size();
  const unsigned data_size   = format->get_data_size();

  const uint64_t samples_per_buf = format->get_samples_for_bytes (db->get_data_bufsz());

  int fd = sock->get_fd();
  char * buf = sock->get_buf();
  char * payload = buf + header_size;
  size_t bufsz = sock->get_bufsz();
  int result;

  // block accounting 
  
  const uint64_t packets_per_buf = db->get_data_bufsz() / data_size;
  uint64_t curr_sample = 0;
  uint64_t next_sample  = samples_per_buf;
  uint64_t packets_this_buf = 0;
  uint64_t offset;

  cerr << "spip::UDPReceiveDB::receive db->get_data_bufsz()=" << db->get_data_bufsz() << endl;
  cerr << "spip::UDPReceiveDB::receive data_size=" << data_size << endl;
  cerr << "spip::UDPReceiveDB::receive packets_per_buf=" << packets_per_buf << endl;
  cerr << "spip::UDPReceiveDB::receive [" << curr_sample << " - " << next_sample << "]" << endl;

#ifdef USING_VMA_EXTRA_API
  int flags;
#endif

  while (keep_receiving)
  {
    if (vma_api)
    {
#ifdef USING_VMA_EXTRA_API
      if (pkts)
      {
        vma_api->free_packets(fd, pkts->pkts, pkts->n_packet_num);
        pkts = NULL;
      }
      while (!have_packet && keep_receiving)
      {
        flags = 0;
        got = vma_api->recvfrom_zcopy(fd, buf, bufsz, &flags, addr, &addr_size);
        if (got == bufsz)
        {
          if (flags & MSG_VMA_ZCOPY) 
          {
            pkts = (struct vma_packets_t*) buf;
            struct vma_packet_t *pkt = &pkts->pkts[0];
            packet_number = format->decode_header_seq ((char *) pkt->iov[0].iov_base, 8);
          }
          else
          {
            packet_number = format->decode_header_seq (buf, bufsz);  
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
          cerr << "spip::UDPReceiveDB::receive error expected " << bufsz  
               << " B, received " << got << " B" <<  endl;
          keep_receiving = false;
        }
      }
#endif
    }
    else
    {
      while (!have_packet && keep_receiving)
      {
        got = recvfrom (fd, buf, bufsz, 0, addr, &addr_size);
        if (got == bufsz)
          have_packet = true;
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
          cerr << "spip::UDPReceiveDB::receive error expected " << bufsz  
               << " B, received " << got << " B" <<  endl;
          keep_receiving = false;
        }
      }
      format->decode_header (buf, bufsz);
    }

    //cerr << "got=" << got << endl;

    stats->sleeps(nsleeps);
    nsleeps = 0;

    // open a new data block buffer if necessary
    if (!db->is_block_open())
    {
      block = (char *) db->open_block();
      need_next_block = false;

      // number is first packet due in block to first packet of next block
      curr_sample = next_sample;
      next_sample += samples_per_buf;

      //cerr << "spip::UDPReceiveDB::receive [" << curr_sample << " - " 
      //     << next_sample << "] (" << packets_this_buf << ")" << endl;

      if (packets_this_buf == 0)
        keep_receiving = false;

      packets_this_buf = 0;
    }

    // copy the current packet into the appropriate place in the buffer
    result = format->insert_packet (block, payload, curr_sample, next_sample);

    if (result == 0)
    {
      have_packet = false;
      packets_this_buf++;
      stats->increment();
    }
    else if (result == 1)
    {
      need_next_block = true;
      cerr << "1: dropped packet" << endl;
      format->print_packet_header();
      stats->dropped (packets_per_buf - packets_this_buf);
    }
    else
    {
      cerr << "2: dropped packet" << endl;
      format->print_packet_header();
      have_packet = false;
      keep_receiving = false;
      stats->dropped();
    }

    // close open data block buffer if is is now full
    if (packets_this_buf == packets_per_buf || need_next_block)
    {
      //cerr << "spip::UDPReceiveDB::receive close_block packets_this_buf=" 
      //     << packets_this_buf << " packets_per_buf=" << packets_per_buf 
      //     << " need_next_block=" << need_next_block << endl;
      //format->print_packet_header();
      db->close_block(db->get_data_bufsz());
    }
  }
  cerr << "spip::UDPReceiveDB::receive done!" << endl;
}
