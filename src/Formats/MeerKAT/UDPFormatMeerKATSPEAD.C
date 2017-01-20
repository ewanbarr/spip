/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

// Assume heaps arrive in order

#include "spip/UDPFormatMeerKATSPEAD.h"
#include "spip/Time.h"

#include "spead2/common_defines.h"
#include "spead2/common_endian.h"
#include "spead2/recv_packet.h"
#include "spead2/recv_utils.h"

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <bitset>
#include <stdexcept>

#include <unistd.h>

using namespace std;

spip::UDPFormatMeerKATSPEAD::UDPFormatMeerKATSPEAD()
{
  packet_header_size = 48 + 8;
  packet_data_size   = 4096;

  obs_start_sample = 0;
  npol = 1;
  ndim = 2;
  nbit = 8;
  nsamp_per_heap = 256;
  nbytes_per_samp = (ndim * npol * nbit) / 8;

  // nchan is variable depending on number of active paritions
  nchan = 4096;
  nbytes_per_heap = nsamp_per_heap * nchan * nbytes_per_samp; 
  samples_to_byte_offset = 1;
  heap_size = nbytes_per_heap;

  // this is the average size 
  avg_pkt_size = 4096;
  pkts_per_heap = (unsigned) ceil ( (float) (nsamp_per_heap * nchan * nbytes_per_samp) / (float) avg_pkt_size);

  curr_heap_cnt = -1;
  curr_heap_bytes = 0;
  first_heap = true;
  first_packet = false;
}

spip::UDPFormatMeerKATSPEAD::~UDPFormatMeerKATSPEAD()
{
}

void spip::UDPFormatMeerKATSPEAD::configure(const spip::AsciiHeader& config, const char* suffix)
{
  if (config.get ("NPOL", "%u", &header_npol) != 1)
    throw invalid_argument ("NPOL did not exist in config");
  if (config.get ("START_CHANNEL", "%u", &start_channel) != 1)
    throw invalid_argument ("START_CHANNEL did not exist in config");
  if (config.get ("END_CHANNEL", "%u", &end_channel) != 1)
    throw invalid_argument ("END_CHANNEL did not exist in config");
  if (config.get ("TSAMP", "%lf", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in config");
  if (config.get ("TSAMP", "%lf", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in config");
  if (config.get ("ADC_SAMPLE_RATE", "%lu", &adc_sample_rate) != 1)
    throw invalid_argument ("ADC_SAMPLE_RATE did not exist in config");
  if (config.get ("BW", "%lf", &bw) != 1)
    throw invalid_argument ("BW did not exist in config");

  nchan = (end_channel - start_channel) + 1;
  nbytes_per_heap = nsamp_per_heap * nchan * nbytes_per_samp;
  samples_to_byte_offset = (double) (bw * 1e6 * ndim * header_npol) / adc_sample_rate; 
  heap_size = nbytes_per_heap;

  configured = true;
}

void spip::UDPFormatMeerKATSPEAD::prepare (spip::AsciiHeader& header, const char * suffix)
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

  spip::Time utc_start(key);

#ifdef _DEBUG
  cerr << "adc_sync_time=" << adc_sync_time << endl;
  cerr << "utc_start.get_time=" << utc_start.get_time() << endl;
  cerr << "adc_sample_rate=" << adc_sample_rate << endl;

  // offset from ADC sync
  cerr << "OFFSET_TIME=" << (utc_start.get_time() - adc_sync_time) << " ADC_SYNC_TIME=" << adc_sync_time << " UTC_START_TIME=" << utc_start.get_time() << endl;
#endif

  // the start sample (at ADC_SAMPLE_RATE), relative to sync time  for the exact UTC start second
  obs_start_sample = (int64_t) (adc_sample_rate * (utc_start.get_time() - adc_sync_time));

  // sample rate of PFB/CBF
  double sample_rate = double(1e6) / tsamp;

  // number of ADC samples per PFB sample
  uint64_t adc_samples_per_sample = uint64_t(rint(double(adc_sample_rate) / sample_rate));

  // number of ADC samples per PFB heap - this should always be 2097152
  adc_samples_per_heap = adc_samples_per_sample * nsamp_per_heap;
  if (adc_samples_per_heap != 2097152)
    throw invalid_argument("ADC samples per heap != 2097152");

#ifdef _DEBUG
  cerr << "OBS_START_SAMPLE=" << obs_start_sample << " ADC_SAMPLES_PER_SAMPLE=" << adc_samples_per_sample << endl;
#endif

  // observation should begin on first heap after UTC_START, add offset in picoseconds to header
  int64_t modulus = obs_start_sample % adc_samples_per_heap;
  if (modulus > 0)
  {
    int64_t adc_samples_to_add = adc_samples_per_heap - modulus;   
    obs_start_sample += adc_samples_to_add;
    double offset_seconds = double(adc_samples_to_add) / double(adc_sample_rate);
    uint64_t offset_picoseconds = uint64_t(rintf(offset_seconds * 1e12));
#ifdef _DEBUG
    cerr << "obs_start_sample=" << obs_start_sample << " modulus=" << modulus << " adc_samples_to_add=" << adc_samples_to_add << " offset_picoseconds=" << offset_picoseconds << endl;
    cerr << "MODULUS=" << modulus << " PICOSECONDS=" << offset_picoseconds << endl;
#endif
    header.set ("PICOSECONDS", "%lu", offset_picoseconds);
  }

#ifdef _DEBUG
  cerr << "UTC_START=" << key<< " obs_start_sample=" << obs_start_sample << " modulus=" << modulus << endl;
#endif

  free (key);

  prepared = true;
}

void spip::UDPFormatMeerKATSPEAD::conclude ()
{
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
  return heap_size;
}

void spip::UDPFormatMeerKATSPEAD::set_channel_range (unsigned start, unsigned end) 
{
  cerr << "spip::UDPFormatMeerKATSPEAD::set_channel_range start=" << start 
       << " end=" << end << endl;
  start_channel = start;
  end_channel   = end;
  nchan = (end - start) + 1;
  cerr << "spip::UDPFormatMeerKATSPEAD::set_channel_range nchan=" <<  nchan << endl;
}

inline void spip::UDPFormatMeerKATSPEAD::encode_header_seq (char * buf, uint64_t seq)
{
  encode_header (buf);
}

inline void spip::UDPFormatMeerKATSPEAD::encode_header (char * buf)
{
  //memcpy (buf, (void *) &header, sizeof (meerkat_spead_udp_hdr_t));
}

void spip::UDPFormatMeerKATSPEAD::decode_spead (char * buf)
{
  spead2::recv::decode_packet (header, (const uint8_t *) buf, 4152);
}

// return byte offset for this payload in the whole data stream
inline int64_t spip::UDPFormatMeerKATSPEAD::decode_packet (char* buf, unsigned * pkt_size)
{
  spead2::recv::decode_packet (header, (const uint8_t *) buf, 4152);

  *pkt_size = (unsigned) header.payload_length;

  if (header.heap_cnt != curr_heap_cnt)
  {
    if (!prepared || !configured)
      throw runtime_error ("Cannot process packet if not configured and prepared");

    // test for a packet of expected size
    if (header.n_items == 2 && header.heap_length == heap_size)
    {
      // heap timestamp is assumed to be the start of the heap!
      int64_t adc_sample = get_timestamp_fast();
      int64_t obs_sample = adc_sample - obs_start_sample;

      if (!first_packet)
      {
#ifdef DEBUG
        if (offset == 0)
          cerr << "FIRST PACKET timestamp=" << get_timestamp_fast()
               << " adc_sample=" << adc_sample << " obs_start_sample=" << obs_start_sample 
               << " obs_sample=" << obs_sample
               << " samples_to_byte_offset=" << samples_to_byte_offset
               << " curr_heap_offset=" << (int64_t) (obs_sample * samples_to_byte_offset) << endl;
#endif
        first_packet = true;
      }

      // if this packet pre-dates our start time, ignore
      if (obs_sample < 0)
        return -1;
      if (curr_heap_cnt == -1)
      {
#ifdef _DEBUG
        if (offset == 0)
          cerr << "FIRST HEAP timestamp= " << get_timestamp_fast() 
               << " adc_sample=" << adc_sample << " obs_start_sample=" << obs_start_sample 
               << " obs_sample=" << obs_sample
               << " samples_to_byte_offset=" << samples_to_byte_offset
               << " curr_heap_offset=" << (int64_t) (obs_sample * samples_to_byte_offset) << endl;
#endif
      }

      curr_heap_cnt = header.heap_cnt;
      curr_heap_offset = (uint64_t) (obs_sample * samples_to_byte_offset);

#ifdef _DEBUG
      double t_offset = (double) obs_sample / adc_sample_rate;
      cerr << "spip::UDPFormatMeerKATSPEAD::decode_packet adc=" << adc_sample << " t_offset=" << t_offset << endl;
#endif
    }
    else
    {
#ifdef _DEBUG
      print_packet_header();
#endif
      // check for the end of stream
      if (check_stream_stop ())
      {
        return -2;
      }
      // ignore
      else
      {
        return -1;
      }
    }
  }
  else
  {
    if (header.n_items != 2 && header.heap_length != heap_size)
      if (check_stream_stop ())
        return -2;
      else
        return -1;
  }
  return (int64_t) curr_heap_offset + header.payload_offset;
}

inline int spip::UDPFormatMeerKATSPEAD::insert_last_packet (char * buffer)
{
  memcpy (buffer, header.payload, header.payload_length);
  return 0;
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

bool spip::UDPFormatMeerKATSPEAD::check_stream_stop ()
{
  spead2::recv::pointer_decoder decoder(header.heap_address_bits);
  for (int i = 0; i < header.n_items; i++)
  {
    spead2::item_pointer_t pointer = spead2::load_be<spead2::item_pointer_t>(header.pointers + i * sizeof(spead2::item_pointer_t));
    spead2::s_item_pointer_t item_id = decoder.get_id(pointer);
    if (item_id == spead2::STREAM_CTRL_ID && decoder.is_immediate(pointer) &&
        decoder.get_immediate(pointer) == spead2::CTRL_STREAM_STOP)
      return true;
  }
  return false;
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
}

void spip::UDPFormatMeerKATSPEAD::print_packet_timestamp ()
{
  double adc_sample = (double) get_timestamp_fast();
  double adc_sample_rate = (double) adc_sample_rate;
  double adc_time = adc_sample / adc_sample_rate;

  spip::Time packet_time (adc_sync_time);
  unsigned adc_time_secs = (unsigned) floor(adc_time);

  packet_time.add_seconds (adc_time_secs);
  string packet_gmtime = packet_time.get_gmtime();

  cerr << packet_gmtime << "." << (adc_time - adc_time_secs) << " UTC" << endl;

}
