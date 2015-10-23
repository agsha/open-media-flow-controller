#ifndef _LF_REF_TOKEN_
#define _LF_REF_TOKEN_

#include <stdio.h>
#include <pthread.h>
#include <stdint.h>

#ifdef __cpluplus
extern "C" {
#endif

#if 0 //keps emacs happy
}
#endif

#define LF_REF_TOKEN_UNAVAIL (0x7fffffff)

typedef struct tag_lf_ref_token {
    pthread_mutex_t lock;
    int32_t *ref_cnt_token;
    uint32_t num_token;
    uint32_t curr_token;
} lf_ref_token_t;

int32_t lf_ref_token_init(lf_ref_token_t *rt,
			  uint32_t num_token);
int32_t lf_ref_token_get_free(lf_ref_token_t *rt);

int32_t lf_ref_token_deinit(lf_ref_token_t *rt);

static inline int32_t
lf_ref_token_hold(lf_ref_token_t *rt)
{
    assert(rt);
    pthread_mutex_lock(&rt->lock);
    return 0;
}

static inline int32_t
lf_ref_token_release(lf_ref_token_t *rt)
{
    assert(rt);
    pthread_mutex_unlock(&rt->lock);
    return 0;
}

static inline int32_t
lf_ref_token_get_curr_token(lf_ref_token_t *rt)
{
    assert(rt);
    return rt->curr_token;
}

static inline int32_t
lf_ref_token_add_ref(lf_ref_token_t *rt, 
		     uint32_t token)
{
    assert(rt);
    rt->ref_cnt_token[token]++;
    return 0;
}
    
static inline int32_t
lf_ref_token_unref(lf_ref_token_t *rt, 
		   uint32_t token)
{
    assert(rt);
    rt->ref_cnt_token[token]--;
    assert(rt->ref_cnt_token[token] >= 0);
    return 0;
}

#ifdef _cplusplus
}
#endif

#endif //_LF_REF_TOKEN_
