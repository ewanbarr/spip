/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPFormatMeerKATSPEAD.h"

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <bitset>

using namespace std;

spip::UDPFormatMeerKATSPEAD::UDPFormatMeerKATSPEAD()
{
  packet_header_size = sizeof(meerkat_spead_udp_hdr_t);
  packet_data_size   = 2 * UDP_FORMAT_MEERKAT_SPEAD_PACKET_NSAMP;

  //nsamp_offset = 0;
  header.frequency_channel.item_address = 0;
  header.beam_number.item_address = 0;
}

spip::UDPFormatMeerKATSPEAD::~UDPFormatMeerKATSPEAD()
{
  cerr << "spip::UDPFormatMeerKATSPEAD::~UDPFormatMeerKATSPEAD()" << endl;
}

void spip::UDPFormatMeerKATSPEAD::generate_signal ()
{
}

uint64_t spip::UDPFormatMeerKATSPEAD::get_samples_for_bytes (uint64_t nbytes)
{
  cerr << "spip::UDPFormatMeerKATSPEAD::get_samples_for_bytes npol=" << npol 
       << " ndim=" << ndim << " nchan=" << nchan << endl;
  uint64_t nsamps = nbytes / (npol * ndim * nchan);
  return nsamps;
}

void spip::UDPFormatMeerKATSPEAD::set_channel_range (unsigned start, unsigned end) 
{
  cerr << "spip::UDPFormatMeerKATSPEAD::set_channel_range start=" << start 
       << " end=" << end << endl;
  start_channel = start;
  end_channel   = end;
  nchan = (end - start) + 1;
  header.frequency_channel.item_address = start;
  cerr << "spip::UDPFormatMeerKATSPEAD::set_channel_range nchan=" <<  nchan << endl;
}

inline void spip::UDPFormatMeerKATSPEAD::encode_header_seq (char * buf, uint64_t seq)
{
  header.heap_number.item_address = seq;
  encode_header (buf);
}

inline void spip::UDPFormatMeerKATSPEAD::encode_header (char * buf)
{
  memcpy (buf, (void *) &header, sizeof (meerkat_spead_udp_hdr_t));
}

inline uint64_t spip::UDPFormatMeerKATSPEAD::decode_header_seq (char * buf)
{
  meerkat_spead_udp_hdr_t * hdr = (meerkat_spead_udp_hdr_t *) buf;
  return hdr->heap_number.item_address;
}

inline void spip::UDPFormatMeerKATSPEAD::decode_header (char * buf)
{
  cerr << "spip::UDPFormatMeerKATSPEAD::decode_header copying " << sizeof (meerkat_spead_udp_hdr_t) << " bytes" << endl;
  // copy the spead pkt hdr to the 
  memcpy ((void *) &header, buf, sizeof (meerkat_spead_udp_hdr_t));

#if 0
  items = (spead_item_pointer_t *) (buf + sizeof (meerkat_spead_udp_hdr_t));

  // copy the item pointers as well
  memcpy ((void *) &(header.items), buf + sizeof (meerkat_spead_udp_hdr_t), header.pkt_hdr.num_items * sizeof(spead_item_pointer_t));

  for (unsigned i=0; i<header.pkt_hdr.num_items; i++)
    if (items[i].item_identifier == 0x0001)
      header.heap_id = items[i].item_address;
    if (items[i].item_identifier == 0x0002)
      header.heap_size = items[i].item_address;
    if (items[i].item_identifier == 0x0003)
      header.heap_offset = items[i].item_address;
    if (items[i].item_identifier == 0x0004)
      header.payload_length.item_address = items[i].item_address;
#endif
}

// assumes the packet has already been "decoded" by a call to decode header
inline int spip::UDPFormatMeerKATSPEAD::insert_packet (char * buf, char * pkt, uint64_t start_samp, uint64_t next_start_samp)
{
  // there are 256 samples per packet, 1 channel per packet
  const uint64_t sample_number = header.heap_number.item_address * UDP_FORMAT_MEERKAT_SPEAD_PACKET_NSAMP;

  if (sample_number < start_samp)
  {
    cerr << "header.heap_number.item_address=" << header.heap_number.item_address << " header.frequency_channel.item_address=" << header.frequency_channel.item_address << endl;
    cerr << "sample_number=" << sample_number << " start_samp=" << start_samp << endl;
    return UDP_PACKET_TOO_LATE;
  }
  if (sample_number >= next_start_samp)
  {
    return UDP_PACKET_TOO_EARLY;
  }
 
  // determine the channel offset in bytes
  const unsigned channel_offset = (header.frequency_channel.item_address - start_channel) * channel_stride;
  const unsigned sample_offset  = sample_number - start_samp;

  // copy the 512 bytes to the correct place in the output buffer
  memcpy (buf + channel_offset + sample_offset, pkt, UDP_FORMAT_MEERKAT_SPEAD_PACKET_NSAMP * 2);

  return 0;
}

// generate the next packet in the cycle
inline void spip::UDPFormatMeerKATSPEAD::gen_packet (char * buf, size_t bufsz)
{
  // cycle through each of the channels to produce a packet with 1024 
  // time samples and two polarisations

  // write the new header
  encode_header (buf);

  // increment channel number
  header.frequency_channel.item_address++;
  if (header.frequency_channel.item_address > end_channel)
  {
    header.frequency_channel.item_address = start_channel;
    header.heap_number.item_address++;
    //nsamp_offset += header.nsamp;
  }

}

void spip::UDPFormatMeerKATSPEAD::print_item_pointer (spead_item_pointer_t item)
{
  //fprintf (stderr, "  item_address_mode: %x\n", item.item_address_mode);
  fprintf (stderr, "  item_identifier: %x\n", item.item_identifier);
  cerr << "  item_address: " << std::dec << item.item_address << endl;
}

void spip::UDPFormatMeerKATSPEAD::print_packet_header()
{
  uint64_t pkt_num = (header.heap_number.item_address * nchan) + (header.frequency_channel.item_address - start_channel);

  uint64_t last_sample = (header.heap_number.item_address * 1024) + 1023;
  cerr << "pkt_num=" << pkt_num << " last_sample=" << last_sample 
       << " seq=" << header.heap_number.item_address << " chan=" << header.frequency_channel.item_address << endl;

  cerr << "SPEAD HDR: " << endl;
  fprintf (stderr, "  magic_version: %x\n", header.spead_hdr.magic_version);
  fprintf (stderr, "  item_pointer_width: %d\n", header.spead_hdr.item_pointer_width);
  fprintf (stderr, "  heap_addr_width: %d\n", header.spead_hdr.heap_addr_width);
  fprintf (stderr, "  num_items: %d\n", header.spead_hdr.num_items);

  uint64_t * temp = (uint64_t *) &header;
  uint64_t tmpval = *temp;
  cerr << "temp=" << std::bitset<64>(tmpval) << endl;

  cerr << "num_items=" << std::bitset<16>(header.spead_hdr.num_items) << endl;

  spead_item_pointer_t * items = (spead_item_pointer_t *) (((char *) &header) + sizeof(spead_hdr_t));

  for (unsigned i=0; i<16; i++)
  {
    cerr << "item[" << i << "]:" << endl;
    print_item_pointer (items[i]);
  }

  cerr << "heap_number:" << endl;
  print_item_pointer (header.heap_number);

  cerr << "heap_length:" << header.heap_length.item_address << endl;
  print_item_pointer (header.heap_length);

  cerr << "payload_offset_in_heap=" << header.payload_offset_in_heap.item_address << endl;
  cerr << "payload_length=" << header.payload_length.item_address << endl;
  cerr << "timestamp=" << header.timestamp.item_address << endl;
  cerr << "frequency_channel =" << header.frequency_channel.item_address << endl;
  cerr << "f_engine_flags=" << header.f_engine_flags.item_address << endl;
  cerr << "beam_number=" << header.beam_number.item_address << endl;

}
