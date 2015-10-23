/*
 *
 * Filename:  nkn_vpe_ts_segmenter_api.c
 * Date:      2010/08/02
 * Module:    implements the ts segmeneter
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include <stdio.h>
#include <string.h>

#include "nkn_vpe_ts_segmenter_api.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_memalloc.h"
#include "nkn_vpe_bitio.h"

//#define TS_CLK_FREQ 90

/***************************************************************
 *             HELPER FUNCTIONS (STATIC)
 **************************************************************/
static inline void
ts_segmenter_setup_olt(bitstream_state_t *bs, size_t start_offset,
		       int32_t PTS, vpe_olt_t *ol);
static inline void
ts_segmenter_setup_olt_eob(bitstream_state_t *bs, size_t start_offset,
			   int32_t PTS, vpe_olt_t *ol);


/* initializes a TS segmenter contex
 * @param fconv_in - data type having all the required input
 * params
 * @param out [out] - fully populated valid TS Segmenter context
 * @return returns 0 on success and negative number on error
 */
int32_t
init_ts_segmenter_ctx(file_conv_ip_params_t *fconv_ip,
		      ts_segmenter_ctx_t **out)
{
    ts_segmenter_ctx_t *ctx;
    int32_t err;

    err = 0;
    ctx = NULL;
    *out = NULL;

    *out = ctx = (ts_segmenter_ctx_t*)\
	nkn_calloc_type(1, sizeof(ts_segmenter_ctx_t),
			mod_vpe_ts_segmenter_ctx_t);

    if (!ctx) {
	return -E_VPE_PARSER_NO_MEM;
    }
    
    ctx->ts_pkt_props = (Ts_t*)\
	nkn_calloc_type(1, sizeof(Ts_t),
			mod_vpe_ts_tplay_parser);
    if (!ctx->ts_pkt_props) {
	free(ctx);
	return -E_VPE_PARSER_NO_MEM;
    }

    /* initialize the TS packet props, wish there was a clean init
     * function to do this
     */
    Ts_alloc(ctx->ts_pkt_props);
    ctx->ts_pkt_props->vsinfo->num_pids=0;
    ctx->ts_pkt_props->asinfo->num_pids=0;
    ctx->ts_pkt_props->num_pids=0;
    ctx->ts_pkt_props->num_pkts=0;
    ctx->ts_pkt_props->pas->prev_cc=0x0F;

    ctx->codec_span_data = (Misc_data_t*)\
	nkn_calloc_type(1, sizeof(Misc_data_t),
			mod_vpe_ts_tplay_parser);
    ctx->segment_duration = fconv_ip->key_frame_interval;
    ctx->curr_segment_num = 1;

    return err;
}

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
int32_t 
ts_segmenter_segment_data(ts_segmenter_ctx_t *seg_ctx,
			  bitstream_state_t *bs, 
			  vpe_olt_t *ol)
{
    int32_t frame_type, PTS, err;
    size_t segment_start_offset, segment_end_offset;
    uint16_t rpid;
    uint8_t *ts_pkt_data;
    size_t ts_pkt_size;
    tplay_index_record tplay_record;
    
    err = 0;
    segment_end_offset = 0;
    memset(&tplay_record, 0, sizeof(tplay_record));
    segment_start_offset = bio_get_pos(bs);

    /* re-scan the current packet so that the search for IDR frame
    will start from proper position(skipping IDR pkt and pointing to adjusant
    pkt of the IDR pkt) . for first time, dont change the position*/
    if (seg_ctx->curr_segment_num != 1) {
	bio_aligned_seek_bitstream(bs, TS_PKT_SIZE,
				   SEEK_CUR);
    }

    ts_pkt_size = TS_PKT_SIZE;
    while (bio_aligned_direct_read(bs, &ts_pkt_data, &ts_pkt_size)) {
	handle_pkt_tplay(ts_pkt_data, seg_ctx->ts_pkt_props, 
			 &tplay_record, seg_ctx->codec_span_data);
	/*assigning the first video PTS of the sequence */ 
	if(tplay_record.PTS != 0) {
	    if ((seg_ctx->curr_segment_num == 1) &&
                (!seg_ctx->is_first_PTS)){
		seg_ctx->codec_span_data->first_PTS =
		    tplay_record.PTS/TS_CLK_FREQ;
		seg_ctx->is_first_PTS = 1;
	    }
	}
	if (seg_ctx->codec_span_data->frame_type_pending) {
	    continue;
	}
	
	if (tplay_record.type == TPLAY_RECORD_PIC_IFRAME) {
	    seg_ctx->num_Iframe++;
	    /*reset the  tplay_record.type and frame_type_pending */
	    tplay_record.type = 0;
	    seg_ctx->codec_span_data->frame_type_pending = 1;
	    if(seg_ctx->curr_segment_num == 1) {
		seg_ctx->curr_profile_start_time =	  \
		    seg_ctx->curr_profile_start_time +	  \
		    (seg_ctx->curr_profile_start_time == 0) *	\
		    seg_ctx->codec_span_data->first_PTS;
	    } else {
		seg_ctx->curr_profile_start_time =        \
                    seg_ctx->curr_profile_start_time +    \
		    (seg_ctx->curr_profile_start_time == 0) *   \
		    (tplay_record.PTS/TS_CLK_FREQ) ;
	    }
	    
	    PTS = tplay_record.PTS/TS_CLK_FREQ -	\
		seg_ctx->curr_profile_start_time;
	    /*assigning the first I_frame PTS of i/p sequence*/
	    if (!seg_ctx->is_first_Iframe_PTS) {
		seg_ctx->prev_Iframe_PTS =
		    tplay_record.PTS/TS_CLK_FREQ;
		seg_ctx->is_first_Iframe_PTS = 1;
	    }
	    /*calculate the actual key_frame interval*/
	    if(seg_ctx->num_Iframe == 1) {
		seg_ctx->first_Iframe_PTS =
		    tplay_record.PTS/TS_CLK_FREQ;
	    } else if(seg_ctx->num_Iframe == 2) {
		seg_ctx->second_Iframe_PTS =
		    tplay_record.PTS/TS_CLK_FREQ;
	    } else if(seg_ctx->is_keyframe_interval == 0){ 
		seg_ctx->keyframe_interval = seg_ctx->second_Iframe_PTS -
		    seg_ctx->first_Iframe_PTS ;
		seg_ctx->is_keyframe_interval = 1;
	    }

	    /* we are going to segment based on the duration given
	       in the PMF file*/
	    if (PTS > seg_ctx->curr_segment_num
                *seg_ctx->segment_duration) {
		//if((tplay_record.PTS/TS_CLK_FREQ) >
		//seg_ctx->prev_Iframe_PTS) {
		/* end of fragment */
		seg_ctx->curr_segment_num++;

		//printf("segment boundary at I-frame PTS %d\n", PTS);
		/* store the last or previous I_frame PTS */
		seg_ctx->last_pts = PTS;
		seg_ctx->prev_Iframe_PTS =
		    tplay_record.PTS/TS_CLK_FREQ;

		/* copy the offset length in the current buffer */
		ts_segmenter_setup_olt(bs, segment_start_offset, PTS,
				       ol);


		/* re-scan the current packet as a the first packet of
		 * the next segment */

		bio_aligned_seek_bitstream(bs, -TS_PKT_SIZE,
					   SEEK_CUR); 
		/* indicate to the converter that the SEGMENT is ready
		 * for write*/
		err = TS_SEGMENTER_SEGMENT_READY;
		return err;
	    }
	    
	}

	if (tplay_record.PTS > 0) {
	    /* store the last PTS */
	    PTS = tplay_record.PTS/TS_CLK_FREQ -        \
		seg_ctx->curr_profile_start_time;
	    seg_ctx->last_pts  = PTS;
	    seg_ctx->curr_frag_last_PTS =
		tplay_record.PTS/TS_CLK_FREQ;

	    //printf("PTS is %d\n", PTS);
	}
    }
    /* no segment found, dump the entire buffer parsed in the current
     * run as a part of the current segment
     */
    ts_segmenter_setup_olt_eob(bs, segment_start_offset, PTS, ol);
    err = TS_SEGMENTER_BUF_COMPLETE;

    /** if no video PID is found return error */
    if(seg_ctx->last_pts == 0){
	DBG_MFPLOG("TS2_IPHONE", ERROR, MOD_MFPFILE,"The"
                   "No video pkts is found, so retur correspond error\n");
	err = -E_VPE_NO_VIDEO_PKT;
    }
    return err;
}

/**
 * clean up the segmenter context
 */
void
cleanup_ts_segmenter(ts_segmenter_ctx_t *seg)
{
    Ts_t *ts;
    if (seg) {
	if (seg->ts_pkt_props) {
	    ts = seg->ts_pkt_props;
	    Ts_free(ts);
	}
	if(seg->codec_span_data) {
	    free(seg->codec_span_data);
	}
	free(seg);
    }
    
}

static inline void
ts_segmenter_setup_olt_eob(bitstream_state_t *bs, size_t start_offset,
			   int32_t PTS, vpe_olt_t *ol)

{
    /* store the offset, length, time */
    ol->offset = start_offset;
    ol->length = bio_get_pos(bs) - start_offset;
    ol->time = PTS;
}

static inline void
ts_segmenter_setup_olt(bitstream_state_t *bs, size_t start_offset,
		       int32_t PTS, vpe_olt_t *ol)

{
    /* store the offset, length, time */
    ol->offset = start_offset;
    ol->length = bio_get_pos(bs) - TS_PKT_SIZE - start_offset; 
    ol->time = PTS;
}
