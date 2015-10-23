#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <pthread.h>

// cprop defs
#include "cprops/collection.h"
#include "cprops/trie.h"

// nkn defs
#include "nkn_memalloc.h"
#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nkncnt_client.h"

static int32_t nkncnt_client_build_cache(nkncnt_client_ctx_t *nc);
static void *trie_copy_func(void *nd);
static void trie_destruct_func(void *nd);

int32_t
nkncnt_client_create(const char *shm, uint64_t max_cnts,
		     nkncnt_client_ctx_t **out)
{
    int32_t err = 0;
    nkncnt_client_ctx_t *nc = NULL;

    assert(shm);
    nc = (nkncnt_client_ctx_t *)
	nkn_calloc_type(1, sizeof(nkncnt_client_ctx_t),
			100);
    if (!nc) {
	err = -E_NKNCNT_CLIENT_NOMEM;
	goto error;
    }

    nc->shm = shm;
    nc->max_cnts = max_cnts;
    pthread_mutex_init(&nc->cache_lock, NULL);

    pthread_mutex_lock(&nc->cache_lock);
    err = nkncnt_client_build_cache(nc);
    if (err) {
	pthread_mutex_unlock(&nc->cache_lock);
	goto error;
    }
    pthread_mutex_unlock(&nc->cache_lock);
    
    *out = nc;
    return err;

 error:
    if (nc) free(nc);
    return err;
    
}

int32_t 
nkncnt_client_init(const char *shm, uint64_t max_cnts,
		   nkncnt_client_ctx_t *const out)
{
    int32_t err = 0;
    nkncnt_client_ctx_t *nc = NULL;

    assert(shm);
    nc = out;

    nc->shm = shm;
    nc->max_cnts = max_cnts;
    pthread_mutex_init(&nc->cache_lock, NULL);

    pthread_mutex_lock(&nc->cache_lock);
    err = nkncnt_client_build_cache(nc);
    if (err) {
	pthread_mutex_unlock(&nc->cache_lock);
	goto error;
    }
    pthread_mutex_unlock(&nc->cache_lock);
    
 error:
    return err;

}

int32_t 
nkncnt_client_name_to_index(nkncnt_client_ctx_t *nc,
			    const char *name,
			    uint64_t *index1)
{
    int32_t err = 0;
    nkn_shmdef_t *pshmdef = (nkn_shmdef_t*) nc->shm;
    glob_item_t *g_allcnts = 
	(glob_item_t*)(nc->shm + sizeof(nkn_shmdef_t));
    glob_item_t *item = NULL;
    
    if (pshmdef->revision != (int32_t)nc->shm_curr_rev) {
	pthread_mutex_lock(&nc->cache_lock);
	err = nkncnt_client_build_cache(nc);
	if (err) {
	    pthread_mutex_unlock(&nc->cache_lock);
	    goto error;
	}
	pthread_mutex_unlock(&nc->cache_lock);
    }
    item = (glob_item_t *)cp_trie_exact_match(nc->cache, (char *)name);
    if (item) {
	*index1 = (((uint64_t)item - (uint64_t)g_allcnts)/sizeof(glob_item_t));
	err = 0;
    } else {
    	*index1 = -1;
    	err = 0; // Not found
    }

 error:
    return err;
}

int32_t
nkncnt_client_get_submatch(nkncnt_client_ctx_t *nc,
			   const char *name,
			   cp_vector **out)
{
    int32_t err = 0;
    nkn_shmdef_t *pshmdef = (nkn_shmdef_t*) nc->shm;
    cp_vector *res = NULL;
    
    if (pshmdef->revision != nc->shm_curr_rev) {
	pthread_mutex_lock(&nc->cache_lock);
	err = nkncnt_client_build_cache(nc);
	if (err) {
	    pthread_mutex_unlock(&nc->cache_lock);
	    goto error;
	}
	pthread_mutex_unlock(&nc->cache_lock);
    }

    res = cp_trie_submatch(nc->cache, (char*) name);
    if (!res) {
	err = E_NKNCNT_CLIENT_SEARCH_FAIL;
	goto error;
    }

    *out = res;
 error:
    return err;

}

int32_t
nkncnt_client_get_exact_match(nkncnt_client_ctx_t *nc,
			      const char *name,
			      glob_item_t **out)
{
    int32_t err = 0;

#if 0
    nkn_shmdef_t *pshmdef = (nkn_shmdef_t*) nc->shm;
    

    if (pshmdef->revision != nc->shm_curr_rev) {
	pthread_mutex_lock(&nc->cache_lock);
	err = nkncnt_client_build_cache(nc);
	if (err) {
	    pthread_mutex_unlock(&nc->cache_lock);
	    goto error;
	}
	pthread_mutex_unlock(&nc->cache_lock);
    }
#endif

    *out = (glob_item_t*)cp_trie_exact_match(nc->cache, (char*) name);
    if (*out) {
	err = E_NKNCNT_CLIENT_SEARCH_FAIL;
	goto error;
    }

 error:
    return err;

}

static int32_t
nkncnt_client_build_cache(nkncnt_client_ctx_t *nc)
{
    /* build trie */
    nkn_shmdef_t *pshmdef = (nkn_shmdef_t*) nc->shm;
    uint64_t tmp = 0;
    int32_t err = 0, end = 0, start = 0, i;
    glob_item_t *g_allcnts = 
	(glob_item_t*)(nc->shm + sizeof(nkn_shmdef_t));
    char *varname = (char *)(nc->shm +  sizeof(nkn_shmdef_t) + 
			   sizeof(glob_item_t) * nc->max_cnts);
    
    cp_trie_destroy(nc->cache);
    nc->cache = NULL;
    nc->cache = cp_trie_create_trie(
		(COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|
		 COLLECTION_MODE_DEEP),
		trie_copy_func, trie_destruct_func);
    if (!nc->cache) {
	err = -E_NKNCNT_CLIENT_TRIE_INIT;
	goto error;
    }

    end = pshmdef->tot_cnts;
    tmp = pshmdef->revision;
    for (i = start; i < end; i++) {
	if (!g_allcnts[i].size) {
	    continue;
	}
	//printf("adding to cache %s\n",  varname + g_allcnts[i].name);
	err = cp_trie_add(nc->cache, varname + g_allcnts[i].name,
			  &g_allcnts[i]);
	if (err) {
	    err = -E_NKNCNT_CLIENT_TRIE_ADD;
	    goto error;
	}
    }

    nc->shm_curr_rev = tmp;

 error:
    return err;
}

static void *
trie_copy_func(void *nd)
{
    UNUSED_ARGUMENT(nd);
    return nd;
}

static void
trie_destruct_func(void *nd)
{
    UNUSED_ARGUMENT(nd);
    return;
}
