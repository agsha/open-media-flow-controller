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

#ifndef _TS2IPHONE_CONV_
#define _TS2IPHONE_CONV_

#include <stdio.h>
#include <inttypes.h>
#include <sys/queue.h>

#include "nkn_vpe_types.h"
#include "nkn_vpe_bitio.h"
#include "nkn_vpe_ts_segmenter_api.h"
#include "mfp_file_converter_intf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TS_NEXT_PROFILE 0x1
#define TS_READ_DATA 0x2
#define TS_START_PARSING 0x4
#define TS_FLUSH_PENDING_DATA 0x8

    struct tag_olt_queue_elm {
	TAILQ_ENTRY(tag_olt_queue_elm) entries;
        vpe_olt_t *ol;
        bitstream_state_t *bs;
        uint8_t last_write_flag;			       
    };
    
    typedef struct tag_olt_queue_elm olt_queue_elm_t;

    /**
     *  \struct writer object which can be used to dump a TS segment 
     * NOTE: the output_path, video_name and uri_prefix are shared
     * between the segmenter context and the segmenter write context
     * and hence __only__ the writer can free the context when the
     * last_write_flag in the writer context is set
     */
    typedef struct tag_ts_segment_writer {
	TAILQ_HEAD(,tag_olt_queue_elm) ol_list_head;
	int32_t num_ol_list_entries;
	const char *output_path;
	const char *video_name;
	const char *uri_prefix;
	int32_t segment_num;
	int32_t profile_num;
        uint32_t segment_start_idx;
        uint8_t min_segment_child_plist;
        uint8_t segment_idx_precision;
	uint8_t new_segment_flag;
	uint8_t cleanup_pending;
	uint32_t duration;
	uint8_t is_plist_created;
    } ts_segment_writer_t;

    /** 
     * \struct data type that maitains the input source and the data
     * segmentation state
     */
    typedef struct tag_ts2iphone_builder {
	ts_segment_writer_t *out;
	int32_t state;
	const char **src_ts_files;
	int32_t num_src_ts_files;
	int32_t *profile_bitrate;
	int32_t avg_bitrate[MAX_STREAM_PER_ENCAP];
	int32_t max_duration[MAX_STREAM_PER_ENCAP];
	char output_path[256];
	char *uri_prefix;
	char *video_name;
	int32_t segment_duration;
	int32_t curr_profile;
	size_t curr_profile_size;
	size_t curr_profile_bytes_remaining;
	int32_t curr_seg_num;
	uint8_t segment_span_flag;
        uint32_t segment_start_idx;
        uint8_t min_segment_child_plist;
        uint8_t segment_idx_precision;
	io_handlers_t *iocb;
	void *io_desc;
	uint32_t duration;
	int64_t current_PTS;
	int64_t prev_PTS;
	int64_t keyframe_interval;
        uint8_t is_keyframe_interval;
	uint8_t is_plist_created;
    } ts2iphone_builder_t;

    /**
     * \struct container context for the ts to iphone segmenter
     */
    typedef struct tag_ts2iphone_converter_ctx {
	ts_segmenter_ctx_t *seg_ctx;
	bitstream_state_t *bs;
	ts2iphone_builder_t *ts_bldr;
    } ts2iphone_converter_ctx_t;

    /** 
     * interface handler for initializing the converter context for
     * ISMV to MOOF file conversions
     * @param fconv_params - the converter input parmars, subset of
     * the PMF publisher context
     * @param conv_ctx [out] - a fully populated converter context
     * that will be passed to subsequent converter interface handlers
     * @return - returns 0 on success and a non-zero negative integer
     * on error
     */
    int32_t init_ts2iphone_converter(\
			    file_conv_ip_params_t *fconv_params,  
			    void **conv_ctx);

    /**
     * interface implementation for processing data; this function
     * segments a chunk of memory containing  a set TS packets to a
     * time bound segment.
     * @param private data[in] - the converter context created with a
     * init call
     * @param out [out] - a unit of data that can be written
     * asynchronously
     * @return - returns 0 on success and a negative integer on error
     */
    int32_t ts2iphone_create_seg(void *private_data, void **out);

    /**
     * implements the writing of processed/converted data to a output
     * sink. the buffer containing the data is asynchronous. the write
     * function frees the buffer once the entire buffer has be converted
     * and written. this is done via the EOB flag set from the
     * read_and_process handler
     * @param private data - the writer context containing the data from
     * the output of a read_and_process call from the conversion engine
     * @return returns 0 on success and a negative number on error
     */
    int32_t ts2iphone_write_output(void *out);

    /**
     * cleanup the converter context
     * @param private_data [in] - context to be cleaned up
     */
    void ts2iphone_cleanup_converter(void *private_data);

#ifdef __cplusplus
}
#endif

#endif //_TS2IPHONE_CONV_

