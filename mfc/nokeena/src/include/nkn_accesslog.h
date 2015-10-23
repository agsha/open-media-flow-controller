/* File : nkn_accesslog.h
 * Copyright (C) 2014 Juniper Networks, Inc. 
 * All rights reserved.
 */

#ifndef __NKN_ACCESSLOG__H__
#define __NKN_ACCESSLOG__H__

#include <pthread.h>
#include "nkn_hash.h"

typedef struct accesslog_stat {
    uint64_t	    tot_logged;
    uint64_t	    err_buf_overflow;
    uint64_t	    tot_dropped;
} al_stat_t;

typedef struct al_info_s {
    char *name;
    void *key;
    uint64_t keyhash;
    pthread_mutex_t	lock;
    int al_fd;
    int in_use;
    int32_t channelid;
    int32_t num_al_list;
    char *al_list[32];
    short al_token[32];
    short al_listlen[32];
    int32_t al_resp_header_configured;
    int32_t al_record_cache_history;
    char *al_buf;
    uint32_t al_buf_start;
    uint32_t al_buf_end;
    al_stat_t	    stats;
} al_info_t;

extern NHashTable *al_profile_tbl;
extern pthread_rwlock_t alp_tbl_lock;
extern uint64_t glob_al_tot_logged;
extern uint64_t glob_al_err_buf_overflow;

#define MAX_ACCESSLOG_BUFSIZE	(4*MiBYTES)

#define ALP_HASH_WLOCK()	\
    do { \
	pthread_rwlock_wrlock(&alp_tbl_lock); \
    } while(0)

#define ALP_HASH_WUNLOCK()	\
    do { \
	pthread_rwlock_unlock(&alp_tbl_lock); \
    } while(0)

#define ALP_HASH_RLOCK()	\
    do { \
	pthread_rwlock_rdlock(&alp_tbl_lock); \
    } while (0)

#define ALP_HASH_RUNLOCK()	\
    do { \
	pthread_rwlock_unlock(&alp_tbl_lock); \
    } while (0)

#if 0
#define ALI_HASH_RLOCK()	\
    do { \
	pthread_rwlock_rdlock(&ali->lock); \
    } while (0)

#define ALI_HASH_RUNLOCK()	\
    do { \
	pthread_rwlock_unlock(&ali->lock); \
    } while (0)
#endif

#define ALI_HASH_WLOCK()	\
    do { \
	pthread_mutex_lock(&ali->lock); \
    } while (0)

#define ALI_HASH_WUNLOCK()	\
    do { \
	pthread_mutex_unlock(&ali->lock) ; \
    } while (0)
#endif /* __NKN_ACCESSLOG__H__ */
