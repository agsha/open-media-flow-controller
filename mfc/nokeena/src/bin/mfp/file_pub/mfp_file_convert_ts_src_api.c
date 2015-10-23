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

#include "nkn_memalloc.h"
#include "nkn_debug.h"
#include "nkn_stat.h"



#include "nkn_vpe_ts2anyformat_api.h"
//#include "nkn_vpe_pre_proc.h"


/***********************************************************
 *                EXTERN DECLARATIONS
 ************************************************************/
extern file_id_t *file_state_cont;
extern uint32_t glob_mfp_fconv_tpool_enable;
extern mfp_thread_pool_t *read_thread_pool, *write_thread_pool;
extern uint32_t glob_mfp_fconv_sync_mode;
extern int32_t fruit_formatter_init(mfp_publ_t *, uint32_t,
				    register_formatter_error_t,
				    int32_t);

extern int32_t num_pmfs_cntr;
extern AO_t glob_mfp_live_num_pmfs_cntr;

NKNCNT_EXTERN(mfp_file_tot_err_sessions, AO_t)
NKNCNT_EXTERN(mfp_file_num_active_sessions, AO_t)
NKNCNT_EXTERN(mfp_file_num_ctrl_sessions, AO_t)

#if 0
NKNCNT_DEF(mfp_file_tot_well_finished, AO_t,\
	       "", "num well finished file convert sessions")
#endif

static char*
remove_trailing_slash(char *str);

file_conv_intf_t ts2anyformat_conv = {
    .prepare_input = NULL,
    .init_file_conv = init_ts2anyformat_converter,
    .read_and_process = ts2anyformat_conversion,
    .write_out_data = NULL,
    .cleanup_file_conv = ts2anyformat_cleanup_converter
};


static int32_t
mfp_init_input_params_ts(int32_t sess_id,
			 file_conv_ip_params_t **out);

static inline int32_t 
mfp_cleanup_input_params_ts(file_conv_ip_params_t *fconv); 

/**
 * function that converts inputs specified in a pub context which is
 * TS src to any format (MOOF segments,TS segments)
 * @param pub - the publisher context that describes the media asset
 * and its corresponding output formats
 * @return returns 0 on success and negative number on error
 */
void
mfp_convert_ts2anyformat(void *arg)
{
    ref_count_mem_t *ref_cont = (ref_count_mem_t *)arg;
    int32_t *sess_id = (int32_t *)ref_cont->mem;
    mfp_publ_t *pub = file_state_cont->get_ctx(file_state_cont,
					       *sess_id);
    file_conv_ip_params_t *fconv_ip = NULL;
    int32_t err;
    ts2anyformat_converter_ctx_t *conv_ctx = NULL;
    void *out = NULL;
    uint32_t k=0;

    err = 0;
    /* set the session state */
    file_state_cont->acquire_ctx(file_state_cont, *sess_id);
    pub->op = PUB_SESS_STATE_INIT;
    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "Session State: INIT");
    file_state_cont->release_ctx(file_state_cont, *sess_id);
    /* init the input params for conversion */
    err = mfp_init_input_params_ts(*sess_id, &fconv_ip);
    if (err) {
        goto error;
    }
    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "starting conversion \
    from"  " Ts src to specified format");

    //fconv_ip->key_frame_interval = pub->ms_parm.key_frame_interval;
    //initialization
    err = ts2anyformat_conv.init_file_conv(fconv_ip, (void**)&conv_ctx);
    if (err) {
        goto error;
    }
    conv_ctx->sess_id = *sess_id;
    /* set the session state */
    file_state_cont->acquire_ctx(file_state_cont, *sess_id);
    pub->op = PUB_SESS_STATE_RUNNING;
    file_state_cont->release_ctx(file_state_cont, *sess_id);
    if (glob_mfp_fconv_tpool_enable) {
#if 0
        mfp_tpool_add_read_task(&ts2anyformat_conv, (void*)conv_ctx,
                                &sess_id,
                                (void*)out);
#endif
        return;
    } else {
	//conversion[ts to ts or ts to moof]
	err = ts2anyformat_conv.read_and_process(conv_ctx,
					      (void**)&out);
#if 0
	if (pub->op == PUB_SESS_STATE_STOP) {
	  err = -E_VPE_FORMATTER_ERROR;
	  if(pub->ms_parm.status == FMTR_ON){
	    for(k = 0; k < conv_ctx->max_profile; k++){
	      register_stream_dead(*sess_id, k);
	    }
	    DBG_MFPLOG(pub->name, MSG, MOD_MFPLIVE,
		       "called register_stream_dead\n");
	  }	
	}
#endif
	switch (err) {
	    case E_VPE_FILE_CONVERT_STOP:
		DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "completed"
			    " conversion from TS to specified foramt"
			    " error code %d", err);
		//AO_fetch_and_add1(&glob_mfp_file_tot_well_finished);
		AO_fetch_and_sub1(&glob_mfp_file_num_active_sessions);
		ts2anyformat_conv.cleanup_file_conv(conv_ctx);
		DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "cleaning up"
			    " conversion from TS to any format Segments");
		goto error;
	    default:
		AO_fetch_and_sub1(&glob_mfp_file_num_active_sessions);
		AO_fetch_and_add1(&glob_mfp_file_tot_err_sessions);
		DBG_MFPLOG (pub->name, ERROR, MOD_MFPFILE, "error"
			    " occured during conversion from TS"
			    " to iPhone Segments - error code %d",
			    err);
		ts2anyformat_conv.cleanup_file_conv(conv_ctx);
		goto error;
	}
    }
 error:
    if (fconv_ip && fconv_ip->ms_param_status){// && !pub->ms_parm.encr_flag){
	  apple_fmtr_release_encap_ctxt(*sess_id);
    }
    mfp_cleanup_input_params_ts(fconv_ip);
    DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "conversion status %d",
		err);
    mfp_mgmt_sess_unlink((char*)pub->name, PUB_SESS_STATE_IDLE, err);
    ref_cont->release_ref_cont(ref_cont);
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
int32_t
mfp_init_input_params_ts(int32_t sess_id,
                         file_conv_ip_params_t **out)
{
    mfp_publ_t *pub = file_state_cont->get_ctx(file_state_cont,
    sess_id);
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
	fconv_ip->sess_name = nkn_calloc_type(strlen((char*)pub->name) + 1,
			sizeof(char), mod_file_conv_ip_params_t);
	if (fconv_ip->sess_name == NULL)
		return -E_MFP_PMF_NO_MEM;
	strcpy(fconv_ip->sess_name, (char*)pub->name);
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
    if (pub->ms_parm.status == FMTR_ON) {
	int32_t rv = 0;
	num_pmfs_cntr++;
	AO_fetch_and_add1(&glob_mfp_live_num_pmfs_cntr);
	pub->ms_parm.dvr = 1;
	pub->ms_parm.segment_idx_precision = 10;
	pub->ms_parm.segment_start_idx = 1;
	//pub->ms_parm.min_segment_child_plist = 0;
	fconv_ip->error_callback_fn = &register_formatter_error_file;
	rv = fruit_formatter_init(pub, sess_id, fconv_ip->error_callback_fn, 0);
	if (rv < 0) {
	    DBG_MFPLOG(pub->name, ERROR, MOD_MFPFILE,
		       "Error in fruit_formatter_init");
	    err = -E_MFP_FRUIT_INIT;
	    goto error;
	}
	//printf("num pmfs = %d\n", sess_id);
	fconv_ip->num_pmf = sess_id;//glob_mfp_live_num_pmfs_cntr;
	fconv_ip->ms_param_status = 1;
	fconv_ip->output_path = (char *)pub->ms_store_url;
	fconv_ip->delivery_url = (char*)pub->ms_delivery_url;
	fconv_ip->segment_start_idx = pub->ms_parm.segment_start_idx;
	fconv_ip->min_segment_child_plist =	\
	    pub->ms_parm.min_segment_child_plist;
	fconv_ip->segment_idx_precision =	\
	    pub->ms_parm.segment_idx_precision;
	fconv_ip->streaming_type = MOBILE_STREAMING;
    }
    if (pub->ss_parm.status == FMTR_ON) {
	fconv_ip->ss_param_status = 1;
	fconv_ip->output_path = (char *)pub->ss_store_url;
	fconv_ip->delivery_url = NULL;
	fconv_ip->streaming_type = SMOOTH_STREAMING;
    }
    if(pub->ms_parm.status == FMTR_ON){
      fconv_ip->key_frame_interval = pub->ms_parm.key_frame_interval;
    }
    if((pub->ss_parm.status  == FMTR_ON)&&(pub->ss_parm.key_frame_interval > 0)){
      if(fconv_ip->key_frame_interval) {
	if (fconv_ip->key_frame_interval > (int32_t)pub->ss_parm.key_frame_interval) {
	  fconv_ip->key_frame_interval = pub->ss_parm.key_frame_interval;
	}
      } else {
	fconv_ip->key_frame_interval = pub->ss_parm.key_frame_interval;
      }
    }
    fconv_ip->output_path =
	remove_trailing_slash(fconv_ip->output_path);
    fconv_ip->sess_id = sess_id;

   return err;
 error:
   if (fconv_ip) {
       free(fconv_ip->sess_name);
       free(fconv_ip);
       *out = NULL;
   }
   return err;

}

static inline int32_t 
mfp_cleanup_input_params_ts(file_conv_ip_params_t *fconv_ip)
{
   if (fconv_ip) {
       free(fconv_ip->sess_name);
       free(fconv_ip);
   }

   return 0;
}

static char*
remove_trailing_slash(char *str)
{
    int len,i;

    i=0;
    len = 0;

    if(str==NULL){
        printf("string cannot be NULL\n");
        return NULL;
    }

    i = len = strlen(str);

    if(len == 0){
        printf("string cannot be a blank\n");
        return NULL;
    }
    while( i && str[--i]=='/'){

    }

    str[i+1]='\0';

    return str;

}
