#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "nkn_debug.h"
#include "nkn_assert.h"
#include "kms/mfp_kms_client.h"
#include "kms/mfp_kms_lib_cmn.h"


static void NativeKMS_Delete(kms_client_t* kms_client);

static kms_key_resp_t* NativeKMS_GetNextKey(kms_client_t* kms_client); 

kms_client_t* createNativeKMSClient(uint32_t channel_id, 
		uint32_t key_chain_length,
		uint8_t const* key_store_loc,
		uint8_t const* key_delivery_base_uri) {

	if ((key_store_loc == NULL) || (key_delivery_base_uri == NULL))
		return NULL;

	kms_client_t* kms_client_ctx = kms_calloc(1, sizeof(kms_client_t));
	if (kms_client_ctx == NULL)
		return NULL;

	kms_client_ctx->get_next_key = NativeKMS_GetNextKey;
	kms_client_ctx->delete_client = NativeKMS_Delete;

	kms_client_ctx->channel_id = channel_id;
	kms_client_ctx->key_chain_length = key_chain_length;
	kms_client_ctx->init_flag = 0;
	kms_client_ctx->sess_pos.pos = 0;

	kms_client_ctx->key_delivery_type = LOCAL;
	uint32_t key_store_loc_len = strlen((char*)key_store_loc);
	uint32_t key_delivery_base_uri_len =
		strlen((char*)key_delivery_base_uri);
	kms_client_ctx->key_store_loc = kms_calloc(key_store_loc_len + 1, 1);
	kms_client_ctx->key_delivery_base_uri = kms_calloc(
			key_delivery_base_uri_len + 1, 1);
	if ((kms_client_ctx->key_store_loc == NULL) ||
			(kms_client_ctx->key_delivery_base_uri == NULL)) {
		kms_client_ctx->delete_client(kms_client_ctx);
		return NULL;
	}
	strncpy((char*)kms_client_ctx->key_store_loc, (char*)key_store_loc,
			key_store_loc_len + 1);
	strncpy((char*)kms_client_ctx->key_delivery_base_uri,
			(char*)key_delivery_base_uri,
			key_delivery_base_uri_len + 1);

	kms_client_ctx->conn_ctx.fd = fopen("/dev/urandom", "rb" );
	if (kms_client_ctx->conn_ctx.fd == NULL) {
		perror("file open: kms vmx client: ");
		kms_client_ctx->delete_client(kms_client_ctx);
		return NULL;
	}
	return kms_client_ctx;
}


static kms_key_resp_t* NativeKMS_GetNextKey(kms_client_t* kms_client_ctx) {

	kms_key_resp_t* k_resp = NULL;

	uint64_t kpos = kms_client_ctx->sess_pos.pos %
		kms_client_ctx->key_chain_length;
	kms_client_ctx->sess_pos.pos++;

	uint8_t key_buf[16];
	uint8_t key_delivery_uri[512];
	int32_t rc = fread(&key_buf[0], 1, 16, kms_client_ctx->conn_ctx.fd);
	if (rc < 16)
		return NULL;

	char const* key_apnd_fmt = "%s/key_%u_t_%s_p_%lu";
	uint32_t max_key_uri = strlen(
			(char*)kms_client_ctx->key_delivery_base_uri) + 128;
	if (max_key_uri >= 512)
		assert(0);
	snprintf((char*)key_delivery_uri, 512, key_apnd_fmt,
			kms_client_ctx->key_delivery_base_uri,
			kms_client_ctx->channel_id, "Native", kpos);

	uint8_t key_store_loc[512];
	max_key_uri = strlen((char*)kms_client_ctx->key_store_loc) + 128;
	if (max_key_uri >= 512)
		assert(0);
	snprintf((char*)key_store_loc, 512, key_apnd_fmt,
			kms_client_ctx->key_store_loc,
			kms_client_ctx->channel_id, "Native", kpos);

	FILE* fp = fopen((char*)key_store_loc, "wb");
	if (!fp) {
	    kms_client_ctx->errcode = errno;
	    NKN_ASSERT(0);
	    return NULL;
	}
	fwrite(&key_buf[0], 16, 1, fp);
	fclose(fp);

	k_resp = createKmsKeyResponse(&(key_buf[0]), 16,
				      &key_delivery_uri[0], 1, 0);
	return k_resp;
}


static void NativeKMS_Delete(kms_client_t* kms_client_ctx) {

	if (kms_client_ctx->conn_ctx.fd != NULL)
		fclose(kms_client_ctx->conn_ctx.fd);

	if (kms_client_ctx->key_store_loc != NULL)
		free(kms_client_ctx->key_store_loc);

	if (kms_client_ctx->key_delivery_base_uri != NULL)
		free(kms_client_ctx->key_delivery_base_uri);

	free(kms_client_ctx);
}

