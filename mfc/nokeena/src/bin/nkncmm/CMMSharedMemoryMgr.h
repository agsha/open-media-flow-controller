/*
 * CMMSharedMemoryMgr.h - Shared memory manager for state machine data.
 */
#ifndef CMMSHAREDMEMORYMGR_H
#define CMMSHAREDMEMORYMGR_H

#include "nkn_cmm_shm.h"

typedef int shm_token_t;

class CMMSharedMemoryMgr {
    public:
    	CMMSharedMemoryMgr();
    	~CMMSharedMemoryMgr();

	// Returns: 0 => Success
	int CMMShmInit(int shm_key, int shm_size, unsigned int max_hdr_size,
		       int shm_magicno, int shm_version, 
		       int segment_size, int segments);

	// Returns: 0 => Success
	int CMMShmShutdown();

	// Returns: 0 => Success
	int CMMGetShm(char*& buf, int& bufsize, shm_token_t &token);

	// Returns: 0 => Success
	int CMMFreeShm(shm_token_t token);

    private:
	int _cmm_shmid;
	cmm_shm_hdr_t* _cmm_shm;
	uint64_t _cmm_shm_size;
    	int _cmm_last_index;
};
#endif /* CMMSHAREDMEMORYMGR_H */

/*
 * End of CMMSharedMemoryMgr.h
 */
