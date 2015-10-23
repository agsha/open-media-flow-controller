#include "mfp_live_accum_ts_v2.h"
#include "mfp_publ_context.h"
#include "nkn_vpe_mfp_ts2mfu.h"
#include "nkn_vpe_h264_parser.h"
#include "mfu2iphone.h"
#include "nkn_vpe_mfu_writer.h"
#include "mfp_video_header.h"
#include "nkn_debug.h"
#include "mfp_live_accum_create.h"
#include "mfp/thread_pool/mfp_thread_pool.h"
#if defined(INC_ALIGNER)
#include "mfp_video_aligner.h"
#endif
#if defined(INC_SEGD)
#include "mfp_live_mfu_merge.h"
#endif
#include "nkn_assert.h"
#define MAX_UDP_SIZE 4096 // 4Kbytes as max size
#define AUDIO_BUFF_DEPTH 15 //10 buffers at a time
//#define MAX_AUDIO_BITRATE 100 //max audio bitrate in kbps

//#define OFFSET_PKT_TIME

#ifdef OFFSET_PKT_TIME
uint32_t PKT_TIME_OFSET = 0;
#endif

//#define PRINT_MSG

#ifdef PRINT_MSG
#define A_(x) x
#define B_(x) x
#define C_(x) //x
#else
//no prints
#define A_(x) //x
#define B_(x) //x
#define C_(x) //x
#endif

extern gbl_fruit_ctx_t fruit_ctx;
extern mfp_thread_pool_t* apple_fmtr_task_processor;

uint32_t glob_sync_time_calc_offset = 500;
uint32_t glob_num_idrs_sync = 5;
uint32_t glob_dump_input_stream_before_sync = 0;
uint32_t glob_dump_input_stream_after_sync = 0;
uint32_t glob_dump_cc_input = 0;
uint32_t glob_sync_table_debug_print = 0;
uint32_t glob_mfp_buff_size_scale = 15;
uint32_t glob_mfp_max_av_lag = 300;
uint32_t glob_init_av_bias_flag = 0;
uint32_t glob_mfp_audio_buff_time =150;
uint32_t glob_max_kfi_tolerance =15;
uint32_t glob_max_audio_bitrate=150;
uint32_t glob_enable_stream_ha=1;
uint32_t glob_enable_session_ha=1;
uint32_t glob_max_sl_queue_items = 700; 
uint32_t glob_slow_strm_HA_enable_flag = 0;
extern uint32_t glob_print_avsync_log;
uint32_t glob_ignore_audio_pid = 0;

extern mfp_thread_pool_t* task_processor;
/*********************************************************************
 *         ACCUMULATOR INTERFACE IMPLEMENTATION
 * Each accumulator needs to implement 3 interfaces namely
 * 1. getBufferArea - the write buffers into which the data is read
 *                    from any device for the the accumulator to build
 * 2. processData - a funtion that will consume the data read from the
 *                  device and maintain its internal state machine to
 *                  decide if it has accumulated enough data to start
 *                  the format conversion
 * 3. deleteAccumulator - clean up for the accumulator's internal
 *                        state
 * 4. handleEOS - implements stream specific functionality for end of
 *                stream e.g write playlist end etc.
 ***********************************************************************/

/*********************************************************************
 *             ACCUMULATOR STATE MACHINE FUNCTIONS
 ********************************************************************/
/*Used Functions*/


#ifdef INC_SEGD
#define APPLE_MASK 0x01
#define MS_MASK 0x02
#define APPLE_UNMASK 0xFE
#define MS_UNMASK 0xFD


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




static uint32_t get_least_mfu_seq_num(uint32_t* last_mfu_seq_num,
				      mfp_publ_t *pub_ctx);

int32_t verify_pids(mux_st_elems_t *,
                    uint16_t ,
                    uint16_t );

static void clear_all_ts2mfu_desc_ref(ts2mfu_desc_ref_t*);
static void release_ts2mfu_desc(ts2mfu_desc_ref_t* ref);

static void release_ts2mfu_desc_ref(ts2mfu_desc_ref_t* ref,
				    uint8_t);

static void set_least_mfu_seq_num(uint32_t*,
				  mfp_publ_t *,
				  uint32_t ,
				  ACCUM_FMTR_TYPE );


static void clear_mfu_data(mfu_data_t* mfu_data);
#endif

static void refresh_stream_buffer_state(stream_buffer_state_t*);
static void init_spts_stream_buffers(spts_st_ctx_t*,
                                     uint32_t ,
                                     uint32_t );
static void stream_buffer_full(stream_buffer_data_t*, uint32_t*,
                               uint32_t);
static mfu_data_t *mfp_create_eos_mfu(mfub_t *mfu_hdr);
static int32_t update_stream_buff(stream_buffer_data_t* buff,
	uint32_t pos,
	uint32_t pkt_time);


static void del_spts_stream_buffers(spts_st_ctx_t* spts);
static void call_converter(
	mfu_data_t** mfu_data,
	accum_ts_t *accum,
	uint32_t* mstr_pkts, uint32_t mstr_npkts,
	uint32_t* slve_pkts, uint32_t slve_npkts,
	ts2mfu_cont_t *ts2mfu_cont,
	uint32_t audio_time,
	uint32_t video_time,
	uint32_t video_time_accumulated);
static void invalidate_stream_buffer(stream_buffer_data_t* buf);
#if defined(MFP_BUGFIX_8460)
static int32_t obtainFirstTimeMux(sync_ctx_params_t* syncp,
	uint32_t len, int8_t *data,
	uint32_t kfi,spts_st_ctx_t *spts,
	mfp_publ_t* pub_ctx,
	sess_stream_id_t* id);
#else
static void obtainFirstTimeMux(sync_ctx_params_t* syncp,
	uint32_t len, int8_t *data,
	uint32_t kfi,uint16_t vpid,
	mfp_publ_t* pub_ctx,
	sess_stream_id_t* id);
#endif
static int32_t syncToTimeMux(sync_ctx_params_t* syncp,
	uint32_t *length, int8_t *data,
	accum_state_t *accum_state,
	int8_t *start_accum,
	uint32_t pos,
	uint32_t kfi,
	spts_st_ctx_t *spts,
	mfp_publ_t* pub_ctx,
	sess_stream_id_t* id
	);
static uint8_t get_frame_type(uint8_t *data);
static void write_ts_mfu_hdrs(mfub_t * mfu,stream_param_t* st, sess_stream_id_t* id);
static void refresh_accum_state(mfp_publ_t* pub_ctx,accum_ts_t* accum);
static void set_encaps_to_resync(mfp_publ_t* pub_ctx);
static int32_t go_to_resync(mfp_publ_t* pub_ctx,
	accum_ts_t* accum,
	sync_ctx_params_t *syn,
	sess_stream_id_t* id);
static void refresh_linear_buffer(stream_lin_buff_t*);
uint64_t sess_chunk_num = 0;
/****************************************
  MPEG2TS Parsing Functions
 *****************************************/
static void parse_spts_pmt(uint8_t* data,
			   uint32_t len,
			   mux_st_elems_t *mux);
static uint16_t get_pmt_pid(uint8_t* data,uint32_t len);
static uint16_t get_pkt_pid(uint8_t* data);
static uint16_t parse_pat_for_pmt(uint8_t *data);
static void get_av_pids_from_pmt(uint8_t *data, uint16_t* vpid,
	uint16_t *apid,  uint16_t* apid2);
static int8_t get_spts_pids(uint8_t*  data,  uint32_t len,
	uint16_t* vpid,  uint16_t *apid,
	uint16_t* apid2, uint16_t pmt_pid);

/* Function to check session and stream state validity */
static int32_t checkSessionStreamState(sess_stream_id_t const *id);

/**************************************
  Silverlight Support for formatting
 **************************************/
#include "nkn_vpe_sl_fmtr.h"
static void refMemRelease(void* ref); 
static ser_task_t* create_ser_sync_fmtr_task(sync_serial_t* sync_ser,
		uint32_t sess_seq_num, uint32_t group_id,
		uint32_t group_seq_num, task_hdlr_fptr task_hdlr, void* ctx); 
void slHandleResync(uint32_t sess_id, uint32_t stream_id,
		uint32_t accum_seq_num);

//#define AUDIO_ADJUST

static int32_t adjust_audio_pkts(ts2mfu_cont_t *ts2mfu_cont,
                                 stream_lin_buff_t* lin_buff,
                                 uint8_t *data);


/*Accumulator V2 Functions*/

static int32_t  update_linear_stream_buff(stream_lin_buff_t*,
					  uint32_t,
					  uint32_t );
static int32_t get_audio_data(stream_lin_buff_t*,
			      uint32_t,
			      accum_audio_prop_t* );





/****************************************
  Rt scheduler related functions
 ****************************************/
#define USE_RT_SCHED
#ifdef USE_RT_SCHED
#include <atomic_ops.h>
#include "nkn_rtsched_api.h"
#include "nkn_slotapi.h"

uint64_t mfpRtscheduleDelayedTask(int64_t microseconds,
	mfpTaskFunc proc,
	void* clientData)
{

    uint64_t tid;
    uint32_t next_avail;
    tid = nkn_rttask_delayed_exec(microseconds / 1000,
	    (microseconds / 1000) + NKN_DEF_MAX_MSEC,
	    proc,
	    clientData,
	    &next_avail);
    return tid;
}
#endif

/*Global Vairables*/
uint32_t frag_no = 0;
uint32_t frag_no1 = 0;
uint32_t frag_no2 = 0;



void register_formatter_error(
			      ref_count_mem_t *ref_cont){
    
    int32_t sess_id;
    int32_t stream_id;
    
    mfu_data_t *mfu_data = (mfu_data_t*)ref_cont->mem;
    fruit_fmt_input_t *fmt_input = (fruit_fmt_input_t *)mfu_data->fruit_fmtr_ctx;	
    
    mfu_mfub_box_t *mfu_box_ptr = parseMfuMfubBox(mfu_data->data);
    
    sess_id = mfu_box_ptr->mfub->program_number;
    stream_id = mfu_box_ptr->mfub->stream_id;
    
    //    live_state_cont->acquire_ctx(live_state_cont, sess_id);
    mfp_publ_t * pub_ctx = (mfp_publ_t *)
	live_state_cont->get_ctx(live_state_cont, sess_id);
    if(pub_ctx){
	if(glob_enable_session_ha){
	    switch(mfu_data->fmtr_type){
		case FMTR_APPLE:
		    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
				"sess[%d] stream[%d]: Rx Error (%d) from APPLE formatter",
					    sess_id, stream_id, mfu_data->fmtr_err);							
		    pub_ctx->ms_parm.status = -mfu_data->fmtr_err;
		    break;
		case FMTR_SILVERLIGHT:
		    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
				"sess[%d] stream[%d]: Rx Error (%d) from SL formatter",
					sess_id, stream_id, mfu_data->fmtr_err);							
		    pub_ctx->ss_parm.status = -mfu_data->fmtr_err;
		    break;
		default:
		    //no error case
		    break;
	    }
	    if(pub_ctx->ms_parm.status != FMTR_ON && pub_ctx->ss_parm.status != FMTR_ON){
			    accum_ts_t* accum = (accum_ts_t*)pub_ctx->accum_ctx[stream_id];
			    if(accum){
				DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
					    "sess[%d] stream[%d]: Registered error from all formatters",
					    sess_id, stream_id);			
				accum->accum_state = ACCUM_ERROR;
			    }		    	
	    }
	}else{
	    accum_ts_t* accum = (accum_ts_t*)pub_ctx->accum_ctx[stream_id];
		    if(accum){
			DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
				    "sess[%d] stream[%d]: Registered error from formatter",
				    sess_id, stream_id);			
			accum->accum_state = ACCUM_ERROR;
		    }
	    }
    }
    //    live_state_cont->release_ctx(live_state_cont, sess_id);
    
    //ref_cont->release_ref_cont(ref_cont);
    mfu_box_ptr->del(mfu_box_ptr);
    
    return;	
}



extern uint64_t glob_mfp_live_tot_pmfs;
#ifndef GLOB_CNT_PMF
extern uint64_t glob_mfp_live_num_pmfs_cntr;
#else
extern int32_t num_pmfs_cntr;
#endif

static uint32_t calcTotalStrms(mfp_publ_t *pub_ctx){
	stream_param_t* stream_parm;
	uint32_t strm_cnt;
	uint32_t num_strms = pub_ctx->streams_per_encaps[0];
	for(strm_cnt = 0; strm_cnt < num_strms; strm_cnt++){
		stream_parm = &(pub_ctx->stream_parm[strm_cnt]);
		if(stream_parm->stream_state == STRM_DEAD)
			num_strms++;
	}
	return (num_strms);
}

#ifdef MFP_MEM_OPT
static void refresh_accum_state(mfp_publ_t* pub_ctx,accum_ts_t* accum){
    memset(accum->data,0,accum->data_size);
    accum->refreshed = 1;
    accum->last_read_index = -1;
    accum->data_pos = 0;
    accum->start_accum = 0;
    accum->present_read_index = 0;
    accum->accum_state = ACCUM_INIT;
    accum->spts->spts_data_size = accum->data_size;
    accum->spts->mux.video_in_sync = accum->spts->mux.audio1_in_sync = accum->spts->mux.audio2_in_sync =0;
    accum->spts->sync_time = 0;
    //accum->running = 0;
    accum->spts->video->num_buffs_pending_read = 0;
    if(accum->spts->mux.video_pid){
        refresh_stream_buffer_state(accum->spts->video);
    }
    if(accum->spts->mux.audio_pid1){
	refresh_linear_buffer(accum->spts->audio1->linear_buff);
    }
    if(accum->spts->mux.audio_pid2){
        refresh_linear_buffer(accum->spts->audio2->linear_buff);
    }

#ifdef INC_SEGD
    clear_all_ts2mfu_desc_ref(accum->ts2mfu_desc_ref);
    if(pub_ctx->ms_parm.status == FMTR_ON)
	clear_fmtr_vars(accum->acc_ms_fmtr->fmtr);
    if(pub_ctx->ss_parm.status == FMTR_ON)
	clear_fmtr_vars(accum->acc_ss_fmtr->fmtr);
#endif

    return;
}


static void refresh_linear_buffer(stream_lin_buff_t* buff){
    uint32_t j;
    buff->write_pos = 0;
    buff->last_read_indx =0;
    buff->last_read_acc_buf = 0;
    buff->audio_time_acced =0;
    buff->last_read_time = 0;
    for(j=0;j<buff->tot_acc_aud_bufs;j++)
	buff->acc_aud_buf[j].num_aud_pkts=0;
    for(j=0; j<buff->max_num_pkts;j++)
	buff->pkts[j].pkt_state = buff->pkts[j].pkt_time = buff->pkts[j].pkt_offset = 0;

    return;
}

static void refresh_stream_buffer_state(stream_buffer_state_t* st){
    uint32_t i;
    st->write_pos = 0;
    st->buff_full = 0;
    st->num_buffs_pending_read = 0;
    st->first_sync_id = 0;
    st->just_in_sync = 0;
    st->global_time_accumulated = 0;
    st->residual_mfu_time = 0;
    st->prev_residue =0;
    st->num_chunked = 0;
    st->video_ts_bias = 0;
    st->video_bias_chkd = 0;
    for(i=0;i<st->mfp_max_buf_depth;i++){
        stream_buffer_data_t *bd = NULL;
        bd = &st->stream[i];
        bd->num_of_pkts=0;
        bd->pending_read=0;
        bd->starting_time=0;
        bd->time_accumulated=0;
        bd->data_full=0;
        bd->audio_time_acced=0;
        bd->video_time_acced=0;
        memset(bd->pkt_offsets,0,bd->mfp_ts_max_pkts*sizeof(uint32_t));
    }
    return;
}
#else

static void refresh_accum_state(accum_ts_t* accum){
    memset(accum->data,0,accum->data_size);
    accum->refreshed = 1;
    accum->last_read_index = -1;
    accum->data_pos = 0;
    accum->start_accum = 0;
    accum->present_read_index = 0;
    accum->sync_state = ACC_NOT_IN_SYNC;
    accum->spts->spts_data_size = accum->data_size;
    accum->spts->mux.video_in_sync = accum->spts->mux.audio1_in_sync = accum->spts->mux.audio2_in_sync =0;
    accum->spts->sync_time = 0;
    //accum->running = 0;
    accum->spts->video->num_buffs_pending_read = 0;
    if(accum->spts->mux.video_pid)
	memset(accum->spts->video,0,sizeof(stream_buffer_state_t));
    if(accum->spts->mux.audio_pid1)
	memset(accum->spts->audio1,0,sizeof(stream_buffer_state_t));
    return;
}
#endif
void getBufferArea(int8_t** data, uint32_t* len_avl,
	sess_stream_id_t* id)
{
    /* if len_avl value is returned as zero, then EPOLLIN on this socket
       will be unset */
    *len_avl = 0;
    if (checkSessionStreamState(id) > 0) {
	mfp_publ_t* pub_ctx = live_state_cont->get_ctx(live_state_cont,
		id->sess_id);
	accum_ts_t* accum = (accum_ts_t*)pub_ctx->accum_ctx[id->stream_id];

	*data = (int8_t*)(accum->data + accum->data_pos);
	*len_avl = accum->data_size-accum->data_pos;
    }
}


void handleTsEOS(void *ctx, uint32_t stream_id)
{
    mfp_publ_t *pub;
    size_t mfu_data_size;
    mfu_data_t *mfu_data;
    accum_ts_t* accum;

    mfu_data_size = 0;
    mfu_data = NULL;

    pub = (mfp_publ_t *)ctx;
    accum = (accum_ts_t*)pub->accum_ctx[stream_id];

    if (!pub) {
	DBG_MFPLOG ("0", ERROR, MOD_MFPLIVE, "Publisher Context already"
		" deleted, cannot write end of session");
	return;
    }

    /* get and end of stream (EOS) mfu */
    call_converter(&mfu_data, accum, NULL,
	    0, NULL, 0, NULL, 0,0,0);

    /* call formatters with this EOS mfu  and let the respective
     * formatters handle the EOS condition
     */
    mfu_data->data_id = pub->sess_chunk_ctr;

    ref_count_mem_t *ref_cont = createRefCountedContainer(mfu_data,
	    destroyMfuData);
    if (pub->ms_parm.status == FMTR_ON || pub->ms_parm.status < 0)
	ref_cont->hold_ref_cont(ref_cont);
    if (pub->ss_parm.status == FMTR_ON || pub->ss_parm.status < 0)
	ref_cont->hold_ref_cont(ref_cont);

    if(pub->ms_parm.status == FMTR_ON 
    	|| pub->ms_parm.status == FMTR_OUTPUT_IO_ERROR
    	|| pub->ms_parm.status == FMTR_INPUT_DATA_ERROR){
	mfpTaskFunc handler = (void*)mfubox_ts;
	fruit_fmt_input_t *fmt_input = NULL;
	fmt_input =\
		nkn_calloc_type(1, sizeof(fruit_fmt_input_t),
		mod_vpe_mfp_fruit_fmt_input_t);
	if (!fmt_input) {
	    DBG_MFPLOG(pub->name, ERROR, MOD_MFPLIVE,
		    "not enough memory: %d",
		    -E_MFP_NO_MEM);
	    assert(0);
	}

	fmt_input->state = FRUIT_FMT_CONVERT;
#if defined (INC_SEGD)
	fmt_input->mfu_seq_num = accum->acc_ms_fmtr->seq_num;
#else
	fmt_input->mfu_seq_num = accum->mfu_seq_num /*+ pub->ms_parm.segment_start_idx*/;
#endif
	fmt_input->sioc = pub->ms_parm.sioc;
	*accum->stats.apple_fmtr_num_active_tasks += 1;
	fmt_input->active_task_cntr = accum->stats.apple_fmtr_num_active_tasks;
	mfu_data->fruit_fmtr_ctx = (void*)fmt_input;
	//mfpRtscheduleDelayedTask(15000, handler,(void*) ref_cont);
	taskHandler tp_handler = (taskHandler)mfubox_ts;
	thread_pool_task_t* t_task = 
		newThreadPoolTask(tp_handler, ref_cont, NULL);
	apple_fmtr_task_processor->add_work_item(apple_fmtr_task_processor, t_task); 
	
    }

    if (pub->ss_parm.status == FMTR_ON 
    	|| pub->ss_parm.status == FMTR_INPUT_DATA_ERROR
    	|| pub->ss_parm.status == FMTR_OUTPUT_IO_ERROR){
	/* bug 7875: check if there is a valid formatter here; if
	 * initSessionPipeline fails because it was unable to create
	 * the SL formatter, then the handlEOS will be called without
	 * the SL fomatter
	 */
	if (pub->sl_publ_fmtr) {
	    sl_fmtr_t* fmt = (sl_fmtr_t*)(pub->sl_publ_fmtr);
	    mfu_data->sl_fmtr_ctx = fmt;
	    mfpTaskFunc handler = pub->sl_sync_ser->serial_in_hdlr; 
#if defined (INC_SEGD)
            ser_task_t* task = create_ser_sync_fmtr_task(pub->sl_sync_ser,
                                                         pub->sess_chunk_ctr, stream_id,
                                                         /*accum->mfu_seq_num,*/
							 accum->acc_ss_fmtr->seq_num,
							 slTaskInterface, ref_cont);
#else
	    ser_task_t* task = create_ser_sync_fmtr_task(pub->sl_sync_ser,
							 pub->sess_chunk_ctr, stream_id,
							 accum->mfu_seq_num,slTaskInterface, ref_cont);
#endif
	    //mfpRtscheduleDelayedTask(15000, handler, (void*)task);
	    thread_pool_task_t* t_task = newThreadPoolTask(handler, task, NULL);
	    task_processor->add_work_item(task_processor, t_task); 
	}
    }
    pub->sess_chunk_ctr++;

}

#ifdef INC_SEGD
static uint32_t get_least_mfu_seq_num(uint32_t* last_mfu_seq_num,
				      mfp_publ_t * pub_ctx){
				      
    uint32_t strm_cnt, least_mfu_seq_num=0xffffffff;
    stream_param_t* stream_parm;
    uint32_t num_strms = calcTotalStrms(pub_ctx);
    
    for(strm_cnt= 0; strm_cnt < num_strms; strm_cnt++){
	stream_parm = &(pub_ctx->stream_parm[strm_cnt]);
	if(stream_parm->stream_state == STRM_ACTIVE){
		if(last_mfu_seq_num[strm_cnt] <least_mfu_seq_num){
		    least_mfu_seq_num = last_mfu_seq_num[strm_cnt];
		}
	}
    }
    return least_mfu_seq_num;
}

static void set_least_mfu_seq_num(uint32_t* last_mfu_seq_num,
				  mfp_publ_t *pub_ctx,
				  uint32_t least_mfu_seq_num,
				  ACCUM_FMTR_TYPE fmtr){
    uint32_t li;
    uint32_t num_strms = calcTotalStrms(pub_ctx);
    stream_param_t* stream_parm;

    for(li= 0; li < num_strms; li++){
    	stream_parm = &(pub_ctx->stream_parm[li]);
	if(stream_parm->stream_state == STRM_ACTIVE){
		accum_ts_t* tmp_accum = (accum_ts_t*)pub_ctx->accum_ctx[li];
		switch(fmtr){
		    case APPLE:
			tmp_accum->acc_ms_fmtr->seq_num = least_mfu_seq_num+1;
			break;
		    case SL:
			tmp_accum->acc_ss_fmtr->seq_num = least_mfu_seq_num+1;
			break;
		    default:
		    	tmp_accum->mfu_seq_num = least_mfu_seq_num + 1;
			break;
		}
		last_mfu_seq_num[li] = least_mfu_seq_num;
	}
    }
    return;
}


#endif
static uint32_t get_ms_accum_seq_num(accum_ts_t* accum){
	if(accum->acc_ms_fmtr)
		return accum->acc_ms_fmtr->seq_num;
	else
		return 0;
}

static uint32_t get_sl_accum_seq_num(accum_ts_t* accum){
	if(accum->acc_ss_fmtr)
		return accum->acc_ss_fmtr->seq_num;
	else
		return 0;
}

/*Return 0 for error. Else return 1*/
int32_t verify_pids(mux_st_elems_t *mux,
		    uint16_t vpid,
		    uint16_t apid){
    if(vpid == 0 &&  apid == 0){
	DBG_MFPLOG("MFP", MSG, MOD_MFPLIVE, "No PIDs entered by the User");
	return 1;
    }
    if(mux->video_pid!=vpid && vpid){
	mux->audio_pid2 = 0;
	return 0;
    }
    /*If required apid is apid2, set it to apid1*/
    if(mux->audio_pid2 == apid){
	mux->audio_pid1 = apid;
	mux->audio_pid2 = 0;
	return 1;
    }
    /*Let apid1 remain: PID not found then error out*/
    if(mux->audio_pid1 != apid){
	mux->audio_pid2 = 0;
	return 0;
    }

    return 1;
}

static char * convert_slicetype_to_string(slice_type_e type){
	char *res;
	switch(type){
		case 1:
			res = (char*)"I Slice";
			break;
		case 2:
			res = (char*)"P Slice";
			break;
		case 3:
			res = (char*)"B Slice";
			break;
		case 0:
			res = (char*)"Not Assigned";
			break;
		case -1:
			res = (char*)"NeedMoreData";
			break;
	}
	return (res);
}


int32_t processData(int8_t* data, 
		    uint32_t len,
		    sess_stream_id_t* id)
{

    mfu_data_t* mfu_data = NULL;
    mfp_publ_t* pub_ctx;
    int32_t ret = 0;
    uint32_t is_aud_pending = 0;
    int32_t glob_tot_num_aud_chnks;
    accum_ts_t* related_st_accum = NULL;
    int8_t related_idx_present=0,video_acced =0 ,audio1_acced=0, audio2_acced=0;
    uint32_t new_data = 1;
    pub_ctx = live_state_cont->get_ctx(live_state_cont,
	    id->sess_id);
    stream_param_t* stream_parm =
	&(pub_ctx->stream_parm[id->stream_id]);
    uint32_t start_pkt = 0;

#if defined(MFP_BUGFIX_8460)
    ts_pkt_infotable_t *pkt_tbl = NULL;
#endif

    if(stream_parm->stream_state == STRM_DEAD){
    	//new return error code to differentiate stream dead
    	//vs other accum and fmtr errors
    	ret = -2;
	goto procDataRet;
    }

    /*Get the sync issue in perspective*/
    sync_ctx_params_t* syn;
    syn = &pub_ctx->encaps[0/*id->sess_id*/]->syncp;

    if(glob_dump_input_stream_before_sync){
        FILE *fid;
        char fname[50]={'\0'};
        uint32_t len1;
        len1 =   snprintf(NULL,0,"/var/mfpbits/%s_raw_stream_%d.ts",pub_ctx->name,id->stream_id);
        snprintf(fname,len1+1,"/var/mfpbits/%s_raw_stream_%d.ts",pub_ctx->name,id->stream_id);
        fid = fopen(fname, "ab");
        fwrite(data,len,1,fid);
        fclose(fid);
    }

    uint32_t pkt_time = 0, i = 0, n_ts_pkts = 0;
    uint32_t pts, dts;
    uint32_t write_index = 0, read_index, total_pkt_time = 0,
	     next_mfu_read_index, total_time;
    int32_t frame_type = 1, rv = 0;
    uint16_t pid=0,spid=0;

    if (checkSessionStreamState(id) < 0) {        	
	ret = -1;
	//	printf("checkSessionStreamState Failed\n");
	DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE, "checkSessionStreamState Failed");
	goto procDataRet;
    }

    accum_ts_t* accum = (accum_ts_t*)pub_ctx->accum_ctx[id->stream_id];

    if(accum->accum_state == ACCUM_ERROR){
	ret = -1;
	//	    printf("Accum state error\n");
	DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE, "Accumulator in Error State");
	goto procDataRet;
    }

    if(accum->prev_data_ptr && accum->prev_len){
    	len = len + accum->prev_len;
        data = accum->prev_data_ptr;
	start_pkt = accum->prev_pktend;
	accum->data_pos = accum->data_pos - accum->prev_len;
	accum->prev_pktend = 0;
	accum->prev_data_ptr = NULL;
	accum->prev_len = 0;
	//printf("sess %u strm %u staggered datalen %u\n", 
	//	id->sess_id, id->stream_id, len);
    }


    /*Parse for PMT and do not enter till PMT is parsed*/
    /*If the audio and video pids are mentioned then skip the parsing*/
    if(!accum->spts->mux.demux_flag&&stream_parm->med_type == MUX) {
	if (id->stream_id == 0) {
	    int uu = 0;
	}
	parse_spts_pmt((uint8_t*)data,len,&accum->spts->mux);
	if(!accum->spts->mux.pmt_parsed) {
	    DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE, "Started Receiving"
		       " Data, searching for PAT, PMT, VPID,"
		       " APID");
	    return 0;
	}
	if(!verify_pids(&accum->spts->mux,stream_parm->vpid,stream_parm->apid)){
	    //	    ret = -1;
	    DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE, "PID verfication Mismatch:" 
		       "Audio PID mentioned %d found %d",
		       stream_parm->apid,accum->spts->mux.audio_pid1);
	    DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE, "PID verfication Mismatch:" 
		       "Video PID mentioned %d found %d", 
		       stream_parm->vpid,accum->spts->mux.video_pid);	    
	    DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE, "Proceeding as follows:"
                       "Audio PID set: %d "
                       "Video PID set: %d ",
                       accum->spts->mux.audio_pid1,
                       accum->spts->mux.video_pid);
	    //	    goto procDataRet;
	}
	else
	    DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE, "PID verfication Succeeded:"
                       "Audio PID set: %d "
                       "Video PID set: %d ",
                       accum->spts->mux.audio_pid1,
                       accum->spts->mux.video_pid);
    
	if(glob_ignore_audio_pid){
		accum->spts->mux.audio_pid1 = 0;
		accum->spts->mux.audio_pid2 = 0;
		DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE, "After ignore audio PID"
	       "Audio PID set: %d "
	       "Video PID set: %d ",
	        accum->spts->mux.audio_pid1,
	        accum->spts->mux.video_pid);
	}

	
	accum->spts->mux.demux_flag = 1;
	if(accum->spts->mux.audio_pid1 == 0){
	    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE, "Stream %d"
			" LIVE FEED Has no Audio OR unsupported Audio",
			id->stream_id);  
	}

	if(!accum->spts->video) {
            init_spts_stream_buffers(accum->spts,
                                     accum->key_frame_interval,
                                     stream_parm->bit_rate);
	}
    }

    if(syn->kill_all_streams){
	DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE,
		"sess[%d]: Killing stream %d as syn->kill_all_streams is TRUE",
		id->sess_id, id->stream_id);
	ret = -1;
	goto procDataRet;
    }

    /*Handle the Normal Resync case*/
    if(((syn->ref_accum_cntr > 0) && (syn->ref_accum_cntr < pub_ctx->streams_per_encaps[0])) ||
       ((syn->ref_accum_cntr == 0) && (pub_ctx->streams_per_encaps[0] == 1))){
	if(!accum->refreshed){
	    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE, \
	    	"sess[%d] stream[%d]: Refreshing accum Num left = %d", 
	    id->sess_id, id->stream_id, syn->ref_accum_cntr);
	    id->ts2mfu_cont.first_instance = 0;
	    memset(&id->ts2mfu_cont.bias,0,sizeof(audio_bias_t));
	    refresh_accum_state(pub_ctx,accum);
	    syn->ref_accum_cntr--;
	    if(pub_ctx->ms_parm.status == FMTR_ON)
		    resetDataPair(accum->acc_ms_fmtr->dproot);
	    if(pub_ctx->ss_parm.status == FMTR_ON)
		    resetDataPair(accum->acc_ss_fmtr->dproot);
	    if(pub_ctx->fs_parm.status == FMTR_ON)
		    resetDataPair(accum->acc_ze_fmtr->dproot);	    
	}
	//all streams got into resyn when ref_accum_cntr is Zero
	if(syn->ref_accum_cntr == 0){
	    mfp_publ_t* pctx_tmp = 
		live_state_cont->acquire_ctx(live_state_cont, id->sess_id);
	    // all are in resync state
	    uint32_t least_mfu_seq_num = 0xffffffff;
	    uint32_t li;
#if defined (INC_SEGD)
	    ACCUM_FMTR_TYPE fmtr_type;
	    syn->ref_accum_cntr = pub_ctx->streams_per_encaps[0];
	    if(pub_ctx->ms_parm.status == FMTR_ON){
		fmtr_type= APPLE;
		least_mfu_seq_num = get_least_mfu_seq_num(syn->ms_last_mfu_seq_num,
							  pub_ctx);
		least_mfu_seq_num = least_mfu_seq_num + 1;
		DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,                \
			    "Next mfu seq num is set to %u for Apple", least_mfu_seq_num);
                register_stream_resync(id->sess_id,
                                       id->stream_id,
                                       least_mfu_seq_num, 0);
		set_least_mfu_seq_num(syn->ms_last_mfu_seq_num,
				      pub_ctx, least_mfu_seq_num-1,
                                      fmtr_type);
            }

	    if (pub_ctx->ss_parm.status == FMTR_ON) {
		fmtr_type = SL;
                least_mfu_seq_num = get_least_mfu_seq_num(syn->ss_last_mfu_seq_num,
                                                          pub_ctx);
                least_mfu_seq_num = least_mfu_seq_num + 1;
                DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,                \
                            "Next mfu seq num is set to %u for SL", least_mfu_seq_num);
		slHandleResync(id->sess_id, id->stream_id, least_mfu_seq_num);
                set_least_mfu_seq_num(syn->ss_last_mfu_seq_num,
                                      pub_ctx,
                                      least_mfu_seq_num-1,
				      fmtr_type);
	    }
	    pctx_tmp->sess_chunk_ctr++;

#else //segd
	    least_mfu_seq_num = get_least_mfu_seq_num(
	    			syn->last_mfu_seq_num, pub_ctx);
	    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,		\
			"least mfu seq num %u", least_mfu_seq_num);
	    least_mfu_seq_num = least_mfu_seq_num + 1;
	    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,		\
			"Next mfu seq num is set to %u", least_mfu_seq_num);
	    //re-init ref_accum_cntr to stable state
	    syn->ref_accum_cntr = pub_ctx->streams_per_encaps[0];
	    if(pub_ctx->ms_parm.status == FMTR_ON){
		register_stream_resync(id->sess_id, 
				       id->stream_id, 
				       least_mfu_seq_num, 0);
	    }
	    if (pub_ctx->ss_parm.status == FMTR_ON) {
		slHandleResync(id->sess_id, id->stream_id, least_mfu_seq_num);
	    }
	    pctx_tmp->sess_chunk_ctr++;
	    
	    set_least_mfu_seq_num(syn->last_mfu_seq_num,
                                  pub_ctx,
                                  least_mfu_seq_num-1,
				  NONE);	    
#endif //segd
	    
	    live_state_cont->release_ctx(live_state_cont, id->sess_id);
	}

	return 0;

    }
    if(accum->refreshed)
	accum->refreshed =0;
    /*Resync Refresh Completed*/
    if((accum->accum_state & 1) != ACCUM_SYNC){
#if defined(MFP_BUGFIX_8460)    	
	ret = obtainFirstTimeMux(syn,len,data,
		accum->key_frame_interval,
		accum->spts, pub_ctx, id);
	if(ret == -1){
	    //failed in obtain first time mux
	    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE, "Exiting: All streams being Killed");
	    goto procDataRet;
	}	
#else
	obtainFirstTimeMux(syn,len,data,
			   accum->key_frame_interval,
			   accum->spts->mux.video_pid, pub_ctx, id);	
#endif
	if(!pub_ctx->sync_params.valid_index_entry[0][id->stream_id]){
	   //not yet found out first IDR, wait for it
	    return 0;
	}
	DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE, "Session State: SYNC");
	A_(printf("sess %u strm %u Session State: SYNC\n", id->sess_id, id->stream_id));
	syn->ref_accum_cntr = pub_ctx->streams_per_encaps[0];
	//init to max positive value till sync time is found out
	syn->sync_time = 0xffffffff;
	accum->spts->sync_time = syn->sync_time;
	accum->accum_state = ACCUM_SYNC;
    }
    /*Look for sync and accum states*/
    if((accum->accum_state & 16) != ACCUM_RUNNING){
	accum->accum_state |= ACCUM_SYNC;
	if(syn->sync_time != 0xffffffff && accum->spts->sync_time == 0xffffffff){
	    //change it once after syn->sync_time is set
	    accum->spts->sync_time = syn->sync_time;
	}
	syn->stream_num = id->stream_id;
	ret = syncToTimeMux(syn,
		&len, data,
		&accum->accum_state,
		&accum->start_accum,
		accum->data_pos,
		accum->key_frame_interval,
		accum->spts, pub_ctx, id);
	if(ret == -1){
		//failed in sync to time mux
	    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE, "Exiting: Sync Failed");
	    syn->kill_all_streams = 1;
	    goto procDataRet;
	}
	pub_ctx->op = PUB_SESS_STATE_SYNC;
	//	DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE, "Session State: SYNC");
	if(accum->accum_state == ACCUM_SYNC){
	    //not yet reached sync_time, wait till it is achieved
	    return 0;
	}
	if((accum->spts->mux.audio_pid1 != 0) && (accum->spts->mux.video_pid != 0)) {
	    if((accum->accum_state == (ACCUM_SYNC | ACCUM_VIDEO_INSYNC | ACCUM_AUDIO1_INSYNC))||
	       (accum->accum_state == (ACCUM_SYNC | ACCUM_VIDEO_INSYNC | ACCUM_AUDIO2_INSYNC))){
		printf("Accum State = %d\n",accum->accum_state);
		DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE, 
			    "sess %u strm %u Session State: RUNNING", id->sess_id, id->stream_id);
		A_(printf("sess %u strm %u Session State: RUNNING\n", id->sess_id, id->stream_id));
		accum->accum_state |= ACCUM_RUNNING;
		pub_ctx->op = PUB_SESS_STATE_RUNNING;
		accum->refresh_ms_fmtr = 1;
		if(pub_ctx->ms_parm.status == FMTR_ON){
		    register_stream_resync(id->sess_id, id->stream_id, 0, 1);
		}
	    }
	} else if((accum->spts->mux.video_pid != 0) && (accum->spts->mux.audio_pid1 == 0)){
	    if(accum->accum_state == (ACCUM_SYNC | ACCUM_VIDEO_INSYNC)){
		DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
			    "sess %u strm %u Session State: RUNNING", id->sess_id, id->stream_id);
		A_(printf("sess %u strm %u Session State: RUNNING\n", id->sess_id, id->stream_id));
		accum->refresh_ms_fmtr = 1;
		accum->accum_state |= ACCUM_RUNNING;
		pub_ctx->op = PUB_SESS_STATE_RUNNING;
	    }
	}
    }


    if(glob_dump_input_stream_after_sync){
        FILE *fid;
        char fname[50]={'\0'};
        uint32_t len1;
        len1 =   snprintf(NULL,0,"/var/mfpbits/%s_stream_%d.ts",pub_ctx->name,id->stream_id);
        snprintf(fname,len1+1,"/var/mfpbits/%s_stream_%d.ts",pub_ctx->name,id->stream_id);
        fid = fopen(fname, "ab");
        fwrite(data,len,1,fid);
        fclose(fid);
    }

    
    /*In accumulation state - not for all streams though.*/
    n_ts_pkts = len / 188;
    accum->stats.tot_pkts_rx += n_ts_pkts;
    pub_ctx->stats.tot_input_bytes += (n_ts_pkts * TS_PKT_SIZE);
    //if(n_ts_pkts > 7)
    //printf("sess %u strm %u n_ts_pkts %u\n", id->sess_id, id->stream_id, n_ts_pkts);
#if defined(MFP_BUGFIX_8460)
    uint32_t pkt_offsets[MAX_NUM_NWINPT_PKTS];
    uint32_t cnt;
    for( cnt = 0; cnt < n_ts_pkts; cnt++){
    	pkt_offsets[cnt] = cnt * TS_PKT_SIZE;
    }
    ret = create_index_table((uint8_t *)data, pkt_offsets, \
                             n_ts_pkts, accum->spts->mux.audio_pid1,
                             accum->spts->mux.video_pid, &pkt_tbl);

    if(ret < 0){
	printf("Create index Table failed\n");
	goto procDataRet;
    }
#endif

    for (i = start_pkt; i < n_ts_pkts; i++){
	
#if defined(MFP_BUGFIX_8460)    	
	pid = pkt_tbl[i].pid;
	dts = pkt_tbl[i].dts;
	pts = pkt_tbl[i].pts;
#else	
	mfu_converter_get_timestamp((uint8_t *)(data+(i * 188)),NULL,
		&pid, &pts, &dts, NULL);
#endif
	pkt_time = dts;
	if((pub_ctx->sync_params.use_pts && pub_ctx->sync_params.use_pts_decision_valid && pts)||
		(pid == accum->spts->mux.video_pid && !dts)){
		pkt_time = pts;	
	}
	if((pid == accum->spts->mux.audio_pid1 || pid == accum->spts->mux.audio_pid2)&& !dts)
		pkt_time = pts;
#ifdef OFFSET_PKT_TIME
	if(pkt_time)
	    pkt_time += PKT_TIME_OFSET;
#endif
	if(*(data+(i * 188)) != 0x47){
	    A_(printf("Sync byte error here\n"));
	    DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "SYNC BYTE NOT FOUND:EXITING\n");
	    ret = -1;
	    //	    printf("Sync byte error here\n");
	    goto procDataRet;
	}
	/*Acc & Check Video Stream*/
	if(pid == accum->spts->mux.video_pid && accum->spts->mux.video_in_sync){
	    stream_buffer_data_t *st = NULL;
	    int8_t video_ts_rollover_pt = 0;
	    if(accum->spts->video->just_in_sync){
		if(i<accum->spts->video->first_sync_id){
		    //skip all packets till the packet @ sync_time is reached
		    continue;
		}
		if(accum->spts->mux.audio_pid1)
		    accum->spts->audio1->linear_buff->video_sync_time = accum->spts->video->sync_time;
		accum->spts->video->just_in_sync = 0;
	    }
	    st = &accum->spts->video->stream[accum->spts->video->write_pos];
	    if(pkt_time && ((int64_t)((int64_t)pkt_time-(int64_t)st->starting_time)<0) && !accum->spts->video->ts_warped){
		accum->spts->video->ts_warped = 1;
		st->video_time_acced = st->time_accumulated- st->starting_time;
		accum->spts->video->warp_start = 0xFFFFFFFF-st->time_accumulated+1;
		DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
			    "Warping of Video Started for Stream %d Warping time %u",
			    id->stream_id,accum->spts->video->warp_start);

	    }

	    rv = update_stream_buff(st,accum->data_pos+(i*188),pkt_time);
	    if (rv == MFP_ACC_BUF_FULL) {
		DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE,
                            "Sess[%d] Stream [%d] Buffer Overrun, Attempting Resync", id\
			    ->sess_id, id->stream_id);
		return go_to_resync(pub_ctx,accum,syn,id);
	    }
	    if(pkt_time && accum->spts->video->ts_warped){
		uint32_t acced = 0;
		acced = st->video_time_acced + pkt_time + accum->spts->video->warp_start;
		if(acced >= (accum->spts->video->video_ts_bias+accum->key_frame_interval*90)){
		    accum->spts->video->ts_warped = 0;
		    video_ts_rollover_pt =1;
		    st->video_time_acced = acced;
		    st->time_accumulated = acced+1;
		    st->starting_time = 1;
		    accum->spts->video->warp_start = 0;
		    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
				"Warping of Video Completed for Stream %d",id->stream_id);
		}
	    }
	    if(st->time_accumulated-st->starting_time >=
	       (accum->spts->video->video_ts_bias+accum->key_frame_interval*90) && !accum->spts->video->ts_warped){
		
		if(0){//len > 1316){
			//DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,"--------------");
			printf("------------------\n");
			for(uint32_t cnti = i; cnti < n_ts_pkts; cnti++){
				//DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,
				printf("pkt# %u  framestart %u Type %s pid#%u pts %u\n",
				cnti, pkt_tbl[cnti].frame_start,
				convert_slicetype_to_string(pkt_tbl[cnti].frame_type),
				pkt_tbl[cnti].pid, pkt_tbl[cnti].pts);
			}
			//DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,"--------------");
			//printf("------------------\n");
			new_data = 0;
		}
#if defined(MFP_BUGFIX_8460)
		slice_type_e ft;
#else
		int32_t ft;
#endif
		B_(printf("sess %d stream %d, video chunk creation at %u\n",id->sess_id, id->stream_id,st->time_accumulated));
		if(!video_ts_rollover_pt)
		    st->video_time_acced = st->time_accumulated- st->starting_time;
		if(!accum->spts->video->video_bias_chkd){
		    accum->spts->video->video_bias_chkd = 1;
		    accum->spts->video->video_ts_bias =
			st->video_time_acced - (accum->key_frame_interval*90);
		}
#if defined(MFP_BUGFIX_8460)
		uint32_t frame_pkt_num = 0;
		ft = find_frame_type(n_ts_pkts,i,accum->spts->mux.video_pid,pkt_tbl,&frame_pkt_num);
		if(ft != slice_type_i){
		    /*                    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
                                "Sess[%d] Stream[%d]: Key Frame Not Found"
                                , id->sess_id, id->stream_id);*/
		    if(n_ts_pkts < 15){
		    	//staggered store only once
			    accum->prev_data_ptr = data;
			    accum->prev_len = len;
			    accum->prev_pktend = i;
			    st->num_of_pkts--; //remove the last entry from stream buffer
			    accum->data_pos += len;
			    if(accum->data_pos >=(accum->data_size - MAX_UDP_SIZE)){
				accum->data_pos = 0;
				//DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE, 
				//	"Data buffer wrap around in StaggerPath\n");
				//printf("sess %u strm %u Data buffer wrap around in StaggerPath\n",
				//	id->sess_id, id->stream_id);
			    }			    
			    goto procDataRet;
		    }
		    //NKN_ASSERT(n_ts_pkts < 15);
                }
#else
		mfu_converter_get_timestamp((uint8_t *)(data+(i * 188)),&ft,
					    &pid, NULL, NULL, &handle_mpeg4AVC);
		if(ft!=1){
		    ;
                }
				
#endif		
		else{

 		    //DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,"FrameType Detected %s",
		    //	convert_slicetype_to_string(ft));
		   	/*	   
		    DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,
		    "sess %d stream %d, video chunk creation at %u",
		    id->sess_id, id->stream_id,
		    st->time_accumulated - st->starting_time);
		    */

		    //NKN_ASSERT((st->time_accumulated - st->starting_time) == 180180);
		    
		    stream_buffer_full(st, &accum->spts->video->write_pos,accum->spts->video->mfp_max_buf_depth);
		    rv = update_stream_buff(&accum->spts->video->stream[accum->spts->video->write_pos], accum->data_pos+(i*188),pkt_time);
		    if (rv == MFP_ACC_BUF_FULL) {
			DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE,
				    "Sess[%d] Stream [%d] Buffer Overrun, Attempting Resync", id \
				    ->sess_id, id->stream_id);
			return go_to_resync(pub_ctx,accum,syn,id);
		    }
		    accum->spts->video->num_buffs_pending_read++;
		}
	    }
	}	
	/*Acc and Check Audio Stream 1*/
	if(pid == accum->spts->mux.audio_pid1 && accum->spts->mux.audio1_in_sync){
            stream_buffer_data_t *st = NULL;
            int8_t  audio_just_warped=0;
            if(accum->spts->audio1->just_in_sync){
                if(i<accum->spts->audio1->first_sync_id)
                    continue;
		accum->spts->audio1->linear_buff->first_after_sync = 1;
                accum->spts->audio1->just_in_sync = 0;
                accum->spts->audio1->global_time_accumulated = pkt_time;
            }
            /*Update Stream Buffer-linear*/
            update_linear_stream_buff(accum->spts->audio1->linear_buff,
				      pkt_time,
				      accum->data_pos+(i*188));
	}

	/*Acc and Check Audio Stream 2*/
	if(pid == accum->spts->mux.audio_pid2 && accum->spts->mux.audio2_in_sync){
	    stream_buffer_data_t *st = NULL;
            int8_t  audio_just_warped=0;
            if(accum->spts->audio2->just_in_sync){
                if(i<accum->spts->audio1->first_sync_id)
                    continue;
                accum->spts->audio2->just_in_sync = 0;
                accum->spts->audio2->global_time_accumulated = pkt_time;
            }
            /*Update Stream Buffer-linear*/
            update_linear_stream_buff(accum->spts->audio2->linear_buff,
				      pkt_time,
				      accum->data_pos+(i*188));
	    
	}
    }

    if(accum->spts->video->num_buffs_pending_read > 0){
	stream_buffer_data_t* vid;//,*aud1,*aud2;
	accum_audio_prop_t *acc_aud;
	int32_t audio_duration_fetch;
	for(;;){
            next_mfu_read_index = (accum->last_read_index + 1) % accum->spts->video->\
		mfp_max_buf_depth;
	    vid = &accum->spts->video->stream[next_mfu_read_index];
	    if((vid->pending_read == MFP_ACC_BUF_PENDING_READ)&&((!accum->spts->mux.audio_pid1)||(accum->spts->mux.audio_pid1 && accum->spts->mux.audio1_in_sync))){
		if(accum->spts->mux.audio_pid1){
		    audio_duration_fetch = vid->video_time_acced;
		    if(id->ts2mfu_cont.audio_sample_duration){
			if(id->ts2mfu_cont.tot_aud_vid_diff > id->ts2mfu_cont.audio_sample_duration){
			    uint32_t factor = id->ts2mfu_cont.tot_aud_vid_diff/id->ts2mfu_cont.audio_sample_duration;
			    audio_duration_fetch = vid->video_time_acced - factor*id->ts2mfu_cont.audio_sample_duration;
			}
		    }

		    ret = get_audio_data(accum->spts->audio1->linear_buff,
					 (uint32_t)audio_duration_fetch,
					 &accum->spts->audio1->linear_buff->acc_aud_buf[accum->spts->audio1->linear_buff->last_read_acc_buf]);
		    if(ret == 1){
			/*Audio data can be acced*/
                        DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
                                    "Sess %d Stream %d apple_MFUseqnum %u, SL_mfuseqnum %u",
                                    id->sess_id, id->stream_id, get_ms_accum_seq_num(accum),
                                    get_sl_accum_seq_num(accum));
			acc_aud = &accum->spts->audio1->linear_buff->acc_aud_buf[accum->spts->audio1->linear_buff->last_read_acc_buf];
			accum->spts->audio1->linear_buff->last_read_acc_buf++;
			(accum->spts->audio1->linear_buff->last_read_acc_buf)%=(accum->spts->audio1->linear_buff->tot_acc_aud_bufs-1);
			accum->spts->audio1->linear_buff->audio_time_acced = audio_duration_fetch;
		    }
		    else if(ret == AUD_DATA_JUNK){
                        DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE,
                            "Sess %d Stream %d Error in audio duration acced",
                            id->sess_id, id->stream_id);
                        return go_to_resync(pub_ctx,accum,syn,id); 
                    } else
                            break;

		}

		accum->stats.tot_video_duration += (vid->video_time_acced/ 90);
		if(accum->spts->mux.audio_pid1)
		    accum->stats.tot_audio_duration_strm1 += (accum->spts->audio1->linear_buff->audio_time_acced/90);
#ifdef BZ7998
		if(glob_init_av_bias_flag){
		    uint32_t audio_time_acced = accum->spts->audio1->linear_buff->audio_time_acced;
		    if((accum->spts->mux.audio_pid1) && (accum->spts->mux.video_pid)){
			if(accum->spts->video->sync_time !=0  && accum->spts->audio1->sync_time!=0){
			    if((int32_t)accum->spts->video->sync_time-((int32_t)accum->spts->audio1->sync_time)>0){
				accum->spts->audio1->linear_buff->audio_time_acced+=(int32_t)accum->spts->video->sync_time-((int32_t)accum->spts->audio1->sync_time);
				DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,"Initial Audio added by %d for Stream %d\n ",(int32_t)accum->spts->video->sync_time-((int32_t)accum->spts->audio1->sync_time),id->stream_id);
				if((int32_t)accum->spts->video->sync_time-((int32_t)accum->spts->audio1->sync_time) > 15000){
				    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,"Go back  to Sync as diff b/w Audio & video sync times > 15000");
				    return go_to_resync(pub_ctx,accum,syn,id);
				}
			    }
			    else{
				vid->video_time_acced+=(int32_t)accum->spts->audio1->sync_time-(int32_t)accum->spts->video->sync_time;
				DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,"Initial Video added by %d for Stream %d\n ",(int32_t)accum->spts->audio1->sync_time-(int32_t)accum->spts->video->sync_time,id->stream_id);
				if((int32_t)accum->spts->audio1->sync_time-(int32_t)accum->spts->video->sync_time > 15000){
				    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,"Go back  to Sync as diff b/w Audio & video sync times > 15000");
				return go_to_resync(pub_ctx,accum,syn,id);
				}
			    }
			    accum->spts->audio1->sync_time = accum->spts->video->sync_time = 0;
			}
		    }
		}
#endif
		memset(&(id->ts2mfu_cont), 0, 4*sizeof(uint32_t));
		if((accum->spts->mux.audio_pid1) && (accum->spts->mux.video_pid)){
		    call_converter(&mfu_data,
				   accum,
				   vid->pkt_offsets, vid->num_of_pkts,
				   acc_aud->accum_pkt_offsets, acc_aud->num_aud_pkts,
				   &(id->ts2mfu_cont),
				   accum->spts->audio1->linear_buff->audio_time_acced,
				   vid->video_time_acced,
				   vid->time_accumulated);
		}else if((accum->spts->mux.audio_pid1 == 0) && (accum->spts->mux.video_pid)) {
		    call_converter(&mfu_data,
				   accum,
				   vid->pkt_offsets, vid->num_of_pkts,
				   NULL, 0,
				   &(id->ts2mfu_cont),
				   0,
				   vid->video_time_acced,
				   vid->time_accumulated);
		}
		

#if defined(INC_SEGD) 
		if(mfu_data!=NULL){
		    int32_t ret_merge_ms =1, ret_merge_ss=1, ret_merge_ze=1;
		    int32_t retdp;
		    ts2mfu_ref_cnt_fmtr_t* ts_ref_cnt_fmtr;
		    ts_ref_cnt_fmtr = &accum->ts2mfu_desc_ref->ts2mfu_ref_cnt[accum->ts2mfu_desc_ref->write_pos];
		    ts_ref_cnt_fmtr->ts2mfuptr = id->ts2mfu_cont.ts2mfu_desc;
		    accum->ts2mfu_desc_ref->write_pos++;
		    if(accum->ts2mfu_desc_ref->write_pos==accum->ts2mfu_desc_ref->max_pos)
			assert(0);
		    accum->num_resyncs = 0;
		    //DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,"Session %s Stream %d Video time %u",pub_ctx->name,id->stream_id,vid->video_time_acced);

		    if (pub_ctx->ms_parm.status == FMTR_ON){
			mfu_data_t* out_mfu_data;
			uint32_t j;
			ret_merge_ms = mfp_live_accum_mfus(mfu_data,
							   accum->acc_ms_fmtr->fmtr,
							   id->ts2mfu_cont.ts2mfu_desc,
							   vid->video_time_acced,
							   &out_mfu_data);
			ts_ref_cnt_fmtr->num_used|=APPLE_MASK;
			if(ret_merge_ms){
#if 0
			    {
				FILE *fid;
				char fname[50]={'\0'};
				mfu_data_t* mfu;
				uint32_t len11;
				len11 =   snprintf(NULL,0,"OMFU_sess%s_st%d_apple.ts",(char*)pub_ctx->name,id->stream_id);
				snprintf(fname,len11+1,"OMFU_sess%s_st%d_apple.ts",(char*)pub_ctx->name,id->stream_id);
				fid = fopen(fname, "wb");
				fwrite(out_mfu_data->data,out_mfu_data->data_size,1,fid);
				fclose(fid);
				frag_no2++;
			    }
#endif
			    release_ts2mfu_desc_ref(accum->ts2mfu_desc_ref, APPLE_UNMASK);

			    uint32_t tot_video_duration = calc_video_duration((void *)out_mfu_data);
			    //NKN_ASSERT(((fmtr_mfu_list_t *)accum->acc_ms_fmtr->fmtr)->duration_acced
			    //	== tot_video_duration);

			    enterDataPair(id, accum->acc_ms_fmtr->seq_num, 
			    	tot_video_duration/*((fmtr_mfu_list_t *)accum->acc_ms_fmtr->fmtr)->duration_acced*/, 
			    	APPLE_FMTR);
			    retdp = checkDataPair(id, accum->acc_ms_fmtr->seq_num, 
			    	tot_video_duration/*((fmtr_mfu_list_t *)accum->acc_ms_fmtr->fmtr)->duration_acced*/, 
			    	APPLE_FMTR);
			    clear_fmtr_mfu_list(accum->acc_ms_fmtr->fmtr);
			    if( retdp == -1){
				    DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, 
				    	"sess %u strm %u uniform chunk duration check failed",
				    	id->sess_id, id->stream_id);
				    	printf("sess %u strm %u uniform chunk duration check failed\n",
				    	id->sess_id, id->stream_id);
				    return go_to_resync(pub_ctx,accum,syn,id);
			    }
			    	

			    syn->ms_last_mfu_seq_num[id->stream_id] =  accum->acc_ms_fmtr->seq_num;//accum->mfu_seq_num;
			    ref_count_mem_t *ref_cont = NULL;
			    ref_cont = createRefCountedContainer(out_mfu_data,
								 destroyMfuData);
			    apple_fmtr_hold_encap_ctxt(id->sess_id);
			    ref_cont->hold_ref_cont(ref_cont);
			    mfpTaskFunc handler = (void*)mfubox_ts;
			    fruit_fmt_input_t *fmt_input = NULL;
			    fmt_input =					\
				nkn_calloc_type(1, sizeof(fruit_fmt_input_t),
					    mod_vpe_mfp_fruit_fmt_input_t);
			    if (!fmt_input) {
				DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE,
					   "not enough memory: %d",
					   -E_MFP_NO_MEM);
				assert(0);
			    }
			    fmt_input->state = FRUIT_FMT_CONVERT;
			    fmt_input->mfu_seq_num = accum->acc_ms_fmtr->seq_num;//accum->mfu_seq_num;
			    fmt_input->sioc = pub_ctx->ms_parm.sioc;
			    fmt_input->streams_per_encap = 
				    pub_ctx->streams_per_encaps[0];
			    if(accum->refresh_ms_fmtr){
				fmt_input->fmt_refresh = 1;
				accum->refresh_ms_fmtr = 0;
				DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,
					   "Refreshing Apple Formatter");
			    }

			    //increment the counter
			    *accum->stats.apple_fmtr_num_active_tasks += 1;
			    if(glob_slow_strm_HA_enable_flag){
				    if(VPE_ERROR == slow_strm_HA(id, pub_ctx)){
					ret = -2;
					stream_parm->stream_state = STRM_DEAD;
					DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE, 
					"Sess id %u Stream id %u - Identified as slow stream- Dropping it", 
					id->sess_id, id->stream_id);
				    	goto procDataRet;
				    }
			    }


			    
			    //			    printf("Accum stats for session %u stream %u Sent Seq Num = %u has value %u\n",id->sess_id, id->stream_id,fmt_input->mfu_seq_num ,*accum->stats.apple_fmtr_num_active_tasks );
			    if(VPE_ERROR == accum_validate_msfmtr_taskcnt(
			    	id, pub_ctx->name, accum->key_frame_interval, 
			    	*accum->stats.apple_fmtr_num_active_tasks)){
				ret = -1;
				NKN_ASSERT(0);
				DBG_MFPLOG("TASK_APPLE", MSG, MOD_MFPLIVE, "Task Limit Exceeded:  Sess id %u Stream id %u Sent Seq Num = %u Number of tasks %u", id->sess_id, id->stream_id, fmt_input->mfu_seq_num, *accum->stats.apple_fmtr_num_active_tasks);

			    	goto procDataRet;
			    }
			    
			    fmt_input->active_task_cntr = accum->stats.apple_fmtr_num_active_tasks;
			    out_mfu_data->fruit_fmtr_ctx = (void*)fmt_input;
			    DBG_MFPLOG("TASK_APPLE", MSG, MOD_MFPLIVE, "ACC calling rtsched Sess id %u Stream id %u Sent Seq Num = %u", id->sess_id, id->stream_id, fmt_input->mfu_seq_num);

#if 0
			    {
				struct timeval st,en;
				DBG_MFPLOG("TASK_APPLE", MSG, MOD_MFPLIVE, "ACC calling rtsched Sess id %u Stream id %u Sent Seq Num = %u", id->sess_id, id->stream_id, fmt_input->mfu_seq_num);
				gettimeofday(&st, NULL);
				mfpRtscheduleDelayedTask(15000, handler,
							 (void*)ref_cont);
				gettimeofday(&en, NULL);
				int32_t idle_time_ms =	diffTimevalToMs(&en, &st);
				DBG_MFPLOG("TASK_APPLE_TIME", MSG, MOD_MFPLIVE, "ACC rtsched time Sess id %u Stream id %u Sent Seq Num = %u Time taken = %d", id->sess_id, id->stream_id, fmt_input->mfu_seq_num, idle_time_ms);
				if(idle_time_ms > 1000)
				    DBG_MFPLOG("TASK_APPLE_TIME_ALARM", MSG, MOD_MFPLIVE, "ACC rtsched time Sess id %u Stream id %u Sent Seq Num = %u Time taken = %d", id->sess_id, id->stream_id, fmt_input->mfu_seq_num, idle_time_ms);

			    }
#endif
#if 0
			    DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,
			       "sess %u strm %u post tasks apple fmtr, seqnum %u",
			       id->sess_id, id->stream_id, fmt_input->mfu_seq_num);
#endif
			    //mfpRtscheduleDelayedTask(15000, handler, (void*)ref_cont);
			    taskHandler tp_handler = (taskHandler)mfubox_ts;
			    thread_pool_task_t	* t_task = 
				newThreadPoolTask(tp_handler, ref_cont, NULL);
   			    apple_fmtr_task_processor->add_work_item(apple_fmtr_task_processor, t_task); 			    
			    accum->acc_ms_fmtr->seq_num++;
			    accum->stats.tot_proc_ms_chunks = accum->acc_ms_fmtr->seq_num;
			}
		    } //ms
		    if (pub_ctx->ss_parm.status == FMTR_ON){
                        mfu_data_t* out_mfu_data;
			ret_merge_ss = mfp_live_accum_mfus(mfu_data,
                                                           accum->acc_ss_fmtr->fmtr,
                                                           id->ts2mfu_cont.ts2mfu_desc,
                                                           vid->video_time_acced,
                                                           &out_mfu_data);

			ts_ref_cnt_fmtr->num_used|=MS_MASK;
			if(ret_merge_ss){
#if 0
                            {
                                FILE *fid;
                                char fname[50]={'\0'};
                                mfu_data_t* mfu;
                                uint32_t len11;
                                len11 =   snprintf(NULL,0,"OMFU_sess%s_st%d_sl.ts",(char*)pub_ctx->name,id->stream_id);
                                snprintf(fname,len11+1,"OMFU_sess%s_st%d_sl.ts",(char*)pub_ctx->name,id->stream_id);
                                fid = fopen(fname, "wb");
                                fwrite(out_mfu_data->data,out_mfu_data->data_size,1,fid);
                                fclose(fid);
                                frag_no2++;
                            }
#endif


			    release_ts2mfu_desc_ref(accum->ts2mfu_desc_ref, MS_UNMASK);

			    uint32_t tot_video_duration = calc_video_duration((void *)out_mfu_data);
			    //NKN_ASSERT(((fmtr_mfu_list_t *)accum->acc_ss_fmtr->fmtr)->duration_acced
			    //	== tot_video_duration);

			    enterDataPair(id, accum->acc_ss_fmtr->seq_num, 
			    	tot_video_duration/*((fmtr_mfu_list_t *)accum->acc_ss_fmtr->fmtr)->duration_acced*/,
			    	SILVERLIGHT_FMTR);
			    retdp = checkDataPair(id, accum->acc_ss_fmtr->seq_num, 
			    	tot_video_duration/*((fmtr_mfu_list_t *)accum->acc_ss_fmtr->fmtr)->duration_acced*/,
			    	SILVERLIGHT_FMTR);
			     clear_fmtr_mfu_list(accum->acc_ss_fmtr->fmtr);
			    if( retdp == -1){
				    DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, 
				    	"sess %u strm %u uniform chunk duration check failed",
				    	id->sess_id, id->stream_id);
				    	printf("sess %u strm %u uniform chunk duration check failed\n",
				    	id->sess_id, id->stream_id);
				    return go_to_resync(pub_ctx,accum,syn,id);
			    }
			    
			    syn->ss_last_mfu_seq_num[id->stream_id] =  accum->acc_ss_fmtr->seq_num;
			    ref_count_mem_t *ref_cont = NULL;
			    ref_cont = createRefCountedContainer(out_mfu_data,
								 destroyMfuData);
			    ref_cont->hold_ref_cont(ref_cont);
			    mfp_publ_t* pctx_tmp = live_state_cont->acquire_ctx(live_state_cont, id->sess_id);
			    uint32_t data_id = pctx_tmp->sess_chunk_ctr;
			    pctx_tmp->sess_chunk_ctr++;
			    live_state_cont->release_ctx(live_state_cont, id->sess_id);
			    sl_fmtr_t* fmt = (sl_fmtr_t*)(pub_ctx->sl_publ_fmtr);
			    out_mfu_data->sl_fmtr_ctx = fmt;
			    out_mfu_data->data_id = data_id;
			    mfpTaskFunc handler = pctx_tmp->sl_sync_ser->serial_in_hdlr;
			    ser_task_t* task = create_ser_sync_fmtr_task(pctx_tmp->sl_sync_ser,
									 data_id, id->stream_id, 
									 /*accum->mfu_seq_num,*/
									 accum->acc_ss_fmtr->seq_num,
									 slTaskInterface, ref_cont);

			    thread_pool_task_t* t_task = newThreadPoolTask(handler,
									   task, NULL);
			    task_processor->add_work_item(task_processor, t_task);
			    if (task_processor->get_queue_fill_len(task_processor) > 100)
				DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
					    "Task processor queue length: %d\n",
					    task_processor->get_queue_fill_len(task_processor));
			    
                            if (task_processor->get_queue_fill_len(task_processor) > glob_max_sl_queue_items){
                                DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,"Task processor queue length exceed threshold: %d\n",
                                            task_processor->get_queue_fill_len(task_processor));
				ret = -1;
				goto procDataRet;
			    }

			    accum->acc_ss_fmtr->seq_num++;
			    accum->stats.tot_proc_ss_chunks = accum->acc_ss_fmtr->seq_num;
			}
		    } //ss 

                    accum->mfu_seq_num++;
		    /*Clear mfu_data*/
		    //		    delete_merge_mfu(id->ts2mfu_cont.ts2mfu_desc);
		    release_ts2mfu_desc(accum->ts2mfu_desc_ref);
		    clear_mfu_data(mfu_data);
		}
#else		 
		if(mfu_data!=NULL){   
		    syn->last_mfu_seq_num[id->stream_id] = accum->mfu_seq_num; 
		    accum->num_resyncs = 0;
		    ref_count_mem_t *ref_cont = NULL;
		    ref_cont = createRefCountedContainer(mfu_data,
							 destroyMfuData);
		    if (pub_ctx->ms_parm.status == FMTR_ON) {
			ref_cont->hold_ref_cont(ref_cont);
			apple_fmtr_hold_encap_ctxt(id->sess_id);
		    }
		    
		    if (pub_ctx->ss_parm.status == FMTR_ON) {
			ref_cont->hold_ref_cont(ref_cont);
		    }
		    
		    if (pub_ctx->ms_parm.status == FMTR_ON) {
			mfpTaskFunc handler = (void*)mfubox_ts;
			fruit_fmt_input_t *fmt_input = NULL;
			fmt_input =					\
			    nkn_calloc_type(1, sizeof(fruit_fmt_input_t),
					mod_vpe_mfp_fruit_fmt_input_t);
			if (!fmt_input) {
			    DBG_MFPLOG(pub_ctx->name, ERROR, MOD_MFPLIVE,
				       "not enough memory: %d",
				       -E_MFP_NO_MEM);
			    assert(0);
			}
			fmt_input->state = FRUIT_FMT_CONVERT;
			fmt_input->mfu_seq_num = accum->mfu_seq_num;
			//+pub_ctx->ms_parm.segment_start_idx;
			fmt_input->sioc = pub_ctx->ms_parm.sioc;
			fmt_input->streams_per_encap = 
				pub_ctx->streams_per_encaps[0];
			if(accum->refresh_ms_fmtr){
			    fmt_input->fmt_refresh = 1;
			    accum->refresh_ms_fmtr = 0;
			    DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,
				       "Refreshing Apple Formatter");
			}
			
			//increment the counter
			*accum->stats.apple_fmtr_num_active_tasks += 1;

			if(VPE_ERROR == accum_validate_msfmtr_taskcnt(
			    	id, pub_ctx->name, accum->key_frame_interval, 
			    	*accum->stats.apple_fmtr_num_active_tasks)){
				ret = -1;
			    	goto procDataRet;
			}
			    
			fmt_input->active_task_cntr = accum->stats.apple_fmtr_num_active_tasks;
			mfu_data->fruit_fmtr_ctx = (void*)fmt_input;
			//mfpRtscheduleDelayedTask(15000, handler,
			//			 (void*)ref_cont);
			taskHandler tp_handler = (taskHandler)mfubox_ts;
			thread_pool_task_t* t_task = 
				newThreadPoolTask(tp_handler, ref_cont, NULL);
			apple_fmtr_task_processor->add_work_item(apple_fmtr_task_processor, t_task); 			
			/**
			 * All network events for a particular session
			 * (accum ctx) is guaranteed to be atomic; we
			 * can safely increment here
			 */
		    }
		    mfp_publ_t* pctx_tmp = live_state_cont->acquire_ctx(live_state_cont, id->sess_id);
		    uint32_t data_id = pctx_tmp->sess_chunk_ctr;
		    pctx_tmp->sess_chunk_ctr++;
		    live_state_cont->release_ctx(live_state_cont, id->sess_id);
		    if (pub_ctx->ss_parm.status == FMTR_ON) {
			sl_fmtr_t* fmt = (sl_fmtr_t*)(pub_ctx->sl_publ_fmtr);
			mfu_data->sl_fmtr_ctx = fmt;
			mfu_data->data_id = data_id;
			mfpTaskFunc handler = pctx_tmp->sl_sync_ser->serial_in_hdlr;
			ser_task_t* task = create_ser_sync_fmtr_task(pctx_tmp->sl_sync_ser,
								     data_id, id->stream_id, accum->mfu_seq_num,
								     slTaskInterface, ref_cont);
			
			thread_pool_task_t* t_task = newThreadPoolTask(handler,
								       task, NULL);
			task_processor->add_work_item(task_processor, t_task); 
			if (task_processor->get_queue_fill_len(task_processor) > 100)
			    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
					"Task processor queue length: %d\n",
					task_processor->get_queue_fill_len(task_processor));
		    }
		    accum->mfu_seq_num++;
		}
#endif
		else {
		    DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "MFU FORMED is NULL\n");
		    //assert(0);
		    return go_to_resync(pub_ctx,accum,syn,id);
		}
		//#endif
		if(accum->spts->mux.audio_pid1) {
		    rv = adjust_audio_pkts(&(id->ts2mfu_cont),
					   accum->spts->audio1->linear_buff,		  
					   accum->data);
		    
		    if(rv == -1){
			DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "Adjust Audio Failed: Back to Resync");
			return go_to_resync(pub_ctx,accum,syn,id);
		    }
		}
	    done:
		accum->spts->video->num_buffs_pending_read--;
		accum->last_read_index = next_mfu_read_index;
		invalidate_stream_buffer(vid);
		if(accum->spts->mux.audio_pid1){
		    acc_aud->num_aud_pkts = 0;
		}
	    }
	    else
		break;
	}
    }
    
    //stop the data buffer around before 2*7*188 and MAX_UDP_SIZE
    //2*7*188 is related to ts packet buffering needed for streams with i-slice is split
    //across packets - found in Imagine streams
    if(accum->data_pos+len>=(accum->data_size-MAX_UDP_SIZE - (2*NUM_PKTS_IN_NETWORK_CHUNK*BYTES_IN_PACKET))){
	//printf("sess %u strm %u Data buffer wrap around\n",id->sess_id, id->stream_id);
	//printf("datapos %u len %u datasize %u diff %u\n", accum->data_pos, len, accum->data_size,
	//	accum->data_size-accum->data_pos);
	accum->data_pos = 0;
	//	printf("Data buffer wrap around\n");
    }
    else
	accum->data_pos += len;
    
 procDataRet:
#if defined(MFP_BUGFIX_8460)
    if(pkt_tbl){
	free(pkt_tbl);
	pkt_tbl = NULL;
    }	
#endif
    if(ret < 0){
	//if failed, pull down all streams
	DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "Killing a stream Session %u Stream %u",id->sess_id, id->stream_id);
	if(!glob_enable_stream_ha || ret == -1){
	    //incase of error in one or more streams, kill all streams
	    //this is non HA failover case
	    syn->kill_all_streams = 1;
	}
	if(glob_enable_stream_ha && ret == -2){
	    pub_ctx->streams_per_encaps[0]--;	
	    //            syn->ref_accum_cntr 
	    pub_ctx->encaps[0]->syncp.ref_accum_cntr= pub_ctx->streams_per_encaps[0];
	}
	if(pub_ctx->ms_parm.status == FMTR_ON || (pub_ctx->ms_parm.status < 0)) {
	    if(glob_enable_stream_ha && (ret == -2))
	        register_stream_failure(id->sess_id, id->stream_id);
	    else
	        register_stream_dead(id->sess_id, id->stream_id);
	}
	if(pub_ctx->ss_parm.status == FMTR_ON || (pub_ctx->ss_parm.status < 0)) {
	    if(glob_enable_stream_ha  && ret == -2){
	    	if(pub_ctx->sl_sync_ser){
		    pub_ctx->sl_sync_ser->streamha_error_hdlr(
			pub_ctx->sl_sync_ser, id->stream_id);
	    	}
		if(pub_ctx->sl_publ_fmtr){
		    pub_ctx->sl_publ_fmtr->set_error_state(
		    	pub_ctx->sl_publ_fmtr, id->stream_id);
		}
	    }
	    else{
	    	if(pub_ctx->sl_sync_ser){
		    pub_ctx->sl_sync_ser->error_hdlr(pub_ctx->sl_sync_ser);
	    	}
	    }
	}
    }
    return (ret);
}



static void clear_mfu_data(mfu_data_t* mfu_data){
    if(mfu_data->data){
	free(mfu_data->data);
	mfu_data->data = NULL;
	free(mfu_data);
	mfu_data=NULL;
    }
    return;
}

#ifdef INC_SEGD
static void release_ts2mfu_desc(ts2mfu_desc_ref_t* ref){
    uint32_t j;
    for(j=0;j<ref->write_pos;j++){
        if(ref->ts2mfu_ref_cnt[j].num_used == 0){
	    if(ref->ts2mfu_ref_cnt[j].ts2mfuptr!=NULL){
		DBG_MFPLOG ("MFUDEL", MSG, MOD_MFPLIVE," Freeing  %p",ref->ts2mfu_ref_cnt[j].ts2mfuptr);
		delete_merge_mfu(ref->ts2mfu_ref_cnt[j].ts2mfuptr);
		ref->ts2mfu_ref_cnt[j].ts2mfuptr = NULL;
	    }
            if(j==ref->write_pos-1)
                ref->write_pos =0;
        }
    }
    return;
}





static void release_ts2mfu_desc_ref(ts2mfu_desc_ref_t* ref,
				    uint8_t mask){
    uint32_t j;
    for(j=0;j<ref->write_pos;j++){
	    ref->ts2mfu_ref_cnt[j].num_used&=mask;
    }
    return;
}


static void clear_all_ts2mfu_desc_ref(ts2mfu_desc_ref_t* ref){
    uint32_t j;
    for(j=0;j<ref->max_pos;j++){
	if(ref->ts2mfu_ref_cnt[j].num_used){
	    //                DBG_MFPLOG ("MFUDEL", MSG, MOD_MFPLIVE," BFreeing  %p",ref->ts2mfu_ref_cnt[j].ts2mfuptr);
	    if(ref->ts2mfu_ref_cnt[j].ts2mfuptr!=NULL){
		//  DBG_MFPLOG ("MFUDEL", MSG, MOD_MFPLIVE," AFreeing  %p",ref->ts2mfu_ref_cnt[j].ts2mfuptr);
		delete_merge_mfu(ref->ts2mfu_ref_cnt[j].ts2mfuptr);
		ref->ts2mfu_ref_cnt[j].ts2mfuptr = NULL;
	    }
	}
	ref->ts2mfu_ref_cnt[j].num_used = 0;
    }
    ref->write_pos =0;
    return;
}

#endif


/*---------------------------------------
Get 2 fold audio data:
 1. Revert if there is sufficient data
 2. Get audio samples and fill the
    alignment structure.
-----------------------------------------*/

static int32_t get_audio_data(stream_lin_buff_t* buff,
                              uint32_t duration,
			      accum_audio_prop_t* audio      
			      ){
    uint32_t j, time_acced = 0,k=0;
    pkt_prop_t *pkts = buff->pkts;
    if(buff->latest_acced_time - buff->last_read_time < duration)
        return WAIT_FOR_AUD_DATA;
    if(buff->first_after_sync &&  ((int32_t)(buff->latest_acced_time-buff->video_sync_time) < (int32_t)duration))
	return WAIT_FOR_AUD_DATA;
    j = (buff->last_read_indx);
    for(;;){
	if(buff->first_after_sync){
	    if((pkts+j)->pkt_time >= buff->video_sync_time){
		buff->first_after_sync = 0;
		duration -= (pkts+j)->pkt_time-buff->video_sync_time;
		if((int32_t)(duration)<=0)
			return AUD_DATA_JUNK;
		buff->last_read_time = (pkts+j)->pkt_time;
	    }
	    else{
		(pkts+j)->pkt_state = 0;
		j++;
		if(j%(buff->max_num_pkts) == 0){
		    j = 0;
		}
		continue;
	    }
	}

	audio->accum_pkt_offsets[audio->num_aud_pkts++] = (pkts+j)->pkt_offset;
	(pkts+j)->pkt_state = 0;
	buff->last_read_indx = j;
	if((pkts+j)->pkt_time){
	    if((pkts+j)->pkt_time - buff->last_read_time >= duration){
		/*printf("AudioDataAcced = %u, EndTime %u StartTime %u PktOffset = %u NumPkts =%u writePos = %u\n",
			(pkts+j)->pkt_time - buff->last_read_time, 
			(pkts+j)->pkt_time, buff->last_read_time,
			j, audio->num_aud_pkts, buff->write_pos);*/
	/*	printf("VideoAcced %u AudioAcced %u Diff %d\n", 
			duration, (pkts+j)->pkt_time - buff->last_read_time,
			(int32_t)duration - (int32_t)(pkts+j)->pkt_time + (int32_t)buff->last_read_time);*/
		buff->last_read_time = (pkts+j)->pkt_time;
		break;
	    }
	}
	j++;
	if(j%(buff->max_num_pkts) == 0){
	    j = 0;	
	}
    }
    return 1;
}



static int32_t  update_linear_stream_buff(stream_lin_buff_t* buff,
                                          uint32_t pkt_time,
					  uint32_t pos){
    pkt_prop_t *pkts = buff->pkts;
    if(pkts[buff->write_pos].pkt_state == 1){
        DBG_MFPLOG ("Linear buffer update", ERROR, MOD_MFPLIVE,
                    "Audio and Video lag too much beyond: %u seconds",MFP_MAX_AV_LAG)\
	    ;
        return MFP_ACC_BUF_FULL;
    }
    pkts[buff->write_pos].pkt_time = pkt_time;
    if(pkt_time!=0){
	int32_t diff_time;
	diff_time = (int32_t)(pkt_time- buff->latest_acced_time);
	if(diff_time<0 && buff->latest_acced_time != 0){
	    DBG_MFPLOG ("Linear buffer update", ERROR, MOD_MFPLIVE,
	    "Audio re-basing encountered - back to resync: %d seconds",diff_time);
     	    return MFP_ACC_BUF_FULL;
	}
        buff->latest_acced_time = pkt_time;
        if(!buff->last_read_time)
            buff->last_read_time = pkt_time;
    }
    pkts[buff->write_pos].pkt_state = 1;
    pkts[buff->write_pos].pkt_offset = pos;
    buff->write_pos++;
    buff->write_pos%=buff->max_num_pkts;
    return 0;
}


static int32_t go_to_resync(mfp_publ_t* pub_ctx,
			    accum_ts_t* accum,
			    sync_ctx_params_t *syn,
			    sess_stream_id_t* id)
{
    accum->stats.resync_count++;
    if(accum->num_resyncs < ACCUM_MAX_RESYNC){
	DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
		"Sess[%d] Stream[%d]:MFU formed is NULL: Return to SYNC", 
		id->sess_id, id->stream_id);
	syn->encap_in_sync = 0;
	syn->first_time_recd = 0;
	syn->kill_all_streams = 0;
	syn->stream_num = 0;
	syn->sync_search_started = 0;
	syn->sync_time = 0;
	syn->ref_accum_cntr -= 1;	
	refresh_accum_state(pub_ctx, accum);
	id->ts2mfu_cont.first_instance = 0;
	id->ts2mfu_cont.tot_aud_vid_diff = 0;
	memset(&id->ts2mfu_cont.bias,0,sizeof(audio_bias_t));
	accum->num_resyncs++;
	memset(&pub_ctx->sync_params, 0, sizeof(sync_params_t));
#ifdef INC_SEGD
	clear_all_ts2mfu_desc_ref(accum->ts2mfu_desc_ref);
	if(pub_ctx->ms_parm.status == FMTR_ON)
	    clear_fmtr_vars(accum->acc_ms_fmtr->fmtr);
	if(pub_ctx->ss_parm.status == FMTR_ON)
	    clear_fmtr_vars(accum->acc_ss_fmtr->fmtr);
#endif
	accum->prev_data_ptr = NULL;
	accum->prev_len = 0;
	accum->prev_pktend = 0;
	if(pub_ctx->ms_parm.status == FMTR_ON)
		resetDataPair(accum->acc_ms_fmtr->dproot);
	if(pub_ctx->ss_parm.status == FMTR_ON)
		resetDataPair(accum->acc_ss_fmtr->dproot);
	if(pub_ctx->fs_parm.status == FMTR_ON)
		resetDataPair(accum->acc_ze_fmtr->dproot);

	return 0;
    }
    else{
	DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE,
		"Sess[%d] Stream[%d]:Mfu formed is NULL.The maximum number of RESYNCs has been reached.Exiting now.", 
		id->sess_id, id->stream_id);
	syn->kill_all_streams = 1;
	if(pub_ctx->ms_parm.status == FMTR_ON){
	    register_stream_dead(id->sess_id, id->stream_id);
	}
	if(pub_ctx->ss_parm.status == FMTR_ON){
	    //	    slHandleResync(id->sess_id, id->stream_id, accum->mfu_seq_num);
	    pub_ctx->sl_sync_ser->error_hdlr(pub_ctx->sl_sync_ser);
	}
	mfp_publ_t* pctx_tmp = live_state_cont->acquire_ctx(
		live_state_cont, id->sess_id);		
	pctx_tmp->sess_chunk_ctr++;
	live_state_cont->release_ctx(live_state_cont, id->sess_id);	
	return -1;
    }
}



static void set_encaps_to_resync(mfp_publ_t* pub_ctx){
    uint32_t i;
    accum_ts_t* accum;
    stream_param_t *strm;
    uint32_t num_strms = calcTotalStrms(pub_ctx);
    for(i = 0 ; i < num_strms; i++){
    	strm = &(pub_ctx->stream_parm[i]);
	if(strm->stream_state == STRM_ACTIVE){
		accum = (accum_ts_t*)pub_ctx->accum_ctx[i];
		if(!accum)
		    continue;
		refresh_accum_state(pub_ctx, accum);
	}
    }
    return;
}


static int32_t checkSessionStreamState(sess_stream_id_t const* id) {

    uint32_t stream_id = id->stream_id;
    if (live_state_cont->get_flag(live_state_cont, id->sess_id) == -1)
	return -1;
    return 1;
}


static void invalidate_stream_buffer(stream_buffer_data_t* buf){
#ifdef MFP_MEM_OPT
    memset(buf->pkt_offsets,0,buf->mfp_ts_max_pkts);
#else
    memset(buf->pkt_offsets,0,MFP_TS_MAX_PKTS);
#endif
    buf->num_of_pkts = 0;
    buf->starting_time = 0;
    buf->time_accumulated = 0;
    buf->pending_read = MFP_ACC_BUF_INVALID;
}

#ifdef MFP_MEM_OPT
static void stream_buffer_full(stream_buffer_data_t* st,
                               uint32_t* write_pos,
                               uint32_t max_depth){
    st->pending_read = MFP_ACC_BUF_PENDING_READ;
    /*Copy the last packet*/
    st->num_of_pkts--;
    *write_pos=(*write_pos+1)%max_depth;
}


static void init_spts_stream_buffers(spts_st_ctx_t* spts,
                                     uint32_t kfi,
                                     uint32_t bitrate)
{
    uint32_t num_pkts, buff_depth,i,num_pkts_max_kfi, num_pkts_audio;
    double scle = (double)(glob_mfp_buff_size_scale)/10;
    num_pkts = (1024*scle*(bitrate + STD_TSBITSTREAMS_OVERHEAD)*kfi)/(188*8*1000);
    num_pkts_audio = (1024*(glob_max_audio_bitrate)*glob_mfp_audio_buff_time)/(188*8);
    buff_depth = (1000*glob_mfp_max_av_lag)/kfi;
    num_pkts_max_kfi = num_pkts*glob_max_kfi_tolerance;
    if(!spts->video && spts->mux.video_pid){
        spts->video = (stream_buffer_state_t*)nkn_calloc_type(1,sizeof(stream_buffer_state_t),mod_vpe_mfp_accum_ts);
        spts->video->mfp_max_buf_depth = buff_depth;
        spts->video->mfp_ts_max_pkts = num_pkts_max_kfi;
        spts->video->stream = (stream_buffer_data_t *)nkn_calloc_type(1,sizeof(stream_buffer_data_t)*buff_depth,mod_vpe_mfp_accum_ts);
        /*Now allaocate for each packet*/
        for(i=0;i<buff_depth;i++){
            spts->video->stream[i].pkt_offsets = (uint32_t*)nkn_calloc_type(1,sizeof(uint32_t)*num_pkts_max_kfi,mod_vpe_mfp_accum_ts);
            spts->video->stream[i].mfp_ts_max_pkts = num_pkts_max_kfi;//num_pkts;
        }
    }
    if(!spts->audio1 && spts->mux.audio_pid1){
	uint32_t num_pkts_per_kfi;
        spts->audio1 = (stream_buffer_state_t*)nkn_calloc_type(1,sizeof(stream_buffer_state_t),mod_vpe_mfp_accum_ts);
        spts->audio1->mfp_max_buf_depth = AUDIO_BUFF_DEPTH;//buff_depth;
        spts->audio1->mfp_ts_max_pkts =  num_pkts_audio;//glob_mfp_audio_buff_time*num_pkts_max_kfi;
	spts->audio1->linear_buff = (stream_lin_buff_t *)nkn_calloc_type(1,sizeof(stream_lin_buff_t),
									 mod_vpe_mfp_accum_ts);
	spts->audio1->linear_buff->pkts = (pkt_prop_t*)nkn_calloc_type(1,sizeof(pkt_prop_t)*spts->audio1->mfp_ts_max_pkts, 
								       mod_vpe_mfp_accum_ts);
	spts->audio1->linear_buff->max_num_pkts = spts->audio1->mfp_ts_max_pkts;
	spts->audio1->linear_buff->acc_aud_buf = (accum_audio_prop_t*)nkn_calloc_type(1,sizeof(accum_audio_prop_t)*buff_depth,
										      mod_vpe_mfp_accum_ts);
	for(i=0;i<spts->audio1->mfp_max_buf_depth;i++){
	    spts->audio1->linear_buff->acc_aud_buf[i].accum_pkt_offsets = (uint32_t*)nkn_calloc_type(1,sizeof(uint32_t)*num_pkts_max_kfi,
											       mod_vpe_mfp_accum_ts);
	}
	spts->audio1->linear_buff->tot_acc_aud_bufs =spts->audio1->mfp_max_buf_depth;//buff_depth;
    }
    if(!spts->audio2 && spts->mux.audio_pid2){
	spts->audio2 = (stream_buffer_state_t*)nkn_calloc_type(1,sizeof(stream_buffer_state_t),mod_vpe_mfp_accum_ts);
        spts->audio2->mfp_max_buf_depth = buff_depth;
        spts->audio2->mfp_ts_max_pkts = num_pkts;
        spts->audio2->stream = (stream_buffer_data_t *)nkn_calloc_type(1,sizeof(stream_buffer_data_t)*buff_depth,
								       mod_vpe_mfp_accum_ts);
        spts->audio2->linear_buff = (stream_lin_buff_t *)nkn_calloc_type(1,sizeof(stream_lin_buff_t),
                                                                         mod_vpe_mfp_accum_ts);
        spts->audio2->linear_buff->pkts = (pkt_prop_t*)nkn_calloc_type(1,sizeof(pkt_prop_t)*buff_depth*num_pkts,
                                                                       mod_vpe_mfp_accum_ts);
        spts->audio2->linear_buff->max_num_pkts = buff_depth*num_pkts;
        spts->audio2->linear_buff->acc_aud_buf = (accum_audio_prop_t*)nkn_calloc_type(1,sizeof(accum_audio_prop_t)*buff_depth,
                                                                                      mod_vpe_mfp_accum_ts);
        for(i=0;i<buff_depth;i++){
            spts->audio2->linear_buff->acc_aud_buf[i].accum_pkt_offsets = (uint32_t*)nkn_calloc_type(1,sizeof(uint32_t)*num_pkts,
												     mod_vpe_mfp_accum_ts);
        }
    }
}


static void  init_buffer_state( buffer_state_t* buff,
                                uint32_t data_size,
                                uint32_t max_depth){
    uint32_t i;
    for(i=0;i<max_depth;i++){
        buff->data = (uint8_t*)nkn_malloc_type(data_size,mod_vpe_mfp_accum_ts);
        buff->data_size = 0;
        buff->buffer_len = data_size;
        buff->pending_read = buff->data_full = -1;
        buff->starting_time = buff->time_accumulated = 0;
    }
}



#else

static void stream_buffer_full(stream_buffer_data_t* st, uint32_t* write_pos){
    st->pending_read = MFP_ACC_BUF_PENDING_READ;
    /*Copy the last packet*/
    st->num_of_pkts--;
    *write_pos=(*write_pos+1)%MFP_MAX_ST_BUF_DEPTH;
}


static void init_spts_stream_buffers(spts_st_ctx_t* spts){
    if(!spts->video && spts->mux.video_pid){
	spts->video = (stream_buffer_state_t*)nkn_calloc_type(1,sizeof(stream_buffer_state_t),mod_vpe_mfp_accum_ts);
    }
    if(!spts->audio1 && spts->mux.audio_pid1){
	spts->audio1 = (stream_buffer_state_t*)nkn_calloc_type(1,sizeof(stream_buffer_state_t),mod_vpe_mfp_accum_ts);
    }
    if(!spts->audio2 && spts->mux.audio_pid2){
	spts->audio2 = (stream_buffer_state_t*)nkn_calloc_type(1,sizeof(stream_buffer_state_t),mod_vpe_mfp_accum_ts);
    }
}


static void del_spts_stream_buffers(spts_st_ctx_t* spts){
    if(spts->mux.video_pid){
	free(spts->video);
	spts->video = NULL;
    }
    if(spts->mux.audio_pid1){
	free(spts->audio1);
	spts->audio1 = NULL;
    }
    if(spts->mux.audio_pid2){
	free(spts->audio2);
	spts->audio2 = NULL;
    }
}


static void  init_buffer_state( buffer_state_t* buff,
	uint32_t data_size){
    int32_t i;
    for(i=0;i<MFP_MAX_ST_BUF_DEPTH;i++){
	buff->data = (uint8_t*)nkn_malloc_type(data_size,mod_vpe_mfp_accum_ts);
	buff->data_size = 0;
	buff->buffer_len = data_size;
	buff->pending_read = buff->data_full = -1;
	buff->starting_time = buff->time_accumulated = 0;
    }
}
#endif

static void parse_spts_pmt(uint8_t* data,uint32_t len,mux_st_elems_t *mux){
    int8_t ret;
    if(!mux->pmt_pid){
	mux->pmt_pid =  get_pmt_pid((uint8_t*)data,len);
	if(!mux->pmt_pid)
	    return;
    }
    if(!mux->pmt_parsed){
	ret = get_spts_pids((uint8_t*)data,len,
		&mux->video_pid,&mux->audio_pid1,
		&mux->audio_pid2,mux->pmt_pid);
	if(ret)
	    mux->pmt_parsed = 1;
    }
}

static int32_t adjust_audio_pkts(ts2mfu_cont_t *ts2mfu_cont,
                                 stream_lin_buff_t* lin_buff,
				 uint8_t *data)
{
    uint32_t next_index,start_pkt_pos,i;
    uint8_t *tmp_data;//, adaptation_field_len;
    if(!ts2mfu_cont->num_tspkts_retx){
	lin_buff->last_read_indx++;
	if(lin_buff->last_read_indx==lin_buff->max_num_pkts)
	    lin_buff->last_read_indx = 0;
	return 1;
    }

    for(i=1;i<ts2mfu_cont->num_tspkts_retx;i++){
	if(lin_buff->last_read_indx==0)
	    lin_buff->last_read_indx=lin_buff->max_num_pkts-1;
	else
	    lin_buff->last_read_indx--;
	lin_buff->pkts[lin_buff->last_read_indx].pkt_time = 0;
    }

    /*Calculate the offset*/
    start_pkt_pos = lin_buff->pkts[lin_buff->last_read_indx].pkt_offset;
    //max out last_pkt_end_offset to TS_PKT_SIZE -safety precaution
    ts2mfu_cont->last_pkt_end_offset = (ts2mfu_cont->last_pkt_end_offset > TS_PKT_SIZE)? TS_PKT_SIZE : ts2mfu_cont->last_pkt_end_offset;
    //ts2mfu_cont->last_pkt_start_offset = 4 ;//TS header
    /*Corrupt the packet returned.*/
    if(ts2mfu_cont->last_pkt_end_offset > ts2mfu_cont->last_pkt_start_offset){
	tmp_data = (uint8_t *)&data[start_pkt_pos] + ts2mfu_cont->last_pkt_start_offset - 1;
	//masking the adpation field  flag
	*tmp_data = (*tmp_data & 0xcf);
	memset(&data[start_pkt_pos] + ts2mfu_cont->last_pkt_start_offset,
		0x00, ts2mfu_cont->last_pkt_end_offset - ts2mfu_cont->last_pkt_start_offset);
	//unset payload start indicator flag
	uint8_t *tmp = &data[start_pkt_pos] + 1;
	*tmp = *tmp & 0xbf;
    }
    return 1;
}












/*=============================================================
Input:
@buf : Input buffer with number of packets
@len : Length of those packets.
@sync: sync parameters to be obtained.
Descprition:
Function to be called only when sync->encap_in_sync = 0 &&
sync->first_time_recd = 0.
Does the following:
.1 Checks pkts whether a valid sync point is present.
this would be any packet with a timestamp.
Output:
sync->first_time_recd would be modified
sync->sync_time - is also provided here

Note: Let all the time be in 90KHz clock
===============================================================*/


static void call_converter(
			   mfu_data_t** mfu_data,
			   accum_ts_t *accum,
			   uint32_t* mstr_pkts, uint32_t mstr_npkts,
			   uint32_t* slve_pkts, uint32_t slve_npkts,
			   ts2mfu_cont_t *ts2mfu_cont,
			   uint32_t audio_time,
			   uint32_t video_time,
			   uint32_t video_time_accumulated)
{


    mfub_t* mfu_hdr;
    mfu_hdr = &accum->mfu_hdr;


    if(glob_dump_cc_input){
	FILE *vfid;
	char fname[50]={'\0'};
	uint32_t len;
	uint32_t i;
	uint8_t* m_ts_data,* s_ts_data, *data;
	uint32_t msize,ssize;
	m_ts_data = ( uint8_t*)malloc(mstr_npkts*188);
	s_ts_data = ( uint8_t*)malloc(slve_npkts*188);
	data = accum->data;
	for(i=0;i<mstr_npkts;i++){
	    memcpy(m_ts_data + (i*188),
		    data + (mstr_pkts[i]),
		    188);
	}
	for(i=0;i<slve_npkts;i++){
	    memcpy(s_ts_data +  (i*188),
		    data + (slve_pkts[i]),
		    188);
	}

	msize = mstr_npkts*188;
	ssize = slve_npkts*188;

	int32_t rc = mkdir("debug/", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	if ((rc < 0) && (errno != EEXIST)) {
		printf("mkdir failed 1: \n");
	}


	len = 	snprintf(NULL,0,"debug/V_st%d_%d.ts",mfu_hdr->stream_id,frag_no1);
	snprintf(fname,len+1,"debug/V_st%d_%d.ts",mfu_hdr->stream_id,frag_no1);

	vfid = fopen(fname,"wb");
	fwrite(m_ts_data,msize,1,vfid);
	fclose(vfid);
	snprintf(fname,len+1,"debug/A_st%d_%d.ts",mfu_hdr->stream_id,frag_no1);
	vfid = fopen(fname,"wb");
	fwrite(s_ts_data,ssize,1,vfid);
	fclose(vfid);
	free(m_ts_data);
	free(s_ts_data);
	frag_no1++;
    }

    if (!mstr_npkts && !slve_npkts) {
	/* end of stream create a dummy MFU */
	*mfu_data = mfp_create_eos_mfu(&accum->mfu_hdr);
	return;
    }
    //check for delta in audio and video acc time
    if(0){
    	uint32_t tmp;
	tmp = video_time >> 2;
	if((audio_time > (video_time + tmp))||
		(audio_time < (video_time - tmp))){
		DBG_MFPLOG ("TS2MFU", MSG, MOD_MFPLIVE,
		"Audio (%u) and video(%u) chunk time differ more than allowed range: Return to SYNC", 
		audio_time, video_time);		
		*mfu_data = NULL;
		return;
	}   
	tmp = audio_time >> 2;
	if((video_time> (audio_time + tmp))||
		(video_time < (audio_time - tmp))){
		DBG_MFPLOG ("TS2MFU", MSG, MOD_MFPLIVE,
		"Audio (%u) and video(%u) chunk time differ more than allowed range: Return to SYNC", 
		audio_time, video_time);
		*mfu_data = NULL;
		return;
	}    	
	
    }
    
    accum->mfu_hdr.audio_duration = audio_time;
#ifdef VERSION1
    accum->mfu_hdr.video_duration = video_time;
#endif
#ifdef PRINT_MSG17
    printf("Frag = %u Atime= %uV Time = %u",frag_no1++,audio_time, video_time);
    tot_aud_acc+= audio_time;
    tot_vid_acc+= video_time;
    printf("Tot Aud = %u Tot Vid = %u\n",tot_aud_acc, tot_vid_acc);
#endif
    ts2mfu_cont->video_duration = video_time;
    ts2mfu_cont->video_time_accumulated = video_time_accumulated;
#ifdef OFFSET_PKT_TIME
    ts2mfu_cont->video_time_accumulated -= PKT_TIME_OFSET;
#endif    
    accum->stats.acc_video_bytes += mstr_npkts * TS_PKT_SIZE;
    accum->stats.acc_audio_1_bytes += slve_npkts * TS_PKT_SIZE;
    if(accum->stats.tot_video_duration)
	accum->stats.avg_video_bitrate = (uint32_t)(accum->stats.acc_video_bytes/accum->stats.tot_video_duration)* 1000 * 8;
    if(accum->stats.tot_audio_duration_strm1)
	accum->stats.avg_audio_bitrate = (uint32_t)(accum->stats.acc_audio_1_bytes/accum->stats.tot_audio_duration_strm1)* 1000 * 8;


#if defined(INC_SEGD)
    ts2mfu_cont->store_ts2mfu_desc = 1;
#endif
    *mfu_data = mfp_ts2mfu(mstr_pkts, mstr_npkts,
			   slve_pkts, slve_npkts,
			   accum,
			   ts2mfu_cont);
    if(*mfu_data == VPE_ERROR){
	*mfu_data = NULL;
    }

#if 0
    {
	FILE *fid;
	char fname[50]={'\0'};
	mfu_data_t* mfu;
	uint32_t len;
	mfu = *mfu_data;
	len =   snprintf(NULL,0,"MFU_st%d_%d.ts",mfu_hdr->stream_id,frag_no1);
	snprintf(fname,len+1,"MFU_st%d_%d.ts",mfu_hdr->stream_id,frag_no1);
	fid = fopen(fname, "wb");
	fwrite(mfu->data,mfu->data_size,1,fid);
	fclose(fid);
	frag_no1++;
    }
#endif
}


static mfu_data_t *mfp_create_eos_mfu(mfub_t *mfu_hdr)
{

    mfu_data_t *mfu_data;
    mfub_t *mfub;
    size_t mfu_data_size;

    mfu_data = NULL;

    /* allocate for the mfu container */
    mfu_data = (mfu_data_t*) nkn_calloc_type(1,sizeof(mfu_data_t),
	    mod_vpe_mfp_ts2mfu_t);
    if (!mfu_data) {
	return NULL;
    }

    /* allocate for the mfu */
    mfu_data->data_size = mfu_data_size = MFU_FIXED_SIZE + sizeof(mfub_t);
    mfu_data->data = (uint8_t *)nkn_malloc_type(mfu_data_size,
	    mod_vpe_mfp_ts2mfu_t);
    if (!mfu_data->data) {
	return NULL;
    }

    /* setup the mfu header to be written as a mfu box; using
     * UINT64MAX since it is independent of endianess and enables
     * quick lookup
     */
    mfu_hdr->offset_vdat = 0xffffffffffffffff;
    mfu_hdr->offset_adat = 0xffffffffffffffff;
    mfu_hdr->mfu_size = mfu_data_size;

    /* write the mfu header */
    mfu_write_mfub_box(sizeof(mfub_t), mfu_hdr, mfu_data->data);

    return mfu_data;

}

static uint16_t get_pmt_pid(uint8_t* data,uint32_t len){

    uint32_t i, n_ts_pkts=0;
    uint16_t pid, pmt_pid = 0;
    n_ts_pkts = len/188;
    for(i=0;i<n_ts_pkts;i++){
	pid = get_pkt_pid(data + i*188);
	if(!pid){
	    pmt_pid = parse_pat_for_pmt(data + i*188);
	    break;
	}
    }

    return pmt_pid;
}


static uint16_t get_pkt_pid(uint8_t* data){
    uint16_t pid = 0;
    pid = data[1]&0x1F;
    pid<<=8;
    pid|=data[2];
    return pid;
}

static uint16_t parse_pat_for_pmt(uint8_t *data){
    uint16_t pid = 0, pos = 0,program_number,program_map_PID=0;;
    uint8_t adaf_field;
    /*Get adaptation field*/

    adaf_field = (data[3]>>4)&0x3;
    pos+= 4;
    if(adaf_field == 0x02 || adaf_field == 0x03)
	pos+=1+data[pos]; //skip adaptation filed.

    pos+=1; //pointer field
    /*Table 2-25 -- Program association section*/
    /*skip 8 bytes till last_section_number (included)*/
    pos += 8;
    for(;;){
        program_number =0;
        program_number = data[pos];
        program_number<<=8;
        program_number|=data[pos+1];
        pos+=2;
        if(program_number == 0) {
            pos+=2;
        } else {
            program_map_PID = data[pos]&0x1F;
            program_map_PID<<=8;
            program_map_PID|=data[pos+1];
            pos+=2;
            break;
        }
    }
    A_(printf("PAT found\n"));
    return program_map_PID;
}



static int8_t get_spts_pids(uint8_t*  data,  uint32_t len,
			    uint16_t* vpid,  uint16_t *apid,
			    uint16_t* apid2, uint16_t pmt_pid){

    int8_t ret = 0;
    uint32_t i, n_ts_pkts = 0;
    uint16_t pid;
    n_ts_pkts = len/188;
    for(i=0;i<n_ts_pkts;i++){
	pid = get_pkt_pid(data + i*188);
	if(pid == pmt_pid){
	    get_av_pids_from_pmt(data+i*188,vpid,apid,apid2);
	    ret = 1;
	}
    }

    return ret;

}


static void get_av_pids_from_pmt(uint8_t *data, uint16_t* vpid,
				 uint16_t *apid,  uint16_t* apid2){

    uint16_t pos = 0,program_info_length=0,pid, elementary_PID=0,
	ES_info_length=0, section_len = 0;
    uint8_t adaf_field, num_aud = 0, stream_type;

    *vpid = *apid =0;
    /*Get adaptation field*/
    adaf_field = (data[3]>>4)&0x3;
    pos+= 4;
    if(adaf_field == 0x02 || adaf_field == 0x03)
	pos+=1+data[pos]; //skip adaptation filed.

    pos+=1; //pointer field
    /*Table 2-28 -- Transport Stream program map section*/
    section_len = (data[6] & 0x0f) << 8;
    section_len |= data[7];

    /*skip 10 bytes till PCR_PID (included)*/
    pos += 10;

    /*skip the data descriptors*/
    program_info_length = data[pos]&0x0F;
    program_info_length<<=8;
    program_info_length|=data[pos+1];
    pos+=program_info_length;
    pos+=2;

    /*parse for PIDs*/
    for(;;){
	stream_type = data[pos++];
	elementary_PID = ES_info_length = 0;
	elementary_PID = data[pos]&0x1F;
	elementary_PID<<=8;
	elementary_PID |= data[pos+1];
	pos+=2;
	ES_info_length =  data[pos]&0xF;
	ES_info_length<<=8;
	ES_info_length|= data[pos+1];
	pos+=2;
	pos+= ES_info_length;
        
	switch(stream_type){
	    case 0x0F:
	    case 0x11:
		/*Audio: 0x0F for ISO/IEC 13818-7*/
		if(*apid == 0)
		    *apid = elementary_PID;
		else
		    *apid2 = elementary_PID;
		break;
	    case 0x1B:
		/*Video: 0x1B for H.264*/
		*vpid = elementary_PID;
		break;
	    default:
		break;
	}
	if (pos >= section_len) {
	    break;
	}

	if(pos>=180 /*||(*vpid&&*apid)*/)
	    break;
    }

}

/*
* obtainFirstTimeMux
* This module waits for receiving the first IDR frame in any one of
* the profile
*/
#if defined(MFP_BUGFIX_8460)
static int32_t obtainFirstTimeMux(sync_ctx_params_t* syncp,
	uint32_t len, int8_t *data,
	uint32_t kfi,spts_st_ctx_t *spts,
	mfp_publ_t* pub_ctx,
	sess_stream_id_t* id)
{
    int32_t ret = 0;
    uint32_t n_ts_pkts;
    uint32_t pts, dts,i, pkt_time;
    uint16_t pid;
    slice_type_e frame_type = slice_default_value;
    ts_pkt_infotable_t *pkt_tbl = NULL;
    uint16_t vpid;
    uint32_t frame_pkt_num = 0;
    n_ts_pkts = len / 188;
    vpid = spts->mux.video_pid;
    
    uint32_t pkt_offsets[MAX_NUM_NWINPT_PKTS];
    uint32_t cnt;
    for( cnt = 0; cnt < n_ts_pkts; cnt++){
    	pkt_offsets[cnt] = cnt * TS_PKT_SIZE;
    }
    ret = create_index_table((uint8_t *)data, pkt_offsets,
                             n_ts_pkts, spts->mux.audio_pid1,
                             spts->mux.video_pid, &pkt_tbl);

    if(ret < 0){
	goto obtainFirstTimeMuxRet;
    }
    
    for (i = 0; i < n_ts_pkts; i++) {
	pid = pkt_tbl[i].pid;
	if(pid!=vpid)
	    continue;

	frame_type = find_frame_type(n_ts_pkts, i, vpid, pkt_tbl, &frame_pkt_num);
	dts = pkt_tbl[i].dts;
	pts = pkt_tbl[i].pts;

	if(frame_type == slice_type_i && (pid == vpid)){
		uint32_t *idr_index = &pub_ctx->sync_params.idr_index[id->stream_id];
		pub_ctx->sync_params.pts_value[*idr_index][id->stream_id] = pts;
		pub_ctx->sync_params.dts_value[*idr_index][id->stream_id] = dts;
	    	if(!dts && !pub_ctx->sync_params.use_pts_decision_valid){
		    pub_ctx->sync_params.dts_value[*idr_index][id->stream_id] = pts;
	    	}		
		pub_ctx->sync_params.valid_index_entry[*idr_index][id->stream_id] = 1;
		A_(printf("First AddEntry sess %u strm %u index %u pts %u dts %u\n", id->sess_id, id->stream_id, *idr_index, pts, dts));	
		*idr_index = *idr_index + 1;	
		break;
	}	
    }
obtainFirstTimeMuxRet:
    if(pkt_tbl){
    	free(pkt_tbl);
    	pkt_tbl = NULL;
    }
    return(ret);
}
#else
static void obtainFirstTimeMux(sync_ctx_params_t* syncp,
	uint32_t len, int8_t *data,
	uint32_t kfi,uint16_t vpid,
	mfp_publ_t* pub_ctx,
	sess_stream_id_t* id)
{

    uint32_t n_ts_pkts;
    uint32_t pts, dts,i, pkt_time;
    uint16_t pid;
    int32_t frame_type=-1;
    n_ts_pkts = len / 188;
    for (i = 0; i < n_ts_pkts; i++) {
	pid = get_pkt_pid((uint8_t *)(data+(i * 188)));
	if(pid!=vpid)
	    continue;
	frame_type=-1;
	mfu_converter_get_timestamp((uint8_t *)(data+(i*188)),&frame_type,&pid, 
		&pts, &dts, &handle_mpeg4AVC);
	if(frame_type == 1 && (pid == vpid)){
	uint32_t *idr_index = &pub_ctx->sync_params.idr_index[id->stream_id];
	pub_ctx->sync_params.pts_value[*idr_index][id->stream_id] = pts;
	pub_ctx->sync_params.dts_value[*idr_index][id->stream_id] = dts;
	pub_ctx->sync_params.valid_index_entry[*idr_index][id->stream_id] = 1;
	A_(printf("First AddEntry sess %u strm %u index %u pts %u dts %u\n", id->sess_id, id->stream_id, *idr_index, pts, dts));	
	*idr_index = *idr_index + 1;	
	//syncp->sync_search_started = 1;
	}
#if 0	
	pkt_time = dts;
	if(!dts)
		pkt_time = pts;

	if (pkt_time!=0 && (frame_type == 1)) {
#ifdef OFFSET_PKT_TIME
	    syncp->first_time_recd = pkt_time + PKT_TIME_OFSET;
#else
	    syncp->first_time_recd = pkt_time;
#endif
	    syncp->sync_search_started = 1;
	    /*Sync from 5 kfis from now*/
	    syncp->sync_time = syncp->first_time_recd + 
	    	(glob_num_idrs_sync * kfi * 90) - glob_sync_time_calc_offset;
#ifdef PRINT_MSG
	    printf("First Time = %u\n",syncp->sync_time );
#endif
	    break;
	}
#endif	
    }
}
#endif


/*
* checkValueAllLowIndeces
* Occasionally IDRs in different profile enter our IDR DB with difference
* in time interval. On that case, DTS across profile/ PTS across profile in same
* index wont match. this module searches DB at lower index for matching
* PTS or DTS
*/
static uint32_t checkValueAllLowIndeces(	
	uint32_t index_entry,
	uint32_t common_value,
	uint32_t *value_strm_array,
	uint32_t *valid_entry,
	uint32_t strm_cnt
	){

	uint32_t cnt;
	for(cnt = 0; cnt < MAX_SYNC_IDR_FRAMES; cnt++){
		//check whether prev index entry is valid
		if(*(valid_entry + (cnt * MAX_STREAM_PER_ENCAP) + strm_cnt) == 1){
		    //check whether value entry matches the common value
		    //if so, return 1
		    uint32_t value_entry;
		    value_entry = *(value_strm_array+ (cnt * MAX_STREAM_PER_ENCAP) + strm_cnt);
		    B_(printf("strm %u: index:%u value %u\n", strm_cnt, cnt, value_entry));
		    if((value_entry > common_value - TIMESTAMP_DELTA)
		    	&&(value_entry < common_value + TIMESTAMP_DELTA)){
		    	return 1;
		    }
		}
	}
	return 0;
}

/*
* checkValueAllStrms
* This module checks whether common value(PTS or DTS) matches the given value
* in all profile in same index
*/
static uint32_t checkValueAllStrms(
	mfp_publ_t* pub_ctx,
	uint32_t common_value,
	uint32_t *value_strm_array,
	uint32_t *valid_entry,
	uint32_t index_entry){
	//run loop over all the streams in the session
	B_(printf("common value %u\n",common_value));
	if(!common_value){
		//don't run comparison for value 0
		C_(printf("checkValueAllStrms: No Result yet on DTS vs PTS\n"));
		return 0;
	}
	assert(pub_ctx->streams_per_encaps[0]<= MAX_STREAM_PER_ENCAP);
	uint32_t num_strms = calcTotalStrms(pub_ctx);
	
	for(uint32_t i = 0; i < num_strms; i++){
		if(*(valid_entry + (index_entry * MAX_STREAM_PER_ENCAP) + i) == 1){
			uint32_t value =  *(value_strm_array + (index_entry * MAX_STREAM_PER_ENCAP) + i);
			B_(printf("strm %u: value %u\n", i, value));
			//timestamp are allowed to be within range of +/- TIMESTAMP_DELTA
			//if outside, return failure
			if((value < common_value - TIMESTAMP_DELTA)
			||(value > common_value + TIMESTAMP_DELTA)){
			    //IDRs of diff profile might have arrived with delay, check
			    //the previous index entry
			    if(checkValueAllLowIndeces(index_entry, common_value, 
			    	value_strm_array, valid_entry, i) == 0){
			    	C_(printf("checkValueAllStrms: No Result yet on DTS vs PTS\n"));
				return 0;
			    }
			}
		} else if(pub_ctx->stream_parm[i].stream_state == STRM_ACTIVE){
			C_(printf("checkValueAllStrms: No Result yet on DTS vs PTS\n"));
			return 0;
		}
	}	
	B_(printf("checkValueAllStrms Success\n"));
	return (1);
}

/*
* DecidePTSvsDTS
* This module checks whether PTS or DTS matches across all profiles in
* given session
*/
static void DecidePTSvsDTS(sync_ctx_params_t* syncp, 
	sess_stream_id_t *id, uint32_t kfi){

	int32_t ret=0;
	mfp_publ_t* pub_ctx;
	pub_ctx = live_state_cont->get_ctx(live_state_cont,
		id->sess_id);
	uint32_t idr_index,all_dts_is_zero=1;
	uint32_t pkt_time;
	uint32_t frame_type = 1;
	
	idr_index = pub_ctx->sync_params.idr_index[id->stream_id];

	if(pub_ctx->sync_params.use_pts_decision_valid == 0){
		//decide on whether to use pts or dts
	    for(uint32_t i = 0; i < idr_index; i++){
		if(pub_ctx->sync_params.dts_value[i][id->stream_id]){
		    all_dts_is_zero = 0;
		    break;
		}
	    }
	    for(uint32_t i = 0; i < idr_index; i++){			
			//check whether DTS matches
		if(!all_dts_is_zero){
		    B_(printf("check DTS for index %u\n", i));
		    pkt_time = pub_ctx->sync_params.dts_value[i][id->stream_id];
		    ret = checkValueAllStrms(pub_ctx, 
					     pkt_time,
					     pub_ctx->sync_params.dts_value[0], 
					     pub_ctx->sync_params.valid_index_entry[0], i);
		}
		if(ret == 1){
		    pub_ctx->sync_params.use_pts = 0;
		    pub_ctx->sync_params.use_pts_decision_valid = 1;
		    A_(printf("DTS is chosen as sync time param\n"));
		    break;
		}
		else {
		    //check whether PTS matches
		    B_(printf("check PTS for index %u\n", i));
		    pkt_time = pub_ctx->sync_params.pts_value[i][id->stream_id];
		    ret = checkValueAllStrms(pub_ctx, 
					     pkt_time,
					     pub_ctx->sync_params.pts_value[0], 
					     pub_ctx->sync_params.valid_index_entry[0],i);
		    if(ret == 1) {
			pub_ctx->sync_params.use_pts = 1;
			pub_ctx->sync_params.use_pts_decision_valid = 1;
			A_(printf("PTS is chosen as sync time param\n"));
			break;
		    }
		}
	    }
	    if(pub_ctx->sync_params.use_pts_decision_valid == 1 && syncp->sync_search_started != 1 ){
		if (pkt_time!=0 && (frame_type == 1)) {
#ifdef OFFSET_PKT_TIME
			    syncp->first_time_recd = pkt_time + PKT_TIME_OFSET;
#else
			    syncp->first_time_recd = pkt_time;
#endif
			    syncp->sync_search_started = 1;
			    /*Sync from 5 kfis from now*/
			    syncp->sync_time = syncp->first_time_recd + 
    				(glob_num_idrs_sync * kfi * 90) - glob_sync_time_calc_offset;
			    A_(printf("sync time set as %u\n",syncp->sync_time ));
			    //break;
		}
	    }
	}
	
	return;
 }

static void print_sync_table(
			     mfp_publ_t* pub_ctx,
			     uint32_t max_index_entry,
			     sess_stream_id_t* id){
    uint32_t index_cnt;
    uint32_t strm_cnt;
    FILE *fp;
    uint32_t len;
    static uint32_t sync_no = 0;
    char fname[50]={'\0'};
    stream_param_t *strm;
    uint32_t num_strms = calcTotalStrms(pub_ctx);
    
    int32_t rc = mkdir("sync_table/", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if ((rc < 0) && (errno != EEXIST)) {
	    printf("mkdir failed 1: \n");
    }

    len =   snprintf(NULL,0,"sync_table/ses%d_state_%d.txt", id->sess_id, sync_no);
    snprintf(fname,len+1,"sync_table/ses%d_state_%d.txt", id->sess_id, sync_no++);
    fp = fopen(fname,"wb");

    if(fp == NULL){
    	printf("unable to open the file to write sync table\n");
	return;
    }


    strm_cnt = 0;
    fprintf(fp, "DTSentry ");
    for(strm_cnt = 0; strm_cnt < num_strms; strm_cnt++){
	    strm = &(pub_ctx->stream_parm[strm_cnt]);
	    if(strm->stream_state == STRM_ACTIVE)
		fprintf(fp, "Srm%d ", strm_cnt);
    }
    fprintf(fp, "\n");
    
    for(index_cnt = 0; index_cnt < max_index_entry; index_cnt++){
	    fprintf(fp, "Idx%d ", index_cnt);
	    for(strm_cnt = 0; strm_cnt < num_strms; strm_cnt++){
	    	strm = &(pub_ctx->stream_parm[strm_cnt]);
		if(strm->stream_state == STRM_ACTIVE)
		fprintf(fp, "%d ", pub_ctx->sync_params.dts_value[index_cnt][strm_cnt]);
	    }
	    fprintf(fp, "\n");    	    	
    }

    
    strm_cnt = 0;
    fprintf(fp, "PTSentry ");
    for(strm_cnt = 0; strm_cnt < num_strms; strm_cnt++){
	    strm = &(pub_ctx->stream_parm[strm_cnt]);
	    if(strm->stream_state == STRM_ACTIVE)    	
		fprintf(fp, "Srm%d ", strm_cnt);
    }
    fprintf(fp, "\n");
    
    for(index_cnt = 0; index_cnt < max_index_entry; index_cnt++){
	    strm_cnt = 0;
	    fprintf(fp, "Idx%d ", index_cnt);
	    for(strm_cnt = 0; strm_cnt < num_strms; strm_cnt++){
		strm = &(pub_ctx->stream_parm[strm_cnt]);
		if(strm->stream_state == STRM_ACTIVE)
		fprintf(fp, "%d ", pub_ctx->sync_params.pts_value[index_cnt][strm_cnt]);
	    }
	    fprintf(fp, "\n");    	    	
    }

    fclose(fp);
    return;
}

//! module to reset sync params data struct
//! sync params data structure is used in arriving at DTS vs PTS decision
static void reset_sync_params(mfp_publ_t *pub_ctx){

	uint32_t strm_cnt;
	uint32_t index_cnt;
	
	//reset the index entries of all the stream to zero after successful sync
	for(strm_cnt = 0; strm_cnt < MAX_STREAM_PER_ENCAP; strm_cnt++){
	    pub_ctx->sync_params.idr_index[strm_cnt] = 0;
	}
	
	for(index_cnt = 0; index_cnt < MAX_SYNC_IDR_FRAMES; index_cnt++){
		for(strm_cnt = 0; strm_cnt < MAX_STREAM_PER_ENCAP; strm_cnt++){
		    pub_ctx->sync_params.valid_index_entry[index_cnt][strm_cnt] = 0;
		    pub_ctx->sync_params.dts_value[index_cnt][strm_cnt] = 0;
		    pub_ctx->sync_params.pts_value[index_cnt][strm_cnt] = 0;
		}
	}

	return;	
}


/*==============================================================
Description: Function checks whether the streams achvieve the
sync point and if so set the appropriate flags. If all
the streams in a program are in sync then the entire
program is deemed to be in sync. Till then this function
is constantly called.
===============================================================*/
static int32_t syncToTimeMux(sync_ctx_params_t* syncp,
			     uint32_t *length, int8_t *data,
			     accum_state_t *accum_state,
			     int8_t *start_accum,
			     uint32_t pos,
			     uint32_t kfi,
			     spts_st_ctx_t *spts,
			     mfp_publ_t* pub_ctx,
			     sess_stream_id_t* id){
    uint32_t n_ts_pkts;
    uint32_t pkt_time,i, len, pts, dts;
    uint16_t pid,pid1;
#if defined(MFP_BUGFIX_8460)    
    slice_type_e frame_type;
    uint32_t frame_pkt_num = 0;
#else
    int32_t frame_type = -1;
    int32_t *ft_req = NULL;
#endif
    uint16_t vpid,apid1,apid2;
    ts_pkt_infotable_t *pkt_tbl = NULL;
    int32_t ret = 0;
    int32_t ret1 = 0;
    vpid  = spts->mux.video_pid;
    apid1 = spts->mux.audio_pid1;
    apid2 = spts->mux.audio_pid2;

    len = *length;
    n_ts_pkts = len / 188;

#if defined(MFP_BUGFIX_8460)
    uint32_t pkt_offsets[MAX_NUM_NWINPT_PKTS];
    uint32_t cnt;
    for( cnt = 0; cnt < n_ts_pkts; cnt++){
    	pkt_offsets[cnt] = cnt * TS_PKT_SIZE;
    }
    ret = create_index_table((uint8_t *)data, pkt_offsets, \
                             n_ts_pkts, spts->mux.audio_pid1,
                             spts->mux.video_pid, &pkt_tbl);

    if(ret < 0){
	goto sync_to_timemux_safe_ret;
    }  
#endif
    
    for (i = 0; i < n_ts_pkts; i++) {
#if defined(MFP_BUGFIX_8460)    	
	pid = pkt_tbl[i].pid;
	frame_type = slice_default_value;
	if(pid == vpid){
	    frame_type = find_frame_type(n_ts_pkts, i, vpid, pkt_tbl, &frame_pkt_num);
	}
	
	dts = pkt_tbl[i].dts;
	pts = pkt_tbl[i].pts;
#else	
	pid = get_pkt_pid((uint8_t *)(data+(i * 188)));
	ft_req = NULL;
	if(pid == vpid)
	    ft_req = &frame_type;
	frame_type = 0;
	mfu_converter_get_timestamp((uint8_t *)(data+(i * 188)), ft_req, 
				    &pid1, &pts, &dts, &handle_mpeg4AVC);
#endif	
	pkt_time = dts;
	if((pub_ctx->sync_params.use_pts && pub_ctx->sync_params.use_pts_decision_valid)
	   || (!dts && !pub_ctx->sync_params.use_pts_decision_valid))
	    pkt_time = pts;	
	if((pid == spts->mux.audio_pid1 || pid == spts->mux.audio_pid2)&& !dts)
	    pkt_time = pts;	
	if(!pkt_time){
	    continue;
	}
#ifdef OFFSET_PKT_TIME
	else 
	    pkt_time+=PKT_TIME_OFSET;
#endif
#if defined(MFP_BUGFIX_8460)
	if(frame_type == slice_type_i && (pid == vpid)){
#else
	if(frame_type == 1 && (pid == vpid)){		
#endif
	    uint32_t *idr_index = &pub_ctx->sync_params.idr_index[id->stream_id];
	    int32_t prev_idx;
	    if(!spts->video->incoming_kfi && pub_ctx->sync_params.use_pts_decision_valid)
		spts->video->incoming_kfi = ((pkt_time - syncp->first_time_recd)/90);
	    uint32_t prev_dts = 0;
	    uint32_t prev_pts = 0;
	    prev_idx = *idr_index - 1;
	    if(prev_idx >= 0){
	    	prev_dts = pub_ctx->sync_params.dts_value[*idr_index - 1][id->stream_id];
	    	prev_pts = pub_ctx->sync_params.pts_value[*idr_index - 1][id->stream_id];
	    }
	    if(prev_pts != pts || prev_dts != dts){
		    pub_ctx->sync_params.pts_value[*idr_index][id->stream_id] = pts;
		    pub_ctx->sync_params.dts_value[*idr_index][id->stream_id] = dts;
		    if(!dts && !pub_ctx->sync_params.use_pts_decision_valid){
		    	pub_ctx->sync_params.dts_value[*idr_index][id->stream_id] = pts;
		    }
		    
		    pub_ctx->sync_params.valid_index_entry[*idr_index][id->stream_id] = 1;
		    A_(printf("AddEntry sess %u strm %u index %u pts %u dts %u\n", id->sess_id, id->stream_id, *idr_index, pts, dts));
		    *idr_index = *idr_index + 1;

		    if(!pub_ctx->sync_params.use_pts_decision_valid){

			    //debugging feature
		    	    if(glob_sync_table_debug_print){
				print_sync_table(pub_ctx, *idr_index, id);
		    	    }

			    //decision taking module
			    DecidePTSvsDTS(syncp, id, kfi);

			    if(pub_ctx->sync_params.use_pts_decision_valid){
			    	if(pub_ctx->sync_params.use_pts){
					DBG_MFPLOG (pub_ctx->name, MSG, 
						MOD_MFPLIVE,"PTS will be used for sync");
			    	}else{
					DBG_MFPLOG (pub_ctx->name, MSG, 
						MOD_MFPLIVE,"DTS will be used for sync");
			    	}
				reset_sync_params(pub_ctx);
			    }
			    
			    if(!pub_ctx->sync_params.use_pts_decision_valid 
			    		&& *idr_index > (MAX_SYNC_IDR_FRAMES -1)){
			    	//failed to find a common timestamp upto 20 IDR frames
			    	//return failure - close the session
			    	DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE,
				    	"Unable to arrive at sync time after searching 20 IDR in each stream");
				reset_sync_params(pub_ctx);
			    	ret = -1;
				goto sync_to_timemux_safe_ret;
			    }
		    }
	     }
	}
 	if(pkt_time >=spts->sync_time){
#if defined(MFP_BUGFIX_8460)		
	    if(frame_type == slice_type_i && (pid == vpid) && !spts->mux.video_in_sync){
#else
	    if(frame_type == 1 && (pid == vpid) && !spts->mux.video_in_sync){
#endif
		spts->mux.video_in_sync = 1;
		spts->video->first_sync_id = i;
		spts->video->just_in_sync =1;
#ifdef BZ7998
                spts->video->sync_time = pkt_time;
#endif
		if(!spts->mux.audio1_in_sync)
		    spts->sync_time = pkt_time;
		A_(printf("Video Stream in Sync @ %u Stream %u\n",pkt_time,syncp->stream_num));
                DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
                            "Video Stream in Sync @ %u Stream %u",pkt_time,syncp->stream_num);

	    }
	    if((pid == apid1)&&!spts->mux.audio1_in_sync&&(apid1!=0)){
		spts->mux.audio1_in_sync = 1;
		spts->audio1->first_sync_id = i;
		spts->audio1->just_in_sync =1;
#ifdef BZ7998
                spts->audio1->sync_time = pkt_time;
#endif
 		A_(printf("1st Audio Stream in Sync: @ %u Stream %u\n",pkt_time,syncp->stream_num));
                 DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
                            "1st Audio Stream in Sync: @ %u Stream %u",pkt_time,syncp->stream_num);

	    }
	    if((pid == apid2)&&!spts->mux.audio2_in_sync&&!(apid2!=0)){
		spts->mux.audio2_in_sync = 1;
		spts->audio2->first_sync_id = i;
		spts->audio2->just_in_sync =1;
		A_(printf("2nd Audio Stream in Sync:\n"));
                DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
                            "2nd Audio Stream in Sync: @ %u Stream %u",pkt_time,syncp->stream_num);		
 	    }
	    //*accum_state = spts->mux.video_in_sync&spts->mux.audio1_in_sync;
	    if(spts->mux.video_in_sync)
	    	*accum_state |= ACCUM_VIDEO_INSYNC;
	    if(spts->mux.audio1_in_sync)
		    *accum_state |= ACCUM_AUDIO1_INSYNC;
	    	
	    if(spts->mux.audio2_in_sync)
		*accum_state |= ACCUM_AUDIO2_INSYNC;
	}
    }
sync_to_timemux_safe_ret:  
#if defined(MFP_BUGFIX_8460)	
    if(pkt_tbl){
    	free(pkt_tbl);
    	pkt_tbl = NULL;
    }
#endif    
    return (ret);
}

/*==================================================
Description: Update the time acc'ed, starting time
(if applicable), position and pkt list for a
buffer.
====================================================*/
    static int32_t
update_stream_buff(stream_buffer_data_t* buff,
		   uint32_t pos,
		   uint32_t pkt_time)
{
    if(buff->pending_read == MFP_ACC_BUF_PENDING_READ){
    	DBG_MFPLOG ("UpdateBuffers", ERROR, MOD_MFPLIVE, 
		"Audio and Video lag too much beyond: %u seconds",MFP_MAX_AV_LAG);
	return MFP_ACC_BUF_FULL;
    }	
    if(buff->pending_read!=MFP_ACC_BUF_WAIT)
	buff->pending_read = MFP_ACC_BUF_WAIT;
    if(pkt_time!=0){
	if(buff->starting_time==0)
	    buff->starting_time = pkt_time;
	buff->time_accumulated = pkt_time;
    }
#ifdef MFP_MEM_OPT
    if(buff->num_of_pkts == buff->mfp_ts_max_pkts - 1){
#else
    if (buff->num_of_pkts == MFP_TS_MAX_PKTS - 1) {
#endif
	return MFP_ACC_BUF_FULL;
    }

    buff->pkt_offsets[buff->num_of_pkts] = pos;
    buff->num_of_pkts++;

    return 0;
}


static ser_task_t* create_ser_sync_fmtr_task(sync_serial_t* sync_ser,
		       uint32_t sess_seq_num, uint32_t group_id,
			   uint32_t group_seq_num, task_hdlr_fptr task_hdlr, void* ctx) {

	sync_task_t* sync_task = createSyncTask(sync_ser, group_id, group_seq_num,
			ctx, task_hdlr, refMemRelease);
	if (sync_task == NULL)
		return NULL;
	ser_task_t* ser_task = createSerialTask(sync_ser, sess_seq_num,
			(void*)sync_task, sync_ser->sync_hdlr);
	if (ser_task == NULL) {
		free(sync_task);
		return NULL;
	}
	return ser_task;
}
		
		
static void refMemRelease(void* ref) {

	ref_count_mem_t* ref_cont = (ref_count_mem_t*)ref;
	ref_cont->release_ref_cont(ref_cont);
}

void slHandleResync(uint32_t sess_id, uint32_t stream_id,
		uint32_t accum_seq_num) {

	// called with lock held
	mfp_publ_t* pub = live_state_cont->get_ctx(
			live_state_cont, sess_id);

	uint32_t data_id = pub->sess_chunk_ctr;
	//pub->sess_chunk_ctr++;

	mfpTaskFunc handler = pub->sl_sync_ser->serial_in_hdlr; 
	ser_task_t* task = create_ser_sync_fmtr_task(pub->sl_sync_ser,
			data_id, stream_id, accum_seq_num, NULL, NULL); 
	thread_pool_task_t* t_task = newThreadPoolTask(handler, task, NULL);
	task_processor->add_work_item(task_processor, t_task); 

}

