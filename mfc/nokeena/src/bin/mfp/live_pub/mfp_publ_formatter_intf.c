/*
  The file that brideges between formmater and publisher.
  Should ideally become the interface when Formatter 
  exists standalone
*/

#include "mfp_publ_context.h"
#include "mfu2iphone.h"
#include <string.h>

#define KEY_CHAIN_LENGTH_DEFAULT 1000

extern gbl_fruit_ctx_t fruit_ctx;
extern uint64_t glob_mfp_live_num_pmfs_cntr;
int32_t fruit_formatter_init(mfp_publ_t *, uint32_t,
			     register_formatter_error_t, 
			     int32_t);

/*****************************************************************
 *  Apple ENCRYPTION/AUTH related functions
 ****************************************************************/
static auth_msg_t *fruit_create_auth_msg(mfp_publ_t *pub);
static kms_client_key_store_t** init_fruit_encr_props(mfp_publ_t *pub,
		uint32_t sess_id);

static void deleteKmsKeyStores(void* ctx);

/*  Handle mobile formatter  */
int32_t fruit_formatter_init(
	mfp_publ_t *pub_ctx,
	uint32_t sess_id,
	register_formatter_error_t error_callback_fn,
        int32_t sl_input_num_profiles)
{
    mobile_parm_t parms;
    fruit_fmt_t fmt;
    char store_url[500]={'\0'};
    int32_t streams_per_encaps = 0;

    /*Get the ms-parms structure ready*/
    strncpy(pub_ctx->ms_parm.store_url,(char*)pub_ctx->ms_store_url,strlen((char*)pub_ctx->ms_store_url));
    strncpy(pub_ctx->ms_parm.delivery_url,(char*)pub_ctx->ms_delivery_url,strlen((char*)pub_ctx->ms_delivery_url));

    snprintf(store_url, strlen((char*)pub_ctx->ms_store_url)+2,"%s/",(char*)pub_ctx->ms_store_url);

    if (recursive_mkdir(store_url, 0755) != 0){
      return -1;
    }

    if( glob_mfp_live_num_pmfs_cntr == 1){
	fruit_ctx.num_encaps_active = 0;
    }
    parms = pub_ctx->ms_parm;
    if(parms.key_frame_interval!=pub_ctx->ms_parm.key_frame_interval)
	return -E_MFP_PUBL_FORM_KFI_UNEQUAL;
    if(parms.min_segment_child_plist > parms.segment_rollover_interval)
	return -1;
    /*Copy the publisher elements into the formatter*/
    fmt.key_frame_interval = parms.key_frame_interval;
    fmt.segment_start_idx = parms.segment_start_idx;
    fmt.min_segment_child_plist = parms.min_segment_child_plist + parms.segment_start_idx;
    fmt.segment_idx_precision = parms.segment_idx_precision;
    if(parms.dvr_window_length){
    	float temp_res = (parms.dvr_window_length * MIN_TO_SECONDS * SECS_TO_MILLISECS) / parms.key_frame_interval;
    	parms.segment_rollover_interval = (uint32_t)((((temp_res + 1)*10)-1)/10); //ceiling to nearest int
    }
    fmt.segment_rollover_interval = parms.segment_rollover_interval + parms.segment_start_idx;
    fmt.dvr = parms.dvr;
    fmt.kms_key_stores = NULL;
    if (pub_ctx->ms_parm.encr_flag) {
	    fmt.kms_key_stores = init_fruit_encr_props(pub_ctx, sess_id);
	    if (fmt.kms_key_stores == NULL)
		return -1;
    }

	strncpy(&fruit_ctx.encaps[sess_id].name[0], (char*)pub_ctx->name, 99);
	fruit_ctx.encaps[sess_id].name[99] = '\0';

	fruit_ctx.encaps[sess_id].kms_ctx_del_track = 
		createRefCountedContainer(fmt.kms_key_stores, deleteKmsKeyStores); 
	if (fruit_ctx.encaps[sess_id].kms_ctx_del_track == NULL)
		return -1;
	apple_fmtr_hold_encap_ctxt(sess_id);
	fruit_ctx.encaps[sess_id].kms_ctx_active_count =
		pub_ctx->streams_per_encaps[0]; 
    
    /**
     * if the soure is ISMV then the streams per encaps which is
     * copied to number of active streams in the encaps structure
     * should be read from the number of video profiles in the ISM file
     */
    if (pub_ctx->encaps[0]->encaps_type == ISMV_SBR ||
	pub_ctx->encaps[0]->encaps_type == ISMV_MBR) {
	streams_per_encaps = sl_input_num_profiles;
    } else {
	streams_per_encaps = pub_ctx->streams_per_encaps[0];
    }
	//strcpy(fmt.mfu_out_fruit_path, parms.store_url);
    strcpy(fmt.mfu_out_fruit_path, store_url);
    strcpy(fmt.mfu_uri_fruit_prefix,parms.delivery_url);
    if(activate_fruit_encap(fmt,(int32_t)sess_id, streams_per_encaps)<0){
	apple_fmtr_release_encap_ctxt(sess_id);
	return -1;
    }
    fruit_ctx.encaps[sess_id].error_callback_fn = error_callback_fn;
    return 1;
}


static kms_client_key_store_t** init_fruit_encr_props(mfp_publ_t *pub,
		uint32_t sess_id) 
{
    //if (pub->ms_parm.kms_addr[0] == '\0' || pub->ms_parm.kms_port == 0)
	//	return NULL;
	if ((pub->ms_parm.kms_type != VERIMATRIX) &&
			(pub->ms_parm.kms_type != NATIVE))
		return NULL;

	kms_client_key_store_t** kms_key_stores = 
		nkn_calloc_type(MAX_FRUIT_STREAMS,
				sizeof(kms_client_key_store_t*), mod_kms_info_t);
	if (kms_key_stores == NULL)
		return NULL;
	uint8_t* key_store_loc = (uint8_t*)&pub->ms_parm.store_url[0];
	uint8_t* key_delivery_base_uri = (uint8_t*)
		&pub->ms_parm.key_delivery_base_uri[0];
	if (key_delivery_base_uri[0] == '\0') {
		key_store_loc = NULL;
		key_delivery_base_uri = NULL;
	}
	if (pub->ms_parm.kms_type == NATIVE)
		if ((key_store_loc == NULL) || (key_delivery_base_uri == NULL))
			return NULL;

	uint32_t i;
	for (i = 0; i < MAX_FRUIT_STREAMS; i++) {

		uint32_t ch_id = (sess_id * MAX_STREAM_PER_ENCAP) + i;
		kms_client_t* kms_client = NULL;
		uint64_t key_chain_len = pow(10, pub->ms_parm.segment_idx_precision);
			
		if (pub->ms_parm.kms_type == VERIMATRIX)
			kms_client = createVerimatrixKMSClient(
					&pub->ms_parm.kms_addr[0], pub->ms_parm.kms_port, ch_id,
					key_chain_len, 0,
					key_store_loc, key_delivery_base_uri);
		else 
			kms_client = createNativeKMSClient(ch_id,
					key_chain_len,
					key_store_loc, key_delivery_base_uri);
		kms_key_stores[i] = createKMS_ClientKeyStore(
				     kms_client, pub->ms_parm.key_rotation_interval,
				     (const char *)pub->name, i);
	}
    return kms_key_stores; 
}


static void deleteKmsKeyStores(void* ctx) {

	if (ctx == NULL)
		return;
	DBG_MFPLOG("KMS-DEL", MSG, MOD_MFPLIVE, "Deleting KMS key store ctx");
	kms_client_key_store_t** kms_key_stores = (kms_client_key_store_t**)ctx;
	uint32_t i;
	for (i = 0; i < MAX_FRUIT_STREAMS; i++) {
		kms_key_stores[i]->delete_ctx(kms_key_stores[i]);
	}
	free(kms_key_stores);
}

#if 0
static auth_msg_t *fruit_create_auth_msg(mfp_publ_t *pub)
{
    auth_msg_t *am = NULL;
    kms_info_t *kms = NULL;
    const char *kms_addr = "74.62.179.10";
    int kms_port = 12684;

    if (!pub->ms_parm.kms_port ||
	pub->ms_parm.kms_addr[0] == '\0') {
	goto err;
    }

    am = (auth_msg_t *)\
	nkn_calloc_type(1, sizeof(auth_msg_t), mod_auth_msg_t);
    if (!am) {
	goto err;
    }
    kms = (kms_info_t*)\
	nkn_calloc_type(1, sizeof(kms_info_t), mod_kms_info_t);
    if (!kms) {
	goto err;
    }

    /* init kms info */
    strcpy((char *)kms->kmsaddr, pub->ms_parm.kms_addr);
    kms->kmsport = pub->ms_parm.kms_port;
    kms->flag = VERIMATRIX_KEY_CHAIN_CREATE;
    kms->iv_len = pub->ms_parm.iv_len;

    /* init auth message */
    am->magic=AUTH_MGR_MAGIC;
    am->authtype=pub->ms_parm.kms_type;
    am->authdata= (void *)kms;
    
    return am;
 err:
    if (am) free(am);
    if (kms) free(kms);
    return NULL;
}
#endif
