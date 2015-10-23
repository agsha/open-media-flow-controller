#ifndef DNS_REF_COUNT_MEM_H
#define DNS_REF_COUNT_MEM_H


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

    struct ref_count_mem;

    typedef int32_t (*ref_cont_fptr)(struct ref_count_mem *ref_mem); 
    typedef void (*destroy_ref_cont_fptr)(struct ref_count_mem *ref_mem); 

    typedef uint32_t (*get_ref_count_fptr)(struct ref_count_mem* ref_mem);

    typedef void (*destroy_ref_mem_fptr)(void* mem);

    typedef struct ref_count_mem {

	void *mem;	// memory region to be ref counted
	destroy_ref_mem_fptr destroy_ref_mem;// destroy func for the mem region

	uint32_t ref_count;
	pthread_mutex_t ref_cont_lock;

	ref_cont_fptr hold_ref_cont;
	ref_cont_fptr release_ref_cont;
	get_ref_count_fptr get_ref_count;
	destroy_ref_cont_fptr destroy_ref_cont;

    } ref_count_mem_t;

    ref_count_mem_t* createRefCountedContainer(void *mem,
	    destroy_ref_mem_fptr destroy_ref_mem);


#ifdef __cplusplus
}
#endif

#endif

