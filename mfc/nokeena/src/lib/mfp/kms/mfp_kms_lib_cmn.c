#include "kms/mfp_kms_lib_cmn.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


static void* kmsMalloc(uint32_t size);

static void* kmsCalloc(uint32_t count, uint32_t size);

kms_malloc_fptr kms_malloc = kmsMalloc;
kms_calloc_fptr kms_calloc = kmsCalloc;

int32_t glob_kms_vmx_key_uri_type = 0;

static void* kmsMalloc(uint32_t size) {

	return malloc(size);
}

static void* kmsCalloc(uint32_t count, uint32_t size) {

	return calloc(count, size);
}

void initKMS_Lib(kms_malloc_fptr kms_malloc_hdlr,
		kms_calloc_fptr kms_calloc_hdlr) {

	if (kms_malloc_hdlr != NULL)
		kms_malloc = kms_malloc_hdlr;
	if (kms_calloc_hdlr != NULL)
		kms_calloc = kms_calloc_hdlr;
}

kms_key_resp_t* createKmsKeyResponse(uint8_t const* key, uint32_t key_len,
				     uint8_t const* key_uri, 
				     uint8_t is_new, 
				     uint32_t seq_num) {

	kms_key_resp_t* dup_k_resp = kms_calloc(1, sizeof(kms_key_resp_t));
	if (dup_k_resp == NULL)
		return NULL;

	dup_k_resp->key = kms_calloc(key_len, 1);
	if (dup_k_resp->key == NULL) {
		free(dup_k_resp);
		return NULL;
	}
	memcpy(dup_k_resp->key, key, key_len);
	dup_k_resp->key_len = key_len;
	dup_k_resp->key_uri = kms_calloc(strlen((char*)key_uri) + 1, 1);
	if (dup_k_resp->key_uri == NULL) {
		free(dup_k_resp->key);
		free(dup_k_resp);
		return NULL;
	}
	strncat((char*)dup_k_resp->key_uri, (char*)key_uri,
			strlen((char*)key_uri));
	dup_k_resp->is_new = is_new;
	dup_k_resp->seq_num = seq_num;
	return dup_k_resp;
}


void deleteKmsKeyResponse(kms_key_resp_t* k_resp) {

	if (k_resp->key != NULL)
		free(k_resp->key);
	if (k_resp->key_uri != NULL)
		free(k_resp->key_uri);
	free(k_resp);
}

