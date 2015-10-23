/**
 * @file   cr_free_list.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Wed Feb 29 02:03:47 2012
 * 
 * @brief  
 * 
 * 
 */
#ifndef _CR_FREE_LIST_
#define _CR_FREE_LIST_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>

#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_memalloc.h"

#ifdef __cplusplus
extern "C" {
#endif 

#if 0 //keeps emacs happy
}
#endif

#define E_OBJ_LIST_EMPTY 1000

typedef struct tag_obj_list_elm {
    void *obj;
    TAILQ_ENTRY(tag_obj_list_elm) entries;
} obj_list_elm_t;
TAILQ_HEAD(tag_obj_list_t, tag_obj_list_elm);
typedef struct tag_obj_list_t obj_list_t;

struct tag_obj_list_ctx;
typedef int32_t (*obj_list_delete_fnp)(struct tag_obj_list_ctx *);
typedef int32_t (*obj_list_get_free_obj_fnp)(
		     struct tag_obj_list_ctx *, void **); 
typedef int32_t (*obj_list_return_obj_fnp)(
		     struct tag_obj_list_ctx *, void *); 
typedef void * (*obj_create_fnp)(void);
typedef void (*obj_delete_fnp)(void *);

typedef struct tag_obj_list_ctx {
    obj_list_t *free_list;
    pthread_mutex_t lock;
    AO_t tot_elm;
    AO_t num_free_elm;
    obj_delete_fnp obj_delete;
    obj_list_delete_fnp delete;
    obj_list_get_free_obj_fnp pop_free;
    obj_list_return_obj_fnp push_free;
} obj_list_ctx_t;

int32_t obj_list_create(int32_t num_objs, obj_create_fnp create_obj,
			obj_delete_fnp delete_obj, obj_list_ctx_t **out);

#ifdef __cplusplus
}
#endif

#endif //_CR_FREE_LIST_
