#ifndef _NKN_VPE_MFP_TS2MFU__
#define _NKN_VPE_MFP_TS2MFU__



#include <stdio.h>
#include "nkn_vpe_types.h"

#include "common.h"


//#include "nkn_vpe_mfu_defs.h"
#include "mfp_live_accum_ts.h"


#define TS_PKT_SIZE 188
#define MAX_SIZE_VIDEO_AU 250*188
#define MAX_SIZE_AUDIO_AU 250*188
#define MAX_SEI 5
#define TOT_MAX_SEI 10000
#define _BIG_ENDIAN_//define it for big_endian

#ifdef AUDIO_BIAS
#define AUDIO_BIAS_TH 10000
#define AUDIO_BIAS_MAX_OVER_TH 5
#endif


typedef struct nkn_vpe_vector_tt{
    uint8_t *data;
    uint32_t offset;
    uint32_t length;
    uint8_t *ascdata;
    uint32_t asclength;
}nkn_vpe_vector_t;

#include "nkn_vpe_pattern_search.h"



typedef struct ts2mfu_desc_tt{
    uint32_t num_audio_AU;
    uint32_t num_video_AU;
    uint32_t num_ts_pkts;
    uint32_t vdat_size;
    uint32_t adat_size;
    uint32_t sps_size;
    uint32_t pps_size;
    uint32_t mfub_box_size;
    uint32_t mfu_raw_box_size;
    uint16_t pid;
    mfub_t *mfub_box;
    mfu_rwfg_t  *mfu_raw_box;
    uint32_t *vid_PTS;
    uint32_t *vid_DTS;
    uint32_t *aud_PTS;
    uint32_t Is_sps_pps;
    uint32_t dump_size;
    uint32_t is_audio;
    uint32_t is_video;
    uint32_t is_big_endian;

    //vector table for video
    nkn_vpe_vector_t *vdat_vector;
    uint32_t num_vdat_vector_entries;
    uint32_t max_vdat_vector_entries;

    //vector table for audio
    nkn_vpe_vector_t *adat_vector;
    uint32_t num_adat_vector_entries;
    uint32_t max_adat_vector_entries;

    //audio sample duration and num_of_channels
    uint32_t sample_freq_index;
    uint32_t num_channels;

    //these variables are for maintaining continuity between fragments
    uint32_t num_tspkts_retx;
    uint32_t last_pkt_start_offset;
    uint32_t last_pkt_end_offset;
    uint32_t curr_chunk_frame_start;
    uint32_t curr_chunk_frame_end;

    //AV sync variables
    uint32_t audio_duration;
    uint32_t video_duration;
    uint32_t last_audio_index;
#ifdef AUDIO_BIAS
    audio_bias_t bias;
#endif

  //SEI details
  uint32_t SEI_pos[MAX_SEI];
  uint32_t SEI_size[MAX_SEI];
  uint32_t num_SEI;
  uint32_t tot_num_SEI;
    uint8_t is_SEI_extract;
    int8_t ignore_PTS_DTS;
    int8_t mfu_join_mode;
  uint32_t video_time_accumulated;

  //LATM parser related
  uint32_t num_latmmux_elements;
  struct LATMContext **latmctx;	

  

}ts2mfu_desc_t;

int32_t ts2mfu_desc_free(ts2mfu_desc_t *);

mfu_data_t*  mfp_ts2mfu(uint32_t* mstr_pkts, uint32_t mstr_npkts,
	uint32_t* slve_pkts, uint32_t slve_npkts,
	accum_ts_t *accum, ts2mfu_cont_t *);

int32_t mfu_write_mfub_box(uint32_t, mfub_t*, uint8_t*);
int32_t mfu_write_vdat_box(accum_ts_t *accum, 
	uint32_t, uint8_t*, ts2mfu_desc_t *);
int32_t mfu_write_adat_box(uint32_t, uint8_t*, ts2mfu_desc_t *);
int32_t mfu_write_rwfg_box(uint32_t, mfu_rwfg_t*, uint8_t*,
	ts2mfu_desc_t*);
int32_t mfu_write_mfu_start_box(uint32_t, uint8_t*, uint32_t);


uint32_t mfu_conveter_get_AU_num(uint32_t* , uint32_t , uint8_t *);
int32_t mfu_get_sample_duration(uint32_t *, uint32_t *, uint32_t, uint32_t, uint32_t );
int32_t mfu_get_composition_duration(uint32_t *, uint32_t *,
				     uint32_t *,uint32_t );



int32_t mfu_converter_get_timestamp(
	uint8_t *pkt, 
	int32_t *frame_type, 
	uint16_t *,
	uint32_t *pts, uint32_t *dts, 
	uint32_t (*handle)(uint8_t *, uint32_t *));

#endif
