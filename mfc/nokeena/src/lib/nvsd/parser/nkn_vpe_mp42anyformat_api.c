
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "nkn_vpe_mp42anyformat_api.h"
#include "nkn_vpe_parser_err.h"
//#define PERF_TIMERS

#define ERR_FRUIT_FMTR (0x1)
#define ERR_SL_FMTR (0x2)

extern gbl_fruit_ctx_t fruit_ctx;
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


/* This function is used to allocate and initailize the context
   required for the conversion of MP4 to any format 
   fconv_params -> input parameter
   conv_ctx->output parameter */
int32_t
init_mp42anyformat_converter(file_conv_ip_params_t *fconv_params,
			     void **conv_ctx)
{
    mp42anyformat_converter_ctx_t *mp42anyformat_ctx = NULL;
    mp4_parser_ctx_t *mp4_ctx = NULL, *mp4_ctx_temp = NULL;
    mp4_mfu_builder_t *bldr_mp4_mfu = NULL;
    mp4src_mfu_ts_builder_t *bldr_mfu_ts = NULL;
    int32_t err = 0, n_traks = 0;
    io_handlers_t *iocb = NULL;
    int32_t i = 0, k=0;
    *conv_ctx = mp42anyformat_ctx = (mp42anyformat_converter_ctx_t *)   \
        nkn_calloc_type(1, sizeof(mp42anyformat_converter_ctx_t),
                        mod_mp42anyforamt_converter_ctx_t);

    if(!mp42anyformat_ctx) {
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
    //initialise and update the "n" ctx from the "n" mp4 file
    for(i=0; i<fconv_params->n_input_files; i++) {
    //choosing the which handler to use further
    if (strstr(fconv_params->input_path[i], "http://")) {
      iocb = &ioh_http;
    } else {
      iocb = &iohldr;
    }
	err = mp4_create_mp4_ctx_from_file(
					   fconv_params->input_path[i],
					   &mp4_ctx_temp,
					   iocb);
	if (err) {
	    goto error;
	}
	mp42anyformat_ctx->mp4_ctx[i] = mp4_ctx_temp;
    }

    //initialise the mp4_mfu builder structure
    err = mp4_mfu_builder_init(fconv_params->key_frame_interval,
                               fconv_params->input_path,
                               fconv_params->streaming_type,
                               fconv_params->num_pmf,
                               fconv_params->n_input_files,
                               mp42anyformat_ctx->mp4_ctx,
			     &bldr_mp4_mfu);
    if (err) {
      goto error;
    }

    //update the mp4_mfu builder structure
    err = mp4_mfu_bldr_update(bldr_mp4_mfu,
			      mp42anyformat_ctx->mp4_ctx,
			      bldr_mp4_mfu->n_profiles);
    if (err) {

        goto error;
    }
    /*based on the status , initialiseing the either ts builder or
    moof builder */
    if(fconv_params->ms_param_status) {
	err = init_mp4src_mfu2ts_builder(fconv_params->key_frame_interval,
					 &bldr_mfu_ts,
					 bldr_mp4_mfu->sync_time*1000, 
					 fconv_params->n_input_files);
	if (err) {
	    goto error;
	}
	mp42anyformat_ctx->bldr_mfu_ts = bldr_mfu_ts;
    } else {
	mp42anyformat_ctx->bldr_mfu_ts = NULL;
    }

    if(fconv_params->ss_param_status) {

	mp42anyformat_ctx->slfmtr =
	    createSLFormatter(fconv_params->sess_name, 
			      fconv_params->output_path, 0, 0, 0, NULL, NULL);
	if (mp42anyformat_ctx->slfmtr == NULL) {
	    err = -E_MFP_SL_INIT;
	    goto error;
	}
    } else {
	mp42anyformat_ctx->slfmtr = NULL;
    }

    mp42anyformat_ctx->ms_parm_status =
	fconv_params->ms_param_status;
    mp42anyformat_ctx->ss_parm_status =
	fconv_params->ss_param_status;
    mp42anyformat_ctx->bldr_mp4_mfu = bldr_mp4_mfu;
    return err;
 error:
    cleanup_mp4_mfu_builder(bldr_mp4_mfu);
    cleanup_mp4src_mfu_ts_builder(bldr_mfu_ts);
    for(k=0; k<fconv_params->n_input_files; k++) {
      cleanup_mp4_ctx(mp42anyformat_ctx->mp4_ctx[k]);
    }
    if(mp42anyformat_ctx)
	free(mp42anyformat_ctx);
    //cleanup_mfu2ts_bldr();
    return err;
}

extern file_id_t* file_state_cont;
static int32_t check_sess_state(int32_t sess_id){
    mfp_publ_t *pub = file_state_cont->get_ctx(file_state_cont,
	sess_id);
    if(pub->op == PUB_SESS_STATE_STOP){
        DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, 
	    "Received STOP command");    	
    	return -1;
    }
    return 0;
}


/* This fucntion is used to convert MP4 to any foramt(ts/MOOF).
first chunk of all available video traks is converted, then second
chunk of all video traks,.... till availble number of chunks. */ 
int32_t
mp42anyformat_conversion(void *private_data, void **out)
{

    mp42anyformat_converter_ctx_t *conv_ctx = NULL;
    mp4_mfu_builder_t *bldr_mp4_mfu = NULL;
    mp4src_mfu_ts_builder_t *bldr_mfu_ts = NULL;
    sl_fmtr_t *slfmtr = NULL;
    uint32_t num_mfu = 0;
    mfu_data_t *mfu_data;
    int32_t err = 0, i=0, err1 = 0, sl_err = 0, k = 0, ts_err = 0;
    int32_t curr_vid_trak = 0, max_vid_traks = 0;
    uint32_t curr_sync_pt = 0;
    uint8_t *aud_chunk_data = NULL, *vid_chunk_data = NULL;
    audio_trak_t *aud = NULL;
    video_trak_t *vid = NULL;
    uint32_t curr_aud_trak = 0;
    uint32_t m=0, n=0, max_sync_points = 0;
    void *io_desc = NULL;
    uint32_t fmtr_err_flag = 0;
    io_handlers_t *iocb = NULL;
    mfp_publ_t *pub_ctx = NULL;

    err = 0;
    conv_ctx = (mp42anyformat_converter_ctx_t*)(private_data);
    pub_ctx = file_state_cont->get_ctx(file_state_cont, conv_ctx->sess_id);

    bldr_mp4_mfu = conv_ctx->bldr_mp4_mfu;
    bldr_mfu_ts = conv_ctx->bldr_mfu_ts;
    slfmtr = conv_ctx->slfmtr;


    if(bldr_mp4_mfu->n_profiles == 1) {
	max_vid_traks = bldr_mp4_mfu->n_vid_traks[0];
	max_sync_points = bldr_mp4_mfu->num_sync_points;
    } else {
	max_vid_traks = bldr_mp4_mfu->n_profiles;
	max_sync_points = bldr_mp4_mfu->num_sync_points;
    }
    conv_ctx->mfu_data = nkn_calloc_type(max_vid_traks, 
					 sizeof(mfu_data_t**),
					 mod_vpe_mfu_data_t);

    conv_ctx->ref_cont = nkn_calloc_type(max_vid_traks,
                                         sizeof(ref_count_mem_t**),
					 mod_vpe_mfu_data_t);

    for(i=0; i<max_vid_traks; i++) {

	conv_ctx->mfu_data[i] = nkn_calloc_type(max_sync_points,
						sizeof(mfu_data_t*),
						mod_vpe_mfu_data_t);

	conv_ctx->ref_cont[i] = nkn_calloc_type(max_sync_points,
						sizeof(ref_count_mem_t*),
						mod_vpe_mfu_data_t);
    }
    conv_ctx->max_vid_traks = max_vid_traks;
    conv_ctx->max_sync_points = max_sync_points;

    struct timeval now1, now2;
    double total_time = 0;
    double conv_time = 0;
    uint32_t chunk_num = 0;

    //for number of sync points
    while(curr_sync_pt < max_sync_points) {
	//for number of video traks
	curr_vid_trak = 0;
	m=0;
	n=0;
	while(curr_vid_trak < max_vid_traks) {
	    if (fmtr_err_flag) {
		/* if we hit an error in the previous formatter call
		 * then we dont process the other profiles
		 */
		curr_vid_trak++;
		continue;
	    }
	    if(check_sess_state(conv_ctx->sess_id)== -1){
		curr_sync_pt = max_sync_points;
	    	break;
	    }
	    bldr_mp4_mfu->profile_num = curr_vid_trak;
	    curr_aud_trak = bldr_mp4_mfu->min_aud_trak_num[m];
	    aud = &bldr_mp4_mfu->audio[m][curr_aud_trak];
	    vid = &bldr_mp4_mfu->video[m][n];
	    io_desc = bldr_mp4_mfu->io_desc_mp4[m];
	    iocb = bldr_mp4_mfu->iocb[m];

	    //mp4 to single MFU conversion
	    mfu_data = (mfu_data_t*)nkn_calloc_type(1,
						    sizeof(mfu_data_t),
						    mod_vpe_mfu_data_t);
#if defined(PERF_TIMERS)   
	    printf("-----------------------------------------------\n");
	    gettimeofday(&now1, NULL);
#endif
      err = mp4_mfu_conversion(aud, vid, mfu_data, bldr_mp4_mfu,
			       io_desc, iocb);
#if defined(PERF_TIMERS)   
	    gettimeofday(&now2, NULL);
	    chunk_num++;
	    conv_time = diffTimevalToMs(&now2, &now1);
	    total_time += conv_time;

	    printf("chunk %u: ins-delay %f tot-delay %f \n", 
	    	chunk_num, conv_time, total_time);
#endif

	    if (err) {
		if(mfu_data) {
		    destroyMfuData(mfu_data);
		}
		goto send_eos;
	    }

	    num_mfu++;
	    bldr_mp4_mfu->video[m][n].num_chunks_processed++;
	    bldr_mp4_mfu->audio[m][curr_aud_trak].num_chunks_processed++;

	    /*conv_ctx->mfu_data is two dimensional. for each MFU,
	    corrsponding refcount is created to avoid the deltion of
	    the MFU memory when it is used by other conversions */
	    conv_ctx->mfu_data[curr_vid_trak][curr_sync_pt] =
		mfu_data;

	    ref_count_mem_t* ref_cont = createRefCountedContainer(mfu_data, destroyMfuData);
	    conv_ctx->ref_cont[curr_vid_trak][curr_sync_pt] = ref_cont;

	    if (conv_ctx->ms_parm_status) {
		ref_cont->hold_ref_cont(ref_cont);
  	        apple_fmtr_hold_encap_ctxt(get_sessid_from_mfu(mfu_data));
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
	        bldr_mfu_ts->mfu2ts_conv_req_num_mfu = 1;
		if(num_mfu == bldr_mfu_ts->mfu2ts_conv_req_num_mfu) {
		    err = mfu2ts_conversion(conv_ctx->bldr_mfu_ts,
					    conv_ctx->ref_cont[curr_vid_trak],
					    bldr_mfu_ts->mfu2ts_conv_req_num_mfu, 
					    curr_vid_trak,
					    max_vid_traks,
					    curr_sync_pt, bldr_mfu_ts->num_active_mfu2ts_task);
		    //err = 1;//included for testing the error path
		    if (ts_err < 0) {
			fmtr_err_flag |= ERR_FRUIT_FMTR;
			err = ts_err;
		    }
		    bldr_mfu_ts->mfu2ts_conv_processed[curr_vid_trak]++;
		    num_mfu = 0;
		    
		}
		
	    }
	    if(conv_ctx->ss_parm_status) {
		sl_err = slfmtr->handle_mfu(ref_cont);
		if (sl_err < 0) {
		  err = sl_err;
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
		for(k = 0; k < max_vid_traks; k++){
		  register_stream_dead(conv_ctx->sess_id, k);
		}
		DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,
			   "called register_stream_dead\n");
		goto send_eos;
	    }
	    if(bldr_mp4_mfu->n_profiles != 1)
		m++;
	    else
		n++;
	    curr_vid_trak++;
	    }//while loop for number of video traks
	    curr_sync_pt++;
	    if (fmtr_err_flag) {
		/* if we hit a formatter error send eos */
		goto send_eos;
	    }
	}//while loop for number of sync points
    //curr_sync_pt++;

 send_eos:
    curr_vid_trak = 0;
    while(curr_vid_trak < max_vid_traks) {
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
                            mod_vpe_mfp_ts2mfu_t);
	if (!mfu_data->data) {
	    return -E_VPE_PARSER_NO_MEM;
	}
	mfu_hdr->offset_vdat = 0xffffffffffffffff;
	mfu_hdr->offset_adat = 0xffffffffffffffff;
	mfu_hdr->stream_id = curr_vid_trak;
	mfu_hdr->mfu_size =  mfu_data->data_size;
	mfu_hdr->program_number = bldr_mp4_mfu->num_pmf;

	/* write the mfu header */
	mfu_write_mfub_box(sizeof(mfub_t),
			   mfu_hdr,
			   mfu_data->data);
	ref_count_mem_t* ref_cont =
	    createRefCountedContainer(mfu_data, destroyMfuData);
	if((conv_ctx->ms_parm_status)) {
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
            fmt_input->mfu_seq_num = curr_sync_pt;
            fmt_input->streams_per_encap = max_vid_traks;
#if 1
	    fmt_input->active_task_cntr = bldr_mfu_ts->num_active_mfu2ts_task[curr_vid_trak];
            (*fmt_input->active_task_cntr)++;
#endif
	    mfu_data->fruit_fmtr_ctx = (void*)fmt_input;

            ts_err = mfubox_ts(ref_cont);
	    if(ts_err == VPE_ERROR) {
	      if(mfu_hdr != NULL)
                free(mfu_hdr);
	      ts_err = -E_VPE_TS_CREATION_FAILED;
	    } else {
	      ts_err = 0;
	    }
        }
	if ((conv_ctx->ss_parm_status)) {
	  ((mfu_data_t*)(ref_cont->mem))->sl_fmtr_ctx = slfmtr;
	  sl_err = slfmtr->handle_mfu(ref_cont);
	  if (sl_err < 0) {
	    err = -E_VPE_MFU2SL_ERR;
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
        if(mfu_hdr != NULL)
            free(mfu_hdr);
	curr_vid_trak++;
    }
    if(err < 0)
      return err;
    /* in some cases,bldr_mfu_ts->num_active_mfu2ts_task is being
       cleared by cleanup function, before it is being used in mfu2iphone.c */
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
mfu2ts_conversion(mp4src_mfu_ts_builder_t *bldr_mfu_ts,
		  ref_count_mem_t **ref_cont, uint32_t req_num_mfu,
		  uint32_t curr_vid_trak, int32_t max_vid_traks,
		  uint32_t curr_sync_pt, int32_t **num_active_mfu2ts_task) 
{
    int32_t err = 0;
    uint32_t i = 0;
    ref_count_mem_t *ref_count_temp = NULL;
    uint32_t processed = bldr_mfu_ts->mfu2ts_conv_processed[curr_vid_trak];
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
	fmt_input->mfu_seq_num = curr_sync_pt;
	fmt_input->streams_per_encap = max_vid_traks;
	fmt_input->active_task_cntr = bldr_mfu_ts->num_active_mfu2ts_task[curr_vid_trak];
	(*fmt_input->active_task_cntr)++;

	ref_count_temp  = ref_cont[req_num_mfu*processed] ;
	mfu_data = (mfu_data_t*)ref_count_temp->mem;
	mfu_data->fruit_fmtr_ctx = (void*)fmt_input;
	err = mfubox_ts(ref_count_temp);
	if(err == VPE_ERROR) {
	    //if(fmt_input)
	    //free(fmt_input);
	    //ref_count_temp->release_ref_cont(ref_count_temp);
	    err = -E_VPE_TS_CREATION_FAILED;
	    return err;
	} else {
	    err = 0;
	}
    } else {
	/*this part has tobe written further. This is the case where
	morehtan one MFU cotributes single Ts fragments*/
	for(i=0; i<req_num_mfu; i++) {
	    
	    
	}
    }
    return err;
}

/* This function is used to update the context for the input MP4
   file. 
   mp4_name -> i/p parameter
   out-> o/p parameter which is context
   iocb->i/p parameter */

int32_t
mp4_create_mp4_ctx_from_file (char *mp4_name, mp4_parser_ctx_t
			      **out, io_handlers_t *iocb)
{
    struct stat st;
    void *io_desc = NULL;
    size_t mp4_size = 0, moov_offset =0, moov_size = 0;
    uint8_t moov_search_data[MOOV_SEARCH_SIZE];
    moov_t *moov= NULL;
    mp4_parser_ctx_t *mp4_ctx = NULL;
    int32_t n_traks = 0, err = 0;
    uint8_t *moov_data = NULL;    
    uint8_t *ftyp_data = NULL;
    char *tmp = NULL, *tmp2 = NULL;
    int32_t bytes_read = 0;
    
    if(!stat(mp4_name, &st)) {
	if (S_ISDIR(st.st_mode)) {
	    err = -E_VPE_PARSER_INVALID_FILE;
	    goto error;
	}
    }

    tmp = strrchr(mp4_name, '/')+1;
    /* checking whther the correct file format is given as input
       argument*/
    tmp2 = strrchr(tmp, '.');
    if(tmp2 != NULL ) {
      if (strncmp(tmp2+1, "mp4", 3) != 0) {
	if (strncmp(tmp2+1, "f4v", 3) != 0) {
	  return -E_VPE_INVALID_FILE_FROMAT;
        }
      }
    } else {
        return -E_VPE_INVALID_FILE_FROMAT;
    }
    
    io_desc = iocb->ioh_open((char*) \
			     mp4_name,
			     "rb", 0);
    if (!io_desc) {
	err = -E_VPE_PARSER_INVALID_FILE;
	return err;
    }
    
    iocb->ioh_seek(io_desc, 0, SEEK_END);
    
    mp4_size = iocb->ioh_tell(io_desc);
    if (!mp4_size) {
	err = -E_VPE_PARSER_INVALID_FILE;
	goto error;
    }
    //printf("mp4_size is %ld",mp4_size);
    /*check whether the file is an ismv file or not*/
    /*assuming that first box is ftype*/
#if 0
    iocb->ioh_seek(io_desc, 8, SEEK_SET);
    ftyp_data = (uint8_t*)calloc(1, 4);
    iocb->ioh_read(ftyp_data, 4, sizeof(uint8_t), io_desc);
    if(strncmp((char*)ftyp_data, "mp42", 4) != 0) {
        err = -E_MP4_PARSE_ERR;
        goto error;
    }
#endif

    iocb->ioh_seek(io_desc, 0, SEEK_SET);
    //iocb->ioh_read(moov_search_data, MOOV_SEARCH_SIZE,
    //	   sizeof(uint8_t), io_desc);
    
    /* read information about the moov box */
    err = mp4_get_moov_offset(io_desc, mp4_size, &moov_offset, &moov_size, iocb);
    if(err < 0) {
      goto error;
    }
    err = mp4_get_moov_info(moov_search_data, MOOV_SEARCH_SIZE,
			    &moov_offset, &moov_size);
    if(err < 0) {
      goto error;
    }
    
    if (!moov_offset) {
	err = -E_VPE_MP4_MOOV_ERR;
	goto error;
    }
    
    /* read the moov data into memory */
    iocb->ioh_seek(io_desc, moov_offset, SEEK_SET);
    moov_data = (uint8_t*)nkn_calloc_type(1, moov_size,
					  mod_vpe_moov_buf);
    if(!moov_data) {
	err = -E_VPE_PARSER_NO_MEM;
        goto error;
    }

    bytes_read = iocb->ioh_read(moov_data, sizeof(uint8_t), moov_size, io_desc);
    if(bytes_read != (int32_t)moov_size) {
      err = -E_VPE_READ_INCOMPLETE;
      goto error;
    }
    /* initialize the moov box */
    moov = mp4_init_moov((uint8_t*)moov_data, moov_offset,
			 moov_size);
    
    /* initialize the trak structure for the mp4 data */
    n_traks = mp4_populate_trak(moov);
    if (!n_traks) {
	err = -E_VPE_MP4_MOOV_ERR;
	goto error;
    }
    
    mp4_ctx = (mp4_parser_ctx_t*)nkn_calloc_type(1,
						 sizeof(mp4_parser_ctx_t),
						 mod_vpe_mp4_parser_ctx_t);
    if(!mp4_ctx) {
	err = -E_VPE_PARSER_NO_MEM;
        goto error;
    }
    mp4_ctx->moov = moov;
    mp4_ctx->n_traks = n_traks;
    *out = mp4_ctx;
    goto exit;
 error:
    //if (mp4_ctx) mp4_cleanup_ismv_ctx(mp4_ctx);
    if (moov) mp4_moov_cleanup(moov, moov->n_tracks);
    if (moov_data) free(moov_data);
 exit:
    if (io_desc)
	iocb->ioh_close(io_desc);

    return err;
    
}


int32_t
mp4_get_moov_offset(void *io_desc, size_t size, size_t *moov_offset,
		    size_t *moov_size, io_handlers_t *iocb)
{
    size_t curr_pos, box_size, moov_pos;
    box_t box;
    uint8_t *moov_data, box_data[12];
    const char moov_id[] = {'m', 'o', 'o', 'v'};
    uint64_t pos = 0;
    int32_t err= 0, bytes_read = 0;
    do {
        /* read the first 8 bytes for box size and type */
	bytes_read = iocb->ioh_read(box_data, 1, 8, io_desc);
	if(bytes_read != 8) {
	  err = -E_VPE_READ_INCOMPLETE;
	  return err;
	}

        /* parse box type and size*/
        read_next_root_level_box(box_data, 8, &box, &box_size);
        /* check if box size is MOOV */
        if(check_box_type(&box, moov_id)) {
            moov_pos = pos;
            *moov_offset = moov_pos;//  -= 8;//move to the head of the box
            *moov_size = box.short_size;
	    pos += box.short_size;
	    break;
        }
	/* got to next root level box */
        if(is_container_box(&box)) {
            //iocb->ioh_seek(io_desc, 0, SEEK_CUR);
	    iocb->ioh_seek(io_desc, box.short_size - 8, SEEK_CUR);
	    pos += box.short_size;
        } else {
            iocb->ioh_seek(io_desc, box.short_size - 8, SEEK_CUR);
	    pos += box.short_size;
        }

    } while( box.short_size < size);

    return 0;


}

/* cleanup fucntion for the elements used for MP4 to any format
   structure*/
void
mp42anyformat_cleanup_converter(void *private_data)
{
    mp42anyformat_converter_ctx_t *conv_ctx;
    int32_t i = 0;

    conv_ctx = (mp42anyformat_converter_ctx_t*)private_data;

    if (conv_ctx) {
	for(i=0; i<conv_ctx->bldr_mp4_mfu->n_profiles; i++) {
	    cleanup_mp4_ctx(conv_ctx->mp4_ctx[i]);
	}

	cleanup_mp4_mfu_builder(conv_ctx->bldr_mp4_mfu);

	if(conv_ctx->ms_parm_status) {
	    if (conv_ctx->bldr_mfu_ts) 
		cleanup_mp4src_mfu_ts_builder(conv_ctx->bldr_mfu_ts);
	}

	if(conv_ctx->ss_parm_status) {
	    if(conv_ctx->slfmtr)
		conv_ctx->slfmtr->delete_ctx(conv_ctx->slfmtr);
	}

	for(i=0; i<(int32_t)conv_ctx->max_vid_traks; i++) {
	    free(conv_ctx->mfu_data[i]);
	    free(conv_ctx->ref_cont[i]);
	}
	free(conv_ctx->mfu_data);
	free(conv_ctx->ref_cont);
	free(conv_ctx);
    }
}

/* This function is used to allocate and initialisse the builder
   structure  used for mfu to Ts conversion
   chunk_time -> i/p key frame interval specified in the PMF
   out -> o/p builder structure which is updated
   actual_frame_interval -> actual I frame interval
*/
int32_t 
init_mp4src_mfu2ts_builder(int32_t chunk_time,
			   mp4src_mfu_ts_builder_t **out, uint32_t
			   actual_frame_interval, int32_t n_profiles)
{
    mp4src_mfu_ts_builder_t *bldr = NULL;
    int32_t err = 0;
    int32_t i = 0;
    *out = bldr = (mp4src_mfu_ts_builder_t*)
        nkn_calloc_type(1, sizeof(mp4_mfu_builder_t),
                        mod_mp4_mfu_builder_t);
    if (!bldr) {
        return -E_VPE_PARSER_NO_MEM;
    }
    bldr->num_active_mfu2ts_task = nkn_calloc_type(MAX_TRACKS, 
						   sizeof(int32_t*),
						   mod_vpe_temp_t);
    if(bldr->num_active_mfu2ts_task == NULL)
      return -E_VPE_PARSER_NO_MEM;
    bldr->n_profiles = n_profiles;
    for(i=0; i< n_profiles; i++) {
      bldr->num_active_mfu2ts_task[i] = nkn_calloc_type(1,
							sizeof(int32_t), 
							mod_vpe_temp_t);
      if(bldr->num_active_mfu2ts_task[i] == NULL) 
	return -E_VPE_PARSER_NO_MEM;
    }
    //bldr->mfu2ts_conv_processed = 0;
    if(actual_frame_interval > (uint32_t)chunk_time) {
	bldr->mfu2ts_conv_req_num_mfu = 1;
    } else {
	bldr->mfu2ts_conv_req_num_mfu =
	    chunk_time / actual_frame_interval;
    }


    return err;
}

void
cleanup_mp4_ctx(mp4_parser_ctx_t* ctx)
{
    if(ctx) {
	if (ctx->moov) {
	    if(ctx->moov->data) {
		free(ctx->moov->data);
		ctx->moov->data = NULL;
	    }
	    mp4_moov_cleanup(ctx->moov, ctx->n_traks);
	}
	free(ctx);
    }


}

void
cleanup_mp4src_mfu_ts_builder(mp4src_mfu_ts_builder_t* bldr)
{
    if(bldr) {
	if(bldr->num_active_mfu2ts_task)
	    free(bldr->num_active_mfu2ts_task);
	free(bldr);
    }


}

void
cleanup_mp4src_mfu_moof_builder(mp4src_mfu_moof_builder_t* bldr)
{
    if(bldr) {
        free(bldr);
    }


}


