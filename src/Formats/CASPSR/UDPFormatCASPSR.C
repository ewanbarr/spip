/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPFormatCASPSR.h"
#include "spip/Time.h"

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <bitset>
#include <stdexcept>

#include <unistd.h>
#include <inttypes.h>

using namespace std;

spip::UDPFormatCASPSR::UDPFormatCASPSR()
{
  packet_header_size = 16;
  packet_data_size   = 8192;

  // fundamentals of CASPSR format
  ndim = 1;   // real input
  npol = 2;   // dual polarisation
  nchan = 1;  // single channel

  global_offset = 0;                 // seq number offset from modulo 1024
  seq_inc = 2048;                    // packet seq_no increment amount
  seq_to_byte = packet_data_size;    // convert seq_no to byte

#ifdef NO_1PPS_RESET
  start_seq_no = 0;
  started = false;
#endif

}

spip::UDPFormatCASPSR::~UDPFormatCASPSR()
{
}

void spip::UDPFormatCASPSR::configure(const spip::AsciiHeader& config, const char* suffix)
{
  if (strcmp(suffix, "_0") == 0)
  {
    seq_to_byte = 2 * packet_data_size;
  }
  else if (strcmp(suffix, "_1") == 0)
  {
    seq_to_byte = 2 * packet_data_size;
  }
  else
  {
    seq_to_byte = packet_data_size;
  }

  configured = true;
}

void spip::UDPFormatCASPSR::prepare (spip::AsciiHeader& header, const char * suffix)
{
  char * key = (char *) malloc (128);

  if (strcmp(suffix, "_0") == 0)
    offset = 0;
  else
    offset = 1;

  if (header.get ("ADC_SYNC_TIME", "%ld", &adc_sync_time) != 1)
    throw invalid_argument ("ADC_SYNC_TIME did not exist in config");
  if (header.get ("UTC_START", "%s", key) != 1)
    throw invalid_argument ("UTC_START did not exist in header");
  if (header.get ("TSAMP", "%lf", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in header");

  spip::Time utc_start(key);

  int64_t offset_seconds = utc_start.get_time() - adc_sync_time;
  int64_t offset_seq = (int64_t) ((offset_seconds * 1e6) / tsamp);

#ifdef NO_1PPS_RESET
  start_seq_no = offset_seq / 4;

  cerr << "spip::UDPFormatCASPSR::prepare start_seq_no=" << start_seq_no << endl;
#endif

  free (key);

  prepared = true;
}

void spip::UDPFormatCASPSR::conclude ()
{
}


void spip::UDPFormatCASPSR::generate_signal ()
{
}

uint64_t spip::UDPFormatCASPSR::get_samples_for_bytes (uint64_t nbytes)
{
  cerr << "spip::UDPFormatCASPSR::get_samples_for_bytes npol=" << npol 
       << " ndim=" << ndim << " nchan=" << nchan << endl;
  uint64_t nsamps = nbytes / (npol * ndim * nchan);
  return nsamps;
}

uint64_t spip::UDPFormatCASPSR::get_resolution ()
{
  return 8192;
}

void spip::UDPFormatCASPSR::set_channel_range (unsigned start, unsigned end) 
{
  cerr << "spip::UDPFormatCASPSR::set_channel_range start=" << start 
       << " end=" << end << endl;
  start_channel = start;
  end_channel   = end;
  nchan = (end - start) + 1;
  cerr << "spip::UDPFormatCASPSR::set_channel_range nchan=" <<  nchan << endl;
}

inline void spip::UDPFormatCASPSR::encode_header_seq (char * buf, uint64_t seq)
{
  encode_header (buf);
}

inline void spip::UDPFormatCASPSR::encode_header (char * buf)
{
  //memcpy (buf, (void *) &header, sizeof (meerkat_spead_udp_hdr_t));
}

// return byte offset for this payload in the whole data stream
inline int64_t spip::UDPFormatCASPSR::decode_packet (char* buf, unsigned * pkt_size)
{
  *pkt_size = packet_data_size;

  // for use in insert_last_packet
  payload = buf + packet_header_size;

  unsigned char * b = (unsigned char *) buf;
  uint64_t tmp = 0;
  unsigned i = 0;
  uint64_t raw_seq_no = 0;
  for (i = 0; i < 8; i++ )
  {
    tmp = (uint64_t) b[8 - i - 1];
    raw_seq_no |= (tmp << ((i & 7) << 3));
  }

#ifdef NO_1PPS_RESET
  // wait for start packet (if start_seq_set
  if (raw_seq_no < start_seq_no)
    return -1;

  raw_seq_no -= start_seq_no;
#endif
  
  // handle the global offset in sequence numbers
  int64_t fixed_raw_seq_no = raw_seq_no + global_offset;

  // check the remainder of the sequence number, for errors in global offset
  int64_t remainder = fixed_raw_seq_no % seq_inc;

  if (remainder == 0)
  {
    // do nothing
  }
  else if (fixed_raw_seq_no < seq_inc)
  {
    cerr << "1: adjusting global offset from " << global_offset
         << " to " << global_offset - remainder << endl;
    global_offset -= remainder;
    fixed_raw_seq_no = raw_seq_no + global_offset;
    remainder = 0;
  }
  else
  {
    cerr << "2: adjusting global offset from " << global_offset 
         << " to " << global_offset + (seq_inc - remainder) << endl;
    global_offset += (seq_inc - remainder);
    fixed_raw_seq_no = raw_seq_no + global_offset;
    remainder = 0;
  }

  seq_no = (fixed_raw_seq_no) / seq_inc;

  if (!started)
    cerr << "fixed_raw_seq_no=" << fixed_raw_seq_no << " seq_no=" << seq_no << endl; 

  if (!started)
    started = true;
  return seq_no * seq_to_byte;
}

int64_t spip::UDPFormatCASPSR::decode_packet_seq (char* buf)
{
  payload = buf + packet_header_size;

  unsigned char * b = (unsigned char *) buf;
  uint64_t tmp = 0;
  unsigned i = 0;
  uint64_t raw_seq_no = 0;
  for (i = 0; i < 8; i++ )
  {
    tmp = (uint64_t) b[8 - i - 1];
    raw_seq_no |= (tmp << ((i & 7) << 3));
  }

  return (int64_t) raw_seq_no;
}


inline int spip::UDPFormatCASPSR::insert_last_packet (char * buffer)
{
  memcpy (buffer, payload, packet_data_size);
  return 0;
}


// generate the next packet in the cycle
inline void spip::UDPFormatCASPSR::gen_packet (char * buf, size_t bufsz)
{
}

void spip::UDPFormatCASPSR::print_packet_header()
{
}

