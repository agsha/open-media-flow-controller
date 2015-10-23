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

#include "nkn_cntr_api.h"

#define USE_TRIE_LOOKASIDE 1 // Enabled


static inline int check_if_init(void);

#ifdef USE_TRIE_LOOKASIDE
#include "cprops/collection.h"
#include "cprops/trie.h"

pthread_mutex_t nkncnt_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cntcache_lock = PTHREAD_MUTEX_INITIALIZER;

#define DEFINE_GET_COUNT(type) \
type ##_t nkncnt_get_## type (const char *name) { \
        type ##_t temp = 0;                 \
        int i = 0;                      \
        if (shm == NULL) \
                libnkncnt_init();        \
        if (shm == NULL ) return temp; \
	if (!trie_nkncnt_name_to_index(name, &i)) { \
	    if (i >= 0) { \
		temp = g_allcnts[i].value; \
	    } \
	} \
        return temp; \
}

#else /* !USE_TRIE_LOOKASIDE */

#define DEFINE_GET_COUNT(type) \
type ##_t nkncnt_get_## type (const char *name) { \
        type ##_t temp = 0;                 \
        int i = 0;                      \
        if (shm == NULL) \
                libnkncnt_init();        \
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
static cp_trie *nkn_cnt_trie;

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

static int trie_nkncnt_name_to_index(const char *name, int *cindex)
{
    int n;
    int rv;
    int ret;
    nkn_shmdef_t cur_shmdef;
    glob_item_t *item;

    pthread_mutex_lock(&nkncnt_lock);

    cur_shmdef = *pshmdef;
    while (cur_shmdef.revision != shmdef.revision) {
    	if (nkn_cnt_trie) {
	    cp_trie_destroy(nkn_cnt_trie);
	}
	nkn_cnt_trie = cp_trie_create_trie(
				(COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|
		 	 	COLLECTION_MODE_DEEP), 
				trie_copy_func, trie_destruct_func);
	if (!nkn_cnt_trie) {
	    ret = 1;
	    goto exit;
	}

	for (n = 0; n < pshmdef->tot_cnts; n++) {
	    if (!g_allcnts[n].size) {
	    	continue;
	    }
	    rv = cp_trie_add(nkn_cnt_trie,(varname+g_allcnts[n].name), 
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

    item = (glob_item_t *)cp_trie_exact_match(nkn_cnt_trie, (char *)name);
    if (item) {
    	*cindex = (((uint64_t)item - (uint64_t)g_allcnts)/sizeof(glob_item_t));
	ret = 0;
    } else {
    	*cindex = -1;
    	ret = 0; // Not found
    }

exit:

    pthread_mutex_unlock(&nkncnt_lock);
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
libnkncnt_init(void)
{
        int err = 0;
	int shmid;
	uint64_t NKN_SHMSZ;

        if (NULL != shm) {
                /* Already initialized */
                return 0;
        }

	NKN_SHMSZ = (sizeof(nkn_shmdef_t)+MAX_CNTS_NVSD*(sizeof(glob_item_t)+30));
        if ((shmid = shmget(NKN_SHMKEY, NKN_SHMSZ, 0666)) < 0) {
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
                        sizeof(glob_item_t) * MAX_CNTS_NVSD);
       
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
                libnkncnt_close();
                goto bail;
	}

        return err;
bail:
        shm = NULL;
        return -1;
}

int libnkncnt_close(void)
{
        if (shm) {
                int err  = 0;
                err = shmdt(shm);
                shm = NULL;
                return err;
        }

        return 0;
}

