/*
 * nkn_memalloc.h -- Instrumented memory allocation interfaces
 */

#ifndef _NKN_MEMALLOC_H
#define _NKN_MEMALLOC_H
#include <pthread.h>
#include <atomic_ops.h>
#include <sys/types.h>
#include <stdint.h>

#define MAX_SLOTS       256
#define INIT_SLOT_SHIFT 5
#define INIT_SLOT_SIZE  (1<<INIT_SLOT_SHIFT)            // 32 bytes
#define SIZE_MAX_SLOT   (INIT_SLOT_SIZE * (MAX_SLOTS))

#define SLOT_TO_SIZE(slot) (size_t)(INIT_SLOT_SIZE * (slot + 1))
#define SIZE_TO_SLOT(size, slot)        \
                slot = ((size - 1) >> INIT_SLOT_SHIFT);

#define CHUNK_SIZE_LOG2 3
#define CHUNK_SIZE (size_t)(1 << CHUNK_SIZE_LOG2)

struct free_queue {
    struct free_queue * next;
};

typedef struct mem_slot_queue {
    uint32_t            size;
    int32_t             tot_buf_in_this_slot;
    pthread_mutex_t     mutex;
    struct free_queue   *freeq;
    AO_t                hits, miss, full;
    uint32_t            flags;
    uint32_t            prealloc_count;
    int32_t             max_bufs;
    void                *prealloc_addr;
    uint64_t            free, hwm, cnt;
} mem_slot_queue_t;

/*
 * malloc'ed memory header definitions
 */
typedef struct mem_alloc_hdr { // 8 byte memory header
    u_int64_t magicno:12;      // 12 bit magic header
    u_int64_t type:11;         // 11 bit type to support 2K object types
    u_int64_t flags:1;          // 1 bit to point [memlib = 1 /glib = 0]
    u_int64_t size_chunk_units:34; //34 bit chunk_units to support 128G max alloc
    u_int64_t align_log2:4;    // 4 bit alignment field - max of 64MB alignment
    u_int64_t align_units:2;   //(units*(2**align_pwr_2))>=sizeof(mem_alloc_hdr)
    mem_slot_queue_t *pslot;
} mem_alloc_hdr_t;

#define ROUNDUP(x, y) ((((x)+((y)-1))/(y))*(y))
#define ALLOC_SIZE(_bytes, _align) \
        ROUNDUP((sizeof(mem_alloc_hdr_t) + \
                 ROUNDUP((_bytes), sizeof(void *)) + (_align)), CHUNK_SIZE)

/* Macro to caclulate the size ofthe buffer allocated */
#define NKN_SIZE(_bytes) \
   (((_bytes + sizeof(mem_alloc_hdr_t)) > SIZE_MAX_SLOT)?\
    (_bytes + sizeof(mem_alloc_hdr_t)):\
    (SLOT_TO_SIZE((((ALLOC_SIZE(_bytes,0)) - 1) >> INIT_SLOT_SHIFT))))

typedef enum nkn_obj_type {
#define OBJ_TYPE(_type) _type,
#include "nkn_mem_object_types.h"
} nkn_obj_type_t;

// Data to be supplied by instrumented binary
typedef struct object_data {
    nkn_obj_type_t type;
    uint32_t *obj_size;
    const char *name;
    AO_t *cnt;
    AO_t *glib;
    AO_t *nlib;
} object_data_t;

extern object_data_t obj_type_data[];
extern nkn_obj_type_t nkn_malloc_max_mem_types;

extern AO_t glob_malloc_calls;
extern AO_t glob_free_calls;
extern AO_t glob_realloc_calls;
extern AO_t glob_calloc_calls;
extern AO_t glob_cfree_calls;
extern AO_t glob_memalign_calls;
extern AO_t glob_valloc_calls;
extern AO_t glob_pvalloc_calls;
extern AO_t glob_posix_memalign_calls;
extern AO_t glob_strdup_calls;

extern AO_t glob_nkn_malloc_calls;
extern AO_t glob_nkn_realloc_calls;
extern AO_t glob_nkn_calloc_calls;
extern AO_t glob_nkn_memalign_calls;
extern AO_t glob_nkn_valloc_calls;
extern AO_t glob_nkn_pvalloc_calls;
extern AO_t glob_nkn_posix_memalign_calls;
extern AO_t glob_nkn_strdup_calls;

extern AO_t glob_nkn_memalloc_bad_free;
extern AO_t glob_nkn_memalloc_bad_realloc;

void *nkn_malloc_type(size_t size, nkn_obj_type_t type);
void *nkn_realloc_type(void *ptr, size_t size, nkn_obj_type_t type);
void *nkn_calloc_type(size_t n, size_t size, nkn_obj_type_t type);
void *nkn_memalign_type(size_t align, size_t s, nkn_obj_type_t type);
void *nkn_valloc_type(size_t size, nkn_obj_type_t type);
void *nkn_pvalloc_type(size_t size, nkn_obj_type_t type);
int nkn_posix_memalign_type(void **r, size_t align, size_t size,
			    nkn_obj_type_t type);
char *nkn_strdup_type(const char *s, nkn_obj_type_t type);

// Add a thread local memory pool for this thread.
void nkn_mem_create_thread_pool(const char *name);

void nkn_init_pool_mem(void);

#endif /* _NKN_MEMALLOC_H */

/*
 * End of nkn_memalloc.h
 */
