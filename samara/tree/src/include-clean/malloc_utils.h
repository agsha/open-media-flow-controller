/*
 *
 * malloc_utils.h
 *
 *
 *
 */

#ifndef __MALLOC_UTILS_H_
#define __MALLOC_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file malloc_utils.h Utilities for working with GLIBC malloc()
 * \ingroup lc
 */

#include "ttypes.h"
#include "ttime.h"
#include "customer.h"

/* These can be set in customer.h */
#ifndef LC_MTRIM_STD_FREE_RETAIN
#define LC_MTRIM_STD_FREE_RETAIN (4 * 1024 * 1024)
#endif
#ifndef LC_MTRIM_STD_TIMER_INTERVAL_MS
#define LC_MTRIM_STD_TIMER_INTERVAL_MS (5 * 1000)
#endif

/**
 * Potentially attempt to return free'd memory to the system.  This
 * trims malloc()'s usage of system-allocated but unused memory.  For
 * some memory allocation patterns, especially ones involving lots of
 * malloc()/free() pairs of small sizes, trimming (and the associated
 * consolidation) may allow malloc() to return to the system significant
 * amounts of memory.
 */
int lc_malloc_trim_std(void);

#ifdef __cplusplus
}
#endif

#endif /* __MALLOC_UTILS_H_ */
