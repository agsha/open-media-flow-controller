/*
 * network_hres_timer.c
 *
 * High resolution timer (subsecond resolution) support for 
 * network_mgr_t objects.
 * Interface provides support for specific types of timers (net_timer_type_t)
 * per network_mgr_t object.
 * 
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <assert.h>
#include <atomic_ops.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include "nkn_stat.h"
#include "nkn_debug.h"
#include "network.h"
#include "network_hres_timer.h"

/*
 * Extern declarations
 */

/*
 * Build definitions
 */
#if 0
#define HRT_UNIT_TESTS		1 // Compile HRT unit tests
#endif

/*
 * Fundamental definitions
 */
#define MSECS_PER_BUCKET   	10 // 10 msecs
#define MAX_INT_INTERVAL_MSECS 	2000 // 2 seconds
#define MAX_INTERVAL_MSECS 	(MAX_INT_INTERVAL_MSECS/2)
#define TIMER_BUCKETS      	(MAX_INT_INTERVAL_MSECS / MSECS_PER_BUCKET)
#define TIMER_DATA_MAGICNO 	0x1a2b3c4d
#define TIMER_DATA_MAGICNO_FREE	0xf0eef0ee
#define MSECS_TO_BUCKET(_msecs) ((_msecs) / MSECS_PER_BUCKET)
#define MILLI_SECS_PER_SEC	1000
#define MICRO_SECS_PER_SEC	(1000 * 1000)
#define NANO_SECS_PER_SEC 	(1000 * 1000 * 1000)
#define MAX_CALLBACK_THREADS	4

#define TRACE_BUCKET(_bucket) { \
    struct timespec _ts; \
    if (hrt_trace_enabled) { \
	clock_gettime(CLOCK_MONOTONIC, &_ts); \
	DBG_LOG(MSG, MOD_HRT, "bucket=%d ts=%ld %ld\n", \
	        (_bucket), _ts.tv_sec, _ts.tv_nsec); \
    } \
}
#define ROUND_BUCKET_TIME(_msecs) \
    (((_msecs) / MSECS_PER_BUCKET) * MSECS_PER_BUCKET)

/*
 * Timer data element
 */
typedef struct net_timer_data {
    int magicno;
    struct net_timer_data *next;
    struct net_timer_data *prev;
    struct net_timer_data *head; // List head
    net_fd_handle_t fhd;
    net_timer_type_t type;
    uint32_t flags;
    struct timespec deadline_ts;
} net_timer_data_t;

/* 
 * net_timer_data_t.flags definitions
 */
#define NTD_FL_CANCELED 0x1

/*
 * Timer data list head element
 */
typedef struct net_timer_data_head {
    net_timer_data_t d; // Always first
    int entries;
} net_timer_data_head_t;

static net_timer_data_head_t timer_bucket[TIMER_BUCKETS];
static AO_t active_timer_data;

/*
 * File descriptor to timer data map
 */
typedef struct timer_data_map {
    net_timer_data_t *map[TT_MAX];
} timer_data_map_t;

static timer_data_map_t FD_to_TD[MAX_GNM];

/*
 * Timer data free list
 */
static net_timer_data_head_t free_timer_data;
static AO_t timer_data_fastalloc_count;
static AO_t timer_data_max_fastalloc  = (32 * 1024); // Upper bound 
static pthread_mutex_t mem_alloc_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Timer data link list macros
 */
 #define NTD_HEAD_INIT(_hdr) { \
     (_hdr)->d.magicno = TIMER_DATA_MAGICNO; \
     (_hdr)->d.next = &(_hdr)->d; \
     (_hdr)->d.prev = &(_hdr)->d; \
     (_hdr)->d.head = 0; \
     (_hdr)->d.fhd.incarn = 0; \
     (_hdr)->d.fhd.fd = -1; \
     (_hdr)->d.type = -1; \
     (_hdr)->d.flags = 0; \
     (_hdr)->entries = 0; \
 }

#define NTD_LINK_TAIL(_hdr, _ntd, _global_cnt) { \
    (_ntd)->next = &(_hdr)->d; \
    (_ntd)->prev = (_hdr)->d.prev; \
    (_ntd)->head = &(_hdr)->d; \
    (_hdr)->d.prev->next = (_ntd); \
    (_hdr)->d.prev = (_ntd); \
    (_hdr)->entries++; \
    AO_fetch_and_add1(&(_global_cnt)); \
}

#define NTD_LINK_HEAD(_hdr, _ntd, _global_cnt) { \
    (_ntd)->next = (_hdr)->d.next; \
    (_ntd)->prev = &(_hdr)->d; \
    (_ntd)->head = &(_hdr)->d; \
    (_hdr)->d.next->prev = (_ntd); \
    (_hdr)->d.next = (_ntd); \
    (_hdr)->entries++; \
    AO_fetch_and_add1(&(_global_cnt)); \
}

#define NTD_UNLINK(_ntd, _global_cnt) { \
    (_ntd)->prev->next = (_ntd)->next; \
    (_ntd)->next->prev = (_ntd)->prev; \
    ((net_timer_data_head_t *)(_ntd)->head)->entries--; \
    AO_fetch_and_sub1(&(_global_cnt)); \
    (_ntd)->next = 0; \
    (_ntd)->prev = 0; \
    (_ntd)->head = 0; \
}

/*
 * Timer thread data
 */
static pthread_t hrt_thread_id;
static int hrt_init_complete = 0;

static pthread_mutex_t hrt_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t hrt_thread_cv = PTHREAD_COND_INITIALIZER;

/*
 * Timer callback thread pool data
 */
static net_timer_data_head_t cb_timer_data;
static AO_t active_cb_timer_data;

static pthread_t cb_thread_id[MAX_CALLBACK_THREADS];
static AO_t cb_init_complete;

static pthread_mutex_t cb_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cb_thread_cv = PTHREAD_COND_INITIALIZER;

static int cb_threads_sleeping = 0;

/*
 *******************************************************************************
 * Note: The following is always accessed under hrt_thread_mutex
 *******************************************************************************
 */
static int 		hrt_base_bucket;
static struct timespec 	hrt_base_bucket_ts;
static struct timespec 	hrt_thread_ts;
static int 		hrt_thread_sleeping;

#define HRT_BASE_BUCKET hrt_base_bucket
#define HRT_BASE_BUCKET_TS hrt_base_bucket_ts
#define HRT_THREAD_TS hrt_thread_ts
#define HRT_THREAD_SLEEPING hrt_thread_sleeping

/*
 * Trace data
 */
static int hrt_trace_enabled = 0;
static int hrt_thread_trace_enabled = 0;

/*
 * Unit test data
 */
#define HRT_INTERVAL_TEST 	(1 << 0)
#define HRT_ADD_CANCEL_TEST	(1 << 1)
#define HRT_ADD_ADD_TEST	(1 << 2)
static int run_net_hrt_tests = 0;

/*
 * Global stats
 */	
NKNCNT_DEF(hrt_add_inv_fhd, int64_t, "", "Invalid file handle in add timer")
NKNCNT_DEF(hrt_add_inv_int, int64_t, "", "Invalid interval in add timer")
NKNCNT_DEF(hrt_add_alloc_failed, int64_t, "", "Alloc failed in add timer")
NKNCNT_DEF(hrt_add_bucket_failed, int64_t, "", "Add bucket failed in add timer")
NKNCNT_DEF(hrt_add_timer, int64_t, "", "Add timer requests")

NKNCNT_DEF(hrt_cancel, int64_t, "", "Timer cancel requests")
NKNCNT_DEF(hrt_bad_cancel, int64_t, "", "Invalid timer cancel")

NKNCNT_DEF(hrt_callbacks, int64_t, "", "timer callbacks")

NKNCNT_DEF(hrt_alloc_fast, int64_t, "", "net_timer_data_t fast allocs")
NKNCNT_DEF(hrt_alloc_malloc, int64_t, "", "net_timer_data_t malloc allocs")
NKNCNT_DEF(hrt_ntd_alloc_failed, int64_t, "", "net_timer_data_t alloc failed")

NKNCNT_DEF(hrt_invalid_free, int64_t, "", "Invalid free, bad magicno")
NKNCNT_DEF(hrt_free_fast, int64_t, "", "net_timer_data_t fast free")
NKNCNT_DEF(hrt_free_malloc, int64_t, "", "net_timer_data_t malloc free")

NKNCNT_DEF(hrt_cond_signal_failed, int64_t, "", "CV signal failed")

NKNCNT_DEF(hrt_invalid_interval, int64_t, "", "timer interval exceeds max")
NKNCNT_DEF(hrt_long_callout, int64_t, "", "callout processing time exceeded")
NKNCNT_DEF(hrt_callout_behind, int64_t, "", "callout processing behind")

/*
 * Forward declarations 
 */
static net_timer_data_t *alloc_net_timer_data_t(net_timer_type_t type, 
						net_fd_handle_t fhd);
static void free_net_timer_data_t(net_timer_data_t *pd);
static int add_to_bucket(net_timer_data_t *nd, int interval_msecs);
static int process_timer_bucket(int bucket);
static int timespec_cmp(const struct timespec *op1, const struct timespec *op2);
static int64_t timespec_diff_msecs(const struct timespec *start, 
				   const struct timespec *end);
static void timespec_add_msecs(struct timespec *ts, long msecs);
static void *hrt_handler_func(void *arg);
static void *cb_handler_func(void *arg);
static int net_hrt_tests(void);

/*
 * Set:
 *   MutexLock FD
 *     Allocate TD
 *     Add TD to FD_to_TD map
 *     Compute Bucket
 *     MutexLock HRT
 *       Add TD
 *     MutexUnlock HRT
 *   MutexUnlock FD
 *
 * Cancel:
 *   MutexLock FD
 *     Find TD in FD_to_TD map
 *     Mark TD "canceled" 
 *   MutexUnlock FD
 *    
 * Timeout handler:
 *   MutexLock HRT
 *   Remove entries
 *   MutexUnlock HRT
 *
 *   For each entry
 *     MutexLock FD
 *     If (inuse && incarn equal && !canceled) {
 *       Callout timer function
 *     }
 *     if (TD == FD_to_TD map) remove from FD_to_TD map
 *     MutexUnlock FD
 *     Free TD
 */

/*
 *****************************************************************************
 *           E X T E R N A L  I N T E R F A C E S
 *****************************************************************************
 */

/* 
 * network_hres_timer_init() -- Subsystem initialization
 *
 *	Return: !=0 => Success
 */
int network_hres_timer_init()
{
    int rv;
    int n;
    pthread_attr_t attr;
    int stacksize = (128 * 1024);
    struct pollfd pfd;

    for (n = 0; n < TIMER_BUCKETS; n++) {
    	NTD_HEAD_INIT(&timer_bucket[n]);
    }

    NTD_HEAD_INIT(&free_timer_data);
    NTD_HEAD_INIT(&cb_timer_data);

    rv = pthread_attr_init(&attr);
    if (rv) {
    	DBG_LOG(SEVERE, MOD_HRT, "pthread_attr_init() failed, rv=%d", rv);
	DBG_ERR(SEVERE, "pthread_attr_init() failed, rv=%d", rv);
	return 0;
    }

    rv = pthread_attr_setstacksize(&attr, stacksize);
    if (rv) {
    	DBG_LOG(SEVERE, MOD_HRT, "pthread_attr_setstacksize() failed, rv=%d", 
		rv);
	DBG_ERR(SEVERE, "pthread_attr_setstacksize() failed, rv=%d", rv);
	return 0;
    }

    /* Create HRT thread */
    if ((rv = pthread_create(&hrt_thread_id, &attr, 
    			     hrt_handler_func, NULL))) {
	DBG_LOG(SEVERE, MOD_HRT, "pthread_create() failed, rv=%d", rv);
	DBG_ERR(SEVERE, "pthread_create() failed, rv=%d", rv);
	return 0;
    }

    for (n = 0; !hrt_init_complete; n++) {
    	poll(&pfd, 0, 100); // 100 msec sleep
	if (n >= 19) { // 2 secs
	    DBG_LOG(SEVERE, MOD_HRT, "HRT thread wait failed");
	    DBG_ERR(SEVERE, "HRT thread wait failed");
	    return 0;
	}
    }

    /* Create callback threads */
    for (n = 0; n < MAX_CALLBACK_THREADS; n++) {
    	if ((rv = pthread_create(&cb_thread_id[n], &attr, 
    			     	 cb_handler_func, NULL))) {
	    DBG_LOG(SEVERE, MOD_HRT, "pthread_create() failed, rv=%d", rv);
	    DBG_ERR(SEVERE, "pthread_create() failed, rv=%d", rv);
	    return 0;
    	}
    }

    for (n = 0; AO_load(&cb_init_complete) < MAX_CALLBACK_THREADS; n++) {
    	poll(&pfd, 0, 100); // 100 msec sleep
	if (n >= 19) { // 2 secs
	    DBG_LOG(SEVERE, MOD_HRT, "CB thread wait failed");
	    DBG_ERR(SEVERE, "CB thread wait failed");
	    return 0;
	}
    }

    if (run_net_hrt_tests) {
    	assert(net_hrt_tests() == 0);
    }

    return 1; // Success
}

/* 
 * net_set_timer() -- Set timer for given network_mgr_t
 *
 *	Return: 0 => Success
 */
int net_set_timer(net_fd_handle_t fhd, int interval_msecs, 
		  net_timer_type_t type)
{
    /* Note: Caller has acquired the gnm[fd].mutex prior to the call */

    int rv;
    net_timer_data_t *nd;

    if ((fhd.fd != gnm[fhd.fd].fd) || 
        (fhd.incarn != gnm[fhd.fd].incarn)) {
    	glob_hrt_add_inv_fhd++;
	return 1;
    }

    if (interval_msecs > MAX_INTERVAL_MSECS) {
    	glob_hrt_add_inv_int++;
    	return 2;
    }

    if (FD_to_TD[fhd.fd].map[type] &&
    	!(FD_to_TD[fhd.fd].map[type]->flags & NTD_FL_CANCELED)) {
    	net_cancel_timer(fhd, type);
    }

    nd = alloc_net_timer_data_t(type, fhd);
    if (nd) {
    	FD_to_TD[fhd.fd].map[type] = nd;
    } else {
    	glob_hrt_add_alloc_failed++;
    	return 3;
    }

    if ((rv = add_to_bucket(nd, interval_msecs)) != 0) {
    	free_net_timer_data_t(nd);
    	FD_to_TD[fhd.fd].map[type] = 0;
    	glob_hrt_add_bucket_failed++;
    	return (rv < 0) ? -1 : 4;
    }
    glob_hrt_add_timer++;

    return 0; // Success
}

/* 
 * net_cancel_timer() -- Cancel timer for given network_mgr_t
 */
int net_cancel_timer(net_fd_handle_t fhd, net_timer_type_t type)
{
    /* Note: Caller has acquired the gnm[fd].mutex prior to the call */

    if (FD_to_TD[fhd.fd].map[type] && 
    	(FD_to_TD[fhd.fd].map[type]->magicno == TIMER_DATA_MAGICNO)) {
    	FD_to_TD[fhd.fd].map[type]->flags |= NTD_FL_CANCELED;
        glob_hrt_cancel++;
	return 0; // Success
    } else {
        glob_hrt_bad_cancel++;
    	return 1;
    }
}

/*
 *****************************************************************************
 *           I N T E R N A L  I N T E R F A C E S
 *****************************************************************************
 */
static net_timer_data_t *alloc_net_timer_data_t(net_timer_type_t type, 
						net_fd_handle_t fhd)
{
    net_timer_data_t *pd;

    pthread_mutex_lock(&mem_alloc_mutex);
    if (free_timer_data.entries) {
    	pd = free_timer_data.d.next;
    	NTD_UNLINK(pd, timer_data_fastalloc_count);
    	pthread_mutex_unlock(&mem_alloc_mutex);
    	glob_hrt_alloc_fast++;
    } else {
    	pthread_mutex_unlock(&mem_alloc_mutex);
	pd = nkn_malloc_type(sizeof(net_timer_data_t), mod_net_timer_data);
	if (!pd) {
	    glob_hrt_ntd_alloc_failed++;
	    return 0;
	}
    	glob_hrt_alloc_malloc++;
    }
    pd->magicno = TIMER_DATA_MAGICNO;
    pd->fhd = fhd;
    pd->type = type;
    pd->flags = 0;

    return pd;
}

static void free_net_timer_data_t(net_timer_data_t *pd)
{
    if (!pd) {
    	return;
    }

    if (pd->magicno != TIMER_DATA_MAGICNO) {
    	/* Bad magicno, leak memory */
    	glob_hrt_invalid_free++;
	return;
    }

    pthread_mutex_lock(&mem_alloc_mutex);
    if (timer_data_fastalloc_count < timer_data_max_fastalloc) {
    	pd->magicno = TIMER_DATA_MAGICNO_FREE;
	NTD_LINK_TAIL(&free_timer_data, pd, timer_data_fastalloc_count);
    	pthread_mutex_unlock(&mem_alloc_mutex);
	glob_hrt_free_fast++;
    } else {
    	pthread_mutex_unlock(&mem_alloc_mutex);
	free(pd);
	glob_hrt_free_malloc++;
    }
}

static int add_to_bucket(net_timer_data_t *nd, int interval_msecs)
{
    int64_t msecs;
    int bucket;
    int rv;

    clock_gettime(CLOCK_MONOTONIC, &nd->deadline_ts);

    pthread_mutex_lock(&hrt_thread_mutex);
    msecs = timespec_diff_msecs(&HRT_BASE_BUCKET_TS, &nd->deadline_ts);
    if (msecs < 0) {
    	msecs = 0;
    }
    msecs += interval_msecs;
    timespec_add_msecs(&nd->deadline_ts, interval_msecs);

    if (msecs > MAX_INT_INTERVAL_MSECS) {
	/*
	 * HRT thread has not run in greater than MAX_INT_INTERVAL_MSECS,
	 * signal it to process requests and return a retry indication.
	 */
    	rv = pthread_cond_signal(&hrt_thread_cv);
	if (rv) {
	    glob_hrt_cond_signal_failed++;
    	    DBG_LOG(SEVERE, MOD_HRT, "CV signal failed, rv=%d", rv);
    	    DBG_ERR(SEVERE, "CV signal failed, rv=%d", rv);
	}
    	pthread_mutex_unlock(&hrt_thread_mutex);
    	glob_hrt_invalid_interval++;
	return -1; // Retry
    }

    bucket = MSECS_TO_BUCKET(msecs);
    bucket = (HRT_BASE_BUCKET + bucket) % TIMER_BUCKETS;
    NTD_LINK_TAIL(&timer_bucket[bucket], nd, active_timer_data);
    TRACE_BUCKET(bucket);

    if (HRT_THREAD_SLEEPING && 
    	(timespec_cmp(&nd->deadline_ts, &HRT_THREAD_TS) < 0)) {
	/*
    	 * HRT Thread is sleeping beyond this request's timeout, signal it
	 * to recompute the sleep interval.
	 */
    	rv = pthread_cond_signal(&hrt_thread_cv);
	if (rv) {
	    glob_hrt_cond_signal_failed++;
    	    DBG_LOG(SEVERE, MOD_HRT, "CV(2) signal failed, rv=%d", rv);
    	    DBG_ERR(SEVERE, "CV(2) signal failed, rv=%d", rv);
	}
    }
    pthread_mutex_unlock(&hrt_thread_mutex);
    return 0; // Success
}

static int process_timer_bucket(int bucket) 
{
    int rv;
    net_timer_data_t *td;
    net_timer_data_head_t td_head;
    AO_t global_cnt;
    int elements;

    /*
     * Note: 
     *	1) Caller acquires hrt_thread_mutex prior to the call
     *  2) hrt_thread_mutex is always freed prior to exit
     */     
    NTD_HEAD_INIT(&td_head);

    for (td = timer_bucket[bucket].d.next; td != &timer_bucket[bucket].d; ) {
    	NTD_UNLINK(td, active_timer_data);
	NTD_LINK_TAIL(&td_head, td, global_cnt);
	td = timer_bucket[bucket].d.next;
    }
    pthread_mutex_unlock(&hrt_thread_mutex);
    elements = td_head.entries;
    if (elements) {
    	TRACE_BUCKET(bucket);
    }

    pthread_mutex_lock(&cb_thread_mutex);
    for (td = td_head.d.next; td != &td_head.d; td = td_head.d.next) {
	NTD_UNLINK(td, global_cnt);
	NTD_LINK_TAIL(&cb_timer_data, td, active_cb_timer_data);
    }

    if (cb_timer_data.entries && cb_threads_sleeping) {
    	rv = pthread_cond_signal(&cb_thread_cv);
	if (rv) {
	    glob_hrt_cond_signal_failed++;
    	    DBG_LOG(SEVERE, MOD_HRT, "CV(3) signal failed, rv=%d", rv);
    	    DBG_ERR(SEVERE, "CV(3) signal failed, rv=%d", rv);
	}
    }
    pthread_mutex_unlock(&cb_thread_mutex);

    return elements;
}

static int timespec_cmp(const struct timespec *op1, const struct timespec *op2)
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

static int64_t timespec_diff_msecs(const struct timespec *start, 
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

static void timespec_add_msecs(struct timespec *ts, long msecs)
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
 * HRT thread handler
 */
static void *hrt_handler_func(void *arg)
{
    UNUSED_ARGUMENT(arg);
    int rv;
    pthread_condattr_t cattr;
    struct timespec cur_ts;
    struct timespec ts;
    int64_t clk_diff_msecs;
    int64_t msecs;
    int n;
    int end_bucket;
    int bucket;

    if (!hrt_init_complete) {
    	/* Name thread */
    	prctl(PR_SET_NAME, "nvsd-hrt", 0, 0, 0);

   	/* Use CLOCK_MONOTONIC for pthread_cond_timedwait */
	pthread_condattr_init(&cattr);
	pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);
	pthread_cond_init(&hrt_thread_cv, &cattr);

	/* Setup initialization state */
    	pthread_mutex_lock(&hrt_thread_mutex);
	HRT_BASE_BUCKET = 0;
        clock_gettime(CLOCK_MONOTONIC, &cur_ts);
	HRT_THREAD_TS = cur_ts;
	HRT_BASE_BUCKET_TS = HRT_THREAD_TS;
	HRT_THREAD_SLEEPING = 0;
    	hrt_init_complete = 1;
    }

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &cur_ts);
	clk_diff_msecs = timespec_diff_msecs(&HRT_BASE_BUCKET_TS, &cur_ts);
	if (clk_diff_msecs < 0) {
	    clk_diff_msecs = 0;
	}
	bucket = MSECS_TO_BUCKET(clk_diff_msecs);
	end_bucket = (HRT_BASE_BUCKET + bucket) % TIMER_BUCKETS;

	if (hrt_thread_trace_enabled) {
	    DBG_LOG(MSG, MOD_HRT,
		    "Scan bucket=%d-%d ts=%ld %ld base_ts=%ld %ld "
		    "sleeped=%ld\n", HRT_BASE_BUCKET, end_bucket, 
		    cur_ts.tv_sec, cur_ts.tv_nsec,
		    HRT_BASE_BUCKET_TS.tv_sec, HRT_BASE_BUCKET_TS.tv_nsec, 
		    clk_diff_msecs);
	}

	n = HRT_BASE_BUCKET;
	while (1) {
    	    TRACE_BUCKET(n);
	    while (process_timer_bucket(n)) {
	    	pthread_mutex_lock(&hrt_thread_mutex);
	    }
	    pthread_mutex_lock(&hrt_thread_mutex);

	    if (n == end_bucket) {
	    	break;
	    } else {
	    	n = (n + 1) % TIMER_BUCKETS;
	    }
	}

	/* Note if callout processing exceeds MSECS_PER_BUCKET */
        clock_gettime(CLOCK_MONOTONIC, &ts);
	msecs = timespec_diff_msecs(&cur_ts, &ts);
	if (msecs > MSECS_PER_BUCKET) {
	    glob_hrt_long_callout++;
    	    DBG_LOG(MSG, MOD_HRT, 
	    	    "Callout processing time exceeded, "
		    "%ld msecs %d msecs limit", msecs, MSECS_PER_BUCKET);
	}

	/* Determine the delay time to the next bucket */
	for (n = 1; n < TIMER_BUCKETS; n++) {
	    bucket = (end_bucket + n) % TIMER_BUCKETS;
	    if (timer_bucket[bucket].entries) {
	    	break;
	    }
	}

	HRT_BASE_BUCKET = end_bucket;
	HRT_BASE_BUCKET_TS = cur_ts;
	if (n < TIMER_BUCKETS) {
	    HRT_THREAD_TS = timer_bucket[bucket].d.prev->deadline_ts;
	    if (timespec_cmp(&cur_ts, &HRT_THREAD_TS) >= 0) {
	    	/* Bucket already expired, process it */
	    	HRT_BASE_BUCKET = bucket;
		glob_hrt_callout_behind++;
	    	continue;
	    }
	} else {
	    HRT_THREAD_TS = cur_ts;
	    timespec_add_msecs(&HRT_THREAD_TS, 
	    		       (TIMER_BUCKETS * MSECS_PER_BUCKET));
	}


	if (hrt_thread_trace_enabled) {
	    int sleep_time_msecs = timespec_diff_msecs(&cur_ts, &HRT_THREAD_TS);
	    DBG_LOG(MSG, MOD_HRT,
	    	    "Sleep %d msecs base_bucket=%d cur_ts=%ld %ld "
	            "hrt_ts=%ld %ld\n", sleep_time_msecs, 
		    HRT_BASE_BUCKET, cur_ts.tv_sec, cur_ts.tv_nsec,
		    HRT_THREAD_TS.tv_sec, HRT_THREAD_TS.tv_nsec);
	}

	HRT_THREAD_SLEEPING = 1;
	rv = pthread_cond_timedwait(&hrt_thread_cv, &hrt_thread_mutex, 
				    &HRT_THREAD_TS);
	HRT_THREAD_SLEEPING = 0;
    }
    return NULL;
}

/*
 * CB thread handler
 */
static void *cb_handler_func(void *arg)
{
    UNUSED_ARGUMENT(arg);
    int rv;
    net_timer_data_t *td;
    network_mgr_t *pnm;

    /* Name thread */
    prctl(PR_SET_NAME, "nvsd-hrtcb", 0, 0, 0);

    AO_fetch_and_add1(&cb_init_complete);
    pthread_mutex_lock(&cb_thread_mutex);

    while (1) {
    	if (cb_timer_data.entries) {
	    td = cb_timer_data.d.next;
	    NTD_UNLINK(td, active_cb_timer_data);
	    pthread_mutex_unlock(&cb_thread_mutex);

	    /* Make f_timer callback */
	    pnm = &gnm[td->fhd.fd];

	    pthread_mutex_lock(&pnm->mutex);
	    if (pnm->f_timer && NM_CHECK_FLAG(pnm, NMF_IN_USE) && 
		(td->fhd.incarn == pnm->incarn) && 
		!(td->flags & NTD_FL_CANCELED)) {

		/* Make timer callout */
		rv = (*pnm->f_timer)(pnm->fd, pnm->private_data, td->type);
		/* Ignore return value */
		glob_hrt_callbacks++;
	    }
	    if (FD_to_TD[td->fhd.fd].map[td->type] == td) {
		FD_to_TD[td->fhd.fd].map[td->type] = 0;
	    }
	    pthread_mutex_unlock(&pnm->mutex);
	    free_net_timer_data_t(td);


	    pthread_mutex_lock(&cb_thread_mutex);
	    continue;
	}

	cb_threads_sleeping++;
    	rv = pthread_cond_wait(&cb_thread_cv, &cb_thread_mutex);
	cb_threads_sleeping--;
    }
    return NULL;
}

#ifdef HRT_UNIT_TESTS
/*
 *******************************************************************************
 * 	U N I T  T E S T S
 *******************************************************************************
 */
typedef struct unit_test_data {
    int complete;
    int iteration;
    struct timespec ts;
} unit_test_data_t;

static unit_test_data_t utd;
static int dummy_f_timer_interval_assert = 0;

static int dummy_f_timer(int fd, void *data, net_timer_type_t type)
{
    struct timespec ts;
    int64_t msecs;
    int64_t msecs_max;

    assert(fd > 0);
    assert(data == &utd);
    assert(type == TT_ONESHOT);

    clock_gettime(CLOCK_MONOTONIC, &ts);
    msecs = timespec_diff_msecs(&utd.ts, &ts);
    printf("[%s] iteration=%d msecs=%ld\n", __FUNCTION__, utd.iteration, msecs);

    if (dummy_f_timer_interval_assert) {
    	assert((utd.iteration * MSECS_PER_BUCKET) <= msecs);
    	msecs_max = (utd.iteration + 1) * MSECS_PER_BUCKET;
    	assert(msecs <= msecs_max);
    }

    utd.complete = 1;

    gnm[fd].incarn++;

    return 0;
}

static int net_hrt_interval_test(void)
{
    int rv;
    int fd = 32768;
    int n;
    net_fd_handle_t fhd;
    struct pollfd pfd;
    network_mgr_t initial_fd_state;

    initial_fd_state = gnm[fd];

    NM_SET_FLAG(&gnm[fd], NMF_IN_USE);
    gnm[fd].fd = fd;
    gnm[fd].private_data = &utd;
    gnm[fd].f_timer = dummy_f_timer;

    printf("[%s]: Start\n", __FUNCTION__);

    for (n = 0; n < (MAX_INTERVAL_MSECS / MSECS_PER_BUCKET); n++) {
    	pthread_mutex_lock(&gnm[fd].mutex);
	utd.complete = 0;
	utd.iteration = n;
        clock_gettime(CLOCK_MONOTONIC, &utd.ts);

	fhd = NM_fd_to_fhandle(fd);
    	rv = net_set_timer(fhd, n * MSECS_PER_BUCKET, TT_ONESHOT);
    	assert(rv == 0);
    	pthread_mutex_unlock(&gnm[fd].mutex);

	while (!utd.complete) {
	    poll(&pfd, 0, (n + 1) * MSECS_PER_BUCKET);
	}
    }

    gnm[fd] = initial_fd_state;
    printf("[%s]: Complete\n", __FUNCTION__);

    return 0; // Success
}

/******************************************************************************/

static int dummy_f_timer_fail(int fd, void *data, net_timer_type_t type)
{
    UNUSED_ARGUMENT(fd);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(type);
    
    assert(!"Should never happen.");
    return 0;
}

static int net_hrt_add_cancel_test(void)
{
#define FD_START 10000
#define FD_END 20000

    int rv;
    int fd;
    net_fd_handle_t fhd;
    struct pollfd pfd;

    printf("[%s]: Start\n", __FUNCTION__);

    /* Setup gnm[] state */
    for (fd = FD_START; fd < FD_END; fd++) {
    	NM_SET_FLAG(&gnm[fd], NMF_IN_USE);
    	gnm[fd].fd = fd;
    	gnm[fd].f_timer = dummy_f_timer_fail;
    }

    /* Add/Cancel 1sec timers */
    for (fd = FD_START; fd < FD_END; fd++) {
    	pthread_mutex_lock(&gnm[fd].mutex);
	fhd = NM_fd_to_fhandle(fd);
    	rv = net_set_timer(fhd, 100 * MSECS_PER_BUCKET, TT_ONESHOT);
    	assert(rv == 0);
    	pthread_mutex_unlock(&gnm[fd].mutex);

    	pthread_mutex_lock(&gnm[fd].mutex);
	fhd = NM_fd_to_fhandle(fd);
	rv = net_cancel_timer(fhd, TT_ONESHOT);
	assert(rv == 0);
    	pthread_mutex_unlock(&gnm[fd].mutex);
    }

    /* Delay to insure cancellation completes */
    poll(&pfd, 0, 2000); // 2 seconds

    /* Restore gnm[] state */
    for (fd = FD_START; fd < FD_END; fd++) {
    	NM_UNSET_FLAG(&gnm[fd], NMF_IN_USE);
    	gnm[fd].fd = -1;
    	gnm[fd].f_timer = 0;
    }

    printf("[%s]: Complete\n", __FUNCTION__);
    return 0; // Success
#undef FD_START
#undef FD_END
}

/******************************************************************************/
#define FD_START 10000
#define FD_END 11000

typedef struct hrt_add_add_fd_state {
    int fd;
    int start_interval_bucket;
    struct timespec ts_start;
    int iteration;
    int complete;
} hrt_add_add_fd_state_t;

hrt_add_add_fd_state_t hrt_add_add_fd_state[FD_END-FD_START+1];

#define MIN_EXCEEDED_STATS   16
#define MAX_EXCEEDED_STATS   16
int hrt_add_add_min_exceeded[MIN_EXCEEDED_STATS];
int hrt_add_add_max_exceeded[MAX_EXCEEDED_STATS];
int hrt_add_add_total_samples;

static int dummy_f_timer_add_add(int fd, void *data, net_timer_type_t type)
{
    int rv;
    struct timespec ts;
    hrt_add_add_fd_state_t *st;
    int64_t msecs;
    int64_t min_msecs;
    int64_t max_msecs;
    int bucket;
    net_fd_handle_t fhd;
    int index;
    struct pollfd pfd;

    assert(fd > 0);
    assert(data);
    assert(type == TT_ONESHOT);

    st = (hrt_add_add_fd_state_t *)data;
    assert(st->fd == fd);

    clock_gettime(CLOCK_MONOTONIC, &ts);
    msecs = timespec_diff_msecs(&st->ts_start, &ts);

    bucket = (st->start_interval_bucket + st->iteration) % 
    		(MAX_INTERVAL_MSECS / MSECS_PER_BUCKET);
#if 0
    printf("[%s]: fd=%d delay_bucket=%d delay=%ld\n", __FUNCTION__, fd,
    	   bucket, msecs);
#endif
    /* Compute response time distribution */
    hrt_add_add_total_samples++;
    if ((bucket-1) > 0) {
    	min_msecs = (bucket-1) * MSECS_PER_BUCKET;
    } else {
    	min_msecs = 0;
    }

    if (msecs < min_msecs) {
        index = (min_msecs - msecs) / MSECS_PER_BUCKET;
	if (index < (MIN_EXCEEDED_STATS-1)) {
	    hrt_add_add_min_exceeded[index]++;
	} else {
	    hrt_add_add_min_exceeded[MIN_EXCEEDED_STATS-1]++;
	}
#if 0
    	printf("[%s]: <<<<<<<<<<<<<<<<<<<<<< "
	       "min_msecs=%ld msecs=%ld bucket=%d\n",
	       __FUNCTION__, min_msecs, msecs, bucket);
#endif
    }

    max_msecs = (bucket+1) * MSECS_PER_BUCKET;
    if (msecs > max_msecs) {
        index = (msecs - max_msecs) / MSECS_PER_BUCKET;
	if (index < (MAX_EXCEEDED_STATS-1)) {
	    hrt_add_add_max_exceeded[index]++;
	} else {
	    hrt_add_add_max_exceeded[MAX_EXCEEDED_STATS-1]++;
	}
#if 0
    	printf("[%s]: >>>>>>>>>>>>>>>>>>>>>> "
	       "max_msecs=%d msecs=%ld bucket=%d\n",
	       __FUNCTION__, (bucket+1) * MSECS_PER_BUCKET, msecs, bucket);
#endif
    }

    st->iteration++;
    bucket = (st->start_interval_bucket + st->iteration) % 
    		(MAX_INTERVAL_MSECS / MSECS_PER_BUCKET);
    if (bucket != st->start_interval_bucket) {
    	clock_gettime(CLOCK_MONOTONIC, &st->ts_start);
	fhd = NM_fd_to_fhandle(fd);
	while ((rv = net_set_timer(fhd, 
			      bucket * MSECS_PER_BUCKET, TT_ONESHOT)) != 0)  {
	    // assert(rv == 0);
	    poll(&pfd, 0, 1000); // 1 seconds
    	    clock_gettime(CLOCK_MONOTONIC, &st->ts_start);
    	    printf("[%s]: retry, fd=%d bucket=%d\n", __FUNCTION__, fd, bucket);
	}
    } else {
    	st->complete = 1;
    }
    return 0;
}

static int net_hrt_add_add_test(void)
{
    int rv;
    int fd;
    net_fd_handle_t fhd;

    struct pollfd pfd;
    int bucket;
    int complete;
    int n;

    printf("[%s]: Start\n", __FUNCTION__);

    /* Setup gnm[] and unit test state */
    for (fd = FD_START; fd < FD_END; fd++) {
    	NM_SET_FLAG(&gnm[fd], NMF_IN_USE);
    	gnm[fd].fd = fd;
    	gnm[fd].private_data = &hrt_add_add_fd_state[fd-FD_START];
    	gnm[fd].f_timer = dummy_f_timer_add_add;

	hrt_add_add_fd_state[fd-FD_START].fd = fd;
	hrt_add_add_fd_state[fd-FD_START].start_interval_bucket = 
	    (fd % (MAX_INTERVAL_MSECS / MSECS_PER_BUCKET));
    }

    /* Start timers */
    for (fd = FD_START; fd < FD_END; fd++) {
    	pthread_mutex_lock(&gnm[fd].mutex);

	clock_gettime(CLOCK_MONOTONIC, 
		      &hrt_add_add_fd_state[fd-FD_START].ts_start);

	fhd = NM_fd_to_fhandle(fd);
	bucket = hrt_add_add_fd_state[fd-FD_START].start_interval_bucket;
    	while ((rv = net_set_timer(fhd, 
			bucket * MSECS_PER_BUCKET, TT_ONESHOT)) != 0) {
	    pthread_mutex_unlock(&gnm[fd].mutex);
	    //assert(rv == 0);
	    poll(&pfd, 0, 1000); // 1 seconds
    	    printf("[%s]: retry, fd=%d bucket=%d\n", __FUNCTION__, fd, bucket);

	    pthread_mutex_lock(&gnm[fd].mutex);
	    clock_gettime(CLOCK_MONOTONIC, 
		      &hrt_add_add_fd_state[fd-FD_START].ts_start);
	}
    	pthread_mutex_unlock(&gnm[fd].mutex);
    }

    /* Wait for all timers to complete */
    while (1) {
    	poll(&pfd, 0, 2000); // 2 seconds
	complete = 0;
    	for (fd = FD_START; fd < FD_END; fd++) {
	    if (hrt_add_add_fd_state[fd-FD_START].complete) {
	    	complete++;
	    }
	}
    	printf("[%s]: Completed %d / %d\n", __FUNCTION__, complete, 
	       (FD_END-FD_START));
	if (complete == (FD_END-FD_START)) {
	    break;
	}
    }

    /* Restore gnm[] state */
    for (fd = FD_START; fd < FD_END; fd++) {
    	NM_UNSET_FLAG(&gnm[fd], NMF_IN_USE);
    	gnm[fd].fd = -1;
    	gnm[fd].private_data = 0;
    	gnm[fd].f_timer = 0;
    }

    printf("\n=============================================================\n");
    printf("Total samples = %d\n", hrt_add_add_total_samples);
    for (n = 0; n < MIN_EXCEEDED_STATS; n++) {
    	printf("min_exceeded[%d msecs]=%d\n", 
	       (n + 1) * MSECS_PER_BUCKET, hrt_add_add_min_exceeded[n]);
    }
    printf("\n");
    for (n = 0; n < MAX_EXCEEDED_STATS; n++) {
    	printf("max_exceeded[%d msecs]=%d\n", 
	       (n + 1) * MSECS_PER_BUCKET, hrt_add_add_max_exceeded[n]);
    }
    printf("\n=============================================================\n");

    printf("[%s]: End\n", __FUNCTION__);
    return 0; // Success
}

#undef FD_START
#undef FD_END

/******************************************************************************/

static int net_hrt_tests(void)
{
    struct pollfd pfd;

    printf("[%s]: Start\n", __FUNCTION__);
    poll(&pfd, 0, 2000); // 2 seconds

    if (run_net_hrt_tests & HRT_INTERVAL_TEST) {
    	assert(net_hrt_interval_test() == 0);
    }
    if (run_net_hrt_tests & HRT_ADD_CANCEL_TEST) {
    	assert(net_hrt_add_cancel_test() == 0);
    }
    if (run_net_hrt_tests & HRT_ADD_ADD_TEST) {
    	assert(net_hrt_add_add_test() == 0);
    }

    printf("[%s]: End\n", __FUNCTION__);
    return 0; // Success
}

#else /* !HRT_UNIT_TESTS */

static int net_hrt_tests(void)
{
    return 0; // Success
}
#endif /* HRT_UNIT_TESTS */

/*
 * End network_hres_timer.c
 */
