/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

// Assume heaps arrive in order

#include "spip/UDPFormatMeerKATSPEAD.h"

#include "spead2/common_defines.h"
#include "spead2/common_endian.h"
#include "spead2/recv_packet.h"
#include "spead2/recv_utils.h"

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <bitset>

#include <unistd.h>

using namespace std;

spip::UDPFormatMeerKATSPEAD::UDPFormatMeerKATSPEAD()
{
  packet_header_size = 48 + 8;
  packet_data_size   = 4096;

  obs_start_sample = 0;
  npol = 1;
  ndim = 2;
  nchan = 4096;
  nsamp_per_heap = 256;
  nbytes_per_samp = 2;
  nbytes_per_heap = nsamp_per_heap * nchan * nbytes_per_samp; 
  timestamp_to_samples = nchan * nbytes_per_samp;

  heap_size = 2097152;
  heap_size = 524288;
  heap_size = 262144;

  // this is the average size 
  avg_pkt_size = 4096;
  pkts_per_heap = (unsigned) ceil ( (float) (nsamp_per_heap * nchan * nbytes_per_samp) / (float) avg_pkt_size);

  curr_heap_cnt = -1;
  curr_heap_bytes = 0;
  first_heap = true;
  
  cerr << "spip::UDPFormatMeerKATSPEAD::UDPFormatMeerKATSPEAD pkts_per_heap=" << pkts_per_heap << endl;
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

uint64_t spip::UDPFormatMeerKATSPEAD::get_resolution ()
{
  uint64_t nbits = nchan * nbytes_per_samp * npol * nbit * ndim;
  return nbits / 8;
}


void spip::UDPFormatMeerKATSPEAD::set_channel_range (unsigned start, unsigned end) 
{
  cerr << "spip::UDPFormatMeerKATSPEAD::set_channel_range start=" << start 
       << " end=" << end << endl;
  start_channel = start;
  end_channel   = end;
  nchan = (end - start) + 1;
  //header.frequency_channel.item_address = start;
  cerr << "spip::UDPFormatMeerKATSPEAD::set_channel_range nchan=" <<  nchan << endl;
}

inline void spip::UDPFormatMeerKATSPEAD::encode_header_seq (char * buf, uint64_t seq)
{
  //header.heap_cnt = (seq * 8192) + (heap.payload_offset / 1024);
  encode_header (buf);
}

inline void spip::UDPFormatMeerKATSPEAD::encode_header (char * buf)
{
  //memcpy (buf, (void *) &header, sizeof (meerkat_spead_udp_hdr_t));
}

inline uint64_t spip::UDPFormatMeerKATSPEAD::decode_header_seq (char * buf)
{
  return 0;
}

inline unsigned spip::UDPFormatMeerKATSPEAD::decode_header (char * buf)
{
  spead2::recv::decode_packet (header, (const uint8_t *) buf, 4152);
  return (unsigned) header.payload_length;
}

// requires the packet header has been decoded
inline int spip::UDPFormatMeerKATSPEAD::check_packet ()
{
  //cerr << header.heap_cnt << " " << header.heap_length << " " << header.payload_offset << " " << header.payload_length << endl;
  cerr << header.heap_cnt << "\t" << header.payload_offset << endl;;

  // if this packet belongs to the currently processing heap
  if (header.heap_cnt == curr_heap_cnt)
  {
    // add the payload to the curr_heap
    curr_heap_bytes += header.payload_length;
    return 0;
  }
  // otherwise this belongs to a new heap
  else if (header.n_items == 2 && header.heap_length == heap_size)
  {
    int bytes_dropped = 0;
    first_heap = false;

    // if we have a currently
    if (curr_heap_cnt >= 0)
    {

      bytes_dropped = heap_size - curr_heap_bytes;
      if (bytes_dropped != 0)
      {
        const int64_t timestamp = get_timestamp_fast();
        const int64_t sample = timestamp / timestamp_to_samples; 
        cerr << "spip::UDPFormatMeerKATSPEAD::check_packet DROP heap=" << header.heap_cnt
             << " timestamp=" << timestamp << " factor=" << timestamp_to_samples << " sample=" << sample 
             << " payload_offset=" << header.payload_offset << " payload_length=" << header.payload_length << endl;
      }
    }

    const int64_t timestamp = get_timestamp_fast();
    const int64_t sample = timestamp / timestamp_to_samples; 

    if (timestamp >= 0)
    {
      curr_heap_bytes = header.payload_length;
      curr_heap_offset = 0;

      if (curr_heap_cnt == -1 )
      {
        cerr << "spip::UDPFormatMeerKATSPEAD::check_packet Starting on heap=" << header.heap_cnt
             << " timestamp=" << timestamp << " factor=" << timestamp_to_samples << " curr_sample_number=" << sample
             << " payload_offset=" << header.payload_offset << " payload_length=" << header.payload_length << endl;

        obs_start_sample = timestamp / timestamp_to_samples;  
        curr_heap_bytes += header.payload_offset;
      }
   
      curr_heap_cnt = header.heap_cnt;
      curr_sample_number = (timestamp / timestamp_to_samples) - obs_start_sample;
    }

    return bytes_dropped;
  }
  else if (header.heap_length > heap_size)
  {
    cerr << "spip::UDPFormatMeerKATSPEAD::check_packet First heap: length=" << header.heap_length << endl;
    const int64_t timestamp = get_timestamp_fast();
    if (timestamp >= 0)
    {
      if (curr_heap_cnt == -1 )
      {
        cerr << "spip::UDPFormatMeerKATSPEAD::check_packet Starting on heap=" << header.heap_cnt
             << " timestamp=" << timestamp << " curr_sample_number=" << (timestamp / timestamp_to_samples)
             << " payload_offset=" << header.payload_offset << " payload_length=" << header.payload_length
             << endl;
        obs_start_sample = timestamp / timestamp_to_samples;       
      }
      curr_heap_cnt = header.heap_cnt;
      curr_sample_number = (timestamp / timestamp_to_samples) - obs_start_sample;
      curr_heap_offset = 0;
      curr_heap_bytes = header.payload_length - 833;
    }

    return 0;
  }

  // we ignore this packet (e.g. meta-data packet)
  else
  {
#ifdef _DEBUG
    cerr << "spip::UDPFormatMeerKATSPEAD::check_packet IGNORE curr_heap=" << curr_heap_cnt 
         << " heap_cnt=" << header.heap_cnt << " header.heap_length=" << header.heap_length 
         << " heap_size=" << heap_size << endl;
#endif
    return 0;
  }
}


// assumes the packet has already been "decoded" by a call to decode header
inline int spip::UDPFormatMeerKATSPEAD::insert_packet (char * buf, char * pkt, uint64_t start_samp, uint64_t next_start_samp)
{
  if (header.heap_cnt == curr_heap_cnt)
  {
    if (curr_sample_number < start_samp)
    {
      cerr << "TOO LATE: header.heap_cnt=" << header.heap_cnt  
           << " sample diff=" << start_samp - curr_sample_number
           << " payload_offset=" << header.payload_offset << endl;
      return UDP_PACKET_TOO_LATE;
    }
    else if (curr_sample_number >= next_start_samp)
    {
      cerr << "TOO EARLY: header.heap_cnt=" << header.heap_cnt  
           << " sample diff=" <<( curr_sample_number +1) - next_start_samp
           << " payload_offset=" << header.payload_offset << endl;
      return UDP_PACKET_TOO_EARLY;
    }
    else
    {
      // all packets from the same heap will have the same offset
      if (!curr_heap_offset)
        curr_heap_offset = (curr_sample_number - start_samp) * nchan * nbytes_per_samp;
    
      if (first_heap)
      {
        int payload_offset = (int) header.payload_offset - 833;
        int payload_length = header.payload_length;
        uint8_t * payload = (uint8_t *) header.payload;
        if (payload_offset < 0)
        {
          payload_offset = 0;
          payload_length -= 833;
          payload += 833;
        }
        memcpy (buf + payload_offset, header.payload, header.payload_length);
        return payload_length;
      }
      else
      { 
        // copy the payload into the output buffer
        memcpy (buf + curr_heap_offset + header.payload_offset, header.payload, header.payload_length);
        return header.payload_length;
      }
    }
  }
  else
    return UDP_PACKET_IGNORE;
}

// generate the next packet in the cycle
inline void spip::UDPFormatMeerKATSPEAD::gen_packet (char * buf, size_t bufsz)
{
  // cycle through each of the channels to produce a packet with 1024 
  // time samples and two polarisations

  // write the new header
  encode_header (buf);

  /*
  // increment channel number
  header.frequency_channel.item_address++;
  if (header.frequency_channel.item_address > end_channel)
  {
    header.frequency_channel.item_address = start_channel;
    header.heap_number.item_address++;
    //nsamp_offset += header.nsamp;
  }
  */

}
// assume packet has been decoded
int64_t spip::UDPFormatMeerKATSPEAD::get_timestamp_fast ()
{
  int64_t timestamp = -1;
  spead2::recv::pointer_decoder decoder(header.heap_address_bits);
  for (int i = 0; i < header.n_items; i++)
  {
    spead2::item_pointer_t pointer = spead2::load_be<spead2::item_pointer_t>(header.pointers + i * sizeof(spead2::item_pointer_t));
    spead2::s_item_pointer_t item_id = decoder.get_id(pointer);
    // CBF TIMESTAMP
    if (item_id == 0x1600 && decoder.is_immediate(pointer))
    {
      timestamp = decoder.get_immediate(pointer);
    }
  }
  return timestamp;
}

void spip::UDPFormatMeerKATSPEAD::print_item_pointer (spead_item_pointer_t item)
{
  //fprintf (stderr, "  item_address_mode: %x\n", item.item_address_mode);
  fprintf (stderr, "  item_identifier: %x\n", item.item_identifier);
  cerr << "  item_address: " << std::dec << item.item_address << endl;
}

void spip::UDPFormatMeerKATSPEAD::print_packet_header()
{
  cerr << "heap_cnt=" << header.heap_cnt << " heap_length=" << header.heap_length 
       << " payload_offset=" << header.payload_offset 
       << " payload_length=" << header.payload_length << " n_items=" 
       << header.n_items << endl;

  spead2::recv::pointer_decoder decoder(header.heap_address_bits);
  for (int i = 0; i < header.n_items; i++)
  {
    // there should be items 
    spead2::item_pointer_t pointer = spead2::load_be<spead2::item_pointer_t>(header.pointers + i * sizeof(spead2::item_pointer_t));

    spead2::s_item_pointer_t item_id = decoder.get_id(pointer);
    if (decoder.is_immediate(pointer))
    {
      uint64_t value = decoder.get_immediate(pointer);
      cerr << "item[" << i << "] is_immediate " << " item_id=" << item_id << " value=" << value << endl;
    }
    else
      cerr << "item[" << i << "] item_id=" << item_id << endl;

  }

  //usleep(1000000);
/*
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
*/
}
