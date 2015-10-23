#ifndef LFD_UINT_CTX_CONT_H
#define LFD_UINT_CTX_CONT_H

#include <stdint.h>
#include <pthread.h>

struct uint_ctx_cont;

struct uint_ctx_cont* createUintCtxCont(uint32_t max_count);

typedef void (*uint_ctx_cont_delete_fptr)(struct uint_ctx_cont* cont);

typedef void (*uint_ctx_element_delete_fptr)(void* ctx);

typedef int32_t (*uint_ctx_element_add_fptr)(struct uint_ctx_cont* cont, 
		uint32_t uint, void* ctx, uint_ctx_element_delete_fptr ctx_delete_hdlr);

typedef void* (*uint_ctx_element_acquire_fptr)(struct uint_ctx_cont* cont, 
		uint32_t uint);

typedef int32_t (*uint_ctx_element_remove_fptr)(struct uint_ctx_cont* cont, 
		uint32_t uint);

typedef struct uint_ctx_cont {

	void** ctx_arr;
	uint_ctx_element_delete_fptr* ctx_delete_hdlr_arr;
	uint32_t max_count;

	pthread_mutex_t* element_locks;

	uint_ctx_element_acquire_fptr get_element_hdlr;
	uint_ctx_element_add_fptr add_element_hdlr;
	uint_ctx_element_acquire_fptr acquire_element_hdlr;
	uint_ctx_element_remove_fptr release_element_hdlr;
	uint_ctx_element_remove_fptr remove_element_hdlr;

	uint_ctx_cont_delete_fptr delete_hdlr;
} uint_ctx_cont_t;

#endif
