/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/UDPFormatVDIF.h"
#include "spip/Time.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace std;

spip::UDPFormatVDIF::UDPFormatVDIF(int pps)
{
  // this assumes the use of the 32 byte VDIF Data Frame Header
  packet_header_size = sizeof(vdif_header);
  packet_data_size   = 2;

  // Each VDIF thread contains 2 "channels", 1 masquerading as each polarisation
  // Each thread represents 1 coarse channel, so a single VDIF data stream will have
  // a number of coarse channels
  packets_per_second = pps;
  tsamp = 0;
  bw = 0;
  start_channel = 0;
  end_channel = 0;
  npol = 0;

  nsamp_per_packet = 0;
  bytes_per_second = 0;

  // we will "extract" the UTC_START from the data stream
  self_start = true;
  configured_stream = false;
}

spip::UDPFormatVDIF::~UDPFormatVDIF()
{
  cerr << "spip::UDPFormatVDIF::~UDPFormatVDIF()" << endl;
}

void spip::UDPFormatVDIF::configure (const spip::AsciiHeader& config, const char* suffix)
{
  if (config.get ("NCHAN", "%u", &nchan) != 1)
    throw invalid_argument ("NCHAN did not exist in config");
  if (config.get ("NDIM", "%u", &ndim) != 1)
    throw invalid_argument ("NDIM did not exist in config");
  if (config.get ("NBIT", "%u", &nbit) != 1)
    throw invalid_argument ("NBIT did not exist in config");
  if (config.get ("NPOL", "%u", &npol) != 1)
    throw invalid_argument ("NPOL did not exist in config");
  if (config.get ("START_CHANNEL", "%u", &start_channel) != 1)
    throw invalid_argument ("START_CHANNEL did not exist in config");
  if (config.get ("END_CHANNEL", "%u", &end_channel) != 1)
    throw invalid_argument ("END_CHANNEL did not exist in config");
  if (config.get ("TSAMP", "%lf", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in config");
  if (config.get ("BW", "%lf", &bw) != 1)
    throw invalid_argument ("BW did not exist in config");
  if (config.get ("BYTES_PER_SECOND", "%lu", &bytes_per_second) != 1)
    throw invalid_argument ("BYTES_PER_SECOND did not exist in config");
 
  if (nchan != (end_channel - start_channel) + 1)
    throw invalid_argument ("NCHAN, START_CHANNEL and END_CHANNEL were in conflict");

  configured = true;
}

void spip::UDPFormatVDIF::prepare (spip::AsciiHeader& config, const char * suffix)
{
  // if the stream is to start on a supplied epoch, extract the 
  // UTC_START from the config
  if (!self_start)
  {
    char * key = (char *) malloc (128);
    if (config.get ("UTC_START", "%s", key) != 1)
      throw invalid_argument ("UTC_START did not exist in config");
    utc_start.set_time (key);
    pico_seconds = 0;
    free (key);
  }

  prepared = true;
}

void spip::UDPFormatVDIF::compute_header ()
{
  // set the channel number of the first channel
  setVDIFThreadID (&header, start_channel);

  // VDIF reference epoch, 6 month period since 2000
  int gm_year = utc_start.get_gm_year();
  int ref_epoch = (gm_year - 2000) * 2;
  int gm_mon  = utc_start.get_gm_month();
  if (gm_mon >= 6)
    ref_epoch++;
  ref_epoch = ref_epoch % 64;
  setVDIFEpoch (&header,  ref_epoch);

  // MJD day for the frame
  int mjd_day = utc_start.get_mjd_day();
  setVDIFFrameMJD (&header, mjd_day);

  // get the time string of the epoch
  char key[32];
  sprintf (key, "%04d-%02d-01-00:00:00", gm_year, gm_mon);
  spip::Time epoch (key);

  // determine number of seconds since the ref epoch
  start_second = int (utc_start.get_time() - epoch.get_time());
  setVDIFFrameSecond (&header, start_second);

  // always start on frame 0
  int frame_number = 0;
  setVDIFFrameNumber (&header, frame_number);

  int vdif_nchan = nchan * npol;
  setVDIFNumChannels (&header, vdif_nchan);

  // determine largest packet size, such that there is an integer
  // number of packets_per_second
  packet_data_size = 8192;
  int nbit_per_sample = (nbit * npol * nchan * ndim);
  int match = 0;

  while (packet_data_size * 8 > nbit_per_sample && !match)
  {
    match = bytes_per_second % packet_data_size;
    packet_data_size -= 8;
  }

  if (!match)
    throw invalid_argument ("No packet size matched VDIF1.1 criteria");

  packets_per_second = bytes_per_second / packet_data_size;

  header.nbits = (nbit - 1);

  // bytes_per_second
  int frame_length = packet_data_size + packet_header_size;
  setVDIFFrameBytes (&header, frame_length);

#ifdef _DEBUG
  cerr << "spip::UDPFormatVDIF::prepare UTC_START=" << utc_start.get_gmtime() << endl;
  cerr << "spip::UDPFormatVDIF::prepare EPOCH=" << epoch.get_gmtime() << endl;
  cerr << "spip::UDPFormatVDIF::prepare start_second=" << start_second << endl;
  cerr << "spip::UDPFormatVDIF::prepare packet_data_size=" << packet_data_size
       << " packets_per_second=" << packets_per_second << endl;
#endif
}

void spip::UDPFormatVDIF::conclude ()
{
}

uint64_t spip::UDPFormatVDIF::get_resolution ()
{
  uint64_t resolution = (uint64_t) (nbit * npol * nchan * ndim) / 8;
  return resolution;
}

uint64_t spip::UDPFormatVDIF::get_samples_for_bytes (uint64_t nbytes)
{
  cerr << "spip::UDPFormatVDIF::get_samples_for_bytes npol=" << npol 
       << " ndim=" << ndim << " nchan=" << nchan << endl;
  uint64_t nsamps = nbytes / (npol * ndim * nchan);

  return nsamps;
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

inline void spip::UDPFormatVDIF::decode_header (char * buf)
{
  memcpy ((void *) &header, buf, sizeof (vdif_header));
}

inline int64_t spip::UDPFormatVDIF::decode_packet (char * buf, unsigned * pkt_size)
{
  // header is stored in the front of the packet
  vdif_header * header_ptr = (vdif_header *) buf;

  if (!configured_stream)
  {
    // decode the header parameters
    decode_header (buf);

    // handle older header versions
    if ((int) (header.legacymode) == 1)
      packet_header_size = 16;
    else
      packet_header_size = 32; 

    if (ndim != (int) (header.iscomplex) + 1)
      throw invalid_argument ("NDIM mismtach between config and VDIF header");

    if (nbit != getVDIFBitsPerSample (&header))
      throw invalid_argument ("NBIT mismtach between config and VDIF header");

    if (nchan * npol != getVDIFNumChannels (&header))
      throw invalid_argument ("NCHAN/NPOL mismtach between config and VDIF header");
     
    packet_data_size = getVDIFFrameBytes (&header) - packet_header_size;
    int nsamp = (8 * packet_data_size) / (npol * nbit * ndim);

    int vdif_epoch = getVDIFEpoch (&header);
    int offset_second = getVDIFFullSecond (&header);
    int frame_number  = getVDIFFrameNumber (&header);

    // this code will not work past 2030 something
    int gm_year = 2000 + (vdif_epoch / 2);
    int gm_month = vdif_epoch % 2;

    // get the time string of the epoch
    char * key = (char *) malloc (32);
    sprintf (key, "%04d-%02d-01-00:00:00", gm_year, gm_month);
    if (self_start)
    {
      utc_start.set_time (key);
      utc_start.add_seconds (offset_second);

      // set start second to be the next whole integer second
      start_second = offset_second;
      if (frame_number > 0)
        start_second++;
      pico_seconds = 0;
    }
    else
    // determine start_seconds of this VDIF data relative to UTC_START
    {
      Time vdif_epoch (key);
      start_second = utc_start.get_time() - vdif_epoch.get_time();
      pico_seconds = 0;
    }

#ifdef _DEBUG
    cerr << "spip::UDPFormatVDIF::decode_packet packet_data_size=" << packet_data_size << endl;
    cerr << "spip::UDPFormatVDIF::decode_packet packet_header_size=" << packet_header_size << endl;
    cerr << "spip::UDPFormatVDIF::decode_packet bit=" << nbit << " npol=" << npol
         << " ndim=" << ndim << " nsamp=" <<  nsamp << endl;
    cerr << "spip::UDPFormatVDIF::decode_packet vdif_epoch=" << vdif_epoch << endl;
    cerr << "spip::UDPFormatVDIF::decode_packet offset_second=" << offset_second << endl;
    cerr << "spip::UDPFormatVDIF::decode_packet frame_number=" << frame_number << endl;
#endif

    configured_stream = true;
    free (key);
  }

  *pkt_size = packet_data_size;

  payload = buf + packet_header_size;

  // extract key parameters from the header
  const int offset_second = getVDIFFullSecond (header_ptr) - start_second;
  const int frame_number  = getVDIFFrameNumber (header_ptr);

  // calculate the byte offset for this frame within the data stream
  int64_t byte_offset = offset_second * bytes_per_second + frame_number * packet_data_size;

//  cerr << "offset_second=" << offset_second << " frame_number=" << frame_number 
//       << " byte_offset=" << byte_offset << endl;

  return (int64_t) byte_offset;
}

inline int spip::UDPFormatVDIF::insert_last_packet (char * buffer)
{
  memcpy (buffer, payload, packet_data_size);
  return 0;
}

// generate the next packet in the sequence
inline void spip::UDPFormatVDIF::gen_packet (char * buf, size_t bufsz)
{
  // packets are packed in TF order 
  // write the new header
  encode_header (buf);

  // increment our "channel number"
  header.threadid++;

  if (header.threadid > end_channel)
  {
    header.threadid = start_channel;

    // generate the next VDIF header 
    nextVDIFHeader (&header, packets_per_second);
  }
}

void spip::UDPFormatVDIF::print_packet_header()
{
  cerr << "VDIF packet header: "
       << " second=" << getVDIFFrameSecond (&header) 
       << " frame=" << getVDIFFrameNumber (&header) << endl;
}
