#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// nkn defs
#include "nkn_memalloc.h"
#include "nkn_defs.h"
#include "nkn_stat.h"

// lf defs
#include "lf_ref_token.h"
#include "lf_err.h"

int32_t 
lf_ref_token_init(lf_ref_token_t *rt,
		  uint32_t num_token)
{
    int32_t err = 0;
    uint32_t i;

    assert(rt);
    
    rt->num_token = num_token;
    rt->ref_cnt_token = (int32_t*)
	nkn_malloc_type(num_token * sizeof(lf_ref_token_t), 100);
    if (!rt->ref_cnt_token) {
	err = -E_LF_NO_MEM;
	goto error;
    }
    for (i = 0; i < rt->num_token; i++) {
	rt->ref_cnt_token[i] = LF_REF_TOKEN_UNAVAIL;
    }
    pthread_mutex_init(&rt->lock, NULL);

    return err;

 error:
    if (rt->ref_cnt_token) free(rt->ref_cnt_token);
    return err;
}

int32_t 
lf_ref_token_deinit(lf_ref_token_t *rt)
{
    if (rt) {
	if(rt->ref_cnt_token) 
	    free(rt->ref_cnt_token);
    }
    
    return 0;
}


int32_t
lf_ref_token_get_free(lf_ref_token_t *rt)
{
    int32_t rv = 0, found = 0;
    uint32_t i;

    assert(rt);

    for (i = 0; i < rt->num_token; i++) {
	if (!rt->ref_cnt_token[i] ||
	    rt->ref_cnt_token[i] == LF_REF_TOKEN_UNAVAIL) {
	    rt->ref_cnt_token[i] = 0;
	    found = 1;
	    rv = i;
	    break;
	}
    }

    if (!found)
	rv = -1;

    rt->curr_token = rv;
    
    return rv;
}


