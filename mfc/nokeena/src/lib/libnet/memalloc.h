/*
 * memalloc.h - Library memory allocation
 */
#ifndef _MEMALLOC_H
#define _MEMALLOC_H

typedef enum nkn_obj_type {
#define OBJ_TYPE(_type) _type,
#include "mem_object_types.h"
} nkn_obj_type_t;

extern void *(*pnet_malloc_type)(size_t size, nkn_obj_type_t type);
extern void *(*pnet_realloc_type)(void *ptr, size_t size, nkn_obj_type_t type);
extern void *(*pnet_calloc_type)(size_t n, size_t size, nkn_obj_type_t type);
extern void *(*pnet_memalign_type)(size_t align, size_t s, 
				    nkn_obj_type_t type);
extern void *(*pnet_valloc_type)(size_t size, nkn_obj_type_t type);
extern void *(*pnet_pvalloc_type)(size_t size, nkn_obj_type_t type);
extern int (*pnet_posix_memalign_type)(void **r, size_t align, size_t size,
                            	       nkn_obj_type_t type);
extern char *(*pnet_strdup_type)(const char *s, nkn_obj_type_t type);
extern void (*pnet_free)(void *ptr);

#define nkn_malloc_type (*pnet_malloc_type)
#define nkn_realloc_type (*pnet_realloc_type)
#define nkn_calloc_type (*pnet_calloc_type)
#define nkn_memalign_type (*pnet_memalign_type)
#define nkn_valloc_type (*pnet_valloc_type)
#define nkn_pvalloc_type (*pnet_pvalloc_type)
#define nkn_posix_memalign_type (*pnet_posix_memalign_type)
#define nkn_strdup_type (*pnet_strdup_type)
#define free (*pnet_free)

#endif /* _MEMALLOC_H */
