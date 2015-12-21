/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/SPEADBeamFormerConfig.h"

#include <iostream>
#include <cstring>
#include <math.h>

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <stdint.h>
#endif

using namespace std;

spip::SPEADBeamFormerConfig::SPEADBeamFormerConfig()
{
  ndim = 2;
  reset();
}

spip::SPEADBeamFormerConfig::~SPEADBeamFormerConfig()
{
}

void spip::SPEADBeamFormerConfig::reset ()
{
  rx_udp_ip_str = "";
  rx_udp_port = 0;
  sync_time = 0;
  scale_factor_timestamp = 0;
  n_chans = 0;
  requant_bits = 0;
  fft_shift = 0;
  feng_pkt_len = 0;
  centre_freq = 0;
  adc_sample_rate = 0;
  n_ants = 0;
  bandwidth = 0;
  adc_bits = 0;
  n_bengs = 0;
  xeng_acc_len = 0;
  beng_out_bits_per_sample = 0;
  beam_weights.resize (0);
  input_labels.resize (0);
  raw_samples_desc = 0;
  raw_timestamp_desc = 0;
}

void spip::SPEADBeamFormerConfig::parse_item (const spead2::recv::item &item)
{
  switch (item.id)
  {
    case SPEAD_ITEM_DESC_ABSOLUTE:
      break;

    case SPEAD_CBF_N_CHANS:
      n_chans = item_ptr_48u (item.ptr);
      break;

    case SPEAD_CBF_FFT_SHIFT:
      fft_shift = item_ptr_48u (item.ptr);
      break;

    case SPEAD_CBF_FENG_PKT_LEN:
      feng_pkt_len = item_ptr_48u (item.ptr);
      break;

    case SPEAD_CBF_RX_UDP_IP_STR:
      rx_udp_ip_str.resize (item.length);
      memcpy (&rx_udp_ip_str[0], item.ptr, item.length);
      break;

    case SPEAD_CBF_CENTRE_FREQ:
      centre_freq = item_ptr_64f (item.ptr);
      break;

    case SPEAD_CBF_ADC_SAMPLE_RATE:
      adc_sample_rate = item_ptr_64u (item.ptr);
      break;

    case SPEAD_CBF_N_ANTS:
      n_ants = item_ptr_48u (item.ptr);
      break;

    case SPEAD_CBF_BANDWIDTH:
      bandwidth = item_ptr_64f (item.ptr);
      break;

    case SPEAD_CBF_RX_UDP_PORT:
      rx_udp_port = item_ptr_48u (item.ptr);
      break;

    case SPEAD_CBF_ADC_BITS:
      adc_bits = item_ptr_48u (item.ptr);
      break;

    case SPEAD_CBF_REQUANT_BITS:
      requant_bits = item_ptr_48u (item.ptr);
      break;

    case SPEAD_CBF_N_BENGS:
      n_bengs = item_ptr_48u (item.ptr);
      break;

    case SPEAD_CBF_XENG_ACC_LEN:
      xeng_acc_len = item_ptr_48u (item.ptr);
      break;

    case SPEAD_CBF_BENG_OUT_BITS_PER_SAMPLE:
      beng_out_bits_per_sample = item_ptr_48u (item.ptr);
      break;

    case SPEAD_CBF_INPUT_LABELLING:
      // TODO parse this properly
      input_labels.resize (10);
      break;

    case SPEAD_CBF_SYNC_TIME:
      sync_time = item_ptr_48u (item.ptr);
      break;

    case SPEAD_CBF_SCALE_FACTOR_TIMESTAMP:
      scale_factor_timestamp = item_ptr_64f (item.ptr);
      break;

    case SPEAD_CBF_BEAM_WEIGHTS:
      {
        unsigned n_weights = item.length / sizeof(int);
        beam_weights.resize(n_weights);
        memcpy (&beam_weights[0], item.ptr, item.length);
      }
      break;

    default:
      break;
  }
}

void spip::SPEADBeamFormerConfig::parse_descriptor (const spead2::descriptor &d)
{
  if (d.id == SPEAD_CBF_RAW_SAMPLES)
    raw_samples_desc = 1;
  if (d.id == SPEAD_CBF_RAW_TIMESTAMP)
    raw_timestamp_desc = 1;
}


bool spip::SPEADBeamFormerConfig::valid ()
{
  return (n_chans > 0 && sync_time > 0 && rx_udp_port > 0 &&
          requant_bits > 0 && fft_shift > 0 && feng_pkt_len > 0 &&
          adc_sample_rate > 0 && n_ants > 0 && adc_bits > 0  &&
          n_bengs > 0 && xeng_acc_len > 0 && beng_out_bits_per_sample > 0 &&
          raw_samples_desc == 1 && raw_timestamp_desc == 1);


}
// return the sampling rate in samples per second
double spip::SPEADBeamFormerConfig::get_bf_sample_rate ()
{
  return bandwidth / n_chans;
}

double spip::SPEADBeamFormerConfig::get_adc_to_bf_sampling_ratio ()
{
  return adc_sample_rate / get_bf_sample_rate();
}

unsigned spip::SPEADBeamFormerConfig::get_samples_per_heap()
{
  return feng_pkt_len;
}

unsigned spip::SPEADBeamFormerConfig::get_bytes_per_heap()
{
  unsigned bits_per_heap = n_chans * feng_pkt_len * beng_out_bits_per_sample * ndim;
  return bits_per_heap / 8;
}

uint64_t spip::SPEADBeamFormerConfig::get_utc_start_second (uint64_t timestamp)
{
  uint64_t seconds_since_last_sync = timestamp / adc_sample_rate;
  return (time_t) sync_time + seconds_since_last_sync;
}

uint64_t spip::SPEADBeamFormerConfig::get_obs_offset (uint64_t timestamp)
{
  uint64_t seconds_since_last_sync = timestamp / adc_sample_rate;
  uint64_t samples_remaining = timestamp - (seconds_since_last_sync * adc_sample_rate);

  uint64_t bits_per_sample = n_chans * beng_out_bits_per_sample * ndim;
  uint64_t bytes_per_sample = bits_per_sample / 8;

  // the obs offset is the number of bytes offset from the UTC tick on which the data flow starts
  uint64_t obs_offset = samples_remaining * bytes_per_sample;
  return obs_offset;
}

void spip::SPEADBeamFormerConfig::print_config ()
{
  cerr << "n_chans=" << n_chans << endl;
  cerr << "requant_bits=" << requant_bits << endl;
  cerr << "fft_shift=" << fft_shift << endl;
  cerr << "feng_pkt_len=" << feng_pkt_len << endl;
  cerr << "rx_udp_ip_str=" << rx_udp_ip_str << endl;
  cerr << "centre_freq=" << centre_freq << endl;
  cerr << "adc_sample_rate=" << adc_sample_rate << endl;
  cerr << "n_ants=" << n_ants << endl;
  cerr << "bandwidth=" << bandwidth << endl;
  cerr << "rx_udp_port=" << rx_udp_port << endl;
  cerr << "adc_bits=" << adc_bits << endl;
  cerr << "n_bengs=" << n_bengs << endl;
  cerr << "xeng_acc_len=" << xeng_acc_len << endl;
  cerr << "beng_out_bits_per_sample=" << beng_out_bits_per_sample << endl;
  cerr << "sync_time=" << sync_time << endl;
  cerr << "scale_factor_timestamp=" << scale_factor_timestamp << endl;
}

double spip::SPEADBeamFormerConfig::item_ptr_64u (const unsigned char * ptr)
{
  uint64_t value = 0;
  for (unsigned i=0; i<8; i++)
  {
    uint64_t tmp = (uint64_t) ptr[i];
    value |= tmp << ((7-i)*8);
  }
  return value;
}

double spip::SPEADBeamFormerConfig::item_ptr_64f (const unsigned char * ptr)
{
  uint64_t value = item_ptr_64u(ptr);
  double * dvalue = reinterpret_cast<double *>(&value);
  return *dvalue;
}

uint64_t spip::SPEADBeamFormerConfig::item_ptr_48u (const unsigned char * ptr)
{
  uint64_t value = 0;
  for (unsigned i=0; i<6; i++)
  {
    uint64_t tmp = (uint64_t) ptr[i];
    value |= tmp << ((5-i)*8);
  }
  return value;
}


