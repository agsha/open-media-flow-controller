#ifndef _NKN_VPE_RAW_H264_
#define _NKN_VPE_RAW_H264_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

//#include "nkn_vpe_bitio.h"
#include "nkn_vpe_ismv_mfu.h"

#define VPE_OUTPUT_PATH ""

#define SUCCESS 1
#define FAILURE 0

#define MAX_VIDEO_STREAMS 5
#define MAX_AUDIO_STREAMS 10

#define NALU_DELIMIT 0x00000001
#define H264_VIDEO 1;

typedef struct params1{
    uint8_t size;
    uint8_t length;
    uint8_t* param_set;
    uint8_t value;
}stream_param_sets_t;

typedef struct avc{
    // uint8_t version;
    uint8_t AVCProfileIndication;
    uint8_t profile_compatibility;
    uint8_t AVCLevelIndication;
    uint8_t NALUnitLength;
    uint8_t numOfSequenceParameterSets;
    uint8_t numOfPictureParameterSets;
    stream_param_sets_t* sps;
    stream_param_sets_t* pps;
}stream_AVC_conf_Record_t;


int32_t mfu_init_ind_video_stream(video_pt *);
int32_t mfu_free_ind_video_stream(video_pt *);
int32_t mfu_allocate_video_streams(video_pt *,int32_t );
int32_t mfu_get_pps_sps(void *, io_handlers_t *, video_pt *);
int32_t mp4_read_avc1(stream_AVC_conf_Record_t *,uint8_t *);
int32_t mfu_check_h264_profile_level(stream_AVC_conf_Record_t *);
//int32_t mp4_get_moov_parameters(FILE *,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), video_pt *);
int32_t mp4_get_stsd_info(uint8_t *,video_pt *);
int32_t  mp4_get_avcC_info(void *, io_handlers_t *, video_pt *);
//int32_t mp4_ret_track_id(uint8_t *);
int32_t mfu_write_sps_pps(stream_AVC_conf_Record_t *,moof2mfu_desc_t*);
//int32_t write_h264_samples( mdat_desc_t* ,uint8_t * ,FILE *);
//int32_t mfu_write_h264_samples(mdat_desc_t *,uint8_t*,video_pt*);
int32_t mfu_get_h264_samples(mdat_desc_t *, moof2mfu_desc_t*);
#endif
