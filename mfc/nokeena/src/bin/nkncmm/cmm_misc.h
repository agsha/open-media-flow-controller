/*
 * cmm_misc.h - Miscellaneous support routines
 */

#ifndef _CMM_MISC_H
#define _CMM_MISC_H
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MILLI_SECS_PER_SEC      1000
#define MICRO_SECS_PER_SEC      (1000 * 1000)
#define NANO_SECS_PER_SEC       (1000 * 1000 * 1000)

int timespec_cmp(const struct timespec *op1, const struct timespec *op2);
int64_t timespec_diff_msecs(const struct timespec *start, 
			    const struct timespec *end);
void timespec_add_msecs(struct timespec *ts, long msecs);

void *CMM_malloc(size_t size);
void *CMM_calloc(size_t nmemb, size_t size);
void CMM_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* _CMM_MISC_H */

/*
 * End of cmm_misc.h
 */
