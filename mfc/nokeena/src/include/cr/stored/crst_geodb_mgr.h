/**
 * @file   crst_geodb_mgr.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Tue Apr 10 17:32:57 2012
 * 
 * @brief  defines the obj_store_t interface for geodb
 * 
 * 
 */
#ifndef _CRST_GEODB_MGR_
#define _CRST_GEODB_MGR_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <cprops/hashtable.h>
#include <sys/queue.h>

// nkn includes
#include "nkn_memalloc.h"
#include "pe_geodb.h"
#include "cr_common_intf.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif

struct tag_geodb_ctx;
typedef int32_t (*geodb_ctx_delete_fnp)(struct tag_geodb_ctx *);

typedef struct tag_geodb_result_cache_elm {
    geo_ip_t res;
    TAILQ_ENTRY(tag_geodb_result_cache_elm) entries;
} geodb_result_cache_elm_t;
TAILQ_HEAD(tag_geodb_result_cache_list, tag_geodb_result_cache_elm);
typedef struct tag_geodb_result_cache_list geodb_result_cache_list_t;

typedef struct tag_geodb_ctx {
    pthread_mutex_t lock;
    uint32_t init_flags;
    char db_file_name[256];
    cp_hashtable *results_cache;
    geodb_ctx_delete_fnp delete;
    uint32_t tot_cache_entries;
    geodb_result_cache_list_t lru_list;
    uint64_t num_cache_hits;
    uint64_t tot_hits;
    uint64_t num_cache_entries;
    void *dbg_last_evicted_entry;
} geodb_ctx_t;

/** 
 * @brief creates a fully instantiated store object
 * 
 * @param out [out] - fully instantiated store object
 * 
 * @return 0 on success and <0 on error 
 */
int32_t crst_geodb_store_create(obj_store_t **out);

/** 
 * creates a geodb access context which contains, initialization flags
 * for the underlying geodb and a results cache to optimize lookups
 *
 * @param tot_cache_entries [in] - size of the results cache
 * @param out [out] - fully instantiated object of type geodb_ctx_t
 * 
 * @return 0 on success and <0 on error
 */
int32_t crst_geodb_db_ctx_create(uint32_t tot_cache_entries, 
				 geodb_ctx_t **out);
#ifdef __cplusplus
}
#endif

#endif //_CRST_GEODB_MGR_
