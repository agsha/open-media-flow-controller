#include "nkn_memalloc.h"
#include "nkn_ref_count_mem.h"


static int32_t holdRefCont(ref_count_mem_t *ref_cont);
static int32_t releaseRefCont(ref_count_mem_t *ref_cont);
static uint32_t getRefCount(ref_count_mem_t *ref_cont);
static void destroyRefCont(ref_count_mem_t *ref_cont);


ref_count_mem_t *createRefCountedContainer(void *mem,
	   destroy_ref_mem_fptr destroy_ref_mem) {

    ref_count_mem_t *ref_cont = 
	nkn_calloc_type(1, sizeof(ref_count_mem_t),
			mod_vpe_refcount_t);
    if (ref_cont == NULL)
	return NULL;

    ref_cont->mem = mem;
    ref_cont->destroy_ref_mem = destroy_ref_mem;

    ref_cont->ref_count = 0;
    pthread_mutex_init(&(ref_cont->ref_cont_lock), NULL);

    ref_cont->hold_ref_cont = holdRefCont;
    ref_cont->release_ref_cont = releaseRefCont;
    ref_cont->destroy_ref_cont = destroyRefCont;
    ref_cont->get_ref_count = getRefCount;
    return ref_cont;
}


static int32_t holdRefCont(ref_count_mem_t* ref_cont) {

    pthread_mutex_lock(&ref_cont->ref_cont_lock);
    ref_cont->ref_count++;
    pthread_mutex_unlock(&ref_cont->ref_cont_lock);
    return 1;
}


static int32_t releaseRefCont(ref_count_mem_t* ref_cont) {

    int32_t rc = 0;
    pthread_mutex_lock(&ref_cont->ref_cont_lock);
    ref_cont->ref_count--;
    if (ref_cont->ref_count == 0)
	rc = 1;
    pthread_mutex_unlock(&ref_cont->ref_cont_lock);
    if (rc > 0)
	ref_cont->destroy_ref_cont(ref_cont);
    return rc;
}


static void destroyRefCont(ref_count_mem_t *ref_cont) {

    if (ref_cont->destroy_ref_mem != NULL)
	ref_cont->destroy_ref_mem(ref_cont->mem);
    pthread_mutex_destroy(&ref_cont->ref_cont_lock);
    free(ref_cont);
}


static uint32_t getRefCount(ref_count_mem_t *ref_cont) {

    return ref_cont->ref_count;
}
