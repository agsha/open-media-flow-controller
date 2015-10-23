#ifndef _NKNCNT_CLIENT_H_
#define _NKNCNT_CLIENT_H_

#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

// cprop defs
#include "cprops/collection.h"
#include "cprops/trie.h"

// nkn defs
#include "nkn_defs.h"
#include "nkn_stat.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 /* keeps emacs happy */
}
#endif

#define E_NKNCNT_CLIENT_NOMEM 1000
#define E_NKNCNT_CLIENT_TRIE_INIT 1001
#define E_NKNCNT_CLIENT_TRIE_ADD 1002
#define E_NKNCNT_CLIENT_SEARCH_FAIL 1003
typedef struct tag_nkncnt_client {
    cp_trie *cache;
    const char *shm;
    uint64_t max_cnts;
    int32_t shm_curr_rev;
    pthread_mutex_t cache_lock;
} nkncnt_client_ctx_t;

int32_t nkncnt_client_create(const char *shm, uint64_t max_cnts,
			     nkncnt_client_ctx_t **out);
int32_t nkncnt_client_init(const char *shm, uint64_t max_cnts,
			   nkncnt_client_ctx_t *const out);
int32_t nkncnt_client_get_submatch(nkncnt_client_ctx_t *nc,
				   const char *name,
				   cp_vector **out);
int32_t nkncnt_client_get_exact_match(nkncnt_client_ctx_t *nc,
				      const char *name,
				      glob_item_t **out);
int32_t nkncnt_client_name_to_index(nkncnt_client_ctx_t *nc,
				    const char *name,
				    uint64_t *index);

#ifdef __cplusplus
}
#endif
#endif //_NKNCNT_CLIENT_H_
