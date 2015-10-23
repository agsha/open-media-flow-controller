#ifndef MFP_KMS_CLIENT_H
#define MFP_KMS_CLIENT_H

#include <stdint.h>
#include <netinet/in.h>
#include <stdio.h>

#include "mfp_kms_lib_cmn.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum {

		KMS_VOD,
		KMS_DTV
	} kms_sess_type_t;

	typedef union {

		uint32_t pos;
		uint64_t time;
	} kms_sess_pos_t;

	typedef enum {

		LOCAL,
		USE_KMS
	} kms_key_delivery_t;

	typedef union {

		struct sockaddr_in addr_ctx;
		uint8_t* file_name;
	} kms_key_src_t;

	typedef union {

		int32_t sd;
		FILE* fd;
	} kms_conn_ctx_t;

	struct kms_client;

	typedef kms_key_resp_t* (*kms_get_next_key_fptr)(struct kms_client*);

	typedef void (*kms_client_delete_fptr)(struct kms_client*);


	typedef struct kms_client {

		uint32_t channel_id;
		uint32_t key_chain_length;
		uint8_t init_flag;
		kms_sess_type_t sess_type;
		kms_sess_pos_t sess_pos;
		kms_key_src_t key_src;
		kms_conn_ctx_t conn_ctx;
		kms_key_delivery_t key_delivery_type;
		uint8_t* key_store_loc;
		uint8_t* key_delivery_base_uri;
        int32_t errcode;

	    /* all clients should implement the following */
		kms_get_next_key_fptr get_next_key;
		kms_client_delete_fptr delete_client;

	} kms_client_t;


	kms_client_t* createVerimatrixKMSClient(char const* kms_addr,
			uint16_t kms_port, uint32_t channel_id, uint32_t key_chain_length,
			kms_sess_type_t sess_type, uint8_t const* key_store_loc,
			uint8_t const* key_delivery_base_uri);


	kms_client_t* createNativeKMSClient(uint32_t channel_id,
			uint32_t key_chain_length, uint8_t const* key_storage_loc,
			uint8_t const* key_delivery_uri);

#ifdef __cplusplus
}
#endif

#endif

