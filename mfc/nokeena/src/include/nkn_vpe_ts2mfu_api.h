/*
 *
 * Filename:  nkn_vpe_ts2mfu_api.h
 * Date:      2010/08/02
 * Module:    implements the ts2mfu file converter callbacks
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _TS2MFU_CONV_
#define _TS2MFU_CONV_

#include <stdio.h>
#include <inttypes.h>
#include "nkn_vpe_bitio.h" 
#include "nkn_vpe_types.h"
#include "nkn_vpe_ts_segmenter_api.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mfu_writer.h"
#include "nkn_vpe_sync_http_read.h"
#include "mfp/mfp_live_accum_create.h"
#define MFP_TS_MAX_PKTS 30000
#define MAX_CHUNKS 30000
#define TS_READ_SIZE_PROFILE 1048476
#ifdef __cplusplus
extern "C" {
#endif

    typedef struct tag_chunk_desc{
	uint32_t num_entries;
	vpe_olt_t *sam_desc;
    }chunk_desc_t;


    typedef struct tag_ts_mfu_profile_builder{
	uint16_t aud_pid1;
	uint16_t aud_pid2;
	uint16_t vid_pid;
	uint16_t pmt_pid;
	uint32_t pmt_offset;
	uint8_t* data;
	bitstream_state_t* bs;
	uint32_t data_len;
        uint32_t num_chunks;//chunk number for each block
	uint32_t PMT_data_offset;
	uint32_t chunk_duration[MAX_CHUNKS];
	uint32_t duration;
	char* ts_name;
	int32_t bitrate;
	void* io_desc_ts;
	uint32_t curr_chunk_num;
	chunk_desc_t **chunk_desc;
	uint32_t vid_pkts[MFP_TS_MAX_PKTS];
	uint32_t vid_pkts_time[MFP_TS_MAX_PKTS];
	uint32_t vid_npkts;
	uint32_t aud_pkts[MFP_TS_MAX_PKTS];
	uint32_t aud_pkts_time[MFP_TS_MAX_PKTS];
        uint32_t aud_npkts;
	int32_t curr_profile_start_time;
	uint32_t first_I_offset;
	uint32_t seg_start_offset;
	uint32_t num_vid_payload_start_pkts;
	uint32_t num_aud_payload_start_pkts;
	ts_segmenter_ctx_t *seg_ctx;
        uint32_t pmf_chunk_time;
        uint32_t seek_offset;
        uint8_t is_data_complete;
        int32_t bytes_remaining;
        uint32_t block_num;
        uint8_t is_span_data;
        uint32_t tot_size;
        uint32_t tot_num_chunks;
        uint32_t last_samp_len;
        uint32_t act_bit_rate;
        uint32_t num_pkts_per_second;
        uint32_t to_read;
        uint32_t prev_bytes;
        io_handlers_t *iocb;
        accum_ts_t *accum;
    }ts_mfu_profile_builder_t;
    /**
     * \struct mfu builder, maintains state for the mfu
     *  write/processing
     */
    typedef struct tag_ts_mfu_builder {
	int32_t n_profiles;
	ts_mfu_profile_builder_t profile_info[MAX_PROFILES];
	int32_t num_pmf;
	int32_t chunk_time;
	int32_t streaming_type;
	int32_t num_chunks;
        uint8_t is_data_complete;
    } ts_mfu_builder_t;


int32_t
ts_mfu_builder_init(int32_t, char**, int32_t*, int32_t, int32_t, int32_t, 
		int32_t, ts_mfu_builder_t**);

int32_t
ts_mfu_bldr_update(ts_mfu_builder_t*, int32_t);
int32_t
ts_mfu_bldr_get_chunk_desc(ts_mfu_profile_builder_t *);

#if !defined(INC_ALIGNER)
int32_t
ts_segment_data(bitstream_state_t*, vpe_olt_t*, ts_segmenter_ctx_t *, 
		uint32_t, int32_t*);
#else
int32_t
ts_segment_data(uint8_t*, uint32_t, vpe_olt_t*, ts_segmenter_ctx_t *,
		uint32_t, int32_t*, uint32_t, uint32_t);

#endif
int32_t
get_pids(uint8_t*, uint32_t, uint16_t*, uint16_t*, uint16_t*,
	 uint16_t*);
int32_t
ts_mfu_calculate_num_chunks(uint8_t*, uint32_t*, uint32_t, uint32_t *, uint32_t, uint32_t, uint32_t *);

static int32_t
init_ts_seg_ctx(ts_segmenter_ctx_t**);

int32_t
ts_mfu_conversion(ts_mfu_profile_builder_t*, mfu_data_t**, uint32_t, uint32_t);

int32_t
update_aud_vid_offset(uint8_t*, vpe_olt_t*, uint16_t, uint32_t*,
		      uint32_t*, uint32_t*, uint16_t, uint32_t*,
		      uint32_t*, uint32_t*, uint32_t*, uint32_t*);

void cleanup_chunk_desc_contents(
    ts_mfu_profile_builder_t *bldr_ts_mfu,
    uint32_t chunk_num);

void cleanup_chunk_desc(
	ts_mfu_builder_t *bldr,
	int32_t strm_id);

void
cleanup_ts_mfu_builder(ts_mfu_builder_t*);

int32_t
ts_mfu_update_chunk_desc(uint8_t*, uint32_t*, uint32_t, uint32_t*,
                         uint32_t, uint32_t, uint32_t*, chunk_desc_t**, uint32_t, uint32_t);

#ifdef __cplusplus
}
#endif

#endif //_TS2MFU_CONV_
