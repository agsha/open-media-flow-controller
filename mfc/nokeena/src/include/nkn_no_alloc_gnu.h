/*
 * (C) Copyright 2008 Nokeena Networks, Inc
 */
#ifndef NKN_NO_ALLOC_GNU_H
#define NKN_NO_ALLOC_GNU_H

#ifndef NKN_MALLOC_OK
#define _GNU_SOURCE
#include <stdlib.h>
/*
 * We have decided to never call these functions directly.  Only the functions
 * which implement the typed interfaces should call these functions.
 */
#define strdup(x)		strdup_function_should_never_be_called
#define calloc(x, y)		calloc_function_should_never_be_called
#define malloc(x)		malloc_function_should_never_be_called
#define realloc(x, y)		realloc_function_should_never_be_called
#define posix_memalign(x, y, z)	posix_memalign_function_should_never_be_called
#endif

#ifndef USE_SPRINTF
#define sprintf(...)	        sprintf_is_not_allowed_to_be_called
#endif

#endif	/* NKN_NO_ALLOC_GNU_H */
