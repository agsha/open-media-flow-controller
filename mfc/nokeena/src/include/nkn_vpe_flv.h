/*
 *
 * Filename:  flv.h
 * Date:      2009/02/06
 * Module:    Basic File Format Definitiions for FLV Format V9
 *
 * (C) Copyright 2009 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#ifndef _NKN_VPE_FLV_DEFS_
#define _NKN_VPE_FLV_DEFS_

#include "nkn_vpe_types.h"

/* FLV container format specfications from Adobe FLV File Format Specification V9/10
 * http://www.adobe.com/devnet/flv/pdf/video_file_format_spec_v10.pdf
 * Basic defs include
 * - FLV tag definition
 * - FLV header (fixed) definition
 * - FLV tag type definitions for Audio/Video and Meta tags
 * - Codec type definitions for AVC, H263, VP6, MP3 and AAC
 * - Frame type definitions for Keyframe, Interframe
 */
#pragma pack(push, 1)

typedef struct tag_flv_header {
    uint8_t        signature[3]; 
    uint8_t        version; 
    uint8_t        flags;
    uint32_t       offset; 
}nkn_vpe_flv_header_t;

#define NKN_VPE_FLV_TAG_TYPE_AUDIO  (8)
#define NKN_VPE_FLV_TAG_TYPE_VIDEO  (9)
#define NKN_VPE_FLV_TAG_TYPE_META   (18)

typedef struct tag_flv_video_data{
    uint8_t video_info;
    uint8_t *au;
}nkn_vpe_flv_video_data_t;

typedef struct tag_flv_tag {
    uint8_t    type;
    uint24_t   body_length;
    uint24_t   timestamp;
    uint8_t   timestamp_extended; 
    uint24_t   stream_id; 
} nkn_vpe_flv_tag_t;

#define NKN_VPE_FLV_VIDEO_CODEC_SORENSEN_H263   2
#define NKN_VPE_FLV_VIDEO_CODEC_SCREEN_VIDEO    3
#define NKN_VPE_FLV_VIDEO_CODEC_ON2_VP6         4
#define NKN_VPE_FLV_VIDEO_CODEC_ON2_VP6_ALPHA   5
#define NKN_VPE_FLV_VIDEO_CODEC_SCREEN_VIDEO_V2 6
#define NKN_VPE_FLV_VIDEO_CODEC_AVC             7
#define NKN_VPE_FLV_AUDIO_CODEC_AAC             10
#define NKN_VPE_FLV_AUDIO_CODEC_MP3             2

#define NKN_VPE_FLV_VIDEO_FRAME_TYPE_KEYFRAME               1
#define NKN_VPE_FLV_VIDEO_FRAME_TYPE_INTERFRAME             2
#define NKN_VPE_FLV_VIDEO_FRAME_TYPE_DISPOSABLE_INTERFRAME  3

static inline int32_t
nkn_vpe_flv_get_timestamp(nkn_vpe_flv_tag_t tag) 
{
    return ((nkn_vpe_convert_uint24_to_uint32((tag).timestamp) + ((tag).timestamp_extended << 24)));
}

#pragma pack(pop)

#endif
