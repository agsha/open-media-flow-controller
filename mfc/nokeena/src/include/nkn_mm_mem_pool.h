/*
  COPYRIGHT: ANKEENA NETWORKS
  AUTHOR: Jeya ganesh babu J
*/

#ifndef NKN_MM_MEM_POOL_H
#define NKN_MM_MEM_POOL_H

#include <sys/queue.h>

typedef struct nkn_mm_malloc_list {
    void    *mem_ptr;
    TAILQ_ENTRY(nkn_mm_malloc_list) free_list_entry;
}nkn_mm_malloc_list_t;

typedef struct nkn_mm_malloc {
    void			*mem_ptr;
    uint64_t			mem_size;
    int				num_slots;
    int				used;
    pthread_mutex_t		mem_mutex;
    nkn_mm_malloc_list_t	*list_ptr;
    TAILQ_HEAD(nkn_mm_malloc_free_q, nkn_mm_malloc_list) mm_malloc_free_q;
}nkn_mm_malloc_t;

#define NUM_MM_MEM_SLOTS 100
nkn_mm_malloc_t *nkn_mm_mem_pool_init(int num_slots, uint64_t size,
				      nkn_obj_type_t obj_type);
void *nkn_mm_get_memory(nkn_mm_malloc_t *mm_alloc_objp);
void nkn_mm_free_memory(nkn_mm_malloc_t *mm_alloc_objp, void *mem_ptr);
void nkn_mm_mem_pool_cleanup(nkn_mm_malloc_t *mem_alloc_objp);

#endif /* NKN_MM_MEM_POOL_H */

