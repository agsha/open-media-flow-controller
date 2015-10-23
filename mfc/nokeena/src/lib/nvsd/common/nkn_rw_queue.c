/*
 * (C) Copyright 2008-2011 Juniper Networks, Inc
 *
 * This file contains atomic read/write Queue Implementation
 *
 * Author: Jeya ganesh babu
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/queue.h>
#include <pthread.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_lockstat.h"
#include "nkn_rw_queue.h"

uint64_t glob_rw_queue_waited;

/* Debug counters */
uint64_t glob_rw_failed_get_cnt1;
uint64_t glob_rw_failed_get_cnt2;
uint64_t glob_rw_failed_put_cnt1;
uint64_t glob_rw_failed_put_cnt2;
uint64_t glob_rw_failed_put_cnt3;
uint64_t glob_rw_get_next_retry_cnt;

/* Function to find the Nearest Pow of 2 */
static uint32_t
nearest_pow(uint32_t num)
{
    uint32_t bit = 0x80000000;
    num--;
    while (!(num & bit)) {
	bit >>= 1;
    }
    return (bit << 1);
}

/*
 * Queue Init routine 
 *
 * Input : num_entries - Number of entries for the RW Queue
 *
 * Output: RW Queue pointer
 *
 */
void *
nkn_rw_queue_init(int num_entries)
{
    nkn_rw_queue_t     *new_rw_q;
    pthread_condattr_t a;

    new_rw_q = nkn_calloc_type(1, sizeof(nkn_rw_queue_t), mod_rw_queue_t);
    if(new_rw_q == NULL)
	return NULL;

    num_entries = nearest_pow(num_entries);

    new_rw_q->rw_q_array = nkn_calloc_type(num_entries, sizeof(void *), mod_rw_queue_t);
    if(new_rw_q->rw_q_array == NULL) {
	free(new_rw_q);
	return NULL;
    }

    new_rw_q->rd_index = 0;
    new_rw_q->wr_index = 0;
    new_rw_q->mask     = num_entries -1;

    pthread_mutex_init(&new_rw_q->lock, NULL);
    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);
    pthread_cond_init(&new_rw_q->cond, &a);

    return new_rw_q;
}

/*
 * Queue Insert Routine
 *
 * Input : ptr     - rw Queue pointer
 *         element - Element to be inserted
 *
 * Output: 0  - Success
 *         -1 - Failed
 *
 */
inline int
nkn_rw_queue_insert(void * restrict ptr, void * restrict element)
{
    nkn_rw_queue_t *rw_q = ptr;
    int wr_index_old, wr_index_new, rd_index;
    int ret;

retry:;
    rd_index     = rw_q->rd_index;
    wr_index_old = rw_q->wr_index;
    wr_index_new = (wr_index_old + 1) & rw_q->mask;
    if(wr_index_new == rd_index) {
	glob_rw_failed_put_cnt1++;
	return -1;
    }
    ret = __sync_bool_compare_and_swap(&rw_q->wr_index, wr_index_old,
					wr_index_new);
    if(!ret) {
	glob_rw_failed_put_cnt2++;
	goto retry;
    }

retry_2:;
    ret = __sync_bool_compare_and_swap(&rw_q->rw_q_array[wr_index_old],
					0, element);
    if(!ret) {
	glob_rw_failed_put_cnt3++;
	goto retry_2;
    }

    return 0;
}

/*
 * Queue Task Insert Routine
 *
 * Input : ptr     - rw Queue pointer
 *         element - Element to be inserted
 *
 * Output: 0  - Success
 *         -1 - Failed
 *
 */
int
nkn_rw_queue_insert_task(void * restrict ptr, void * restrict element)
{
    int ret;
    ret = nkn_rw_queue_insert(ptr, element);
    if(!ret)
	pthread_cond_signal(&(((nkn_rw_queue_t *)ptr)->cond));
    return ret;
}

/*
 * Queue Pop Routine
 *
 * Input : ptr     - rw Queue pointer
 *
 * Output: Element pointer
 *         NULL in case of failure
 *
 */
inline void *
nkn_rw_queue_get_next(void *ptr)
{
    nkn_rw_queue_t *rw_q = ptr;
    void *ret_val;
    int  ret, rd_index_old, rd_index_new;

retry:;
    rd_index_old = rw_q->rd_index;
    if(rd_index_old != rw_q->wr_index) {
	ret_val      = rw_q->rw_q_array[rd_index_old];
	if(!ret_val)
	    goto retry;

	rd_index_new = (rd_index_old + 1) & rw_q->mask;
	ret = __sync_bool_compare_and_swap(&rw_q->rd_index, rd_index_old,
					    rd_index_new);
	if(!ret) {
	    glob_rw_failed_get_cnt1++;
	    goto retry;
	}

	ret = __sync_bool_compare_and_swap(&rw_q->rw_q_array[rd_index_old],
					ret_val, 0);
	if(!ret) {
	    glob_rw_failed_get_cnt2++;
	}

	return ret_val;
    }
    return NULL;
}

/*
 * Queue Task Pop Routine
 *
 * Input : ptr     - rw Queue pointer
 *
 * Output: Element pointer
 *         NULL in case of failure
 *
 */
void *
nkn_rw_queue_get_next_element(void *ptr)
{
    nkn_rw_queue_t *rw_q = ptr;
    struct timespec ts;
    void *ret_val;
    int  ret;
    assert(ptr != NULL);

    while(1) {
	ret_val = nkn_rw_queue_get_next(ptr);
	if(ret_val)
	    return ret_val;
	/*
	 * Have to use timedwait because there is no guarantee
	 * that there is no waiting task when the sched thread
	 * goes into sleep. There is a small window when a task
	 * could be waiting and the sched thread goes to sleep.
	 */
	if(ENTRY_PRESENT(rw_q)) {
	    continue;
	}

	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_sec += 1;

	pthread_mutex_lock(&rw_q->lock);
	ret = pthread_cond_timedwait(&rw_q->cond, &rw_q->lock, &ts);
	pthread_mutex_unlock(&rw_q->lock);
	if(ret == ETIMEDOUT)
	    glob_rw_queue_waited++;
    }
}

