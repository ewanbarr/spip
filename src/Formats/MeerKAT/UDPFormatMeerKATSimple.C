/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPFormatMeerKATSimple.h"

#include <cstdlib>
#include <iostream>

using namespace std;

spip::UDPFormatMeerKATSimple::UDPFormatMeerKATSimple()
{
  packet_header_size = sizeof(meerkat_simple_udp_header_t);
  packet_data_size   = 4096;

  nsamp_offset = 0;

  header.cbf_version = 1;
  header.seq_number = 0;
  header.weights = 1;
  header.nsamp = spip::UDPFormatMeerKATSimple::get_samples_per_packet();
  header.channel_number = 0;
  header.beam_number = 0;

  generate_noise_buffer (8);
}

spip::UDPFormatMeerKATSimple::~UDPFormatMeerKATSimple()
{
  cerr << "spip::UDPFormatMeerKATSimple::~UDPFormatMeerKATSimple()" << endl;
}

void spip::UDPFormatMeerKATSimple::generate_signal ()
{
}

uint64_t spip::UDPFormatMeerKATSimple::get_samples_for_bytes (uint64_t nbytes)
{
  cerr << "spip::UDPFormatMeerKATSimple::get_samples_for_bytes npol=" << npol 
       << " ndim=" << ndim << " nchan=" << nchan << endl;
  uint64_t nsamps = nbytes / (npol * ndim * nchan);
  return nsamps;
}

void spip::UDPFormatMeerKATSimple::set_channel_range (unsigned start, unsigned end) 
{
  cerr << "spip::UDPFormatMeerKATSimple::set_channel_range start=" << start 
       << " end=" << end << endl;
  start_channel = start;
  end_channel   = end;
  nchan = (end - start) + 1;
  header.channel_number = start;
  cerr << "spip::UDPFormatMeerKATSimple::set_channel_range nchan=" <<  nchan << endl;
}

inline void spip::UDPFormatMeerKATSimple::encode_header_seq (char * buf, uint64_t seq)
{
  header.seq_number = seq;
  encode_header (buf);
}

inline void spip::UDPFormatMeerKATSimple::encode_header (char * buf)
{
  memcpy (buf, (void *) &header, sizeof (meerkat_simple_udp_header_t));
}

inline uint64_t spip::UDPFormatMeerKATSimple::decode_header_seq (char * buf)
{
  memcpy ((void *) &header, buf, sizeof(uint64_t));
  return header.seq_number;
}

inline void spip::UDPFormatMeerKATSimple::decode_header (char * buf)
{
  memcpy ((void *) &header, buf, sizeof(header));
}


inline int spip::UDPFormatMeerKATSimple::insert_packet (char * buf, char * pkt, uint64_t start_samp, uint64_t next_start_samp)
{
  const uint64_t sample_number = header.seq_number * 1024; 
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
  const unsigned sample_offset = (sample_number - start_samp) * ndim;
 
  // incremement buf pointer to 
  const unsigned pol0_offset = channel_offset + sample_offset;
  const unsigned pol1_offset = pol0_offset + chanpol_stride;

  //cerr << "seq=" << header.seq_number << " chan=" << header.channel_number << " sample_number=" << sample_number << " channel_offset=" << channel_offset << " sample_offset=" << sample_offset << " pol0_offset=" << pol0_offset << " pol1_offset=" << pol1_offset << endl;

  memcpy (buf + pol0_offset, pkt, 2048);
  memcpy (buf + pol1_offset, pkt + 2048, 2048);

  return 0;
}

// generate the next packet in the cycle
inline void spip::UDPFormatMeerKATSimple::gen_packet (char * buf, size_t bufsz)
{
  // cycle through each of the channels to produce a packet with 1024 
  // time samples and two polarisations

  // write the new header
  encode_header (buf);

  // fill packet with gaussian noise
  fill_noise (buf + packet_header_size, packet_data_size);

  // increment channel number
  header.channel_number++;
  if (header.channel_number > end_channel)
  {
    header.channel_number = start_channel;
    header.seq_number++;
    nsamp_offset += header.nsamp;
  }

}

void spip::UDPFormatMeerKATSimple::print_packet_header()
{
  uint64_t pkt_num = (header.seq_number * nchan) + (header.channel_number - start_channel);
  uint64_t last_sample = (header.seq_number * 1024) + 1023;
  cerr << "pkt_num=" << pkt_num << " last_sample=" << last_sample 
       << " seq=" << header.seq_number << " chan=" << header.channel_number << endl;
}
