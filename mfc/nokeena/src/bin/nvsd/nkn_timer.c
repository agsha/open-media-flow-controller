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
#include <sys/shm.h>

#include "nkn_cfg_params.h"
#include "nkn_debug.h"
#include "server.h"
#include "network.h"
#include <sys/prctl.h>

static pthread_t timerid;
int nkn_timer_interval=1; // in (1/nkn_timer_interval) second
int64_t nkn_cur_100_tms;
extern unsigned long long max_shm_loop_created;
unsigned long long glob_shm_loop_created;

extern void server_timer_1sec(void);
extern void al_cleanup(void);
extern void sl_cleanup(void);
//extern void el_cleanup(void);
extern void omq_head_leak_detection(void);
extern void nvsd_rp_bw_timer_cleanup_1sec(void);

void server_timer_init(void);

static inline int32_t
tv_diff (struct timeval *tv1, struct timeval *tv2)
{
    return ((tv2->tv_sec - tv1->tv_sec) * 1e6
          + (tv2->tv_usec - tv1->tv_usec));
}


static void * logcleanup_func(void *arg)
{
	struct timeval tv1, tv2;
	static int cnt1sec=0;
	int usecs;
	prctl(PR_SET_NAME, "nvsd-logcleanup", 0, 0, 0);
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

		/* ------------------------------------------------------ */
		cnt1sec++;
		if(cnt1sec < nkn_timer_interval) continue;
		cnt1sec=0;

		al_cleanup();
		if( rtsp_enable ) sl_cleanup();
		//el_cleanup();
	}
        return NULL;
}

static void * timer_func(void * arg)
{
	static int count1sec=0;
	static int count2sec=0;
	static int count5sec=0;
	int usecs, i;
	struct timeval tv1, tv2;
	nkn_interface_t * pns;

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
		for(i=0;i<NM_tot_threads;i++) {
			g_NM_thrs[i].cleanup_sbq=1;
		}
		for(i=0;i<MAX_NKN_INTERFACE;i++) {
            pns = &interface[i];
            if(pns->if_bandwidth > pns->if_totbytes_sent) {
                pns->if_credit = pns->if_bandwidth - pns->if_totbytes_sent;
            }
            else {
                pns->if_credit = 0;
            }
            pns->if_totbytes_sent = 0;
        }
        nvsd_rp_bw_timer_cleanup_1sec();
		server_timer_1sec();
		nkn_update_combined_stats();
		glob_shm_loop_created = max_shm_loop_created;
		/*
		 * This block Functions are called once every 2 seconds.
		 */
		/* ------------------------------------------------------ */
		count2sec++;
		if(count2sec >= 2) {
			count2sec=0;
			NM_timer();
		}

		/*
		 * This block Functions are called once every 5 seconds.
		 */
		/* ------------------------------------------------------ */
		count5sec++;
		if(count5sec >= 5) {
			count5sec=0;
			omq_head_leak_detection();
		}
	}
	return NULL;
}

static int 
nvsd_check_thread_block(void)
{	
	static unsigned int g_NM_thrs_epoll_event_cnt[MAX_EPOLL_THREADS] = {0,} ;

        int i = 0;

        for (i=0; i < NM_tot_threads; i++) {
		if (g_NM_thrs_epoll_event_cnt[i] && /* non zero */
                    (g_NM_thrs[i].epoll_event_cnt == g_NM_thrs_epoll_event_cnt[i])) {
                        DBG_LOG(SEVERE, MOD_NETWORK, "Thread[%lu]: PID:'%ld', in blocked state",
                                        g_NM_thrs[i].num, g_NM_thrs[i].pid) ;
                	return TRUE; /* This thread is blocked */
                } else {
       			g_NM_thrs_epoll_event_cnt[i] = g_NM_thrs[i].epoll_event_cnt;
                }
        }
        return FALSE ;
}

static void * nvsd_health_check_func(void * arg)
{
        static int count2sec=0;
        int usecs;
        struct timeval tv1, tv2;
        int    nvsd_health_good = 0;

        prctl(PR_SET_NAME, "nvsd-health", 0, 0, 0);

        UNUSED_ARGUMENT(arg);

        gettimeofday(&tv1, NULL);
        while(1) {
                int    nvsd_health_curr = 1;

                /*
                 * remove the time which calls all functions in the timer_func
                 * x2, is to adjust more.
                 */
                gettimeofday(&tv2, NULL);
                usecs = (1000000 / nkn_timer_interval) - (tv_diff(&tv1, &tv2) * 2);
                if (usecs < 10000) usecs = 10000;

                usleep(usecs);  // sleep 1/nkn_timer_interval seconds
                gettimeofday(&tv1, NULL);
                /*
                 * This block Functions are called once every 1/nkn_timer_interval seconds.
                 */
                count2sec++;
                if(count2sec < 2) {
                     continue;
                }
                count2sec = 0;
               /*
                *   Do health check here, if needed update the shared memory with info.
                */
                if (nvsd_check_thread_block()) {
                      nvsd_health_curr = 0;  // Not in good health.
                }

                if (nkn_glob_nvsd_health && (nvsd_health_curr != nvsd_health_good)) { // when there is state change, update.
                      /*Update shared memory*/
                      nkn_glob_nvsd_health->good = nvsd_health_good = nvsd_health_curr ;
                }
        }
        return NULL;
}

static void *mstimer_func(void * arg)
{
	struct timeval tv1, tv2;
        int usecs;

	prctl(PR_SET_NAME, "nvsd-mstimer", 0, 0, 0);

	UNUSED_ARGUMENT(arg);

	gettimeofday(&tv1, NULL);
	while(1) {
		/* 
		 * remove the time which calls all functions in the timer_func
		 * x2, is to adjust more.
		 */
		gettimeofday(&tv2, NULL);
		// 100ms - extra work time give the sleep time
		usecs = 100000 - (tv_diff(&tv1, &tv2));
		// < 10ms then keep 10ms for accurate sleep
		if (usecs < 10000) usecs = 10000;

		/* ------------------------------------------------------ */
		usleep(usecs);

		gettimeofday(&tv1, NULL);

		nkn_cur_100_tms = (tv1.tv_sec * 1000000 + tv1.tv_usec) / 10000;  // 100 millisecond timer
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
		DBG_LOG(MSG, MOD_NETWORK, "pthread_attr_init() failed, rv=%d",
			rv);
		return;
	}

	rv = pthread_attr_setstacksize(&attr, stacksize);
	if (rv) {
		DBG_LOG(MSG, MOD_NETWORK, 
			"pthread_attr_setstacksize() failed, rv=%d", rv);
		return;
	}

        if(pthread_create(&timerid, &attr,timer_func, NULL)) {
		DBG_LOG(MSG, MOD_NETWORK, "Failed to create timer thread.%s", "");
		return;
        }

        if(pthread_create(&timerid, &attr, logcleanup_func, NULL)) {
		DBG_LOG(MSG, MOD_NETWORK, "Failed to create timer thread.%s", "");
		return;
        }

       /* Initialize this thread only when l4proxy is enabled*/
        if(l4proxy_enabled && pthread_create(&timerid, &attr, nvsd_health_check_func, NULL)) {
		DBG_LOG(MSG, MOD_NETWORK, "Failed to create health check thread.%s", "");
		return;
        }

	if(pthread_create(&timerid, &attr, mstimer_func, NULL)) {
		DBG_LOG(MSG, MOD_NETWORK, "Failed to create mstimer thread.%s", "");
		return;
	}
}





