#ifndef _MFP_KMS_LIB_CMN_H
#define _MFP_KMS_LIB_CMN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct kms_key_resp {

		uint8_t* key;
   	        uint32_t seq_num;
		uint32_t key_len;
		uint8_t* key_uri;
		uint8_t is_new;
	} kms_key_resp_t;

	typedef void* (*kms_malloc_fptr)(uint32_t size);

	typedef void* (*kms_calloc_fptr)(uint32_t count, uint32_t size);

	void initKMS_Lib(kms_malloc_fptr, kms_calloc_fptr);

	kms_key_resp_t* createKmsKeyResponse(uint8_t const* key, uint32_t key_len,
					     uint8_t const* key_uri,
					     uint8_t is_new, uint32_t seq_num);

	void deleteKmsKeyResponse(kms_key_resp_t* k_resp);

	extern kms_malloc_fptr kms_malloc;
	extern kms_calloc_fptr kms_calloc;

#ifdef __cplusplus
}
#endif

#endif

