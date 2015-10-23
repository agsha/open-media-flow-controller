/*
 *******************************************************************************
 * cmm_misc.c - Utility routines
 *******************************************************************************
 */
#include <string.h>
#include "cmm_misc.h"
#include "CMMNewDelete.h"

int timespec_cmp(const struct timespec *op1, const struct timespec *op2)
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

int64_t timespec_diff_msecs(const struct timespec *start, 
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

void timespec_add_msecs(struct timespec *ts, long msecs)
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

/* Function is called in libnkn_memalloc, but we don't care about this. */
extern int nkn_mon_add(const char *name_tmp, char *instance, void *obj,
		       uint32_t size);
int nkn_mon_add(const char *name_tmp __attribute((unused)),
		char *instance __attribute((unused)),
		void *obj __attribute((unused)),
		uint32_t size __attribute((unused)))
{
    return 0;
}

void *CMM_malloc(size_t size)
{
    return CMMnew(size);
}

void *CMM_calloc(size_t nmemb, size_t size)
{
    char *p;
    size_t totsize = nmemb * size;

    p = CMMnew(totsize);
    if (p) {
    	memset(p, 0, totsize);
    }
    return p;
}

void CMM_free(void *ptr)
{
    CMMdelete(ptr);
}

/*
 * End of cmm_misc.c
 */
