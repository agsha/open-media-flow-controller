/*
 * memalloc.h - Library memory allocation
 */
#ifndef _MEMALLOC_H
#define _MEMALLOC_H

typedef enum nkn_obj_type {
#define OBJ_TYPE(_type) _type,
#include "mem_object_types.h"
} nkn_obj_type_t;

extern void *(*phttp_malloc_type)(size_t size, nkn_obj_type_t type);
extern void *(*phttp_realloc_type)(void *ptr, size_t size, nkn_obj_type_t type);
extern void *(*phttp_calloc_type)(size_t n, size_t size, nkn_obj_type_t type);
extern void *(*phttp_memalign_type)(size_t align, size_t s, 
				    nkn_obj_type_t type);
extern void *(*phttp_valloc_type)(size_t size, nkn_obj_type_t type);
extern void *(*phttp_pvalloc_type)(size_t size, nkn_obj_type_t type);
extern int (*phttp_posix_memalign_type)(void **r, size_t align, size_t size,
                            	       nkn_obj_type_t type);
extern char *(*phttp_strdup_type)(const char *s, nkn_obj_type_t type);
extern void (*phttp_free)(void *ptr);

#define nkn_malloc_type (*phttp_malloc_type)
#define nkn_realloc_type (*phttp_realloc_type)
#define nkn_calloc_type (*phttp_calloc_type)
#define nkn_memalign_type (*phttp_memalign_type)
#define nkn_valloc_type (*phttp_valloc_type)
#define nkn_pvalloc_type (*phttp_pvalloc_type)
#define nkn_posix_memalign_type (*phttp_posix_memalign_type)
#define nkn_strdup_type (*phttp_strdup_type)
#define free (*phttp_free)

#if 0
void *nkn_malloc_type(size_t size, nkn_obj_type_t type);
void *nkn_realloc_type(void *ptr, size_t size, nkn_obj_type_t type);
void *nkn_calloc_type(size_t n, size_t size, nkn_obj_type_t type);
void *nkn_memalign_type(size_t align, size_t s, nkn_obj_type_t type);
void *nkn_valloc_type(size_t size, nkn_obj_type_t type);
void *nkn_pvalloc_type(size_t size, nkn_obj_type_t type);
int nkn_posix_memalign_type(void **r, size_t align, size_t size,
                            nkn_obj_type_t type);
char *nkn_strdup_type(const char *s, nkn_obj_type_t type);
#endif

#endif /* _MEMALLOC_H */
