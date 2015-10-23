#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

//mfp includes
#include "mfp_publ_context.h"

//nkn includes
#include "nkn_mem_counter_def.h"

//nkn video parser includes
#include "nkn_vpe_ism_read_api.h"
#include "nkn_mem_counter_def.h"

//dispatacher includes
#include "disp/entity_context.h"

//zvm includes
//#include "zvm_api.h"

/*************************************************************
 *           HELPER - (STATIC) FUNCTION PROTOTYPES
 ************************************************************/
static void deletePublishContext(mfp_publ_t* pub_ctx);
static void initPubContextToNull(mfp_publ_t* pub_ctx);
static void initStreamContextToNull(stream_param_t* stream_parm, 
	SOURCE_TYPE src_type); 
static int8_t allocStreamParmContext(stream_param_t* stream_parm,
	uint32_t max_streams, 
	SOURCE_TYPE src_type); 
static void deleteStreamContext(stream_param_t* stream_parm, 
	SOURCE_TYPE src_type); 

/******************************************************************
 *              HELPER FUNCTION IMPLEMENTATION
 *****************************************************************/
/**
 * creates a new publisher context
 * @param encaps_count - the number of encapsulations in the PMF file
 * @param src_type - the source type as defined in the PMF file (can
 * be LIVE/FILE)
 * @return - returns the address of a valid publisher context, NULL on
 * error
 */

uint32_t create_pub_dbg_counter(
	mfp_publ_t* pub_ctx,
	char **name,
	char *instance,
	char *string,
	void *count_var,
	uint32_t var_byte_size){

    *name = (char *)mfp_live_calloc(1, 256);

    if (!*name) {
	DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "EXITTING DUE TO LACK OF MEMORY\n");
	return 0;
    }
    strcpy(*name, string);
    nkn_mon_add(*name, instance,
	    (void*)count_var,
	    var_byte_size);
    memset(count_var, 0, var_byte_size);

    return (1);
}


uint32_t newPublishContextStats(mfp_publ_t *pub_ctx){

    /* create the stats counters */
    pub_ctx->stats.instance = (char*)mfp_live_calloc(1, 256);
    if (!pub_ctx->stats.instance) {
	DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "EXITTING DUE TO LACK OF MEMORY\n");
	return 0;
    }

    snprintf(pub_ctx->stats.instance, 256, "%s", pub_ctx->name);

    /*copying the name since we dont want a case where the reference
     *gets deleted due to some instantaneous cleanup
     */

    char *str = (char*)nkn_calloc_type(1, 256, mod_vpe_charbuf);
    uint32_t ret;
    if (!str) {
	DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPLIVE, "EXITTING DUE TO LACK OF MEMORY\n");
	return 0;
    }

    //counter 1
    snprintf(str, 256 , "mfp_live.num_multicast_strms");
    ret = create_pub_dbg_counter(pub_ctx, &pub_ctx->stats.num_multicast_strms_name, pub_ctx->stats.instance,
	    str, (void *)&pub_ctx->stats.num_multicast_strms, 4);
    if(ret == 0){
	free(str);
	return 0;
    }

    //counter 2
    snprintf(str, 256 , "mfp_live.num_unicast_strms");
    ret = create_pub_dbg_counter(pub_ctx, &pub_ctx->stats.num_unicast_strms_name, pub_ctx->stats.instance,
	    str, (void *)&pub_ctx->stats.num_unicast_strms, 4);
    if(ret == 0){
	free(str);
	return 0;
    }

    //counter 3
    snprintf(str, 256 , "mfp_live.tot_input_bytes");
    ret = create_pub_dbg_counter(pub_ctx, &pub_ctx->stats.tot_input_bytes_name, pub_ctx->stats.instance,
	    str, (void *)&pub_ctx->stats.tot_input_bytes, 8);
    if(ret == 0){
	free(str);
	return 0;
    }

    //counter 4
    snprintf(str, 256 , "mfp_live.tot_output_bytes");
    ret = create_pub_dbg_counter(pub_ctx, &pub_ctx->stats.tot_output_bytes_name, pub_ctx->stats.instance,
	    str, (void *)&pub_ctx->stats.tot_output_bytes, 8);
    if(ret == 0){
	free(str);
	return 0;
    }

    //counter 5
    snprintf(str, 256 , "mfp_live.num_apple_fmtr");
    ret = create_pub_dbg_counter(pub_ctx, &pub_ctx->stats.num_apple_fmtr_name, pub_ctx->stats.instance,
	    str, (void *)&pub_ctx->stats.num_apple_fmtr, 4);
    if(ret == 0){
	free(str);
	return 0;
    }

    //counter 6
    snprintf(str, 256 , "mfp_live.num_sl_fmtr");
    ret = create_pub_dbg_counter(pub_ctx, &pub_ctx->stats.num_sl_fmtr_name, pub_ctx->stats.instance,
	    str, (void *)&pub_ctx->stats.num_sl_fmtr, 4);
    if(ret == 0){
	free(str);
	return 0;
    }

    //counter 7
    snprintf(str, 256 , "mfp_live.num_zeri_fmtr");
    ret = create_pub_dbg_counter(pub_ctx, &pub_ctx->stats.num_zeri_fmtr_name, pub_ctx->stats.instance,
	    str, (void *)&pub_ctx->stats.num_zeri_fmtr, 4);
    if(ret == 0){
	free(str);
	return 0;
    }


    if(str != NULL){
	free(str);
	str = NULL;
    }

    return 1;
}


mfp_publ_t* newPublishContext(uint8_t encaps_count,
	SOURCE_TYPE src_type) {

    if ((src_type != LIVE_SRC) && (src_type != FILE_SRC) &&
	    (src_type != CONTROL_SRC))
	return NULL;

    if (encaps_count == 0)
	return NULL;
    mfp_publ_t* pub_ctx =
	(mfp_publ_t*)mfp_live_calloc(1, sizeof(mfp_publ_t));
    if (!pub_ctx)
	return NULL;

    pub_ctx->src_type = src_type;
    pub_ctx->encaps_count = encaps_count;
    uint32_t max_streams = encaps_count * MAX_STREAM_PER_ENCAP;

    initPubContextToNull(pub_ctx);

    pub_ctx->name = mfp_live_calloc(MAX_PUB_NAME_LEN, sizeof(int8_t));

    pub_ctx->encaps = mfp_live_calloc(encaps_count,
	    sizeof(stream_param_t*));
    pub_ctx->streams_per_encaps = mfp_live_calloc(encaps_count,
	    sizeof(uint32_t));
    pub_ctx->stream_parm = mfp_live_calloc(max_streams,
	    sizeof(stream_param_t));

    pub_ctx->ss_store_url = mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));
    pub_ctx->fs_store_url = mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));
    pub_ctx->ms_store_url = mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));
    pub_ctx->ss_delivery_url = mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));
    pub_ctx->fs_delivery_url = mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));
    pub_ctx->ms_delivery_url = mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));

    pub_ctx->accum_ctx = mfp_live_calloc(max_streams, sizeof(void*));
    pub_ctx->accum_intf = mfp_live_calloc(max_streams,
	    sizeof(accum_interface_t*));

    pub_ctx->sl_publ_fmtr = NULL;

    if ((!pub_ctx->ss_store_url) || (!pub_ctx->fs_store_url) ||
	    (!pub_ctx->ms_store_url) || (!pub_ctx->ss_delivery_url) ||
	    (!pub_ctx->fs_delivery_url) || (!pub_ctx->ms_delivery_url) ||
	    (!pub_ctx->name) || (!pub_ctx->stream_parm) ||
	    (!pub_ctx->accum_intf) || (!pub_ctx->accum_ctx) ||
	    (!pub_ctx->encaps) || (!pub_ctx->streams_per_encaps))
	goto exit_clean;

    pub_ctx->fs_parm.common_key = mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));
    if(pub_ctx->fs_parm.common_key == NULL){
	goto exit_clean;
    }
    pub_ctx->fs_parm.license_server_cert = mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));
    if(pub_ctx->fs_parm.common_key == NULL){
	goto exit_clean;
    }
    pub_ctx->fs_parm.license_server_url= mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));
    if(pub_ctx->fs_parm.common_key == NULL){
	goto exit_clean;
    }
    pub_ctx->fs_parm.transport_cert= mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));
    if(pub_ctx->fs_parm.common_key == NULL){
	goto exit_clean;
    }
    pub_ctx->fs_parm.packager_cert= mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));
    if(pub_ctx->fs_parm.common_key == NULL){
	goto exit_clean;
    }
    pub_ctx->fs_parm.credential_pwd= mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));
    if(pub_ctx->fs_parm.common_key == NULL){
	goto exit_clean;
    }
    pub_ctx->fs_parm.policy_file= mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));
    if(pub_ctx->fs_parm.common_key == NULL){
	goto exit_clean;
    }
    pub_ctx->fs_parm.content_id= mfp_live_calloc(MAX_URL_LEN, sizeof(int8_t));
    if(pub_ctx->fs_parm.common_key == NULL){
	goto exit_clean;
    }


    uint32_t i = 0;
    for (; i < max_streams; i++) {
	initStreamContextToNull(&(pub_ctx->stream_parm[i]),
		pub_ctx->src_type);
	pub_ctx->accum_ctx[i] = NULL;
	pub_ctx->accum_intf[i] = NULL;
    }
    for (i = 0; i < max_streams; i++) {
	if (allocStreamParmContext(&(pub_ctx->stream_parm[i]),
		    max_streams, pub_ctx->src_type) == -1)
	    goto exit_clean;
    }
    for (i = 0; i < encaps_count; i++)
	pub_ctx->encaps[i] =
	    pub_ctx->stream_parm + (i * MAX_STREAM_PER_ENCAP);

    pub_ctx->encaps[0]->syncp.ref_accum_cntr = 0;

    pub_ctx->delete_publish_ctx = deletePublishContext;
    pub_ctx->active_fd_count = 0;

	pub_ctx->sess_chunk_ctr = 0;

    //init sync params
    memset(&pub_ctx->sync_params, 0, sizeof(sync_params_t));
    return pub_ctx;
exit_clean:
    deletePublishContext(pub_ctx);
    return NULL;
}

int32_t initControlPMF(mfp_publ_t *pub_ctx,
		       const char *const name,
		       PUB_SESS_OP_STATE op)
{
    pub_ctx->name = (int8_t *)strdup((const char *)name);
    pub_ctx->op = op;

    return 0;
}

static void deletePublishContext(mfp_publ_t* pub_ctx) {

    uint32_t i;
    uint32_t max_streams = pub_ctx->encaps_count * MAX_STREAM_PER_ENCAP;
    if (pub_ctx->encaps)
	free(pub_ctx->encaps);
    if (pub_ctx->streams_per_encaps)
	free(pub_ctx->streams_per_encaps);

    if (pub_ctx->accum_ctx) {
	for (i = 0; i < max_streams; i++)
	    if (pub_ctx->accum_ctx[i]) {
		pub_ctx->accum_intf[i]->handle_EOS(pub_ctx, i);
		pub_ctx->accum_intf[i]->delete_accum(pub_ctx->accum_ctx[i]);
	    }
	free(pub_ctx->accum_ctx);
    }

    if (pub_ctx->stream_parm) {
	for (i = 0; i < max_streams; i++)
	    deleteStreamContext(&(pub_ctx->stream_parm[i]),
		    pub_ctx->src_type);
	free(pub_ctx->stream_parm);
    }

    if (pub_ctx->op_cfg.disk_usage) {
	if (pub_ctx->op_cfg.sioc) 
	    mfp_safe_io_clean_ctx(pub_ctx->op_cfg.sioc);
    } else {
	if (pub_ctx->ms_parm.disk_usage && pub_ctx->ms_parm.sioc) 
	    mfp_safe_io_clean_ctx(pub_ctx->ms_parm.sioc);
	if (pub_ctx->ss_parm.disk_usage && pub_ctx->ss_parm.sioc)
	    mfp_safe_io_clean_ctx(pub_ctx->ss_parm.sioc);
    }

    if (pub_ctx->name)
	free(pub_ctx->name);
    if (pub_ctx->ss_store_url)
	free(pub_ctx->ss_store_url);
    if (pub_ctx->fs_store_url)
	free(pub_ctx->fs_store_url);
    if (pub_ctx->ms_store_url)
	free(pub_ctx->ms_store_url);
    if (pub_ctx->ss_delivery_url)
	free(pub_ctx->ss_delivery_url);
    if (pub_ctx->fs_delivery_url)
	free(pub_ctx->fs_delivery_url);
    if (pub_ctx->ms_delivery_url)
	free(pub_ctx->ms_delivery_url);
    if(pub_ctx->fs_parm.common_key){
	free(pub_ctx->fs_parm.common_key);
	pub_ctx->fs_parm.common_key = NULL;
    }
    if(pub_ctx->fs_parm.license_server_url){
	free(pub_ctx->fs_parm.license_server_url);
	pub_ctx->fs_parm.license_server_url = NULL;
    }
    if(pub_ctx->fs_parm.license_server_cert){
	free(pub_ctx->fs_parm.license_server_cert);
	pub_ctx->fs_parm.license_server_cert = NULL;
    }
    if(pub_ctx->fs_parm.transport_cert){
	free(pub_ctx->fs_parm.transport_cert);
	pub_ctx->fs_parm.transport_cert= NULL;
    }

    if(pub_ctx->fs_parm.packager_cert){
	free(pub_ctx->fs_parm.packager_cert);
	pub_ctx->fs_parm.packager_cert = NULL;
    }
    if(pub_ctx->fs_parm.credential_pwd){
	free(pub_ctx->fs_parm.credential_pwd);
	pub_ctx->fs_parm.credential_pwd= NULL;
    }
    if(pub_ctx->fs_parm.policy_file){
	free(pub_ctx->fs_parm.policy_file);
	pub_ctx->fs_parm.policy_file = NULL;
    }
    if(pub_ctx->fs_parm.content_id){
	free(pub_ctx->fs_parm.content_id);
	pub_ctx->fs_parm.content_id= NULL;
    }


    if (pub_ctx) {
	if (pub_ctx->sess_name_list) {
	    for (i = 0; i <pub_ctx->n_sess_names; i++) {
		if (pub_ctx->sess_name_list[i])
		    free(pub_ctx->sess_name_list[i]);
	    }
	}
    }

    if(pub_ctx->stats.num_multicast_strms_name){
	nkn_mon_delete(pub_ctx->stats.num_multicast_strms_name, pub_ctx->stats.instance);
	free(pub_ctx->stats.num_multicast_strms_name);
	pub_ctx->stats.num_multicast_strms_name = NULL;
    }
    if(pub_ctx->stats.num_unicast_strms_name){
	nkn_mon_delete(pub_ctx->stats.num_unicast_strms_name, pub_ctx->stats.instance);
	free(pub_ctx->stats.num_unicast_strms_name);
	pub_ctx->stats.num_unicast_strms_name = NULL;
    }

    if(pub_ctx->stats.tot_input_bytes_name){
	nkn_mon_delete(pub_ctx->stats.tot_input_bytes_name, pub_ctx->stats.instance);
	free(pub_ctx->stats.tot_input_bytes_name);
	pub_ctx->stats.tot_input_bytes_name = NULL;
    }

    if(pub_ctx->stats.tot_output_bytes_name){
	nkn_mon_delete(pub_ctx->stats.tot_output_bytes_name, pub_ctx->stats.instance);
	free(pub_ctx->stats.tot_output_bytes_name);
	pub_ctx->stats.tot_output_bytes_name = NULL;
    }

    if(pub_ctx->stats.num_apple_fmtr_name){
	nkn_mon_delete(pub_ctx->stats.num_apple_fmtr_name, pub_ctx->stats.instance);
	free(pub_ctx->stats.num_apple_fmtr_name);
	pub_ctx->stats.num_apple_fmtr_name = NULL;
    }
    if(pub_ctx->stats.num_sl_fmtr_name){
	nkn_mon_delete(pub_ctx->stats.num_sl_fmtr_name, pub_ctx->stats.instance);
	free(pub_ctx->stats.num_sl_fmtr_name);
	pub_ctx->stats.num_sl_fmtr_name = NULL;
    }
    if(pub_ctx->stats.num_zeri_fmtr_name){
	nkn_mon_delete(pub_ctx->stats.num_zeri_fmtr_name, pub_ctx->stats.instance);
	free(pub_ctx->stats.num_zeri_fmtr_name);
	pub_ctx->stats.num_zeri_fmtr_name = NULL;
    }
    if(pub_ctx->stats.instance){
	free(pub_ctx->stats.instance);
	pub_ctx->stats.instance = NULL;
    }


    if (pub_ctx->accum_intf)
	free(pub_ctx->accum_intf);

    //ZVM_DEQUEUE(pub_ctx);
    free(pub_ctx);
    // Note: sl_fmtr_t will not be freed: As they are part of async calls
}


int32_t allocSessNameList(mfp_publ_t *pub, uint32_t n_sess_names) {

    uint32_t i;
    int32_t err;

    err = 0;
    pub->n_sess_names = n_sess_names;

    /* allocate for the array of ptrs */
    pub->sess_name_list = (char **)mfp_live_calloc(n_sess_names,\
	    sizeof(char *));

    /* allocate for the char list */
    for (i = 0;i < n_sess_names; i++) {
	pub->sess_name_list[i] = (char*)\
				 mfp_live_calloc(MAX_PUB_NAME_LEN, sizeof(char));
	if (!pub->sess_name_list[i]) {
	    err = -E_MFP_NO_MEM;
	    goto error;
	}
    }

    return err;
error:
    for(;i > 0; i--) {
	free(pub->sess_name_list[i - 1]);
    }
    return err;
}

static void initPubContextToNull(mfp_publ_t* pub_ctx) {

    pub_ctx->name = NULL;
    pub_ctx->encaps = NULL;
    pub_ctx->streams_per_encaps = NULL;
    pub_ctx->stream_parm = NULL;

    pub_ctx->ss_store_url = NULL;
    pub_ctx->ss_delivery_url = NULL;

    pub_ctx->fs_store_url = NULL;
    pub_ctx->fs_delivery_url = NULL;

    pub_ctx->ms_store_url = NULL;
    pub_ctx->ms_delivery_url = NULL;

    pub_ctx->accum_ctx = NULL;
    pub_ctx->accum_intf = NULL;

    pub_ctx->fs_parm.common_key = NULL;
    pub_ctx->fs_parm.license_server_url = NULL;
    pub_ctx->fs_parm.license_server_cert = NULL;
    pub_ctx->fs_parm.transport_cert= NULL;

    pub_ctx->fs_parm.packager_cert = NULL;
    pub_ctx->fs_parm.credential_pwd= NULL;
    pub_ctx->fs_parm.policy_file = NULL;
    pub_ctx->fs_parm.content_id= NULL;

}


static void initStreamContextToNull(stream_param_t* stream_parm,
		SOURCE_TYPE src_type) {

	stream_parm->related_idx = NULL;
	stream_parm->stream_state = STRM_DEAD;
	stream_parm->med_type = MUX;
	if (src_type == FILE_SRC)
		stream_parm->media_src.file_src.file_url = NULL;
	else
		stream_parm->media_src.live_src.source_if.s_addr = 0;
}


static int8_t allocStreamParmContext(stream_param_t* stream_parm,
	uint32_t max_streams, SOURCE_TYPE src_type) {

    stream_parm->related_idx = mfp_live_calloc(max_streams,
	    sizeof(uint32_t));

    if (!stream_parm->related_idx)
	return -1;
    if (src_type == FILE_SRC) {
	stream_parm->media_src.file_src.file_url =
	    mfp_live_calloc((MAX_FILE_NAME_LEN),
		    sizeof(int8_t));
	if (!stream_parm->media_src.file_src.file_url)
	    return -1;
    }
    return 1;
}


static void deleteStreamContext(stream_param_t* stream_parm, 
	SOURCE_TYPE src_type) {

    if (stream_parm->related_idx)
	free(stream_parm->related_idx);
    if (src_type == FILE_SRC)
	if (stream_parm->media_src.file_src.file_url)
	    free(stream_parm->media_src.file_src.file_url);
}

