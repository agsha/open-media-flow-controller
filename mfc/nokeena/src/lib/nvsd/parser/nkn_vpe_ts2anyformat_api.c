#include <stdio.h>
#include <sys/stat.h>
#include "nkn_vpe_ts2anyformat_api.h"
#include "nkn_vpe_ts2mfu_api.h"
#include "nkn_vpe_parser_err.h"

extern gbl_fruit_ctx_t fruit_ctx;
uint32_t glob_mfubox_ts_call = 0;
extern file_id_t* file_state_cont;

#define MAX_FDS_PROCESSED (10)

#define BITS_PER_Kb (1024)
#define BYTES_IN_PACKET (188)
#define NUM_BITS_IN_BYTES (8)
#define NUM_PKTS_IN_NETWORK_CHUNK (7)
#define MS_IN_SEC (1000)

#define MAX_PID_EST  (6)
#define MAX_PKTS_REQD_FOR_ESTIMATE (100)
#define EST_PKT_DEPTH (MAX_PKTS_REQD_FOR_ESTIMATE*TS_PKT_SIZE)
#define MAX_TO_READ_IN_SEC 20

#define ERR_FRUIT_FMTR (0x1)
#define ERR_SL_FMTR (0x2)

#if 1
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
#endif

#ifdef MEMDEBUG
#define A_(x) x 
#else
#define A_(x) 
#endif


static int32_t calc_bitrate(void *io_desc_ts, uint32_t inp_bitrate, io_handlers_t *iocb);
static void register_value_db(
                              uint32_t pkt_time,
                              uint32_t pid,
                              pid_map_t *pid_map,
                              uint32_t array_depth);

static void
cleanup_num_active_mfu2ts_task(int32_t **num_active_mfu2ts_task,
                               int32_t n_profiles);
static int32_t check_discontinuity(mfu_data_t* mfu_data){
	int32_t ret = VPE_SUCCESS;
	mfu_contnr_t *mfu_contnr;
	uint32_t sd0, sd1;
	int32_t video_index = -1, audio_index = -1;
	uint32_t i;
	
	mfu_contnr = parseMFUCont(mfu_data->data);
	
	for (i = 0; i < mfu_contnr->rawbox->desc_count; i++){
		if(!strncmp(mfu_contnr->datbox[i]->cmnbox->name, "vdat", 4))
		video_index = i;
		if(!strncmp(mfu_contnr->datbox[i]->cmnbox->name, "adat", 4))
		audio_index = i;
	}	      
	assert(video_index != -1);
	sd0 = mfu_contnr->rawbox->descs[video_index]->sample_duration[0];
	sd1 = mfu_contnr->rawbox->descs[video_index]->sample_duration[1];
	//check whether video bias occured
	if((sd0 - sd1) > sd1){
		ret = VPE_ERROR;
	}
	mfu_contnr->del(mfu_contnr);
	return (ret);
}

/* This function is used to allocate and initailize the context
   required for the conversion of TS to any format 
   fconv_params -> input parameter
   conv_ctx->output parameter */
int32_t
init_ts2anyformat_converter(file_conv_ip_params_t *fconv_params,
			    void **conv_ctx)
{
    ts2anyformat_converter_ctx_t *ts2anyformat_ctx = NULL;
    ts_mfu_builder_t *bldr_ts_mfu = NULL;
    ts_src_mfu_ts_builder_t *bldr_mfu_ts = NULL;
    int32_t err = 0, n_traks = 0;
    
    A_(printf("at init_ts2anyformat_converter %lu\n", nkn_memstat_type(mod_unknown)));
    //io_handlers_t *iocb = NULL;
    int32_t i = 0;
    *conv_ctx = ts2anyformat_ctx = (ts2anyformat_converter_ctx_t *)   \
        nkn_calloc_type(1, sizeof(ts2anyformat_converter_ctx_t),
                        mod_ts2anyforamt_converter_ctx_t);

    if(!ts2anyformat_ctx) {
        err = -E_VPE_PARSER_NO_MEM;
        return err;
    }
#if 0
    //choosing the which handler to use further
    if (strstr(fconv_params->input_path[0], "http://")) {
        iocb = &ioh_http;
    } else {
        iocb = &iohldr;
    }
#endif
    A_(printf("at init_ts2anyformat_converter %lu\n", nkn_memstat_type(mod_ts_mfu_builder_t)));
    A_(printf("at init_ts2anyformat_converter %lu\n", nkn_memstat_type(mod_vpe_charbuf)));


    //initialise the ts_mfu builder structure
    err = ts_mfu_builder_init(fconv_params->key_frame_interval,
			      fconv_params->input_path,
			      fconv_params->profile_bitrate,
			      fconv_params->streaming_type,
			      fconv_params->num_pmf,
			      fconv_params->n_input_files,
			      fconv_params->sess_id,
			      &bldr_ts_mfu);

    ts2anyformat_ctx->bldr_ts_mfu = bldr_ts_mfu;
    A_(printf("after ts_mfu_builder_init %lu\n", nkn_memstat_type(mod_ts_mfu_builder_t)));
    A_(printf("after ts_mfu_builder_init %lu\n", nkn_memstat_type(mod_vpe_charbuf)));
    if (err) {
        goto error;
    }
#if 0
    //update the ts_mfu builder structure
    err = ts_mfu_bldr_update(bldr_ts_mfu,
			     bldr_ts_mfu->n_profiles);
    if (err) {
        goto error;
    }
#endif
    /*based on the status , initialiseing the either ts builder or
    moof builder */
    if(fconv_params->ms_param_status) {
	err = init_ts_src_mfu2ts_builder(fconv_params->key_frame_interval,
					 &bldr_mfu_ts,
					 bldr_ts_mfu->chunk_time, 
					 fconv_params->n_input_files);
	ts2anyformat_ctx->bldr_mfu_ts = bldr_mfu_ts;
	if (err) {
	    goto error;
	}
    } else {
	ts2anyformat_ctx->bldr_mfu_ts = NULL;
    }
    if(fconv_params->ss_param_status) {
	ts2anyformat_ctx->slfmtr =
	    createSLFormatter(fconv_params->sess_name, 
				fconv_params->output_path, 0, 0, 0, NULL, NULL);
	if (ts2anyformat_ctx->slfmtr == NULL) {
	  err = -E_VPE_SL_FMT_INIT_ERR;
	    goto error;
	}
    } else
	ts2anyformat_ctx->slfmtr = NULL;
    

    ts2anyformat_ctx->ms_parm_status =
	fconv_params->ms_param_status;
    ts2anyformat_ctx->ss_parm_status =
	fconv_params->ss_param_status;
    ts2anyformat_ctx->sess_id = fconv_params->sess_id;
    return err;
 error:
    //printf("the error is %d \n" , err);
    cleanup_ts_mfu_builder(ts2anyformat_ctx->bldr_ts_mfu);
    cleanup_ts_src_mfu_ts_builder(ts2anyformat_ctx->bldr_mfu_ts);
    A_(printf("after cleanup %lu\n", nkn_memstat_type(mod_ts_mfu_builder_t)));
    A_(printf("after cleanup %lu\n", nkn_memstat_type(mod_vpe_charbuf)));
    if(ts2anyformat_ctx)
	free(ts2anyformat_ctx);
    return err;
}


/* This fucntion is used to convert TS to any foramt(ts/MOOF).
first chunk of all available video traks is converted, then second
chunk of all video traks,.... till availble number of chunks. */ 
int32_t
ts2anyformat_conversion(void *private_data, void **out)
{

    ts2anyformat_converter_ctx_t *conv_ctx = NULL;
    ts_mfu_builder_t *bldr_ts_mfu = NULL;
    ts_src_mfu_ts_builder_t *bldr_mfu_ts = NULL;
    sl_fmtr_t *slfmtr = NULL;
    int32_t err = 0, bytes_read = 0;
    uint32_t max_chunk =0, curr_chunk = 0;
    uint32_t max_profile = 0, curr_profile = 0;
    mfu_data_t* mfu_data = NULL;
    uint32_t num_mfu = 0, min_chunk_num = 0, k=0;
    uint32_t i = 0, read_size = 0, span_data_len = 0;
    int32_t l =0, sl_err = 0, ts_err = 0;
    uint8_t *data = NULL;
    uint8_t temp = 1;
    uint32_t temp_loop_cnt = 0, one_prof_data_complete = 0;
    uint32_t total_read_size = 0, fmtr_err_flag = 0;
    mfp_publ_t *pub_ctx = NULL;

    A_(printf("At ts2anyformat_conversion %lu\n",nkn_memstat_type(mod_unknown)));
    conv_ctx = (ts2anyformat_converter_ctx_t*)(private_data);
    
    pub_ctx = file_state_cont->get_ctx(file_state_cont, conv_ctx->sess_id);


    bldr_ts_mfu = conv_ctx->bldr_ts_mfu;
    bldr_mfu_ts = conv_ctx->bldr_mfu_ts;
    slfmtr = conv_ctx->slfmtr;
    max_profile = bldr_ts_mfu->n_profiles;
    conv_ctx->max_profile = max_profile;
    /*allocating memory for ref two dimensional array and ref count
      two dimensional array */
    conv_ctx->mfu_data = nkn_calloc_type(max_profile,
                                         sizeof(mfu_data_t**),
                                         mod_vpe_mfu_data_t);
    if(!conv_ctx->mfu_data) {
      err = -E_VPE_PARSER_NO_MEM;
      goto error;
    }

    conv_ctx->ref_cont = nkn_calloc_type(max_profile,
                                         sizeof(ref_count_mem_t**),
                                         mod_vpe_mfu_data_t);
    if(!conv_ctx->ref_cont) {
      err = -E_VPE_PARSER_NO_MEM;
      goto error;
    }

    /* calculate the bitrate of two streams */
    for(l=0; l< bldr_ts_mfu->n_profiles; l++) {
      ts_mfu_profile_builder_t *curr_prof = &bldr_ts_mfu->profile_info[l];
      err = calc_bitrate(curr_prof->io_desc_ts, 
			 curr_prof->bitrate, bldr_ts_mfu->profile_info[l].iocb);
      if(err < 0) {
	goto error;
      } else {
	curr_prof->act_bit_rate = err;
      }
      curr_prof->num_pkts_per_second = (uint32_t) (curr_prof->act_bit_rate * BITS_PER_Kb)/
	( 188 * NUM_BITS_IN_BYTES);//bytes per second
      curr_prof->to_read = curr_prof->num_pkts_per_second * MAX_TO_READ_IN_SEC * 188;
      //curr_prof->to_read = curr_prof->to_read + (curr_prof->to_read /20);//increasing 5%
    }
    //FILE * fp = NULL;
    //fp  = fopen("example.txt", "wb");

    while(1) {
      temp = 1;
      temp_loop_cnt++;
      max_chunk = 0;
      for(l=0; l< bldr_ts_mfu->n_profiles; l++) {
	ts_mfu_profile_builder_t *curr_prof = &bldr_ts_mfu->profile_info[l];
	//curr_prof->is_span_data = 0;
	//reading the data of size TS2IPHONE_READ_SIZE
	if (curr_prof->bytes_remaining == 0) {
	  curr_prof->is_data_complete = 1;
	  continue;
	} 
	read_size = curr_prof->to_read + curr_prof->prev_bytes;
	if (curr_prof->bytes_remaining < (int32_t)read_size/*curr_prof->to_read*/) {
	  read_size = curr_prof->bytes_remaining;
	}
	data = (uint8_t*)			\
	  nkn_calloc_type(1, read_size,
			  mod_vpe_media_buffer);
	if(!data) {
	  err = -E_VPE_PARSER_NO_MEM;
	  goto error;
	}
	//fprintf(fp,"%d, %d, %d \n",read_size, curr_prof->bytes_remaining, curr_prof->prev_bytes); 
	curr_prof->iocb->ioh_seek(curr_prof->io_desc_ts, curr_prof->seek_offset, SEEK_SET);
	bytes_read = curr_prof->iocb->ioh_read(data, 1, read_size, curr_prof->io_desc_ts);
	if(bytes_read != (int32_t)read_size) {
	  err = -E_VPE_READ_INCOMPLETE;
	  if(data != NULL) {
	    free(data);
	    data = NULL;
	  }
	  goto send_eos;
	}
	total_read_size += read_size;
	curr_prof->bytes_remaining -= read_size;
	curr_prof->data = data;
	curr_prof->data_len = read_size;
	/* this function will get the number of chunks present in this 
	 * block of data and also give the offset and length of each chunk */
	//printf("stream %d create chunk desc\n", l);
	err = ts_mfu_bldr_get_chunk_desc(curr_prof);
	if(err < 0) {
	  if(data != NULL) {
            free(data);
	    data = NULL;
          }
	  cleanup_num_active_mfu2ts_task(bldr_mfu_ts->num_active_mfu2ts_task, 
					 bldr_ts_mfu->n_profiles);
	  goto error;
	}
	//curr_prof->last_samp_len = curr_prof->chunk_desc[curr_prof->num_chunks-1]->sam_desc->length;
	
	curr_prof->block_num++;
      }
      /* to find the minimum chunk number for all the profiles 
       * in the read block of data*/
      min_chunk_num = bldr_ts_mfu->profile_info[0].num_chunks;
      for(l=1; l< bldr_ts_mfu->n_profiles; l++) {
	if(min_chunk_num > bldr_ts_mfu->profile_info[l].num_chunks) {
	  min_chunk_num = bldr_ts_mfu->profile_info[l].num_chunks;
	}
      }
      for(l=0; l< bldr_ts_mfu->n_profiles; l++) {
	ts_mfu_profile_builder_t *curr_prof = &bldr_ts_mfu->profile_info[l];
	/* if the block read is the last block of data for given profile,
	 * then number of chunks tobe processed is the total number of chunks
	 *, if the block read is not the last block in that profile, then 
	 * last chunk is carried as the first chunk for next block of data*/
	if(curr_prof->is_data_complete == 1) {
	  continue;
	}
	if(min_chunk_num != 0) {
	  if(curr_prof->bytes_remaining == 0) {
	    max_chunk = min_chunk_num;
	  } else {
	    max_chunk = min_chunk_num - 1;
	  }
	} else {
	  max_chunk = 0;
	}
	conv_ctx->mfu_data[l] = nkn_calloc_type(max_chunk,
                                                sizeof(mfu_data_t*),
                                                mod_vpe_mfu_data_t);
	if(!conv_ctx->mfu_data[l]) {
          err = -E_VPE_PARSER_NO_MEM;
	  goto error;
        }
	
        conv_ctx->ref_cont[l] = nkn_calloc_type(max_chunk,
                                                sizeof(ref_count_mem_t*),
                                                mod_vpe_mfu_data_t);
	if(!conv_ctx->ref_cont[l]) {
          err = -E_VPE_PARSER_NO_MEM;
          goto error;
        }
	/* to calculate the last_sam_len, this sample_length is carried with the 
	* next block of data, these chunks are not processed in this loop and 
	* carried for next blcok, so freeing this here*/
	uint32_t span_chunk_num = curr_prof->num_chunks - max_chunk;
	curr_prof->last_samp_len = 0;
	for(k=max_chunk; k<curr_prof->num_chunks; k++) {
	  curr_prof->last_samp_len += curr_prof->chunk_desc[k]->sam_desc->length;
	  if(curr_prof->chunk_desc[k] != NULL) {
	    if(curr_prof->chunk_desc[k]->sam_desc != NULL){
	      free(curr_prof->chunk_desc[k]->sam_desc);
	      curr_prof->chunk_desc[k]->sam_desc = NULL;
	    }
	    free(curr_prof->chunk_desc[k]);
	    curr_prof->chunk_desc[k] = NULL;
	  }
	}
	/* the last chunk length is nothing but the data after
         * the last IDR in the given block of size which is copied to next block size */
        if (curr_prof->bytes_remaining != 0) {
          curr_prof->is_span_data = 1;
          //span_data_len = curr_prof->chunk_desc[max_chunk-1]->sam_desc->length;
          span_data_len = curr_prof->last_samp_len;
          curr_prof->seek_offset = (curr_prof->block_num*curr_prof->to_read)
	    - span_data_len - TS_PKT_SIZE ;
          curr_prof->bytes_remaining = curr_prof->bytes_remaining + span_data_len + TS_PKT_SIZE;
          curr_prof->prev_bytes = span_data_len + TS_PKT_SIZE;
        }
      }//for max_profile
      /*check whether all the profile data's are non-empty, 
       * otherwise send eos */
      for(l=0; l< bldr_ts_mfu->n_profiles; l++) {
	if(bldr_ts_mfu->profile_info[l].is_data_complete == 1) {
	  one_prof_data_complete = 1;
	  goto send_eos;
	}
      }
      curr_chunk = 0;
      while(curr_chunk < max_chunk) {
	  curr_profile = 0;
	  while(curr_profile < max_profile) {
	      /**
	       * a. if the previous mfu2ts or mfu2sl hit a formatter
	       * error, do not process this chunk
	       * b. if there are unequal number of I-frames in the
	       * accumulated data across profiles then we should not
	       * process the profile which has lesser number of I-frames
	       * this is marked with an empty chunk_desc
	       */
	      	DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPFILE,
			"sess %u stream %u: processing chunk %u"
			, conv_ctx->sess_id, curr_profile, 
			bldr_ts_mfu->profile_info[curr_profile].tot_num_chunks);	       
		if(bldr_ts_mfu->profile_info[curr_profile].is_data_complete == 1) {
		  curr_profile++;
		  continue;
		}
		if (fmtr_err_flag || 
		    (!bldr_ts_mfu->profile_info[curr_profile].chunk_desc[curr_chunk])) {
		  /* fmtr error skip conversion */
		  curr_profile++;
		  continue;
		}
	      bldr_ts_mfu->profile_info[curr_profile].curr_chunk_num = curr_chunk;
	      //ts to single MFU conversion
	      err =
		  ts_mfu_conversion(&bldr_ts_mfu->profile_info[curr_profile],
				    &mfu_data, conv_ctx->sess_id, curr_profile);
	      if (err == E_VPE_FILE_CONVERT_STOP) {
		  //goto done;
		  curr_profile++;
		  continue;
	      }else if (err < 0) {
	        DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPFILE,
		      "sess %u stream %u: ts_mfu_conversion error code %d"
		      ,conv_ctx->sess_id, curr_profile, err);		     
		if(mfu_data != NULL) {
		  if(mfu_data->data != NULL) {
		    free(mfu_data->data);
		  }
		  free(mfu_data);
		}
		  goto send_eos;
	      }
	      num_mfu++;
	      if(check_discontinuity(mfu_data)== VPE_ERROR){
	      	DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPFILE,
			"sess %u stream %u: Video track shows timestamp gap"
			, conv_ctx->sess_id, curr_profile);
		if(mfu_data != NULL) {
                  if(mfu_data->data != NULL) {
                    free(mfu_data->data);
                  }
                  free(mfu_data);
                }
		err = -E_VPE_PARSER_PAYLOAD_TIMESTAMP_GAP;
	      	goto send_eos;
	      }
	      	
	      
	      /*conv_ctx->mfu_data is two dimensional. for each MFU,
		corrsponding refcount is created to avoid the deltion of
		the MFU memory when it is used by other conversions */
	      conv_ctx->mfu_data[curr_profile][curr_chunk] =
		  mfu_data;
	      ref_count_mem_t* ref_cont =
		  createRefCountedContainer(mfu_data, destroyMfuData);
	      conv_ctx->ref_cont[curr_profile][curr_chunk] = ref_cont;
	      if (conv_ctx->ms_parm_status){
		  apple_fmtr_hold_encap_ctxt(get_sessid_from_mfu(mfu_data));
		  ref_cont->hold_ref_cont(ref_cont);
	      }
	      if (conv_ctx->ss_parm_status) {
		  ref_cont->hold_ref_cont(ref_cont);
		  ((mfu_data_t*)(ref_cont->mem))->sl_fmtr_ctx = slfmtr;
	      }
	    
	      if (conv_ctx->ms_parm_status) {
		  /*conversion takes place only when the number of MFU's
		    required are created. suppose the actual I frame
		    interval is 2 seconds and the key frame interval
		    specified in the PMF is 4 sec, then  no:of:MFU's
		    required to form single TS/MOOF fragment is 2*/
		  if(num_mfu == bldr_mfu_ts->mfu2ts_conv_req_num_mfu) {
		      ts_err = ts_mfu2ts_conversion(conv_ctx->bldr_mfu_ts,
						 conv_ctx->ref_cont[curr_profile],
						 bldr_mfu_ts->mfu2ts_conv_req_num_mfu,
						 curr_profile,
						 bldr_ts_mfu->profile_info[curr_profile].tot_num_chunks,
						 max_profile,
						 bldr_mfu_ts->num_active_mfu2ts_task, curr_chunk);
		
		      if (ts_err < 0) {
			  fmtr_err_flag |= ERR_FRUIT_FMTR;
			  err = ts_err;
		      }
		      bldr_mfu_ts->mfu2ts_conv_processed[curr_profile]++;
		      bldr_ts_mfu->profile_info[curr_profile].tot_num_chunks++;
		      num_mfu = 0;
		
		  }
	      }
	      if(conv_ctx->ss_parm_status) {
		sl_err = slfmtr->handle_mfu(ref_cont);
		if (sl_err < 0) {
		  fmtr_err_flag |= ERR_SL_FMTR;
		  err = -E_VPE_MFU2SL_ERR;
		} 
	      }
	      //register stream dead when any one of the formatter has failed
	      if((pub_ctx->op == PUB_SESS_STATE_STOP) || (ts_err < 0))
		err = ts_err;

	      if ((pub_ctx->op == PUB_SESS_STATE_STOP) || (fmtr_err_flag)) {
		/*it enters into this if condition if eithr one of formatter has
		 * failed or error occured in the mfu2iphone through fmt_callback*/
		if(err == 0) {
		  fmtr_err_flag |= ERR_FRUIT_FMTR;//PUB_STOP maybe beacuse of err in apple formatter
		  err = -E_VPE_FORMATTER_ERROR;
		}
		for(k = 0; k < max_profile; k++){
		  register_stream_dead(conv_ctx->sess_id, k);
		  }
		DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,
			   "called register_stream_dead\n");
		goto send_eos;
	      }
	      curr_profile++;
	  }
	  curr_chunk++;
	  if (fmtr_err_flag) {
	      /* if we hit a formatter error send eos */
	      goto send_eos;
	  }
      }

    send_eos:
      for(l=0; l< bldr_ts_mfu->n_profiles; l++) {
	  ts_mfu_profile_builder_t *curr_prof = &bldr_ts_mfu->profile_info[l];

	  cleanup_chunk_desc(bldr_ts_mfu, l);
	  
	  if(conv_ctx->mfu_data[l] != NULL) {
	      free(conv_ctx->mfu_data[l]);
	      conv_ctx->mfu_data[l] = NULL;
	  }
	  if(conv_ctx->ref_cont[l] != NULL) {
	      free(conv_ctx->ref_cont[l]);
	      conv_ctx->ref_cont[l] = NULL;
	  }
	  if(curr_prof->data != NULL) {
	      free(curr_prof->data);
	      curr_prof->data = NULL;
	  }
	  curr_prof->num_chunks = 0;
	
      }//for number of profiles
      if (fmtr_err_flag || err < 0) {
	  goto done;
      }
#if 0
      // to check whether the data is complete for all the profiles
      for(l=0; l<bldr_ts_mfu->n_profiles; l++) {
	  temp &= bldr_ts_mfu->profile_info[l].is_data_complete;
      }
      bldr_ts_mfu->is_data_complete = temp;
      //once all the data for all profile is completed , start published NULL MFUs
      if(bldr_ts_mfu->is_data_complete) {
	  goto done;
      }
#else
      //check whether one profile data is empty, if so exit by sending eos
      if(one_prof_data_complete == 1) {
	goto done;
      }
      /*for(l=0; l<bldr_ts_mfu->n_profiles; l++) {
	if(bldr_ts_mfu->profile_info[l].is_data_complete == 1)
	  goto done;
	  }*/

#endif
    }//while(1)
    //fclose(fp);

 done:
    curr_profile = 0;
    //sending dummy mfu
    while(curr_profile < max_profile) {
	//creating dummy mfu's
	mfu_data = (mfu_data_t*)nkn_calloc_type(1,
						sizeof(mfu_data_t),
						mod_vpe_mfu_data_t);
	if (mfu_data == NULL)
	    return -E_VPE_PARSER_NO_MEM;
	mfub_t* mfu_hdr = nkn_calloc_type(1, sizeof(mfub_t),
					  mod_vpe_mfu_data_t);
	if (mfu_hdr == NULL) {
	    free(mfu_data);
	    return -E_VPE_PARSER_NO_MEM;
	}
	/* allocate for the mfu */
	mfu_data->data_size = MFU_FIXED_SIZE + sizeof(mfub_t);
	mfu_data->data = (uint8_t*)
	    nkn_malloc_type(mfu_data->data_size,
			    mod_vpe_mfp_ts2mfu_t1);
	if (!mfu_data->data) {
	    return -E_VPE_PARSER_NO_MEM;
	}
	mfu_hdr->offset_vdat = 0xffffffffffffffff;
	mfu_hdr->offset_adat = 0xffffffffffffffff;
	mfu_hdr->stream_id = curr_profile;
	mfu_hdr->mfu_size =  mfu_data->data_size;
	mfu_hdr->program_number = bldr_ts_mfu->num_pmf;
	/* write the mfu header */
	mfu_write_mfub_box(sizeof(mfub_t),
			   mfu_hdr,
			   mfu_data->data);
	ref_count_mem_t* ref_cont =
	    createRefCountedContainer(mfu_data, destroyMfuData);
	if((conv_ctx->ms_parm_status)) {
	    apple_fmtr_hold_encap_ctxt(get_sessid_from_mfu(mfu_data));
	    ref_cont->hold_ref_cont(ref_cont);
	}
	if((conv_ctx->ss_parm_status)) {
	    ref_cont->hold_ref_cont(ref_cont);
	}
	if((conv_ctx->ms_parm_status)) {
	    fruit_fmt_input_t *fmt_input = NULL;
	    fmt_input = nkn_calloc_type(1,
					sizeof(fruit_fmt_input_t),
					mod_vpe_mfp_fruit_fmt_input_t);
	    if (!fmt_input) {
		return -E_MFP_NO_MEM;
	    }
	    
	    fmt_input->state = FRUIT_FMT_CONVERT;
	    fmt_input->mfu_seq_num = bldr_ts_mfu->profile_info[curr_profile].tot_num_chunks;
	    fmt_input->streams_per_encap = max_profile;

	    fmt_input->active_task_cntr = bldr_mfu_ts->num_active_mfu2ts_task[curr_profile];
            (*fmt_input->active_task_cntr)++;
	    //printf("seq_num is %d \n", fmt_input->mfu_seq_num);
	    //printf(" the active_tsk_cntr add is %p \n", fmt_input->active_task_cntr);
	    //printf(" the active_tsk_cntr val is %d \n", *fmt_input->active_task_cntr);
	    //fmt_input->encr_props = NULL; /*need to fill
	    //                      this */
	    mfu_data->fruit_fmtr_ctx = (void*)fmt_input;
	    //usleep(1000);
	    ts_err = mfubox_ts(ref_cont);
	    if(ts_err == VPE_ERROR) {
	      ts_err = -E_VPE_TS_CREATION_FAILED;
	      if(mfu_hdr != NULL)
                free(mfu_hdr);
	      //return err;
	    } else {
	      ts_err = 0;
	    }
	}
	if ((conv_ctx->ss_parm_status)) {
	    ((mfu_data_t*)(ref_cont->mem))->sl_fmtr_ctx = slfmtr;
	    sl_err = slfmtr->handle_mfu(ref_cont);
	    if (sl_err < 0) {
	      err = -E_VPE_MFU2SL_ERR;
	      printf("the sl_err for null mfus is %d", sl_err);
	      if(mfu_hdr != NULL)
		free(mfu_hdr);
	      return err;
	    }
	}
	//register stream dead when any one of the formatter has failed
	if((pub_ctx->op == PUB_SESS_STATE_STOP) || (ts_err < 0)) {
	  err = -E_VPE_FORMATTER_ERROR;
	  if(mfu_hdr != NULL)
            free(mfu_hdr);
	  return err;
	}
#if 0
	if(sl_err < 0)
	  err = sl_err;
	if ((pub_ctx->op == PUB_SESS_STATE_STOP) || (fmtr_err_flag)) {
	  /*it enters into this if condition if eithr one of formatter has
	   * failed or error occured in the mfu2iphone through fmt_callback*/
	  if(err == 0) {
	    err = -E_VPE_FORMATTER_ERROR;
	  }
	  for(k = 0; k < max_profile; k++){
	    register_stream_dead(conv_ctx->sess_id, k);
	  }
	  DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,
		     "called register_stream_dead\n");
	  goto error;
	}
#endif
	if(mfu_hdr != NULL)
	    free(mfu_hdr);
	curr_profile++;
    }
    /* in some cases,bldr_mfu_ts->num_active_mfu2ts_task is being 
       cleared by cleanup function, before it is being used in mfu2iphone.c */
    //sleep(20);
    if(err < 0)
    	return err;
    err = E_VPE_FILE_CONVERT_STOP;
 error:
    return err;

}

/* This function is used to convert mfu's t corresponding fragments.
it takes "req_num_mfu" number of MFU's to form single Ts fragment 
bldr_mfu_ts-> i/p paramnet
ref_cont -> i/p which has the MFU memory
req_num_mfu -> i/p
curr_vid_trak -> i/p parameter*/
int32_t 
ts_mfu2ts_conversion(ts_src_mfu_ts_builder_t *bldr_mfu_ts,
		     ref_count_mem_t **ref_cont, uint32_t req_num_mfu,
		     uint32_t curr_vid_trak, uint32_t total_chunk_num,
		     uint32_t max_profile, int32_t **num_active_mfu2ts_task,
		     uint32_t curr_chunk) 
{
    int32_t err = 0;
    uint32_t i = 0;
    ref_count_mem_t *ref_count_temp = NULL;
    uint32_t processed =
    bldr_mfu_ts->mfu2ts_conv_processed[curr_vid_trak];
    mfu_data_t *mfu_data = NULL;
    if(req_num_mfu == 1) {
	fruit_fmt_input_t *fmt_input = NULL;
        fmt_input = nkn_calloc_type(1,
                                    sizeof(fruit_fmt_input_t),
                                    mod_vpe_mfp_fruit_fmt_input_t);
        if (!fmt_input) {
            return -E_MFP_NO_MEM;
        }

        fmt_input->state = FRUIT_FMT_CONVERT;
        fmt_input->mfu_seq_num = total_chunk_num;
        fmt_input->streams_per_encap = max_profile;
	fmt_input->active_task_cntr = 
	  bldr_mfu_ts->num_active_mfu2ts_task[curr_vid_trak];
        (*fmt_input->active_task_cntr)++;

	//printf("seq_num is %d \n", fmt_input->mfu_seq_num);
	//printf(" the active_tsk_cntr add is %p \n", fmt_input->active_task_cntr);
	//printf(" the active_tsk_cntr val is %d \n", *fmt_input->active_task_cntr);
        //fmt_input->encr_props = NULL; /*need to fill*/
                                        
        //ref_count_temp  = ref_cont[req_num_mfu*processed] ;
	ref_count_temp = ref_cont[curr_chunk];
        mfu_data = (mfu_data_t*)ref_count_temp->mem;
        mfu_data->fruit_fmtr_ctx = (void*)fmt_input;

        err = mfubox_ts(ref_count_temp);
        if(err == VPE_ERROR) {
            //if(fmt_input)
	    //  free(fmt_input);
            //ref_count_temp->release_ref_cont(ref_count_temp);
            err = -E_VPE_TS_CREATION_FAILED;
            return err;
        } else {
            err = 0;
        }
    } else {
        /*this part has tobe written further. This is the case where
	  morehtan one MFU cotributes single Ts fragments*/
        for(i=0; i< req_num_mfu; i++) {


        }
    }
    return err;
}

/* cleanup fucntion for the elements used for TS to any format
   structure*/
void
ts2anyformat_cleanup_converter(void *private_data)
{
    ts2anyformat_converter_ctx_t *conv_ctx;
    int32_t i = 0;

    conv_ctx = (ts2anyformat_converter_ctx_t*)private_data;
    
    if (conv_ctx) {
	if(conv_ctx->ms_parm_status || conv_ctx->ss_parm_status)
	    cleanup_ts_mfu_builder(conv_ctx->bldr_ts_mfu);
	if(conv_ctx->ss_parm_status) {
	    if(conv_ctx->slfmtr)
		conv_ctx->slfmtr->delete_ctx(conv_ctx->slfmtr);
	}
	cleanup_ts_src_mfu_ts_builder(conv_ctx->bldr_mfu_ts);
	for(i=0; i<(int32_t)conv_ctx->max_profile; i++) {
	    free(conv_ctx->mfu_data[i]);
	    free(conv_ctx->ref_cont[i]);
	}
	free(conv_ctx->mfu_data);
	free(conv_ctx->ref_cont);
	free(conv_ctx);
    }
   A_(printf("at ts2anyformat_cleanup_converter %lu\n", nkn_memstat_type(mod_unknown)));
}

/* This function is used to allocate and initialisse the builder
   structure  used for mfu to Ts conversion
   chunk_time -> i/p key frame interval specified in the PMF
   out -> o/p builder structure which is updated
   actual_frame_interval -> actual I frame interval
*/
int32_t 
init_ts_src_mfu2ts_builder(int32_t chunk_time,
			   ts_src_mfu_ts_builder_t **out, uint32_t
			   actual_frame_interval, int32_t n_profiles)
{
    ts_src_mfu_ts_builder_t *bldr = NULL;
    int32_t err = 0, i=0;
    *out = bldr = (ts_src_mfu_ts_builder_t*)
        nkn_calloc_type(1, sizeof(ts_mfu_builder_t),
                        mod_ts_mfu_builder_t);
    if (!bldr) {
        return -E_VPE_PARSER_NO_MEM;
    }
    bldr->num_active_mfu2ts_task = nkn_calloc_type(MAX_TRACKS,
                                                   sizeof(int32_t*),
                                                   mod_vpe_temp_t1);
    if(bldr->num_active_mfu2ts_task == NULL)
      return -E_VPE_PARSER_NO_MEM;
    for(i=0; i< n_profiles; i++) {
      bldr->num_active_mfu2ts_task[i] = nkn_calloc_type(1,
                                                        sizeof(int32_t),
                                                        mod_vpe_temp_t2);
      if(bldr->num_active_mfu2ts_task[i] == NULL)
        return -E_VPE_PARSER_NO_MEM;
    }
   
    //bldr->mfu2ts_conv_processed = 1;
    bldr->mfu2ts_conv_req_num_mfu = 1;
#if 0 				
    /* needs to be activated when we segment based on the segment
     * duration and not at as every key frame as we are doing
     * currently 
     */ 
    if(actual_frame_interval > (uint32_t)chunk_time) {
	bldr->mfu2ts_conv_req_num_mfu = 1;
    } else {
	bldr->mfu2ts_conv_req_num_mfu =
	    chunk_time / actual_frame_interval;
    }
#endif

    return err;
}

void
cleanup_ts_src_mfu_ts_builder(ts_src_mfu_ts_builder_t* bldr)
{
    if(bldr) {
      if(bldr->num_active_mfu2ts_task)
        free(bldr->num_active_mfu2ts_task);
      free(bldr);
    }
    
    
}
static void
cleanup_num_active_mfu2ts_task(int32_t **num_active_mfu2ts_task, 
			       int32_t n_profiles)
{
  int32_t i =0;
  for(i=0; i<n_profiles; i++) {
    if(num_active_mfu2ts_task[i] != NULL) {
      free(num_active_mfu2ts_task[i]);
      num_active_mfu2ts_task[i] = NULL;
    }
  }
  
}

//-------------------------------------------------------
//calc_bitrate
//statically, estimate the bitrate of the given file
//-------------------------------------------------------
static int32_t calc_bitrate(void* io_desc_ts, uint32_t inp_bitrate, 
			     io_handlers_t *iocb)
{
  uint32_t bitrate_est = 0;
  uint32_t file_size;
  int32_t start_pos, end_pos;
  size_t bytes_to_read = 0, tot_bytes_read = 0;

  pid_map_t *pid_map = NULL;

  uint32_t pkt_time;
  uint16_t pid;
  int32_t ret = 0;
  uint32_t bytes_read = 0;
  uint32_t duration;

  uint8_t *init_data = NULL;
  uint8_t *final_data = NULL;
  uint32_t i;
  uint32_t dts, pts;

  pid_map = (pid_map_t *)nkn_calloc_type(MAX_PID_EST,
					      sizeof(pid_map_t), mod_mfp_file_pump);
  if(pid_map == NULL){
    //unable to allocate mem
    ret = -E_VPE_PARSER_NO_MEM;
    //on failure, assign the user input bitrate
    bitrate_est = -1;
    goto calc_bitrate_ret;
  }
  init_data = (uint8_t *) nkn_malloc_type(EST_PKT_DEPTH * sizeof(uint8_t),
					  mod_mfp_file_pump);
  if(init_data == NULL){
    //unable to allocate mem
    ret = -E_VPE_PARSER_NO_MEM;
    //on failure, assign the user input bitrate
    bitrate_est = -1;
    goto calc_bitrate_ret;
  }
  final_data= (uint8_t *) nkn_malloc_type(EST_PKT_DEPTH * sizeof(uint8_t),
					  mod_mfp_file_pump);
  if(final_data == NULL){
    //unable to allocate mem
    ret = -E_VPE_PARSER_NO_MEM;
    //on failure, assign the user input bitrate
    bitrate_est = -1;
    goto calc_bitrate_ret;
  }

  //memset(pid_map, 0, MAX_PID_EST*sizeof(pid_ppty_map_t));

  //start_pos = iocb->ioh_seek(io_desc_ts, 0, SEEK_CUR);

  //end_pos = iocb->ioh_seek(io_desc_ts, 0, SEEK_END);
  iocb->ioh_seek(io_desc_ts, 0, SEEK_END);
  file_size = iocb->ioh_tell(io_desc_ts);

  iocb->ioh_seek(io_desc_ts, -EST_PKT_DEPTH, SEEK_END);
  bytes_read = iocb->ioh_read(final_data, 1, EST_PKT_DEPTH, io_desc_ts);
  if(bytes_read !=EST_PKT_DEPTH){
    //failed to read enough packets
    ret = -1;
    //on failure, assign the user input bitrate
    bitrate_est = -1;
    goto calc_bitrate_ret;
  }
  //calc file size
  //file_size = end_pos - start_pos;

  //calc duration
  iocb->ioh_seek(io_desc_ts, 0, SEEK_SET);
  bytes_to_read = EST_PKT_DEPTH;
  while(1){
      if (tot_bytes_read + EST_PKT_DEPTH > file_size) {
	  int uu = 0;
	  bytes_to_read = file_size - tot_bytes_read;
      } else {
	  bytes_to_read = EST_PKT_DEPTH;
      }
    if(!bytes_to_read){
      ret = -1;
      bitrate_est = -1;
      goto calc_bitrate_ret;
    }
    bytes_read = iocb->ioh_read(init_data, 1, EST_PKT_DEPTH, io_desc_ts);
    if(bytes_read != bytes_to_read){
      //failed to read enough packets
      ret = -1;
      bitrate_est = -1;
      goto calc_bitrate_ret;
    }
    tot_bytes_read += bytes_read;
    for (i = 0; i < MAX_PKTS_REQD_FOR_ESTIMATE; i++){
      pts = 0;
      dts = 0;
      ret = mfu_converter_get_timestamp(
				  (uint8_t *)(init_data+(i * BYTES_IN_PACKET)),NULL,&pid,
				  &pts, &dts, NULL);
      if(ret == -E_VPE_NO_SYNC_BYTE){
	//on failure, assign the user input bitrate
	bitrate_est = -1;
	 goto calc_bitrate_ret;
      }
      pkt_time = dts;
      if(!dts)
	pkt_time = pts;
      if(pkt_time){
	register_value_db(pkt_time, (uint32_t)pid, pid_map, MAX_PID_EST);
      }
    }
    if(pid_map->entry_valid /*&& (pid_map + 1)->entry_valid*/)
      break;
  }
  uint32_t offset = 0;
  for (i = 0; i < MAX_PKTS_REQD_FOR_ESTIMATE; i++){
    pts = 0;
    dts = 0;
    uint32_t tmp;
    while(1){
      tmp = *(final_data+(i * BYTES_IN_PACKET)+offset);
      if( tmp == 0x47)
	break;
      offset += 1;
    }
    ret = mfu_converter_get_timestamp(
				      (uint8_t *)(final_data+(i * BYTES_IN_PACKET)+offset),NULL,&pid,
				      &pts, &dts, NULL);
    if(ret == -E_VPE_NO_SYNC_BYTE){
      return ret;
    }
    pkt_time = dts;
    if(!dts)
      pkt_time = pts;
    if(pkt_time){
      register_value_db(pkt_time, (uint32_t)pid, pid_map, MAX_PID_EST);
    }
  }
  duration = 0;
  //calc the max duration among the diff PIDs
  pid_map_t *l_pid_map = pid_map;
  for(i = 0; i < MAX_PID_EST; i++){
    if(l_pid_map->entry_valid){
      if(l_pid_map->max_duration > duration)
	duration = l_pid_map->max_duration;
    }
    l_pid_map++;
  }
  if(!duration){
    //unable to find duration
    ret = -1;
    //on failure, assign the user input bitrate
    bitrate_est = -1;
    goto calc_bitrate_ret;
  }

  //duration is in milli seconds; divided by 90 to move to ms
  duration = duration / 90;

  //bitrate is in kbps
  bitrate_est = (uint32_t)
    (((uint64_t)((uint64_t)file_size * NUM_BITS_IN_BYTES * MS_IN_SEC))/ (uint64_t)(duration * BITS_PER_Kb)\
     );
 calc_bitrate_ret:
#if 0
  if(ret == -1){
    //on failure, assign the user input bitrate
    bitrate_est = inp_bitrate;
  }
#endif
  if(pid_map != NULL){
    free(pid_map);
    pid_map = NULL;
  }
  if(init_data != NULL){
    free(init_data);
    init_data = NULL;
  }
  if(final_data != NULL){
    free(final_data);
    final_data = NULL;
  }
  //reset the fd to start of file
  iocb->ioh_seek(io_desc_ts, 0, SEEK_SET);
  return (bitrate_est);

}

static void register_value_db(
			      uint32_t pkt_time,
			      uint32_t pid,
			      pid_map_t *pid_map,
			      uint32_t array_depth){

  uint32_t i;
  for(i = 0; i < array_depth; i++){
    if((pid_map->entry_valid == 0) ||
       (pid_map->entry_valid && pid_map->pid == pid)){
      pid_map->pid = pid;
      if(pid_map->entry_valid == 0)
	pid_map->min_pkt_time = pkt_time;
      if(pkt_time < pid_map->min_pkt_time)
	pid_map->min_pkt_time = pkt_time;
      if(pkt_time > pid_map->max_pkt_time)
	pid_map->max_pkt_time = pkt_time;
      if(pid_map->max_pkt_time){
	pid_map->max_duration = pid_map->max_pkt_time -
	  pid_map->min_pkt_time;
      }
      pid_map->entry_valid = 1;
      break;
    }
    pid_map++;
  }
  return;
}

