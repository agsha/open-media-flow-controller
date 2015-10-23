/*
 * CMMNewDelete.cc - CMM custom new/delete functions
 */
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <sys/param.h>
#include <atomic_ops.h>
#include "CMMNewDelete.h"

using namespace std;

#define MAGIC_ALLOC 0x12345678
#define MAGIC_MALLOC_ALLOC 0x87654321
#define MAGIC_FREE  0xdeadbeef

// MAX_POOLS where each pool is a pwr2 of POOL_ELEMENT_SIZE
#define POOL_ELEMENT_SIZE_PWR2 7
#define POOL_ELEMENT_SIZE (1 << POOL_ELEMENT_SIZE_PWR2)

#define C_64MB_PWR2 26
#define MAX_POOLS 17

class MemAllocHdr {
    public:
    	int32_t _magicno;
    	int32_t _pool_number;
};

class FreeListHdr : public MemAllocHdr {
    public:
	FreeListHdr* _next;
};

static int FastAllocEnabled = 0;
static int AllocInit = 0;
static FreeListHdr FreeList[MAX_POOLS];
static int FreeListElements[MAX_POOLS];
static int FastAllocMisses[MAX_POOLS];
static int FreeListMin[MAX_POOLS] = {
	INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX,
	INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX,
	INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX,
	INT_MAX, INT_MAX,
};

static pthread_mutex_t FreeListMutex[MAX_POOLS] = {
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
};

static const int FreeListSize[MAX_POOLS] = {
    (POOL_ELEMENT_SIZE << 0),  // 128
    (POOL_ELEMENT_SIZE << 1),  // 256
    (POOL_ELEMENT_SIZE << 2),  // 512
    (POOL_ELEMENT_SIZE << 3),  // 1024
    (POOL_ELEMENT_SIZE << 4),  // 2048
    (POOL_ELEMENT_SIZE << 5),  // 4096
    (POOL_ELEMENT_SIZE << 6),  // 8192
    (POOL_ELEMENT_SIZE << 7),  // 16384
    (POOL_ELEMENT_SIZE << 8),  // 32768
    (POOL_ELEMENT_SIZE << 9),  // 65536
    (POOL_ELEMENT_SIZE << 10), // 131072
    (POOL_ELEMENT_SIZE << 11), // 262144
    (POOL_ELEMENT_SIZE << 12), // 524288
    (POOL_ELEMENT_SIZE << 13), // 1048576
    (POOL_ELEMENT_SIZE << 14), // 2*1048576
    (POOL_ELEMENT_SIZE << 15), // 4*1048576
    (POOL_ELEMENT_SIZE << 16), // 8*1048576
};

static const int MaxFreeListElements[MAX_POOLS] = { // 64MB max per list
    (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 0)),
    (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 1)),
    (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 2)),
    (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 3)),
    (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 4)),
    (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 5)),
    (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 6)),
    (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 7)),
    (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 8)),
    (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 9)),
    64, // 8MB
    32, // 8MB
    16, // 8MB
    8,  // 8MB
    4,  // 8MB
    4,  // 16MB
    4,  // 32MB
};

// Stats
static long MallocAllocs;
static long FastAllocs;
static long FastFree;
static long FreeFree;
AO_t MallocBytes;
static long LargeMallocBytes;

static int Size2Pool(size_t sz)
{
    int pool;

    for (pool = 0; pool < MAX_POOLS; pool++) {
    	if ((int)sz <= FreeListSize[pool]) {
	    return pool;
	}
    }
    return -1; // No pool to satisfy request
}

void CMMAllocInit(int useFastAlloc)
{
    int pool;
    int n;
    FreeListHdr* pobj = NULL;

    FastAllocEnabled = useFastAlloc;
    if (!FastAllocEnabled) {
    	return;
    }

    for (pool = 0; pool < MAX_POOLS; pool++) {
    	for (n = 0; n < MaxFreeListElements[pool]; n++) {
	    pobj = (FreeListHdr*)malloc(FreeListSize[pool]);
	    if (pobj) {
	    	pobj->_magicno = MAGIC_ALLOC;
	    	pobj->_pool_number = pool;
	    	MallocAllocs++;
	    	AO_fetch_and_add(&MallocBytes, FreeListSize[pool]);
	    	CMMdelete((void*)&pobj->_next);
	    }
	}
    }
    AllocInit = 1;
}

void* CMMnew(size_t sz) 
{
    int realsize;
    int pool;
    FreeListHdr* pobj = NULL;

    if (!sz) {
    	return pobj;
    }
    realsize = roundup((sizeof(MemAllocHdr) + sz), sizeof(void*));
    pool = Size2Pool(realsize);

    if (pool >= 0) {
    	pthread_mutex_lock(&FreeListMutex[pool]);
	if (FreeListElements[pool]) {
	    pobj = FreeList[pool]._next;
	    FreeList[pool]._next = pobj->_next;
	    FreeListElements[pool]--;
	    FastAllocs++;
	    if (AllocInit && (FreeListElements[pool] < FreeListMin[pool])) {
	    	FreeListMin[pool] = FreeListElements[pool];
	    }
	    pthread_mutex_unlock(&FreeListMutex[pool]);
	} else {
	    FastAllocMisses[pool]++;
	    MallocAllocs++;
	    pthread_mutex_unlock(&FreeListMutex[pool]);
	    pobj = (FreeListHdr*)malloc(FreeListSize[pool]);
	    AO_fetch_and_add(&MallocBytes, FreeListSize[pool]);
	}
	if (pobj) {
	    pobj->_magicno = MAGIC_ALLOC;
	    pobj->_pool_number = pool;
	    return (void*)&pobj->_next;
	}
    } else {
	pobj = (FreeListHdr*)malloc(realsize);
	if (pobj) {
	    pobj->_magicno = MAGIC_MALLOC_ALLOC;
	    pobj->_pool_number = realsize;
	    MallocAllocs++;
	    AO_fetch_and_add(&MallocBytes, realsize);
	    LargeMallocBytes += realsize;
	    return (void*)&pobj->_next;
	}
    }
    return (void*)pobj;
}

void* operator new(size_t sz) 
{
    return CMMnew(sz);
}

void CMMdelete(void* p) 
{
    FreeListHdr* pobj;
    int pool;

    if (!p) {
    	return;
    }
    pobj = (FreeListHdr*)((char*)p - sizeof(MemAllocHdr));

    if (pobj) {
    	pool = pobj->_pool_number;
    	if ((pobj->_magicno == MAGIC_ALLOC) && (pool >= 0)) {
	    pthread_mutex_lock(&FreeListMutex[pool]);
	    if (FastAllocEnabled && 
	    	FreeListElements[pool] < MaxFreeListElements[pool]) {
	        pobj->_magicno = MAGIC_FREE;
	        pobj->_next = FreeList[pool]._next;
	        FreeList[pool]._next = pobj;
		FreeListElements[pool]++;
		FastFree++;
	    	pthread_mutex_unlock(&FreeListMutex[pool]);
	    } else {
	    	pthread_mutex_unlock(&FreeListMutex[pool]);
		free((void*)pobj);
		FreeFree++;
	    	AO_fetch_and_add(&MallocBytes, -FreeListSize[pool]);
	    }
	} else {
	    free((void*)pobj);
	    FreeFree++;
	    AO_fetch_and_add(&MallocBytes, -pool);
	}
    }
}

void operator delete(void* p) 
{
    return CMMdelete(p);
}

/*
 * End of CMMNewDelete.cc
 */
