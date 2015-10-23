#ifndef _PATTERN_SEARCH_
#define _PATTERN_SEARCH_


#ifdef __cplusplus
extern "C" {
#endif


#include <stdio.h>
#include <stdlib.h>

#include "nkn_memalloc.h"
#include "nkn_debug.h"


typedef struct pattern_ctxt{
	uint32_t *ts_pattern;
	uint32_t array_depth;
	uint32_t pattern_start;
	uint32_t pattern_end;
}pattern_ctxt_t;

int32_t anomaly_correction_with_average_ts(
	uint32_t *sample_duration,
	uint32_t num_frames,
	uint32_t *tot_t);


pattern_ctxt_t * create_pattern_ctxt(uint32_t num_AU);

int32_t delete_pattern_ctxt(pattern_ctxt_t *pat_ctxt);


int32_t search_pattern_ctxt(pattern_ctxt_t *pat_ctxt,
	uint32_t sample_duration);

int32_t insert_pattern(pattern_ctxt_t *pat_ctxt,
	uint32_t sample_duration);

int32_t comp_duration_validator(
	uint32_t *sample_duration,
	uint32_t *PTS,
	uint32_t *DTS,
	uint32_t num_AU);


#if 0

uint32_t validate_ptsentry_fw(
	video_frame_info_t *cur_frame,
	uint32_t avg_samp_duration);

uint32_t validate_ptsentry_bw(
	video_frame_info_t *cur_frame,
	uint32_t avg_samp_duration);

uint32_t validate_dtsentry_fw(
	video_frame_info_t *cur_frame,
	uint32_t avg_samp_duration);

uint32_t validate_dtsentry_bw(
	video_frame_info_t *cur_frame,
	uint32_t avg_samp_duration);

typedef enum pred_direction{
	forward = 0,
	backward = 1,
}pred_direction_e;

typedef struct anomaly_sample{
	uint32_t frame_num;
	pred_direction_e prediction_direction;
	uint32_t try_again;
	uint32_t valid_entry;
}anomaly_sample_t;

int32_t pts_anomaly_correction(
	struct video_frame_info *first_frame,
	uint32_t num_frames);

int32_t dts_anomaly_correction(
	struct video_frame_info *first_frame,
	uint32_t num_frames);
#endif





#ifdef __cplusplus
}
#endif



#endif

