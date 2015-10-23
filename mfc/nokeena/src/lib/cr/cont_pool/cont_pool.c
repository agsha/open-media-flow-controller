#include "cont_pool.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <cprops/linked_list.h>
#include <pthread.h>

typedef struct local_ctx {

    uint32_t cont_cnt_perq;
    uint32_t queue_cnt;
    pthread_mutex_t* qlock;
    pthread_cond_t* qcond_woe;
    pthread_cond_t* qcond_wof;
    cp_list** queue;
    uint32_t* qsize;
    uint32_t last_get;
    uint32_t last_put;
    pthread_mutex_t get_lock;
    pthread_mutex_t put_lock;
} local_ctx_t;


static void* getCont(cont_pool_t* cp);

static int32_t putCont(cont_pool_t* cp, void* cont);

static void delete(cont_pool_t* cp);


cont_pool_t* createContainerPool(uint32_t cont_cnt_perq, uint32_t queue_cnt) {

    if ((cont_cnt_perq == 0) || (queue_cnt == 0))
	return NULL;
    cont_pool_t* cp = calloc(1, sizeof(cont_pool_t));
    if (cp == NULL)
	return NULL;
    local_ctx_t* lctx = calloc(1, sizeof(local_ctx_t));
    if (lctx == NULL) {

	free(cp);
	return NULL;
    }
    lctx->cont_cnt_perq = cont_cnt_perq;
    lctx->queue_cnt = queue_cnt;
    lctx->last_get = 0;
    lctx->last_put = 0;
    lctx->qlock = calloc(queue_cnt, sizeof(pthread_mutex_t));
    lctx->qcond_woe = calloc(queue_cnt, sizeof(pthread_cond_t));
    lctx->qcond_wof = calloc(queue_cnt, sizeof(pthread_cond_t));
    lctx->queue = calloc(queue_cnt, sizeof(cp_list*));
    lctx->qsize = calloc(queue_cnt, sizeof(uint32_t));
    uint32_t i = 0;
    for (; i < queue_cnt; i++) {

	pthread_mutex_init(&lctx->qlock[i], NULL);
	lctx->queue[i] = cp_list_create();
	lctx->qsize[i] = 0;
	pthread_cond_init(&lctx->qcond_woe[i], NULL);
	pthread_cond_init(&lctx->qcond_wof[i], NULL);
    }
    pthread_mutex_init(&lctx->get_lock, NULL);
    pthread_mutex_init(&lctx->put_lock, NULL);

    cp->__internal = (void*)lctx;
    cp->get_cont_hdlr = getCont;
    cp->put_cont_hdlr = putCont;
    cp->delete_hdlr = delete;

    return cp;
}


static void delete(cont_pool_t* cp) {

    local_ctx_t* lctx = (local_ctx_t*)cp->__internal;
    uint32_t i = 0;
    for (; i < lctx->queue_cnt; i++) {

	cp_list_destroy(lctx->queue[i]);
	pthread_mutex_destroy(&lctx->qlock[i]);
	pthread_cond_destroy(&lctx->qcond_woe[i]);
	pthread_cond_destroy(&lctx->qcond_wof[i]);
    }
    free(lctx->queue);
    free(lctx->qlock);
    free(lctx->qsize);
    free(lctx->qcond_woe);
    free(lctx->qcond_wof);
    pthread_mutex_destroy(&lctx->get_lock);
    pthread_mutex_destroy(&lctx->put_lock);
    free(lctx);
    free(cp);
}


static void* getCont(cont_pool_t* cp) {

    local_ctx_t* lctx = (local_ctx_t*)cp->__internal;
    pthread_mutex_lock(&lctx->get_lock);
    uint32_t idx = lctx->last_get++;
    pthread_mutex_unlock(&lctx->get_lock);
    idx %= lctx->queue_cnt;
    pthread_mutex_lock(&lctx->qlock[idx]);
    //printf("idx: %u lctx->qsize[idx]: %u\n", idx, lctx->qsize[idx]);
    if (lctx->qsize[idx] == 0) {
	printf("Wait\n");
	pthread_cond_wait(&lctx->qcond_woe[idx], &lctx->qlock[idx]);
    }
    void* cont = cp_list_remove_head(lctx->queue[idx]);
    lctx->qsize[idx]--;
    pthread_cond_signal(&lctx->qcond_wof[idx]);
    pthread_mutex_unlock(&lctx->qlock[idx]);
    return cont;
}


static int32_t putCont(cont_pool_t* cp, void* cont) {

    local_ctx_t* lctx = (local_ctx_t*)cp->__internal;
    pthread_mutex_lock(&lctx->put_lock);
    uint32_t idx = lctx->last_put++;
    pthread_mutex_unlock(&lctx->put_lock);
    idx %= lctx->queue_cnt;
    pthread_mutex_lock(&lctx->qlock[idx]);
    //printf("idx: %u lctx->qsize[idx]: %u\n", idx, lctx->qsize[idx]);
    if (lctx->qsize[idx] == lctx->cont_cnt_perq)
	pthread_cond_wait(&lctx->qcond_wof[idx], &lctx->qlock[idx]);
    cp_list_append(lctx->queue[idx], cont);
    lctx->qsize[idx]++;
    pthread_cond_signal(&lctx->qcond_woe[idx]);
    pthread_mutex_unlock(&lctx->qlock[idx]);
    return 1;
}

