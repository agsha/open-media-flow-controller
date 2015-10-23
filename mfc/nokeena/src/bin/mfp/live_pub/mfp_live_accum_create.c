#include "mfp_live_accum_create.h"
#include "mfp_publ_context.h"
#include "mfp_video_header.h"
#include "nkn_debug.h"
#if defined(INC_ALIGNER)
#include "mfp_video_aligner.h"
#endif
#if defined(INC_SEGD)
#include "mfp_live_mfu_merge.h"
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

extern file_id_t *file_state_cont;
//extern live_id_t *live_state_cont;
extern uint32_t glob_mfp_live_enable_accumv2;

static uint32_t create_dbg_counter(
                                   mfp_publ_t* pub_ctx,
                                   char **name,
                                   char *instance,
                                   char *string,
                                   uint32_t *count_var);
static void write_ts_mfu_hdrs(mfub_t * mfu,stream_param_t* st, sess_stream_id_t* id);
static void del_spts_stream_buffers(spts_st_ctx_t* spts);
#define MAX_UDP_SIZE 4096 // 4Kbytes as max size

accum_ts_t* newAccumulatorTS(sess_stream_id_t* id,
			     SOURCE_TYPE stype)
{

    int32_t i, err = 0;
    uint32_t data_size;
    mfp_publ_t *pub_ctx = NULL;
    stream_param_t *stream_parm = NULL;
    sess_id_t *sess_state_cont = NULL;

    A_(printf("entering accumulator alloc routines\n"));

    /* retrive the correct stream context and publisher context */
    switch (stype) {
	case LIVE_SRC:
	    sess_state_cont = live_state_cont;
	    break;
	case FILE_SRC:
	    sess_state_cont = file_state_cont;
	    break;
	default:
	    err = -1;
	    break;
    }
    if (err == -1) {
	return NULL;
    }
    pub_ctx = sess_state_cont->get_ctx(sess_state_cont, id->sess_id);
    stream_parm = &(pub_ctx->stream_parm[id->stream_id]);


    /* allocate for the the accumulator */
    accum_ts_t* accum = nkn_calloc_type(1, sizeof(accum_ts_t),mod_vpe_mfp_accum_ts);
    if (!accum){
	DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE, "ACCUM MEM ALLOC FAILED\n");
	return NULL;
    }

    /* setup the callback functions */
#ifndef _FILE_PATH_
    accum->accum_intf.get_data_buf = getBufferArea;
    accum->accum_intf.data_in_handler = processData;
    accum->accum_intf.delete_accum = deleteAccumulator;
    accum->accum_intf.handle_EOS = handleTsEOS;
#else
    accum->accum_intf.get_data_buf = NULL;
    accum->accum_intf.data_in_handler = NULL;
    accum->accum_intf.delete_accum = deleteAccumulator;
    accum->accum_intf.handle_EOS = NULL;

#endif
    //    pub_ctx->ms_parm.key_frame_interval/=1000;
    accum->key_frame_interval = 0;
    if (pub_ctx->ms_parm.status == FMTR_ON)
	accum->key_frame_interval = pub_ctx->ms_parm.key_frame_interval;
    if (pub_ctx->ss_parm.status == FMTR_ON){
	if(!accum->key_frame_interval || accum->key_frame_interval > pub_ctx->ss_parm.key_frame_interval)
	    accum->key_frame_interval = pub_ctx->ss_parm.key_frame_interval;
    }
    if(stype != FILE_SRC){
      if(!accum->key_frame_interval){
	DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE, "KEY FRAME INT NOT SET\n");
	return NULL;
      }
    }
    /*Variable Segment duration support*/
#if defined(INC_SEGD)
    if(stype != FILE_SRC){
	uint32_t j, num_fmtrs=0;
	if(pub_ctx->ms_parm.status == FMTR_ON){
	    num_fmtrs++;
	   accum->acc_ms_fmtr=
	       nkn_calloc_type(1, sizeof(accum_fmtr_segd_t),mod_vpe_mfp_accum_ts);
	    accum->acc_ms_fmtr->seq_num = 0;
	    accum->acc_ms_fmtr->fmtr = init_merge_mfu(pub_ctx->ms_parm.key_frame_interval,
						      MFP_SEG_MAX_MFUS);
	    accum->acc_ms_fmtr->dproot = createDataPair(id, pub_ctx->ms_parm.key_frame_interval);
	    //	    strncpy(accum->acc_ms_fmtr->fmtr->sess_name,pub_ctx->name,strlen((char*)pub_ctx->name));
	    set_sessid_name(accum->acc_ms_fmtr->fmtr,
                            (char*)(pub_ctx->name),
                            id->stream_id);


	}
	if (pub_ctx->ss_parm.status == FMTR_ON){
	    num_fmtrs++;
	    accum->acc_ss_fmtr=
		nkn_calloc_type(1, sizeof(accum_fmtr_segd_t),mod_vpe_mfp_accum_ts);
	    accum->acc_ss_fmtr->seq_num = 0;
	    accum->acc_ss_fmtr->fmtr =    init_merge_mfu(pub_ctx->ss_parm.key_frame_interval,		 
							 MFP_SEG_MAX_MFUS);
	    accum->acc_ss_fmtr->dproot = createDataPair(id, pub_ctx->ss_parm.key_frame_interval);
	    set_sessid_name(accum->acc_ss_fmtr->fmtr,
			    (char*)(pub_ctx->name),
			    id->stream_id);
	    //	    accum->acc_ss_fmtr->fmtr.sess_name = nkn_malloc_type(sizeof(char)*500,mod_vpe_mfp_accum_ts);
	    //strncpy(accum->acc_ms_fmtr->fmtr->sess_name,pub_ctx->name,strlen((char*)pub_ctx->name));
	}
	accum->ts2mfu_desc_ref =  nkn_calloc_type(1, sizeof(ts2mfu_desc_ref_t),mod_vpe_mfp_accum_ts);
	accum->ts2mfu_desc_ref->ts2mfu_ref_cnt =  nkn_calloc_type(1, 
								  sizeof(ts2mfu_ref_cnt_fmtr_t)*MFP_SEG_MAX_MFUS,
								  mod_vpe_mfp_accum_ts);
	accum->ts2mfu_desc_ref->num_fmtrs = num_fmtrs;
	accum->ts2mfu_desc_ref->max_pos = MFP_SEG_MAX_MFUS;
    }
#endif//SEGD

    /* calculate the approxfrag size */
    data_size = /* computed in bytes */
	(MFP_TS_MAX_ACC_TIME * stream_parm->bit_rate *
	 (1024 / 8) * (accum->key_frame_interval/1000));

    //DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE, "Stream id %u Bitrate %u", id->stream_id,  stream_parm->bit_rate);
    if (!data_size && stype != FILE_SRC) {
	/* data size cannot be zero except for FILE conversions which
	 * do not use the accum->data buffer
	 */
	DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE, "CHECK BITRATE ENTERED. EXITING NOW\n");
	free(accum);
	return NULL;
    }
    if(data_size < 2*MAX_UDP_SIZE)
	data_size = 2*MAX_UDP_SIZE;

    accum->data = (uint8_t*)nkn_malloc_type(data_size,mod_vpe_mfp_accum_ts);
    accum->data_size = data_size;
    accum->last_read_index = -1;
    accum->data_pos = 0;
    accum->start_accum = 0;
    accum->present_read_index = 0;
    //accum->sync_state = ACC_NOT_IN_SYNC;
    accum->num_resyncs = 0;
    accum->refreshed = 0;
    accum->accum_state = ACCUM_INIT;

    accum->prev_data_ptr = NULL;
    accum->prev_len = 0;
    accum->prev_pktend = 0;
#ifndef _FILE_PATH_
    accum->error_callback_fn = &register_formatter_error;
#else
    accum->error_callback_fn = NULL;
#endif
    
    /*Initialize the SPTS streams*/
    accum->spts = (spts_st_ctx_t*)nkn_calloc_type(1,sizeof(spts_st_ctx_t),mod_vpe_mfp_accum_ts);
    if(!accum->spts||!accum->data){
	DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "EXITTING DUE TO LACK OF MEMORY\n");
	free(accum);
	return NULL;
    }
    accum->spts->spts_data_size = data_size;
    memset(&accum->spts->mux,0,sizeof(mux_st_elems_t));
    //TODO
    accum->mfu_seq_num = 0;

    /* create the stats counters */
    accum->stats.instance = (char*)			\
			    nkn_calloc_type(1, 256, mod_vpe_charbuf);
    if (!accum->stats.instance) {
	DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "EXITTING DUE TO LACK OF MEMORY\n");
	free(accum);
	return NULL;
    }



    snprintf(accum->stats.instance, 256, "%s", pub_ctx->name);

    /*copying the name since we dont want a case where the reference
     *gets deleted due to some instantaneous cleanup
     */

    char *str = (char*)nkn_calloc_type(1, 256, mod_vpe_charbuf);
    uint32_t ret;
    if (!str) {
	DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "EXITTING DUE TO LACK OF MEMORY\n");
	free(accum);
	return NULL;
    }

    //counter 1
    snprintf(str, 256 , "mfp_live_strm_%d.resync_count", id->stream_id);
    ret = create_dbg_counter(pub_ctx, &accum->stats.resync_count_name, accum->stats.instance,
	    str, &accum->stats.resync_count);
    if(ret == 0){
	free(accum);
	free(str);
	return NULL;
    }

    //counter 2
    snprintf(str, 256 , "mfp_live_strm_%d.lost_video_pkts_count", id->stream_id);
    ret = create_dbg_counter(pub_ctx, &accum->stats.tot_video_pkt_loss_name, accum->stats.instance,
	    str, &accum->stats.tot_video_pkt_loss);
    if(ret == 0){
	free(accum);
	free(str);
	return NULL;
    }


    //counter 3
    snprintf(str, 256 , "mfp_live_strm_%d.lost_audio_pkts_count", id->stream_id);
    ret = create_dbg_counter(pub_ctx, &accum->stats.tot_audio_pkt_loss_name, accum->stats.instance,
	    str, &accum->stats.tot_audio_pkt_loss);
    if(ret == 0){
	free(accum);
	free(str);
	return NULL;
    }

    //counter 4
    snprintf(str, 256 , "mfp_live_strm_%d.keyframe_notfound_count", id->stream_id);
    ret = create_dbg_counter(pub_ctx, &accum->stats.tot_kfnf_err_name, accum->stats.instance,
	    str, &accum->stats.tot_kfnf_err);
    if(ret == 0){
	free(accum);
	free(str);
	return NULL;
    }

    //counter 5
    snprintf(str, 256 , "mfp_live_strm_%d.total_pkts_received", id->stream_id);
    ret = create_dbg_counter(pub_ctx, &accum->stats.tot_pkts_rx_name, accum->stats.instance,
	    str, &accum->stats.tot_pkts_rx);
    if(ret == 0){
	free(accum);
	free(str);
	return NULL;
    }

    //counter 6
    snprintf(str, 256 , "mfp_live_strm_%d.total_video_duration", id->stream_id);
    ret = create_dbg_counter(pub_ctx, &accum->stats.tot_video_duration_name, accum->stats.instance,
	    str, &accum->stats.tot_video_duration);
    if(ret == 0){
	free(accum);
	free(str);
	return NULL;
    }

    //counter 7a
    snprintf(str, 256 , "mfp_live_strm_%d.total_audio_duration_stream_1", id->stream_id);
    ret = create_dbg_counter(pub_ctx, &accum->stats.tot_audio_duration_strm1_name, accum->stats.instance,
	    str, &accum->stats.tot_audio_duration_strm1);
    if(ret == 0){
	free(accum);
	free(str);
	return NULL;
    }

    //counter 7b
    snprintf(str, 256 , "mfp_live_strm_%d.total_audio_duration_stream_2", id->stream_id);
    ret = create_dbg_counter(pub_ctx, &accum->stats.tot_audio_duration_strm2_name, accum->stats.instance,
	    str, &accum->stats.tot_audio_duration_strm2);
    if(ret == 0){
	free(accum);
	free(str);
	return NULL;
    }

    //counter 8
    snprintf(str, 256 , "mfp_live_strm_%d.tot_processed_ms_chunks", id->stream_id);
    ret = create_dbg_counter(pub_ctx, &accum->stats.tot_proc_ms_chunks_name, accum->stats.instance,
	    str, &accum->stats.tot_proc_ms_chunks);
    if(ret == 0){
	free(accum);
	free(str);
	return NULL;
    }
    snprintf(str, 256 , "mfp_live_strm_%d.tot_processed_ss_chunks", id->stream_id);
    ret = create_dbg_counter(pub_ctx, &accum->stats.tot_proc_ss_chunks_name, accum->stats.instance,
	    str, &accum->stats.tot_proc_ss_chunks);
    if(ret == 0){
	free(accum);
	free(str);
	return NULL;
    }

    //counter 9
    snprintf(str, 256 , "mfp_live_strm_%d.avg_audio_bitrate", id->stream_id);
    ret = create_dbg_counter(pub_ctx, &accum->stats.avg_audio_bitrate_name, accum->stats.instance,
	    str, &accum->stats.avg_audio_bitrate);
    if(ret == 0){
	free(accum);
	free(str);
	return NULL;
    }

    //counter 10
    snprintf(str, 256 , "mfp_live_strm_%d.avg_video_bitrate", id->stream_id);
    ret = create_dbg_counter(pub_ctx, &accum->stats.avg_video_bitrate_name, accum->stats.instance,
	    str, &accum->stats.avg_video_bitrate);
    if(ret == 0){
	free(accum);
	free(str);
	return NULL;
    }
    accum->stats.apple_fmtr_num_active_tasks = (int32_t*)
      nkn_calloc_type(1, sizeof(int32_t), mod_vpe_temp_t);
    if (!accum->stats.apple_fmtr_num_active_tasks) {
      DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "EXITTING DUE TO LACK OF MEMORY\n");
      free(accum);
      return NULL;
    }

    //counter 11
    snprintf(str, 256 , "mfp_live_strm_%d.apple_fmtr_num_active_tasks", id->stream_id);
    ret = create_dbg_counter(pub_ctx,&accum->stats.apple_fmtr_num_active_tasks_name, accum->stats.instance,
			     str, (uint32_t*)accum->stats.apple_fmtr_num_active_tasks);
    if(ret == 0){
	free(accum);
	free(str);
	return NULL;
    }

    free(str);
#if defined(INC_ALIGNER)
    /*create aligner*/
    if(create_aligner_ctxt(&accum->aligner_ctxt) != aligner_success){
    	printf("failed to create aligner ctxt\n");
	if(accum)	
	    free(accum);
	return NULL;
    }
#endif

    /*Update standard MFU headers*/
    write_ts_mfu_hdrs(&accum->mfu_hdr,stream_parm, id);
    accum->mfu_hdr.duration = accum->key_frame_interval/1000;
    A_(printf("address of accum %p\n", accum));
    return accum;
}

void deleteAccumulator(void* accum_ts)
{
    accum_ts_t* accum = (accum_ts_t*) accum_ts;

    free(accum->data);
    accum->data = NULL;

    del_spts_stream_buffers(accum->spts);

    
    if(accum->spts->mux.video_pid){
	free(accum->spts->video);
	accum->spts->video = NULL;
    }
    if(accum->spts->mux.audio_pid1){
	free(accum->spts->audio1);
	accum->spts->audio1 = NULL;
    }
    if(accum->spts->mux.audio_pid2){
	free(accum->spts->audio2);
	accum->spts->audio2 = NULL;
    }
    free(accum->spts);
    accum->spts= NULL;

    if(accum->stats.resync_count_name){
	nkn_mon_delete(accum->stats.resync_count_name, accum->stats.instance);
	free(accum->stats.resync_count_name);
	accum->stats.resync_count_name = NULL;
    }
    if(accum->stats.tot_pkts_rx_name){
	nkn_mon_delete(accum->stats.tot_pkts_rx_name, accum->stats.instance);
	free(accum->stats.tot_pkts_rx_name);
	accum->stats.tot_pkts_rx_name = NULL;
    }
    if(accum->stats.tot_video_duration_name){
	nkn_mon_delete(accum->stats.tot_video_duration_name, accum->stats.instance);
	free(accum->stats.tot_video_duration_name);
	accum->stats.tot_video_duration_name = NULL;
    }
    if(accum->stats.tot_audio_duration_strm1_name){
	nkn_mon_delete(accum->stats.tot_audio_duration_strm1_name, accum->stats.instance);
	free(accum->stats.tot_audio_duration_strm1_name);
	accum->stats.tot_audio_duration_strm1_name = NULL;
    }
    if(accum->stats.tot_audio_duration_strm2_name){
	nkn_mon_delete(accum->stats.tot_audio_duration_strm2_name, accum->stats.instance);
	free(accum->stats.tot_audio_duration_strm2_name);
	accum->stats.tot_audio_duration_strm2_name = NULL;
    }
    if(accum->stats.tot_proc_ms_chunks_name){
	nkn_mon_delete(accum->stats.tot_proc_ms_chunks_name, accum->stats.instance);
	free(accum->stats.tot_proc_ms_chunks_name);
	accum->stats.tot_proc_ms_chunks_name = NULL;
    }
    if(accum->stats.tot_proc_ss_chunks_name){
	nkn_mon_delete(accum->stats.tot_proc_ss_chunks_name, accum->stats.instance);
	free(accum->stats.tot_proc_ss_chunks_name);
	accum->stats.tot_proc_ss_chunks_name = NULL;
    }
    if(accum->stats.tot_video_pkt_loss_name){
	nkn_mon_delete(accum->stats.tot_video_pkt_loss_name, accum->stats.instance);
	free(accum->stats.tot_video_pkt_loss_name);
	accum->stats.tot_video_pkt_loss_name = NULL;
    }
    if(accum->stats.tot_audio_pkt_loss_name){
	nkn_mon_delete(accum->stats.tot_audio_pkt_loss_name, accum->stats.instance);
	free(accum->stats.tot_audio_pkt_loss_name);
	accum->stats.tot_audio_pkt_loss_name = NULL;
    }
    if(accum->stats.tot_kfnf_err_name){
	nkn_mon_delete(accum->stats.tot_kfnf_err_name, accum->stats.instance);
	free(accum->stats.tot_kfnf_err_name);
	accum->stats.tot_kfnf_err_name = NULL;
    }
    if(accum->stats.avg_audio_bitrate_name){
	nkn_mon_delete(accum->stats.avg_audio_bitrate_name, accum->stats.instance);
	free(accum->stats.avg_audio_bitrate_name);
	accum->stats.avg_audio_bitrate_name = NULL;
    }
    if(accum->stats.avg_video_bitrate_name){
	nkn_mon_delete(accum->stats.avg_video_bitrate_name, accum->stats.instance);
	free(accum->stats.avg_video_bitrate_name);
	accum->stats.avg_video_bitrate_name = NULL;
    }
    if(accum->stats.apple_fmtr_num_active_tasks_name){
	nkn_mon_delete(accum->stats.apple_fmtr_num_active_tasks_name, accum->stats.instance);
	free(accum->stats.apple_fmtr_num_active_tasks_name);
	accum->stats.apple_fmtr_num_active_tasks_name = NULL;
    }
    if(accum->stats.instance){
	free(accum->stats.instance);
	accum->stats.instance = NULL;
    }
#if defined(INC_ALIGNER)
    if(accum->aligner_ctxt){
    	if(accum->aligner_ctxt->del(accum->aligner_ctxt) != aligner_success){
		printf("Failed to delete mem for aligner context\n");
    	}
	accum->aligner_ctxt = NULL;
    }
#endif
    free(accum);

    accum = NULL;
}


static uint32_t create_dbg_counter(
				   mfp_publ_t* pub_ctx,
				   char **name,
				   char *instance,
				   char *string,
				   uint32_t *count_var){
   //printf("before nkn_mon_add %s\n", string);

  *name = (char *)nkn_calloc_type(1, 256, mod_vpe_charbuf);

  if (!name) {
    DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "EXITTING DUE TO LACK OF MEMORY\n");
    return 0;
  }
  strcpy(*name, string);
  nkn_mon_add(*name, instance,
	      (void*)count_var,
	      sizeof(*count_var));
  *count_var = 0;

  return (1);
}

static void write_ts_mfu_hdrs(mfub_t * mfu,stream_param_t* st, sess_stream_id_t* id){
  mfu->version = 0x10;
  mfu->program_number = id->sess_id;
  //mfu->program_number = glob_mfp_live_tot_pmfs-1;
  mfu->stream_id = id->stream_id;
  mfu->timescale = 90000;
  if(st->med_type == VIDEO|| st->med_type == MUX)
    mfu->video_rate = st->bit_rate;
  if(st->med_type == AUDIO)
    mfu->audio_rate = st->bit_rate;
  mfu->offset_vdat = 0;
  mfu->offset_adat = 0;
}

static void del_spts_stream_buffers(spts_st_ctx_t* spts){
  uint32_t i;
  if(spts->mux.video_pid){
    if(spts->video){
      for(i=0;i<spts->video->mfp_max_buf_depth;i++){
	free(spts->video->stream[i].pkt_offsets);
	spts->video->stream[i].pkt_offsets = NULL;
      }
      free(spts->video->stream);
      spts->video->stream = NULL;
      free(spts->video);
      spts->video = NULL;
    }
  }
#ifndef MFP_LIVE_ACCUMV2
  if(spts->mux.audio_pid1){
      if(spts->audio1){
	  for(i=0;i<spts->audio1->mfp_max_buf_depth;i++){
	      free(spts->audio1->stream[i].pkt_offsets);
	      spts->audio1->stream[i].pkt_offsets = NULL;
	  }
	  free(spts->audio1->stream);
	  spts->audio1->stream = NULL;
	  free(spts->audio1);
	  spts->audio1 = NULL;
      }
  }
  if(spts->mux.audio_pid2){
      if(spts->audio2){
	  for(i=0;i<spts->audio2->mfp_max_buf_depth;i++){
	      free(spts->audio2->stream[i].pkt_offsets);
		  spts->audio2->stream[i].pkt_offsets = NULL;
	  }
	  free(spts->audio2->stream);
	  spts->audio2->stream = NULL;
	  free(spts->audio2);
	  spts->audio2 = NULL;
      }
  }
#else
  if(spts->mux.audio_pid1){
      if(spts->audio1){
	  for(i=0;i<spts->audio1->mfp_max_buf_depth;i++){
	      free(spts->audio1->linear_buff->acc_aud_buf[i].accum_pkt_offsets);
	      spts->audio1->linear_buff->acc_aud_buf[i].accum_pkt_offsets = NULL;
	  }
	  free(spts->audio1->linear_buff->acc_aud_buf);
	  spts->audio1->linear_buff->acc_aud_buf = NULL;
	  
	  free(spts->audio1->linear_buff->pkts);
	  spts->audio1->linear_buff->pkts = NULL;
	  free(spts->audio1->linear_buff);
	  spts->audio1->linear_buff = NULL;
      }
  }
  if(spts->mux.audio_pid2){
      for(i=0;i<spts->audio2->mfp_max_buf_depth;i++){
	  free(spts->audio2->linear_buff->acc_aud_buf[i].accum_pkt_offsets);
	  spts->audio2->linear_buff->acc_aud_buf[i].accum_pkt_offsets = NULL;
      }
      free(spts->audio2->linear_buff->acc_aud_buf);
      spts->audio2->linear_buff->acc_aud_buf = NULL;
      free(spts->audio2->linear_buff->pkts);
      spts->audio2->linear_buff->pkts = NULL;
      free(spts->audio2->linear_buff);
      spts->audio2->linear_buff = NULL;
  }
#endif
}

/*Global Times*/
//uint32_t tot_vid_acc = 0;
//uint32_t tot_aud_acc = 0;

extern uint32_t glob_apple_fmtr_num_task_low_threshold;
extern uint32_t glob_apple_fmtr_num_task_high_threshold;

int32_t accum_validate_msfmtr_taskcnt(
	sess_stream_id_t* id,
	int8_t *name,
	uint32_t key_frame_interval,
	int32_t task_counter){

	static int32_t task_buildup_warn_limit = 0;
	static int32_t task_buildup_err_limit = 0;

	if(!task_buildup_warn_limit){
	    task_buildup_warn_limit = 
		glob_apple_fmtr_num_task_low_threshold/key_frame_interval;
	}
	
	if(!task_buildup_err_limit){
	    task_buildup_err_limit = 
		glob_apple_fmtr_num_task_high_threshold/key_frame_interval;
	}

	
	if(task_counter > task_buildup_warn_limit&&
	   task_counter <= task_buildup_err_limit){
	    //warning case
	    DBG_MFPLOG(name, WARNING, MOD_MFPLIVE,
		       "sess[%u] stream[%u] msfmtr active task (%d) > %d", 
		       id->sess_id, id->stream_id, task_counter,task_buildup_warn_limit);
	    return VPE_WARNING;
	}
	
	if(task_counter > task_buildup_err_limit){
	    //error case
	    DBG_MFPLOG(name, SEVERE, MOD_MFPLIVE,
		       "sess[%u] stream[%u] msfmtr active task (%d) > %d", 
		       id->sess_id, id->stream_id, task_counter, task_buildup_err_limit);
	    return VPE_ERROR;
	}
	
	return VPE_SUCCESS;
}

static int32_t identify_slow_strm(sess_stream_id_t *id, mfp_publ_t *pub_ctx){
	uint32_t strm_cnt;
	stream_param_t *stream_parm;
	accum_ts_t *accum;
	int32_t ret;
	uint32_t min_seqnum = 0xffffffff;
	uint32_t slow_strmid = 0xffffffff;
	uint32_t max_seqnum_diff;
	uint32_t max_seqnum = 0;
	uint32_t fast_strmid = 0xffffffff;
	
	static uint32_t task_buildup_warn_limit = 0;
	uint32_t tot_strm_cnt = pub_ctx->streams_per_encaps[0];

	for(strm_cnt = 0; strm_cnt < tot_strm_cnt; strm_cnt++){
		stream_parm = &(pub_ctx->stream_parm[strm_cnt]);
		accum = (accum_ts_t*)pub_ctx->accum_ctx[strm_cnt];

		if(!task_buildup_warn_limit){
		    task_buildup_warn_limit = 
			glob_apple_fmtr_num_task_low_threshold/accum->key_frame_interval;
		}
		if(stream_parm->stream_state == STRM_DEAD){
			tot_strm_cnt++;
		}
	}	

	for(strm_cnt = 0; strm_cnt < tot_strm_cnt; strm_cnt++){
		
		stream_parm = &(pub_ctx->stream_parm[strm_cnt]);
		accum = (accum_ts_t*)pub_ctx->accum_ctx[strm_cnt];

		if(stream_parm->stream_state != STRM_DEAD){
			if(accum->acc_ms_fmtr->seq_num < min_seqnum){
				//find minimum
				min_seqnum = accum->acc_ms_fmtr->seq_num;
				slow_strmid = strm_cnt;
			} 
			if(accum->acc_ms_fmtr->seq_num > max_seqnum){
				//find max
				max_seqnum = accum->acc_ms_fmtr->seq_num;
				fast_strmid = strm_cnt;
			}
		} 
	}
	if(fast_strmid != 0xffffffff){
		max_seqnum_diff = max_seqnum - min_seqnum;
	}

	if((slow_strmid == id->stream_id) && 
	(max_seqnum_diff > task_buildup_warn_limit)){
		return VPE_ERROR;
	}
	return VPE_SUCCESS;

}
	
int32_t slow_strm_HA(sess_stream_id_t *id, mfp_publ_t *pub_ctx){

	uint32_t strm_cnt;
	stream_param_t *stream_parm;
	accum_ts_t *accum;
	int32_t ret;
	uint32_t tot_strm_cnt = pub_ctx->streams_per_encaps[0];

	for(strm_cnt = 0; strm_cnt < tot_strm_cnt; strm_cnt++){
		stream_parm = &(pub_ctx->stream_parm[strm_cnt]);
		accum = (accum_ts_t*)pub_ctx->accum_ctx[strm_cnt];
		if(stream_parm->stream_state != STRM_DEAD){
			ret = accum_validate_msfmtr_taskcnt(
				id, pub_ctx->name, accum->key_frame_interval, 
			    	*accum->stats.apple_fmtr_num_active_tasks);
			if(ret == VPE_WARNING){
				if(identify_slow_strm(id, pub_ctx) == VPE_ERROR){
					return VPE_ERROR;
				}
			}
		}else{
			tot_strm_cnt++;
		}
	}	
	return VPE_SUCCESS;
}

