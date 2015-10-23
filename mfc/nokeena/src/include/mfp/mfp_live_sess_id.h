#ifndef MFP_LIVE_SESS_ID_H
#define MFP_LIVE_SESS_ID_H

#include <stdint.h>
#include <pthread.h>
#include "mfp_live_conf.h"

struct live_id;
typedef void (*ctxFree_fptr)(void*);

typedef void (*deleteLiveIdState_fptr)(struct live_id*);

typedef int32_t (*insertLiveId_fptr)(struct live_id*, void*, ctxFree_fptr);
typedef void (*removeLiveId_fptr)(struct live_id*, uint32_t);

typedef void* (*acqLiveIdCtx_fptr)(struct live_id*, uint32_t);
typedef void (*relLiveIdCtx_fptr)(struct live_id*, uint32_t);
typedef void (*markInactiveLiveIdCtx_fptr)(struct live_id*, uint32_t); 
typedef int32_t (*getLiveIdFlag_fptr)(struct live_id*, uint32_t);

typedef struct live_id {

	int8_t* flag;
	void** data;
	pthread_mutex_t** live_state;
	ctxFree_fptr* ctx_free;
	uint32_t max_sess;
	uint32_t last_used_idx;

	deleteLiveIdState_fptr delete_live_id;

	insertLiveId_fptr insert;
	acqLiveIdCtx_fptr get_ctx;

	acqLiveIdCtx_fptr acquire_ctx;
	removeLiveId_fptr remove;
	markInactiveLiveIdCtx_fptr mark_inactive_ctx;
	/*removeLiveIdCtx has to be called between acquire_ctx and release_ctx*/
	getLiveIdFlag_fptr get_flag;
	relLiveIdCtx_fptr release_ctx;

} live_id_t, file_id_t, sess_id_t;


live_id_t* newLiveIdState(uint32_t max_sess);

#endif

