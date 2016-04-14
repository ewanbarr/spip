/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPFormatMeerKATSimple.h"

#include <cstdlib>
#include <cstdio>
#include <iostream>

using namespace std;

#define SAMPLES_PER_BLOCK 256 

spip::UDPFormatMeerKATSimple::UDPFormatMeerKATSimple()
{
  packet_header_size = sizeof(meerkat_simple_udp_header_t);

  packet_data_size   = SAMPLES_PER_BLOCK * ndim * npol;

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
#ifdef _DEBUG
  cerr << "spip::UDPFormatMeerKATSimple::~UDPFormatMeerKATSimple()" << endl;
#endif
}

void spip::UDPFormatMeerKATSimple::configure(const spip::AsciiHeader& config, const char* suffix)
{
  if (config.get ("START_CHANNEL", "%u", &start_channel) != 1)
    throw invalid_argument ("START_CHANNEL did not exist in config");
  if (config.get ("END_CHANNEL", "%u", &end_channel) != 1)
    throw invalid_argument ("END_CHANNEL did not exist in config");
  nchan = (end_channel - start_channel) + 1;
  header.channel_number = start_channel;
}

void spip::UDPFormatMeerKATSimple::prepare (const spip::AsciiHeader& hdr, const char * suffix)
{
}
void spip::UDPFormatMeerKATSimple::generate_signal ()
{
}

uint64_t spip::UDPFormatMeerKATSimple::get_samples_for_bytes (uint64_t nbytes)
{
#ifdef _DEBUG
  cerr << "spip::UDPFormatMeerKATSimple::get_samples_for_bytes npol=" << npol 
       << " ndim=" << ndim << " nchan=" << nchan << endl;
#endif
  uint64_t nsamps = nbytes / (npol * ndim * nchan);
  return nsamps;
}

uint64_t spip::UDPFormatMeerKATSimple::get_resolution ()
{
#ifdef _DEBUG
  cerr << "spip::UDPFormatMeerKATSimple::get_resolution()" << endl;
#endif
  uint64_t nbits = nchan * SAMPLES_PER_BLOCK * npol * nbit * ndim;
  return nbits / 8;
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
  decode_header (buf);
  return 0;
}

inline unsigned spip::UDPFormatMeerKATSimple::decode_header (char * buf)
{
  memcpy ((void *) &header, buf, sizeof(header));
  return packet_data_size;
}

inline int64_t spip::UDPFormatMeerKATSimple::decode_packet (char* buf, unsigned * pkt_size)
{
  return -1;
}
inline int spip::UDPFormatMeerKATSimple::insert_last_packet (char * buffer)
{
  return 0;
}

inline int spip::UDPFormatMeerKATSimple::check_packet ()
{
  prev_packet_number = curr_packet_number;
  curr_packet_number = (header.seq_number * nchan) + (header.channel_number - start_channel);
  return (curr_packet_number - (prev_packet_number + 1)) * packet_data_size;
}

// generate the next packet in the cycle
inline void spip::UDPFormatMeerKATSimple::gen_packet (char * buf, size_t bufsz)
{
  // cycle through each of the channels to produce a packet with SAMPLES_PER_BLOCK 
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
  uint64_t last_sample = (header.seq_number * SAMPLES_PER_BLOCK) + (nchan - 1);
  cerr << "pkt_num=" << pkt_num << " last_sample=" << last_sample 
       << " seq=" << header.seq_number << " chan=" << header.channel_number << endl;
}
