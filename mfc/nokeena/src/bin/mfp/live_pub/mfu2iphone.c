#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>

//parser includes
#include "nkn_vpe_types.h"
#include "nkn_vpe_mp4_parser.h"
#include "mfu2iphone.h"
#include "nkn_vpe_mfu_parse.h"
#include "nkn_vpe_utils.h"
#include "mfp_err.h"
#include "nkn_vpe_parser_err.h"

//NKN includes
#include "nkn_memalloc.h"
#include "nkn_authmgr.h"
#include "nkn_sched_api.h"
#include "nkn_rtsched_api.h"
#include "nkn_slotapi.h"
#include "nkn_vpe_bitio.h"
#include "mfp/thread_pool/mfp_thread_pool.h"
#include "nkn_assert.h"

#include "mfp_apple_io_hdlr.h"
#include "io_hdlr/mfp_io_hdlr.h"
#include "kms/mfp_kms_lib_cmn.h"

//zvm includes
//#include "zvm_api.h"
//#define APPLE_DEBUG
//#define DBG_FORCE_IO_ERR
#ifdef DBG_FORCE_IO_ERR
uint32_t dbg_num_calls = 0;
#endif

#define M3U8_PREAMBLE "#EXTM3U\n#EXT-X-VERSION:2\n#EXT-X-TARGETDURATION:%d\n#EXT-X-MEDIA-SEQUENCE:%d\n"

/*Global Variable to Maintain the m3u8 states*/
uint32_t tot_num_resched_sess = 0;
gbl_fruit_ctx_t fruit_ctx;
extern uint64_t glob_mfp_live_num_pmfs_cntr;
extern mfp_thread_pool_t* apple_fmtr_task_processor;

extern uint32_t glob_use_async_io_apple_fmtr;

uint32_t glob_apple_fmtr_num_task_low_threshold = 30000; //milli seconds
uint32_t glob_apple_fmtr_num_task_high_threshold = 200000; //milli seconds
extern uint32_t glob_latm_audio_encapsulation_enabled;

#define MAX_SPS_PPS_SIZE 300
#define FIXED_BOX_SIZE 8
/* unit_count,timescale,default_unit_duration,default_unit size each
   of4 bytes */
#define RAW_FG_FIXED_SIZE 16
#define FRUIT_DISCONT
#ifdef FRUIT_DISCONT
extern uint64_t glob_output_ts_strms_base_time;
#endif

/* mfu2ts function */
static ts_desc_t* mfu2ts_desc_init(char *, mfub_t*);
static int32_t mfu2ts_create_segment(ref_count_mem_t *ref_cont);

/* fruit formatter helper functions */
static int32_t fruit_write_ts(char* data, uint32_t len, char *fname);
static void fruit_get_video_name(char *output_path, char *video_name);
static void fruit_strip_uri_prefix(fruit_encap_t *encap);
static nkn_task_id_t fruit_create_auth_mgr_task(ref_count_mem_t *ref_cont);
static nkn_task_id_t fruit_create_enc_mgr_task(ref_count_mem_t *ref_cont);
static int32_t fruit_init_auth_msg(uint8_t authtype,
				   void *authdata, uint32_t seq_num,
				   auth_msg_t *out);
static int32_t fruit_init_enc_msg(fruit_fmt_input_t *fmt_input, enc_msg_t *em);
static void fruit_cleanup_enc_msg(enc_msg_t *em);
static void set_fruit_encaps(fruit_encap_t*, mfub_t *,char*);
static void set_fruit_stream(fruit_stream_t*,mfub_t *,char*,fruit_fmt_t );
static int32_t form_master_playlist(char* out_path, char* video_name);
static int32_t rename_master_playlist(char* out_path, char* video_name);

static int32_t form_child_playlist(char* out_path, char* master_name,
				char* child_name, uint32_t bitrate, char *uri_prefix);

static inline int32_t fruit_playlist_update_encr_line(FILE *f_m3u8_file,
						      uint32_t mod_chunk_num,
						      kms_key_resp_t* key_resp);

static inline int32_t fruit_encr_remove_keys(uint32_t start_key_seq_num, 
					     uint32_t K,	      			     
					     kms_client_key_store_t *key_store);
static void
cleanup_mfu2ts_desc(ts_desc_t *ts);


static int32_t update_stream_child_playlist(char* child_name, char* uri_prefix,
				  uint32_t duration,
				  char* out_path,
				  char* out_name,
				  uint32_t chunk_num,
				  fruit_fmt_t,
				  uint32_t*,
				  uint32_t*,
 			          kms_client_key_store_t *,   
				  kms_key_resp_t*
#ifdef FRUIT_DISCONT
                                     ,int8_t
#endif
				     );

static void form_fruit_chunk_name(char * fname, fruit_fmt_t fmt,
				  ts_desc_t *ts);
static inline void update_stream_max_duration(fruit_stream_t *stream,
					      uint32_t duration);
static int32_t finalize_child_playlist(const char *child_m3u8_name,
				       fruit_stream_t *stream);
/* RT Scheduler functions */
typedef void (*TaskFunc)(void* clientData);
static nkn_task_id_t rtscheduleDelayedTask(int64_t microseconds,
					   TaskFunc proc,
					   void* clientData) ;

/*
 * Module required for LATM integration
 */

static int32_t add_adts_header(fruit_encap_t *encaps, unit_desc_t *aud){
    int32_t ret = 0;
    uint32_t sample_cnt;
    uint32_t num_samples = aud->sample_count;
    uint32_t tot_sam_size = 0;
    
#define ADTS_HEADER_SIZE (7)    

    for(sample_cnt = 0; sample_cnt < num_samples; sample_cnt++){
    	tot_sam_size += (aud->sample_sizes[sample_cnt] + ADTS_HEADER_SIZE);
    }
    uint8_t *raw_buffer = aud->mdat_pos;
    
    aud->mdat_pos = (uint8_t *) nkn_malloc_type(tot_sam_size * sizeof(uint8_t), 
    	mod_vpe_unit_desc_t);
    
    if(aud->mdat_pos == NULL){
    	DBG_MFPLOG(encaps->name, ERROR, MOD_MFPLIVE, "Unable to allocate mem add_adts_header");
	return (-1);
    }
    //frame duration in 90000 clock ticks for various sample frequency
    //96k,	  88k,	 64k,	    48k,
    //44.1k, 32k,	24k,	   22.05k,
    //16k,	  12k,	 11.025k, 8k, 
    //undef, undef, undef, undef

    uint32_t std_sample_duration[16] = {960, 1045, 1440, 1920, 
	2090, 2880, 3840, 4180, 
	5760, 7680, 8360, 11520, 
	0, 0, 0, 0};

    uint16_t sync_word = 0x0fff;
    uint8_t id = 0; //mpeg4
    uint8_t layer  = 0;
    uint32_t protection_absent = 1;
    uint8_t profile_objecttype = 1; //aac LC
    uint32_t sampling_frequency_index;
    uint32_t i;

    uint32_t lhs, rhs;
    lhs = aud->sample_duration[0];
    for(i = 0; i < 16; i++){
    	rhs = std_sample_duration[i];
    	if((lhs == rhs)||(lhs == rhs-1)){
		sampling_frequency_index = i;
		break;
    	}
    }
    uint8_t private_bit = 0;
    uint8_t channel_configuration = 2;
    uint8_t original_copy = 0;
    uint8_t home = 0;

    uint8_t copyright_identification_bit = 0;
    uint8_t copyright_identification_start = 0;

    uint16_t aac_frame_length;
    uint16_t adts_buffer_fullness = 7;
    uint8_t num_of_datablocks_in_frame = 0;
    uint8_t adts_header[ADTS_HEADER_SIZE];

    adts_header[0] = (uint8_t)( (sync_word & 0xff0) >> 4);
    adts_header[1] = (uint8_t)( (sync_word & 0x00f) << 4 | 
    				((id&0x1) << 3) | 
    				((layer&0x3) << 1) | 
    				(protection_absent&0x1));
    adts_header[2] = (uint8_t)(	((profile_objecttype&0x3) << 6) | 
    		                ((sampling_frequency_index&0xf) << 2) |  
    		                ((private_bit&0x1) << 1) | 
    		                ((channel_configuration&0x4) >> 2));
    adts_header[3] = (uint8_t)( ((channel_configuration&0x3) << 6)|
    				((original_copy&0x1)<< 5) |
    				((home&0x1)<< 4)|
    				((copyright_identification_bit&0x1)<< 3)|
    				((copyright_identification_start&0x1)<< 2));
   //frame length is 2 bits in adts_header3, 8 bits in adts_header4, 3bits in adts_header5
   adts_header[6] = (uint8_t)( ((adts_buffer_fullness&0x3f) << 2)|
			       (num_of_datablocks_in_frame&0x3));

   uint8_t *buffer = aud->mdat_pos;
   uint32_t offset = 0;
   uint32_t aac_frame_len;
   
   for(sample_cnt = 0; sample_cnt < num_samples; sample_cnt++){
   	
	aac_frame_len = aud->sample_sizes[sample_cnt] + ADTS_HEADER_SIZE;

	adts_header[3] = (uint8_t)( (adts_header[3]) |
				    ((aac_frame_len&0x1800) >> 11));
	adts_header[4] = (uint8_t)( ((aac_frame_len&0x7f8) >> 3));
	adts_header[5] = (uint8_t)( ((aac_frame_len&0x7) << 5) |
				    ((adts_buffer_fullness&0x7c0) >> 6));
	memcpy(buffer + offset, adts_header, ADTS_HEADER_SIZE);
	offset += ADTS_HEADER_SIZE;
	memcpy(buffer + offset, raw_buffer, aud->sample_sizes[sample_cnt]);
	offset += aud->sample_sizes[sample_cnt];
	raw_buffer += aud->sample_sizes[sample_cnt];	
   }
   
   assert (offset == tot_sam_size);

   for(sample_cnt = 0; sample_cnt < num_samples; sample_cnt++){
       aud->sample_sizes[sample_cnt] += ADTS_HEADER_SIZE;
   }

   free(raw_buffer);
   return (ret);

}

static int32_t free_local_adts_mem(unit_desc_t *aud){
    int32_t ret = 0;
    if(aud->mdat_pos){
    	free(aud->mdat_pos);
	aud->mdat_pos = NULL;
    }
    return(ret);
}


/********************************************************************
 *               MFP LIVE Task Callbacks
 *******************************************************************/
void mfp_live_input(nkn_task_id_t tid)
{
}

static int32_t diffTimevalToMs(struct timeval const* from,
                               struct timeval const * val) {
    //if -ve, return 0
    double d_from = from->tv_sec + ((double)from->tv_usec)/1000000;
    double d_val = val->tv_sec + ((double)val->tv_usec)/1000000;
    double diff = d_from - d_val;
    if (diff < 0)
        return 0;
    return (int32_t)(diff * 1000);
}





void mfp_live_output(nkn_task_id_t tid)
{
    ref_count_mem_t *ref_cont;
    //    fruit_fmt_input_t *ffi;
    mfu_data_t *mfu_data;
    fruit_fmt_input_t *fmt_input;
    kms_client_key_store_t* kms_key_store;
    kms_key_resp_t* key_resp = NULL;
    uint32_t tmp = 0;

    ref_cont = (ref_count_mem_t*) \
	nkn_task_get_private(TASK_TYPE_MFP_LIVE, tid);
    mfu_data = (mfu_data_t*)ref_cont->mem;
    mfu_mfub_box_t* mfubbox = parseMfuMfubBox(mfu_data->data);
    mfub_t* mfu_hdr = mfubbox->mfub;
    fruit_encap_t *encaps = &fruit_ctx.encaps[mfu_hdr->program_number];
    fmt_input = (fruit_fmt_input_t *)mfu_data->fruit_fmtr_ctx;
    kms_key_store = (kms_client_key_store_t*)(fmt_input->am.authdata);
    mfu_data->fmtr_type = FMTR_APPLE;
    switch(fmt_input->state) {
	case FRUIT_FMT_AUTH:
	    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE,
		       "returning form auth mgr task for chunk num"
		       " %d", fmt_input->mfu_seq_num);
	    key_resp = &fmt_input->key_resp;
	    if (key_resp->key != NULL) {
		FRUIT_FMT_INPUT_SET_STATE(fmt_input, FRUIT_FMT_ENCRYPT);
		nkn_task_set_action_and_state(tid, TASK_ACTION_CLEANUP,
					      TASK_STATE_RUNNABLE);
	    } else {
		DBG_MFPLOG(encaps->name, ERROR, MOD_MFPLIVE,
			   "error retrieving key from KMS, returned"
			   " with errcode (%d)",
			   kms_key_store->kms_client->errcode);
		apple_fmtr_release_encap_ctxt(mfu_hdr->program_number);
		FMTR_ERRORCALLBACK(2);
		//		encaps->error_callback_fn(ref_cont);
		mfubbox->del(mfubbox);
		return;
	    }
	    break;
	case FRUIT_FMT_ENCRYPT:
	    {
		bitstream_state_t *bs;
		enc_msg_t *em;
#ifdef APPLE_DEBUG
		mfu_mfub_box_t* mfubbox = parseMfuMfubBox(mfu_data->data);
		mfub_t* mfu_hdr = mfubbox->mfub;
		DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE, "Start of encrypt to pl update: Session id: %d,"
			   " Stream id: %d MFU2TS formatter"
			   " Chunk number %d",
			   mfu_hdr->program_number,
			   mfu_hdr->stream_id,
			   fmt_input->mfu_seq_num);

#endif

		em = (enc_msg_t*)nkn_task_get_data(tid);
		bs = bio_init_bitstream((uint8_t*)em->out_enc_data, 
					em->out_len);
		bio_aligned_seek_bitstream(bs, 0, SEEK_END);
		fruit_write_ts((char*)bio_get_buf(bs), bio_get_pos(bs),
				fmt_input->chunk_name);

		/* free the input un-encrypted and output encrypted
		   streams */
		free(bio_get_buf(fmt_input->ts_stream));
		bio_close_bitstream(fmt_input->ts_stream);
		bio_close_bitstream(bs);//bs->data will be freed in write flow
#ifdef APPLE_DEBUG
                DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE, "End of encrypt to pl update: Session id: %d,"
                           " Stream id: %d MFU2TS formatter"
                           " Chunk number %d",
                           mfu_hdr->program_number,
                           mfu_hdr->stream_id,
                           fmt_input->mfu_seq_num);

#endif

		FRUIT_FMT_INPUT_SET_STATE(fmt_input, FRUIT_FMT_PL_UPDATE);
		nkn_task_set_action_and_state(tid, TASK_ACTION_CLEANUP,
					      TASK_STATE_RUNNABLE);

		break;
	    }
	default:
	    break;
    }
#ifdef APPLE_DEBUG   
    {
	mfu_mfub_box_t* mfubbox = parseMfuMfubBox(mfu_data->data);
	mfub_t* mfu_hdr = mfubbox->mfub;
	struct timeval st,en;
	DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE, "Calling RTsched: Session id: %d,"
		   " Stream id: %d MFU2TS formatter"
		   " Chunk number %d",
		   mfu_hdr->program_number,
		   mfu_hdr->stream_id,
		   fmt_input->mfu_seq_num);
    
	gettimeofday(&st, NULL);
	/*rtscheduleDelayedTask(0, (TaskFunc)mfubox_ts,
			      (void*)
			      ref_cont); */
	taskHandler handler = (taskHandler)mfubox_ts;
	thread_pool_task_t* t_task = 
		newThreadPoolTask(handler, ref_cont, NULL);
	apple_fmtr_task_processor->add_work_item(apple_fmtr_task_processor, t_task); 
			      
	gettimeofday(&en, NULL);
	int32_t idle_time_ms =  diffTimevalToMs(&en, &st);
	DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE, "Completed RTsched: Session id: %d,"
                   " Stream id: %d MFU2TS formatter"
                   " Chunk number %d",
		   " Time taken %d",
                   mfu_hdr->program_number,
                   mfu_hdr->stream_id,
                   fmt_input->mfu_seq_num,
		   idle_time_ms);
	if(idle_time_ms > 500)
	    DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE, "Completed RTsched: Session id: %d,"
                   " Stream id: %d MFU2TS formatter"
		       " Chunk number %d",
		       " Time taken %d",
		       mfu_hdr->program_number,
		       mfu_hdr->stream_id,
		       fmt_input->mfu_seq_num,
		       idle_time_ms);



    }
#else
    /*rtscheduleDelayedTask(0, (TaskFunc)mfubox_ts,
			  (void*)
			  ref_cont);*/
	taskHandler handler = (taskHandler)mfubox_ts;
	thread_pool_task_t* t_task = 
		newThreadPoolTask(handler, ref_cont, NULL);
	apple_fmtr_task_processor->add_work_item(apple_fmtr_task_processor, t_task); 
			  
#endif
    mfubbox->del(mfubbox);

}

void mfp_live_cleanup(nkn_task_id_t tid)
{
}

//Module to validate the num of active rtsched tasks
//safety feature- used to kill apple formatter and pull down the session
//based on task buildup on any stream
static void validate_active_task_count(
				       fruit_encap_t * encaps, 
				       int32_t *active_task_cntr,
				       uint32_t sess_id,
				       uint32_t stream_id){
    
    uint32_t i;

    //DBG_MFPLOG("MFU2TS", MSG, MOD_MFPLIVE,
    //"sess[%u] stream[%u]  apple formatter num active task %d", 
    //sess_id, stream_id, *active_task_cntr);    
#if 0
    //check for lower threshold limit
    if(*active_task_cntr > (int32_t)encaps->task_build_warn_limit&&
    	*active_task_cntr <= (int32_t)encaps->task_build_high_limit){
   	//warning case
   	DBG_MFPLOG("MFU2TS", WARNING, MOD_MFPLIVE,
   	"sess[%u] stream[%u] apple formatter num active task (%d) > %d", 
        sess_id, stream_id, *active_task_cntr,encaps->task_build_warn_limit);
    }
#endif

    //check for higher threshold limit
    if(*active_task_cntr > (int32_t)encaps->task_build_high_limit){
   	//error case
	if(!(*active_task_cntr%1000)){
	    DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE,
		       "sess[%u] stream[%u] apple formatter num active task (%d) > %d", 
		       sess_id, stream_id, *active_task_cntr, encaps->task_build_high_limit);

	    NKN_ASSERT(0);
	}
#if 0
   	for(i = 0; i < encaps->num_streams_active; i++){
		pthread_mutex_lock(&encaps->stream[i].lock);
		encaps->stream[i].status = FRUIT_STREAM_ERROR;
		pthread_mutex_unlock(&encaps->stream[i].lock);
   	}
#endif	
    }
  
    return;
}

//Module to update(decrement) the num of active rtsched tasks
//safety feature- used to kill apple formatter and pull down the session
//based on task buildup on any stream
static void decr_active_task_count(
	fruit_encap_t *encaps,
	int32_t *active_task_cntr){
    //decrement the counter
    *active_task_cntr = *active_task_cntr - 1;
    return;
}

/* compute the instantaneous, max, avg bitrates */
static void calc_stats(
    int8_t *name,
    uint32_t inst_duration,
    uint32_t sess_id,
    uint32_t strm_id,
    ts_desc_t *ts,
    uint32_t eos_flag){
	
    uint32_t bitrate;
    float deviation_percentage;
    bitrate = 0;

    if(ts){

    ts->inst_deviation = 0;
    deviation_percentage = 0.0;
    
    if(!eos_flag){
	    if (inst_duration) {	    /*kbps*/
	    	ts->num_samples++;
		bitrate = ((ts->chunk_size << 3) / (inst_duration)) / 1000;
	    }
	    if (bitrate > ts->max_bitrate) {
		ts->max_bitrate = bitrate;
	    }
	    if(ts->num_samples > 1){
	    	ts->avg_bitrate = (((ts->num_samples - 2) * ts->avg_bitrate) + \
			       bitrate )/ (ts->num_samples - 1);
		ts->inst_deviation = bitrate - ts->avg_bitrate;
		if(ts->inst_deviation > 0){
			deviation_percentage = (ts->inst_deviation * 100)/ts->avg_bitrate;
		}
		else{
			int32_t tmp = ts->avg_bitrate - bitrate;
			deviation_percentage = (tmp * 100)/ts->avg_bitrate;
		}			
	    }	
	    
	    ts->sum_of_deviation_square += pow(ts->inst_deviation , 2);
	    ts->standard_deviation = sqrt(ts->sum_of_deviation_square/ts->num_samples);	

	    if(deviation_percentage > (float)10){
	    	DBG_MFPLOG(name, WARNING, MOD_MFPLIVE,
			"sess %u stream %u Media segment bitrate outside of target playlist bitrate by %2.0f%%",
			sess_id, strm_id, deviation_percentage);
	    	DBG_MFPLOG(name, WARNING, MOD_MFPLIVE,
			"Bitrate [%u]  Vs Average [%u]", bitrate, ts->avg_bitrate);
	    }
    }else{
	    DBG_MFPLOG(name, MSG, MOD_MFPLIVE,
		"sess %u stream %u AvgBitrate %u StandardDeviation %f",
		sess_id, strm_id,ts->avg_bitrate,
		ts->standard_deviation);
    }
    }    
}

static inline void update_stream_max_duration(fruit_stream_t *stream,
					      uint32_t duration)
{
    if (duration > stream->max_duration) {
	stream->max_duration = duration;
    }
    
}


//! module to publish parent playlist with details of
//! stream info

static int32_t publish_parent_playlist(
    fruit_encap_t *encaps,
    fruit_stream_t *stream,
    uint32_t chunk_num, 
    uint32_t min_chunk_num,
    uint8_t target_chunk_num)
{

    int32_t rv = 0;
    char *mfu_uri_fruit_prefix, *mfu_out_fruit_path;
    
    mfu_out_fruit_path = encaps->fruit_formatter.mfu_out_fruit_path;
    mfu_uri_fruit_prefix = encaps->fruit_formatter.mfu_uri_fruit_prefix;

    if(!stream->ts){
    	rv = -1;
    	return (rv);
    }
    /* if the session ends before the min children in playlist is
    reached, then we might as well flush the playlists. this may
    happen in the case of file publishing, where the number of
    segments created may be less than the min segs in child playlist
    */
    if ((stream->status == FRUIT_STREAM_ENDLIST_PUB || stream->status == FRUIT_STREAM_ERROR) &&
	encaps->status == FRUIT_ENCAP_PART_ACTIVE){
	int ret;
	ret = form_master_playlist(mfu_out_fruit_path,
				   encaps->m3u8_file_name);
	if(ret < 0 ) {
	  DBG_MFPLOG(encaps->name, ERROR, MOD_MFPLIVE,
		     " I/O Error (%d) - Unable"
		     " to form master playlist", ret);
	    rv = -1;
	    return (rv);
	}
	ret = form_child_playlist(mfu_out_fruit_path, 
				  encaps->m3u8_file_name,
				  stream->m3u8_file_name,
				  stream->ts->avg_bitrate,
				  mfu_uri_fruit_prefix);
	if(ret < 0 ) {
	  DBG_MFPLOG(encaps->name, ERROR, MOD_MFPLIVE,
                     " I/O Error (%d) - Unable"
                     " to form child playlist", ret);
	  rv = -1;
	  return (rv);
        }
	ret = rename_master_playlist(mfu_out_fruit_path,
				     encaps->m3u8_file_name);
	if(ret < 0){
	    DBG_MFPLOG(encaps->name, ERROR, MOD_MFPLIVE,"Renaming master playlist failed");
	    rv = -1;
	    return (rv);
	}
    }

    if (encaps->status == FRUIT_ENCAP_PART_ACTIVE) {
	if(chunk_num >= min_chunk_num) {// || dvr_mode
	    encaps->status = FRUIT_ENCAP_TEMP_MASTER_PL;
	    int32_t ret = 0;
	    ret = form_master_playlist(mfu_out_fruit_path,
				       encaps->m3u8_file_name);
	    if(ret < 0) {
	      DBG_MFPLOG(encaps->name, ERROR, MOD_MFPLIVE,
			 " I/O Error (%d) - Unable"
			 " to form master playlist", ret);
		rv = -1;
		return (rv);
	    }
	}
    }

	
    if (stream->status == FRUIT_STREAM_PART_ACTIVE) {
	if(chunk_num >= min_chunk_num) {// || dvr_mode
	    stream->status = FRUIT_STREAM_ACTIVE;
	    int32_t ret = 0;
	    ret = form_child_playlist(mfu_out_fruit_path, 
				      encaps->m3u8_file_name,
				      stream->m3u8_file_name,
				      stream->ts->avg_bitrate,
				      mfu_uri_fruit_prefix);
	    if(ret < 0) {
	      DBG_MFPLOG(encaps->name, ERROR, MOD_MFPLIVE,
			 " I/O Error (%d) - Unable"
                         " to form child playlist", ret);
	      rv = -1;
	      return (rv);
            }
	} 
    }

    if(encaps->status == FRUIT_ENCAP_TEMP_MASTER_PL) {
    	int ret;
	if(((encaps->mfu_seq_num + encaps->fruit_formatter.segment_start_idx) > 
	    target_chunk_num)){
	        encaps->status = FRUIT_ENCAP_ACTIVE;
		ret = rename_master_playlist(mfu_out_fruit_path,
		     encaps->m3u8_file_name);
		if(ret < 0){
			DBG_MFPLOG(encaps->name, ERROR, MOD_MFPLIVE,"Renaming master playlist failed");
			rv = -1;
			return (rv);
		}
	}
	    
    }	
    return(rv);
}

//! chunks produces are updated on their respective child playlist
//! based on sync across profiles and serialization within a stream
//! using updplqueue - update playlist queue

static int32_t update_session_child_playlist(
	fruit_encap_t *encaps,
	int32_t sess_id){
	
	 //publish child playlist for all streams
	 uint32_t slot_cnt, pub_strm = 0;
	 update_playlist_queue_t *updplqueue;
	 kms_client_key_store_t   *kms_stream_key_store = NULL;
	 int32_t err = 0;
	 int32_t rv = 0;

	 //pthread_mutex_lock(&encaps->lock);
	 for(slot_cnt = 0; 
	     ((pub_strm < encaps->num_streams_active) 
		 && (slot_cnt < MAX_FRUIT_STREAMS)); 
	     slot_cnt++){
	     //check whether slot is filled	 
	     if(encaps->updplqueue[slot_cnt].slot_filled == 1){
		 pub_strm++;
		 uint32_t strm_cnt = encaps->updplqueue[slot_cnt].stream_id;
		 fruit_stream_t *stream_desc = &encaps->stream[strm_cnt];
		 updplqueue = &encaps->updplqueue[slot_cnt];
		 if (updplqueue->key_resp)
		     kms_stream_key_store = fruit_ctx.encaps[sess_id].fruit_formatter.kms_key_stores[slot_cnt];
		 pthread_mutex_lock(&stream_desc->lock);
		 update_stream_max_duration(stream_desc,
					    updplqueue->duration);
		 err = update_stream_child_playlist(stream_desc->m3u8_file_name,
			 encaps->fruit_formatter.mfu_uri_fruit_prefix, 
			 updplqueue->duration,
			 encaps->fruit_formatter.mfu_out_fruit_path,
			 stream_desc->ts->out_name,
			 encaps->mfu_seq_num, // ts->chunk_num-1,
			 encaps->fruit_formatter,
			 &stream_desc->seq_num,
			 &stream_desc->total_num_chunks, 
			 kms_stream_key_store,		     
			 updplqueue->key_resp
#ifdef FRUIT_DISCONT
			,updplqueue->discontinuity_flag
#endif
			);
	
		 if (err < 0){
		   //FMTR_ERRORCALLBACK(2);
		   DBG_MFPLOG(encaps->name, ERROR, MOD_MFPLIVE,
			      "stream id %d, error updating child playlist,"
			      " errno %d", strm_cnt, err);
		   pthread_mutex_unlock(&stream_desc->lock);
		   NKN_ASSERT(0);
		   //pthread_mutex_unlock(&encaps->lock);
		   rv = -1;
		   return (rv);
		 }
		 updplqueue->slot_filled = 0;
		 encaps->num_streams_pub_msn--;
		 //if(encaps->updplqueue[strm_cnt].key_resp){
		 //  deleteKmsKeyResponse(encaps->updplqueue[strm_cnt].key_resp);
		 //}
		 pthread_mutex_unlock(&stream_desc->lock);
	     }			 
	 }	     
	 if(encaps->num_streams_pub_msn == 0){
	     encaps->mfu_seq_num++;
	 }	
	 
	 //pthread_mutex_unlock(&encaps->lock);
	 return (rv);

}

static int32_t process_stream_endlist(
	uint32_t sess_id,
	uint32_t stream_id,
	uint32_t mfu_seq_num
	){
	
	int32_t rv = 0;
	char* mfu_out_fruit_path;
	char child_m3u8_name[500];	
	fruit_encap_t *encaps;
	fruit_stream_t *stream_desc;
	const fruit_fmt_t *fmt;

	fmt = &(encaps->fruit_formatter);
	encaps = &fruit_ctx.encaps[sess_id];
	stream_desc = &encaps->stream[stream_id];

	
	mfu_out_fruit_path = encaps->fruit_formatter.mfu_out_fruit_path;

	  pthread_mutex_lock(&stream_desc->lock);

	    //calculate statistics at stream level
	  calc_stats(
	      (int8_t*)encaps->name,
	      0,
	      sess_id,
	      stream_id,     
	      stream_desc->ts, 1);

	    stream_desc->is_end_list = 1; 
	    snprintf(child_m3u8_name, strlen(mfu_out_fruit_path)
		     + strlen(stream_desc->m3u8_file_name)+2,
		     "%s/%s", mfu_out_fruit_path,
		     stream_desc->m3u8_file_name);
	    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, "Stream [%d] add"
		       " endlist tag", stream_id); 
	    /**
	     * check if the NULL MFU sequence number is less than
	     * min segs in child playlist. if this is the case 
	     * then we need to rename the temp file and add the
	     * endlist 
	     */
	    if ((mfu_seq_num + encaps->fruit_formatter.segment_start_idx) < 
		encaps->fruit_formatter.min_segment_child_plist) {
		char tmp_child_m3u8[500] = {0};
		snprintf(tmp_child_m3u8, 500,
			 "%s/temp_%s", mfu_out_fruit_path,
			 stream_desc->m3u8_file_name);
		rename(tmp_child_m3u8, child_m3u8_name);
	    }

	    /** 
	     * for DVR/File, the target duration is written at the
	     * begining of the session based on the duration of the
	     * segments seen at that point. If one of the segments
	     * subsequently has a longer duration, then the target
	     * duration tag is not updated accordingly. this function
	     * updates the target duration to the maximum duration
	     */
	    if (stream_desc->status == FRUIT_STREAM_ACTIVE &&
		fmt->dvr) {
		rv = finalize_child_playlist(child_m3u8_name, stream_desc);
		if (rv) {
		    DBG_MFPLOG(encaps->name, ERROR,
			       MOD_MFPLIVE,
			       "Error finalizing child playlist; error"
			       " code = %d", rv);
		}

	    }
	    rv = update_child_end_list(child_m3u8_name);
	    if(rv < 0){
		DBG_MFPLOG(encaps->name, ERROR, MOD_MFPLIVE,
			   "Stream [%d] I/O Error (%d) - Unable"
			   " to write endlist tag", stream_id, rv);
	    }

	    //encaps->stream[i].status = FRUIT_STREAM_ERROR;
	    if (stream_desc->status != FRUIT_STREAM_INACTIVE) {
		/* if we hit endlist without even producing one
		 * segment then 'ts' descriptors are not created
		 * except for the one stream which is processing the
		 * first EOS
		 */
		if (stream_desc->ts) {
		    /* check added because, we could have hit an error
		     * before or after the TS description creation
		     */
		    mfu_clean_ts(stream_desc->ts);
		    cleanup_mfu2ts_desc(stream_desc->ts);
		}
	    }	
	    
	  pthread_mutex_unlock(&stream_desc->lock);
	    
	  return (rv);
}

static int32_t finalize_child_playlist(const char *child_m3u8_name,
				       fruit_stream_t *stream)
{

    int32_t err = 0;
    uint32_t duration = 0, seq_num = 0;
    FILE *fp = NULL, *fout = NULL;
    char temp_dvr_name[500] = {0};
    size_t copy_start = 0, file_size = 0, read_size = 0, rv = 0;
    uint8_t *temp_dvr_data = NULL;

    fp = fopen(child_m3u8_name, "rb");
    if (!fp) {
	err = -E_VPE_PARSER_INVALID_OUTPUT_PATH;
	goto done;
    }

    /* compute file size */
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);

    /* parse preamble */
    fscanf(fp, M3U8_PREAMBLE, &duration, &seq_num);
    if (stream->max_duration == duration) {
	err = 0;
	goto done;
    }

    /* copy the remaining data */
    copy_start = ftell(fp);
    temp_dvr_data = (uint8_t *)\
	nkn_malloc_type(file_size, mod_vpe_charbuf);
    rv = fread(temp_dvr_data, 1, (read_size = file_size - copy_start), fp);
    if (rv != read_size) {
	err = -E_VPE_READ_INCOMPLETE;
	goto done;
    }	
    fclose(fp);
    fp = NULL;
    
    /* write modified preamble with new duration and copy the
     * remanining data 
     */
    snprintf(temp_dvr_name, 500, "%s", child_m3u8_name);
    fout = fopen(temp_dvr_name, "wb");
    if (!fout) {
	err = -E_VPE_PARSER_INVALID_OUTPUT_PATH;
	goto done;
    }
    fprintf(fout, M3U8_PREAMBLE, stream->max_duration, seq_num);
    rv = fwrite(temp_dvr_data, 1, read_size, fout);
    if (rv != read_size) {
	err = -E_VPE_PARSER_INVALID_OUTPUT_PATH;
	goto done;
    }	
    rename(temp_dvr_name, child_m3u8_name);

 done:
    if (fp) fclose(fp);
    if (fout) fclose(fout);
    if (temp_dvr_data) free(temp_dvr_data);

    return err;
}

//module to add EXT-X-ENDLIST tag in the child playlist
static int32_t process_session_endlist(fruit_encap_t *encaps,
			       fruit_fmt_input_t *fmt_input,
			       uint32_t sess_id)
{
	uint32_t i;
	int32_t rv = 0, err_flag = 0;

	uint32_t num_streams_active = encaps->num_streams_active;

	for(i = 0; i < num_streams_active; i++){

	   fruit_stream_t *stream_desc = &encaps->stream[i];

	   if(stream_desc->status != FRUIT_STREAM_ENDLIST_PUB){

		   rv = process_stream_endlist(sess_id, i, 
		    	fmt_input->mfu_seq_num);

		   if(rv < 0){
		       err_flag = rv;
		   }	    

		   pthread_mutex_lock(&stream_desc->lock);

		   if(stream_desc->status == FRUIT_STREAM_ERROR){
		       num_streams_active++;
		   }
		   stream_desc->status = FRUIT_STREAM_ENDLIST_PUB;
	
		   pthread_mutex_unlock(&stream_desc->lock);
	   }
	}
	
	/* flag set when any one stream hit an error when writing
	 * endlist tag
	 */
	if (err_flag) 
	    rv = err_flag;

	return(rv);
}

static void convert_rwdesc2_unitdesc(
				     mfu_contnr_t *mfu_contnr,
				     unit_desc_t *vmd,
				     unit_desc_t *amd){
    
	mfu_rw_desc_t* rw_desc;
        uint32_t i;
	int32_t video_index = -1;
	int32_t audio_index = -1;

	for (i = 0; i < mfu_contnr->rawbox->desc_count; i++){
		if(!strncmp(mfu_contnr->datbox[i]->cmnbox->name, "vdat", 4))
			video_index = i;
		if(!strncmp(mfu_contnr->datbox[i]->cmnbox->name, "adat", 4))
			audio_index = i;				
	}
	
	assert(video_index != -1);
	//assert(audio_index != -1);
	//assert(video_index != audio_index);
	
	if(video_index != -1){
	    rw_desc = mfu_contnr->rawbox->descs[video_index];
	    memset(vmd, 0, sizeof(unit_desc_t));
	    vmd->sample_count = rw_desc->unit_count;
	    vmd->default_sample_duration = rw_desc->default_unit_duration;
	    vmd->default_sample_size = rw_desc->default_unit_size;
	    vmd->timescale = (uint64_t)rw_desc->timescale;
	    if(rw_desc->sample_duration)
	    	vmd->sample_duration = rw_desc->sample_duration;
	    if(rw_desc->composition_duration)
	    	vmd->composition_duration = rw_desc->composition_duration;
	    if(rw_desc->sample_sizes)
	    	vmd->sample_sizes = rw_desc->sample_sizes;
	    vmd->mdat_pos = (uint8_t *)mfu_contnr->datbox[video_index]->dat;
	}
	
	if(audio_index != -1){
	    rw_desc = mfu_contnr->rawbox->descs[audio_index];
	    memset(amd, 0, sizeof(unit_desc_t));
	    amd->sample_count = rw_desc->unit_count;
	    amd->default_sample_duration = rw_desc->default_unit_duration;
	    amd->default_sample_size = rw_desc->default_unit_size;
	    amd->timescale = (uint64_t)rw_desc->timescale;
	    if(rw_desc->sample_duration)
	    	amd->sample_duration = rw_desc->sample_duration;
	    if(rw_desc->composition_duration)
	    	amd->composition_duration = rw_desc->composition_duration;
	    if(rw_desc->sample_sizes)
	    	amd->sample_sizes = rw_desc->sample_sizes;
	    amd->mdat_pos = (uint8_t *)mfu_contnr->datbox[audio_index]->dat;
	}
	return;
}



/**
 *  MFU2TS FRUIT_FMT_CONVERT state handler
 */
static int32_t mfu2ts_create_segment(ref_count_mem_t *ref_cont)
{
    ts_desc_t *ts;
    unit_desc_t *vmd = NULL, *amd = NULL;    
    mfub_t *mfu_hdr;
    int32_t ret = 0;
    char* mfu_out_fruit_path =NULL, *mfu_uri_fruit_prefix=NULL;
    char *chunk_fname;
    char child_m3u8_name[500];
    //uint32_t min_chunk_num;
    uint32_t dvr_mode, eos, encr_flag = 0;
    mfu_data_t *mfu_data = (mfu_data_t*)ref_cont->mem;
    fruit_fmt_input_t *fmt_input =
		(fruit_fmt_input_t *)mfu_data->fruit_fmtr_ctx;
    kms_client_key_store_t** kms_key_stores = NULL;
    int32_t rv = 0;
    mfu_contnr_t *mfu_contnr;
    fruit_encap_t *encaps;
    fruit_stream_t *stream;
    uint32_t i;
    int32_t video_index = -1, audio_index = -1;
	uint32_t program_num, stream_num;
    uint32_t endlist_mfu = 0;

    /*Init data*/
    eos = 0;

    chunk_fname = fmt_input->chunk_name;

    /*Parse the MFU box*/ 
    mfu_contnr = parseMFUCont(mfu_data->data);

    if (mfu_contnr == NULL){
	DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE, 
		   " set_fruit_stream formatter returned error; STREAM DEAD");
	ret = -1;
        goto create_segment_ret;
    }
    mfu_hdr = mfu_contnr->mfubbox->mfub;
    program_num = mfu_hdr->program_number;
    stream_num = mfu_hdr->stream_id;
    encaps = &fruit_ctx.encaps[program_num];
    stream = &encaps->stream[stream_num];

    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE,
       "apple fmtr received sess %u strm %u seqnum %u",
       program_num, stream_num, 
       fmt_input->mfu_seq_num + encaps->fruit_formatter.segment_start_idx);
    
#ifdef APPLE_DEBUG
    DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE, "Start Session id: %d,"
               " Stream id: %d MFU2TS formatter"
               " Chunk number %d",
               mfu_hdr->program_number,
               mfu_hdr->stream_id,
               fmt_input->mfu_seq_num);
#endif 
    if(stream->status == FRUIT_STREAM_ENDLIST_PUB){
      DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE,
		 "stream %d, exp %d, got %d - Dropped at createSegment",
		 mfu_hdr->stream_id,
		 stream->total_num_chunks,
		 fmt_input->mfu_seq_num + encaps->fruit_formatter.segment_start_idx);                   
      ret = 2;
      goto create_segment_ret;
    }

    if(stream->status == FRUIT_STREAM_ERROR){
	DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE,
		    "stream %d, exp %d, got %d - Dropped at createSegment",
		    mfu_hdr->stream_id,
		    stream->total_num_chunks,
		    fmt_input->mfu_seq_num + encaps->fruit_formatter.segment_start_idx);    	
	ret = -2;
	if (mfu_hdr->offset_vdat == 0xffffffffffffffff &&
			mfu_hdr->offset_adat == 0xffffffffffffffff){
		//all tracks endlist are added at the first instance of
		//NULL mfu in any one of the tracks.so drop other null MFU without error
		endlist_mfu = 1;

		char child_m3u8[500];
		char* mfu_out_fruit_uri = encaps->fruit_formatter.mfu_out_fruit_path;
		encaps->stream[stream_num].is_end_list = 1;
		snprintf(child_m3u8, 500, "%s/%s", mfu_out_fruit_uri,
				encaps->stream[stream_num].m3u8_file_name);
		/**
		 * check if the NULL MFU sequence number is less than
		 * min segs in child playlist. if this is the case 
		 * then we need to rename the temp file and add the
		 * endlist 
		 */
		if ((fmt_input->mfu_seq_num +
		    encaps->fruit_formatter.segment_start_idx) < 
		    encaps->fruit_formatter.min_segment_child_plist) {
		    char tmp_child_m3u8[500] = {0};
		    snprintf(tmp_child_m3u8, strlen(mfu_out_fruit_uri)
			     + strlen(encaps->stream[stream_num].m3u8_file_name)+2,
			     "%s/temp_%s", mfu_out_fruit_uri,
			     encaps->stream[stream_num].m3u8_file_name);
		    rename(tmp_child_m3u8, child_m3u8);
		}

		DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE,
				"Stream [%d] add endlist tag", stream_num);
		rv = process_session_endlist(encaps, fmt_input, mfu_hdr->program_number);
		if(rv <0) {
                  ret = -1;
                  goto create_segment_ret;
                }
		if (publish_parent_playlist(encaps, stream, stream->chunk_num,
					    encaps->fruit_formatter.min_segment_child_plist,
					    encaps->fruit_formatter.min_segment_child_plist) == -1) {
		    ret = -1;
		    goto create_segment_ret;
		}

		ret = 2;
	}
        goto create_segment_ret;
    }
    	

    /*Initialize "Fruit" header*/
    mfu_out_fruit_path =
	encaps->fruit_formatter.mfu_out_fruit_path;
    mfu_uri_fruit_prefix =
	encaps->fruit_formatter.mfu_uri_fruit_prefix;

    /*Check if encap is new: set it if so*/
    if (encaps->status == FRUIT_ENCAP_INACTIVE) {
	set_fruit_encaps(encaps,
			 mfu_hdr, mfu_uri_fruit_prefix);
	set_fruit_stream(stream, 
			 mfu_hdr, mfu_out_fruit_path,
			 encaps->fruit_formatter);
	//2 init session level mfu_seq_num
	pthread_mutex_lock(&encaps->lock);
	encaps->num_streams_pub_msn = 0;
	pthread_mutex_unlock(&encaps->lock);

    } else if (stream->status == FRUIT_STREAM_INACTIVE) {
	set_fruit_stream(stream, mfu_hdr,
			 mfu_out_fruit_path,
			 encaps->fruit_formatter);
    }

    //chunk_num = fmt_input->mfu_seq_num;
    //min_chunk_num = encaps->fruit_formatter.min_segment_child_plist;
    dvr_mode = encaps->fruit_formatter.dvr;
    kms_key_stores = encaps->fruit_formatter.kms_key_stores;
    if (kms_key_stores != NULL) {
	encr_flag = 1;
    }

    /* check if end of stream skip the playlist write and only write
       the endlist tag*/
    if (mfu_hdr->offset_vdat == 0xffffffffffffffff &&
	mfu_hdr->offset_adat == 0xffffffffffffffff) {
	/* End of Steam Signal received do the following
	 * a. set the is_end_list flag
	 * b. write the end list tag via update_child_end_list API
	 * c. release reference count
	 */
	/* serialize playlist update */
	endlist_mfu = 1;
	pthread_mutex_lock(&stream->lock);
	if((stream->total_num_chunks == fmt_input->mfu_seq_num + encaps->fruit_formatter.segment_start_idx) 
	   || (fmt_input->mfu_seq_num == encaps->fruit_formatter.segment_start_idx)) {
	   
	    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, "sess [%d] strm [%d] enter create endlist"
                       " tot_chunk %d, mfu_seq %d", mfu_hdr->program_number, mfu_hdr->stream_id,
                       stream->total_num_chunks,
                       fmt_input->mfu_seq_num);
	    pthread_mutex_unlock(&stream->lock);
	    rv = process_session_endlist(encaps, fmt_input, mfu_hdr->program_number);
	    if(rv <0) {
		    ret = -1;
		    goto create_segment_ret;
	    }
	    if (publish_parent_playlist(encaps, stream, stream->chunk_num,
					encaps->fruit_formatter.min_segment_child_plist,
					encaps->fruit_formatter.min_segment_child_plist) == -1) {
		ret = -1;
		goto create_segment_ret;
	    }


	    ret = 2;
	} else {
	    /* post a task for the next state */
	    pthread_mutex_unlock(&stream->lock);
	    if(stream->num_resched++%300 == 0)
	    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, "sess [%d] strm [%d] create endlist resched "
		       "tol_chunk %d, mfu_seq %d", mfu_hdr->program_number,
		       mfu_hdr->stream_id,
		       stream->total_num_chunks,
		       fmt_input->mfu_seq_num);
	    ret = 1;
	}
	goto create_segment_ret;
    }

    ts = stream->ts;

    if (!ts) {
	/* stream not yet started */
	ret = -1;
	DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, " Create segment ret =-1");
	goto create_segment_ret;
    }
    ts->chunk_num = fmt_input->mfu_seq_num;

    form_fruit_chunk_name(chunk_fname,
			  encaps->fruit_formatter,
			  ts);

    /*to maintain the audio mdat_pos, this is required for freeing. In
      mfu2_ts function, the audio_mdat_pos is getting changed*/
    for (i = 0; i < mfu_contnr->rawbox->desc_count; i++){
      if(!strncmp(mfu_contnr->datbox[i]->cmnbox->name, "vdat", 4))
	video_index = i;
      if(!strncmp(mfu_contnr->datbox[i]->cmnbox->name, "adat", 4))
	audio_index = i;
    }
    assert (video_index != -1);
    if(video_index != -1) {
      vmd = (unit_desc_t*)nkn_malloc_type(sizeof(unit_desc_t), 
					  mod_vpe_unit_desc_t);
    }
    if(audio_index != -1) {
      amd = (unit_desc_t*)nkn_malloc_type(sizeof(unit_desc_t),
                                          mod_vpe_unit_desc_t);
    }
    convert_rwdesc2_unitdesc(mfu_contnr, vmd, amd);

    if(audio_index != -1) {
      ts->aud_mdat_pos_add = amd->mdat_pos;
    }

    if(glob_latm_audio_encapsulation_enabled){
    	if(add_adts_header(encaps, amd) == -1){
		ret = -1;
		DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, " add_adts_header ret =-1");
		goto create_segment_ret;		
    	}
	ts->aud_new_mdat_buffer = amd->mdat_pos;
    }
#ifdef FRUIT_DISCONT
    if(fmt_input->fmt_refresh){
        ts->base_vid_time = glob_output_ts_strms_base_time;
        ts->stream_aud[0].base_time = glob_output_ts_strms_base_time;
        printf("Times tamp refreshed to 0\n");
    }
#endif

    ret = mfu2_ts(ts,(unit_desc_t*)vmd,(unit_desc_t*)amd,
		  mfu_contnr->rawbox->descs[video_index]->codec_specific_data,
		  mfu_contnr->rawbox->descs[video_index]->codec_info_size,
		  chunk_fname,mfu_data->data_size,
		  encr_flag, fmt_input->sioc, &fmt_input->ts_stream);
#ifdef APPLE_DEBUG   
    DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE, "End Session id: %d,"
	       " Stream id: %d MFU2TS formatter"
	       " Chunk number %d",
	       mfu_hdr->program_number,
	       mfu_hdr->stream_id,
	       fmt_input->mfu_seq_num);
#endif
    if(glob_latm_audio_encapsulation_enabled){
	amd->mdat_pos = ts->aud_new_mdat_buffer;
	free_local_adts_mem(amd);
    }

#ifdef DBG_FORCE_IO_ERR
    dbg_num_calls++;
    if (dbg_num_calls >= 20) {
	ret = -1;
    }
#endif
    if (ret < 0) {
	//stream->status = FRUIT_STREAM_ERROR; 
	DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE, "Encaps No: %d,"
		   " Stream id: %d MFU2TS formatter"
		   " returned error %d", mfu_hdr->program_number,
		   mfu_hdr->stream_id, ret);
	ret = -1;
	goto create_segment_ret;
    }

    calc_stats(
    	(int8_t*)encaps->name,
    	mfu_hdr->duration,
        mfu_hdr->program_number,
	mfu_hdr->stream_id,    	
	ts, 0);

    
    if(audio_index != -1) 
      amd->mdat_pos  = ts->aud_mdat_pos_add;

    /* setup the next state */
    if (encr_flag) {
	if (0) {
	    //ts->chunk_num % encr->key_rotation_interval 
	    FRUIT_FMT_INPUT_SET_STATE(fmt_input, FRUIT_FMT_ENCRYPT);
	} else {
	    FRUIT_FMT_INPUT_SET_STATE(fmt_input, FRUIT_FMT_AUTH);
	}
    } else {
	FRUIT_FMT_INPUT_SET_STATE(fmt_input, FRUIT_FMT_PL_UPDATE);
    }
create_segment_ret:
    if(video_index != -1) {
      if(vmd)
	free(vmd);
    }
    if(audio_index != -1) {
      if(amd)
	free(amd);
    }
    if(!endlist_mfu){
	validate_active_task_count(encaps,
				   fmt_input->active_task_cntr,
				   mfu_hdr->program_number,
				   mfu_hdr->stream_id);
    }

    mfu_contnr->del(mfu_contnr);

     if(ret < 0){
     	//return value for Error cases
     	 if(ret == -1){
	     stream->status = FRUIT_STREAM_ERROR;
	     FMTR_ERRORCALLBACK(2);
     	 }
	 decr_active_task_count(encaps,
				fmt_input->active_task_cntr);
	 if(fmt_input){
	   /*if(*fmt_input->active_task_cntr == 0 &&
	      (stream->status == FRUIT_STREAM_ERROR ||
	       stream->status == FRUIT_STREAM_ENDLIST_PUB)){
		free(fmt_input->active_task_cntr);
		fmt_input->active_task_cntr = NULL;
	      }*/
	     free(fmt_input);
	     fmt_input = NULL;
	 }
	ref_cont->release_ref_cont(ref_cont);
	return VPE_ERROR;		    
    }


    /* post a task for the next state */
    if(ret == 0){
    	//return for calling update_playlist from this module
	    //rtscheduleDelayedTask(0, (TaskFunc)mfubox_ts,(void*) ref_cont);
	taskHandler handler = (taskHandler)mfubox_ts;
	thread_pool_task_t* t_task = 
		newThreadPoolTask(handler, ref_cont, NULL);
	apple_fmtr_task_processor->add_work_item(apple_fmtr_task_processor, t_task); 
#ifdef APPLE_DEBUG   
	DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE, "Add Task: Session id: %d,"
		   " Stream id: %d MFU2TS formatter"
		   " Chunk number %d",
		   mfu_hdr->program_number,
		   mfu_hdr->stream_id,
		   fmt_input->mfu_seq_num);
#endif

    }
    if(ret == 1){
    	//return for rescheduling endlist tag addition
	//    rtscheduleDelayedTask(100000, (TaskFunc)mfubox_ts,(void*) ref_cont);
	taskHandler handler = (taskHandler)mfubox_ts;
	thread_pool_task_t* t_task = 
		newThreadPoolTask(handler, ref_cont, NULL);
	apple_fmtr_task_processor->add_work_item(apple_fmtr_task_processor, t_task);
#ifdef APPLE_DEBUG   
	DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE, "Add Task: Session id: %d,"
		   " Stream id: %d MFU2TS formatter"
		   " Chunk number %d",
		   mfu_hdr->program_number,
		   mfu_hdr->stream_id,
		   fmt_input->mfu_seq_num);
#endif

    }
    if(ret == 2){
    	//return after writing endlist tag
    	decr_active_task_count(encaps,
        	fmt_input->active_task_cntr);
	if(fmt_input){
	  if(*fmt_input->active_task_cntr == 0 &&
	     (stream->status == FRUIT_STREAM_ERROR ||
	      stream->status == FRUIT_STREAM_ENDLIST_PUB)){
	    free(fmt_input->active_task_cntr);
	    fmt_input->active_task_cntr = NULL;
	    /*
	    DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE, "Clearing the stats variable: Session id: %d,"
		       " Stream id: %d",
		       mfu_hdr->program_number,
		       mfu_hdr->stream_id);
	    */
	  }
	  free(fmt_input);
	  fmt_input = NULL;
	}
	ref_cont->release_ref_cont(ref_cont);
	// Delete KMS key store context : if exists
	pthread_mutex_lock(&encaps->lock);
	kms_client_key_store_t** kms_key_stores_tmp =
		encaps->fruit_formatter.kms_key_stores;
	if (kms_key_stores_tmp != NULL) {
	      uint32_t idx, cnt = 0;
	      kms_client_key_store_t* kms_key_store =
		  kms_key_stores_tmp[stream_num];
	      kms_key_resp_t *kr = NULL;
	      uint32_t key_pos_end = 0, key_pos_start = stream->seq_num;
		  
	      if (dvr_mode) {
		  key_pos_start = stream->total_num_chunks;
	      } 
	      if (kms_key_store->key_rot_count == 0)
		  key_pos_end = 0;
	      else {
		  key_pos_end =					\
		      stream->total_num_chunks + 
		      (kms_key_store->key_rot_count -
		       ((stream->total_num_chunks - 1) %
			kms_key_store->key_rot_count));
	      }
	      
	      DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE,
			 "deleting keys from %d to %d",
			 stream->seq_num,
			 stream->total_num_chunks + 2);
	      fruit_encr_remove_keys(key_pos_start,
				     key_pos_end,
				     kms_key_store);
	      
	      DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, "num of kms key"
			 " entries in the key store at EOS; %d\n",
			 kms_key_store->dbg_key_count);
		  encaps->kms_ctx_active_count--;

	      if (encaps->kms_ctx_active_count == 0) {
		apple_fmtr_release_encap_ctxt(program_num);
		encaps->fruit_formatter.kms_key_stores = NULL;
	      }
	}
	pthread_mutex_unlock(&encaps->lock);

    }

    return VPE_SUCCESS;
}


/**
 * MFU2TS handler for the FRUIT_FMT_PL_UPDATE state
 */
static int32_t mfu2ts_update_playlist(ref_count_mem_t *ref_cont)
{
    uint32_t chunk_num, min_chunk_num, dvr_mode;//, num_pub_strms_msn, min_ses_seqnum;//, num_streams_active;
    mfub_t *mfu_hdr;    
    ts_desc_t *ts;
    mfu_data_t *mfu_data = (mfu_data_t*)ref_cont->mem;
    fruit_fmt_input_t *fmt_input = (fruit_fmt_input_t *)mfu_data->fruit_fmtr_ctx;
    kms_client_key_store_t **kms_key_stores = NULL,
	*kms_stream_key_store = NULL;
    kms_key_resp_t *key_resp = NULL;
    int32_t err = 0;
    int32_t rv = 0;
    mfu_mfub_box_t *mfubbox;
    fruit_encap_t *encaps;
    fruit_stream_t *stream;

    /* initialization */
    mfubbox = parseMfuMfubBox(mfu_data->data);
    mfu_hdr = mfubbox->mfub;
    encaps = &fruit_ctx.encaps[mfu_hdr->program_number];
    stream = &encaps->stream[mfu_hdr->stream_id];
#ifdef APPLE_DEBUG
    {
	DBG_MFPLOG(encaps->name, SEVERE, MOD_MFPLIVE, "Encrypt to PL update reced: Session id: %d,"
		   " Stream id: %d MFU2TS formatter"
		   " Chunk number %d",
		   mfu_hdr->program_number,
		   mfu_hdr->stream_id,
		   fmt_input->mfu_seq_num);
    }
#endif

    validate_active_task_count(encaps,
			       fmt_input->active_task_cntr,
			       mfu_hdr->program_number,
			       mfu_hdr->stream_id);

    pthread_mutex_lock(&stream->lock);
    if ((stream->status == FRUIT_STREAM_ERROR) 
	    || ((stream->status & 0x8) == FRUIT_STREAM_RESYNC)){
    
	DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE,
		   "stream %d, got %d - Dropped",
		   mfu_hdr->stream_id,
		   fmt_input->mfu_seq_num + encaps->fruit_formatter.segment_start_idx);
	if(stream->status == FRUIT_STREAM_ERROR){
	    FMTR_ERRORCALLBACK(2);
	}
	pthread_mutex_unlock(&stream->lock);
	rv = -1;
	goto update_playlist_ret;
    }

    /* serialize playlist update */
    if (stream->total_num_chunks != 
	fmt_input->mfu_seq_num + encaps->fruit_formatter.segment_start_idx) {
	//	if(tot_num_resched_sess++%10000 == 0)
	if(stream->num_resched++%50000 == 0)
	    //	    assert(0);
	    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE,
		       "sess_id %d, stream %d, exp %d, got %d - Re-sched",
		       mfu_hdr->program_number,
		       mfu_hdr->stream_id,
		       stream->total_num_chunks,
		       fmt_input->mfu_seq_num + encaps->fruit_formatter.segment_start_idx);
	pthread_mutex_unlock(&stream->lock);
	mfubbox->del(mfubbox);
	return VPE_REDO;
    }
#ifdef APPLE_DEBUG
    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE,
	       "sess_id %d, stream id %d, current chunk number %d Process Now",
	       mfu_hdr->program_number,
	       mfu_hdr->stream_id,	       
	       fmt_input->mfu_seq_num);
#endif
    //    stream->num_resched = 0;
    chunk_num = stream->total_num_chunks;
    min_chunk_num = encaps->fruit_formatter.min_segment_child_plist;
    dvr_mode = encaps->fruit_formatter.dvr;
    ts = stream->ts;
    kms_key_stores = encaps->fruit_formatter.kms_key_stores;


    //2 changing encap status - acquire and release lock
    pthread_mutex_lock(&encaps->lock);

    if(publish_parent_playlist(encaps, stream, chunk_num, 
			       min_chunk_num,
			       encaps->fruit_formatter.min_segment_child_plist) == -1){
	FMTR_ERRORCALLBACK(2);				     
        stream->status = FRUIT_STREAM_ERROR;
	pthread_mutex_unlock(&encaps->lock);
	pthread_mutex_unlock(&stream->lock);
       goto update_playlist_ret;	
    }

    pthread_mutex_unlock(&encaps->lock);
    pthread_mutex_unlock(&stream->lock);

    if (kms_key_stores) {
	key_resp = &fmt_input->key_resp;
    }

    //2 changing encaps mfu seq number, acquire and release lock
    pthread_mutex_lock(&encaps->lock);
    if(fmt_input->mfu_seq_num == encaps->mfu_seq_num){ 

#if 0
	DBG_MFPLOG("MFU2TS", MSG, MOD_MFPLIVE,
		   "stream id %d, PubReq chunk %d, SessionChunk %d, NumStrms %d ProcessNow",
		   mfu_hdr->stream_id, fmt_input->mfu_seq_num,
		   encaps->mfu_seq_num,
		   encaps->num_streams_pub_msn);
#endif
	update_playlist_queue_t *updplqueue;
	updplqueue = &encaps->updplqueue[mfu_hdr->stream_id];

	    //update queue if the stream id is free
	if(updplqueue->slot_filled == 0){
	        encaps->num_streams_pub_msn++;
	    	updplqueue->program_num = mfu_hdr->program_number;
		updplqueue->stream_id = mfu_hdr->stream_id;
		updplqueue->duration = mfu_hdr->duration;
    		updplqueue->discontinuity_flag = fmt_input->fmt_refresh;
		if(fmt_input->fmt_refresh)
			fmt_input->fmt_refresh = 0;
		updplqueue->key_resp = NULL;
		if(kms_key_stores != NULL){
		    updplqueue->key_resp = createKmsKeyResponse(key_resp->key,
					key_resp->key_len, key_resp->key_uri,
		                        key_resp->is_new, fmt_input->mfu_seq_num);
		    //free(key_resp->key);
		    //free(key_resp->key_uri);
		}
		updplqueue->slot_filled = 1;	    	
	}

	//if condition to check playlist queue list is full
	if(encaps->num_streams_pub_msn == encaps->num_streams_active){
		if(update_session_child_playlist(encaps, mfu_hdr->program_number) == -1){
		  FMTR_ERRORCALLBACK(2);
		  stream->status = FRUIT_STREAM_ERROR;
		  pthread_mutex_unlock(&encaps->lock);
		  goto update_playlist_ret;
		}
	}
	pthread_mutex_unlock(&encaps->lock);
    }else{
#if 0
	DBG_MFPLOG("MFU2TS", MSG, MOD_MFPLIVE,
		"stream %d, SessionExp %d, got %d,  numstrms %d Re-sched",
		mfu_hdr->stream_id, 
		encaps->mfu_seq_num + encaps->fruit_formatter.segment_start_idx,
		fmt_input->mfu_seq_num + encaps->fruit_formatter.segment_start_idx,
		encaps->num_streams_pub_msn);	
#endif
	pthread_mutex_unlock(&encaps->lock);
	mfubbox->del(mfubbox);
	return VPE_REDO;
    }



 
update_playlist_ret:
    apple_fmtr_release_encap_ctxt(mfu_hdr->program_number);
    mfubbox->del(mfubbox);

    decr_active_task_count(encaps,
	    	fmt_input->active_task_cntr);
    if(*fmt_input->active_task_cntr == 0 && rv<0 && 
       (stream->status == FRUIT_STREAM_ERROR || 
	stream->status == FRUIT_STREAM_ENDLIST_PUB)){
      free(fmt_input->active_task_cntr);
      fmt_input->active_task_cntr = NULL;
    }
    /* cleanup */
     if (fmt_input) {
	 free(fmt_input);
     }
     ref_cont->release_ref_cont(ref_cont);  

    if(rv < 0){
       return VPE_ERROR;
     }else{
    	return VPE_SUCCESS;
     }
}


static void fruit_cleanup_enc_msg(enc_msg_t *em)
{
    if (em) {
	free(em);
    }

}

//! new interface for implementing streaming level HA in
//! apple formatter


void register_stream_failure(int32_t sess_id,
			     int32_t stream_id){
    
    int32_t i;
    fruit_encap_t *encaps = &fruit_ctx.encaps[sess_id];
    fruit_stream_t *stream = &fruit_ctx.encaps[sess_id].stream[stream_id];
    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, "sess[%d] stream[%d] register_stream_failure",
	       sess_id, stream_id);
    

    //acquire lock
    pthread_mutex_lock(&stream->lock);
    //set state as ERROR
    stream->status = FRUIT_STREAM_ERROR;
    //release lock
    pthread_mutex_unlock(&stream->lock);
    

    pthread_mutex_lock(&encaps->lock);
    if(encaps->mfu_seq_num){
	//reduce num active streams, reduce slot numbers
	//free up any exisiting deadlock
	//update the expected chunk number at session level
	encaps->num_streams_active--;
	
	int32_t strm_cnt = encaps->num_streams_active;
	encaps->status = FRUIT_ENCAP_PART_ACTIVE;
	for(i = 0; i < strm_cnt; i++){
	    //if(i != stream_id){
	    stream = &fruit_ctx.encaps[sess_id].stream[i];
	    if(stream->status != FRUIT_STREAM_ERROR){
		pthread_mutex_lock(&stream->lock);
		stream->status = FRUIT_STREAM_PART_ACTIVE;
		publish_parent_playlist(encaps, stream, 
					stream->total_num_chunks,
					encaps->fruit_formatter.min_segment_child_plist,
					(encaps->mfu_seq_num+encaps->fruit_formatter.segment_start_idx+1));
		pthread_mutex_unlock(&stream->lock);
	    }else{
		strm_cnt++;
	    }
	    //}
	}
	
	
	if(encaps->updplqueue[stream_id].slot_filled){
	    encaps->num_streams_pub_msn--;
	    encaps->updplqueue[stream_id].slot_filled = 0;
	}
	if(encaps->num_streams_pub_msn == encaps->num_streams_active){
	    update_session_child_playlist(encaps, sess_id);
	}
	stream = &fruit_ctx.encaps[sess_id].stream[stream_id];
	process_stream_endlist(sess_id, stream_id, stream->total_num_chunks);
	pthread_mutex_lock(&stream->lock);
	stream->status = FRUIT_STREAM_ENDLIST_PUB;
	pthread_mutex_unlock(&stream->lock);
	
    }else{
	encaps->num_streams_active--;    
    }
    pthread_mutex_unlock(&encaps->lock);
    
    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, "sess[%d] stream[%d] return register_stream_failure",
	    sess_id, stream_id);
    
    return;
}

void register_stream_dead(
	int32_t sess_id,
	int32_t stream_id){

    fruit_encap_t *encaps = &fruit_ctx.encaps[sess_id];
    fruit_stream_t *stream = &fruit_ctx.encaps[sess_id].stream[stream_id];

    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, "sess[%d] stream[%d] register_stream_dead",
	    sess_id, stream_id);

    //acquire lock
    //    pthread_mutex_lock(&stream->lock);

    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, "sess[%d] stream[%d] set status FRUIT_STREAM_ERROR",
	    sess_id, stream_id);
    if((stream->status == FRUIT_STREAM_ACTIVE) || (stream->status == FRUIT_STREAM_PART_ACTIVE)){    
    //set state as ERROR
    pthread_mutex_lock(&encaps->lock);
    encaps->num_streams_active--;
    pthread_mutex_unlock(&encaps->lock);
    pthread_mutex_lock(&stream->lock);
    stream->status = FRUIT_STREAM_ERROR;
    pthread_mutex_unlock(&stream->lock);
    process_stream_endlist(sess_id, stream_id, stream->total_num_chunks);
    stream->status = FRUIT_STREAM_ENDLIST_PUB;
    //    pthread_mutex_lock(&stream->lock);
    }

    //release lock
    //    pthread_mutex_unlock(&stream->lock);
    //process_stream_endlist(sess_id, stream_id, stream->total_num_chunks);
    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, "sess[%d] stream[%d] return register_stream_dead",
	    sess_id, stream_id);

    return;
}

void register_stream_resync(
	int32_t sess_id,
	int32_t stream_id,
	uint32_t accum_mfu_seq_num,
	uint32_t refreshed){

    fruit_encap_t *encaps = &fruit_ctx.encaps[sess_id];
    fruit_stream_t *stream;	

    if(!refreshed){
	    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, "sess[%d] stream[%d] register_stream_resync",
		    sess_id, stream_id);
	    uint32_t i;
	    uint32_t ret_mfu_seq_num;
	    
	    pthread_mutex_lock(&encaps->lock);
	    encaps->mfu_seq_num = accum_mfu_seq_num;
	    encaps->num_streams_pub_msn = 0; 
	    //set the expected session common seq number
	    ret_mfu_seq_num = encaps->mfu_seq_num;
	    pthread_mutex_unlock(&encaps->lock);

	    uint32_t num_strms = encaps->num_streams_active;
	    

	    for(i = 0; i < num_strms; i++){

		stream = &encaps->stream[i];
		if(stream->status != FRUIT_STREAM_ERROR){
			//acquire lock
			pthread_mutex_lock(&stream->lock);
			//change the expected stream chunkto 0
			stream->total_num_chunks = ret_mfu_seq_num + encaps->fruit_formatter.segment_start_idx;
			DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, \
				"sess[%d] stream[%d] expchunk [%d] set status FRUIT_STREAM_RESYNC",
				sess_id, i ,stream->total_num_chunks);
			//set state as ERROR
			stream->status |= FRUIT_STREAM_RESYNC;
			pthread_mutex_lock(&encaps->lock);
			encaps->updplqueue[i].slot_filled = 0;
			pthread_mutex_unlock(&encaps->lock);
			//release lock
			pthread_mutex_unlock(&stream->lock);
		}else{
			//if some intermediate strm show error, increase strm count
			//to look for strm at the end of list
			num_strms++;
		}

	    }	    
	    //DBG_MFPLOG("MFU2TS", MSG, MOD_MFPLIVE, "sess[%d] stream[%d] return register_stream_resync",
	    //sess_id, stream_id);
    }else{

	    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE, "sess[%d] stream[%d] refresh_stream_resync",
		    sess_id, stream_id);
	    stream = &encaps->stream[stream_id];
	    //acquire lock
	    pthread_mutex_lock(&stream->lock);
	    stream->status &= (~FRUIT_STREAM_RESYNC);
	    //release lock
	    pthread_mutex_unlock(&stream->lock);
    }	
    return;
}

uint32_t get_sessid_from_mfu(mfu_data_t *mfu_data){

    mfu_mfub_box_t* mfubbox = parseMfuMfubBox(mfu_data->data);
    uint32_t sess_id = mfubbox->mfub->program_number;
    mfubbox->del(mfubbox);	
    return (sess_id);	
}

void apple_fmtr_hold_encap_ctxt(uint32_t sess_id){
	
    fruit_encap_t *encaps = &fruit_ctx.encaps[sess_id];
    encaps->kms_ctx_del_track->hold_ref_cont(encaps->kms_ctx_del_track);
    return;
}

void apple_fmtr_release_encap_ctxt(uint32_t sess_id){

    fruit_encap_t *encaps = &fruit_ctx.encaps[sess_id];
    encaps->kms_ctx_del_track->release_ref_cont(encaps->kms_ctx_del_track);
    return;
}

/**
 * MFU to Apple TS formatting entry point - handles the formatter
 * state machine. there are 3 possible states. the transitions are
 * depicted below
 * 1. creating the Apple HLS compliant TS segement from an MFU. This
 * state is described by FRUIT_FMT_CONVERT. the segment is written to
 * a file if encryption is turned on and is stored as an in memory
 * stream if encryption is reequired. 
 * 2. encrypt the TS segment (in memory segment) by posting a AuthMgr
 * task. The AuthMgr returns back to the caller (the mfu2ts formatter
 * in this case)once all the encryption related work is done with an
 * in memory encrypted stream
 * 3. update the playlists (if the stream is encrypted, the stream is
 * written and then playlis it updated in this state
 */
int32_t mfubox_ts(ref_count_mem_t* ref_cont)
{
    mfu_data_t *mfu_data = (mfu_data_t*)ref_cont->mem;
    fruit_fmt_input_t *fmt_input =
		(fruit_fmt_input_t *)mfu_data->fruit_fmtr_ctx;
	mfu_mfub_box_t* mfubbox = NULL;

    int32_t rv = VPE_SUCCESS;

    switch (fmt_input->state) {
	case FRUIT_FMT_ENCRYPT:
	    fruit_create_enc_mgr_task(ref_cont);
	    break;
	case FRUIT_FMT_AUTH:
	    /* post AuthMgr task */
	    fruit_create_auth_mgr_task(ref_cont);
	    break;
	case FRUIT_FMT_CONVERT:
	    /* create TS segment from MFU - writes file if encryption is
	     * not required
	     */

		rv = mfu2ts_create_segment(ref_cont);
	    break;
	case FRUIT_FMT_PL_UPDATE:
	    /* updates the playlist */
	    rv = mfu2ts_update_playlist(ref_cont);
	    if (rv == VPE_REDO) {
		//rtscheduleDelayedTask(100000, (TaskFunc)mfubox_ts,
		//		      (void*) ref_cont); 
		taskHandler handler = (taskHandler)mfubox_ts;
		thread_pool_task_t* t_task = 
			newThreadPoolTask(handler, ref_cont, NULL);
		apple_fmtr_task_processor->add_work_item(apple_fmtr_task_processor, t_task); 
	    }
	    break;
    }
    return rv;
}


static int32_t fruit_set_auth_task_output(kms_info_t *ki,
					  kms_out_t *ko)
{
    //    ki->ko = (const kms_out_t *)ko;   
    //    ko->iv_len = ki->iv_len;
    return 0;
}

static nkn_task_id_t fruit_create_auth_mgr_task(ref_count_mem_t *ref_cont)
{
    nkn_task_id_t tid;
    mfu_data_t *mfu_data = (mfu_data_t*)ref_cont->mem;
    fruit_fmt_input_t *fmt_input = (fruit_fmt_input_t*)mfu_data->fruit_fmtr_ctx;

    kms_client_key_store_t* kms_key_store = NULL;
    mfub_t *mfu_hdr;
    mfu_mfub_box_t *mfubbox;

    mfubbox = parseMfuMfubBox(mfu_data->data);
    mfu_hdr = mfubbox->mfub;
    kms_key_store = fruit_ctx.encaps[mfu_hdr->program_number].fruit_formatter.kms_key_stores[mfu_hdr->stream_id];
    fruit_encap_t *encaps = &fruit_ctx.encaps[mfu_hdr->program_number];
    
    DBG_MFPLOG(encaps->name, MSG, MOD_MFPLIVE,
	       "creating auth mgr task for chunk num %d",
	       fmt_input->mfu_seq_num);
    fruit_init_auth_msg(VERIMATRIX, kms_key_store,
			fmt_input->mfu_seq_num + 1,
			&fmt_input->am);
    mfubbox->del(mfubbox);
    tid = nkn_task_create_new_task(
				   TASK_TYPE_AUTH_MANAGER,
				   TASK_TYPE_MFP_LIVE,
				   TASK_ACTION_INPUT,
				   0, /* NOTE: Put callee operation if any*/
				   (void*)(&fmt_input->am),
				   sizeof(auth_msg_t),
				   1);
    if (!tid) {
	return -1;
    }
    fmt_input->am.task_id = tid;
    nkn_task_set_private(TASK_TYPE_MFP_LIVE, tid,
			 (void*)(ref_cont));
    nkn_task_set_private(TASK_TYPE_AUTH_MANAGER, tid,
			 (void *)(&fmt_input->key_resp));
    nkn_task_set_state(tid, TASK_STATE_RUNNABLE);

    return tid;
}

static int32_t fruit_init_auth_msg(uint8_t authtype,
				   void *authdata, uint32_t seq_num,
				   auth_msg_t *out)
{
    out->authtype = authtype;
    out->errcode = 0;
    out->authdata = authdata;
    out->seq_num = seq_num;

    return 0;
}

static nkn_task_id_t fruit_create_enc_mgr_task(ref_count_mem_t *ref_cont)
{
    nkn_task_id_t tid;
    mfu_data_t *mfu_data = (mfu_data_t*)ref_cont->mem;
    fruit_fmt_input_t *fmt_input =
		(fruit_fmt_input_t *)mfu_data->fruit_fmtr_ctx;

    fruit_init_enc_msg(fmt_input, &fmt_input->em);
    tid = nkn_task_create_new_task(
				   TASK_TYPE_ENCRYPTION_MANAGER,
				   TASK_TYPE_MFP_LIVE,
				   TASK_ACTION_INPUT,
				   0, /* NOTE: Put callee operation if any*/
				   &fmt_input->em,
				   sizeof(enc_msg_t),
				   1);
    if (!tid) {
	return -1;
    }

    nkn_task_set_private(TASK_TYPE_MFP_LIVE, tid, (void*)ref_cont);
    nkn_task_set_state(tid, TASK_STATE_RUNNABLE);

    return tid;
}

static int32_t fruit_init_enc_msg(fruit_fmt_input_t *fmt_input,
				  enc_msg_t *em_out)
{
    kms_key_resp_t *key_resp = &fmt_input->key_resp;
    //int fd;
    int32_t err = 0;
    enc_msg_t *em = NULL;

    em = em_out;

    /* setup the basic fields */
    em->magic = ENCODING_MGR_MAGIC;
    em->in_enc_data = bio_get_buf(fmt_input->ts_stream);
    /* allocated size will not be the same as the written size since we
     * accomodate for overheads apporximately 
     */
    em->in_len = bio_get_pos(fmt_input->ts_stream); 
    memcpy(em->key, key_resp->key, 16);
    em->key_len = key_resp->key_len; 
    strncpy((char*)&(em->key_uri[0]), (char*)key_resp->key_uri, 512);
    em->enctype = AES_128_CBC;
    em->out_len = em->in_len + 1024;
    em->out_enc_data = NULL;
    em->out_enc_data = nkn_malloc_type(em->out_len, mod_vpe_enc_data);

    uint32_t tmp = fmt_input->mfu_seq_num + 1;
    tmp = nkn_vpe_swap_uint32(tmp);
    memset((void*)em->iv, 0, key_resp->key_len);
    memcpy((uint8_t*)em->iv + (key_resp->key_len-4), &tmp, 4);
    em->iv_len = key_resp->key_len; 

    return err;
}

int32_t update_child_end_list(char *child_name)
{
    FILE  *f_m3u8_file=NULL; 
    int32_t err = 0;
    f_m3u8_file = fopen(child_name, "ab");
    if(!f_m3u8_file)
	return -E_MFP_NO_SPACE;
    err = fprintf(f_m3u8_file, "#EXT-X-ENDLIST\n");
    if(err < 0) {
      fclose(f_m3u8_file);
      return -E_VPE_WRITE_ERR;
    }
    fclose(f_m3u8_file);
    return 1;
}


int32_t update_master_end_list(fruit_encap_t* encap)
{
    char parent_m3u8_name[500];
    FILE  *f_m3u8_file=NULL;
    char* mfu_out_fruit_path =NULL;
    uint8_t is_master_end_list = 1;
    int32_t i, err =0;
    mfu_out_fruit_path =
	encap->fruit_formatter.mfu_out_fruit_path;
    for(i=0;i<MAX_FRUIT_STREAMS;i++) {
	if(encap->stream[i].status == FRUIT_STREAM_ACTIVE) {
	    is_master_end_list &= encap->stream[i].is_end_list;
	}
    }
    if (is_master_end_list == 1) {
	snprintf(parent_m3u8_name, strlen(mfu_out_fruit_path) +
		 strlen(encap->m3u8_file_name)+2,"%s/%s",
		 mfu_out_fruit_path,
		 encap->m3u8_file_name);
	f_m3u8_file = fopen(parent_m3u8_name, "ab");
	if (!f_m3u8_file) {
	    return -E_MFP_NO_SPACE;
	}
	err = fprintf(f_m3u8_file, "#EXT-X-ENDLIST\n");
	if(err < 0) {
	  fclose(f_m3u8_file);
	  return -E_VPE_WRITE_ERR;
	}
	fclose(f_m3u8_file);
    }
    return 1;
}


static void form_fruit_chunk_name(char * fname, fruit_fmt_t fmt, ts_desc_t *ts){

    char fmt_string[100];
    uint32_t precision_max;
    uint32_t mod_chunk_num;

    precision_max = pow(10,fmt.segment_idx_precision);
    mod_chunk_num = ts->chunk_num;
    //if (ts->chunk_num  >= precision_max){
    mod_chunk_num =  ts->chunk_num % (precision_max - fmt.segment_start_idx);
    mod_chunk_num += fmt.segment_start_idx;
    //}    
    
    //char chunk_name[200];
    //if(ts->chunk_num >= pow(10,fmt.segment_idx_precision))
    //	ts->chunk_num = fmt.segment_start_idx;

    snprintf(fmt_string, 100, "%%s_%%0%dd.ts",fmt.segment_idx_precision);
    snprintf(fname,256,fmt_string, ts->out_name,mod_chunk_num);
    
    return;
}

/*****      Playlist Functions    ******/
static int32_t form_master_playlist(char* out_path, char* video_name){

    FILE *f_parent_m3u8_file;
    char parent_m3u8_name[500];
    int32_t err = 0;
    f_parent_m3u8_file = NULL;
    snprintf(parent_m3u8_name, strlen(out_path) + strlen("temp_") +
	     strlen(video_name)+2, "%s/temp_%s",out_path, video_name);
    f_parent_m3u8_file = fopen(parent_m3u8_name, "wb");
    if (!f_parent_m3u8_file) {
	return -E_MFP_NO_SPACE;
    }
    err = fprintf(f_parent_m3u8_file, "#EXTM3U\n");
    //err = -1;
    if(err < 0) {
      fclose(f_parent_m3u8_file);
      return -E_VPE_WRITE_ERR;
    }
    fclose(f_parent_m3u8_file);
    return 1;
}

static int32_t rename_master_playlist(char* out_path, char* video_name){
	int32_t len;
	FILE *f_parent_m3u8_file;//, *f_m3u8_file=NULL;
	char parent_m3u8_name[500];

	//check whether temporary master playlist exists
	f_parent_m3u8_file = NULL;
	snprintf(parent_m3u8_name, strlen(out_path) + strlen("temp_") +
		 strlen(video_name)+2, "%s/temp_%s",out_path, video_name);
	f_parent_m3u8_file = fopen(parent_m3u8_name, "ab");
	if (!f_parent_m3u8_file) {
	    return -1;
	}
 	fclose(f_parent_m3u8_file);	
	
	char des[300] = {'\0'};
	char src[300] = {'\0'};
	len =snprintf(NULL,0,"%s/temp_%s",out_path, video_name);
	snprintf(src,len+1,"%s/temp_%s",out_path, video_name);
	len =snprintf(NULL,0,"%s/%s",out_path, video_name);
	snprintf(des,len+1,"%s/%s",out_path, video_name);
	rename(src,des);
	return 0;
}

static int32_t form_child_playlist(char* out_path, char* master_name,
				char* child_name, uint32_t bitrate, char *uri_prefix){

    FILE *f_parent_m3u8_file;//, *f_m3u8_file=NULL;
    char m3u8_name[500];
    char parent_m3u8_name[500];
    int32_t err = 0;

    f_parent_m3u8_file = NULL;
    snprintf(parent_m3u8_name, strlen(out_path) + strlen("temp_") +
	     strlen(master_name)+2,"%s/temp_%s",out_path, master_name);
    snprintf(m3u8_name, strlen(out_path) + strlen(child_name)+2,
	     "%s/%s",out_path, child_name);
    /*Augment Master Playlist*/
    f_parent_m3u8_file = fopen(parent_m3u8_name, "ab");
    if (!f_parent_m3u8_file) {
	return -E_MFP_NO_SPACE;
    }
    err = fprintf(f_parent_m3u8_file,
		  "#EXT-X-STREAM-INF:PROGRAM-ID=%d,BANDWIDTH=%d\n",
		  1,bitrate * 1000); 
    //err = -1;
    if(err < 0) {
      fclose(f_parent_m3u8_file);
      return -E_VPE_WRITE_ERR;
    }
    err = fprintf(f_parent_m3u8_file, "%s\n",  child_name );
    if(err < 0) {
      fclose(f_parent_m3u8_file);
      return -E_VPE_WRITE_ERR;
    }
    /*Create Child Playlist File*/

    /*Close both files*/
    fclose(f_parent_m3u8_file);
    return 1;
}





#define FRUIT_PL_OVERLAP 1
//#define ROLL_OVER_INTERVAL 5
static int32_t update_stream_child_playlist(char* child_name, char* uri_prefix,
				  uint32_t duration,
				  char* out_path, 
				  char* out_name,
				  uint32_t chunk_num,
				  fruit_fmt_t fmt,
				  uint32_t *seq_num,
				  uint32_t* tot_chnk_num,
		                  kms_client_key_store_t *key_store,
				  kms_key_resp_t *key_resp
#ifdef FRUIT_DISCONT
                                     ,int8_t disc_flag
#endif
                                     )
   
{
    char m3u8_name[500];//, key_name[500];
    char fmt_string[100] = {'\0'};
    FILE  *f_m3u8_file=NULL;
    char uri[4096] = {'\0'};
    int32_t len=0;
    char chunk_name[4096] = {'\0'};
    uint32_t precision_max, trigger_copy = 0;
    uint32_t mod_chunk_num, mod_chunk_num1;
    uint8_t file_start_flag = 0;
    int32_t err = 0;
    precision_max = pow(10,fmt.segment_idx_precision);
    snprintf(fmt_string, 100, "%%s_%%0%dd.ts",fmt.segment_idx_precision);
    if(fmt.dvr){
	/* 3 cases possible here
	 * a. the chunk count is lesser than min seg in child playlist
	 *    where we simple write to a temporary file
	 * b. the chunk count is equal to the min seg in child
	 *    playlist where we rename the temp file to its original
	 *    name
	 * c. greater than min seg in child playlist where we append
	 *    to the playlist
	 */
	mod_chunk_num = chunk_num;
	mod_chunk_num =  chunk_num % (precision_max - fmt.segment_start_idx);
	mod_chunk_num += fmt.segment_start_idx;
	if (mod_chunk_num <= fmt.min_segment_child_plist) {
	    /* if the chunk num is lesser than min segs in playlist
	     * then create a temp file */
	    snprintf(m3u8_name, strlen(out_path) +
		     strlen(child_name)+2+strlen("temp_"),"%s/temp_%s" ,out_path,
		     child_name);
	    strncpy(chunk_name,child_name,strlen(child_name)-strlen(".m3u8"));
	    f_m3u8_file = fopen(m3u8_name, "ab");
	    if (!f_m3u8_file) {
		return -E_MFP_NO_SPACE;
	    }
	    if (mod_chunk_num == fmt.min_segment_child_plist) 
		trigger_copy = 1;
	} else {
	    /* append to the child playlist */
	    snprintf(m3u8_name, strlen(out_path) +
		     strlen(child_name)+2,"%s/%s" ,out_path,
		     child_name);
	    f_m3u8_file = fopen(m3u8_name, "ab");
	    if (!f_m3u8_file) {
		return -E_MFP_NO_SPACE;
	    }

	}

	/* if its the segment start index then write the target
	   duration tag
	*/
	if (mod_chunk_num == fmt.segment_start_idx) {
		file_start_flag = 1;
	    err = fprintf(f_m3u8_file, M3U8_PREAMBLE, duration, *seq_num);
	    if(err < 0) {
	      fclose(f_m3u8_file);
	      return -E_VPE_WRITE_ERR;
	    }
	}
	strncpy(chunk_name,child_name,strlen(child_name)-strlen(".m3u8"));
	snprintf(uri,256,fmt_string, chunk_name, mod_chunk_num);
#ifdef FRUIT_DISCONT
        if(disc_flag){
            err = fprintf(f_m3u8_file, "#EXT-X-DISCONTINUITY\n");
            DBG_MFPLOG ("MFU2TS", MSG, MOD_MFPLIVE,
                            "Insert Discontinuity: tot chunk num %d, modified chunk"
			" num %d, chunk num %d, seq num %d",
                        *tot_chnk_num, chunk_num, mod_chunk_num, *seq_num);
	    if(err < 0) {
              fclose(f_m3u8_file);
              return -E_VPE_WRITE_ERR;
            }
        }
#endif
	if ((key_resp)) {
	    if ((key_resp->is_new == 1) || (file_start_flag == 1)) {
		err = fruit_playlist_update_encr_line(f_m3u8_file,
						      chunk_num, key_resp);
		if(err < 0) {
		  DBG_MFPLOG ("MFU2TS", MSG, MOD_MFPLIVE,
			      "error in fruit_playlist_update %d",
			     err);
		  fclose(f_m3u8_file);
		  return err;
		}
	    }
	    key_store->del_kms_key(key_store, chunk_num + fmt.segment_start_idx);
	    //deleteKmsKeyResponse(key_resp);
	}
	
	err = fprintf(f_m3u8_file, "#EXTINF:%d,\n%s\n", duration, uri);
	if(err < 0) {
	  fclose(f_m3u8_file);
	  return -E_VPE_WRITE_ERR;
	}
	fclose(f_m3u8_file);
	*tot_chnk_num+=1;	
	if(trigger_copy)
	    goto copy_tmp;
    } else {
	/*Check if temp file needs to be called */
	/*Handle the First playlist case*/
	*tot_chnk_num+=1;
	mod_chunk_num = chunk_num;

	mod_chunk_num =  chunk_num % (precision_max - fmt.segment_start_idx);
	mod_chunk_num += fmt.segment_start_idx;

	if((mod_chunk_num <=  fmt.min_segment_child_plist)){
	    snprintf(m3u8_name, strlen(out_path) + strlen(child_name)+2+strlen("temp_"),"%s/temp_%s" ,out_path, child_name);
	    if(mod_chunk_num ==  fmt.segment_start_idx){
		f_m3u8_file = fopen(m3u8_name, "wb");
		if (!f_m3u8_file) {
		    return -E_MFP_NO_SPACE;
		}
		file_start_flag = 1;
		err = fprintf(f_m3u8_file, "#EXTM3U\n#EXT-X-VERSION:2\n#EXT-X-TARGETDURATION:%d\n#EXT-X-MEDIA-SEQUENCE:%d\n",duration,*seq_num);
		if(err < 0) {
		  fclose(f_m3u8_file);
		  return -E_VPE_WRITE_ERR;
		}
	    } else {
		f_m3u8_file = fopen(m3u8_name, "ab");
		if (!f_m3u8_file) {
		    return -E_MFP_NO_SPACE;
		}
	    }
	    
	    strncpy(chunk_name,child_name,strlen(child_name)-strlen(".m3u8"));
#ifdef FRUIT_DISCONT
            if(disc_flag){
                err = fprintf(f_m3u8_file, "#EXT-X-DISCONTINUITY\n");
                DBG_MFPLOG ("MFU2TS", MSG, MOD_MFPLIVE,
                            "Insert Discontinuity: tot chunk num %d, modified chunk"
			    " num %d, chunk num %d, seq num %d",
                            *tot_chnk_num, chunk_num, mod_chunk_num, *seq_num);
		if(err < 0) {
		  fclose(f_m3u8_file);
		  return -E_VPE_WRITE_ERR;
		}
            }
#endif

	    snprintf(uri,256,fmt_string, chunk_name, mod_chunk_num);
	    if ((key_resp) && ((key_resp->is_new == 1) || (file_start_flag == 1))) {
		err = fruit_playlist_update_encr_line(f_m3u8_file,
						      mod_chunk_num, key_resp);
		if(err < 0) {
                  DBG_MFPLOG ("MFU2TS", MSG, MOD_MFPLIVE,
			      "error in fruit_playlist_update %d",
			      err);
		  fclose(f_m3u8_file);
                  return err;
                }

	    }
	    err = fprintf(f_m3u8_file, "#EXTINF:%d,\n%s\n", duration, uri);
	    if(err < 0) {
              fclose(f_m3u8_file);
              return -E_VPE_WRITE_ERR;
            }
	    if(mod_chunk_num ==  fmt.min_segment_child_plist)
		trigger_copy = 1;
	    fclose(f_m3u8_file);
	}

	/*When to trigger a copy*/
	if((*tot_chnk_num >
	fmt.segment_rollover_interval/*ROLL_OVER_INTERVAL*/) &&	(mod_chunk_num%FRUIT_PL_OVERLAP == 0)) {
	    uint32_t t,i;
	    int32_t tmp_seq_num = 0;
	    kms_key_resp_t *kr = NULL;

	    t = *tot_chnk_num;
	    snprintf(m3u8_name, strlen(out_path) + strlen(child_name)+2+strlen("temp_"),"%s/temp_%s" ,out_path, child_name);
	    f_m3u8_file = fopen(m3u8_name, "wb");
	    if (!f_m3u8_file) {
		return -E_MFP_NO_SPACE;
	    }
	    *seq_num+=FRUIT_PL_OVERLAP;
	    err = fprintf(f_m3u8_file, "#EXTM3U\n#EXT-X-VERSION:2\n#EXT-X-TARGETDURATION:%d\n#EXT-X-MEDIA-SEQUENCE:%d\n",duration,*seq_num);//chunk_num-ROLL_OVER_INTERVAL+1);//*seq_num);
	    if(err < 0) {
              fclose(f_m3u8_file);
              return -E_VPE_WRITE_ERR;
            }

	    for(i = 0 ; i < fmt.segment_rollover_interval - fmt.segment_start_idx ; i++){
		tmp_seq_num = (*seq_num) + i;
		strncpy(chunk_name,child_name,strlen(child_name)-strlen(".m3u8"));
		mod_chunk_num1 = t-fmt.segment_rollover_interval+i+1;
		mod_chunk_num1 = (mod_chunk_num1 - 1)%(precision_max - fmt.segment_start_idx);
		mod_chunk_num1 += fmt.segment_start_idx;
		snprintf(uri,256,fmt_string, chunk_name,mod_chunk_num1);//chunk_num-fmt.segment_rollover_interval+i+1);//t-ROLL_OVER_INTERVAL+i);
#ifdef FRUIT_DISCONT
                if(disc_flag) {
		  err = fprintf(f_m3u8_file, "#EXT-X-DISCONTINUITY\n");
		  if(err < 0) {
		    fclose(f_m3u8_file);
		    return -E_VPE_WRITE_ERR;
		  }
		}
#endif
#ifdef PRINT_MSG
		printf("Present Chunk Num = %d Calc Chunknum = %d\n",chunk_num,mod_chunk_num1);
#endif
		/* retrive the key from the key store if
		 * KMS/encryption is turned on
		 */
		if (key_resp)
		    kr = key_store->get_kms_key(key_store, tmp_seq_num);
#ifdef PRINT_MSG	
		if (kr) 
		    printf("Child Playlist: key no %d, key resp addr"
			   " %p\n", tmp_seq_num, kr);
#endif

		/**
		 * add encryption specific lines in the playlist
		 * 1. add only when there is a new key
		 * 2. assuming that the IV corresponds to the media sequence
		 *    num
		 */
		if (((kr) && (kr->is_new == 1)) || ((kr) && (i == 0))) {
		    err = fruit_playlist_update_encr_line(f_m3u8_file,
						    mod_chunk_num, kr);
		    if(err < 0) {
		      DBG_MFPLOG ("MFU2TS", MSG, MOD_MFPLIVE,
				  "error in fruit_playlist_update %d",
				  err);
		      fclose(f_m3u8_file);
		      return err;
		    }
		}
		err = fprintf(f_m3u8_file, "#EXTINF:%d,\n%s\n", duration, uri);
		if(err < 0) {
		  fclose(f_m3u8_file);
		  return -E_VPE_WRITE_ERR;
		}
	    }

	    /**
	     * remove the keys that have rolled out of the current
	     * segment roll over interval
	     */
	    if (key_resp) {
		fruit_encr_remove_keys(*seq_num - FRUIT_PL_OVERLAP, *seq_num,
				       key_store);
	    }
	    
	    trigger_copy = 1;
	    fclose(f_m3u8_file);
	}


	if(*tot_chnk_num <= fmt.segment_rollover_interval && *tot_chnk_num > fmt.min_segment_child_plist){
	    snprintf(m3u8_name, strlen(out_path) + strlen(child_name)+2,"%s/%s" ,out_path, child_name);
	    f_m3u8_file = fopen(m3u8_name, "ab");
	    if (!f_m3u8_file) {
		return -E_MFP_NO_SPACE;
	    }
	    strncpy(chunk_name,child_name,strlen(child_name)-strlen(".m3u8"));
	    snprintf(uri,256,fmt_string, chunk_name,mod_chunk_num);
#ifdef FRUIT_DISCONT
            if(disc_flag){
                err = fprintf(f_m3u8_file, "#EXT-X-DISCONTINUITY\n");
                DBG_MFPLOG ("MFU2TS", MSG, MOD_MFPLIVE,
                            "Insert Discontinuity: tot chunk num %d, modified chunk"
			    " num %d, chunk num %d, seq num %d",
                            *tot_chnk_num, chunk_num, mod_chunk_num, *seq_num);
		if(err < 0) {
		  fclose(f_m3u8_file);
		  return -E_VPE_WRITE_ERR;
		}

            }
#endif
	    if ((key_resp) && (key_resp->is_new == 1)) {
		err = fprintf(f_m3u8_file, "#EXT-X-KEY:METHOD=AES-128,URI=\"%s\"\n",
			      key_resp->key_uri);
		if(err < 0) {
		  fclose(f_m3u8_file);
		  return -E_VPE_WRITE_ERR;
		}
		uint32_t tmp = mod_chunk_num; 
		tmp = nkn_vpe_swap_uint32(tmp);
		char key_iv[key_resp->key_len];
		memset((void*)&key_iv[0], 0, key_resp->key_len);
		memcpy((uint8_t*)&key_iv[0] + (key_resp->key_len-4), &tmp, 4);
	    }
	    err = fprintf(f_m3u8_file, "#EXTINF:%d,\n%s\n", duration, uri);
	    if(err < 0) {
              fclose(f_m3u8_file);
              return -E_VPE_WRITE_ERR;
            }
	    fclose(f_m3u8_file);
	}

    copy_tmp:
	/*DBG_MFPLOG ("MFU2TS", MSG, MOD_MFPLIVE,
		    "tot chunk num %d, modified chunk num %d, chunk num %d, seq num %d",
		    *tot_chnk_num, chunk_num, mod_chunk_num, *seq_num);*/
	if(trigger_copy){//chunk_num ==  fmt.segment_start_idx + fmt.min_segment_child_plist- 
	    char des[300] = {'\0'};
	    char src[300] = {'\0'};
	    len =snprintf(NULL,0,"%s/temp_%s",out_path, child_name);//,out_path, child_name 
	    snprintf(src,len+1,"%s/temp_%s",out_path, child_name);
	    len =snprintf(NULL,0,"%s/%s",out_path, child_name);//,out_path, child_name 
	    snprintf(des,len+1,"%s/%s",out_path, child_name);
	    rename(src,des);
#ifdef TIME_DEBUG
	    struct timeval result;
	    gettimeofday(&currPktTime, NULL);
	    timeval_subtract(&currPktTime, &lastPktTime, &result);
	    memcpy(&lastPktTime, &currPktTime, sizeof(struct timeval));
	    printf("Time difference is = %ld\n",(uint64_t)(result.tv_sec));
#endif

	}
    }

	if ((key_resp))
		deleteKmsKeyResponse(key_resp);
    return 0;
}


/********************* Handle "Fruit" Strutures ******************************/

/**
 * remove the keys outside of the playlist update range
 * if K is the playlist update rate and segment rollover
 * interval is R and the current sequence number is 'seq_num',
 * then keys in the key store from 'seq_num'-K to I are deleted
 */
static inline int32_t fruit_encr_remove_keys(uint32_t start_key_seq_num, 
					     uint32_t end_key_seq_num,	      			     
					     kms_client_key_store_t *key_store)
{
    uint32_t i = 0;
    kms_key_resp_t *kr = NULL;

    if (!key_store) {
	return -E_MFP_FRUIT_INVALID_ARG;
    }

    for (i = start_key_seq_num; i < end_key_seq_num; i++) {
	kr = key_store->get_kms_key(key_store, i);
	if (kr) {
#if 0
	    printf("remove key: key num %d, key resp addr"
		   " %p\n", i, kr);
#endif
	    key_store->del_kms_key(key_store, i);
	} else {
#if 0
	    printf("remove key: key num %d, key resp addr"
		   " %p\n", i, NULL);
#endif
	}
    }
    
    return 0;
}

static inline int32_t fruit_playlist_update_encr_line(FILE *f_m3u8_file,
						      uint32_t mod_chunk_num,
						      kms_key_resp_t* kr)

{
    uint32_t tmp;
    char *key_iv = NULL;
    int32_t err = 0;
    err = fprintf(f_m3u8_file, "#EXT-X-KEY:METHOD=AES-128,URI=\"%s\"\n",
	    kr->key_uri);
    if(err < 0) {
      fclose(f_m3u8_file);
      return -E_VPE_WRITE_ERR;
    }
    tmp = mod_chunk_num; 
    tmp = nkn_vpe_swap_uint32(tmp);

    key_iv = (char *)malloc(kr->key_len);
    if (!key_iv) {
	return -E_MFP_NO_MEM;
    }

    memset((void*)&key_iv[0], 0, kr->key_len);
    memcpy((uint8_t*)&key_iv[0] + (kr->key_len-4),
	   &tmp, 4);
    
    free(key_iv);
    return 0;
}



static void set_fruit_encaps(fruit_encap_t* encap, mfub_t* mfu_hdr,
			     char* del_url)
{
    fruit_get_video_name(del_url, encap->m3u8_file_name);

    /* strip the uri prefix ; this is an inline operation and the
     * mfu_uri_fruit_prefix is modified inline
     */
    fruit_strip_uri_prefix(encap);
    strcat(encap->m3u8_file_name,".m3u8");
    encap->status = FRUIT_ENCAP_PART_ACTIVE;
}

static void fruit_strip_uri_prefix(fruit_encap_t *encap)
{
    char *p = NULL,
	*src = encap->fruit_formatter.mfu_uri_fruit_prefix;
    p = strstr(src, "http://");
    if (!p) {
	return;
    }

    p = strrchr(src, '/');
    if (*(p+1) == '\0') {
	return;
    }

    /* never have the trailing slash */
    *(p) = '\0';
    return;
}

static void set_fruit_stream(fruit_stream_t* stream,mfub_t* mfu_hdr,char* out_path,
			     fruit_fmt_t fmt)
{
    int32_t len;
    char fname_wo_ext[500]={'\0'};
    len = snprintf(NULL, 0, "Channel_%d_%d.m3u8", mfu_hdr->program_number, mfu_hdr->stream_id);
    snprintf(stream->m3u8_file_name, len+1, "Channel_%d_%d.m3u8", mfu_hdr->program_number, mfu_hdr->stream_id);
    stream->total_bit_rate = (1400*1000*(mfu_hdr->video_rate + mfu_hdr->audio_rate))/1000;
    stream->status = FRUIT_STREAM_PART_ACTIVE;
    strcat(fname_wo_ext,out_path);
    strncat(fname_wo_ext,stream->m3u8_file_name,strlen(stream->m3u8_file_name)-5);
    stream->ts = mfu2ts_desc_init(fname_wo_ext,mfu_hdr);
    mfu2ts_init(stream->ts);
    if(fmt.dvr){
	stream->ts->chunk_num = 1;
    } else {
	stream->ts->chunk_num = fmt.segment_start_idx;
	stream->seq_num = fmt.segment_start_idx;
    }
}

static void
cleanup_mfu2ts_desc(ts_desc_t *ts)
{
    if(ts) {
	if(ts->out_name)
	    free(ts->out_name);
	free(ts);
    }


}

static ts_desc_t* 
mfu2ts_desc_init(char *filename, mfub_t* mfu_hdr)
{
    uint32_t i;
    ts_desc_t* ts;
    ts = (ts_desc_t*)nkn_calloc_type(1, sizeof(ts_desc_t),
				     mod_vpe_ts_desc_t);
    ts->num_streams = 0;
    ts->chunk_size = 0;
    ts->avg_bitrate = 0;
    ts->max_bitrate = 0;
    ts->num_aud_streams = 0;
    ts->out_name = (char*)nkn_malloc_type(500*sizeof(char),
					  mod_vpe_file_name_buffer);
    strcpy(ts->out_name,filename);
    ts->chunk_time = mfu_hdr->duration;
    if(mfu_hdr->offset_adat){
	ts->num_aud_streams = 1;
	ts->num_streams++;
    }
    if(mfu_hdr->offset_vdat){
	ts->num_vid_streams = 1;
	ts->num_streams++;
    }

    ts->Is_audio = 1;
    for(i=0;i< ts->num_aud_streams;i++){
	ts->stream_aud[i].stream_type = TS_AAC_ADTS_ST;
    }
    for(i=0;i< ts->num_vid_streams;i++){
	ts->stream_vid[i].stream_type = TS_H264_ST;
    }
    //    ts->stream_aud[0].fout = fopen("first_audio.txt","wb");
    //    ts->stream_vid[0].fout = fopen("first_video.txt","wb");

    return ts;
}



#if 0
void init_fruit_structures(mobile_parm_t);
void init_fruit_structures(mobile_parm_t parms){
    int32_t i;
    fruit_encap_t en;
    fruit_stream_t st;
    if(!fruit_ctx.num_encaps_active){
	st.status=FRUIT_STREAM_INACTIVE;
	st.stream_id = 0;
	st.chunk_num = 0;
	st.ts = NULL;
	st.total_bit_rate = 0;
	en.num_streams_active=0;
	en.status =FRUIT_ENCAP_INACTIVE;
	en.num_streams_active = 0;
	for(i=0;i<MAX_FRUIT_STREAMS;i++)
	    memcpy(&en.stream[i],&st,sizeof(fruit_stream_t));

	for(i=0;i<MAX_FRUIT_CHNLS;i++)
	    memcpy(&fruit_ctx.encaps[i],&en,sizeof(fruit_encap_t));
    }



    /*Fill the required formatter*/
    i =  fruit_ctx.num_encaps_active;
    fruit_ctx.encaps[i].segment_start_idx = parms.segment_start_idx;
    fruit_ctx.encaps[i].min_segment_child_plist = parms.min_segment_child_plist;
    fruit_ctx.encaps[i].segment_idx_precision = parms.segment_idx_precision;
    fruit_ctx.encaps[i].segment_rollover_interval = parms.segment_rollover_interval;
    fruit_ctx.encaps[i].dvr = parms.dvr;
    strcpy(fruit_ctx.encaps[i].mfu_out_fruit_path, parms.store_url);
    strcpy(fruit_ctx.encaps[i].mfu_uri_fruit_prefix,parms.delivery_url);
    fruit_ctx.num_encaps_active++;
}
#endif


int32_t activate_fruit_encap(
	fruit_fmt_t fmt, 
	int32_t encap_num,
	uint32_t num_streams_per_encaps)
{
    int32_t i;
    //DBG_MFPLOG("MFU2TS", MSG, MOD_MFPLIVE, "Assigned Fruit Ctx %d", encap_num);
    fruit_encap_t *encaps;
    fruit_stream_t st;
    st.status=FRUIT_STREAM_INACTIVE;
    st.stream_id = 0;
    st.chunk_num = 0;
    st.ts = NULL;
    st.total_bit_rate = 0;
    st.seq_num = 1;
    st.total_num_chunks = fmt.segment_start_idx;
    st.is_end_list = 0;

    encaps = &fruit_ctx.encaps[encap_num];
    encaps->num_streams_active = num_streams_per_encaps;
    encaps->status =FRUIT_ENCAP_INACTIVE;
    encaps->is_end_list = 0;
    encaps->mfu_seq_num = 0;
    encaps->num_streams_pub_msn = 0;
    encaps->mfu_seq_num = 0;
    encaps->task_build_warn_limit = 
    	(glob_apple_fmtr_num_task_low_threshold)/fmt.key_frame_interval;
    encaps->task_build_high_limit = 
    	(glob_apple_fmtr_num_task_high_threshold)/fmt.key_frame_interval;
    pthread_mutex_init(&encaps->lock, NULL);
    pthread_mutex_init(&encaps->error_callback_lock, NULL);

    for(i=0;i<MAX_FRUIT_STREAMS;i++) {
	memcpy(&encaps->stream[i],&st,sizeof(fruit_stream_t));
	pthread_mutex_init(&encaps->stream[i].lock, NULL);
	encaps->updplqueue[i].slot_filled = 0;
    }
   
    memcpy(&encaps->fruit_formatter,&fmt,sizeof(fruit_stream_t)); 
    encaps->fruit_formatter.kms_key_stores = fmt.kms_key_stores;
    fruit_ctx.num_encaps_active++;

    if(glob_mfp_live_num_pmfs_cntr >= (MAX_FRUIT_CHNLS+1))
	return -1;
    return 0;
}
#if 1
static void fruit_get_video_name(char *output_path, char *video_name)
{
    char  *st, *end;
    int32_t video_name_size;
    video_name_size = 0;

    st = strrchr(output_path, '/');
    if (!st) {
	st = output_path;
    }else{
	st++;
    }

    end = strrchr(output_path, '.');
    if ( (!st) || end <= st) {
	end = output_path + strlen(output_path);
    }

    video_name_size = end - st; /*includes null term */
    memcpy(video_name, st, video_name_size);
    video_name[video_name_size] = '\0'; 

    return;// video_name;
}



#endif

void destroyMfuData(void *mfu_data) 
{
    mfu_data_t *mfu_cont = (mfu_data_t*)mfu_data;
    //    printf("release ref cont address %p\n", mfu_cont);
    if (mfu_cont) {
	if (mfu_cont->data){
	    //	    printf("Free  mod_vpe_mfp_merge_t2 %p\n",mfu_cont->data);
	    free(mfu_cont->data);
	    mfu_cont->data = NULL;
	}
	if (mfu_cont->mfu_discont != NULL) {

		int32_t i = 0;
		while(mfu_cont->mfu_discont[i] != NULL) {
		  //free(mfu_cont->mfu_discont[i++]);
		  free(mfu_cont->mfu_discont[i]);
		  mfu_cont->mfu_discont[i] = NULL;
		  i++;
		}
		free(mfu_cont->mfu_discont);
		mfu_cont->mfu_discont = NULL;
	}
	free(mfu_cont);
	mfu_cont = NULL;
    }
}

static nkn_task_id_t rtscheduleDelayedTask(int64_t microseconds,
					   TaskFunc proc,
					   void* clientData) 
{
    uint64_t tid;
    uint32_t next_avail;
    tid = nkn_rttask_delayed_exec(microseconds / 1000,
				  (microseconds / 1000) + NKN_DEF_MAX_MSEC,
				  proc,
				  clientData,
				  &next_avail);
    return ((nkn_task_id_t)tid);
}


static int32_t fruit_write_ts(char* data, uint32_t len, char *fname)
{
	io_handler_t* io_hdlr = NULL;
	io_complete_hdlr_fptr io_end_hdlr;
	ref_count_mem_t* ref_cont = NULL;
	file_handle_t* fh = NULL;
	int32_t err = 0;

	if (glob_use_async_io_apple_fmtr) {
		io_hdlr = createAIO_Hdlr();
		io_end_hdlr = AppleAIO_CompleteHdlr;
	} else {
		io_hdlr = createIO_Hdlr();
		io_end_hdlr = AppleIO_CompleteHdlr;
	}
	if (io_hdlr == NULL)
		return -E_MFP_NO_SPACE;

	io_open_ctx_t open_ctx;
	open_ctx.path = (uint8_t*)fname;
	open_ctx.flags = O_CREAT | O_WRONLY | O_TRUNC | O_ASYNC;
	open_ctx.mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	fh = io_hdlr->io_open(&open_ctx);
	if (fh == NULL) {
		err = -E_MFP_NO_SPACE;
		goto error;
	}
	if (glob_use_async_io_apple_fmtr) {
		ref_cont = createRefCountedContainer(fh, deleteAIO_RefCtx);
	} else {
		ref_cont = createRefCountedContainer(fh, deleteIO_RefCtx);
	}
	if (ref_cont == NULL) {
		err = -E_MFP_NO_SPACE;
		goto error;
	}
	Apple_WriteNewFile(io_hdlr, io_end_hdlr, fh, data, len, 0, ref_cont);

error:
	io_hdlr->io_hdlr_delete(io_hdlr);
	return err;
}

//#define TIME_DEBUG
//#define PRINT_MSG

#ifdef TIME_DEBUG
#include <sys/time.h>

struct timeval currPktTime;
struct timeval lastPktTime;
static inline int32_t timeval_subtract (struct timeval *x, struct
					timeval *y, struct timeval *result);
int32_t timeval_subtract (struct timeval *x, struct timeval *y, struct
			  timeval *result)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
	int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
	y->tv_usec -= 1000000 * nsec;
	y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
	int nsec = (x->tv_usec - y->tv_usec) / 1000000;
	y->tv_usec += 1000000 * nsec;
	y->tv_sec -= nsec;
    }


    /* Compute the time remaining to wait.
       tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}

#endif
