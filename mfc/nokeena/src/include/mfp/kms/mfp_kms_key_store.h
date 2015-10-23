#ifndef MFP_KMS_KEY_STORE_H
#define MFP_KMS_KEY_STORE_H

#include <glib.h>
#include <pthread.h>
#include <stdint.h>

#include "kms/mfp_kms_client.h"
#include "kms/mfp_kms_lib_cmn.h"
#include "mfp_limits.h"

#ifdef __cplusplus
extern "C" {
#endif

	struct kms_client_key_store;

	typedef kms_key_resp_t* (*get_kms_key_fptr)(struct kms_client_key_store*, 
			uint32_t key_seq_num);

	typedef void (*delete_ctx_fptr)(struct kms_client_key_store* key_store);
        typedef void (*del_kms_key_fptr)(struct kms_client_key_store *,
					 uint32_t key_seq_num);
    
        typedef struct tag_kms_client_key_store_stats {

	        char instance_name[MAX_PUB_NAME_LEN];

	        char *kms_num_keys_kms_name;
	        uint32_t kms_num_keys_kms;
	    
  	        char *kms_num_keys_store_name;
	        uint32_t kms_num_keys_store;
	} kms_client_key_store_stats_t;

	typedef struct kms_client_key_store {

		kms_client_t* kms_client;
		GTree* key_store;
		kms_key_resp_t* k_resp_rtz;
		uint32_t key_rot_count;
                uint32_t dbg_key_count;
		pthread_mutex_t lock;

		get_kms_key_fptr get_kms_key;
		delete_ctx_fptr delete_ctx;
    	        del_kms_key_fptr del_kms_key;

	        kms_client_key_store_stats_t stats;

	} kms_client_key_store_t;

	kms_client_key_store_t* createKMS_ClientKeyStore(
		 kms_client_t* kms_client, uint32_t key_rot_count,
		 const char *sess_name, uint32_t sess_id);

#ifdef __cpluscplus
}
#endif

#endif
