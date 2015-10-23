/*
 * nkn_cmm_shm_api.c - CMM shared memory access API
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/param.h>
#include <string.h>
#include <errno.h>
#include "nkn_cmm_shm.h"
#include "nkn_cmm_shm_api.h"

static int shm_init_complete = 0;
static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

static int error_point;
static int last_errno;

static cmm_shm_hdr_t *cmm_hdr;
#define CMM_HDR cmm_hdr

static int setup_shm_segment(void)
{
    int shmid;

    /* Note: Single threaded execution under "init_mutex". */

    while (1) {

    shmid = shmget(CMM_LD_SHM_KEY, CMM_LD_SHM_SIZE, 0);
    if (shmid == -1) {
        error_point = 1;
	last_errno = errno;
	break;
    }

    CMM_HDR = (cmm_shm_hdr_t *)shmat(shmid, (void *)0, SHM_RDONLY);
    if (CMM_HDR == (cmm_shm_hdr_t *) -1) {
    	error_point = 2;
	last_errno = errno;
	break;
    }
    
    if ((CMM_HDR->cmm_magic != CMM_LD_SHM_MAGICNO) ||
    	(CMM_HDR->cmm_version != CMM_LD_SHM_VERSION)) {
	error_point = 3;
	break;
    }
    error_point = 0;
    break;

    } /* End while */

    return error_point;
}

int
cmm_shm_open_chan(const char *node_handle, cmm_shm_chan_type_t type, 
		  cmm_shm_chan_t *chan)
{
    int n;
    cmm_loadmetric_entry_t *lmp;

    if (!shm_init_complete) {
    	pthread_mutex_lock(&init_mutex);
	if (!shm_init_complete) {
	    if (!setup_shm_segment()) {
	    	shm_init_complete = 1;
	    } else {
    		pthread_mutex_unlock(&init_mutex);
	    	return 10; // Unable to setup shared memory segment
	    }
	}
    	pthread_mutex_unlock(&init_mutex); 
    }

    if (!node_handle || !chan) {
    	return 1; /* Invalid input */
    }

    switch(type) {
    case CMMSHM_CH_LOADMETRIC:
    	break;
    default:
    	return 2; /* Invalid */
    }

    for (n = 0; n < CMM_HDR->cmm_segments; n++) {
    	if (isset(CMM_HDR->cmm_bitmap, n)) {
	    lmp = (cmm_loadmetric_entry_t *)CMM_SEG_PTR(CMM_HDR, n);
	    if (strcmp(node_handle, lmp->u.loadmetric.node_handle) == 0) {
	    	chan->u.chan.magicno = CMM_CHAN_MAGICNO;
	    	chan->u.chan.index = n;
	    	chan->u.chan.incarnation = lmp->u.loadmetric.incarnation;

		return 0; /* Found */
	    }
	}
    }
    return 3; /* Not found */
}

int
cmm_shm_close_chan(cmm_shm_chan_t *chan)
{
    chan->u.chan.magicno = 0;
    return 0;
}

int
cmm_shm_is_chan_invalid(cmm_shm_chan_t *chan)
{
    cmm_loadmetric_entry_t *lmp;

    if (!chan || (chan->u.chan.magicno != CMM_CHAN_MAGICNO)) {
        return 1;
    }

    if (!isset(CMM_HDR->cmm_bitmap, chan->u.chan.index)) { /* Entry dealloced */
    	chan->u.chan.magicno = ~CMM_CHAN_MAGICNO;
    	return 2;
    }
    lmp = (cmm_loadmetric_entry_t *)CMM_SEG_PTR(CMM_HDR, chan->u.chan.index);

    if (chan->u.chan.incarnation != lmp->u.loadmetric.incarnation) { /* Stale */
    	chan->u.chan.magicno = ~CMM_CHAN_MAGICNO;
    	return 3;
    }

    return 0; // Valid
}

int
cmm_shm_chan_getdata_ptr(cmm_shm_chan_t *chan, void **data, int *datalen,
			 uint64_t *version)
{
    cmm_loadmetric_entry_t *lmp;

    if (cmm_shm_is_chan_invalid(chan)) {
    	return 1;
    }
    lmp = (cmm_loadmetric_entry_t *)CMM_SEG_PTR(CMM_HDR, chan->u.chan.index);

    *data = &lmp->u.loadmetric.load_metric;
    *datalen = sizeof(node_load_metric_t);
    if (version) {
    	*version = lmp->u.loadmetric.version;
    }

    return 0;
}

/*
 * End of nkn_cmm_shm_api.c
 */
