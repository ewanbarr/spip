/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPFormatCustom.h"

#include <cstdlib>
#include <iostream>

using namespace std;

spip::UDPFormatCustom::UDPFormatCustom()
{
  packet_header_size = sizeof(ska1_custom_udp_header_t);
  packet_data_size   = 4096;

  nsamp_offset = 0;

  header.cbf_version = 1;
  header.seq_number = 0;
  header.weights = 1;
  header.nsamp = spip::UDPFormatCustom::get_samples_per_packet();
  header.channel_number = 0;
  header.beam_number = 0;
}

spip::UDPFormatCustom::~UDPFormatCustom()
{
  cerr << "spip::UDPFormatCustom::~UDPFormatCustom()" << endl;
}

void spip::UDPFormatCustom::generate_signal ()
{
}

void spip::UDPFormatCustom::set_channel_range (unsigned start, unsigned end) 
{
  start_channel = start;
  end_channel   = end;
  nchan = (end - start) + 1;
  header.channel_number = start;
}

inline void spip::UDPFormatCustom::encode_header_seq (char * buf, size_t bufsz, uint64_t seq)
{
  header.seq_number = seq;
  encode_header (buf, bufsz);
}

inline void spip::UDPFormatCustom::encode_header (char * buf, size_t bufsz)
{
  memcpy (buf, (void *) &header, sizeof (ska1_custom_udp_header_t));
}

inline void spip::UDPFormatCustom::decode_header_seq (char * buf, size_t bufsz)
{
  memcpy ((void *) &header, buf, sizeof(uint64_t));
}

inline void spip::UDPFormatCustom::decode_header (char * buf, size_t bufsz)
{
  memcpy ((void *) &header, buf, bufsz);
}

inline void spip::UDPFormatCustom::insert_packet (char * buf, uint64_t block_sample, char * pkt)
{
  const unsigned ichan = header.channel_number - start_channel;
  unsigned offset = ichan * chanpol_stride + block_sample;

  buf += offset;

  memcpy (buf, pkt, 2048);
  memcpy (buf + chanpol_stride, pkt + 2048, 2048);
}

// generate the next packet in the cycle
inline void spip::UDPFormatCustom::gen_packet (char * buf, size_t bufsz)
{
  // cycle through each of the channels to produce a packet with 1024 time samples
  // and two polarisations

  // write the new header
  encode_header (buf, bufsz);

  // increment channel number
  header.channel_number++;
  if (header.channel_number >= end_channel)
  {
    header.channel_number = start_channel;
    header.seq_number++;
    nsamp_offset += header.nsamp;
  }

}
