#ifndef _MPEG4_ES_
#define _MPEG4_ES_

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

//#include "nkn_vpe_bitio.h"

#ifdef __cplusplus
extern "C"{
#endif

#define ODF_ESD_TAG 0x03
#define ODF_DCC_TAG 0x04
#define ODF_SLC_TAG 0x06
#define ODF_DECODER_SPECIFIC_INFO_TAG 0x05

    /* all data-types with a pre-fix mpeg4_es are from ISO/IEC 14496-1*/
    /* all data-types with a pre-fix m4a are from ISO/IEC 14496-3 */

    /*****************************************************************
     *         Elementary stream descriptor organization
     *         ----------------------------------------
     *         |     object descriptor tag (u8)       |
     *         ----------------------------------------
     *         |   object descriptor size (bitpacked) |
     *         ----------------------------------------
     *         |        ES - specific headers         |
     *         ----------------------------------------
     *         |      decoder config record           |
     *         ----------------------------------------
     *         |    sync layer config record          |
     *         ----------------------------------------
     ****************************************************************/

    /* ISO/IEC 14496-1 decoder configuration record, Section 2.6.6 */
    typedef struct tag_mpeg4_decoder_config_t {

	uint8_t profile_level_indicator;
	uint8_t stream_type;
	uint8_t upstream;
	uint8_t specific_info_flag;
	uint32_t decoder_buffer_size;
	uint32_t max_bitrate;
	uint32_t avg_bitrate;
	uint8_t *specific_info;
	size_t specific_info_size;

    }mpeg4_decoder_config_t;

    /* definition from ISO/IEC 14496-3, Subpart4 */
    typedef struct tag_m4a_GA_specific_config {

	uint32_t core_coder_delay;
	uint32_t layer_num;
	uint32_t num_sub_frames;
	uint32_t layer_length;
	uint32_t aac_section_data_resilience_flag;
	uint32_t aac_scale_factore_data_resilience_flag;
	uint32_t aac_spectral_data_resilience_flag;
	uint32_t extn_flag3;

    }m4a_GA_specific_config_t;

    /* definition from ISO/IEC 14496-3, Section 1:Subclause 1.6 */
    typedef struct tag_m4a_decoder_specific_info {

	uint32_t base_object_type;
	uint32_t sampling_freq_idx;
	uint32_t sampling_freq;
	uint32_t channel_config;
	int32_t sbr_present_flag;
	uint32_t extn_sampling_freq_idx;
	uint32_t extn_sampling_freq;
	uint32_t extn_audio_object_type;
	m4a_GA_specific_config_t *GA_si;
	uint8_t *ascData;
	uint32_t ascLen;

    }m4a_decoder_specific_info_t;

    /* ISO/IEC 14496-1 sync layer config record, Section 2.6.8 */
    typedef struct tag_mpeg4_sync_layer_config_t {
	uint8_t use_au_start_flag;
	uint8_t use_au_end_flag;
	uint8_t use_au_rap_flag;
	uint8_t rap_only_flag;
	uint8_t use_padding_flag;
	uint8_t use_ts_flag;
	uint8_t use_idle_flag;
	uint8_t use_duration_flag;
	uint32_t ts_resolution;
	uint32_t ocr_resolution;
	uint32_t time_scale;
	uint8_t ts_length;
	uint8_t ocr_length;
	uint8_t au_length;
	uint8_t instant_bitrate_length;
	uint8_t degradation_priority_length;
	uint8_t sequence_num_length;
	uint8_t packet_sequence_num_length;

	uint16_t au_rate;
	uint16_t cu_rate;
	uint64_t start_cts;
	uint64_t start_dts;
    }mpeg4_sync_layer_config_t;

    /* container for all MPEG-4 systems to RTP/RTSP/SDP mapping info */
    typedef struct tag_mpeg4_au_header {
	
	/*stream info */
	uint32_t stream_type; // Stream Type 4 - Video, 5 - Audio
	uint32_t pl_id; //Level Indicator in MPEG-4 Systems
	uint32_t object_type_indicator; //profile indicator in MPEG-4 Systems
	
	/* RFC 3640 - RTP packetization for AAC */
	uint64_t CTS_delta;
	uint8_t CTS_flag;
	uint64_t DTS;
	size_t au_size;
	uint32_t rap_flag;
	uint32_t au_sequence_num;
	uint32_t last_au_sequence_num;
	
	/* RFC 3640 - RTP spec for values to be encoded in SDP */
	uint32_t size_length;
	uint32_t index_length;
	uint32_t index_delta_length;
	char mode[20];
	
    }mpeg4_au_header_t;

    /* ISO/IEC 14496-1, Elementary stream descriptor, Section 2.6.5 */
    typedef struct tag_mpeg4_es_desc {
	uint16_t esid;
	uint16_t ocresid;
	uint16_t dependsOn_esid;
	uint8_t stream_priority;
	uint8_t url_length;
	uint8_t *url;
	mpeg4_decoder_config_t *dc;
	mpeg4_sync_layer_config_t *slc;
	m4a_decoder_specific_info_t *dsi;
	mpeg4_au_header_t *auh;
    }mpeg4_es_desc_t;

    /* init and cleanup functions */
    m4a_decoder_specific_info_t* initAscInfo(void);
    void delete_asc_info(m4a_decoder_specific_info_t *dc);


    /* m4a specific functions */
    int32_t parseAscInfo(GetBitContext *gb, m4a_decoder_specific_info_t *m4a_dec_si);

#ifdef __cplusplus
}
#endif


#endif
