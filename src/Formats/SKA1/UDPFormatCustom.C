/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPFormatCustom.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace std;

spip::UDPFormatCustom::UDPFormatCustom()
{
  packet_header_size = sizeof(ska1_custom_udp_header_t);
  packet_data_size   = UDP_FORMAT_CUSTOM_PACKET_NSAMP * UDP_FORMAT_CUSTOM_NDIM * UDP_FORMAT_CUSTOM_NPOL;

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

void spip::UDPFormatCustom::configure(const spip::AsciiHeader& config, const char* suffix)
{
  if (config.get ("START_CHANNEL", "%u", &start_channel) != 1)
    throw invalid_argument ("START_CHANNEL did not exist in header");
  if (config.get ("END_CHANNEL", "%u", &end_channel) != 1)
    throw invalid_argument ("END_CHANNEL did not exist in header");
  nchan = (end_channel - start_channel) + 1;
  header.channel_number = start_channel;
}

void spip::UDPFormatCustom::prepare (spip::AsciiHeader& header, const char * suffix)
{
  // conversion factor for a sequence number to byte offset
  seq_to_bytes = (UDP_FORMAT_CUSTOM_PACKET_NSAMP * nchan * ndim * npol * nbit) / 8;

  // number of byte per channel
  channel_stride = (UDP_FORMAT_CUSTOM_PACKET_NSAMP * ndim * npol * nbit) / 8;
}

uint64_t spip::UDPFormatCustom::get_samples_for_bytes (uint64_t nbytes)
{
  cerr << "spip::UDPFormatCustom::get_samples_for_bytes npol=" << npol 
       << " ndim=" << ndim << " nchan=" << nchan << endl;
  uint64_t nsamps = nbytes / (npol * ndim * nchan);
  return nsamps;
}

uint64_t spip::UDPFormatCustom::get_resolution ()
{
  uint64_t nbits = nchan * npol * ndim * nbit * spip::UDPFormatCustom::get_samples_per_packet();
  return nbits / 8;
}

inline void spip::UDPFormatCustom::encode_header_seq (char * buf, uint64_t seq)
{
  header.seq_number = seq;
  encode_header (buf);
}

inline void spip::UDPFormatCustom::encode_header (char * buf)
{
  memcpy (buf, (void *) &header, sizeof (ska1_custom_udp_header_t));
}

inline uint64_t spip::UDPFormatCustom::decode_header_seq (char * buf)
{
  memcpy ((void *) &header, buf, sizeof(uint64_t));
  return header.seq_number;
}

inline unsigned spip::UDPFormatCustom::decode_header (char * buf)
{
  memcpy ((void *) &header, buf, sizeof(header));
  return packet_data_size;
}

inline int64_t spip::UDPFormatCustom::decode_packet (char *buf, unsigned * payload_size)
{
  // copy the header from the packet to our internal buffer
  *payload_size = decode_header (buf);

  // set the pointer to the payload
  payload_ptr = buf + packet_header_size;

  //cerr << "header.channel_number=" << header.channel_number << " start_channel=" << start_channel << " channel_stride=" << channel_stride << endl;

  // compute absolute byte offset for this packet within the data stream
  const uint64_t byte_offset = (header.seq_number * seq_to_bytes) + 
                               (header.channel_number - start_channel) * channel_stride;

  return (int64_t) byte_offset;  
}

inline int spip::UDPFormatCustom::insert_last_packet (char * buffer)
{
  memcpy (buffer, payload_ptr, packet_data_size);
  return 0;
}

// generate the next packet in the cycle
inline void spip::UDPFormatCustom::gen_packet (char * buf, size_t bufsz)
{
  // cycle through each of the channels to produce a packet with 1024 
  // time samples and two polarisations

  // write the new header
  encode_header (buf);

  // increment channel number
  header.channel_number++;
  if (header.channel_number > end_channel)
  {
    header.channel_number = start_channel;
    header.seq_number++;
    nsamp_offset += header.nsamp;
  }

}

void spip::UDPFormatCustom::print_packet_header()
{
  uint64_t pkt_num = (header.seq_number * nchan) + (header.channel_number - start_channel);
  uint64_t last_sample = (header.seq_number * 1024) + 1023;
  cerr << "pkt_num=" << pkt_num << " last_sample=" << last_sample 
       << " seq=" << header.seq_number << " chan=" << header.channel_number << endl;
}
