/*
 * cb_malloc.c -- Content Broker memory allocation interfaces
 */
#include "cb_malloc.h"
#include <string.h>

extern void *__libc_malloc(size_t size);
extern void  __libc_free(void *ptr);
extern void *__libc_realloc(void *ptr, size_t size);
extern void *__libc_calloc(size_t n, size_t size);
extern void  __libc_cfree(void *ptr);
extern void *__libc_memalign(size_t align, size_t s);
extern void *__libc_valloc(size_t size);
extern void *__libc_pvalloc(size_t size);

void *cb_malloc(size_t size)
{
    return __libc_malloc(size);
}

void *cb_calloc(size_t nmemb, size_t size)
{
    return __libc_calloc(nmemb, size);
}

void *cb_realloc(void *ptr, size_t size)
{
    return __libc_realloc(ptr, size);
}

void *cb_memalign(size_t align, size_t size)
{
    return __libc_memalign(align, size);
}

void *cb_pvalloc(size_t size)
{
    return __libc_pvalloc(size);
}

void *cb_valloc(size_t size)
{
    return __libc_valloc(size);
}

void cb_free(void *ptr)
{
    return __libc_free(ptr);
}

char *cb_strdup(const char *s)
{
    size_t len;
    char *new_str = NULL;

    len = strlen(s) + 1;
    new_str = cb_malloc(len);
    if (new_str) {
        memcpy(new_str, s, len);
    }
    return new_str;
}

/*
 * End of cb_malloc.c
 */
