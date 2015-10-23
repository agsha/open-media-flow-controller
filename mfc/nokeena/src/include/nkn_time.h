/*
 * nkn_time.h - Basic time functions
 */
#ifndef _NKN_TIME_H
#define _NKN_TIME_H

#include <sys/types.h>
#include <time.h>

#define MILLI_SECS_PER_SEC      1000
#define MICRO_SECS_PER_SEC      (1000 * 1000)
#define NANO_SECS_PER_SEC       (1000 * 1000 * 1000)

int 
nkn_timespec_cmp(const struct timespec *op1, const struct timespec *op2);

int64_t 
nkn_timespec_diff_msecs(const struct timespec *start,
                        const struct timespec *end);

void nkn_timespec_add_msecs(struct timespec *ts, long msecs);

#endif /* _NKN_TIME_H */

/*
 * End of nkn_time.h
 */
