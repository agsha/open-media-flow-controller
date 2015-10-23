/*
 *
 * Filename:  nkn_vpe_ts2iphone_api.h
 * Date:      2010/08/02
 * Module:    implements the ts to iphone converter callbacks
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _TS_SEGMENTER_API_
#define _TS_SEGMENTER_API_

#include <stdio.h>
#include <inttypes.h>

#include "nkn_vpe_types.h"
#include "mfp_file_converter_intf.h"
#include "nkn_vpe_mfp_ts2mfu.h"
#include "nkn_vpe_bitio.h"
#include "nkn_vpe_mpeg2ts_parser.h"

#define TS_SEGMENTER_SEGMENT_READY 0x1
#define TS_SEGMENTER_BUF_COMPLETE 0x2
#define TS_CLK_FREQ 90

#ifdef __cplusplus 
extern "C" {
#endif

    /**
     * \struct - TS segmenter context 
     * NOTE: the output_path, video_name and uri_prefix are shared
     * between the segmenter context and the segmenter write context
     * and hence __only__ the writer can free the context when the
     * last_write_flag in the writer context is set
     */
    typedef struct tag_ts_segmenter_ctx {
	int32_t segment_duration;
	int32_t curr_segment_num;
	int32_t curr_profile_start_time;
	Ts_t *ts_pkt_props;
	Misc_data_t *codec_span_data;
	uint8_t is_first_PTS;
	uint8_t is_first_Iframe_PTS;
        int64_t prev_Iframe_PTS;
        int64_t last_pts;
	int64_t  curr_frag_last_PTS;
	int64_t first_Iframe_PTS;
	int64_t second_Iframe_PTS;
	int64_t keyframe_interval;
	uint8_t is_keyframe_interval;
	uint32_t num_Iframe;
    } ts_segmenter_ctx_t;

    /* initializes a TS segmenter contex
     * @param fconv_in - data type having all the required input
     * params
     * @param out [out] - fully populated valid TS Segmenter context
     * @return returns 0 on success and negative number on error
     */
    int32_t init_ts_segmenter_ctx(file_conv_ip_params_t *fconv_ip,
				  ts_segmenter_ctx_t **out);

    /**
     * segments the data pointed to in the bitstream buffer. returns
     * after one segment has been constructed. if in the current run
     * the buffer has been consumed without a segment being created
     * then we return with TS_SEGMENTER_BUF_COMPLETE indicating the
     * caller to read more data to proceed
     * @param seg_ctx - the segmenter context
     * @param bs - the bitstream handler containing the buffer
     * @param ol [out] - a [offset, length] pair that describes the
     * write buffer
     * @return returns TS_SEGMENTER_BUF_COMPLETE if the segmenter
     * requires more data and TS_SEGMENTER_SEGMENT_READY if a segment
     * has been created and is ready for a write
     */
    int32_t ts_segmenter_segment_data(ts_segmenter_ctx_t *seg_ctx, 
				      bitstream_state_t *bs,
				      vpe_olt_t *ol);

    /**
     * clean up the segmenter context
     */
    void cleanup_ts_segmenter(ts_segmenter_ctx_t *seg);

    /** 
     * resets the segmenter context
     * @param ctx - the segmenter context that needs to be reset
     */
    /** 
     * resets the segmenter context
     * @param ctx - the segmenter context that needs to be reset
     */
    static inline int32_t
    ts_segmenter_reset(ts_segmenter_ctx_t *ctx)
    {
	Ts_reset(ctx->ts_pkt_props);
	ctx->curr_segment_num = 1;
	ctx->curr_profile_start_time = 0;
	ctx->is_first_PTS = 0;
	ctx->is_first_Iframe_PTS = 0;
	ctx->prev_Iframe_PTS = 0;
	ctx->last_pts = 0;
	ctx->curr_frag_last_PTS = 0;
	ctx->first_Iframe_PTS = 0;
	ctx->second_Iframe_PTS = 0;
	ctx->keyframe_interval = 0;
	ctx->num_Iframe = 0;
	return 0;
    }


#ifdef __cplusplus
}
#endif

#endif //_TS_SEGMENTER_API_
