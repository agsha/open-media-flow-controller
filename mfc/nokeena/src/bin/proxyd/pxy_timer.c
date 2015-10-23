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

#include "pxy_defs.h"
#include "pxy_network.h"
#include "nkn_debug.h"

static pthread_t timerid;
int              nkn_timer_interval=1; // in (1/nkn_timer_interval) second

void pxy_server_timer_1sec(void)
{
        time(&pxy_cur_ts);
}


static inline int32_t
tv_diff (struct timeval *tv1, struct timeval *tv2)
{
    return ((tv2->tv_sec - tv1->tv_sec) * 1e6
          + (tv2->tv_usec - tv1->tv_usec));
}

static void * timer_func(void * arg)
{
	static int count1sec=0;
	static int count2sec=0;
	static int count5sec=0;
	int usecs, i;
	struct timeval tv1, tv2;

	prctl(PR_SET_NAME, "nvsd-timer", 0, 0, 0);

	UNUSED_ARGUMENT(arg);

	gettimeofday(&tv1, NULL);
	while(1) {
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
		//server_timer();

		/* ------------------------------------------------------ */
		count1sec++;
		if(count1sec < nkn_timer_interval) continue;
		count1sec=0;

		/*
		 * This block Functions are called once every 1 second.
		 */
                 pxy_server_timer_1sec();

		/*
		 * This block Functions are called once every 2 seconds.
		 */
		/* ------------------------------------------------------ */
		count2sec++;
		if(count2sec >= 2) {
			count2sec=0;
			pxy_NM_timer();
		}

		/*
		 * This block Functions are called once every 5 seconds.
		 */
		/* ------------------------------------------------------ */
		count5sec++;
		if(count5sec >= 5) {
			count5sec=0;
		}
	}
	return NULL;
}

void pxy_server_timer_init(void)
{
	int rv;
	pthread_attr_t attr;
	int stacksize = 128 * KiBYTES;

	rv = pthread_attr_init(&attr);
	if (rv) {
		DBG_LOG(MSG, MOD_PROXYD, "pthread_attr_init() failed, rv=%d",
			rv);
		return;
	}

	rv = pthread_attr_setstacksize(&attr, stacksize);
	if (rv) {
		DBG_LOG(MSG, MOD_PROXYD, 
			"pthread_attr_setstacksize() failed, rv=%d", rv);
		return;
	}

        if(pthread_create(&timerid, &attr,timer_func, NULL)) {
		DBG_LOG(MSG, MOD_PROXYD, "Failed to create timer thread.%s", "");
		return;
        }
}





