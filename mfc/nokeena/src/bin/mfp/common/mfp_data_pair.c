
#include "mfp_data_pair.h"
#include "mfp_limits.h"
#include "mfp_live_accum_create.h"
#include "mfp_publ_context.h"
#include "nkn_debug.h"
#include "nkn_vpe_mfu_defs.h"
#include "nkn_vpe_mfu_parse.h"

#ifdef MFP_LIVE_ACCUMV2
 #include "mfp_live_accum_ts_v2.h"
#else
 #include "mfp_live_accum_ts.h"
#endif

extern uint32_t glob_apple_fmtr_num_task_high_threshold;


void resetDataPair(data_pair_root_t *dproot){
	uint32_t i;
	for(i = 0; i < dproot->max_len; i++){
		dproot->dpleaf[i].mfu_seq_num = (uint32_t)-1;
		dproot->dpleaf[i].mfu_duration = (uint32_t)-1;
	}
	dproot->wr_index = 0;
	return;
}

data_pair_root_t *createDataPair(
	sess_stream_id_t* id,
	uint32_t key_frame_interval){

	data_pair_root_t *dproot= NULL;
	mfp_publ_t* pub_ctx;
	pub_ctx = live_state_cont->get_ctx(live_state_cont,
		id->sess_id);

	dproot = (data_pair_root_t *)
	    nkn_malloc_type(sizeof(data_pair_root_t),mod_vpe_mfp_accum_ts);
	if(dproot == NULL){
		DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "unable to allocate mem for dproot\n");
		return NULL;
	}

	dproot->wr_index = 0;
	dproot->max_len = 
	    glob_apple_fmtr_num_task_high_threshold/key_frame_interval;
	
	dproot->dpleaf = (data_pair_element_t *)nkn_malloc_type(
	    dproot->max_len * sizeof(data_pair_element_t),
	    mod_vpe_mfp_accum_ts);

	if(dproot->dpleaf == NULL){
		DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "unable to allocate mem for dpleaf\n");
		return NULL;
	}
	resetDataPair(dproot);
	return(dproot);
}


/*  lookupDataPair
 *  search for given mfu sequence number in the DataPair root data
 *  structure
 *  return the index if found
 *  return -1 if not found
 */
static int32_t lookupDataPair(data_pair_root_t *dproot,
			uint32_t seq_num){

	uint32_t i = 0;
	int32_t ret_index = -1;
	for(i = 0; i < dproot->max_len; i++){
		if(dproot->dpleaf[i].mfu_seq_num == seq_num){
			ret_index = (int32_t)i;
			break;
		}
	}
	return (ret_index);
}

/* lookupDpRoot
 * from the given formatter string, pick the correct
 * data_pair_root_t data structure
 */
static data_pair_root_t * lookupDpRoot(accum_ts_t *accum,
				     FMTR_LIST_E fmtr){
	data_pair_root_t *dproot = NULL;

	switch(fmtr){
		case APPLE_FMTR:
			dproot = accum->acc_ms_fmtr->dproot;
			break;
		case SILVERLIGHT_FMTR:
			dproot = accum->acc_ss_fmtr->dproot;
			break;
		default:
			dproot = NULL;
	}
	return (dproot);
}

/* enterDataPair
 * function to enter the seq_num and duration data into appropriate
 * dpleaf data structure
 */
int32_t enterDataPair( sess_stream_id_t* id, 
			uint32_t seq_num, 
			uint32_t mfu_duration,
			FMTR_LIST_E fmtr){

	int32_t ret = 0;
	mfp_publ_t* pub_ctx;
	pub_ctx = live_state_cont->get_ctx(live_state_cont,
		id->sess_id);
	accum_ts_t* accum = (accum_ts_t*)pub_ctx->accum_ctx[id->stream_id];
	data_pair_root_t *dproot;

	dproot = lookupDpRoot(accum, fmtr);
	if(dproot == NULL){
		ret = -1;
		return(ret);
	}

	//check wr pointer in circular buffer
	//enter data at that location
	dproot->dpleaf[dproot->wr_index].mfu_seq_num = seq_num;
	dproot->dpleaf[dproot->wr_index].mfu_duration = mfu_duration;
	

	//increment write pointer - based on circular buffer condition	
	dproot->wr_index = dproot->wr_index + 1;
	if(dproot->wr_index >= dproot->max_len){
		dproot->wr_index = 0;
	}

	return (ret);

}

static void printValueAcrossStrms(sess_stream_id_t* id, 
			uint32_t seq_num, 
			FMTR_LIST_E fmtr){
	mfp_publ_t* pub_ctx;
	pub_ctx = live_state_cont->get_ctx(live_state_cont,
		id->sess_id);
	uint32_t strm_cnt = 0;
	data_pair_root_t * dproot;
	accum_ts_t* accum;
	int32_t rd_index;
	stream_param_t* stream_parm;
	uint32_t total_strm_cnt = pub_ctx->streams_per_encaps[0];
	
	//loop on streams_per_encaps[0]
	for(strm_cnt = 0;
	    strm_cnt < total_strm_cnt; 
	    strm_cnt++){

		accum = (accum_ts_t*)pub_ctx->accum_ctx[strm_cnt];
		stream_parm =  &(pub_ctx->stream_parm[strm_cnt]);

		if(stream_parm->stream_state == STRM_ACTIVE){
			dproot = lookupDpRoot(accum,fmtr);
			if(dproot == NULL){
				continue;
			}
			//check if value exists in the circular buffer
			rd_index = lookupDataPair(dproot,seq_num);
			if(rd_index != -1){
				DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE, 
				"sess %u strm %u seqnum %u ChunkDurn %u",
				id->sess_id, strm_cnt, 
				dproot->dpleaf[rd_index].mfu_seq_num,
				dproot->dpleaf[rd_index].mfu_duration);
			}
		}else if(stream_parm->stream_state == STRM_DEAD){
			total_strm_cnt++;
		}
	}
	return;
}

/* checkDataPair
 * function to check whether similar dpleaf entry exists for other streams in
 * session and take decision accordingly
 * return 1, if the allow decision is taken
 * return -1 for errors
 */

int32_t checkDataPair( sess_stream_id_t* id, 
			uint32_t seq_num, 
			uint32_t mfu_duration,
			FMTR_LIST_E fmtr){

	mfp_publ_t* pub_ctx;
	pub_ctx = live_state_cont->get_ctx(live_state_cont,
		id->sess_id);
	uint32_t strm_cnt = 0;
	int32_t decision = 1;
	data_pair_root_t * dproot;
	accum_ts_t* accum;
	int32_t rd_index;
	stream_param_t* stream_parm;
	uint32_t total_strm_cnt = pub_ctx->streams_per_encaps[0];
	
	//loop on streams_per_encaps[0]
	for(strm_cnt = 0;
	    strm_cnt < total_strm_cnt; 
	    strm_cnt++){

		accum = (accum_ts_t*)pub_ctx->accum_ctx[strm_cnt];
		stream_parm =  &(pub_ctx->stream_parm[strm_cnt]);

		if(stream_parm->stream_state == STRM_ACTIVE){

			//skip processing for same stream id
			if(strm_cnt == id->stream_id)
				continue;

			dproot = lookupDpRoot(accum,fmtr);
			if(dproot == NULL){
				decision = 1;
				continue;
			}

			//check if value exists in the circular buffer
			rd_index = lookupDataPair(dproot,seq_num);

			// if no, return 1
			if(rd_index == -1){
				decision = 1;
			} else {
				//if yes, check the value
				//allow if undefined or remain equal or within 10ms tolerance level
				//10ms in 90000 timescale is 900
				int32_t diff;
				diff = dproot->dpleaf[rd_index].mfu_duration - mfu_duration;
				if(((diff > -900) && (diff < 900)) ||
				(dproot->dpleaf[rd_index].mfu_duration == (uint32_t)-1)){
					//if equal or within tolerance level
					decision = 1;
				}else{
					//if not equal, raise error NKN_ASSERT, return -1
					// error can be treated to resync or kill session
					DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, 
					"sess %u Curstrm %u currChunkDurn %u Vs strm %u ChunkDurn %u",
					id->sess_id, id->stream_id, mfu_duration, 
					strm_cnt,dproot->dpleaf[rd_index].mfu_duration);
					printf("sess %u Curstrm %u currChunkDurn %u Vs strm %u ChunkDurn %u\n",
					id->sess_id, id->stream_id, mfu_duration, 
					strm_cnt,dproot->dpleaf[rd_index].mfu_duration);
					printValueAcrossStrms(id, seq_num, fmtr);
					decision = -1;
					//NKN_ASSERT(0);
					break;
				}
			}
		}else if(stream_parm->stream_state == STRM_DEAD){
			total_strm_cnt++;
		}
	}
	return(decision);
}

uint32_t calc_video_duration(void *mfudata){

	mfu_data_t *mfu_data = (mfu_data_t *)mfudata;

	mfu_contnr_t *mfu_contnr;
	mfu_contnr = parseMFUCont(mfu_data->data);
	int32_t video_index = -1;
	int32_t audio_index = -1;
	int32_t i;
	uint32_t tot_video_duration = 0;
	
	
        if (mfu_contnr == NULL){
		DBG_MFPLOG("DATAPAIR", ERROR, MOD_MFPLIVE, "unable to parse mfub");
		 return(0);
       	}
	for (i = 0; i < (int32_t)mfu_contnr->rawbox->desc_count; i++){
		if(!strncmp(mfu_contnr->datbox[i]->cmnbox->name, "vdat", 4))
		video_index = i;
		if(!strncmp(mfu_contnr->datbox[i]->cmnbox->name, "adat", 4))
		audio_index = i;
	}
        if (video_index == -1){
		DBG_MFPLOG("DATAPAIR", ERROR, MOD_MFPLIVE, "no video data in mfu");
		 return(0);
       	}

	for(i = 0; i < (int32_t)mfu_contnr->rawbox->descs[video_index]->unit_count; i++){
		tot_video_duration += mfu_contnr->rawbox->descs[video_index]->sample_duration[i];
	}
	mfu_contnr->del(mfu_contnr);
	return(tot_video_duration);
	
}


