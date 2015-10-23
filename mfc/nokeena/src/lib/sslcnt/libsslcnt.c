#include <sys/types.h>
#include <stdint.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <limits.h>
#include "nkn_stat.h"
#include "nkn_common_config.h"

#include "ssl_cntr_api.h"

#define USE_TRIE_LOOKASIDE 1 // Enabled


static inline int check_if_init(void);

#ifdef USE_TRIE_LOOKASIDE
#include "cprops/collection.h"
#include "cprops/trie.h"

pthread_mutex_t sslcnt_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cntcache_lock = PTHREAD_MUTEX_INITIALIZER;

#define DEFINE_GET_COUNT(type) \
type ##_t sslcnt_get_## type (const char *name) { \
        type ##_t temp = 0;                 \
        int i = 0;                      \
        if (shm == NULL) \
                libsslcnt_init();        \
        if (shm == NULL ) return temp; \
	if (!trie_sslcnt_name_to_index(name, &i)) { \
	    if (i >= 0) { \
		temp = g_allcnts[i].value; \
	    } \
	} \
        return temp; \
}

#else /* !USE_TRIE_LOOKASIDE */

#define DEFINE_GET_COUNT(type) \
type ##_t sslcnt_get_## type (const char *name) { \
        type ##_t temp = 0;                 \
        int i = 0;                      \
        if (shm == NULL) \
                libsslcnt_init();        \
        if (shm == NULL ) return temp; \
        for(i = 0; i < pshmdef->tot_cnts; i++) {        \
                if(g_allcnts[i].size && strcmp((const char*) (varname+g_allcnts[i].name), name) == 0) { \
                        temp = g_allcnts[i].value; \
                } \
        } \
        return temp; \
}
#endif /* !USE_TRIE_LOOKASIDE */

struct parameters {
        char sig[64];
        time_t tv;
        int freq;  // in seconds
        int tot_cnts;
        long headoffset;
	int reserved;
} gpar;


static glob_item_t * g_allcnts = NULL;
static nkn_shmdef_t * pshmdef = NULL;
static char * varname = NULL;
static char *shm = NULL;


////////////////////////////////////////////////////////////////////////////////
#ifdef USE_TRIE_LOOKASIDE
////////////////////////////////////////////////////////////////////////////////
#define UNUSED_ARGUMENT(x) (void )x
static nkn_shmdef_t shmdef = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			      0, 0, INT_MAX};
static cp_trie *ssl_cnt_trie;
static cp_trie *cntcache_trie;

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

static int trie_sslcnt_name_to_index(const char *name, int *cindex)
{
    int n;
    int rv;
    int ret;
    nkn_shmdef_t cur_shmdef;
    glob_item_t *item;

    pthread_mutex_lock(&sslcnt_lock);

    cur_shmdef = *pshmdef;
    while (cur_shmdef.revision != shmdef.revision) {
    	if (ssl_cnt_trie) {
	    cp_trie_destroy(ssl_cnt_trie);
	}
	ssl_cnt_trie = cp_trie_create_trie(
				(COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|
		 	 	COLLECTION_MODE_DEEP), 
				trie_copy_func, trie_destruct_func);
	if (!ssl_cnt_trie) {
	    ret = 1;
	    goto exit;
	}

	for (n = 0; n < pshmdef->tot_cnts; n++) {
	    if (!g_allcnts[n].size) {
	    	continue;
	    }
	    rv = cp_trie_add(ssl_cnt_trie,(varname+g_allcnts[n].name), 
			     &g_allcnts[n]);
	    if (rv) {
	    	ret = 2;
	    	goto exit;
	    }
	}
	if (cur_shmdef.revision == pshmdef->revision) {
	    shmdef = cur_shmdef;
	    break;
	}
    	cur_shmdef = *pshmdef;
    }

    item = (glob_item_t *)cp_trie_exact_match(ssl_cnt_trie, (char *)name);
    if (item) {
    	*cindex = (((uint64_t)item - (uint64_t)g_allcnts)/sizeof(glob_item_t));
	ret = 0;
    } else {
    	*cindex = -1;
    	ret = 0; // Not found
    }

exit:

    pthread_mutex_unlock(&sslcnt_lock);
    return ret;
}

static int trie_invalidate_cntcache(void)
{
    pthread_mutex_lock(&cntcache_lock);
    if (cntcache_trie) {
	cp_trie_destroy(cntcache_trie);
	cntcache_trie = NULL;
    }
    pthread_mutex_unlock(&cntcache_lock);
    return 0;
}

static int trie_add_cntcache(const char *name, void *data)
{
    int rv;

    pthread_mutex_lock(&cntcache_lock);
    if (!cntcache_trie) {
    	cntcache_trie = cp_trie_create_trie(
			    (COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|
			    COLLECTION_MODE_DEEP), 
			    trie_copy_func, trie_destruct_func);
	if (!cntcache_trie) {
	    pthread_mutex_unlock(&cntcache_lock);
	    return 1;
	}
    }
    rv = cp_trie_add(cntcache_trie, (char *)name, data);
    pthread_mutex_unlock(&cntcache_lock);
    if (rv) {
    	return 2;
    }
    return 0;
}


static int trie_cntcache_name_to_data(const char *name, void **data)
{
    int ret;

    pthread_mutex_lock(&cntcache_lock);
    if (cntcache_trie) {
    	*data = (void *)cp_trie_exact_match(cntcache_trie, (char *)name);
    	if (*data) {
	    ret = 0;
    	} else {
	    ret = 1; // Not found
    	}
    } else {
	ret = 1; // Not found
    }
    pthread_mutex_unlock(&cntcache_lock);
    return ret;
}
////////////////////////////////////////////////////////////////////////////////
#endif /* USE_TRIE_LOOKASIDE */
////////////////////////////////////////////////////////////////////////////////

DEFINE_GET_COUNT( uint32 );
DEFINE_GET_COUNT( int32 );
DEFINE_GET_COUNT( int16 );
DEFINE_GET_COUNT( uint16 );
DEFINE_GET_COUNT( int64 );
DEFINE_GET_COUNT( uint64 );

static int ssl_refresh_needed = 1;
int
sslcnt_get_counter_index(const char *name, int dynamic, int *cindex)
{
    int i = 0, found = 0, start = 0, end = 0;

    if (shm == NULL) {
	libsslcnt_init();
    }
    if (shm == NULL) {
	/* TODO - return proper  tatus code */
	return 1;
    }

#ifdef USE_TRIE_LOOKASIDE
    if (trie_sslcnt_name_to_index(name, cindex)) {
    	*cindex = -1;
    }
    return 0;
#endif

    if (dynamic) {
	start = pshmdef->static_cnts;
	end = pshmdef->tot_cnts;
    } else {
	start = 0;
	end = pshmdef->static_cnts;
    }
    for (i = start; i < end; i++) {
	if (0 == g_allcnts[i].size) {
	    continue;
	}
	if (0 == strcmp((const char *)(varname+g_allcnts[i].name), name)) {
	    *cindex = i;
	    found = 1;
	    break;
	}
    }
    if (found) {
	return 0;
    } else {
	*cindex = -1;
    }
    return 0;
}
int
sslcnt_get_counter_byindex(const char *name, int cindex, uint64_t *value)
{
    int retval = 0;
    if (shm == NULL) {
	return EINVAL;
    }
    if (name == NULL || cindex == -1) {
	return EINVAL;
    }
    /* avoid race conditions if any */
    if (0 == g_allcnts[cindex].size) {
	return ENOENT;
    }
    if (0 == strcmp((const char *)(varname+g_allcnts[cindex].name), name)) {
	*value = g_allcnts[cindex].value;
	retval = 0;
    } else {
	*value = 0;
	retval = ENOENT;
    }
    return retval;
}

static inline int check_if_init(void)
{
        return (NULL == shm) ? NKN_FALSE : NKN_TRUE;
}

/*
 * Always first to be called !
 * Attach to shared memory, grab the contents and build our
 * own "database"
 */
int 
libsslcnt_init(void)
{
        int err = 0;
	int shmid;
	uint64_t NKN_SHMSZ;

        if (NULL != shm) {
                /* Already initialized */
                return 0;
        }

	NKN_SHMSZ = (sizeof(nkn_shmdef_t)+MAX_CNTS_SSLD*(sizeof(glob_item_t)+30));
        if ((shmid = shmget(NKN_SSL_SHMKEY, NKN_SHMSZ, 0666)) < 0) {
#if 0
		fprintf(stderr, 
                        "Failed to do shmget."
                        "nvsd may not running on this machine. : %s",
                        strerror(errno));
#endif
                goto bail;
        }

	/*
	 * Now we attach the segment to our data space.
	 */
	if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
#if 0
		fprintf(stderr, 
                        "Failed to do shmat. "
                        "nvsd may not running on this machine. : %s",
                        strerror(errno));
#endif
                goto bail;
	}

	pshmdef = (nkn_shmdef_t *) shm;
        g_allcnts = (glob_item_t *)(shm + sizeof(nkn_shmdef_t));
        varname = (char *)(shm + 
                        sizeof(nkn_shmdef_t) + 
                        sizeof(glob_item_t) * MAX_CNTS_SSLD);
       
        /* This is a version check to see if the counters exposed by the 
         * NVSD and this module are built with the same definitions. 
         */
	if(strcmp(pshmdef->version, NKN_VERSION)!=0) {
                /* Log an error, close the shmem and return error */
#if 0
		fprintf(stderr,
                        "Version does not match. "
                        "It requires %s but this module version is %s\n", 
                        pshmdef->version, NKN_VERSION);

#endif
                libsslcnt_close();
                goto bail;
	}

        return err;
bail:
        shm = NULL;
        return -1;
}

int libsslcnt_close(void)
{
        if (shm) {
                int err  = 0;
                err = shmdt(shm);
                shm = NULL;
                return err;
        }

        return 0;
}

int sslcnt_cache_refresh(void)
{
    ssl_refresh_needed = 1;
    return 0;
}
#define MAX_COUNTER_CACHE 300000
#define MAX_COUNTER_NAME_LEN 256
int counter_cache_number = 0;

struct counter_cache_table_s {
    char name[MAX_COUNTER_NAME_LEN];
    uint16_t strlen;
    int invalid;
    int index;
};
struct counter_cache_table_s counter_cache_table[MAX_COUNTER_CACHE];
int find_counter_index(const char *name, int *index);
int save_counter_index(const char *name, int index);

int
save_counter_index(const char *name, int cindex)
{
    int namelen = 0, i = 0, saved = 0;

    if (name == NULL || cindex == -1) {
	return EINVAL;
    }
    if (counter_cache_number == MAX_COUNTER_CACHE) {
	return ENOMEM;
    }

    namelen = strlen(name);
    /* include one space for '\0', strlen doesn't include '\0' */
    if (namelen >= MAX_COUNTER_NAME_LEN -1 ) {
	return EINVAL;
    }
#ifdef USE_TRIE_LOOKASIDE
    for (i = counter_cache_number; i < MAX_COUNTER_CACHE; i++) {
    	if (1) {
#else
    for (i = 0; i < MAX_COUNTER_CACHE; i++) {
	if (1 == counter_cache_table[i].invalid) {
#endif
	    /* we found a entry */
	    strncpy(counter_cache_table[i].name, name,
		    sizeof(counter_cache_table[i].name)-1);
	    counter_cache_table[i].name[MAX_COUNTER_NAME_LEN] = '\0';
	    counter_cache_table[i].strlen = namelen;
	    counter_cache_table[i].index = cindex;
	    counter_cache_table[i].invalid = 0;
#ifdef USE_TRIE_LOOKASIDE
	    if (trie_add_cntcache(name, (void *)&counter_cache_table[i])) {
	    	return 1; // Error
	    }
#endif
	    counter_cache_number++;
	    saved = 1;
	    break;
	}
    }
    if (saved) {
	return 0;
    }
    return 1;
}

int
find_counter_index(const char *name, int *pindex)
{
#ifdef USE_TRIE_LOOKASIDE
    int rv;
    void *data;
    if (name == NULL) {
	return EINVAL;
    }

    rv = trie_cntcache_name_to_data(name, &data);
    if (!rv) {
    	*pindex = ((uint64_t)data - (uint64_t)counter_cache_table)/
			sizeof(struct counter_cache_table_s);
    } else {
    	*pindex = -1;
    }
    return 0;

#else /* !USE_TRIE_LOOKASIDE */
    int num = counter_cache_number, i = 0, namelen = 0;
    if (name == NULL) {
	return EINVAL;
    }
    *pindex = -1;
    namelen = strlen(name);
    for (i = 0; i < num; i++) {
	if (counter_cache_table[i].invalid == 1) {
	    continue;
	}
	if (counter_cache_table[i].strlen != namelen) {
	    continue;
	}
	if (0 == strcmp(name, counter_cache_table[i].name)) {
	    *pindex = counter_cache_table[i].index;
	    break;
	}
    }
    return 0;
#endif /* !USE_TRIE_LOOKASIDE */
}

uint64_t
cntr_cache_get_value(const char *name, int dynamic, int type)
{
#ifndef USE_TRIE_LOOKASIDE
    int i = 0;
#endif
    int cindex = 0, err = 0;
    uint64_t value = 0;
    int refresh_local = ssl_refresh_needed;

    if ((name == NULL) || (name[0]=='\0')) {
	return 0;
    }
    /* avoid compile time error */
    type  = type;
    if ( 0 != refresh_local) {
#ifdef USE_TRIE_LOOKASIDE
	trie_invalidate_cntcache();
#else
	/* some configuration changed which affected cached or nvsd restarted */
	for(i=0;i<MAX_COUNTER_CACHE;i++) {
	    /* mark all entries as invalid */
	    counter_cache_table[i].invalid=1;
	}
#endif
	/* set number of counters available to zero */
	counter_cache_number = 0;
	ssl_refresh_needed = 0;
    }

    /*
     * check if we have space in cache, if not then use existing
     * nkncnt_get_XXX()
     */
    if (counter_cache_number == MAX_COUNTER_CACHE) {
	/* TODO - call nkncnt_get_XXX() */
	return 0;
    }
    err = find_counter_index(name, &cindex);
    if (err) {
	/* some error occured while finding index */
	goto bail_9;
    }
    if (cindex == -1) {
	/* counter is not there in cache */
	cindex  = 0;
	err = sslcnt_get_counter_index(name, dynamic, &cindex);
	if (err) {
	    goto bail_9;
	}
	if (cindex == -1) {
	    goto bail_9;
	}
	err = save_counter_index(name, cindex);
	if (err) {
	    /* counter cache ran out of space */
	    goto bail_9;
	}
    }
    err = sslcnt_get_counter_byindex(name, cindex, &value);
    if (err) {
	value = 0;
	goto bail_9;
    }
    return value;

bail_9:
    ssl_refresh_needed = 1;
    return 0;
}
