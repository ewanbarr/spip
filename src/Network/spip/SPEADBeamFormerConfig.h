#ifndef __SPEADBeamFormerConfig_h
#define __SPEADBeamFormerConfig_h

#include "spead2/recv_udp.h"
#include "spead2/recv_live_heap.h"
#include "spead2/recv_ring_stream.h"

#include <cstdlib>
#include <string>
#include <inttypes.h>

#define SPEAD_ITEM_DESC_ABSOLUTE 0x5
#define SPEAD_CBF_N_CHANS 0x1009
#define SPEAD_CBF_REQUANT_BITS 0x1020
#define SPEAD_CBF_FFT_SHIFT 0x101e
#define SPEAD_CBF_FENG_PKT_LEN 0x1021
#define SPEAD_CBF_RX_UDP_IP_STR 0x1024
#define SPEAD_CBF_CENTRE_FREQ 0x1011
#define SPEAD_CBF_ADC_SAMPLE_RATE 0x1007
#define SPEAD_CBF_N_ANTS 0x100a
#define SPEAD_CBF_BANDWIDTH 0x1013
#define SPEAD_CBF_RX_UDP_PORT 0x1022
#define SPEAD_CBF_ADC_BITS 0x1045
#define SPEAD_CBF_REQUANT_BITS 0x1020
#define SPEAD_CBF_N_BENGS 0x100f
#define SPEAD_CBF_XENG_ACC_LEN 0x101f
#define SPEAD_CBF_BENG_OUT_BITS_PER_SAMPLE 0x1050
#define SPEAD_CBF_INPUT_LABELLING 0x100e
#define SPEAD_CBF_SYNC_TIME 0x1027
#define SPEAD_CBF_SCALE_FACTOR_TIMESTAMP 0x1046
#define SPEAD_CBF_SCALE_FACTORS_START 0x1400
#define SPEAD_CBF_SCALE_FACTORS_END 0x1500
#define SPEAD_CBF_BEAM_WEIGHTS 0x2000

#define SPEAD_CBF_RAW_TIMESTAMP 0x1600
#define SPEAD_CBF_RAW_SAMPLES 0x5000

namespace spip {

  class SPEADBeamFormerConfig {

    public:

      SPEADBeamFormerConfig();

      ~SPEADBeamFormerConfig();

      void reset ();

      void parse_item (const spead2::recv::item&);

      void parse_descriptor (const spead2::descriptor&);

      void print_config ();

      static double item_ptr_64u (const unsigned char * ptr);

      static double item_ptr_64f (const unsigned char * ptr);

      static uint64_t item_ptr_48u (const unsigned char * ptr);

      bool valid ();

      // return the ratio of adc to bf samples per second
      double get_adc_to_bf_sampling_ratio();

      // return the beam former sample rate in samples per second
      double get_bf_sample_rate();

      unsigned get_bytes_per_heap();

      unsigned get_samples_per_heap();

      uint64_t get_utc_start_second (uint64_t timestamp);

      uint64_t get_obs_offset (uint64_t timestamp);

      time_t get_sync_time () { return (time_t) sync_time; };

      uint64_t get_adc_sample_rate () { return (uint64_t) adc_sample_rate; };

    protected:

    private:

      std::string rx_udp_ip_str;

      uint64_t rx_udp_port;

      uint64_t sync_time;

      double scale_factor_timestamp;

      uint64_t n_chans;

      uint64_t requant_bits;

      uint64_t fft_shift;

      uint64_t feng_pkt_len;

      double centre_freq;

      uint64_t adc_sample_rate;

      uint64_t n_ants;

      double bandwidth;

      uint64_t adc_bits;

      uint64_t n_bengs;

      uint64_t xeng_acc_len;

      uint64_t beng_out_bits_per_sample;

      std::vector<int> beam_weights;

      unsigned ndim;

      bool is_valid;

      std::vector<std::string> input_labels;
    
      unsigned int raw_samples_id;

      char raw_timestamp_desc;

  };

}

#endif
