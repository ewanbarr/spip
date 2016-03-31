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
  nchan = 4096;
  nsamp_per_heap = 256;
  nbytes_per_samp = 2;
  nbytes_per_heap = nsamp_per_heap * nchan * nbytes_per_samp; 
  timestamp_to_samples = nchan * nbytes_per_samp;

  heap_size = 2097152;

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

void spip::UDPFormatMeerKATSPEAD::configure(const spip::AsciiHeader& config, const char* suffix)
{
  if (config.get ("START_CHANNEL", "%u", &start_channel) != 1)
    throw invalid_argument ("START_CHANNEL did not exist in config");
  if (config.get ("END_CHANNEL", "%u", &end_channel) != 1)
    throw invalid_argument ("END_CHANNEL did not exist in config");
  if (config.get ("TSAMP", "%lf", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in config");
  nchan = (end_channel - start_channel) + 1;
  configured = true;
}

void spip::UDPFormatMeerKATSPEAD::prepare (const spip::AsciiHeader& header, const char * suffix)
{
  char * buffer = (char *) malloc (128);
  char * meta_host = (char *) malloc (128);
  int meta_port = -1;

  sprintf (buffer, "META_HOST%s", suffix);
  if (header.get (buffer, "%s", meta_host) != 1)
    throw invalid_argument ("META_HOST did not exist in header");
  sprintf (buffer, "META_PORT%s", suffix);
  if (header.get (buffer, "%u", &meta_port) != 1)
    throw invalid_argument ("META_PORT did not exist in header");

  int lower = 1048576;
  int upper = lower + 4096;

  std::shared_ptr<spead2::memory_pool> pool = std::make_shared<spead2::memory_pool>(lower, upper, 12, 8);
  spead2::thread_pool worker;
  spead2::recv::ring_stream<> stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2);
  stream.set_memory_pool(pool);
  // fix this 
  boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address_v4::any(), meta_port);
  stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 262144);

  bool keep_receiving = true;
  bool have_metadata = false;

  cerr << "Waiting for meta-data on port " << meta_port << endl;
  while (keep_receiving && !have_metadata)
  {
    try
    {
      spead2::recv::heap fh = stream.pop();
      const auto &items = fh.get_items();
      for (const auto &item : items)
        bf_config.parse_item (item);

      vector<spead2::descriptor> descriptors = fh.get_descriptors();
      for (const auto &descriptor : descriptors)
        bf_config.parse_descriptor (descriptor);
      have_metadata = bf_config.valid();
    }
    catch (spead2::ringbuffer_stopped &e)
    {
      keep_receiving = false;
    }
  }
  cerr << "Received meta-data" << endl;

  time_t adc_sync_time = bf_config.get_sync_time();
  uint64_t adc_sample_rate = bf_config.get_adc_sample_rate();

  if (header.get ("UTC_START", "%s", buffer) != 1)
    throw invalid_argument ("UTC_START did not exist in header");
  spip::Time utc_start(buffer);

  cerr << "adc_sync_time=" << adc_sync_time << endl;
  cerr << "utc_start.get_time=" << utc_start.get_time() << endl;
  cerr << "adc_sample_rate=" << adc_sample_rate << endl;

  obs_start_sample = (int64_t) (adc_sample_rate * (utc_start.get_time() - adc_sync_time));

  cerr << "UTC_START=" << buffer << " obs_start_sample=" << obs_start_sample << endl;

  double cbf_byte_rate = ((double) nchan * nbytes_per_samp * 1e6) / tsamp;
  cerr << "cbf_byte_rate=" << cbf_byte_rate << endl;

  adc_to_cbf = (double) adc_sample_rate / cbf_byte_rate;
  cerr << "adc_to_cbf=" << adc_to_cbf << endl;

  free (buffer);
  free (meta_host);

  prepared = true;
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
      int64_t adc_sample = get_timestamp_fast();
      int64_t obs_sample = adc_sample - obs_start_sample;
      curr_heap_cnt = header.heap_cnt;

      curr_heap_offset = (uint64_t) ((double) obs_sample / adc_to_cbf);

      double t_offset = (double) obs_sample / 1712000000.0;
      if (header.heap_cnt % 1000 == 0)
        print_packet_timestamp();
      //cerr << "adc_sample=" << adc_sample << " obs_sample=" << obs_sample << " offset=" << t_offset <<  " curr_heap_offset=" << curr_heap_offset << endl;

    }
    // ignore
    else
    {
      return -1;
    }
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
  double adc_sample_rate = (double) bf_config.get_adc_sample_rate();
  double adc_time = adc_sample / adc_sample_rate;
  time_t adc_sync_time = bf_config.get_sync_time();

  spip::Time packet_time (adc_sync_time);
  unsigned adc_time_secs = (unsigned) floor(adc_time);

  packet_time.add_seconds (adc_time_secs);
  string packet_gmtime = packet_time.get_gmtime();

  cerr << packet_gmtime << "." << (adc_time - adc_time_secs) << " UTC" << endl;

}
