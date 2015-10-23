#include "mfp_live_sess_id.h"


static void deleteLiveIdState(live_id_t*);
static int32_t insertLiveId(live_id_t*, void*, ctxFree_fptr);
static void removeLiveId(live_id_t*, uint32_t);
static void* getLiveIdCtx(live_id_t*, uint32_t);
static void* acqLiveIdCtx(live_id_t*, uint32_t);
static void relLiveIdCtx(live_id_t*, uint32_t);
static void markInactiveLiveIdCtx(live_id_t*, uint32_t); 
static int32_t getLiveIdFlag(live_id_t*, uint32_t); 


live_id_t* newLiveIdState(uint32_t max_sess) {

	if (max_sess == 0)
		return NULL;
	live_id_t* live_state_cont = mfp_live_malloc(sizeof(live_id_t));
	if (!live_state_cont)
		return NULL;

	live_state_cont->max_sess = max_sess;
	live_state_cont->last_used_idx = -1;
	live_state_cont->flag = NULL;
	live_state_cont->data = NULL;
	live_state_cont->ctx_free = NULL;
	live_state_cont->live_state = NULL;

	live_state_cont->flag = mfp_live_calloc(max_sess, sizeof(int32_t));
	if (!live_state_cont->flag)
		goto exit_clean;
	live_state_cont->data = mfp_live_calloc(max_sess, sizeof(void*));
	if (!live_state_cont->data)
		goto exit_clean;

	live_state_cont->live_state = 
		mfp_live_calloc(max_sess, sizeof(pthread_mutex_t*));
	if (!live_state_cont->live_state)
		goto exit_clean;
	live_state_cont->live_state[0] = 
		mfp_live_calloc(max_sess, sizeof(pthread_mutex_t));
	if (!live_state_cont->live_state[0])
		goto exit_clean;
	live_state_cont->ctx_free = 
		mfp_live_calloc(max_sess, sizeof(ctxFree_fptr));
	if (!live_state_cont->ctx_free)
		goto exit_clean;

	live_state_cont->flag[0] = -1;
	live_state_cont->data[0] = NULL;
	live_state_cont->ctx_free[0] = NULL;
	pthread_mutex_init(live_state_cont->live_state[0], NULL);
	uint32_t i = 1;
	for (; i < max_sess; i++) {

		live_state_cont->flag[i] = -1;
		live_state_cont->data[i] = NULL;
		live_state_cont->ctx_free[i] = NULL;
		live_state_cont->live_state[i] = 
		    live_state_cont->live_state[i-1] + 1; 
		pthread_mutex_init(live_state_cont->live_state[i], NULL);
	}

	live_state_cont->delete_live_id = deleteLiveIdState;
	live_state_cont->insert = insertLiveId;
	live_state_cont->remove = removeLiveId;
	live_state_cont->get_ctx = getLiveIdCtx;
	live_state_cont->acquire_ctx = acqLiveIdCtx;
	live_state_cont->mark_inactive_ctx = markInactiveLiveIdCtx;
	live_state_cont->get_flag = getLiveIdFlag;
	live_state_cont->release_ctx = relLiveIdCtx;
	return live_state_cont;

exit_clean:
	deleteLiveIdState(live_state_cont);
	return NULL;
}


static void deleteLiveIdState(live_id_t* live_state_cont) {


	if (live_state_cont->data != NULL) {
	   uint32_t i = 0;
	   for (; i < live_state_cont->max_sess; i++)
		if (live_state_cont->flag[i] != -1)
		    if (live_state_cont->ctx_free[i])
			live_state_cont->ctx_free[i](live_state_cont->data[i]);
		free(live_state_cont->data);
	}

	if (live_state_cont->flag != NULL)
		free(live_state_cont->flag);

	if (live_state_cont->ctx_free != NULL)
		free(live_state_cont->ctx_free);

	if (live_state_cont->live_state != NULL) {
		if (live_state_cont->live_state[0] != NULL)
			free(live_state_cont->live_state[0]);
		free(live_state_cont->live_state);
	}

	free(live_state_cont);
}


static int32_t insertLiveId(live_id_t* live_state_cont, 
		void* ctx, ctxFree_fptr ctx_free_hdlr) {

	int32_t i = live_state_cont->max_sess;
	uint32_t idx = (live_state_cont->last_used_idx + 1) % 
		live_state_cont->max_sess;
	while (i > 0) {

		if (pthread_mutex_trylock(
			live_state_cont->live_state[idx]) == 0) {
			if (live_state_cont->flag[idx] == -1) {
				live_state_cont->flag[idx] = 1;
				live_state_cont->data[idx] = ctx;
				live_state_cont->ctx_free[idx] = ctx_free_hdlr;
				live_state_cont->last_used_idx = idx;
				pthread_mutex_unlock(
					live_state_cont->live_state[idx]);
				return idx;
			}
			pthread_mutex_unlock(live_state_cont->live_state[idx]);
		}
		i--;
		idx++;
		idx = idx % live_state_cont->max_sess;
	}
	return -1;
}


static void* getLiveIdCtx(live_id_t* live_state_cont, uint32_t idx) {

	return live_state_cont->data[idx];
}


static void* acqLiveIdCtx(live_id_t* live_state_cont, uint32_t idx) {

	pthread_mutex_lock(live_state_cont->live_state[idx]);
	return live_state_cont->data[idx];
}


static void relLiveIdCtx(live_id_t* live_state_cont, uint32_t idx) {

	pthread_mutex_unlock(live_state_cont->live_state[idx]);
}


static void removeLiveId(live_id_t* live_state_cont, uint32_t idx) {

	live_state_cont->flag[idx] = -1;
	if (live_state_cont->ctx_free[idx])
		live_state_cont->ctx_free[idx](live_state_cont->data[idx]);
	live_state_cont->ctx_free[idx] = NULL;
	live_state_cont->data[idx] = NULL;
}


static int32_t getLiveIdFlag(live_id_t* live_state_cont, uint32_t idx) {

	return live_state_cont->flag[idx];
}


// This func should be called only between acqLiveIdCtx and relLiveIdCtx 
static void markInactiveLiveIdCtx(live_id_t* live_state_cont, uint32_t idx) {

	live_state_cont->flag[idx] = -1;
}
 

