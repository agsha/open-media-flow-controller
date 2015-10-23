#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mfp_file_convert_api.h"
#include "mfp_publ_context.h"
#include "thread_pool/mfp_thread_pool.h"
#include "mfp_file_converter_intf.h"
#include "mfp_mgmt_sess_id_map.h"

#include "nkn_vpe_ism_read_api.h"
#include "nkn_vpe_sync_http_read.h"
#include "nkn_vpe_mp42anyformat_api.h"
#include "nkn_vpe_ts2anyformat_api.h"
#include "nkn_vpe_ismv_mfu.h"
#include "nkn_vpe_ismv2ts_api.h"
#include "nkn_vpe_ismv2moof_api.h"
#include "nkn_vpe_ts2iphone_api.h"
#include "nkn_vpe_moof2mfu_api.h"
#include "nkn_vpe_mfu2ts_api.h"
#include "nkn_memalloc.h"
#include "nkn_debug.h"
#include "nkn_stat.h"

/***********************************************************
 *                EXTERN DECLARATIONS
************************************************************/
extern file_id_t *file_state_cont;
extern gbl_fruit_ctx_t fruit_ctx;
extern uint32_t glob_mfp_fconv_tpool_enable;
extern mfp_thread_pool_t *read_thread_pool, *write_thread_pool;
extern uint32_t glob_mfp_fconv_sync_mode;
extern int32_t fruit_formatter_init(mfp_publ_t *, uint32_t,
				    register_formatter_error_t, 
				    int32_t);
int32_t num_pmfs_cntr = 0;
AO_t glob_mfp_live_num_pmfs_cntr = 0;

NKNCNT_EXTERN(mfp_file_tot_err_sessions, AO_t)
NKNCNT_EXTERN(mfp_file_num_active_sessions, AO_t)
NKNCNT_EXTERN(mfp_file_num_ctrl_sessions, AO_t)
NKNCNT_DEF(mfp_file_tot_well_finished, AO_t,\
	   "", "num well finished file convert sessions")

/* note: the string array should be aligned with the enum indices of
         mfp_op_state
*/

/* prepare inputs */
static int32_t
moof2mfu_prepare_input(void *, void *);
static int32_t
mfu2ts_prepare_input(void *, void *);

static char*
remove_trailing_slash(char *str);

static int32_t
send_eos(int32_t n_aud_traks, int32_t n_vid_traks, int32_t seq_num,
         int32_t num_pmf, mfu2ts_builder_t *bldr);

file_conv_intf_t ismv2moof_conv = {
    .prepare_input = NULL,//ismv2moof_prepare_input,
    .init_file_conv = init_ismv2moof_converter,
    .read_and_process = ismv2moof_process_frags,
    .write_out_data = ismv2moof_write_output,
    .cleanup_file_conv = ismv2moof_cleanup_converter
};

file_conv_intf_t moof2mfu_conv = {
    .prepare_input = moof2mfu_prepare_input,
    .init_file_conv = init_moof2mfu_converter,
    .read_and_process = moof2mfu_process_frags,
    .write_out_data = moof2mfu_write_output,//NULL,
    .cleanup_file_conv = moof2mfu_cleanup_converter
};

file_conv_intf_t mfu2ts_conv = {
    .prepare_input = mfu2ts_prepare_input,
    .init_file_conv = init_mfu2ts_converter,
    .read_and_process = mfu2ts_process_frags,
    .write_out_data = mfu2ts_write_output,//NULL
    .cleanup_file_conv = mfu2ts_cleanup_converter
};

file_conv_intf_t ts2iphone_conv = {
    .prepare_input = NULL,
    .init_file_conv = init_ts2iphone_converter,
    .read_and_process = ts2iphone_create_seg,
    .write_out_data = ts2iphone_write_output,
    .cleanup_file_conv = ts2iphone_cleanup_converter
};

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

#if 0
//file_conv_intf_t (*ismv2iphone_convert[])() = {
file_conv_intf_t** ismv2iphone_convert[2] = {
    &ismv2moof_conv,
    &moof2mfu_conv,
    &mfu2ts_conv,
};
#endif

#if 0
file_conv_intf_t** ismv2iphone_convert = NULL;

static int32_t init_test(void); 

static int32_t init_test(void) {


    ismv2iphone_convert = nkn_calloc_type(3,
					  sizeof(file_conv_intf_t*),
					  mod_vpe_ismv2iphone_convert);
    if (!ismv2iphone_convert) {
	return -E_MFP_NO_MEM;
    }
    ismv2iphone_convert[0] = &ismv2moof_conv;
    ismv2iphone_convert[1] = &moof2mfu_conv;
    ismv2iphone_convert[2] = &mfu2ts_conv;
    return 0;
    //arr[0]->prepare_input(param);
}
#endif
#define CONV_ARRAY_SIZE 3

/******************************************************************
 *              HELPER FUNCTION IMPLEMENTATION
 *****************************************************************/
static int32_t mfu_process_ism(xml_read_ctx_t*ism,
			       char* ism_file,
			       ism_bitrate_map_t **out);
static int32_t mfp_init_input_params_ismv(int32_t sess_id,
					  int32_t ismv2iphone_flag,
					  file_conv_ip_params_t **out);  
static int32_t mfp_init_input_params_ts(int32_t sess_id,
					file_conv_ip_params_t **out);
static void mfp_cleanup_input_params_ismv(\
			    file_conv_ip_params_t *fconv);  
static void mfp_tpool_cleanup_read_task(void *arg);
static void mfp_tpool_hdl_read_task(void *arg);
static int32_t mfp_tpool_add_read_task(file_conv_intf_t *intf,
				       void *input1, void *input2,
				       void *output);
static void mfp_tpool_hdl_write_task(void *arg);
static int32_t mfp_tpool_add_write_task(file_conv_intf_t *intf,
					void *input1, void *input2, 
					void *output);
static int32_t mfp_tpool_add_convert_task(taskHandler converter, 
					  ref_count_mem_t *ref_cont);
static void mfp_convert_ismv2iphone(void *sess_id); 
static void mfp_convert_ismv2moof(void *sess_id);
static void mfp_convert_ts2iphone(void *sess_id); 
static void mfp_cleanup_ref_cont(void *private_data); 
static int32_t mfp_convert_pmf_add_ref(mfp_publ_t *pub,
				       ref_count_mem_t *ref_cont); 

/*****************************************************************
 * API's implemented in this file are 
 * 1. mfp_convert_pmf - converts an input publisher context to the
 *    respective publish schemes
 ****************************************************************/

/** 
 * converts a publisher context to the respective publish schemes
 * @param pub - the publisher context
 * @return returns 0 on success and negative integer on error
 */
int32_t 
mfp_convert_pmf(int32_t sess_id)
{
    mfp_publ_t *pub = file_state_cont->get_ctx(file_state_cont, sess_id); 
    int32_t err, *ref_sess_id;
    ref_count_mem_t *ref_cont;
    formatter_status_e temp_ss_param_status = FMTR_OFF;       
    err = 0;

    if (!pub) {/* rogue session id? */
	err = -E_MFP_INVALID_SESS;
	goto done;
    }
    temp_ss_param_status = pub->ss_parm.status;

    /*if none of the pub schemes is on, exit */
    if ((pub->ms_parm.status == FMTR_OFF) &&
	(pub->ss_parm.status == FMTR_OFF)&&
	(pub->fs_parm.status == FMTR_OFF)) {
	    err = -E_MFP_INVALID_CONVERSION_REQ;
	    DBG_MFPLOG (pub->name, ERROR, MOD_MFPFILE, "invalid"
			" conversion request %d", err);
	    mfp_mgmt_sess_unlink((char*)pub->name, PUB_SESS_STATE_REMOVE, err);
	    file_state_cont->remove(file_state_cont, sess_id);	
    }

    /* create a ref counted container to hold the context for a one to
     * many conversion task. this is to ensure that the context
     * cleanup is synchronized even if the convert tasks are async
     */
    ref_sess_id = (int32_t *)nkn_calloc_type(1, sizeof(int32_t),
					     mod_mfp_convert_pmf);
    if (!ref_sess_id) {
	err = -E_MFP_NO_MEM;
	goto done;
    }
    *ref_sess_id = sess_id;
    ref_cont = createRefCountedContainer(ref_sess_id, mfp_cleanup_ref_cont);
    mfp_convert_pmf_add_ref(pub, ref_cont);   

    if ((pub->ms_parm.status == FMTR_ON) || (pub->ss_parm.status == FMTR_ON)){
	switch(pub->encaps[0]->encaps_type) {
	    case ISMV_SBR:
		if (pub->ms_parm.status == FMTR_ON) {
		    if (glob_mfp_fconv_sync_mode) {
			mfp_convert_ismv2iphone(ref_cont);
		    } else {
			mfp_tpool_add_convert_task(mfp_convert_ismv2iphone,
						   ref_cont);
		    }
		} 
		if(temp_ss_param_status == FMTR_ON){
		  if (glob_mfp_fconv_sync_mode) {
		    mfp_convert_ismv2moof(ref_cont);
		  } else {
		    mfp_tpool_add_convert_task(mfp_convert_ismv2moof,
					       ref_cont);
		  }
		}
		break;
	case TS:
		if (glob_mfp_fconv_sync_mode) {
		    mfp_convert_ts2anyformat(ref_cont);
		} else {
		    mfp_tpool_add_convert_task(mfp_convert_ts2anyformat,
					       ref_cont);
		}
		break;
	    case MP4:
		if(glob_mfp_fconv_sync_mode) {
		    mfp_convert_mp42anyformat(ref_cont);
		} else {
		    mfp_tpool_add_convert_task(mfp_convert_mp42anyformat,
					       ref_cont);
		}
		break;
	    default:
		err = -E_MFP_INVALID_CONVERSION_REQ;
		break;	
	}
    }
    
 done:
    return err;
}

/** 
 * function that converts inputs specified in a pub context to iphone
 * compatible TS segments
 * @param pub - the publisher context that describes the media asset
 * and its corresponding output formats
 * @return returns 0 on success and negative number on error
 */
static void
mfp_convert_ismv2iphone(void *arg)
{
    ref_count_mem_t *ref_cont = (ref_count_mem_t *)arg;
    int32_t *sess_id = (int32_t *)ref_cont->mem;
    mfp_publ_t *pub = file_state_cont->get_ctx(file_state_cont, *sess_id); 
    ismv2ts_builder_t *bldr = NULL;
    file_conv_ip_params_t *fconv_ip;
    int32_t err;
    void **out;
    void *input_bldr_array[3];
    int32_t i=0, j=0, k = 0;
    file_conv_intf_t** ismv2iphone_convert = NULL;

    err = 0;
    /* set the session state */
    file_state_cont->acquire_ctx(file_state_cont, *sess_id);
    pub->op = PUB_SESS_STATE_INIT;
    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "Session State: INIT");
    file_state_cont->release_ctx(file_state_cont, *sess_id);

    /* init the input params for conversion */
    err = mfp_init_input_params_ismv(*sess_id, 1, &fconv_ip);
    if (err) {
        goto error;
    }
    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "starting conversion \
    from"  " ISMV to IPHONE");

    fconv_ip->key_frame_interval = pub->ms_parm.key_frame_interval;

    /* Init the builders required for ismv2iphone conversion */
    ismv2iphone_convert = nkn_calloc_type(3,
                                          sizeof(file_conv_intf_t*),
                                          mod_vpe_ismv2iphone_convert);
    if (!ismv2iphone_convert) {
      err = -E_MFP_NO_MEM;
      goto error;
    }
    ismv2iphone_convert[0] = &ismv2moof_conv;
    ismv2iphone_convert[1] = &moof2mfu_conv;
    ismv2iphone_convert[2] = &mfu2ts_conv;
    //init_test();
    for (i=0;i<CONV_ARRAY_SIZE;i++) {
	err = ismv2iphone_convert[i]->init_file_conv(fconv_ip,
                                                (void**)(&input_bldr_array[i]));
	if (err)
	    goto error;
    }

    mfp_cleanup_input_params_ismv(fconv_ip);
    /* set the session state */
    file_state_cont->acquire_ctx(file_state_cont, *sess_id);
    pub->op = PUB_SESS_STATE_RUNNING;
    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "Session State: RUNNING");
    file_state_cont->release_ctx(file_state_cont, *sess_id);
    if (glob_mfp_fconv_tpool_enable) {
        mfp_tpool_add_read_task(ismv2iphone_convert[0], (void*)bldr,
                                &sess_id,
                                (void*)out);
        return;
    } else {
        while(1) {	    
	    for(i=0;i<CONV_ARRAY_SIZE;i++) {
		err = ismv2iphone_convert[i]->read_and_process
		    ((void*)input_bldr_array[i],
		     (void**)&out);

		if (pub->op == PUB_SESS_STATE_STOP) {
		  err = -E_VPE_FORMATTER_ERROR;
		    if(pub->ms_parm.status == FMTR_ON ){
			moof2mfu_converter_ctx_t *ptr =
			    (moof2mfu_converter_ctx_t *) input_bldr_array[1];
			for(k = 0; k < ptr->bldr->n_vid_traks; k++){
			    register_stream_dead(*sess_id, k);
			}
			DBG_MFPLOG("MFU2TS23", MSG, MOD_MFPLIVE,
				   "called register_stream_dead\n");
		    }
		}

		switch (err) {
		    case E_VPE_FILE_CONVERT_STOP:
			for(j=0; j<CONV_ARRAY_SIZE; j++) {
			    ismv2iphone_convert[j]->cleanup_file_conv
				((void*)input_bldr_array[j]);
			}
			DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE,
				    "completed  conversion from ISMV"
				    "to IPHONE Fragments- status code"
				    " %d, idx %d",
				    err, i);
			AO_fetch_and_add1(&glob_mfp_file_tot_well_finished);
			AO_fetch_and_sub1(&glob_mfp_file_num_active_sessions);
			goto error;
		    case E_VPE_FILE_CONVERT_CONTINUE:
			/*copying the output for the next call input*/
			if( i < (CONV_ARRAY_SIZE-1)) {
			    err =
				ismv2iphone_convert[i+1]->prepare_input
				((void*)out,
				 (void*)input_bldr_array[i+1]);
			    if (err) {
				AO_fetch_and_sub1(&glob_mfp_file_num_active_sessions);
				AO_fetch_and_add1(&glob_mfp_file_tot_err_sessions);
				goto error;
			    }
			}
			break;
		    case E_VPE_FILE_CONVERT_SKIP:
			break;
		    default:
		      /*incase of error, send eos*/
		      {
			moof2mfu_converter_ctx_t *ptr =
			  (moof2mfu_converter_ctx_t *) input_bldr_array[1];
			moof2mfu_builder_t *bldr_temp = ptr->bldr;
			mfu2ts_converter_ctx_t *ptr_mfu2ts = 
			  (mfu2ts_converter_ctx_t *) input_bldr_array[2];
			mfu2ts_builder_t *bldr_mfu2ts = ptr_mfu2ts->bldr;

			send_eos(bldr_temp->n_aud_traks, bldr_temp->n_vid_traks,  
				 bldr_temp->seq_num, bldr_temp->num_pmf, bldr_mfu2ts);
		      }
		      AO_fetch_and_sub1(&glob_mfp_file_num_active_sessions);
			AO_fetch_and_add1(&glob_mfp_file_tot_err_sessions);
			DBG_MFPLOG (pub->name, ERROR, MOD_MFPFILE,
				    "error occured during conversion"
				    "from ISMV to IPHONE Fragments -"
				    "error code %d", err);
			for(j=0; j<CONV_ARRAY_SIZE; j++) {
                            ismv2iphone_convert[j]->cleanup_file_conv
                                ((void*)input_bldr_array[j]);
                        }
			goto error;
		}//switch
	    }//for loop
	}//while loop
    }//else
    
 error:
    if(ismv2iphone_convert)
	free(ismv2iphone_convert);
    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "conversion status %d",
		err);
    mfp_mgmt_sess_unlink((char*)pub->name, PUB_SESS_STATE_IDLE, err);
    ref_cont->release_ref_cont(ref_cont);    
}

/**
 * converts the media asset described in the publisher context into
 * MOOF fragments
 * @param pub - the publisher context that describes the input media
 * asset and the output format
 * @return returns 0 on success and a negative number on error
 */
void
mfp_convert_ismv2moof(void *arg)
{
    ref_count_mem_t *ref_cont = (ref_count_mem_t *)arg;
    int32_t *sess_id = (int32_t *)ref_cont->mem;
    mfp_publ_t *pub = file_state_cont->get_ctx(file_state_cont, *sess_id); 
    int32_t err;
    ismv_moof_builder_t *bldr = NULL;
    file_conv_ip_params_t *fconv_ip;
    ismv_moof_proc_buf_t *out;
    
    err = 0;

    /* set the session state */
    file_state_cont->acquire_ctx(file_state_cont, *sess_id);
    pub->op = PUB_SESS_STATE_INIT;
    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "Session State: INIT");
    file_state_cont->release_ctx(file_state_cont, *sess_id);

    /* init the input params for conversion */
    err = mfp_init_input_params_ismv(*sess_id, 0, &fconv_ip);
    if (err) {
	goto error;
    }

    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "starting conversion from"
		" ISMV to MOOF");
    fconv_ip->key_frame_interval = pub->ss_parm.key_frame_interval;
    err = ismv2moof_conv.init_file_conv(fconv_ip, (void**)(&bldr));
    if (err) 
	goto error;
    mfp_cleanup_input_params_ismv(fconv_ip);

    /* set the session state */
    file_state_cont->acquire_ctx(file_state_cont, *sess_id);
    pub->op = PUB_SESS_STATE_RUNNING;
    file_state_cont->release_ctx(file_state_cont, *sess_id);

    if (glob_mfp_fconv_tpool_enable) {
	mfp_tpool_add_read_task(&ismv2moof_conv, (void*)bldr,
				&sess_id,
				(void*)out);
	return;	
    } else {
	while(1) {
	    err = ismv2moof_conv.read_and_process((void*)bldr,
						  (void**)&out); 
	    if (pub->op == PUB_SESS_STATE_STOP) {
		err = E_VPE_FILE_CONVERT_STOP;
		DBG_MFPLOG(pub->name, MSG, MOD_MFPFILE,
			   "received STOP signal via control PMF");
	    } 
	    switch (err) {
		case E_VPE_FILE_CONVERT_STOP:
		    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "completed"
			    " conversion from ISMV to MOOF Fragments -"
			    " status code %d", err);
		    AO_fetch_and_add1(&glob_mfp_file_tot_well_finished);
		    AO_fetch_and_sub1(&glob_mfp_file_num_active_sessions);
		    ismv2moof_conv.cleanup_file_conv(bldr);
		    goto error;
		case E_VPE_FILE_CONVERT_CONTINUE:
		    err = ismv2moof_conv.write_out_data((void*)out);
		    if (err) {
			AO_fetch_and_sub1(&glob_mfp_file_num_active_sessions);
			AO_fetch_and_add1(&glob_mfp_file_tot_err_sessions);
			ismv2moof_conv.cleanup_file_conv(bldr);
			goto error;
		    }
		    break;
		case E_VPE_FILE_CONVERT_SKIP:
		    break;
		default:
		    AO_fetch_and_sub1(&glob_mfp_file_num_active_sessions);
		    AO_fetch_and_add1(&glob_mfp_file_tot_err_sessions);
		    DBG_MFPLOG (pub->name, ERROR, MOD_MFPFILE, "error"
				" occured during conversion from ISMV" 
				" to MOOF Fragments - error code %d",
				err); 
		    ismv2moof_conv.cleanup_file_conv(bldr);
		    goto error;
	    }
	}
    }

 error:
    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "conversion status %d",
		err);
    mfp_mgmt_sess_unlink((char*)pub->name, PUB_SESS_STATE_IDLE, err);
    ref_cont->release_ref_cont(ref_cont);
}

/** 
 * function that converts inputs specified in a pub context to iphone
 * compatible TS segments
 * @param pub - the publisher context that describes the media asset
 * and its corresponding output formats
 * @return returns 0 on success and negative number on error
 */
static void
mfp_convert_ts2iphone(void *arg)
{
    ref_count_mem_t *ref_cont = (ref_count_mem_t *)arg;
    int32_t *sess_id = (int32_t *)ref_cont->mem;
    mfp_publ_t *pub = file_state_cont->get_ctx(file_state_cont,
					       *sess_id);
    int32_t err, err1;
    file_conv_ip_params_t *fconv_ip;
    ts2iphone_converter_ctx_t *conv_ctx;
    ts_segment_writer_t *out;

    err = err1 = 0;
    fconv_ip = NULL;

    /* set the session state */
    file_state_cont->acquire_ctx(file_state_cont, *sess_id);
    pub->op = PUB_SESS_STATE_INIT;
    file_state_cont->release_ctx(file_state_cont, *sess_id);

    /* init the input params for the conveters */
    err = mfp_init_input_params_ts(*sess_id, &fconv_ip);
    if (err) {
	goto error;
    }

    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "starting conversion from"
		" TS to iphone Segments"); 
    err = ts2iphone_conv.init_file_conv(fconv_ip, (void**)&conv_ctx);
    if (err) {
	goto error;
    }

    /* set the session state */
    file_state_cont->acquire_ctx(file_state_cont, *sess_id);
    pub->op = PUB_SESS_STATE_RUNNING;
    file_state_cont->release_ctx(file_state_cont, *sess_id);

    if (glob_mfp_fconv_tpool_enable) {
	mfp_tpool_add_read_task(&ts2iphone_conv, (void*)conv_ctx,
				&sess_id,
				(void*)out); 
	return;
    } else {
	do {
	    err = ts2iphone_conv.read_and_process(conv_ctx,
						  (void**)&out); 
	    if (pub->op == PUB_SESS_STATE_STOP) {
		err = E_VPE_FILE_CONVERT_STOP;
	    }
	    switch (err) {
		case E_VPE_FILE_CONVERT_STOP:
		    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "completed"
				" conversion from TS to iphone Segments -"
				" status code %d", err);
		    AO_fetch_and_add1(&glob_mfp_file_tot_well_finished);
		    AO_fetch_and_sub1(&glob_mfp_file_num_active_sessions);
		    ts2iphone_conv.cleanup_file_conv(conv_ctx);
		    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "cleaning up"
				" conversion from TS to iphone Segments");
		    goto error;
		case E_VPE_FILE_CONVERT_CONTINUE:
		    err1 = ts2iphone_conv.write_out_data((void*)out);
		    if (err1) {
			AO_fetch_and_add1(&glob_mfp_file_tot_err_sessions);
			AO_fetch_and_sub1(&glob_mfp_file_num_active_sessions);
			ts2iphone_conv.cleanup_file_conv(conv_ctx);
			goto error;
		    }
		    break;
		case E_VPE_FILE_CONVERT_SKIP:
		    break;
		default:
		    AO_fetch_and_sub1(&glob_mfp_file_num_active_sessions);
		    AO_fetch_and_add1(&glob_mfp_file_tot_err_sessions);
		    DBG_MFPLOG (pub->name, ERROR, MOD_MFPFILE, "error"
				" occured during conversion from TS" 
				" to iPhone Segments - error code %d",
				err); 
		    ts2iphone_conv.cleanup_file_conv(conv_ctx);
		    goto error;
	    }
	} while (1);
    }

 error:
    if (fconv_ip) free(fconv_ip);
    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "conversion status %d",
		err);
    mfp_mgmt_sess_unlink((char*)pub->name, PUB_SESS_STATE_IDLE, err);
    ref_cont->release_ref_cont(ref_cont);
}


/**
 * cleanup a thread pool task
 * @param arg - the input argument passed from the thread pool cleanup
 * event
 */
static void
mfp_tpool_cleanup_read_task(void *arg)
{
    //if (arg)
    //free(arg);
}

/**
 * create and add a 'read' task into the thread pool
 * @param intf - the file converter interface
 * @param input - the input to the 'read_and_process' callback
 * implemented by the the file converters
 * @param output - the output processed data from the file
 * converter. this needs to be used to create a write task
 */
static int32_t
mfp_tpool_add_read_task(file_conv_intf_t *intf,
			void *input, void *input2, 
			void *output)
{
    mfp_fconv_tpool_obj_t *obj;
    thread_pool_task_t* work_item;

    obj = (mfp_fconv_tpool_obj_t*)
	nkn_calloc_type(1, sizeof(mfp_fconv_tpool_obj_t),
			mod_mfp_fconv_tpool_obj_t);

    obj->intf = intf;
    obj->input = input;
    obj->sess_id = *((int32_t*)(input2));
    obj->output = output;
    
    work_item = newThreadPoolTask(mfp_tpool_hdl_read_task,
				  obj,
				  mfp_tpool_cleanup_read_task);
    read_thread_pool->add_work_item(read_thread_pool, work_item);
    return 0;
    
}

/** 
 * create and add a write task to the thread pool
 * @param intf - the file converter interface
 * @param input - the input to the 'write_output' callback
 * implemented by the the file converters
 * @param output - NULL always
 */
static int32_t
mfp_tpool_add_write_task(file_conv_intf_t *intf,
 			 void *input, void *input2,
			 void *output)
{
    mfp_fconv_tpool_obj_t *obj;
    thread_pool_task_t* work_item;

    obj = (mfp_fconv_tpool_obj_t*)
	nkn_calloc_type(1, sizeof(mfp_fconv_tpool_obj_t),
			mod_mfp_fconv_tpool_obj_t);

    obj->intf = intf;
    obj->input = input;
    obj->sess_id = *((int32_t*)(input2));
    obj->output = output;
    work_item = newThreadPoolTask(mfp_tpool_hdl_write_task,
				  obj,
				  mfp_tpool_cleanup_read_task);
    write_thread_pool->add_work_item(write_thread_pool, work_item);
    return 0;
}

/** 
 * task handler for respective task types woked up by the thread
 * pool 
 * @param arg - the private data that is needed by the read handler
 * (instantiated at the time of task creation)
 */
static void
mfp_tpool_hdl_read_task(void *arg)
{
    mfp_fconv_tpool_obj_t *tp_in;
    file_conv_intf_t *intf;
    int32_t err;

    err = 0;
    tp_in = (mfp_fconv_tpool_obj_t*)arg;
    intf = (file_conv_intf_t*)tp_in->intf;
    err = intf->read_and_process(tp_in->input, &tp_in->output);

    DBG_MFPLOG ("0", MSG, MOD_MFPFILE, "read err code %d\n", err);
    switch (err) {
	case E_VPE_FILE_CONVERT_STOP:
	    //printf("completed conversion\n");
	    DBG_MFPLOG ("0", MSG, MOD_MFPFILE, "completed"
			" conversion  error code %d", err);
	    goto error;
	case E_VPE_FILE_CONVERT_CONTINUE:
	    DBG_MFPLOG ("0", ERROR, MOD_MFPFILE, "write task create: address of output buf %p\n",
		   tp_in->output); 
	    mfp_tpool_add_write_task(tp_in->intf, tp_in->output,
				     &tp_in->sess_id,
				     NULL); 
	    mfp_tpool_add_read_task(tp_in->intf, tp_in->input,
				    &tp_in->sess_id,
				    NULL);
	    break;
	case E_VPE_FILE_CONVERT_SKIP:
	    mfp_tpool_add_read_task(tp_in->intf, tp_in->input,
				    &tp_in->sess_id,
				    NULL); 
	    break;
	default:
	    DBG_MFPLOG ("0", ERROR, MOD_MFPFILE,"error reading/processing input"
		   "(errno=%d)\n", err);
	    DBG_MFPLOG ("0", ERROR, MOD_MFPFILE, "error"
			" occured during conversion error code %d",
			err);
	    break;
    }

 error:
    return;
}

/** 
 * task handler for respective task types woken up by the thread
 * pool 
 * @param arg - the private data that is needed by the write handler
 * (instantiated at the time of task creation)
 */
static void
mfp_tpool_hdl_write_task(void *arg)
{
    mfp_fconv_tpool_obj_t *tp_in;
    file_conv_intf_t *intf;
    int32_t err;

    err = 0;
    tp_in = (mfp_fconv_tpool_obj_t*)arg;
    intf = (file_conv_intf_t*)tp_in->intf;

    DBG_MFPLOG ("0", MSG, MOD_MFPFILE, "write task hanlder: address of output buf %p\n",
	   tp_in->input); 
    err = intf->write_out_data(tp_in->input);
    if (err == E_VPE_FILE_CONVERT_CLEANUP_PENDING) {
	mfp_tpool_add_write_task(tp_in->intf, tp_in->input,
				 &tp_in->sess_id, NULL);
    }

}

/**
 * create and add convert task
 * @param converter - the converter trigger function
 * @param sess_id - sess id for the session
 */
static int32_t
mfp_tpool_add_convert_task(taskHandler converter, ref_count_mem_t *ref_cont)
{
    thread_pool_task_t* work_item;

    work_item = newThreadPoolTask(converter,
				  ref_cont,
				  mfp_tpool_cleanup_read_task);
    read_thread_pool->add_work_item(read_thread_pool, work_item);
    return 0;
    
}

/**
 * cleanup the input params structure for initializing the ISMV to
 * MOOF converters
 */
static void
mfp_cleanup_input_params_ismv(file_conv_ip_params_t *fconv)
{
    if (fconv) {
	free(fconv);
    }
}

/**
 * initializes the input params structure that will be passed as an
 * input to the respective file converter
 * @param pub - the publisher context from which a valid/required set
 * of parameters are copied into the input param structure
 * @param sess_id - the session id of the file conversion session
 * @param out - a fully populated input param structure
 * @return returns 0 on success and a negative number on error
 */
static int32_t
mfp_init_input_params_ismv(int32_t sess_id,
			   int32_t ismv2iphone_flag,
			   file_conv_ip_params_t **out)    
{
    mfp_publ_t *pub = file_state_cont->get_ctx(file_state_cont, sess_id); 
    int32_t i, j, med_type_flag, err, num_profiles;
    char *ismc_path, *ism_path, *ismv_path;
    FILE *f_ismc, *f_ism;
    stream_param_t *sp;
    ism_bitrate_map_t *map;
    io_handlers_t  *iocb;
    file_conv_ip_params_t *fconv_ip;
    ism_bitrate_map_t *ism_map; 
    
    med_type_flag = 0;
    err = 0;
    fconv_ip = NULL;
    iocb = NULL;
    ism_map = NULL;

    if (pub->ms_parm.status == FMTR_ON && ismv2iphone_flag) {
	AO_fetch_and_add1(&glob_mfp_live_num_pmfs_cntr);
    }

    *out = fconv_ip = (file_conv_ip_params_t *) 
	nkn_calloc_type(1, sizeof(file_conv_ip_params_t),
			mod_file_conv_ip_params_t);
    if (!fconv_ip) {
	return -E_MFP_PMF_NO_MEM;
    }
    
    for (i = 0; i < pub->encaps_count; i++) {
	sp = pub->encaps[i];
	for (j = 0; j < (int32_t)pub->streams_per_encaps[i]; j++) {
	    switch (sp[j].med_type) {
		case SVR_MANIFEST:
		    if (med_type_flag & 0x01) {
			err = E_MFP_PMF_INCOMPATIBLE_MEDIA_TYPE;
			goto error;
		    }
		    ism_path = (char*)\
			sp[j].media_src.file_src.file_url;
		    ism_path = remove_trailing_slash(ism_path);
		    if (strstr(ism_path, "http://")) {
			iocb = &ioh_http;
		    } else {
			iocb = &iohldr;
		    }

		    err = ism_read_map_from_file(ism_path, &ism_map,
						 iocb);
		    if (err) {
			goto error;
		    }
		    num_profiles = ism_map->attr[0]->n_profiles;
		    med_type_flag |= 0x01;
		    break;
		case CLIENT_MANIFEST:
		    if (med_type_flag & 0x02) {
			err = E_MFP_PMF_INCOMPATIBLE_MEDIA_TYPE;
			goto error;
		    }
		    ismc_path = (char *)\
			sp[j].media_src.file_src.file_url;
		    ismc_path = remove_trailing_slash(ismc_path);
		    med_type_flag |= 0x02;
		    break;
		case MUX:
		case AUDIO:
		case VIDEO:
		    if (med_type_flag & 0x04) {
			err = E_MFP_PMF_INCOMPATIBLE_MEDIA_TYPE;
			goto error;
		    }
		    ismv_path = (char *)\
			sp[j].media_src.file_src.file_url;
		    ismv_path = remove_trailing_slash(ismv_path);
		    med_type_flag |= 0x04;
		    break;
		default:
		    break;
	    }
	}
    }
    if (pub->ms_parm.status == FMTR_ON && ismv2iphone_flag) {
	int32_t rv = 0;
	num_pmfs_cntr++;
	//AO_fetch_and_add1(&glob_mfp_live_num_pmfs_cntr);
	pub->ms_parm.dvr = 1;
	pub->ms_parm.segment_idx_precision = 10;
	pub->ms_parm.segment_start_idx = 1;
	// pub->ms_parm.min_segment_child_plist = 0;
	fconv_ip->error_callback_fn = &register_formatter_error_file;
	rv = fruit_formatter_init(pub, sess_id,
				  fconv_ip->error_callback_fn,
				  num_profiles);
	if (rv < 0) {
	    DBG_MFPLOG(pub->name, ERROR, MOD_MFPFILE,
		       "Reached Maximum Parallel Apple conversions"
		       " Number of currently active sessions = %ld",
		       glob_mfp_live_num_pmfs_cntr);
	    err = -E_MFP_FRUIT_INIT;
	    goto error;
	} 
	printf("num pmfs = %d\n", sess_id);
	fconv_ip->num_pmf = sess_id;//glob_mfp_live_num_pmfs_cntr;
        fconv_ip->output_path = (char *)pub->ms_store_url;
	fconv_ip->output_path = remove_trailing_slash(fconv_ip->output_path);
        fconv_ip->delivery_url = (char*)pub->ms_delivery_url;
	fconv_ip->streaming_type = MOBILE_STREAMING;
    } else if (pub->ss_parm.status == FMTR_ON) {
        fconv_ip->ss_package_type = ISMV_PKG_TYPE_SBR;
        fconv_ip->output_path = (char *)pub->ss_store_url;
        fconv_ip->delivery_url = (char*)pub->ss_delivery_url;
	fconv_ip->streaming_type = SMOOTH_STREAMING;
    }
    if (!(med_type_flag & 0x07)) {
	err = -E_MFP_PMF_INSUFF_MEDIA_TYPE;
	goto error;
    }

   fconv_ip->n_input_files = 3;
   fconv_ip->input_path[0] = ismv_path;
   fconv_ip->input_path[1] = ism_path;
   //fconv_ip->output_path = (char *)pub->ms_store_url;
   fconv_ip->input_path[2] = ismc_path;
   fconv_ip->sess_id = sess_id;
   fconv_ip->streams_per_encap = pub->streams_per_encaps[0];

   ism_cleanup_map(ism_map);
   return err;
 error:
   if (fconv_ip) free(fconv_ip);
   if (ism_map) ism_cleanup_map(ism_map);
   return err;
}

/**
 * initializes the input params structure that will be passed as an
 * input to the respective file converter
 * @param pub - the publisher context from which a valid/required set
 * of parameters are copied into the input param structure
 * @param sess_id - the session id of the file conversion session
 * @return returns 0 on success and a negative number on error
 */
static int32_t
mfp_init_input_params_ts(int32_t sess_id,
			 file_conv_ip_params_t **out) 
{
    mfp_publ_t *pub = file_state_cont->get_ctx(file_state_cont, sess_id); 
    int32_t i, j, k, med_type_flag,err;
    stream_param_t *sp;
    file_conv_ip_params_t *fconv_ip;
    
    med_type_flag = 0;
    err = 0;
    fconv_ip = NULL;
    k = 0;

    *out = fconv_ip = (file_conv_ip_params_t *) 
	nkn_calloc_type(1, sizeof(file_conv_ip_params_t),
			mod_file_conv_ip_params_t);
    if (!fconv_ip) {
	return -E_MFP_PMF_NO_MEM;
    }
    
    for (i = 0; i < pub->encaps_count; i++) {
	sp = pub->encaps[i];
	for (j = 0; j < (int32_t)pub->streams_per_encaps[i]; j++) {
	    switch (sp[j].med_type) {
		case SVR_MANIFEST:
		case CLIENT_MANIFEST:
		    break;
		case MUX:
		case AUDIO:
		case VIDEO:
		    fconv_ip->input_path[k]= \
			(char *)sp[j].media_src.file_src.file_url;
		    fconv_ip->profile_bitrate[k] = sp[j].bit_rate;
		    k++;
		    break;
		default:
		    break;
	    }
	}
    }
    
   fconv_ip->n_input_files = k;
   fconv_ip->output_path = (char *)pub->ms_store_url;
   fconv_ip->key_frame_interval = pub->ms_parm.key_frame_interval;
   fconv_ip->segment_start_idx = pub->ms_parm.segment_start_idx;
   fconv_ip->min_segment_child_plist =\
       pub->ms_parm.min_segment_child_plist;
   fconv_ip->segment_idx_precision = \
       pub->ms_parm.segment_idx_precision;
   fconv_ip->sess_id = sess_id;
   fconv_ip->delivery_url = (char*)pub->ms_delivery_url;

   return err;
 error:
   if (fconv_ip) free(fconv_ip);
   return err;

}


static int32_t
moof2mfu_prepare_input(void *out, void *private_data)
{
    moof2mfu_converter_ctx_t *conv_ctx;
    uint32_t moof_count=0;
    ismv2moof_write_array_t *writer_array;
    uint32_t i,j;
    ismv2moof_write_t *writer;
    ismv_moof_proc_buf_t *proc_buf;
    vpe_olt_t *ol;
    uint32_t video_track_count = 0;
    uint32_t audio_track_count = 0;
    moof2mfu_builder_t *bldr;
    int32_t err=0;
    uint32_t num_moof_stored;
    size_t temp_offset = 0;

    err = 0;
    conv_ctx = (moof2mfu_converter_ctx_t*)(private_data);
    bldr = conv_ctx->bldr;
    writer_array = (ismv2moof_write_array_t*)out;
    bldr->num_pmf = writer_array->num_pmf;
    bldr->total_moof_count = writer_array->total_moof_count;
    for (j=0;j<writer_array->num_tracks_towrite;j++) {
	temp_offset = 0;
        writer = &writer_array->writer[j];
	
	if(writer->trak_type == 0/*VIDE_HEX*/) {
	    if(writer->out != NULL) {
		proc_buf = writer->out;
		ol = proc_buf->ol_list;
		bldr->num_moof_count =
		    proc_buf->ol_list_num_entries;
		num_moof_stored =
		    bldr->video[video_track_count].num_moof_stored;
		for(i = 0; i<bldr->num_moof_count; i++) {
		    bldr->video[video_track_count].moofs[num_moof_stored]
			= nkn_calloc_type(1, ol[i].length,
                                          mod_vpe_moof_array_t);
		    if(!bldr->video[video_track_count].moofs[num_moof_stored]) {
			return -E_MFP_NO_MEM;
		    }
		    memcpy
			(bldr->video[video_track_count].moofs[num_moof_stored],
			 proc_buf->buf + temp_offset, ol[i].length);
		    temp_offset+= ol[i].length;

		    bldr->video[video_track_count].moof_len[num_moof_stored] = 
			ol[i].length;
		    num_moof_stored++;

		}
		bldr->video[video_track_count].num_moof_stored =
		    num_moof_stored;
		bldr->video[video_track_count].profile = writer->profile;
	    }
	    //bldr->video[video_track_count].profile = writer->profile;
	    video_track_count++;
	}
	if(writer->trak_type == 1/*SOUN_HEX*/) {
	    if(writer->out != NULL) {
		proc_buf = writer->out;
		ol = proc_buf->ol_list;
		bldr->num_moof_count =
		    proc_buf->ol_list_num_entries;
		num_moof_stored =
		    bldr->audio[audio_track_count].num_moof_stored;
		for(i = 0; i<bldr->num_moof_count; i++) {
                    bldr->audio[audio_track_count].moofs[num_moof_stored]
                        = nkn_calloc_type(1, ol[i].length,
					  mod_vpe_moof_array_t);
		    if(!bldr->audio[audio_track_count].moofs[num_moof_stored]) {
			return -E_MFP_NO_MEM;
		    }
                    memcpy
                        (bldr->audio[audio_track_count].moofs[num_moof_stored],
                         proc_buf->buf + temp_offset, ol[i].length);
		    temp_offset+= ol[i].length;
		    bldr->audio[audio_track_count].moof_len[num_moof_stored] = 
			ol[i].length;
		    num_moof_stored++;
		}
		bldr->audio[audio_track_count].num_moof_stored =
		    num_moof_stored;
		bldr->audio[audio_track_count].profile = writer->profile;
	    }
	    audio_track_count++;
	}
    }//for loop
    /*cleaning the writer_array after copying or preparing input for
      next function*/
    ismv2moof_cleanup_writer_array(writer_array);    

    return err;
}


static int32_t
mfu2ts_prepare_input(void *out, void *private_data)
{
    mfu2ts_converter_ctx_t *conv_ctx;
    int32_t err=0;
    mfu2ts_builder_t *bldr;
    moof2mfu_write_array_t *writer_array;
    moof2mfu_write_t *writer;
    uint32_t i=0,j=0;
    mfu_data_t* mfu_data;
    err = 0;
    conv_ctx = (mfu2ts_converter_ctx_t*)(private_data);
    bldr = conv_ctx->bldr;
    writer_array = (moof2mfu_write_array_t*)out;
    bldr->num_mfu_count = writer_array->num_mfu_towrite;
    bldr->num_mfu_processed = 0;
    bldr->is_mfu2ts_conv_stop = writer_array->is_moof2mfu_conv_stop;
    bldr->is_last_mfu = writer_array->is_last_moof;
    bldr->num_pmf = writer_array->num_pmf;
    bldr->seq_num = writer_array->seq_num;
    bldr->streams_per_encap = writer_array->streams_per_encap;
    bldr->n_traks = writer_array->n_traks;
    //bldr->num_moofs = writer_array->num_moofs;
    bldr->mfu_data = nkn_calloc_type(bldr->num_mfu_count,
				     sizeof(mfu_data_t),
				     mod_vpe_moof_temp_buf);
    if (!bldr->mfu_data) {
	err = -E_MFP_NO_MEM;
	return err;
    }
    for(i=0;i<writer_array->num_mfu_towrite;i++) {
	bldr->mfu_data[i].data_size = writer_array->writer[i].out->data_size;
	bldr->mfu_data[i].data = nkn_calloc_type(1,
						 bldr->mfu_data[i].data_size,
						 mod_vpe_mfu_array_t);
	if(!bldr->mfu_data[i].data) {
	    return -E_MFP_NO_MEM;
	}

	memcpy( bldr->mfu_data[i].data, writer_array->writer[i].out->data,
		bldr->mfu_data[i].data_size);

	bldr->num_mfu_stored++;
    }
    bldr->tot_mfu_count = bldr->tot_mfu_count +
	writer_array->num_mfu_towrite;
    /*cleaning the writer_array after copying or preparing input for
      next function*/
    moof2mfu_cleanup_writer_array(writer_array);
    return err;
}
static int32_t
send_eos(int32_t n_aud_traks, int32_t n_vid_traks, int32_t seq_num, 
	 int32_t num_pmf, mfu2ts_builder_t *bldr) 
{
  int32_t k = 0, l=0;
  int32_t err = 0;
  mfu_data_t* mfu_data = NULL;
  for (k=0; k<n_aud_traks; k++) {
    for (l=0; l<n_vid_traks; l++) {
      //creating dummy mfu's
      mfu_data = (mfu_data_t*)nkn_calloc_type(1,
                                              sizeof(mfu_data_t),
                                              mod_vpe_mfu_data_t4);
      if (mfu_data == NULL)
        return -E_VPE_PARSER_NO_MEM;
      mfub_t* mfu_hdr = nkn_calloc_type(1, sizeof(mfub_t),
                                        mod_vpe_mfu_data_t5);
      if (mfu_hdr == NULL) {
        free(mfu_data);
        return -E_VPE_PARSER_NO_MEM;
      }
      /* allocate for the mfu */
      mfu_data->data_size = MFU_FIXED_SIZE + sizeof(mfub_t);
      mfu_data->data = (uint8_t*) nkn_malloc_type(mfu_data->data_size,
                                                  mod_vpe_mfp_ts2mfu_t);
      if (!mfu_data->data) {
        return -E_VPE_PARSER_NO_MEM;
      }
      mfu_hdr->offset_vdat = 0xffffffffffffffff;
      mfu_hdr->offset_adat = 0xffffffffffffffff;
      mfu_hdr->stream_id = k+l;
      mfu_hdr->mfu_size =  mfu_data->data_size;
      mfu_hdr->program_number = num_pmf;

      /* write the mfu header */
      mfu_write_mfub_box(sizeof(mfub_t),
                         mfu_hdr,
                         mfu_data->data);
      ref_count_mem_t* ref_cont =
	createRefCountedContainer(mfu_data, destroyMfuData);
      ref_cont->hold_ref_cont(ref_cont);

      fruit_fmt_input_t *fmt_input = NULL;
      fmt_input = nkn_calloc_type(1, sizeof(fruit_fmt_input_t),
				  mod_vpe_mfp_fruit_fmt_input_t);
      if (!fmt_input) {
	return -E_MFP_NO_MEM;
      }
      fmt_input->mfu_seq_num = seq_num;
      fmt_input->state = FRUIT_FMT_CONVERT;
      fmt_input->streams_per_encap = n_vid_traks;
      fmt_input->active_task_cntr =				\
	bldr->num_active_moof2ts_task[k+l];
      (*fmt_input->active_task_cntr)++;
      mfu_data->fruit_fmtr_ctx = (void*)fmt_input;
      
      //apple_fmtr_hold_encap_ctxt(get_sessid_from_mfu(mfu_data_cont));
      err = mfubox_ts(ref_cont);
      if(err != VPE_SUCCESS) {
	return -E_VPE_TS_CREATION_FAILED;
      }
      if(mfu_hdr != NULL) {
	free(mfu_hdr);
      }
    }
  }
  
  
  



  return err;
}

static char*
remove_trailing_slash(char *str)
{
    int len,i;

    i=0;
    len = 0;

    if(str==NULL){
        DBG_MFPLOG ("0", ERROR, MOD_MFPFILE, "string cannot be NULL\n");
        return NULL;
    }

    i = len = strlen(str);

    if(len == 0){
        DBG_MFPLOG ("0", ERROR, MOD_MFPFILE, "string cannot be a blank\n");
        return NULL;
    }

    while( i && str[--i]=='/'){

    }

    str[i+1]='\0';

    return str;

}

static int32_t
mfp_convert_pmf_add_ref(mfp_publ_t *pub, ref_count_mem_t *ref_cont)
{
    int32_t err = 0;

    /* for ISMV source,
     * ISMV->TS and ISMV->SL happen as two separate calls,
     * hence need to acquire the reference count twice.
     * For others, need to acquire ref count only once
     */
    switch (pub->encaps[0]->encaps_type) {
	case ISMV_SBR: 	
	    if (pub->ms_parm.status == FMTR_ON) {
		ref_cont->hold_ref_cont(ref_cont);
	    }
	    if (pub->ss_parm.status == FMTR_ON) {
		ref_cont->hold_ref_cont(ref_cont);
	    }
	    break;
	case TS:
	    ref_cont->hold_ref_cont(ref_cont);
	    break;
	case MP4:
	    ref_cont->hold_ref_cont(ref_cont);
	    break;
	default:
	    err = -E_MFP_INVALID_CONVERSION_REQ;
	    break;
    }

    return err;
}

static void
mfp_cleanup_ref_cont(void *private_data)
{
    int32_t *sess_id = (int32_t *)private_data;
    mfp_publ_t *pub = file_state_cont->get_ctx(file_state_cont,
					       *sess_id); 
    if (pub->ms_parm.status == FMTR_ON) {
	AO_fetch_and_sub1(&glob_mfp_live_num_pmfs_cntr);
	assert((int64_t)glob_mfp_live_num_pmfs_cntr >= 0);
    }
    file_state_cont->remove(file_state_cont, *sess_id);    
    free(sess_id);
}

void register_formatter_error_file(
				   ref_count_mem_t *ref_cont)
{

    int32_t sess_id, i;
    int32_t stream_id;
    fruit_encap_t *encaps;
    mfu_data_t *mfu_data = (mfu_data_t*)ref_cont->mem;
    fruit_fmt_input_t *fmt_input = (fruit_fmt_input_t
				    *)mfu_data->fruit_fmtr_ctx;

    mfu_mfub_box_t *mfu_box_ptr = parseMfuMfubBox(mfu_data->data);

    sess_id = mfu_box_ptr->mfub->program_number;
    stream_id = mfu_box_ptr->mfub->stream_id;
    encaps = &fruit_ctx.encaps[sess_id];

    DBG_MFPLOG("MFU2TS", MSG, MOD_MFPLIVE,
	       "entered register_formatter_error_file\n");

#if 0
    pthread_mutex_lock(&encaps->lock);
    for (i = 0;i < encaps->num_streams_active; i++) {
	fruit_stream_t *stream = &encaps->stream[i];
	stream->status = FRUIT_STREAM_ERROR;
    }
#endif 

    pthread_mutex_unlock(&encaps->lock);

    if(mfu_box_ptr->cmnbox){
	free(mfu_box_ptr->cmnbox);
	mfu_box_ptr->cmnbox = NULL;
    }
    if(mfu_box_ptr->mfub){
	free(mfu_box_ptr->mfub);
	mfu_box_ptr->mfub = NULL;
    }
    if(mfu_box_ptr){
	free(mfu_box_ptr);
	mfu_box_ptr = NULL;
    }
    mfp_publ_t * pub = (mfp_publ_t *)
	file_state_cont->get_ctx(file_state_cont,
				 sess_id);
    file_state_cont->acquire_ctx(file_state_cont, sess_id);
    if(pub){
	if(pub->op == PUB_SESS_STATE_RUNNING) {
	    pub->op = PUB_SESS_STATE_STOP;
	}
    }
    file_state_cont->release_ctx(file_state_cont, sess_id);

    return;
}
