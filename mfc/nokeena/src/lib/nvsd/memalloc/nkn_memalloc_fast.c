/*
 * nkn_memalloc_fast.c -- Intercept untyped memory allocation calls.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <atomic_ops.h>
#include <pthread.h>

extern void *__libc_malloc(size_t size);
extern void  __libc_free(void* ptr);
extern void *__libc_realloc(void* ptr, size_t size);
extern void *__libc_calloc(size_t n, size_t size);
extern void  __libc_cfree(void* ptr);
extern void *__libc_memalign(size_t align, size_t s);
extern void *__libc_valloc(size_t size);
extern void *__libc_pvalloc(size_t size);

void *memalign(size_t align, size_t s);
void *pvalloc(size_t size);

/*
 *******************************************************************************
 * Allocate glob_xxx call counters
 *******************************************************************************
 */
u_int64_t glob_malloc_calls;
u_int64_t glob_free_calls;
u_int64_t glob_realloc_calls;
u_int64_t glob_calloc_calls;
u_int64_t glob_cfree_calls;
u_int64_t glob_memalign_calls;
u_int64_t glob_valloc_calls;
u_int64_t glob_pvalloc_calls;
u_int64_t glob_posix_memalign_calls;
u_int64_t glob_strdup_calls;

u_int64_t glob_nkn_malloc_calls;
u_int64_t glob_nkn_realloc_calls;
u_int64_t glob_nkn_calloc_calls;
u_int64_t glob_nkn_memalign_calls;
u_int64_t glob_nkn_valloc_calls;
u_int64_t glob_nkn_pvalloc_calls;
u_int64_t glob_nkn_posix_memalign_calls;
u_int64_t glob_nkn_strdup_calls;

u_int64_t glob_nkn_memalloc_bad_free;
u_int64_t glob_nkn_memalloc_bad_realloc;

/*
 *******************************************************************************
 * Object stat definitions
 *******************************************************************************
 */
typedef enum {
    unknown
} nkn_obj_type_t;

typedef struct object_data {
    nkn_obj_type_t type;
    uint32_t *obj_size;
    const char *name;
    AO_t *cnt;
    AO_t *glib;
    AO_t *nlib;
} object_data_t;

static unsigned int nkn_malloc_max_mem_types = 1;
static AO_t unknown_obj_cnt;
static object_data_t obj_type_data[1] = {
    {unknown, 0, "unknown", &unknown_obj_cnt, 0, 0}
};

/*
 *******************************************************************************
 * Retain memory released via free() for fast allocation
 *******************************************************************************
 */
struct free_queue {
    struct free_queue * next;
};

typedef struct mem_slot_queue {
    size_t 		size;
    uint32_t 		tot_buf_in_this_slot;
    uint32_t 		max_bufs;
    pthread_mutex_t 	mutex;
    struct free_queue 	*freeq;
} mem_slot_queue_t;

#define SIZE_MAX_SLOT	(mem_slot_q[MAX_SLOTS-1].size)

#ifdef _TARGET_CMM
// MAX_SLOTS where each slot is a pwr2 of POOL_ELEMENT_SIZE
#define POOL_ELEMENT_SIZE_PWR2 7
#define POOL_ELEMENT_SIZE (1 << POOL_ELEMENT_SIZE_PWR2)

#define C_64MB_PWR2 	26
#define MAX_SLOTS	14

static mem_slot_queue_t mem_slot_q[MAX_SLOTS]= { // 64MB max per list
    {(POOL_ELEMENT_SIZE << 0), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 0)),
     PTHREAD_MUTEX_INITIALIZER, NULL},

    {(POOL_ELEMENT_SIZE << 1), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 1)),
     PTHREAD_MUTEX_INITIALIZER, NULL},

    {(POOL_ELEMENT_SIZE << 2), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 2)),
     PTHREAD_MUTEX_INITIALIZER, NULL},

    {(POOL_ELEMENT_SIZE << 3), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 3)),
     PTHREAD_MUTEX_INITIALIZER, NULL},

    {(POOL_ELEMENT_SIZE << 4), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 4)),
     PTHREAD_MUTEX_INITIALIZER, NULL},

    {(POOL_ELEMENT_SIZE << 5), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 5)),
     PTHREAD_MUTEX_INITIALIZER, NULL},

    {(POOL_ELEMENT_SIZE << 6), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 6)),
     PTHREAD_MUTEX_INITIALIZER, NULL},

    {(POOL_ELEMENT_SIZE << 7), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 7)),
     PTHREAD_MUTEX_INITIALIZER, NULL},

    {(POOL_ELEMENT_SIZE << 8), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 8)),
     PTHREAD_MUTEX_INITIALIZER, NULL},

    {(POOL_ELEMENT_SIZE << 9), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 9)),
     PTHREAD_MUTEX_INITIALIZER, NULL},

    {(POOL_ELEMENT_SIZE << 10), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 10)),
     PTHREAD_MUTEX_INITIALIZER, NULL},

    {(POOL_ELEMENT_SIZE << 11), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 11)),
     PTHREAD_MUTEX_INITIALIZER, NULL},

    {(POOL_ELEMENT_SIZE << 12), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 12)),
     PTHREAD_MUTEX_INITIALIZER, NULL},

    {(POOL_ELEMENT_SIZE << 13), 0,
     (1 << (C_64MB_PWR2 - POOL_ELEMENT_SIZE_PWR2 - 13)),
     PTHREAD_MUTEX_INITIALIZER, NULL},
};
#endif /* _TARGET_CMM */

static mem_slot_queue_t *convert_alloc_size(size_t align, size_t alloc_size, 
					    size_t *new_alloc_size)
{
    int i;
    mem_slot_queue_t *pslot;

    if (align || (alloc_size > SIZE_MAX_SLOT)) {
	return NULL;
    }
    pslot = &mem_slot_q[0];

    for (i = 0; i < MAX_SLOTS; i++) {
	if (alloc_size > pslot[i].size) {
	    continue;
	}
	*new_alloc_size = pslot[i].size;
	return &pslot[i];
    }
    return NULL;
}

static void *mem_get_memory(size_t align, size_t alloc_size)
{
    struct free_queue *pfreeq;
    mem_slot_queue_t *pslot;
    size_t new_alloc_size;

    new_alloc_size = alloc_size;
    pslot = convert_alloc_size(align, alloc_size, &new_alloc_size);
    if (pslot) {
	pthread_mutex_lock(&pslot->mutex);
	pfreeq = pslot->freeq;
	if (pfreeq != NULL) {
	    pslot->freeq = pfreeq->next;
	    pslot->tot_buf_in_this_slot--;
	    pthread_mutex_unlock(&pslot->mutex);
	    return (void *)pfreeq;
	}
	pthread_mutex_unlock(&pslot->mutex);
    }

    if (align) {
	return __libc_memalign(align, new_alloc_size);
    } else {
	return __libc_malloc(new_alloc_size);
    }
}

static void mem_return_memory(void * data, size_t align, size_t alloc_size)
{
    struct free_queue *pfreeq;
    mem_slot_queue_t *pslot;
    size_t new_alloc_size;

    pslot = convert_alloc_size(align, alloc_size, &new_alloc_size);
    if (pslot && (pslot->tot_buf_in_this_slot < pslot->max_bufs)) {
	// insert into freeq
	pthread_mutex_lock(&pslot->mutex);
	pfreeq = (struct free_queue *)data;
	pfreeq->next = pslot->freeq;
	pslot->freeq = pfreeq;
	pslot->tot_buf_in_this_slot++;
	pthread_mutex_unlock(&pslot->mutex);
	return;
    }
    __libc_free(data);
    return;
}
 
/*
 *******************************************************************************
 * malloc'ed memory header definitions
 *******************************************************************************
 */
typedef struct mem_alloc_hdr { // 16 bytes, must be multiple of 8
    u_int32_t magicno;
    u_int32_t type;
    u_int32_t size_chunk_units;
    u_int16_t align_log2;
    u_int8_t  align_units; // (units*(2**align_pwr_2)) >= sizeof(mem_alloc_hdr)
    u_int8_t  flags;
} mem_alloc_hdr_t;

#define MH_MAGIC 0x2359abef
#define MH_MAGIC_FREE 0xccccdddd

#define CHUNK_SIZE_LOG2 3
#define CHUNK_SIZE (size_t)(1 << CHUNK_SIZE_LOG2)

#define MEM_TO_MHDR(_ptr) \
	((mem_alloc_hdr_t *)((char *)(_ptr) - sizeof(mem_alloc_hdr_t)))

#define MHDR_TO_MEM(_ptr) \
	((char *)((char *)(_ptr) + sizeof(mem_alloc_hdr_t)))

#define MHDR_TO_MEM_START(_mh) \
	((_mh)->align_units ? \
	 (char *)((char *)(_mh) + sizeof(*(_mh))) - \
		((size_t)(1 << (_mh)->align_log2) * (_mh)->align_units) : \
			(char *)((_mh)))

#define ROUNDUP(x, y) ((((x)+((y)-1))/(y))*(y))

#define ALLOC_SIZE(_bytes, _align) \
	ROUNDUP((sizeof(mem_alloc_hdr_t) + \
		 ROUNDUP((_bytes), sizeof(void *)) + (_align)), CHUNK_SIZE)

/*
 *******************************************************************************
 * Internal functions
 *******************************************************************************
 */
static void *int_nkn_memalloc_malloc(size_t size, size_t align, 
				     nkn_obj_type_t type)
{
    size_t alloc_size;
    void *ptr;
    char *data;
    mem_alloc_hdr_t *mh;
    int align_log2;
    int n;

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
    	ptr = mem_get_memory(0, alloc_size);
    }

    if (ptr) {
	data = (char *)ptr + (align ? (n * align) : sizeof(mem_alloc_hdr_t));
	mh = (mem_alloc_hdr_t *)(data - sizeof(mem_alloc_hdr_t));
	mh->magicno = MH_MAGIC;
	mh->type = type;
	mh->size_chunk_units = (size_t)(alloc_size >> CHUNK_SIZE_LOG2);
	mh->align_log2 = align_log2;
	mh->align_units = n;
	mh->flags = 0;

    	// Update stats
	if (type < nkn_malloc_max_mem_types) {
	    (*obj_type_data[type].cnt) += alloc_size;
	}

	return MHDR_TO_MEM(mh);
    } else {
    	return 0;
    }
}

static void int_nkn_memalloc_free(void *ptr)
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
	    (*obj_type_data[mh->type].cnt) -= 
	    	(size_t)(mh->size_chunk_units << CHUNK_SIZE_LOG2);
	}

	data = MHDR_TO_MEM_START(mh);
    	mh->magicno = MH_MAGIC_FREE;
	mem_return_memory(data, mh->align_log2, 
			  (size_t)(mh->size_chunk_units << CHUNK_SIZE_LOG2));
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
void *nkn_malloc_type(size_t size, nkn_obj_type_t type);
void *nkn_malloc_type(size_t size, nkn_obj_type_t type)
{
    glob_nkn_malloc_calls++;

    return int_nkn_memalloc_malloc(size, 0, type);
}

void *nkn_realloc_type(void *ptr, size_t size, nkn_obj_type_t type);
void *nkn_realloc_type(void *ptr, size_t size, nkn_obj_type_t type)
{
    mem_alloc_hdr_t *mh;
    size_t alloc_size;
    void *data;
    void *new_data;
    size_t new_alloc_size;
    mem_slot_queue_t * pslot;

    glob_nkn_realloc_calls++;

    if (!ptr) {
    	return int_nkn_memalloc_malloc(size, 0, type);
    }

    if (!size) {
    	int_nkn_memalloc_free(ptr);
	return 0;
    }
    mh = MEM_TO_MHDR(ptr);
    if (mh->magicno != MH_MAGIC) {
	/*
    	 * Invalid header, should never happen.
	 * Note condition and just realloc.
	 */
	glob_nkn_memalloc_bad_realloc++;
        return __libc_realloc(ptr, size);
    }
    data = MHDR_TO_MEM_START(mh);

    alloc_size = ALLOC_SIZE(size, 0);
    pslot = convert_alloc_size(0, alloc_size, &new_alloc_size);
    if(pslot) {
	alloc_size = new_alloc_size;
    }
    
    if (alloc_size > (size_t)(mh->size_chunk_units << CHUNK_SIZE_LOG2)) {
	new_data = int_nkn_memalloc_malloc(size, 0, type);
	if (new_data) {
	    memcpy((char *)new_data, (char *)ptr,
		   (size_t)((mh->size_chunk_units << CHUNK_SIZE_LOG2) -
			    sizeof(mem_alloc_hdr_t)));
	    int_nkn_memalloc_free(ptr);
	    return new_data;
	} else {
	    return NULL;
	}
    }
    return ptr;
}

void *nkn_calloc_type(size_t n, size_t size, nkn_obj_type_t type);
void *nkn_calloc_type(size_t n, size_t size, nkn_obj_type_t type)
{
    glob_nkn_calloc_calls++;
    void *data;

    data = int_nkn_memalloc_malloc((n * size), 0, type);
    if (data) {
    	memset(data, 0, (n * size));
    }
    return data;
}

void *nkn_memalign_type(size_t align, size_t s, nkn_obj_type_t type);
void *nkn_memalign_type(size_t align, size_t s, nkn_obj_type_t type)
{
    glob_nkn_memalign_calls++;
    if (!align || (align % sizeof(void*)) || (align % 2)) {
    	return 0;
    }

    return int_nkn_memalloc_malloc(s, align, type);
}

void *nkn_valloc_type(size_t size, nkn_obj_type_t type);
void *nkn_valloc_type(size_t size, nkn_obj_type_t type)
{
    glob_nkn_valloc_calls++;

    return int_nkn_memalloc_malloc(size, PAGE_SIZE, type);
}

void *nkn_pvalloc_type(size_t size, nkn_obj_type_t type);
void *nkn_pvalloc_type(size_t size, nkn_obj_type_t type)
{
    glob_nkn_pvalloc_calls++;

    return int_nkn_memalloc_malloc(size, PAGE_SIZE, type);
}

int nkn_posix_memalign_type(void **r, size_t align, size_t size, 
		       	    nkn_obj_type_t type);
int nkn_posix_memalign_type(void **r, size_t align, size_t size, 
		       	    nkn_obj_type_t type)
{
    void *rp;

    glob_posix_memalign_calls++;
    if (!align || (align % sizeof(void*)) || (align % 2))
    {
	return EINVAL;
    }
    rp = int_nkn_memalloc_malloc(size, align, type);
    *r = rp;
    return (rp ? 0 : ENOMEM);
}

char *nkn_strdup_type(const char *s, nkn_obj_type_t type);
char *nkn_strdup_type(const char *s, nkn_obj_type_t type)
{
    size_t len;
    char *new_str;

    glob_nkn_strdup_calls++;

    len = strlen(s) + 1;
    new_str = int_nkn_memalloc_malloc(len, 0, type);
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
    glob_malloc_calls++;

    return nkn_malloc_type(size, unknown);
}

void free(void *ptr)
{
    glob_free_calls++;

    return int_nkn_memalloc_free(ptr);
}

void *realloc(void *ptr, size_t size)
{
    glob_realloc_calls++;

    return nkn_realloc_type(ptr, size, unknown);
}

void *calloc(size_t n, size_t size)
{
    glob_calloc_calls++;

    return nkn_calloc_type(n, size, unknown);
}

void cfree(void *ptr)
{
    glob_cfree_calls++;

    return int_nkn_memalloc_free(ptr);
}

void *memalign(size_t align, size_t s)
{
    glob_memalign_calls++;

    return nkn_memalign_type(align, s, unknown);
}

void *valloc(size_t size)
{
    glob_valloc_calls++;

    return nkn_valloc_type(size, unknown);
}

void *pvalloc(size_t size)
{
    glob_pvalloc_calls++;

    return nkn_pvalloc_type(size, unknown);
}

int posix_memalign(void **r, size_t align, size_t size)
{
    glob_posix_memalign_calls++;

    return nkn_posix_memalign_type(r, align, size, unknown);
}

char *strdup(const char *s)
{
    glob_strdup_calls++;

    return nkn_strdup_type(s, unknown);
}

/*
 * End of nkn_memalloc_fast.c
 */
