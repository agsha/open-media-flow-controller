#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>

#include "nkn_defs.h"
#include "nkn_debug.h"
#include "rtsp_session.h"
#include "nkn_rtsched_api.h"
#include "nkn_slotapi.h"


///// sch_queue_item_t /////
typedef struct sch_queue_item {
        struct sch_queue_item * fNext;
        struct sch_queue_item * fPrev;
        struct timeval fDeltaTimeRemaining;
        TaskToken fToken;
        TaskFunc* fProc;
        void* fClientData;
} sch_queue_item_t;

typedef struct sch_thr_mgr {
	pthread_t pid;

	struct timeval fLastSyncTime;
	pthread_mutex_t sch_queue_mutex; // = PTHREAD_MUTEX_INITIALIZER;
	sch_queue_item_t * g_dqe_last; //=NULL;
	sch_queue_item_t * g_rdq; // = NULL;
} sch_thr_mgr_t;

#define MAX_SCH_THR_MGR	10
static sch_thr_mgr_t g_schmgr[MAX_SCH_THR_MGR];
static int num_schmgr = 0;
static pthread_key_t key;

static struct timeval DELAY_ZERO = {0, 0};
static struct timeval ETERNITY = {0x7FFFFFFF, MILLION-1};

static void remove_entry_func(sch_queue_item_t* entry); // but doesn't delete it
static sch_queue_item_t* sch_find_entry(sch_thr_mgr_t * pmgr, TaskToken token);
static void synchronize(sch_thr_mgr_t * pmgr);
static void sch_callback_entry(sch_thr_mgr_t * pmgr);
static struct timeval * sch_get_next_entry_time(sch_thr_mgr_t * pmgr);
static sch_queue_item_t * sch_remove_entry(sch_thr_mgr_t * pmgr, TaskToken tokenToFind);
static sch_queue_item_t * sch_new_entry(struct timeval * delay);
static void sch_add_entry(sch_thr_mgr_t * pmgr, sch_queue_item_t* newEntry);
pthread_mutex_t round_robin_select_lock;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
static void tv_minus(struct timeval * ret, struct timeval * arg1, struct timeval * arg2) ;
static int tv_gt_eq(struct timeval * arg1, struct timeval * arg2);
static void tv_plus_eq(struct timeval * arg1, struct timeval * arg2);
static void tv_minus_eq(struct timeval * arg1, struct timeval * arg2);


static int tv_gt_eq(struct timeval * arg1, struct timeval * arg2) 
{
	return arg1->tv_sec > arg2->tv_sec
		|| (arg1->tv_sec == arg2->tv_sec && arg1->tv_usec >= arg2->tv_usec);
}

static void tv_plus_eq(struct timeval * arg1, struct timeval * arg2) 
{
	arg1->tv_sec += arg2->tv_sec; 
	arg1->tv_usec += arg2->tv_usec;
	if (arg1->tv_usec >= MILLION) {
		arg1->tv_usec -= MILLION;
		++arg1->tv_sec;
	}
}

static void tv_minus_eq(struct timeval * arg1, struct timeval * arg2) 
{
	arg1->tv_sec -= arg2->tv_sec; 
	arg1->tv_usec -= arg2->tv_usec;
	if (arg1->tv_usec < 0) {
		arg1->tv_usec += MILLION;
		--arg1->tv_sec;
	}
	if (arg1->tv_sec < 0) {
		arg1->tv_sec = arg1->tv_usec = 0;
	}
}

static void tv_minus(struct timeval * ret, struct timeval * arg1, struct timeval * arg2) 
{
	long secs = arg1->tv_sec - arg2->tv_sec;
	long usecs = arg1->tv_usec - arg2->tv_usec;

	if (usecs < 0) {
		usecs += MILLION;
		--secs;
	}
	if (secs < 0) {
		ret->tv_sec = 0;
		ret->tv_usec = 0;
	}
	else {
		ret->tv_sec = secs;
		ret->tv_usec = usecs;
	}
}




//////////////////////////////////////////////////////////////////////////////
///// sch_queue_item_t /////
static void synchronize(sch_thr_mgr_t * pmgr) 
{
	// First, figure out how much time has elapsed since the last sync:
	struct timeval timeNow;
	struct timeval timeSinceLastSync;

	gettimeofday(&timeNow, NULL);
	tv_minus(&timeSinceLastSync, &timeNow, &pmgr->fLastSyncTime);
	pmgr->fLastSyncTime = timeNow;

	// Then, adjust the delay queue for any entries whose time is up:
	sch_queue_item_t* curEntry = pmgr->g_rdq->fNext;
	while (tv_gt_eq(&timeSinceLastSync, &curEntry->fDeltaTimeRemaining)) {
		tv_minus_eq(&timeSinceLastSync, &curEntry->fDeltaTimeRemaining);
		curEntry->fDeltaTimeRemaining = DELAY_ZERO;
		curEntry = curEntry->fNext;
	}
	tv_minus_eq(&curEntry->fDeltaTimeRemaining, &timeSinceLastSync);
}

static void remove_entry_func(sch_queue_item_t* entry) 
{
	if (entry == NULL || entry->fNext == NULL) return;

	tv_plus_eq(&entry->fNext->fDeltaTimeRemaining, &entry->fDeltaTimeRemaining);
	entry->fPrev->fNext = entry->fNext;
	entry->fNext->fPrev = entry->fPrev;
	entry->fNext = entry->fPrev = NULL;
	// in case we should try to remove it again
}

static sch_queue_item_t * sch_new_entry(struct timeval * delay) 
{
	static long tokenCounter = 0;
	sch_queue_item_t * p;

	tokenCounter ++;
	if(tokenCounter==0) tokenCounter=1;

	p=(sch_queue_item_t *)nkn_malloc_type(sizeof(sch_queue_item_t), mod_rtsp_sh_sne_malloc);
	if(!p) return NULL;

	p->fDeltaTimeRemaining.tv_sec = delay->tv_sec;
	p->fDeltaTimeRemaining.tv_usec = delay->tv_usec;
	p->fNext = p->fPrev = p;
	p->fToken = (void *)tokenCounter;
	p->fProc = NULL;
	p->fClientData = NULL;

	return p;
}

static sch_queue_item_t * sch_find_entry(sch_thr_mgr_t * pmgr, TaskToken tokenToFind) 
{
	sch_queue_item_t* cur = pmgr->g_rdq->fNext;

	while (cur != pmgr->g_dqe_last) {
		if (cur->fToken == tokenToFind) return cur;
		cur = cur->fNext;
	}
	return NULL;
}

/* Caller will get the mutex locker */
static void sch_add_entry(sch_thr_mgr_t * pmgr, sch_queue_item_t* newEntry) 
{
	synchronize(pmgr);

	sch_queue_item_t* cur = pmgr->g_rdq->fNext;
	while (tv_gt_eq(&newEntry->fDeltaTimeRemaining, &cur->fDeltaTimeRemaining)) {
		tv_minus_eq(&newEntry->fDeltaTimeRemaining, &cur->fDeltaTimeRemaining);
		cur = cur->fNext;
	}

	tv_minus_eq(&cur->fDeltaTimeRemaining, &newEntry->fDeltaTimeRemaining);

	// Add "newEntry" to the queue, just before "cur":
	newEntry->fNext = cur;
	newEntry->fPrev = cur->fPrev;
	cur->fPrev = newEntry->fPrev->fNext = newEntry;
}

/* Caller will get the mutex locker */
static sch_queue_item_t * sch_remove_entry(sch_thr_mgr_t * pmgr, TaskToken tokenToFind) 
{
	if(tokenToFind == 0) return NULL;
	sch_queue_item_t* entry = sch_find_entry(pmgr, tokenToFind);
	remove_entry_func(entry);
	return entry;
}

/* Caller will get the mutex locker */
static struct timeval * sch_get_next_entry_time(sch_thr_mgr_t * pmgr) 
{
	struct timeval * tv;

	tv = &(pmgr->g_rdq->fNext->fDeltaTimeRemaining);
	if (tv->tv_sec == 0 && tv->tv_usec == 0) {
		return &DELAY_ZERO; // a common case
	}

	synchronize(pmgr);
	return &(pmgr->g_rdq->fNext->fDeltaTimeRemaining);
}

void sch_callback_entry(sch_thr_mgr_t * pmgr) 
{
	struct timeval * tv;

	pthread_mutex_lock(&pmgr->sch_queue_mutex);
	if(pmgr->g_rdq->fNext) {
		tv = &(pmgr->g_rdq->fNext->fDeltaTimeRemaining);
		if (tv->tv_sec != 0 || tv->tv_usec != 0) { synchronize(pmgr); }

		if (tv->tv_sec == 0 && tv->tv_usec == 0) {
			// This event is due to be handled:
			sch_queue_item_t* toRemove = pmgr->g_rdq->fNext;
			remove_entry_func(toRemove); // do this first, in case handler accesses queue
			pthread_mutex_unlock(&pmgr->sch_queue_mutex);
			(*toRemove->fProc)(toRemove->fClientData);
			free(toRemove);
			return;
		}
	}
	pthread_mutex_unlock(&pmgr->sch_queue_mutex);
}

TaskToken nkn_scheduleDelayedTask(int64_t microseconds,
                                TaskFunc* proc,
                                void* clientData)
{
	sch_thr_mgr_t * pmgr;
	static int lbid = 0;

	/* Based on thread specific data , we do stickness */
	pmgr = (sch_thr_mgr_t *)pthread_getspecific(key);
	if(!pmgr) {
		pthread_mutex_lock(&round_robin_select_lock);
		++ lbid;
		lbid = lbid % num_schmgr;
		pmgr = &g_schmgr[lbid];
		pthread_mutex_unlock(&round_robin_select_lock);
	}

	pthread_mutex_lock(&pmgr->sch_queue_mutex);
	if (microseconds < 0) microseconds = 0;
	struct timeval timeToDelay;
	timeToDelay.tv_sec = (long)(microseconds/1000000);
	timeToDelay.tv_usec = (long)(microseconds%1000000);
	//assert (timeToDelay.tv_sec < 10);
	sch_queue_item_t * alarmHandler = sch_new_entry(&timeToDelay);
	if(alarmHandler == NULL) {
		printf("nkn_scheduleDelayedTask: alarmHandler == NULL\n");
		pthread_mutex_unlock(&pmgr->sch_queue_mutex);
		return 0;
	}
	alarmHandler->fProc = proc;
	alarmHandler->fClientData = clientData;
	sch_add_entry(pmgr, alarmHandler);

	pthread_mutex_unlock(&pmgr->sch_queue_mutex);

	return alarmHandler->fToken;
}

void nkn_unscheduleDelayedTask(TaskToken prevTask)
{
	sch_thr_mgr_t * pmgr;
	int i;

	if(prevTask == 0) return;
	for(i=0; i<num_schmgr; i++) {
		pmgr = &g_schmgr[i];
		pthread_mutex_lock(&pmgr->sch_queue_mutex);
		if( sch_find_entry(pmgr, prevTask) != NULL) {
		        /* NOTICE: caller should set prevTask to NULL */
			sch_queue_item_t* alarmHandler = sch_remove_entry(pmgr, prevTask);
			if(alarmHandler) { free(alarmHandler); }
			pthread_mutex_unlock(&pmgr->sch_queue_mutex);
			return;
		}
		pthread_mutex_unlock(&pmgr->sch_queue_mutex);
	}
	if(i == num_schmgr) return;

#if 0
	pthread_mutex_lock(&pmgr->sch_queue_mutex);
        /* NOTICE: caller should set prevTask to NULL */
	sch_queue_item_t* alarmHandler = sch_remove_entry(pmgr, prevTask);
	if(alarmHandler) { free(alarmHandler); }
	pthread_mutex_unlock(&pmgr->sch_queue_mutex);
#endif
}

/* Wrapper for existing Api's */
TaskToken nkn_rtscheduleDelayedTask(int64_t microseconds,
                                TaskFunc* proc,
                                void* clientData)
{
    uint64_t tid;
    uint32_t next_avail;
    tid = nkn_rttask_delayed_exec(microseconds / 1000,
			    (microseconds / 1000) + NKN_DEF_MAX_MSEC,
			    proc,
			    clientData,
			    &next_avail);
    return ((TaskToken)tid);
}

void nkn_rtunscheduleDelayedTask(TaskToken prevTask)
{
    nkn_rttask_cancel_exec((nkn_task_id_t)prevTask);
}

// "maxDelayTime" is in microseconds.  It allows a subclass to impose a limit
// on how long "select()" can delay, in case it wants to also do polling.
// 0 (the default value) means: There's no maximum; just look at the delay queue
static void * nkn_real_time_scheduler_main(void * arg);
static void * nkn_real_time_scheduler_main(void * arg)
{
	sch_thr_mgr_t * pmgr = (sch_thr_mgr_t *)arg;
	unsigned int maxDelayTime;
	struct timeval tv_timeToDelay;
	struct timeval * timeToDelay;
	const long MAX_TV_SEC = MILLION;
	int selectResult;
	static pthread_mutex_t keylock;  /* static ensures only one copy of keylock */
	static int once_per_keyname = 0;

	prctl(PR_SET_NAME, "nvsd-rt-scler", 0, 0, 0);

	/*
	 * Adding thread specific data so we could do task sticking on the same thread.
	 */
	pthread_mutex_lock(&keylock);
	if (!once_per_keyname++) {
		/* retest with lock */
		pthread_key_create(&key, NULL);
	}
	pthread_mutex_unlock(&keylock);

	pthread_setspecific(key, (void *)pmgr);

         // Repeatedly loop, handling readble sockets and timed events:
	while (1) {
		maxDelayTime = 0;

		pthread_mutex_lock(&pmgr->sch_queue_mutex);

		timeToDelay = sch_get_next_entry_time(pmgr);
		tv_timeToDelay.tv_sec = timeToDelay->tv_sec;
		tv_timeToDelay.tv_usec = timeToDelay->tv_usec;

		// Very large "tv_sec" values cause select() to fail.
		// Don't make it any larger than 1 million seconds (11.5 days)
		if (tv_timeToDelay.tv_sec > MAX_TV_SEC) {
			tv_timeToDelay.tv_sec = MAX_TV_SEC;
			tv_timeToDelay.tv_sec = 0;
			tv_timeToDelay.tv_usec = 2000;
		}
		// Also check our "maxDelayTime" parameter (if it's > 0):
		if (maxDelayTime > 0 &&
			(tv_timeToDelay.tv_sec > (long)maxDelayTime/MILLION ||
			(tv_timeToDelay.tv_sec == (long)maxDelayTime/MILLION &&
			tv_timeToDelay.tv_usec > (long)maxDelayTime%MILLION))) {
			tv_timeToDelay.tv_sec = maxDelayTime/MILLION;
			tv_timeToDelay.tv_usec = maxDelayTime%MILLION;
		}
		//if ((tv_timeToDelay.tv_sec == 0) && (tv_timeToDelay.tv_sec == 0)) {
		if (tv_timeToDelay.tv_sec > 2) {
			tv_timeToDelay.tv_sec = 0;
			tv_timeToDelay.tv_usec = 2000;
		}
		pthread_mutex_unlock(&pmgr->sch_queue_mutex);

		selectResult = select(0, NULL, NULL, NULL, &tv_timeToDelay);
		if(selectResult > 0) {assert(0); }
		sch_callback_entry(pmgr);
	}

	return NULL;
}


void init_DelayQueue(void);
void init_DelayQueue(void) 
{
	int i;
	int rv;
	pthread_attr_t attr;
	int stacksize = 128 * KiBYTES;

	pthread_mutex_init(&round_robin_select_lock, NULL);

	for(i=0; i<MAX_SCH_THR_MGR; i++) {
		pthread_mutex_init(&g_schmgr[i].sch_queue_mutex, NULL);
		gettimeofday(&g_schmgr[i].fLastSyncTime, NULL);
		g_schmgr[i].g_dqe_last = sch_new_entry(&ETERNITY);
		g_schmgr[i].g_rdq = g_schmgr[i].g_dqe_last;


        	rv = pthread_attr_init(&attr);
        	if (rv) {
                	DBG_LOG(SEVERE, MOD_RTSP, "(1) pthread_attr_init() failed, rv=%d", rv);
                	break;
        	}
        	rv = pthread_attr_setstacksize(&attr, stacksize);
        	if (rv) {
                	DBG_LOG(SEVERE, MOD_RTSP,
                        	"(1) pthread_attr_setstacksize() stacksize=%d failed, "
                        	"rv=%d", stacksize, rv);
                	break;
        	}

        	if(pthread_create(&g_schmgr[i].pid, &attr, 
				nkn_real_time_scheduler_main, 
				(void *)&g_schmgr[i])) {
                	DBG_LOG(MSG, MOD_RTSP, "error in create nkn_real_time_scheduler_main thread");
                	break;
        	}
	}
	num_schmgr = i;
}

void exit_DelayQueue(void);
void exit_DelayQueue(void)
{
	int i;
	sch_thr_mgr_t * pmgr;

	for(i=0; i<num_schmgr; i++) {
		pmgr = &g_schmgr[i];
		pthread_mutex_lock(&pmgr->sch_queue_mutex);
		while (pmgr->g_rdq->fNext != pmgr->g_dqe_last) {
			remove_entry_func(pmgr->g_rdq->fNext);
		}
		pmgr->g_rdq = NULL;
		pmgr->g_dqe_last = NULL;
		pthread_mutex_unlock(&pmgr->sch_queue_mutex);
	}
}

