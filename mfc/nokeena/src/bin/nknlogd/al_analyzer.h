#ifndef __AL_ANALYZER_H__
#define __AL_ANALYZER_H__

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>

#define HASH_UNITTEST
#include "nkn_hash.h"

/*
 * Main hash table for storing the data
 * should contain 1000 entries
 */

#define TOT_SECONDS	60
#define TOT_MINUTES	60
#define TOT_HOURS	24

#define USE_MUTEX

#ifdef USE_MUTEX

#define LOCK(t)		do {  pthread_mutex_lock(&((t)->lock)); } while (0)
#define UNLOCK(t)	do {  pthread_mutex_unlock(&((t)->lock)); } while (0)
#define TRY_LOCK(t)	(pthread_mutex_trylock(&((t)->lock)))
#define TRY_LOCK_RET(t)	do { if (pthread_mutex_trylock(&((t)->lock)) != 0) return -1; } while(0)

#else		/* Never use this.. Unsafe. */

#define LOCK(t)		do { ; } while (0)
#define UNLOCK(t)	do { ; } while (0)
#define TRY_LOCK(t)	(0)
#define TRY_LOCK_RET(t)	do { ; } while(0)

#endif		/* ! USE_MUTEX */


#define CONVERT_TO_MBYTES(x)    ((x)/1000000)



/*
 * 0	-	0 - 1023			[0 - 	1KB] 	10
 * 1	-	1024 - 4095			[1KB - 	4K]	12	
 * 2	-	4096 - 8191			[4K - 	8K]	13
 * 3	-	8192 - 16833			[8K - 	16K]	14
 * 4	-	16834 - 32767			[16K - 	32K]	15
 * 5	-	32768 - 65535			[32K - 	64K]	16
 * 6	-	65536 - 131071			[64K - 	128K]	17
 * 7	-	131072 - 262143			[128K - 256K]	18
 * 8	-	262144 - 524287 		[256K - 512K]	19
 * 9	-	524288 - 2097151		[512K - 2M]	21
 * 10	-	2097152 - 8388607 		[2M - 	8M]	23
 * 11	-	8388608 - 33554431 		[8M - 	32M]	25
 * 12	-	33554432 - 134217727		[32M - 	128M]	27
 * 13	-	134217728 - 536870911 		[128M - 512M]	29
 * 14	-	536870912 - 1073741823 		[512M - 1G]	30
 * 15	-	1073741825 >			[1G >]
 */
#define TOT_FILESIZE_GRP	16
typedef struct size_dist {
	int32_t min_size;
	int32_t max_size;
	const char * prompt;
} size_dist_t;

/*
 * Status code
 */
#define STATUS_CODE_GRP	7
typedef struct status_code {
	uint32_t	hits_200;
	uint32_t	hits_206;
	uint32_t	hits_302;
	uint32_t	hits_304;
	uint32_t	hits_404;
	uint32_t	hits_500;
	uint32_t	hits_other;
} status_code_t;

/*
 * Domain and uri list structure
 * We only compare domain and uri up to 40 bytes.
 * we treated all uris (even different beyond 40 bytes) as the same uri.
 */
#define MAX_URI_STRING_SIZE	64
typedef struct uri_list {
	char * uri;
	uint32_t hits;
} uri_list_t;

#define MAX_DOMAIN_STRING_SIZE	64
typedef struct domain_list {
	char * domain;
	uint32_t hits;
	float  cache_bytes;
	float  origin_bytes;
	float  unknown_bytes;
	NHashTable      * p_uri_ht;
} domain_list_t;

/* A generic table */
typedef struct table {
	uint32_t	tot_samples;

	/* File size distribution */
	int32_t		hits_filesize[TOT_FILESIZE_GRP];

	/* Status Code distribution */
	status_code_t	resp_code;

	/* domain hotness distribution */
	NHashTable      * p_domain_ht;
} table_t;

/*
 * cache hit indicator
 */
typedef enum {
	bytes_unknown = 0,
	bytes_cache,
	bytes_origin,
} cache_indicator_t;

// This strcture is sent from logger to alan to update stats. 
// This MUST be allocated in stack only. 
// internal pointers MUST NOT be allocated. they should only 
// bew references to the actual log string.
typedef struct analysis_data {
	char	*key;
	char	*ns;
	int	ns_len;
	char 	*hostname;
	int	hostlen;
	char 	*uri;
	int	urilen;
	int	bytes_out;
	int 	errcode;
	cache_indicator_t       cache_indicator;
} analysis_data_t;


/*
 * HTTP request parsing result.
 */
typedef enum {
    dt_none = 0,
    dt_filesize,
    dt_respcode,
    dt_domain,
    dt_uri,
} data_type_t;

typedef enum {
    pt_none = 0,
    pt_namespace,
    pt_domain,
    pt_uri,
    pt_all,
} profile_type_t;

typedef struct {
    data_type_t type;		// analyze, size

    int thour;			// time period in hour
    int tmin;			// time period in min
    char *domain;
    int depth;			// depth

    int err;
    int argc;
    char * resp;
    int resp_len;
} data_request_t;



extern void free_args(int argc, char ***argv);

extern int alan_process_data_request(data_request_t *dr);

#endif
