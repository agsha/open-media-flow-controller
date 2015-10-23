/*
 * nkn_time.c
 *
 * Basic time functions
 */
#include "nkn_time.h"

int nkn_timespec_cmp(const struct timespec *op1, const struct timespec *op2)
{
    if (op1->tv_sec < op2->tv_sec) {
    	return -1;
    } else if (op1->tv_sec > op2->tv_sec) {
    	return 1;
    } else {
    	if (op1->tv_nsec < op2->tv_nsec) {
	    return -1;
	} else if (op1->tv_nsec > op2->tv_nsec) {
	    return 1;
	}
	return 0;
    }
}

int64_t nkn_timespec_diff_msecs(const struct timespec *start, 
			   	const struct timespec *end)
{
    /*
     * Assumptions:
     *   1) start <= end
     */
    struct timespec t;

    if (end->tv_nsec < start->tv_nsec)  {
    	t.tv_sec = end->tv_sec - start->tv_sec - 1;
	t.tv_nsec = (NANO_SECS_PER_SEC + end->tv_nsec) - start->tv_nsec;
    } else {
    	t.tv_sec = end->tv_sec - start->tv_sec;
	t.tv_nsec = end->tv_nsec - start->tv_nsec;
    }
    return (t.tv_sec * MILLI_SECS_PER_SEC) + (t.tv_nsec / MICRO_SECS_PER_SEC);
}

void nkn_timespec_add_msecs(struct timespec *ts, long msecs)
{
    if ((ts->tv_nsec + (msecs * MICRO_SECS_PER_SEC)) > NANO_SECS_PER_SEC) {
    	ts->tv_sec += 
	    (ts->tv_nsec + (msecs * MICRO_SECS_PER_SEC)) / NANO_SECS_PER_SEC;
	ts->tv_nsec = 
	    (ts->tv_nsec + (msecs * MICRO_SECS_PER_SEC)) % NANO_SECS_PER_SEC;
    } else {
	ts->tv_nsec += (msecs * MICRO_SECS_PER_SEC);
    }
}

/*
 * End of nkn_time.c
 */
