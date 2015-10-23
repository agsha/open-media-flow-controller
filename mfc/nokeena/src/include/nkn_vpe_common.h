/*
 *
 * Filename:  common.h
 * Date:      2009/02/06
 * Module:    FLV pre - processing for Smooth Flow
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#ifndef __COMMON__
#define __COMMON__

#include <stdio.h>
#include "nkn_vpe_types.h"
#include "nkn_vpe_metadata.h"

#ifdef __cplusplus
extern "C"{
#endif

    typedef struct _tag_meta_data{
	uint32_t iframe_interval;
	uint32_t n_chunks;
	uint32_t smooth_flow_duration;
	uint32_t fps;
	uint32_t sequence_duration;
	int bitrate;
	char *video_name;
	uint32_t n_iframes;
	uint64_t content_length;
	avcc_config *avcc;
        uint32_t codec_id;
        uint32_t container_id;
    }meta_data;

#ifdef __cplusplus
}
#endif

#endif //__COMMON__
