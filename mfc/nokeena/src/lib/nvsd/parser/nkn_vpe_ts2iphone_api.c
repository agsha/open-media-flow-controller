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

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <math.h>

#include "nkn_vpe_ts2iphone_api.h"
#include "nkn_vpe_ts_segmenter_api.h"
#include "mfp_file_converter_intf.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_memalloc.h"
#include "nkn_vpe_sync_http_read.h"
#include "nkn_vpe_parser_err.h"

#define TS2IPHONE_READ_SIZE (3145616) //closest number of integral
					//TS packets to 3MB

/***************************************************************
 *             HELPER FUNCTIONS (STATIC)
 **************************************************************/
static char* ts2iphone_get_video_name(char * const output_path);
static char *ts2iphone_get_uri_prefix(char * const output_path);
static int32_t ts2iphone_update_bandwith_stats(ts2iphone_builder_t *bldr,
					       ts_segment_writer_t *writer);
static inline void ts2iphone_caclulate_curr_seg_duration(ts2iphone_builder_t *bldr,
							    ts_segmenter_ctx_t *seg_ctx);
static inline void ts2iphone_writer_set_duration(ts2iphone_builder_t *bldr);
#if 0
static int32_t ts2iphone_write_child_m3u8(const char * const out_path,\
					  const char * const video_name,  
					  const char * const uri_prefix,
					  const uint32_t segment_idx_precision,
					  const uint32_t segment_start_idx,
 					  int32_t profile, 
					  int32_t n_segments, 
					  int32_t segment_duration,
					  uint32_t *duration, 
					  int32_t  *avg_bitrate, 
					  const uint8_t is_append);
#endif

static int32_t ts2iphone_finalize_m3u8(const char * const out_path, 
				       const char * const video_name,
				       const char * const uri_prefix,
				       const int32_t * const bitrate,
				       const int32_t * const max_duration,
				       int32_t n_profiles);
static int32_t ts2iphone_recursive_mkdir(const char *const path, 
					 int32_t mode);
int32_t ts2iphone_setup_write_buf(ts2iphone_builder_t *bldr, 
				  bitstream_state_t*bs, 
				  vpe_olt_t *ol,
				  int32_t eov_flag,
				  int32_t new_segment_flag,
				  ts_segment_writer_t **out);    
int32_t ts2iphone_attach_ol(bitstream_state_t *bs, int32_t eov_flag, 
			    ts_segment_writer_t *writer, 
			    vpe_olt_t *ol); 
int32_t init_ts2iphone_builder(file_conv_ip_params_t *fconv_params,
			       ts2iphone_builder_t **out);
size_t ts2iphone_read_data(ts2iphone_builder_t *bldr, 
			   bitstream_state_t **bs, 
			   ts_segmenter_ctx_t *seg);
int32_t ts2iphone_next_profile(ts2iphone_builder_t *bldr, 
			       bitstream_state_t *bs);
void cleanup_ts2iphone_builder(ts2iphone_builder_t *bldr);

const char *_dbg_uri_prefix = "http://172.27.159.11/iphone/bbb";

static io_handlers_t iohldr = {
    .ioh_open = nkn_vpe_fopen,
    .ioh_read = nkn_vpe_fread,
    .ioh_tell = nkn_vpe_ftell,
    .ioh_seek = nkn_vpe_fseek,
    .ioh_close = nkn_vpe_fclose,
    .ioh_write = nkn_vpe_fwrite
};

static io_handlers_t ioh_http = {
    .ioh_open = vpe_http_sync_open,
    .ioh_read = vpe_http_sync_read,
    .ioh_tell = vpe_http_sync_tell,
    .ioh_seek = vpe_http_sync_seek,
    .ioh_close = vpe_http_sync_close,
    .ioh_write = NULL
};  

/** 
 * interface handler for initializing the converter context for
 * TS to iphone segments
 * @param fconv_params - the converter input parmars, subset of
 * the PMF publisher context
 * @param conv_ctx [out] - a fully populated converter context
 * that will be passed to subsequent converter interface handlers
 * @return - returns 0 on success and a non-zero negative integer
 * on error
 */
int32_t
init_ts2iphone_converter(file_conv_ip_params_t *fconv_params,  
			 void **conv_ctx)
{
    int32_t err;
    ts2iphone_converter_ctx_t *ctx;
    char *video_name;

    err = 0;
    ctx = NULL;

    ctx = (ts2iphone_converter_ctx_t*)\
	nkn_calloc_type(1, sizeof(ts2iphone_converter_ctx_t),
			mod_vpe_ts2iphone_converter_ctx_t);

    if (!ctx) {
	err = -E_VPE_PARSER_NO_MEM;
	goto error;
    }
    
    err = init_ts_segmenter_ctx(fconv_params,
				&ctx->seg_ctx);
    if (err) {
	goto error;
    }

    err = init_ts2iphone_builder(fconv_params, &ctx->ts_bldr);
    if (err) {
	goto error;
    }

    ctx->bs = NULL;//bio_init_dummy_bitstream();
    *conv_ctx = ctx;
    
    return err;

 error:
    if (ctx->seg_ctx) free(ctx->seg_ctx);
    if (ctx) free(ctx);
    return err;
}

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
int32_t
ts2iphone_create_seg(void *private_data, void **out)
{
    int32_t err, err1, eob, new_segment_flag;
    ts2iphone_converter_ctx_t *conv_ctx;
    ts_segmenter_ctx_t *seg_ctx;
    ts2iphone_builder_t *bldr;
    bitstream_state_t *bs;
    ts_segment_writer_t *out_buf;
    vpe_olt_t *ol;

    conv_ctx = (ts2iphone_converter_ctx_t*)private_data;
    seg_ctx = conv_ctx->seg_ctx;
    bldr = conv_ctx->ts_bldr;
    bs = conv_ctx->bs;
    eob = 0;
    new_segment_flag = 0;
    *out = NULL;

    err = E_VPE_FILE_CONVERT_CONTINUE;
    
    if (bldr->state & TS_NEXT_PROFILE) {
	err = ts2iphone_next_profile(bldr, bs);
	if (err) {
	    /* TODO:cleanup */
	    return err;
	}
	ts_segmenter_reset(seg_ctx);
    }

    if (bldr->state & TS_FLUSH_PENDING_DATA) {
	ts2iphone_writer_set_duration(bldr);
	*out = bldr->out;
	bldr->segment_span_flag = 0;
	bldr->state &= (~TS_FLUSH_PENDING_DATA);
	return E_VPE_FILE_CONVERT_CONTINUE;
    }

    if (bldr->state & TS_READ_DATA) {
	err = ts2iphone_read_data(bldr, &bs, seg_ctx);
	if (err == E_VPE_FILE_CONVERT_SKIP) {
	    return err;
	}
	conv_ctx->bs = bs;
    }

    if (bldr->state & TS_START_PARSING) {
	ol = (vpe_olt_t*)\
	    nkn_calloc_type(1, sizeof(vpe_olt_t), mod_vpe_ol_t);
	err = ts_segmenter_segment_data(seg_ctx, bs, 
					ol);
	if(seg_ctx->is_keyframe_interval == 1) {
	    bldr->keyframe_interval = seg_ctx->keyframe_interval;
	    bldr->is_keyframe_interval = 1;
	}
	/*initializing the prev_PTS with the first I_frame PTS value
	  of the seq*/
	if(bldr->curr_seg_num == 0){
	    bldr->prev_PTS = seg_ctx->codec_span_data->first_PTS;
	}
	switch (err) {
	    case TS_SEGMENTER_BUF_COMPLETE:
	    
		eob = 1;/* this flag will indicate that the writer can
		     * free the buffer after writing the data
		     */
		bldr->state |= TS_READ_DATA;
		/* set data span flag; create a new output buffer */
		if (!bldr->segment_span_flag) {
		    bldr->segment_span_flag = 1;
		    new_segment_flag = 1;
		    bldr->curr_seg_num++;
		    err1 = ts2iphone_setup_write_buf(bldr, bs, ol, eob,  
						     new_segment_flag,
						     &out_buf); 
		    bldr->is_plist_created = 1;
		    *out = bldr->out = out_buf;
		} else {
		    /* continuation of a span; attach the [offset,
		     * length] to the existing writer context 
		     */
		    err1 = ts2iphone_attach_ol(bs, eob, bldr->out, ol);
		    *out = bldr->out;
		}
		//bldr->prev_PTS = bldr->current_PTS;
		if(err1) {
		    /* TODO cleanup writer buf */
		    err = E_VPE_FILE_CONVERT_STOP;
		    goto error;
		}
		err = E_VPE_FILE_CONVERT_SKIP;
		break;
	    case TS_SEGMENTER_SEGMENT_READY:          
		new_segment_flag = 1;
		/* append if this is the continuation of a segment span */
		if (bldr->segment_span_flag) {
		    bldr->segment_span_flag = 0;
		    new_segment_flag = 0;
		    ts2iphone_caclulate_curr_seg_duration(bldr, seg_ctx);
		    err1 = ts2iphone_attach_ol(bs, eob, bldr->out,
					       ol);
		    *out = bldr->out;
		} else {
		    /* create a new buffer */

		    bldr->curr_seg_num++;
		    ts2iphone_caclulate_curr_seg_duration(bldr, seg_ctx);
		    err1 = ts2iphone_setup_write_buf(bldr, bs, ol, eob,  
						     new_segment_flag,
						     &out_buf); 
		    ts2iphone_update_bandwith_stats(bldr, out_buf);
		    bldr->is_plist_created = 1;
		    *out = bldr->out = out_buf;
		}
		bldr->prev_PTS = bldr->current_PTS;
		err = E_VPE_FILE_CONVERT_CONTINUE;
		if(err1) {
		    /* TODO cleanup writer buf */
		    err = E_VPE_FILE_CONVERT_STOP;
		    goto error;
		}
	}	
	
	if(err < 0) {
	    return err;
	    goto error;
	}
	
    }
    

 error:
    return err;    
}

/**
 * cleanup the segment writer context
 * @param writer - the segment writer context that needs to be freed
 * @return returns E_VPE_FILE_CONVERT_CLEANUP_PENDING if the cleanup
 * is not completed becuase of buffer sharing, 0 otherwise if cleanup
 * is completed successfully
 */
static inline int32_t
cleanup_ts_segment_writer(ts_segment_writer_t *writer)
{
    olt_queue_elm_t *elm;
    int32_t err;

    err = 0;

    if (writer) {
	writer->cleanup_pending = 0;
	while((elm = TAILQ_FIRST(&writer->ol_list_head))) {
	    if (elm->last_write_flag) {
		if (!bio_get_ref_cnt(elm->bs)) {
		    free(bio_get_buf(elm->bs));
		    bio_close_bitstream(elm->bs);
		} else {
		    writer->cleanup_pending = 1;
		    err = E_VPE_FILE_CONVERT_CLEANUP_PENDING;
		    continue;
		}
	    } 
	    TAILQ_REMOVE(&writer->ol_list_head, elm, entries);
	    free(elm->ol);
	    free(elm);
	    
	}
	if(!writer->cleanup_pending) {
	    free(writer);
	    return 0;
	}
    }
    
    return err;
}

/**
 * implements the writing of processed/converted data to a output
 * sink. the buffer containing the data is asynchronous. the write
 * function frees the buffer once the entire buffer has be converted,
 * writter and if the reference count for the buffer is 0. this is
 * done via the EOB flag set from the read_and_process handler and the
 * ref counting mechanism in the bistream API's.
 * @param private data - the writer context containing the data from
 * the output of a read_and_process call from the conversion engine
 * @return returns 0 on success and E_VPE_FILE_CONVERT_CLEANUP_PENDING
 * if the cleanup could not be completed due to buffer sharing across
 * threads etc. it is the responsibility of the caller to re-schedule
 * this function at a later point in time.
 */
int32_t 
ts2iphone_write_output(void *private_data)
{
    ts_segment_writer_t *writer;
    char output_file_name[256], fmt_string[100], 
	str[256], fmt_string1[100];
    olt_queue_elm_t *elm;
    vpe_olt_t *ol;
    FILE *fp, *fp1;
    int32_t st_idx, err, temp2_duration;
    uint8_t *buf;
    uint64_t ref_cnt;
    float temp_duration, temp1_duration;

    err = 0;
    writer = (ts_segment_writer_t*)private_data;
    fp = NULL;
    fp1 = NULL;

    /* check if this is follow up cleanup task, dont write anything if
     * this is the case; just need to call cleanup
     */
    if (!writer) {
	goto error;
    }

    if (writer->cleanup_pending) {
	goto error;
    }

    /* compute the format string based on the input settings for start
     * index etc
     */
    snprintf(fmt_string, 100, "%%s/%%s_p0%%d_%%0%dd.ts",
	     writer->segment_idx_precision);
    snprintf(fmt_string1, 100, "%%s_p0%%d_%%0%dd.ts", 
	     writer->segment_idx_precision);

    st_idx = writer->segment_start_idx;
    /* check if the segment indices overflows the segment
       precision */
    if ( (writer->segment_start_idx + writer->segment_num) >  
	 pow(10, writer->segment_idx_precision)) { 
	err = -E_VPE_PARSER_FATAL_ERR; 
	goto error;
    }
 
    err = ts2iphone_recursive_mkdir(writer->output_path,
				    S_IRWXU | S_IRWXG | S_IROTH |\
				    S_IXOTH);
    if (err) {
	err = -E_VPE_PARSER_INVALID_OUTPUT_PATH;
	//printf("invalid output path (errno=%d)\n", err);
	goto error;
    }
    
    /* build the output file name */
    snprintf(output_file_name, 256, fmt_string,
	     writer->output_path, writer->video_name,
	     writer->profile_num, writer->segment_num + st_idx - 1);  
    snprintf(str, 256, "%s/temp_%s_p%02d.m3u8", writer->output_path, 
	     writer->video_name, writer->profile_num);
    
    if (writer->new_segment_flag) {
	fp = fopen(output_file_name, "wb");
    } else {
	fp = fopen(output_file_name, "ab");
    }

    if (!writer->is_plist_created) {
	fp1 = fopen(str, "wb");
    } else {
	fp1 = fopen(str, "ab");
    }

    if (!fp || !fp1) {
	err = -E_VPE_PARSER_INVALID_OUTPUT_PATH;
	goto error;
    }
    
   TAILQ_FOREACH(elm, &writer->ol_list_head, entries) {
       ol = elm->ol;
       buf = bio_get_buf(elm->bs);
       fwrite(buf + ol->offset, 1, ol->length, fp);
       ref_cnt = bio_dec_ref(elm->bs);
   }
   fclose(fp);
   
   temp_duration = (float)writer->duration / 1000;
   temp1_duration = round(temp_duration);
   temp2_duration = (int32_t) temp1_duration;
   snprintf(str, 256, fmt_string1, writer->video_name,
	    writer->profile_num, writer->segment_num + st_idx - 1);
   fprintf(fp1, "#EXTINF:%d,\n%s\n",
	   temp2_duration /*secs*/, str);
   fclose(fp1);

   err = cleanup_ts_segment_writer(writer);
   return err;

 error:
   /* when we hit an error we need to unref the buffers so that they
    * can be freed. we are not going to write them anyways
    */
   TAILQ_FOREACH(elm, &writer->ol_list_head, entries) {
       ref_cnt = bio_dec_ref(elm->bs);
   }
   if (fp) fclose(fp);
   if (fp1) fclose(fp1);
   err = cleanup_ts_segment_writer(writer);
   return err;    
}


/**
 * cleanup the converter context
 * @param private_data [in] - context to be cleaned up
 */
void ts2iphone_cleanup_converter(void *private_data)
{
    ts2iphone_converter_ctx_t *conv_ctx;
    
    conv_ctx = (ts2iphone_converter_ctx_t*)private_data;
    
    if (conv_ctx) {
	cleanup_ts_segmenter(conv_ctx->seg_ctx);
	cleanup_ts2iphone_builder(conv_ctx->ts_bldr);
	free(conv_ctx);
    }

}

int32_t 
init_ts2iphone_builder(file_conv_ip_params_t *fconv_params,
		       ts2iphone_builder_t **out)
{
    ts2iphone_builder_t *ctx;
    void *io_desc = NULL;
    int32_t err = 0;
    uint8_t first_byte_data[1] = {0};
    int32_t i=0;
    ctx = (ts2iphone_builder_t*)\
	nkn_calloc_type(1, sizeof(ts2iphone_builder_t),
			mod_vpe_ts2iphone_builder_t);

    if (!ctx) {
	return -E_VPE_PARSER_NO_MEM;
    }

    ctx->video_name =\
	ts2iphone_get_video_name(fconv_params->delivery_url);
   
    ctx->src_ts_files = (const char **)fconv_params->input_path;
    if (strstr(ctx->src_ts_files[0], "http://")) {
	ctx->iocb = &ioh_http;  
    } else {
	ctx->iocb = &iohldr;
    }

    ctx->num_src_ts_files = fconv_params->n_input_files;
    /*check whether the file is an ts file or not*/
    for(i=0; i<ctx->num_src_ts_files; i++) {
	io_desc = ctx->iocb->ioh_open((char*)	\
				      ctx->src_ts_files[i] ,
				      "rb", 0);
	if (!io_desc) {
	    err = -E_VPE_PARSER_INVALID_FILE;
	    return err;
	}
	//ctx->iocb->ioh_seek(io_desc, 8, SEEK_SET);
	ctx->iocb->ioh_read(&first_byte_data, 1, sizeof(uint8_t),
			    io_desc);
	ctx->iocb->ioh_close(io_desc);
	
	if(first_byte_data[0] != 0x47) {
	    err = -E_VPE_INVALID_FILE_FROMAT;
	    return err;
	}
    }

    snprintf((char *)ctx->output_path, 256, "%s/", fconv_params->output_path);
    ctx->uri_prefix = ts2iphone_get_uri_prefix(fconv_params->delivery_url);
    ctx->profile_bitrate = fconv_params->profile_bitrate;
    ctx->curr_seg_num = 0;
    ctx->curr_profile = 0;
    ctx->segment_start_idx = fconv_params->segment_start_idx;
    ctx->min_segment_child_plist =
	fconv_params->min_segment_child_plist;
    ctx->segment_idx_precision = fconv_params->segment_idx_precision;
    ctx->segment_duration = fconv_params->key_frame_interval;
    ctx->state = TS_NEXT_PROFILE;
    
    *out = ctx;
    
    return 0;
    
}

void
cleanup_ts2iphone_builder(ts2iphone_builder_t *bldr)
{
    int32_t i =0;
    if (bldr) {
	if (bldr->io_desc) bldr->iocb->ioh_close(bldr->io_desc);
	if (bldr->video_name) free(bldr->video_name);
	if (bldr->uri_prefix) free(bldr->uri_prefix);
	free(bldr);
    }
}

/**
 * sets up the writer structure for a asynchronous write. all shared
 * data elements should be protected ex. output path, video name etc
 * and the buffer containing the output data should be detached from
 * the processing engine
 * @param bldr - the ts segment builder context
 * @param bs - the bitstream handler that contains the buffers
 * @param ol - the [offset, length] pair that needs to be written
 * @param eov_flag - end of the buffer indicates that the buffer needs
 * to be free'd after write
 * @param out - a fully populated valid writer context
 * @return - returns 0 on success and a negative number on error
 */
int32_t 
ts2iphone_setup_write_buf(ts2iphone_builder_t *bldr, 
			  bitstream_state_t*bs, 
			  vpe_olt_t *ol,
			  int32_t eov_flag,
			  int32_t new_segment_flag,
			  ts_segment_writer_t **out)
{
    ts_segment_writer_t *writer;
    olt_queue_elm_t *elm;
    int32_t err;

    err = 0;
    *out = NULL;

    *out = writer = (ts_segment_writer_t*)
	nkn_calloc_type(1, sizeof(ts_segment_writer_t),
			mod_vpe_ts_segment_writer_t);
    if (!writer) {
	err = -E_VPE_PARSER_NO_MEM;
	goto error;
    }

    elm = (olt_queue_elm_t*)
	nkn_calloc_type(1, sizeof(olt_queue_elm_t),
			mod_vpe_ol_t);
    if (!elm) {
	err = -E_VPE_PARSER_NO_MEM;
	goto error;
    }


    TAILQ_INIT(&writer->ol_list_head);
    
    elm->ol = ol;
    elm->bs = bs;
    bio_add_ref(bs);
    elm->last_write_flag = eov_flag;

    TAILQ_INSERT_TAIL(&(writer->ol_list_head), elm, entries);
    writer->num_ol_list_entries++;
    writer->output_path = bldr->output_path;
    writer->profile_num = bldr->curr_profile;
    writer->video_name = bldr->video_name;
    writer->uri_prefix = bldr->uri_prefix;
    writer->segment_num = bldr->curr_seg_num;
    writer->new_segment_flag = new_segment_flag;
    writer->segment_start_idx = bldr->segment_start_idx;
    writer->segment_idx_precision = bldr->segment_idx_precision;
    writer->duration = bldr->duration;
    writer->is_plist_created = bldr->is_plist_created;

    return err;
 error:
    if (writer) free(writer);
    return err;
}

int32_t
ts2iphone_attach_ol(bitstream_state_t *bs, int32_t eov_flag,
		    ts_segment_writer_t *writer, vpe_olt_t *ol)
{
    olt_queue_elm_t *elm;
    int32_t err;

    elm = (olt_queue_elm_t*)
	nkn_calloc_type(1, sizeof(olt_queue_elm_t),
			mod_vpe_ol_t);
    if (!elm) {
	err = -E_VPE_PARSER_NO_MEM;
	return err;
    }

    elm->ol = ol;
    elm->bs = bs;
    bio_add_ref(bs);
    elm->last_write_flag = eov_flag;

    TAILQ_INSERT_TAIL(&(writer->ol_list_head), elm, entries);
    writer->num_ol_list_entries++;
    
    return 0;
}

/**
 * sets the builder state to setup parsing for the next track
 * @param bldr - the ts segment builder
 * @param bs - bitstream handler context
 * @return returns 0 on success and negative integer on error
 */  
int32_t  
ts2iphone_next_profile(ts2iphone_builder_t *bldr, 
		       bitstream_state_t *bs) 
{
    int32_t err;

    err = 0;

    /* check if there is any pending data in the current profile which
     *  needs to be flushed
     */
    if (bldr->segment_span_flag) {
	bldr->state |= TS_FLUSH_PENDING_DATA;
	return 0;
    }

    /* check if all profiles are processed */
    if (bldr->curr_profile >= bldr->num_src_ts_files) {
	bldr->state = 0;
	err = ts2iphone_finalize_m3u8(bldr->output_path, 
				bldr->video_name,
				bldr->uri_prefix,
				bldr->avg_bitrate, 
				bldr->max_duration,
				bldr->num_src_ts_files);
	if (!err) {
	    err = E_VPE_FILE_CONVERT_STOP;
	}
	return err;
    }

    /* open the io descriptor for the next track */
    if (bldr->io_desc) {
	bldr->iocb->ioh_close(bldr->io_desc);
    }
    bldr->io_desc =\
	bldr->iocb->ioh_open((char*)\
			bldr->src_ts_files[bldr->curr_profile],
			"rb", 0); 
    if (!bldr->io_desc) {
	err = -E_VPE_PARSER_INVALID_FILE;
	return err;
    }
    
    /* find size of the profile */
    bldr->iocb->ioh_seek(bldr->io_desc, 0, SEEK_END);
    bldr->curr_profile_size = bldr->curr_profile_bytes_remaining =\
	bldr->iocb->ioh_tell(bldr->io_desc);
    bldr->iocb->ioh_seek(bldr->io_desc, 0, SEEK_SET);

    /* update the state */
    bldr->curr_profile++;
    bldr->curr_seg_num = 0;
    bldr->segment_span_flag = 0;
    bldr->is_plist_created = 0;
    bldr->state = TS_READ_DATA;
    
    return err;
}

/**<
 * reads data into the bitstream buffer for parsing and sets the
 * builder to start parsing state
 * @param bldr - iphone segment builder
 * @param bs - bitstream context
 * @return returns a positive number indicating the read size
 */
size_t
ts2iphone_read_data(ts2iphone_builder_t *bldr, bitstream_state_t **bs,
		    ts_segmenter_ctx_t *seg_ctx)
{
    uint8_t *data;
    int32_t err;
    size_t read_size, bytes_remaining;
    float temp_act_keyframe_interval, temp_segment_duration;
    int32_t act_keyframe_interval, segment_duration;
    err = E_VPE_FILE_CONVERT_CONTINUE;

    bytes_remaining = bldr->curr_profile_bytes_remaining;
    if (bytes_remaining == 0) {
	/* if bitrate is not present in the input the calculate from
	   the file size and the last PTS (note: this will include the
	   trasnport header overheads */
	if (0) {//!bldr->profile_bitrate[bldr->curr_profile - 1]) {
	    bldr->profile_bitrate[bldr->curr_profile - 1] = \
		((bldr->curr_profile_size*8)/seg_ctx->last_pts);
	}
	/* for the last fragement , update the duration with the prev
	   I_frame PTS value and last PTS of that fragment*/
	bldr->current_PTS =
	    seg_ctx->curr_frag_last_PTS;
	bldr->duration = 
	    (bldr->current_PTS - bldr->prev_PTS);
	ts2iphone_update_bandwith_stats(bldr, bldr->out);
#if 0
	ts2iphone_write_child_m3u8(bldr->output_path,
				   bldr->video_name, 
				   bldr->uri_prefix,
				   bldr->segment_idx_precision,
				   bldr->segment_start_idx + bldr->last_seg_num_in_plist,
				   bldr->curr_profile,
				   bldr->curr_seg_num,
				   bldr->segment_duration,
				   bldr->duration, 
				   bldr->avg_bitrate,
				   bldr->is_plist_created);
#endif

	/* print the actual  key frame interval*/
	DBG_MFPLOG("TS2_IPHONE", WARNING, MOD_MFPFILE,"The"
		   "actual keyframe interval in millisec is %ld\n", bldr->keyframe_interval);
	bldr->state = TS_NEXT_PROFILE;
	err = E_VPE_FILE_CONVERT_SKIP;
	return err;
    }

    read_size = TS2IPHONE_READ_SIZE;
    if (bytes_remaining < TS2IPHONE_READ_SIZE) {
	read_size = bytes_remaining;
    } 

    data = (uint8_t*)\
	nkn_calloc_type(1, read_size,
			mod_vpe_media_buffer);
    err = bldr->iocb->ioh_read(data, 1, read_size, bldr->io_desc);
    bldr->curr_profile_bytes_remaining -= read_size;
    *bs = bio_init_safe_bitstream(data, read_size);
    bldr->state = TS_START_PARSING;
    
    return err;
}

/**
 * strips the video name from a path string. the video name is the set
 * of characters from the last '/' till the start of the extension
 * type of the string
 * @param output_path - path from which the video name needs to be
 * stripped
 * @return - returns the video name string and NULL on error. this
 * call is re-entrant
 */
static char *
ts2iphone_get_video_name(char * const output_path)
{
    char const *st, *end;
    char *video_name;
    int32_t video_name_size;

    video_name_size = 0;

    st = strrchr(output_path, '/');
    if (!st) {
	st = output_path;
	end = strrchr(output_path, '.');
	if ( end <= st) {
	    end = output_path + strlen(output_path);
	}

	video_name_size = end - st; 
	video_name_size+=1;/*includes null term */
	video_name = (char *)\
	    nkn_calloc_type(1, sizeof(char) * video_name_size ,
			    mod_vpe_charbuf);
	memcpy(video_name, st , video_name_size - 1);
    }
    else {
    
	end = strrchr(output_path, '.');
	if ( end <= st) {
	    end = output_path + strlen(output_path);
	}
	
	video_name_size = end - st; /*includes null term */
	video_name = (char *)\
	    nkn_calloc_type(1, sizeof(char) * video_name_size ,
			    mod_vpe_charbuf);
	memcpy(video_name, st + 1, video_name_size - 1);
    }
    
    return video_name;
}

static char *
ts2iphone_get_uri_prefix(char * const output_path)
{
    char *uri_prefix, *p;
    int32_t len;    

    uri_prefix = strdup(output_path);
    len = strlen(uri_prefix);

    if (uri_prefix[len-1] == '/') {
	/* trailing slash */
	uri_prefix[len - 1] = '\0';
    }

    p = strrchr(uri_prefix, '/');
    //if(!p)
    if(p)
	*p = '\0';
    else
	uri_prefix[0] = '\0';
    return uri_prefix;
}

#if 0
static int32_t
ts2iphone_write_child_m3u8(const char * const out_path, 
			   const char * const video_name,
			   const char * const uri_prefix,
			   const uint32_t segment_idx_precision,
			   const uint32_t segment_start_idx,
			   int32_t profile, 
			   int32_t n_segments, 
			   int32_t segment_duration,
			   uint32_t *duration,
			   int32_t *avg_bitrate,
			   const uint8_t is_append)
{
    char m3u8_name[256], uri[256];
    FILE *f_m3u8_file;
    int32_t j, err;
    char fmt_string[100];
    char fmt_string1[100];
    uint32_t st_idx;
    uint32_t max_duration=0;    
    uint64_t total_duration = 0;
    char output_file_name[256];
    uint32_t bitrate = 0;
    int32_t tmp_avg_bitrate = 0;
    FILE *fp = NULL;
    uint32_t chunk_size = 0;
    err = 0;
    snprintf(fmt_string1, 100, "%%s/%%s_p0%%d_%%0%dd.ts",
             segment_idx_precision);
    snprintf(fmt_string, 100, "%%s_p0%%d_%%0%dd.ts",
	     segment_idx_precision);

    snprintf(m3u8_name, 256, "%s/%s_p%02d.m3u8", out_path, video_name, profile);
    if (!is_append) {
	f_m3u8_file = fopen(m3u8_name, "wb");
    } else {
	f_m3u8_file = fopen(m3u8_name, "ab");
    }

    if (!f_m3u8_file) {
	return -E_VPE_PARSER_INVALID_OUTPUT_PATH;
    }

    st_idx = segment_start_idx;
    /* check if the segment indices overflows the segment
       precision */
    if ( (segment_start_idx + n_segments) > 
	 pow(10, segment_idx_precision)) { 
	err = -E_VPE_PARSER_FATAL_ERR; 
	goto error;
    }
    /* calculatng the maximum value of all the segments duration*/
    max_duration = duration[0];
    for(j=0; j<n_segments; j++) {
	/* build the output file name */
        snprintf(output_file_name, 256, fmt_string1,
                 out_path, video_name,
                 profile, j + segment_start_idx);
        fp = fopen(output_file_name, "ab");
        if (!fp) {
            err = -E_VPE_PARSER_INVALID_OUTPUT_PATH;
            goto error;
        }
        fseek(fp, 0, SEEK_END);
	chunk_size = ftell(fp);
	bitrate = ((chunk_size << 3) / (duration[j])) * 1000;
	tmp_avg_bitrate = (((j) * tmp_avg_bitrate) +	\
			   bitrate )/ (j+1);
        if(duration[j] > max_duration)
            max_duration = duration[j];
    }
    avg_bitrate[profile-1] = tmp_avg_bitrate/1000;//bps to kbps
    float temp_duration = (float)max_duration / 1000;
    float temp1_duration = round(temp_duration);
    int32_t temp2_duration = (int32_t) temp1_duration;

    fprintf(f_m3u8_file, "#EXTM3U\n#EXT-X-TARGETDURATION:%d\n",
            temp2_duration/*secs*/);

    for(j = 1; j <= n_segments; j++) { 
	//snprintf(uri, 256,
	// fmt_string, uri_prefix, video_name,
	// profile, j + segment_start_idx - 1); 
	temp_duration = (float)duration[j-1] / 1000; 
	temp1_duration = round(temp_duration);
	temp2_duration = (int32_t) temp1_duration;
	snprintf(uri, 256,
                 fmt_string,  video_name,
                 profile, j + segment_start_idx - 1);
	fprintf(f_m3u8_file, "#EXTINF:%d,\n%s\n", 
		temp2_duration /*secs*/, uri);
    }
    fprintf(f_m3u8_file, "#EXT-X-ENDLIST\n");

 error:
    fclose(f_m3u8_file);
    return err;

}
#endif

static int32_t
ts2iphone_finalize_m3u8(const char * const out_path,
			const char * const video_name,
			const char * const uri_prefix,
			const int32_t * const bitrate,
			const int32_t * const max_duration,
			int32_t n_profiles)

{
    int32_t i, err = 0, temp2_duration;
    float temp_duration, temp1_duration;
    char parent_m3u8_name[256], uri[256], m3u8_name[256], uri2[256];
    FILE *f_parent_m3u8_file, *f_tmp_child_m3u8[MAX_STREAM_PER_ENCAP], 
	*f_child_m3u8[MAX_STREAM_PER_ENCAP];
    size_t temp_m3u8_size;
    uint8_t *buf[MAX_STREAM_PER_ENCAP];

    memset(buf, 0, sizeof(uint8_t*) * MAX_STREAM_PER_ENCAP);
    memset(f_tmp_child_m3u8, 0, sizeof(FILE*) * MAX_STREAM_PER_ENCAP);
    memset(f_child_m3u8, 0, sizeof(FILE*) * MAX_STREAM_PER_ENCAP);

    /* create a child playlist from the temp playlist with the correct
     * max duration 
     */
    for (i = 0; i < n_profiles; i++) {
	/* copy contents of the temp playlist */
	snprintf(uri, 256, "%s/temp_%s_p%02d.m3u8", out_path,
		 video_name, i+1);
	f_tmp_child_m3u8[i] = fopen(uri, "rb");
	if (!f_tmp_child_m3u8[i]) {
	    return -E_VPE_PARSER_INVALID_OUTPUT_PATH;
	}
	fseek(f_tmp_child_m3u8[i], 0, SEEK_END);
	temp_m3u8_size = ftell(f_tmp_child_m3u8[i]);
	buf[i]= (uint8_t*)nkn_malloc_type(temp_m3u8_size, mod_vpe_charbuf);
	if (!buf[i]) {
	    err = -E_VPE_PARSER_NO_MEM;
	    goto error;
	}
	fseek(f_tmp_child_m3u8[i], 0, SEEK_SET);
	fread(buf[i], 1, temp_m3u8_size, f_tmp_child_m3u8[i]);
	
	/* create the new child playlist with the max duration entry */
	snprintf(uri2, 256, "%s/%s_p%02d.m3u8", out_path, 
		video_name, i+1); 
	f_child_m3u8[i] = fopen(uri2, "wb");
	if (!f_child_m3u8[i]) {
	    err = -E_VPE_PARSER_INVALID_OUTPUT_PATH;
	    goto error;
	}
	temp_duration = (float)max_duration[i] / 1000;
	temp1_duration = round(temp_duration);
	temp2_duration = (int32_t) temp1_duration;
	fprintf(f_child_m3u8[i], "#EXTM3U\n#EXT-X-TARGETDURATION:%d\n",
		temp2_duration/*secs*/);
	fwrite(buf[i], 1, temp_m3u8_size, f_child_m3u8[i]);
	fprintf(f_child_m3u8[i],"#EXT-X-ENDLIST\n");
	
	/* cleanup */
	fclose(f_child_m3u8[i]);
	fclose(f_tmp_child_m3u8[i]);
	free(buf[i]);

	f_tmp_child_m3u8[i] = NULL;
	f_child_m3u8[i] = NULL;
	buf[i] = NULL;

	/* remove temp file */
	remove(uri);
    }

    snprintf(parent_m3u8_name, 256, "%s/%s.m3u8", out_path,
	     video_name);
    f_parent_m3u8_file = fopen(parent_m3u8_name, "wb");
    if (!f_parent_m3u8_file) {
	return -E_VPE_PARSER_INVALID_OUTPUT_PATH;
    }

    fprintf(f_parent_m3u8_file, "#EXTM3U\n");
    for(i = 1; i <= n_profiles; i++) { 
	snprintf(m3u8_name, 256, "%s/%s_p%02d.m3u8", out_path, video_name, i); 
       	fprintf(f_parent_m3u8_file,
		"#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=%d\n",
		bitrate[i-1] /*kbps->bps*/);

	//fprintf(f_parent_m3u8_file, "%s/%s\n", uri_prefix,
	//(m3u8_name+strlen(out_path)+1) );
	fprintf(f_parent_m3u8_file, "%s\n", (m3u8_name+strlen(out_path)+1) );
    }

    fprintf(f_parent_m3u8_file, "\n");
    fclose(f_parent_m3u8_file);

 error:
    for (i = 0;i < MAX_STREAM_PER_ENCAP; i++) {
	if (buf[i]) 
	    free(buf[i]);
	if(f_tmp_child_m3u8[i])
	    fclose(f_tmp_child_m3u8[i]);
	if(f_child_m3u8[i])
	    fclose(f_child_m3u8[i]);
    }
    return err;
}

static int32_t
ts2iphone_recursive_mkdir(const char *const path, int32_t mode)
{
    char *p, *tmp;
    struct stat st;
    int32_t rv;
    
    /* test if there is a file with the name of this directory already
     * existing 
     */
    if (!stat(path, &st)) {
	if (!S_ISDIR(st.st_mode)) {
	    return -1;
	}
	
    }
	    
    p = tmp = strdup(path);

    /* skip '/' at the start */
    while(*p == '/') {
	p++;
    }
    
    while((p = strchr(p, '/'))) {
	*p = '\0';
	if (!stat(tmp, &st)) {
	    if (!S_ISDIR(st.st_mode)) {
		free(tmp);
		return -1;
	    }
	} else {
	    rv = mkdir(tmp, mode);
	    if (rv) {
		free(tmp);
		return -1;
	    }
	}	    
		
	/* restore the slash */
	*p = '/';

	/* skip trailing slash */
	while (*p == '/') {
	    p++;
	}
    }
    
    free(tmp);
    return 0;
}

static int32_t
ts2iphone_update_bandwith_stats(ts2iphone_builder_t *bldr,
				ts_segment_writer_t *writer)
{
    uint32_t chunk_size, bitrate;
    olt_queue_elm_t *elm;
    vpe_olt_t *ol;

    chunk_size = 0;
    bitrate = 0;

    TAILQ_FOREACH(elm, &writer->ol_list_head, entries) {
	ol = elm->ol;
	chunk_size += ol->length;
    }

    bitrate = ((chunk_size << 3) / (bldr->duration)) * 1000;
    bldr->avg_bitrate[bldr->curr_profile - 1] =	\
	(((bldr->curr_seg_num - 1) *
	  bldr->avg_bitrate[bldr->curr_profile - 1]) +	\
	 bitrate )/ (bldr->curr_seg_num);
    if((uint32_t)bldr->duration >
	   (uint32_t)bldr->max_duration[bldr->curr_profile - 1])
	bldr->max_duration[bldr->curr_profile -1] = bldr->duration;

#if 0    
    printf("profile num: %d, moving avg bitrate %d, max seg duration"
	   " %d\n", bldr->curr_profile - 1,
	   bldr->avg_bitrate[bldr->curr_profile -1 ],
	   bldr->max_duration[bldr->curr_profile - 1]);
#endif 

    return 0;
    
}

static inline void
ts2iphone_caclulate_curr_seg_duration(ts2iphone_builder_t *bldr,
				      ts_segmenter_ctx_t *seg_ctx)
{
    /*once the I frame is detected update the
      prev_Iframe_PTS and calculate the duration*/
    bldr->current_PTS = seg_ctx->prev_Iframe_PTS;
    bldr->duration = (bldr->current_PTS - bldr->prev_PTS);
    
}

static inline void
ts2iphone_writer_set_duration(ts2iphone_builder_t *bldr)
{

    if(bldr->out) {
	bldr->out->duration = bldr->duration;
    }
}
