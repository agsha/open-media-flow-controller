/*
 * CMMSharedMemoryMgr.cc - Shared memory manager for state machine data.
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
#include <errno.h>
#include <assert.h>

#include "CMMSharedMemoryMgr.h"
#include "cmm_defs.h"
#include "nkn_cmm_shm.h"

////////////////////////////////////////////////////////////////////////////////
// Public functions
////////////////////////////////////////////////////////////////////////////////

CMMSharedMemoryMgr::CMMSharedMemoryMgr() : _cmm_shmid(0), _cmm_shm(0), 
	_cmm_shm_size(0), _cmm_last_index(0)
{
}

CMMSharedMemoryMgr::~CMMSharedMemoryMgr()
{
}

////////////////////////////////////////////////////////////////////////////////
// Static functions
////////////////////////////////////////////////////////////////////////////////

// Returns: 0 => Success
int 
CMMSharedMemoryMgr::CMMShmInit(int shm_key, int shm_size, 
			       unsigned int max_hdr_size,
			       int shm_magicno, int shm_version, 
			       int segment_size, int segments)
{
    int retval = 0;
    cmm_shm_hdr_t hdr;

    while (1) { // Begin while

    _cmm_shmid = shmget(shm_key, shm_size, (0644|IPC_CREAT));
    if (_cmm_shmid == -1) {
    	DBG("shmget() failed, errno=%d shmid=%d", errno, _cmm_shmid);
	retval = 1;
	break;
    }

    _cmm_shm = (cmm_shm_hdr_t*)shmat(_cmm_shmid, (void *)0, 0);
    if (_cmm_shm != (cmm_shm_hdr_t*)-1) {
        _cmm_shm_size = shm_size;

        memset(&hdr, 0, sizeof(cmm_shm_hdr_t));
	hdr.cmm_magic = shm_magicno;
	hdr.cmm_version = shm_version;
	hdr.cmm_segment_size = segment_size;
	hdr.cmm_segments = segments;
	hdr.cmm_seg_offset = max_hdr_size;

	memcpy(_cmm_shm, &hdr, sizeof(hdr));
	memset(_cmm_shm->cmm_bitmap, 0, roundup(segments, NBBY)/NBBY);
    } else {
    	DBG("Unable to create shared memory segment, size=%d "
	    "addr=%p errno=%d", shm_size, _cmm_shm, errno);
	retval = 2;
	break;
    }
    break;

    } // End while

    return retval;
}

// Returns: 0 => Success
int 
CMMSharedMemoryMgr::CMMShmShutdown()
{
    int rv;

    if (_cmm_shm != (cmm_shm_hdr_t*)-1) {
#if 0
	// Mark segment for delete when refcnt=0
    	rv = shmctl(_cmm_shmid, IPC_RMID, NULL);
	if (rv == -1) {
	    DBG("shmctl(shmid=%d, IPC_RMID) failed, rv=%d errno=%d", 
	    	_cmm_shmid, rv, errno);
	}
#endif

    	rv = shmdt(_cmm_shm);
	if (rv) {
	    DBG("shmdt(addr=%p) failed, rv=%d errno=%d", _cmm_shm, rv, errno);
	}
    }
    return 0;
}

// Returns: 0 => Success
int 
CMMSharedMemoryMgr::CMMGetShm(char*& buf, int& bufsize, shm_token_t& token)
{
    int n;
    int index;

    if (_cmm_shm == (cmm_shm_hdr_t*)-1) {
    	return 1; // Shared memory use disabled
    }

    for (n = 0; n < _cmm_shm->cmm_segments; n++) {
    	index = (_cmm_last_index + n) % _cmm_shm->cmm_segments;
	if (isclr(_cmm_shm->cmm_bitmap, index)) {
	    // Found free entry
	    buf = (char*)CMM_SEG_PTR(_cmm_shm, index);
	    bufsize = _cmm_shm->cmm_segment_size;
	    token = index;
	    _cmm_last_index = (index + 1) % _cmm_shm->cmm_segments;
	    memset(buf, 0, bufsize);
	    setbit(_cmm_shm->cmm_bitmap, index);
	    return 0; // Success
	}
    }
    return 2; // Failed
}

// Returns: 0 => Success
int 
CMMSharedMemoryMgr::CMMFreeShm(shm_token_t token)
{
    if ((token >= 0) && (token < _cmm_shm->cmm_segments)) {
    	if (isset(_cmm_shm->cmm_bitmap, token)) {
    	    clrbit(_cmm_shm->cmm_bitmap, token);
	} else {
	    DBG("Freeing free entry, index=%d, ignoring request", token);
	}
    } else {
	DBG("Invalid token=%d, valid range (%d-%d), ignoring request", 
	    token, 0, (_cmm_shm->cmm_segments-1));
    }
    return 0;
}

/*
 * End of CMMSharedMemoryMgr.cc
 */
