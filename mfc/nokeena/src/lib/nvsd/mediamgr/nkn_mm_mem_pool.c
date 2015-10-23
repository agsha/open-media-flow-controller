/*
  COPYRIGHT: ANKEENA NETWORKS
  AUTHOR: Jeya ganesh babu J
*/
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#ifdef HAVE_FLOCK
#  include <sys/file.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>
#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_errno.h"
#include "nkn_assert.h"
#include "nkn_mm_mem_pool.h"
#ifdef UNIT_TEST
#undef NKN_ASSERT
#define NKN_ASSERT assert
#define nkn_calloc_type(X, Y, Z) (void *)calloc(X, Y)
#define nkn_posix_memalign_type(X, Y, Z, A) posix_memalign(X, Y, Z)
#endif

AO_t glob_mm_pool_alloc;
AO_t glob_mm_pool_alloc_fail;

/* Initialization Routine -
 * Pre-allocate memory and store it in mem_alloc_objp
 * Allocate the free list pointers and store the memory offsets
 * Add the Free list pointers to the free list queue
 */
nkn_mm_malloc_t *nkn_mm_mem_pool_init(int num_slots, uint64_t size,
				      nkn_obj_type_t obj_type)
{
    nkn_mm_malloc_t *mem_alloc_objp;
    int		    ret;
    int		    idx;

    mem_alloc_objp = nkn_calloc_type(1, sizeof(nkn_mm_malloc_t),
					mod_mm_mem_pool_1);
    if(mem_alloc_objp == NULL) {
	return NULL;
    }

    mem_alloc_objp->num_slots = num_slots;
    mem_alloc_objp->mem_size  = size;

    ret = pthread_mutex_init(&mem_alloc_objp->mem_mutex, NULL);
    if(ret < 0) {
	free(mem_alloc_objp);
        return NULL;
    }

    ret = nkn_posix_memalign_type((void *)&mem_alloc_objp->mem_ptr, 4096,
				    num_slots * size,
				    obj_type);
    if(ret != 0) {
	free(mem_alloc_objp);
	return NULL;
    }

    TAILQ_INIT(&(mem_alloc_objp->mm_malloc_free_q));
    mem_alloc_objp->list_ptr = nkn_calloc_type(num_slots,
						sizeof(nkn_mm_malloc_list_t),
						mod_mm_mem_pool_2);
    if(mem_alloc_objp->list_ptr == NULL) {
	free(mem_alloc_objp);
	return NULL;
    }

    pthread_mutex_lock(&mem_alloc_objp->mem_mutex);
    for(idx=0; idx<num_slots; idx++) {
	mem_alloc_objp->list_ptr[idx].mem_ptr = (char *)mem_alloc_objp->mem_ptr
						     + (idx * size);
	TAILQ_INSERT_TAIL(&(mem_alloc_objp->mm_malloc_free_q),
			    &(mem_alloc_objp->list_ptr[idx]), free_list_entry);
    }
    pthread_mutex_unlock(&mem_alloc_objp->mem_mutex);
    return mem_alloc_objp;
}

/* Memory get function -
 * Take out the head of the free list queue and return the memory pointer
 * Remove the head from the queue
 */
void *nkn_mm_get_memory(nkn_mm_malloc_t *mem_alloc_objp)
{
    nkn_mm_malloc_list_t *free_ptr;

    pthread_mutex_lock(&mem_alloc_objp->mem_mutex);
    free_ptr = TAILQ_FIRST(&(mem_alloc_objp->mm_malloc_free_q));
    //NKN_ASSERT(free_ptr);
    if(!free_ptr) {
	pthread_mutex_unlock(&mem_alloc_objp->mem_mutex);
	AO_fetch_and_add1(&glob_mm_pool_alloc_fail);
	return NULL;
    }
    TAILQ_REMOVE(&(mem_alloc_objp->mm_malloc_free_q),
			    free_ptr, free_list_entry);
    pthread_mutex_unlock(&mem_alloc_objp->mem_mutex);
    AO_fetch_and_add1(&glob_mm_pool_alloc);
    return free_ptr->mem_ptr;
}

/* Free function -
 * Find the index from the memory address
 * Use the index to find the memory list pointer. Add this to the 
 * free list Queue.
 */
void nkn_mm_free_memory(nkn_mm_malloc_t *mem_alloc_objp, void *mem_ptr)
{
    int idx;

    memset(mem_ptr, 0, mem_alloc_objp->mem_size);
    pthread_mutex_lock(&mem_alloc_objp->mem_mutex);
    idx = ((char *)mem_ptr - (char *)mem_alloc_objp->mem_ptr)/mem_alloc_objp->mem_size;
    NKN_ASSERT(idx < mem_alloc_objp->num_slots);
    if(idx >= mem_alloc_objp->num_slots)
	return;
    TAILQ_INSERT_TAIL(&(mem_alloc_objp->mm_malloc_free_q),
			    &(mem_alloc_objp->list_ptr[idx]), free_list_entry);
    pthread_mutex_unlock(&mem_alloc_objp->mem_mutex);
    AO_fetch_and_sub1(&glob_mm_pool_alloc);
}

void nkn_mm_mem_pool_cleanup(nkn_mm_malloc_t *mem_alloc_objp)
{
    free(mem_alloc_objp->list_ptr);
    free(mem_alloc_objp->mem_ptr);
    free(mem_alloc_objp);
}

#ifdef UNIT_TEST
int main(void)
{
    int i;
    char *mem[100];
    nkn_mm_malloc_t *mem_alloc_objp;

    mem_alloc_objp = nkn_mm_mem_pool_init(100, 2 * 1024 * 1024, 0);
    for (i=0;i<100;i++) {
	mem[i] = nkn_mm_get_memory(mem_alloc_objp);
	if(!mem[i])
	    assert(0);
    }
    for (i=99;i>=0;i--)
	nkn_mm_free_memory(mem_alloc_objp, mem[i]);

    nkn_mm_mem_pool_cleanup(mem_alloc_objp);
}
#endif

