#ifndef NKN_VPE_MFU2MOOF_H
#define NKN_VPE_MFU2MOOF_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <assert.h>
#include <sys/time.h>

#include "nkn_vpe_mfu_defs.h"
#include "nkn_vpe_mfu_parse.h"
#include "nkn_debug.h"

#ifndef SL_MFU2MOOF_NORMAL_ALLOC
#include "nkn_memalloc.h"
#endif

#include "nkn_vpe_types.h"

#define SL_MOOF_TIMESCALE 10000000UL


#ifdef __cplusplus
extern "C" {
#endif

	struct sl_live_uuid1;
	struct sl_live_uuid2;

	struct mfu2moof_ret;
	typedef void (*delete_mfu2moof_fptr)(struct mfu2moof_ret*);

	// **** MFU 2 MOOF Conversion utility and their config and return state
	
	typedef struct mfu2moof_ret {

		uint16_t track_id;
		uint8_t media_type;
		char mdat_type[4];
		uint32_t bit_rate;
		uint32_t chunk_interval;
		uint32_t exact_duration;

		uint8_t* moof_ptr;
		uint32_t moof_len;

		uint8_t* codec_specific_data;
		uint32_t codec_info_size;

		delete_mfu2moof_fptr delete_ctx;

	} mfu2moof_ret_t;

	mfu2moof_ret_t** util_mfu2moof(uint32_t seq_num,
			struct sl_live_uuid1**, struct sl_live_uuid2**,
			mfu_contnr_t const* mfuc);

	// cleans an array of NULL terminted mfu2moof_ret_t pointers
	void destroyMfu2MoofRet(mfu2moof_ret_t**);

	int32_t getRawDescCount(char const* rwfg_data);

	int32_t updateUUID(mfu2moof_ret_t* ret, uint32_t duration);


	// Silverlight Live specific UUID definitions 
	typedef struct sl_live_uuid_common {

		uint8_t uuid_type[16];
		uint32_t version:8;
		uint32_t flags:24;
	} sl_live_uuid_common_t;

	
	struct sl_live_uuid1;
	typedef void (*delete_sl_live_uuid1_fptr)(struct sl_live_uuid1*);
	typedef struct sl_live_uuid1 {

		sl_live_uuid_common_t uuid_cmn;
		uint64_t timestamp;
		uint64_t duration;

		delete_sl_live_uuid1_fptr delete_sl_live_uuid1;
	} sl_live_uuid1_t;

	sl_live_uuid1_t* createSlLiveUuid1(void);


	struct sl_live_uuid2;
	typedef void (*delete_sl_live_uuid2_fptr)(struct sl_live_uuid2*);
	typedef struct sl_live_uuid2 {

		sl_live_uuid_common_t uuid_cmn;
		uint8_t count;
		uint64_t *timestamp;
		uint64_t *duration;

		delete_sl_live_uuid2_fptr delete_sl_live_uuid2;
	} sl_live_uuid2_t;

	sl_live_uuid2_t* createSlLiveUuid2(uint8_t count);


#ifdef __cplusplus
}
#endif

#endif

