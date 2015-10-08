/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPFormatMeerKAT.h"

#include <cstdlib>
#include <iostream>

using namespace std;

spip::UDPFormatMeerKAT::UDPFormatMeerKAT()
{
  packet_header_size = sizeof(meerkat_udp_header_t);
  packet_data_size   = 4096;

  nsamp_offset = 0;

  header.cbf_version = 1;
  header.seq_number = 0;
  header.weights = 1;
  header.nsamp = spip::UDPFormatMeerKAT::get_samples_per_packet();
  header.channel_number = 0;
  header.beam_number = 0;
}

spip::UDPFormatMeerKAT::~UDPFormatMeerKAT()
{
  cerr << "spip::UDPFormatMeerKAT::~UDPFormatMeerKAT()" << endl;
}

void spip::UDPFormatMeerKAT::generate_signal ()
{
}

uint64_t spip::UDPFormatMeerKAT::get_samples_for_bytes (uint64_t nbytes)
{
  cerr << "spip::UDPFormatMeerKAT::get_samples_for_bytes npol=" << npol 
       << " ndim=" << ndim << " nchan=" << nchan << endl;
  uint64_t nsamps = nbytes / (npol * ndim * nchan);
  return nsamps;
}

void spip::UDPFormatMeerKAT::set_channel_range (unsigned start, unsigned end) 
{
  cerr << "spip::UDPFormatMeerKAT::set_channel_range start=" << start 
       << " end=" << end << endl;
  start_channel = start;
  end_channel   = end;
  nchan = (end - start) + 1;
  header.channel_number = start;
  cerr << "spip::UDPFormatMeerKAT::set_channel_range nchan=" <<  nchan << endl;
}

inline void spip::UDPFormatMeerKAT::encode_header_seq (char * buf, uint64_t seq)
{
  header.seq_number = seq;
  encode_header (buf);
}

inline void spip::UDPFormatMeerKAT::encode_header (char * buf)
{
  memcpy (buf, (void *) &header, sizeof (meerkat_udp_header_t));
}

inline uint64_t spip::UDPFormatMeerKAT::decode_header_seq (char * buf)
{
  memcpy ((void *) &header, buf, sizeof(uint64_t));
  return header.seq_number;
}

inline void spip::UDPFormatMeerKAT::decode_header (char * buf)
{
  // copy the spead pkt hdr to the 
  memcpy ((void *) &(header.pkt_hdr), buf, sizeof (spead_pkt_hdr_t));

  items = (spead_item_pointer_t *) (buf + sizeof (spead_pkt_hdr_t));

  // copy the item pointers as well
  memcpy ((void *) &(header.items), buf + sizeof (spead_pkt_hdr_t), header.pkt_hdr.num_items * sizeof(spead_item_pointer_t));

  for (unsigned i=0; i<header.pkt_hdr.num_items; i++)
    if (items[i].item_identifier == 0x0001)
      header.heap_id = items[i].item_address;
    if (items[i].item_identifier == 0x0002)
      header.heap_size = items[i].item_address;
    if (items[i].item_identifier == 0x0003)
      header.heap_offset = items[i].item_address;
    if (items[i].item_identifier == 0x0004)
      header.payload_length = items[i].item_address;
}

// assumes the packet has already been "decoded" by a call to decode header
inline int spip::UDPFormatMeerKAT::insert_packet (char * buf, char * pkt, uint64_t start_samp, uint64_t next_start_samp)
{
  // there are 256 samples per packet, 1 channel per packet
  const uint64_t sample_number = header.heap_id * 256;

  if (sample_number < start_samp)
  {
    cerr << "header.seq_number=" << header.seq_number << " header.channel_number=" << header.channel_number << endl;
    cerr << "sample_number=" << sample_number << " start_samp=" << start_samp << endl;
    return UDP_PACKET_TOO_LATE;
  }
  if (sample_number >= next_start_samp)
  {
    return UDP_PACKET_TOO_EARLY;
  }
 
  // determine the channel offset in bytes
  const unsigned channel_offset = (header.channel_number - start_channel) * channel_stride;
  const unsigned sample_offset = (header.seq_number * header.nsamp) - start_samp;

  // incremement buf pointer to 
  const unsigned pol0_offset = channel_offset + sample_offset;
  const unsigned pol1_offset = pol0_offset + chanpol_stride;

  memcpy (buf + pol0_offset, pkt, 2048);
  memcpy (buf + pol1_offset, pkt + 2048, 2048);

  return 0;
}

// generate the next packet in the cycle
inline void spip::UDPFormatMeerKAT::gen_packet (char * buf, size_t bufsz)
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

void spip::UDPFormatMeerKAT::print_packet_header()
{
  uint64_t pkt_num = (header.seq_number * nchan) + (header.channel_number - start_channel);
  uint64_t last_sample = (header.seq_number * 1024) + 1023;
  cerr << "pkt_num=" << pkt_num << " last_sample=" << last_sample 
       << " seq=" << header.seq_number << " chan=" << header.channel_number << endl;
}
