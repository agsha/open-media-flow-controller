/*
 *
 * Filename:  nkn_memalloc.c
 *	      Intercept memory allocation calls
 *
 * (C) Copyright 2010 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/user.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <atomic_ops.h>

#include "nkn_memalloc_priv.h"
#include "nkn_memalloc.h"
#include "nkn_lockstat.h"
#include "nkn_util.h"
#include "nkn_stat.h"

extern void *__libc_malloc(size_t size);
extern void  __libc_free(void *ptr);
extern void *__libc_realloc(void *ptr, size_t size);
extern void *__libc_calloc(size_t n, size_t size);
extern void  __libc_cfree(void *ptr);
extern void *__libc_memalign(size_t align, size_t s);
extern void *__libc_valloc(size_t size);
extern void *__libc_pvalloc(size_t size);

void *memalign(size_t align, size_t s);
void *pvalloc(size_t size);

long __attribute__ ((weak)) glob_lockwait_MALLOC_LOCK;
/*While doing zvm build use native mallocs*/
#ifdef __ZVM__
#undef ENABLE_INSTRUMENTED_MALLOC
#endif
////////////////////////////////////////////////////////////////////////////////
#ifdef ENABLE_INSTRUMENTED_MALLOC
////////////////////////////////////////////////////////////////////////////////
/*
 *******************************************************************************
 * Memory management.
 *    We are not freeing the memory to achieve fast memory allocation
 *******************************************************************************
 */

int32_t MAX_BUF_IN_SLOT = 3000;

nkn_lockstat_t mslotlockstat = NKN_LOCKSTAT_INITVAL;
// mem hdr
#define MEM_SLOT_GLIB		0               // glib hit
#define MEM_SLOT_NLIB		1               // nlib hit

//memqueue
#define MEM_SLOT_INIT_DONE	4

#if 0
#define SLOT_LOCK(x)    \
			if (!(x->flags & MEM_SLOT_LOCAL))	\
				NKN_MUTEX_LOCKSTAT(&x->mutex, &mslotlockstat);
#endif

#define SLOT_LOCK(x)	\
			NKN_MUTEX_LOCKSTAT(&x->mutex, &mslotlockstat);

#if 0
#define SLOT_UNLOCK(x)  \
			if (!(x->flags & MEM_SLOT_LOCAL))	\
				NKN_MUTEX_UNLOCKSTAT(&x->mutex);
#endif

#define SLOT_UNLOCK(x)    \
			NKN_MUTEX_UNLOCKSTAT(&x->mutex);

#define SLOT_TO_SIZE(slot) (size_t)(INIT_SLOT_SIZE * (slot + 1))
#define SIZE_TO_SLOT(size, slot)	\
		slot = ((size - 1) >> INIT_SLOT_SHIFT);

static mem_slot_queue_t mem_slot_q[MAX_SLOTS] = {
    NKN_MEM_SLOT_INIT
};
static int global_mem_pool_init = 0;

// Memory Header MAGIC
#define MH_MAGIC 0xf5f
#define MH_MAGIC_FREE 0xded

/* MAX values for memory, alloc type and alignment */
#define MH_MAX_MEMORY_SIZE (1UL << 37)
#define MH_MAX_TYPE_VAL    (1 << 12)
#define MH_MAX_ALIGN_VAL   (1 << 4)

#define MEM_TO_MHDR(_ptr) \
	((mem_alloc_hdr_t *)((char *)(_ptr) - sizeof(mem_alloc_hdr_t)))

#define MHDR_TO_MEM(_ptr) \
	((char *)((char *)(_ptr) + sizeof(mem_alloc_hdr_t)))

#define MHDR_TO_MEM_START(_mh) \
	((_mh)->align_units ? \
	 (char *)((char *)(_mh) + sizeof(*(_mh))) - \
		((size_t)(1 << (_mh)->align_log2) * (_mh)->align_units) : \
			(char *)((_mh)))

// Support for thread local pools
pthread_key_t mempool_key;
pthread_once_t mempool_once = PTHREAD_ONCE_INIT;

static int nkn_pool_enable = 1;

static void *mem_slot_get(mem_slot_queue_t *pslot, mem_slot_queue_t **base);
//static void  *mem_slot_get(mem_slot_queue_t *pslot);
static int mem_slot_put(mem_slot_queue_t *pslot, void *data);

static void
init_slot_mem(mem_slot_queue_t **base)
{
    u_int64_t  preallocate_size = 0;
    mem_slot_queue_t *pslot;
    int allcnt, pcnt;
    u_int64_t poff;
    struct free_queue *pfreeq;

    pslot = *base;
    for (allcnt = 0; allcnt < MAX_SLOTS; ++allcnt) {
	preallocate_size += pslot->size * pslot->prealloc_count;
	pslot++;
    }
    if (preallocate_size == 0) {
	base[0]->flags |= MEM_SLOT_INIT_DONE;
	return;
    }
    base[0]->prealloc_addr = __libc_malloc(preallocate_size);
    if (base[0]->prealloc_addr == NULL)
	return;
    void *temp_alloc = base[0]->prealloc_addr;
    pslot = *base;
    for (allcnt = 0; allcnt < MAX_SLOTS; ++allcnt) {
	SLOT_LOCK(pslot);
	pslot->tot_buf_in_this_slot = pslot->prealloc_count;
	for (pcnt =0, poff=0; pcnt < pslot->tot_buf_in_this_slot;
	     pcnt++, poff+=pslot->size) {
	    pfreeq = (struct free_queue *)
		((u_int64_t)temp_alloc + (u_int64_t)poff);
	    pfreeq->next = pslot->freeq;
	    pslot->freeq = pfreeq;
#ifdef MEM_DEBUG
	    AO_fetch_and_add1(&pslot->cnt);
#endif
	}
	SLOT_UNLOCK(pslot);
	pslot++;
	temp_alloc = (void *)((u_int64_t)temp_alloc + (u_int64_t)poff);
    }
    base[0]->flags |= MEM_SLOT_INIT_DONE;
}

static void *
mem_slot_get(mem_slot_queue_t *pslot,
	     mem_slot_queue_t **base)
{
    struct free_queue *pfreeq;

    if ((base[0]->flags & MEM_SLOT_INIT_DONE) == 0) {
	// don't call init_slot_mem for global pool
	if (*base  != mem_slot_q)
	    init_slot_mem(base);
    }
    // Quick, lock free check
    if (!pslot->freeq) {
#ifdef MEM_DEBUG
	AO_fetch_and_add1(&pslot->miss);
	if (AO_load(&pslot->miss) +
	    AO_load(&pslot->hits) -
	    AO_load(&pslot->free) > AO_load(&pslot->hwm))
	    AO_store(&pslot->hwm, AO_load(&pslot->miss) +
		     AO_load(&pslot->hits) - AO_load(&pslot->free));
#endif
	return NULL;
    }
    SLOT_LOCK(pslot);
    pfreeq = pslot->freeq;
    if (pfreeq != NULL) {
	pslot->freeq = pfreeq->next;
	pslot->tot_buf_in_this_slot --;
	SLOT_UNLOCK(pslot);
#ifdef MEM_DEBUG
	if (AO_load(&pslot->miss) + AO_load(&pslot->hits) -
	    AO_load(&pslot->free) > AO_load(&pslot->hwm))
	    AO_store(&pslot->hwm, AO_load(&pslot->miss) +
		     AO_load(&pslot->hits) - AO_load(&pslot->free));
	AO_fetch_and_add1(&pslot->hits);
	AO_fetch_and_sub1(&pslot->cnt);
#endif
	return (void *)pfreeq;
    }
    SLOT_UNLOCK(pslot);
#ifdef MEM_DEBUG
    AO_fetch_and_add1(&pslot->miss);
#endif
    return NULL;
}

static int
mem_slot_put(mem_slot_queue_t *pslot,
	     void *data)
{
    struct free_queue *pfreeq;

    if (pslot->tot_buf_in_this_slot >= pslot->max_bufs) {
#ifdef MEM_DEBUG
	AO_fetch_and_add1(&pslot->full);
#endif
	return -1;
    }
    pfreeq = (struct free_queue *)data;
    // insert into freeq
    SLOT_LOCK(pslot);
    pfreeq->next = pslot->freeq;
    pslot->freeq = pfreeq;
    pslot->tot_buf_in_this_slot ++;
    SLOT_UNLOCK(pslot);
#ifdef MEM_DEBUG
    AO_fetch_and_add1(&pslot->free);
    AO_fetch_and_add1(&pslot->cnt);
#endif
    return 0;
}

static void *mem_get_memory(size_t align, size_t *alloc_size,
			     int *ispool, mem_slot_queue_t **ppslot);
static void *
mem_get_memory(size_t align,
	       size_t *alloc_size,
	       int *ispool,
	       mem_slot_queue_t **ppslot)
{
    mem_slot_queue_t *pslot = NULL;
    size_t new_alloc_size;
    size_t sn;
    void *data;

    new_alloc_size = *alloc_size;
    if (!nkn_pool_enable || align || (*alloc_size > SIZE_MAX_SLOT))
	goto get_poolmiss;

    SIZE_TO_SLOT(*alloc_size, sn);
    new_alloc_size = SLOT_TO_SIZE(sn);
    *alloc_size = new_alloc_size;

    // Check thread local pool if available
    if ((int)mempool_key) {
	pslot = pthread_getspecific(mempool_key);
	if (pslot) {
	    *ppslot = &pslot[sn];
	    if ((data = mem_slot_get(&pslot[sn], &pslot))) {
		*ispool = MEM_SLOT_NLIB;
		return data;
	    }
	}
    }

    if (!pslot) {
	// Check global pool
	pslot = &mem_slot_q[0];
	*ppslot = &pslot[sn];
	data = mem_slot_get(&mem_slot_q[sn], &pslot);
	if (data) {
	    *ispool = MEM_SLOT_NLIB;
	    return data;
	}
    }

    // Fall thru to regular allocation with a size that can be
    // pooled during the free.

 get_poolmiss:
    if (align) {
	data =  __libc_memalign(align, new_alloc_size);
    } else {
	data = __libc_malloc(new_alloc_size);
    }
    return data;
}

static void mem_return_memory(void *data, size_t alloc_size, mem_alloc_hdr_t *mh);
static void
mem_return_memory(void *data,
		  size_t alloc_size,
		  mem_alloc_hdr_t *mh)
{
    mem_slot_queue_t *pslot = mh->pslot;
    size_t sn;
    size_t align = mh->align_log2;
    int ispool   = mh->flags;

    if (!nkn_pool_enable || align || (alloc_size > SIZE_MAX_SLOT))
	goto ret_poolmiss;

    SIZE_TO_SLOT(alloc_size, sn);

    // Must be an exact size match to add to the pool.
    if (alloc_size != SLOT_TO_SIZE(sn))
	goto ret_poolmiss;

    if (ispool && !mem_slot_put(pslot, data))
	return;
#ifdef MEM_DEBUG

    /*
     * Because we bump 'miss' on a 'get' which can not find an obj
     * but whose size fits the pool, we must bump 'free' when we
     * free an obj which never calls mem_slot_put but should fit
     * into the pool
     */
    SLOT_LOCK(pslot);
    pslot->free++;
    SLOT_UNLOCK(pslot);
#endif


 ret_poolmiss:
    __libc_free(data);
    return;
}

void dbg_dump_mem_pool(void);
void dbg_dump_mem_pool(void)
{
    int i;

    printf("memory\tqueue size\n");
    for (i=0; i<MAX_SLOTS; i++) {
	printf("%d\t%d\n", mem_slot_q[i].size,
	       mem_slot_q[i].tot_buf_in_this_slot);
    }
}

static void
nkn_create_pool_key(void)
{
    (void) pthread_key_create(&mempool_key, NULL);
}

static void
nkn_mem_mon_add(mem_slot_queue_t *pool, const char *pname)
{
    mem_slot_queue_t *slot;
    int i, size;

    for (i=0,slot=pool,size=INIT_SLOT_SIZE; i<MAX_SLOTS; i++,slot++,size*=2) {
	char inst[16];
#if 1	// link issue with nknfqueue
	snprintf(inst, 16, "%d_hits", slot->size);
	nkn_mon_add(inst, pname, &slot->hits, sizeof(slot->hits));
	snprintf(inst, 16, "%d_miss", slot->size);
	nkn_mon_add(inst, pname, &slot->miss, sizeof(slot->miss));
	snprintf(inst, 16, "%d_full", slot->size);
	nkn_mon_add(inst, pname, &slot->full, sizeof(slot->full));
	snprintf(inst, 16, "%d_free", slot->size);
	nkn_mon_add(inst, pname, &slot->free, sizeof(slot->free));
	snprintf(inst, 16, "%d_hwm", slot->size);
	nkn_mon_add(inst, pname, &slot->hwm, sizeof(slot->hwm));
	snprintf(inst, 16, "%d_slotcnt", slot->size);
	nkn_mon_add(inst, pname, &slot->cnt, sizeof(slot->cnt));
	snprintf(inst, 16, "%d_maxbufs", slot->size);
	nkn_mon_add(inst, pname, &slot->max_bufs, sizeof(slot->max_bufs));

#else
	snprintf(inst, 16, "%s", pname);
#endif
    }
}

void
nkn_mem_create_thread_pool(const char *pname)
{
    mem_slot_queue_t *pool, *slot;
    int i, size;

    if (!global_mem_pool_init) {
	global_mem_pool_init = 1;
	nkn_mem_mon_add(&mem_slot_q[0], "gmempool");
    }

    // Create key the first time we get here.
    (void) pthread_once(&mempool_once, nkn_create_pool_key);

    pool = pthread_getspecific(mempool_key);
    if (pool)
	return;

    pool = (mem_slot_queue_t *)nkn_calloc_type(1,
					MAX_SLOTS * sizeof(mem_slot_queue_t),
					       mod_mem_thread_pool);
    if (pool == NULL) {
	// XXX - Need to log an error
	return;
    }
    for (i=0, slot=pool, size=INIT_SLOT_SIZE;
	 i<MAX_SLOTS;
	 i++, slot++, size += INIT_SLOT_SIZE) {
	slot->size = size;
	slot->tot_buf_in_this_slot = 0;
	slot->freeq = NULL;
	slot->flags = MEM_SLOT_NLIB;
	slot->hits = 0;
	slot->miss = 0;
	slot->full = 0;
	slot->cnt = 0;
	slot->prealloc_count = 10;
	slot->max_bufs = 10;
	slot->prealloc_addr = NULL;
    }
    // updated currently based on cache hit stats
    if ((strcmp(pname, "schedmempool-0") == 0) ||
	(strcmp(pname, "schedmempool-1") == 0) ||
	(strcmp(pname, "schedmempool-2") == 0) ||
	(strcmp(pname, "schedmempool-3") == 0) ||
	(strcmp(pname, "schedmempool-4") == 0) ||
	(strcmp(pname, "schedmempool-5") == 0)) {
	pool[0].prealloc_count = pool[0].max_bufs = 100;
	pool[1].prealloc_count = pool[1].max_bufs = 100;
	pool[2].prealloc_count = pool[2].max_bufs = 100;
	pool[3].prealloc_count = pool[3].max_bufs = 100;
	pool[6].prealloc_count = pool[6].max_bufs = 100;
	pool[23].prealloc_count = pool[23].max_bufs = 200;
	pool[24].prealloc_count = pool[24].max_bufs = 200;
	pool[26].prealloc_count = pool[26].max_bufs = 100;
    }
    if ((strcmp(pname, "nt0_mempool") == 0) ||
	(strcmp(pname, "nt1_mempool") == 0) ||
	(strcmp(pname, "nt2_mempool") == 0) ||
	(strcmp(pname, "nt3_mempool") == 0) ||
	(strcmp(pname, "nt4_mempool") == 0) ||
	(strcmp(pname, "nt5_mempool") == 0)) {
	pool[1].prealloc_count = pool[1].max_bufs = 100;
	pool[2].prealloc_count = pool[2].max_bufs = 4000;
	pool[38].prealloc_count = pool[38].max_bufs = 500;
	pool[128].prealloc_count = pool[128].max_bufs = 500;
	pool[154].prealloc_count = pool[154].max_bufs = 500;
    }
    nkn_mem_mon_add(pool, pname);
    pthread_setspecific(mempool_key, (void *)pool);
}

static void initialize_sizes(void);
static void
initialize_sizes(void)
{
    int i = 0;

    for ( ; i < MAX_SLOTS ; ++i) {
	mem_slot_q[i].prealloc_count = 25;
	mem_slot_q[i].max_bufs = 25;
    }
    // updated currently based on cache hit stats
    mem_slot_q[0].prealloc_count = mem_slot_q[0].max_bufs = 2000;
    mem_slot_q[1].prealloc_count = mem_slot_q[1].max_bufs = 5000;
    mem_slot_q[2].prealloc_count = mem_slot_q[2].max_bufs = 1000;
    mem_slot_q[3].prealloc_count = mem_slot_q[3].max_bufs = 1500;
    mem_slot_q[5].prealloc_count = mem_slot_q[5].max_bufs = 200;
    mem_slot_q[10].prealloc_count = mem_slot_q[10].max_bufs = 400;
}


void
nkn_init_pool_mem(void)
{
    initialize_sizes();
    mem_slot_queue_t *pslot = &mem_slot_q[0];
    init_slot_mem(&pslot);
}

/*
 *******************************************************************************
 * Internal functions
 *******************************************************************************
 */
static void *
int_nkn_memalloc_malloc(size_t size,
			size_t align,
			nkn_obj_type_t type)
{
    size_t alloc_size;
    void *ptr;
    char *data;
    mem_alloc_hdr_t *mh;
    mem_slot_queue_t *pslot = NULL;
    int align_log2;
    int n;
    int ispool = MEM_SLOT_GLIB;

    if (size >= MH_MAX_MEMORY_SIZE || type >= MH_MAX_TYPE_VAL) {
	assert(0);
    }

    if (align) {
	// Note: align is assumed to be a pwr2 and multiple of sizeof(void*)
	for (n = 1; (n * align) < sizeof(mem_alloc_hdr_t); n++)
            ;
    } else {
	n = 0;
    }

    alloc_size = ALLOC_SIZE(size, (n * align));
    if (align) {
        // compute log2(align)
        for (align_log2 = CHUNK_SIZE_LOG2;
	     align > (size_t)(1 << align_log2); align_log2++)
	    ;
        ptr = __libc_memalign(align, alloc_size);
    } else {
        align_log2 = 0;
	ptr = mem_get_memory(0, &alloc_size, &ispool, &pslot);	// alloc_size may be
						// modified
    }
    if (ptr) {
	data = (char *)ptr + (align ? (n * align) : sizeof(mem_alloc_hdr_t));
	mh = (mem_alloc_hdr_t *)(data - sizeof(mem_alloc_hdr_t));
	mh->magicno = MH_MAGIC;
	mh->type = type;
	mh->size_chunk_units = (size_t)(alloc_size >> CHUNK_SIZE_LOG2);
	if (align_log2 >= MH_MAX_ALIGN_VAL)
	    assert(0);
	mh->align_log2 = align_log2;
	mh->align_units = n;
	mh->flags = ispool;
	mh->pslot = pslot;

	// Update stats
	if (type < nkn_malloc_max_mem_types) {
	    AO_fetch_and_add(obj_type_data[type].cnt, alloc_size);
	    *obj_type_data[type].obj_size = alloc_size;
	    if (ispool)
		AO_fetch_and_add1(obj_type_data[type].nlib);
	    else {
		AO_fetch_and_add1(obj_type_data[type].glib);
	    }
	}

	return MHDR_TO_MEM(mh);
    } else {
	return 0;
    }
}

static void
int_nkn_memalloc_free(void *ptr)
{
    mem_alloc_hdr_t *mh;
    char *data;

    if (!ptr) {
        return;
    }

    mh = MEM_TO_MHDR(ptr);
    if (mh->magicno == MH_MAGIC) {
	// Update stats
	if (mh->type < nkn_malloc_max_mem_types) {
	    AO_fetch_and_add(obj_type_data[mh->type].cnt,
			    -(size_t)(mh->size_chunk_units << CHUNK_SIZE_LOG2));
	}

	data = MHDR_TO_MEM_START(mh);
	mh->magicno = MH_MAGIC_FREE;
	mem_return_memory(data,
			  (size_t)(mh->size_chunk_units << CHUNK_SIZE_LOG2),
			  mh);
    } else {
	/*
	 * Invalid header, should never happen.
	 * Note condition and leak memory.
	 */
	glob_nkn_memalloc_bad_free++;
    }
}

/*
 *******************************************************************************
 * Nokeena memory allocation functions
 *******************************************************************************
 */
void *
nkn_malloc_type(size_t size,
		nkn_obj_type_t type)
{

    return int_nkn_memalloc_malloc(size, 0, type);
}

void *
nkn_realloc_type(void *ptr,
		 size_t size,
		 nkn_obj_type_t type)
{
    mem_alloc_hdr_t *mh;
    size_t alloc_size;
    void *new_data;

    if (!ptr) {
	return int_nkn_memalloc_malloc(size, 0, type);
    }

    if (!size) {
	int_nkn_memalloc_free(ptr);
	return NULL;
    }
    mh = MEM_TO_MHDR(ptr);
    if (mh->magicno != MH_MAGIC) {
	/*
	 * Invalid header, should never happen.
	 * Note condition and just realloc.
	 */
	AO_fetch_and_add(&glob_nkn_memalloc_bad_realloc, 1);
        return __libc_realloc(ptr, size);
    }

    alloc_size = ALLOC_SIZE(size, 0);

    if (alloc_size > (size_t)(mh->size_chunk_units << CHUNK_SIZE_LOG2)) {
	/*
	 * We try to get the new buffer from our memory pool.
	 * then we copy the data.
	 */
	new_data = int_nkn_memalloc_malloc(size, 0, type);
	if (new_data == NULL) {
		/* according to realloc spec, old pointer is left untouched */
		return NULL;
	}
	if (new_data == ptr) {
		/* Same location. Why? OS is capable to expand the size? */
		return ptr;
	}

	/* copy data first */
	memcpy( (char *)new_data, (char *)ptr,
		(size_t)((mh->size_chunk_units << CHUNK_SIZE_LOG2) -
			sizeof(mem_alloc_hdr_t)));

	/* Time to free old pointer */
	int_nkn_memalloc_free(ptr);

	return new_data;
    }

    /* size shrinking, don't do anything */
    return ptr;
}

void *nkn_calloc_type(size_t n, size_t size, nkn_obj_type_t type)
{
    void *data;

    data = int_nkn_memalloc_malloc((n * size), 0, type);
    if (data) {
	memset(data, 0, (n * size));
    }
    return data;
}

void *nkn_memalign_type(size_t align, size_t s, nkn_obj_type_t type)
{
    if (!align || (align % sizeof(void*)) || (align % 2)) {
	return 0;
    }

    return int_nkn_memalloc_malloc(s, align, type);
}

void *nkn_valloc_type(size_t size, nkn_obj_type_t type)
{
    return int_nkn_memalloc_malloc(size, PAGE_SIZE, type);
}

void *nkn_pvalloc_type(size_t size, nkn_obj_type_t type)
{
    return int_nkn_memalloc_malloc(size, PAGE_SIZE, type);
}

int nkn_posix_memalign_type(void **r, size_t align, size_t size,
			    nkn_obj_type_t type)
{
    void *rp;
    if (!align || (align % sizeof(void*)) || (align % 2))
    {
	return EINVAL;
    }
    rp = int_nkn_memalloc_malloc(size, align, type);
    *r = rp;
    return (rp ? 0 : ENOMEM);
}

char *nkn_strdup_type(const char *s, nkn_obj_type_t type)
{
    size_t len;
    char *new_str = NULL;

    len = strlen(s) + 1;
    new_str = int_nkn_memalloc_malloc(len, 0, type);
    if (new_str)
	memcpy(new_str, s, len);

    return new_str;
}

/*
 *******************************************************************************
 * clib memory allocation functions
 *******************************************************************************
 */
void *malloc(size_t size)
{
    return int_nkn_memalloc_malloc(size, 0, mod_unknown);
}

void free(void *ptr)
{
    return int_nkn_memalloc_free(ptr);
}

void *realloc(void *ptr, size_t size)
{
    return nkn_realloc_type(ptr, size, mod_unknown);
}

void *calloc(size_t n, size_t size)
{
    return nkn_calloc_type(n, size, mod_unknown);
}

void cfree(void *ptr)
{
    return int_nkn_memalloc_free(ptr);
}

void *memalign(size_t align, size_t s)
{
    return nkn_memalign_type(align, s, mod_unknown);
}

void *valloc(size_t size)
{
    return nkn_valloc_type(size, mod_unknown);
}

void *pvalloc(size_t size)
{
    return nkn_pvalloc_type(size, mod_unknown);
}

int posix_memalign(void **r, size_t align, size_t size)
{
    return nkn_posix_memalign_type(r, align, size, mod_unknown);
}

char *strdup(const char *s)
{
    return nkn_strdup_type(s, mod_unknown);
}

////////////////////////////////////////////////////////////////////////////////
#else /* !ENABLE_INSTRUMENTED_MALLOC */
////////////////////////////////////////////////////////////////////////////////

void *nkn_malloc_type(size_t size,
		      nkn_obj_type_t type __attribute((unused)))
{
    return malloc(size);
}

void *nkn_realloc_type(void *ptr,
		       size_t size,
		       nkn_obj_type_t type __attribute((unused)))
{
    return realloc(ptr, size);
}

void *nkn_calloc_type(size_t n,
		      size_t size,
		      nkn_obj_type_t type __attribute((unused)))
{
    return calloc(n, size);
}

void *nkn_memalign_type(size_t align,
			size_t s,
			nkn_obj_type_t type __attribute((unused)))
{
    return memalign(align, s);
}

void *nkn_valloc_type(size_t size,
		      nkn_obj_type_t type __attribute((unused)))
{
    return valloc(size);
}

void *nkn_pvalloc_type(size_t size,
		       nkn_obj_type_t type __attribute((unused)))
{
    return pvalloc(size);
}

int nkn_posix_memalign_type(void **r,
			    size_t align,
			    size_t size,
                            nkn_obj_type_t type __attribute((unused)))
{
    return posix_memalign(r, align, size);
}

char *nkn_strdup_type(const char *s,
		      nkn_obj_type_t type __attribute((unused)))
{
    return strdup(s);
}

#define UNUSED_ARGUMENT(x) (void)x

void
nkn_mem_create_thread_pool(const char *pname)
{
	UNUSED_ARGUMENT(pname);
	return;
}

void
nkn_init_pool_mem(void)
{
	return;
}

#endif /* !ENABLE_INSTRUMENTED_MALLOC */

/*
 * End of nkn_memalloc.c
 */
