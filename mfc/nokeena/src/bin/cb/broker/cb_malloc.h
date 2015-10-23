/*
 * cb_malloc.h -- Content Broker memory allocation interfaces
 */
#ifndef _CB_MALLOC_H
#define _CB_MALLOC_H

#include <sys/types.h>

void *cb_malloc(size_t size);
void *cb_calloc(size_t nmemb, size_t size);
void *cb_realloc(void *ptr, size_t size);
void *cb_memalign(size_t align, size_t s);
void *cb_pvalloc(size_t size);
void *cb_valloc(size_t size);
void cb_free(void *ptr);

char *cb_strdup(const char *s);

#define MALLOC(_size) cb_malloc((_size))
#define CALLOC(_num, _size) cb_calloc((_num), (_size))
#define REALLOC(_ptr, _size) cb_realloc((_ptr), (_size))
#define MEMALIGN(_align, _size) cb_memalign((_align), (_size))
#define PVALLOC(_size) cb_pvalloc((_size))
#define VALLOC(_size) cb_valloc((_size))
#define FREE(_ptr) cb_free((_ptr))

#define STRDUP(_ptr) cb_strdup((_ptr))
#endif /* _CB_MALLOC_H */

/*
 * End of cb_malloc.h
 */
