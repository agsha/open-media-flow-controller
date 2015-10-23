The functions in this file are no longer used.  (miken: 12/22/2009)

/*
 * mem_mgr.c -- Intercept memory allocation called.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "nkn_elf.h"
#include "nkn_stat.h"

NKNCNT_DEF(malloc_called, uint64_t, "", "numof malloc called")
NKNCNT_DEF(calloc_called, uint64_t, "", "numof calloc called")
NKNCNT_DEF(realloc_called, uint64_t, "", "numof realloc called")
NKNCNT_DEF(free_called, uint64_t, "", "numof free called")
NKNCNT_DEF(strdup_called, uint64_t, "", "numof free called")

typedef void * (* FUNC_malloc)(size_t size);
typedef void * (* FUNC_calloc)(size_t nmemb, size_t size);
typedef void * (* FUNC_realloc)(void *ptr, size_t size);
typedef void (* FUNC_free)(void *ptr);
typedef char * (* FUNC_strdup)(const char *s);

static FUNC_malloc psys_malloc = NULL;
static FUNC_calloc psys_calloc = NULL;
static FUNC_realloc psys_realloc = NULL;
static FUNC_free psys_free = NULL;
static FUNC_strdup psys_strdup = NULL;

void * nkn_malloc(size_t size);
void * nkn_calloc(size_t n, size_t size);
void * nkn_realloc(void *ptr, size_t size);
void nkn_free(void *ptr);
char * nkn_strdup(const char *s);
void init_mem_counters(void);

void * nkn_malloc(size_t size)
{
    glob_malloc_called++;
    return (*psys_malloc) (size);
}

void * nkn_calloc(size_t n, size_t size)
{
    glob_calloc_called++;
    return (*psys_calloc) (n, size);
}

void * nkn_realloc(void *ptr, size_t size)
{
    glob_realloc_called++;
    return (*psys_realloc) (ptr, size);
}

void nkn_free(void *ptr)
{
    glob_free_called++;
    return (*psys_free) (ptr);
}

char * nkn_strdup(const char *s)
{
    glob_strdup_called++;
    return (*psys_strdup) (s);
}

void init_mem_counters(void)
{
	psys_malloc = (FUNC_malloc)nkn_replace_func("malloc", (void *)nkn_malloc);
	psys_calloc = (FUNC_calloc)nkn_replace_func("calloc", (void *)nkn_calloc);
	psys_realloc = (FUNC_realloc)nkn_replace_func("realloc", (void *)nkn_realloc);
	psys_free = (FUNC_free)nkn_replace_func("free", (void *)nkn_free);
	psys_strdup = (FUNC_strdup)nkn_replace_func("strdup", (void *)nkn_strdup);
}

