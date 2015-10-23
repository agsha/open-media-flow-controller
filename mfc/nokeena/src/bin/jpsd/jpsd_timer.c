/*
 * @file jpsd_timer.c
 * @brief
 * jpsd_timer.c - definations for jpsd timer functions.
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2014, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <atomic_ops.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include "nkn_memalloc.h"

#include "jpsd_defs.h"

static int jpsd_timer_interval = 1;
time_t jpsd_cur_ts;

static pthread_t timer_pid;
static pthread_mutex_t timer_mutex;

extern int tdf_session_init_done;

static inline int32_t jpsd_tv_diff(struct timeval *tv1, struct timeval *tv2)
{
    return ((tv2->tv_sec - tv1->tv_sec) * 1e6 + (tv2->tv_usec - tv1->tv_usec));
}

static void *jpsd_timer_func(void *arg __attribute__((unused)))
{
	static int count1sec = 0;
	static int count2sec = 0;
	static int count5sec = 0;
	int usecs;
	int i;
	struct timeval tv1, tv2;

	prctl(PR_SET_NAME, "jpsd-timer", 0, 0, 0);

	while (tdf_session_init_done == 0) {
		sleep(2);
	}

	pthread_mutex_init(&timer_mutex, NULL);
	gettimeofday(&tv1, NULL);

	while (1) {
		gettimeofday(&tv2, NULL);
		usecs = (1000000 / jpsd_timer_interval) - (jpsd_tv_diff(&tv1, &tv2) * 2);
		if (usecs < 10000)
			usecs = 10000;
		usleep(usecs);
		gettimeofday(&tv1, NULL);
		count1sec++;
		if (count1sec < jpsd_timer_interval)
			continue;
		count1sec = 0;
		time(&jpsd_cur_ts);
		count2sec++;
		if (count2sec >= 2) {
			count2sec = 0;
			diameter_timer();
		}
		count5sec++;
		if (count5sec >= 5) {
			count5sec = 0;
			get_cacheable_ip_list();
		}
	}

	return NULL;
}

void jpsd_timer_start(void)
{
	int rv;
	pthread_attr_t attr;
	int stacksize = 128 * 1000;

	rv = pthread_attr_init(&attr);
	assert(rv == 0);

	rv = pthread_attr_setstacksize(&attr, stacksize);
	assert(rv == 0);

	diameter_timer_queue_init();

        rv = pthread_create(&timer_pid, &attr, jpsd_timer_func, NULL);
	assert(rv == 0);

	return;
}
