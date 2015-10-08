/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPFormatVDIF.h"

#include <cstdlib>
#include <iostream>

using namespace std;

spip::UDPFormatVDIF::UDPFormatVDIF(int pps)
{
  packet_header_size = sizeof(vdif_header);
  packet_data_size   = 8000;

  nsamp_offset = 0;

  // Each VDIF thread contains 2 "channels", 1 masquerading as each polarisation
  // Each thread represents 1 coarse channel, so a single VDIF data stream will have
  // a number of coarse channels

  // initialise the VDIF header
  int frameLength = 2000;     // 2000 samples from each pol
  int thread_id = 0;          // corase channel number
  int bits = 8;
  int nchan = 2;
  int is_complex = 1;
  char station_id[3] = "B01";

  // this is a requirement of VDIF (integer pps)
  packets_per_second = pps;

  createVDIFHeader (&header, frame_length, thread_id, bits, nchan, is_complex, station_id);
}

spip::UDPFormatVDIF::~UDPFormatVDIF()
{
  cerr << "spip::UDPFormatVDIF::~UDPFormatVDIF()" << endl;
}

void spip::UDPFormatVDIF::generate_signal ()
{
}

uint64_t spip::UDPFormatVDIF::get_samples_for_bytes (uint64_t nbytes)
{
  cerr << "spip::UDPFormatVDIF::get_samples_for_bytes npol=" << npol 
       << " ndim=" << ndim << " nchan=" << nchan << endl;
  uint64_t nsamps = nbytes / (npol * ndim * nchan);

  return nsamps;
}

void spip::UDPFormatVDIF::set_channel_range (unsigned start, unsigned end) 
{
  cerr << "spip::UDPFormatVDIF::set_channel_range start=" << start 
       << " end=" << end << endl;
  start_channel = start;
  end_channel   = end;
  nchan = (end - start) + 1;
  
  // set the channel number of the first channel
  setVDIFThreadID (&header, start);
}

inline void spip::UDPFormatVDIF::encode_header_seq (char * buf, uint64_t seq)
{
  header.frame = seq % packets_per_second;
  header.seconds = seq / packets_per_second;
  encode_header (buf);
}

inline void spip::UDPFormatVDIF::encode_header (char * buf)
{
  memcpy (buf, (void *) &header, sizeof (vdif_header));
}

inline uint64_t spip::UDPFormatVDIF::decode_header_seq (char * buf)
{
  memcpy ((void *) &header, buf, sizeof(uint64_t));
  return header.seconds * packets_per_second + header.frame;
}

inline void spip::UDPFormatVDIF::decode_header (char * buf)
{
  memcpy ((void *) &header, buf, sizeof (vdif_header));
}

inline int spip::UDPFormatVDIF::insert_packet (char * buf, char * pkt, uint64_t start_samp, uint64_t next_start_samp)
{
  const uint64_t packet_number = header.seconds * packets_per_second + header.frame;
  const uint64_t sample_number = packet_number * samples_per_packet;

  if (sample_number < start_samp)
  {
    cerr << "header.seq_number=" << header.seq_number << " header.channel_number=" << header.channel_number << endl;
    cerr << "sample_number=" << sample_number << " start_samp=" << start_samp << endl;
    return 2;
  }
  if (sample_number >= next_start_samp)
  {
    return 1;
  }
 
  // determine the channel offset in bytes
  const unsigned channel_offset = (header.threadid - start_channel) * channel_stride;
  const unsigned sample_offset = sample_number - start_samp;

  // incremement buf pointer to 
  const unsigned pol0_offset = channel_offset + sample_offset;
  const unsigned pol1_offset = pol0_offset + chanpol_stride;

  memcpy (buf + pol0_offset, pkt, 4000);
  memcpy (buf + pol1_offset, pkt + 4000, 4000);

  return 0;
}

// generate the next packet in the cycle
inline void spip::UDPFormatVDIF::gen_packet (char * buf, size_t bufsz)
{
  // cycle through each of the channels to produce a packet with 1024 
  // time samples and two polarisations

  // write the new header
  encode_header (buf);

  // increment our "channel number"
  header.threadid++;

  if (header.threadid > end_channel)
  {
    header.threadid = start_channel;
    nextVDIFHeader (&header, packets_per_second);
    header.frame++;
    nsamp_offset += header.nsamp;
  }

}

void spip::UDPFormatVDIF::print_packet_header()
{
  cerr << "seq=" << header.seq_number << " chan=" << header.channel_number << endl;
}
