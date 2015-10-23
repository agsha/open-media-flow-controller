#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <limits.h>

#include "ssl_defs.h"
#include "ssl_network.h"
#include "nkn_debug.h"

static pthread_t timerid;
int nkn_timer_interval = 1; // in (1/nkn_timer_interval) second

extern struct nm_threads g_NM_thrs[MAX_EPOLL_THREADS];

void server_timer(void);
void server_timer_init(void);

static inline int32_t  tv_diff (struct timeval *tv1, struct timeval *tv2)
{
    return ((tv2->tv_sec - tv1->tv_sec) * 1e6 + (tv2->tv_usec - tv1->tv_usec));
}
void server_timer(void)
{
        time(&nkn_cur_ts);
}

static void * timer_func(void * arg)
{
	static int count1sec=0;
	static int count2sec=0;
	static int count5sec=0;
	int usecs, i;
	struct timeval tv1, tv2;

	UNUSED_ARGUMENT(arg);

	prctl(PR_SET_NAME, "ssld-timer", 0, 0, 0);

	gettimeofday(&tv1, NULL);
	while (1) {
		/*
		 * remove the time which calls all functions in the timer_func
		 * x2, is to adjust more.
		 */
		gettimeofday(&tv2, NULL);
		usecs = (1000000 / nkn_timer_interval) - (tv_diff(&tv1, &tv2) * 2);
		if (usecs < 10000) usecs = 10000;

		/* ------------------------------------------------------ */
		usleep(usecs);	// sleep 1/nkn_timer_interval seconds

		gettimeofday(&tv1, NULL);
		/*
		 * This block Functions are called once every 1/nkn_timer_interval seconds.
		 */
		server_timer();

		/* ------------------------------------------------------ */
		count1sec++;
		if (count1sec < nkn_timer_interval) continue;
		count1sec=0;

		/*
		 * This block Functions are called once every 2 seconds.
		 */
		/* ------------------------------------------------------ */
		count2sec++;
		if (count2sec >= 2) {
			count2sec=0;
			NM_timer();
		}

		/*
		 * This block Functions are called once every 5 seconds.
		 */
		/* ------------------------------------------------------ */
		count5sec++;
		if (count5sec >= 5) {
			count5sec=0;
		}
	}

	return NULL;
}

void server_timer_init(void)
{
	int rv;
	pthread_attr_t attr;
	int stacksize = 128 * KiBYTES;

	rv = pthread_attr_init(&attr);
	if (rv) {
		DBG_LOG(MSG, MOD_NETWORK, "pthread_attr_init() failed, rv=%d", rv);
		return;
	}

	rv = pthread_attr_setstacksize(&attr, stacksize);
	if (rv) {
		DBG_LOG(MSG, MOD_NETWORK,
			"pthread_attr_setstacksize() failed, rv=%d", rv);
		return;
	}

        if (pthread_create(&timerid, &attr,timer_func, NULL)) {
		DBG_LOG(MSG, MOD_NETWORK, "Failed to create timer thread.%s", "");
		return;
        }

	return;
}
