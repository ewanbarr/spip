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

using namespace std;

spip::UDPReceiveDB::UDPReceiveDB(const char * key_string)
{
  db = new DataBlockWrite (key_string);

  db->connect();
  db->lock();

  keep_receiving = true;
  format = new UDPFormat();
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
}

void spip::UDPReceiveDB::prepare (std::string ip_address, int port)
{
  // create and open a UDP receiving socket
  sock = new UDPSocketReceive ();

  sock->open (ip_address, port);
  
  //sock->set_nonblock ();

  sock->resize (format->get_header_size() + format->get_data_size());

  // this should not be required when using VMA offloading
  //sock->resize_kernel_buffer (4*1024*1024);

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

  int fd = sock->get_fd();
  char * buf = (char *) sock->get_buf();
  size_t bufsz = sock->get_bufsz();

  uint64_t total_bytes_recvd = 0;

  size_t got;
  uint64_t nsleeps;

  socklen_t size = sizeof(struct sockaddr);
  struct sockaddr_in client_addr;

  bool have_packet = false;

  // block control logic
  char * block = (char *) db->open_block();
  bool need_next_block = false;

  const unsigned header_size = format->get_header_size();
  const unsigned data_size   = format->get_data_size();

  // block accounting 
  uint64_t packets_per_buf = db->get_data_bufsz() / data_size;
  uint64_t curr_block_start_packet = 0;
  uint64_t next_block_start_packet = packets_per_buf;
  uint64_t packets_this_buf = 0;
  uint64_t offset;

  cerr << "spip::UDPReceiveDB::receive db->get_data_bufsz()=" << db->get_data_bufsz() << endl;
  cerr << "spip::UDPReceiveDB::receive data_size=" << data_size << endl;
  cerr << "spip::UDPReceiveDB::receive packets_per_buf=" << packets_per_buf << endl;
  cerr << "spip::UDPReceiveDB::receive [" << curr_block_start_packet << " - " << next_block_start_packet << "]" << endl;

  while (keep_receiving)
  {
    nsleeps = 0;
    while (!have_packet && keep_receiving)
    {
      got = recvfrom (fd, buf, bufsz, 0, (struct sockaddr *)&client_addr, &size);
      if (got == bufsz)
        have_packet = true;
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
        cerr << "spip::UDPReceiveDB::receive error expected " << bufsz  
             << " B, received " << got << "B" <<  endl;
        keep_receiving = false;
      }
    }

    stats->sleeps(nsleeps);
    format->decode_header (buf, bufsz, &packet_number);

    // open a new data block buffer if necessary
    if (!db->is_block_open())
    {
      block = (char *) db->open_block();
      need_next_block = false;

      // number is first packet due in block to first packet of next block
      curr_block_start_packet = next_block_start_packet;
      next_block_start_packet += packets_per_buf;
      packets_this_buf = 0;
    }

    // if the packet belongs in the currently open block
    if (packet_number >= curr_block_start_packet && packet_number < next_block_start_packet)
    {
      // determine offset into the block
      offset = (packet_number - curr_block_start_packet) * data_size;

      // copy the current UDP packet into the open block
      memcpy (block + offset, buf + header_size, data_size);

      // mark the packet as consumed
      have_packet = false;
      
      // increment bytes written
      packets_this_buf++;
      stats->increment();
    }
    else if (packet_number >= next_block_start_packet)
    {
      // this will cause the block to be closed this loop
      // and a new block opened in the next loop, keeping the
      // packet buffered
      need_next_block = true;
      stats->dropped (packets_per_buf - packets_this_buf);
    }
    else
    {
      cerr << "packet drop else case" << endl;
      have_packet = false;
      stats->dropped();
    }
  
    // close open data block buffer if is is now full
    if (packets_this_buf == packets_per_buf || need_next_block)
    {
      db->close_block(db->get_data_bufsz());
      block = 0;
    }
  }
}

