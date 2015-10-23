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
#include "nkn_rtsched_api.h"
#include "nkn_lockstat.h"
#include "nkn_slotapi.h"

#define MAX_SLOT	1000000
#define SLOT_MAGIC	0x1010101010101010

/* Enable the below line for valgrind runs/slow systems */
/* #define VALGRIND_RUN 1 */

int64_t glob_sched_thread_waited = 0;
int64_t glob_rtsched_thread_waited = 0;
uint64_t glob_rtsched_task_too_long = 0;
uint64_t glob_rtsched_invalid_queue_avoided = 0;
int glob_rtsched_sync = 0;
int glob_rtsched_max_time_drift = 0;
extern nkn_lockstat_t slotlockstat;
nkn_lockstat_t slotavail_lockstat;
AO_t glob_slot_remove_err;
AO_t glob_slot_add_success;
AO_t glob_slot_remove_success;
AO_t glob_rtsched_resync;

int32_t glob_rtsched_mfp_enable = 0;

/*
 * These four functions can be called by any threads.
 * They are thread-safe.
 */
//void * slot_init(int numof_slots, int slot_interval, int is_real_time);
//int slot_add(void * p, int32_t msec, uint64_t item);
//void slot_remove(void * p);
//uint64_t slot_get_next_item(void * p);

/*
 * numof_slots: the slot array size
 * slot_interval: the time interval between two neighbor slots
 * is_real_time: is real time slot manager or deadline design.
 *
 * return value is the pointer of slot_mgr structure.
 */

void *
slot_init(int numof_slots, int slot_interval, int is_real_time)
{
    nkn_slot_mgr *pslotmgr;
    pthread_condattr_t a;
    static int slot_cnt = 0;
    char slot_mutex_str[64];
    int i;

    if (numof_slots <= 0 || slot_interval <= 0)
	return NULL;
    if (numof_slots > MAX_SLOT)
	return NULL;

    /*
     * initialization of memory
     */
    pslotmgr = (nkn_slot_mgr *)nkn_malloc_type(sizeof(nkn_slot_mgr),
					       mod_slot_mgr);
    if (pslotmgr == NULL) {
	DBG_LOG_MEM_UNAVAILABLE(MOD_SYSTEM);
	return NULL;
    }
    pslotmgr->magic = SLOT_MAGIC;
    pslotmgr->real_time = is_real_time;
    pslotmgr->cur_slot = 0;
    AO_store(&pslotmgr->num_tasks, 0);
    pslotmgr->numof_slots = numof_slots;
    pslotmgr->slot_interval = slot_interval;
    pslotmgr->head = (struct nkn_task_head *)
	nkn_calloc_type(numof_slots, sizeof(struct nkn_task_head),	mod_slot_item);
    pslotmgr->muts = (pthread_mutex_t *)
	nkn_calloc_type(numof_slots, sizeof(pthread_mutex_t), mod_slot_item);
    if ((pslotmgr->head == NULL) || (pslotmgr->muts == NULL)) {
	free(pslotmgr);
	DBG_LOG_MEM_UNAVAILABLE(MOD_SYSTEM);
	return NULL;
    }
    if (is_real_time) {
#if defined(__ZVM__) || defined(VALGRIND_RUN)
	pslotmgr->num_slots_per_ms = 1; /* 1 slot for 1msec */
#else
	if (glob_rtsched_mfp_enable > 0)
		pslotmgr->num_slots_per_ms = 1; /* 1 slot for 1msec */
	else
		pslotmgr->num_slots_per_ms = 100; /* 100 slot for 1msec for
					     granularity of 10usec */
#endif

	pslotmgr->num_tasks_per_interval =
	    (INIT_NUM_TASKS_PER_MS*slot_interval) / pslotmgr->num_slots_per_ms;
	pslotmgr->state = (int *)
	    nkn_calloc_type(numof_slots, sizeof(int), mod_slot_item);
	pslotmgr->availability = (int *)
	    nkn_calloc_type(numof_slots/slot_interval, sizeof(int),
			    mod_slot_item);
	pslotmgr->mut_avail = (pthread_mutex_t *)
	    nkn_calloc_type(numof_slots/slot_interval, sizeof(pthread_mutex_t),
			    mod_slot_item);
	pslotmgr->task_run = 0;
	clock_gettime(CLOCK_MONOTONIC, &pslotmgr->next_ts);
    }

    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);

    pthread_cond_init(&pslotmgr->cond, &a);
    for (i = 0; i < numof_slots; i++) {
	TAILQ_INIT(&pslotmgr->head[i]);
	pthread_mutex_init(&pslotmgr->muts[i], NULL);
	if (is_real_time) {
	    if (!(i % slot_interval)) {
		pthread_mutex_init(&pslotmgr->mut_avail[i/slot_interval], NULL);
	    }
	}
    }
    pthread_mutex_init(&pslotmgr->slotwait_lock, NULL);
    snprintf(slot_mutex_str, sizeof(slot_mutex_str), "slot-mutex-%d", slot_cnt);
    NKN_LOCKSTAT_INIT(&slotlockstat, slot_mutex_str);
    snprintf(slot_mutex_str, sizeof(slot_mutex_str), "slotavail-mutex-%d",
		    slot_cnt);
    NKN_LOCKSTAT_INIT(&slotavail_lockstat, slot_mutex_str);

    slot_cnt++;
    DBG_LOG(MSG, MOD_SYSTEM, "slot %p created (size=%d interval=%d)",
	    pslotmgr, numof_slots, slot_interval);
    return (void *)pslotmgr;
}

AO_t nkn_sched_last_wakeup = 0;
/*
 * insert one item into the queue.
 * deadline is msec.
 * item can be a pointer.
 */
int slot_add(void * p, int32_t msec, struct nkn_task *item)
{
    nkn_slot_mgr *pslotmgr = (void *)p;
    int insert_slot;
    u_int64_t last_wakeup;

    if (!pslotmgr)
	return 0;
    assert(pslotmgr->magic == SLOT_MAGIC);

    last_wakeup = AO_load(&pslotmgr->cur_wakeup);
    /* calculate which slot to insert this item */
    if (msec >= pslotmgr->slot_interval * pslotmgr->numof_slots) {
	insert_slot = pslotmgr->cur_slot - 1;
    } else {
	if (msec >= pslotmgr->slot_interval) {
	    insert_slot = pslotmgr->cur_slot + (msec / pslotmgr->slot_interval);
	} else {
	    /*
	     * never put the task in the current slot. Pick
	     * one of the next 8 slots at random. The sched
	     * thread will anyway execute these tasks as
	     * soon as possible.
	     */
	    insert_slot = pslotmgr->cur_slot + 2;
	}
    }
    insert_slot = (insert_slot + pslotmgr->numof_slots) % pslotmgr->numof_slots;
    assert(insert_slot >= 0);
    assert(insert_slot < pslotmgr->numof_slots);


    add_slot_item(pslotmgr, insert_slot, item);
    /*
     * No point having two threads waking up the sched thread
     * No point all slotmgr consumers vying for slotwait_lock
     */
    if (last_wakeup == AO_load(&pslotmgr->cur_wakeup)) {
	AO_fetch_and_add1(&pslotmgr->cur_wakeup);
	pthread_mutex_lock(&pslotmgr->slotwait_lock);
	pthread_cond_signal(&pslotmgr->cond);
	pthread_mutex_unlock(&pslotmgr->slotwait_lock);
    } else {
	/* it is guaranteed that someone did a sched wakeup
	 * after I incremented num_tasks.
	 * there is still a small window where the sched thread
	 * might go to sleep and not look at my task in the
	 * queue until timedwait expires. This will happen when
	 * my update to num_tasks doesn't make it to the sched
	 * thread "in time".
	 */
    }

    return (1);
}


/*
 * This function is a blocking call.
 * It is either blocked or one item is returned.
 */
struct nkn_task *
slot_get_next_item(void *p)
{
    nkn_slot_mgr *pslotmgr = (void *)p;
    struct nkn_task *pslotitem;
    int cur_slot;
    struct timespec ts;

    if (!pslotmgr)
	return 0;
    assert(pslotmgr->magic == SLOT_MAGIC);

    // Blocked until some item can be returned
    while (1) {
	/*
	 * This is not real time implementation.
	 * This is only deadline implementation.
	 */
	cur_slot = pslotmgr->cur_slot;

	/* Search for the next available slot*/
	while (AO_load(&pslotmgr->num_tasks)) {
		/* non-atomic increment */
	    if ((pslotitem = remove_slot_item(pslotmgr, cur_slot)) != NULL)
		return pslotitem;
		++cur_slot;
	    cur_slot = cur_slot % pslotmgr->numof_slots;
	    pslotmgr->cur_slot = cur_slot;
	}
	/*
	 * Have to use timedwait because there is no guarantee
	 * that there is no waiting task when the sched thread
	 * goes into sleep. There is a small window when a task
	 * could be waiting and the sched thread goes to sleep.
	 */
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_sec += 1;
	pthread_mutex_lock(&pslotmgr->slotwait_lock);
	if (AO_load(&pslotmgr->num_tasks)) {
	    pthread_mutex_unlock(&pslotmgr->slotwait_lock);
	    continue;
	}
	pthread_cond_timedwait(&pslotmgr->cond, &pslotmgr->slotwait_lock, &ts);
	glob_sched_thread_waited++;
	pthread_mutex_unlock(&pslotmgr->slotwait_lock);
    }
}

int glob_max_slots_used;
uint64_t glob_rtsched_slot_unavail;
/*
 * insert one item into the queue.
 * deadline is msec.
 * item can be a pointer.
 */
int rtslot_add(void *p, int32_t min_msec, int32_t max_msec,
	struct nkn_task *item, uint32_t *next_avail)
{
    nkn_slot_mgr *pslotmgr = (void *)p;
    int insert_slot = -1;
    int32_t msec, rel_msec;
    int next_slot_check = 0;

    if (!pslotmgr)
	return 0;
    assert(pslotmgr->magic == SLOT_MAGIC);

    if (min_msec > (pslotmgr->numof_slots / pslotmgr->num_slots_per_ms)) {
	return 0;
    }
    if (!min_msec)
	next_slot_check = pslotmgr->slot_interval;
    msec = min_msec;
    if (!max_msec) {
	max_msec = min_msec + NKN_DEF_MAX_MSEC;
    }
    while (next_slot_check < (max_msec * pslotmgr->num_slots_per_ms)) {
	rel_msec = ((msec * pslotmgr->num_slots_per_ms) +
		    pslotmgr->cur_slot + next_slot_check) %
	    pslotmgr->numof_slots;
	NKN_MUTEX_LOCKSTAT(&pslotmgr->mut_avail[rel_msec / pslotmgr->slot_interval],
			   &slotavail_lockstat);
	insert_slot = ((rel_msec/pslotmgr->slot_interval) *
			   pslotmgr->slot_interval) % pslotmgr->numof_slots;
	if (pslotmgr->availability[insert_slot/pslotmgr->slot_interval] <
	    (pslotmgr->num_tasks_per_interval)) {
	    if (pslotmgr->availability[insert_slot/pslotmgr->slot_interval] >
		glob_max_slots_used)
		glob_max_slots_used =
		    pslotmgr->availability[insert_slot/pslotmgr->slot_interval];
	    AO_fetch_and_add1(&pslotmgr->num_tasks);
	    AO_fetch_and_add1(&glob_slot_add_success);
	    assert(insert_slot < pslotmgr->numof_slots);
	    pthread_mutex_lock(&pslotmgr->slotwait_lock);
	    if(insert_slot <= pslotmgr->cur_slot) {
		glob_rtsched_invalid_queue_avoided ++;
		insert_slot = (pslotmgr->cur_slot + 1) % pslotmgr->numof_slots;
	    }
	    NKN_MUTEX_LOCKSTAT(&pslotmgr->muts[insert_slot],
			       &slotavail_lockstat);
	    pslotmgr->availability[insert_slot/pslotmgr->slot_interval]--;
	    TAILQ_INSERT_TAIL(&pslotmgr->head[insert_slot], item, slot_entry);
	    item->cur_slot = insert_slot;
	    item->pslotmgr = (void *)pslotmgr;
	    pthread_mutex_unlock(&pslotmgr->muts[insert_slot]);
	    pthread_mutex_unlock(&pslotmgr->slotwait_lock);
	    pthread_mutex_unlock(
		&pslotmgr->mut_avail[rel_msec / pslotmgr->slot_interval]);

	    break;
	}
	pthread_mutex_unlock(
		&pslotmgr->mut_avail[rel_msec / pslotmgr->slot_interval]);
	next_slot_check += pslotmgr->slot_interval;
    }

    if (insert_slot == -1) {
	*next_avail = 1;
	glob_rtsched_slot_unavail++;
	return 0;
    }

    pthread_mutex_lock(&pslotmgr->slotwait_lock);
    pthread_cond_broadcast(&pslotmgr->cond);
    pthread_mutex_unlock(&pslotmgr->slotwait_lock);
    return (1);
}

int rtslot_remove(int tid)
{
    int insert_slot;
    struct nkn_task *item;
    nkn_slot_mgr *pslotmgr;

    item = nkn_rttask_get_task(tid);
    if (!item) {
	AO_fetch_and_add1(&glob_slot_remove_err);
	return -1;
    }

    insert_slot = item->cur_slot;
    pslotmgr = (nkn_slot_mgr *)item->pslotmgr;
    if (!pslotmgr) {
	AO_fetch_and_add1(&glob_slot_remove_err);
	return -1;
    }

    NKN_MUTEX_LOCKSTAT(&pslotmgr->muts[insert_slot], &slotavail_lockstat);
    TAILQ_REMOVE(&pslotmgr->head[insert_slot], item, slot_entry);
    pthread_mutex_unlock(&pslotmgr->muts[insert_slot]);
    AO_fetch_and_sub1(&pslotmgr->num_tasks);
    AO_fetch_and_add1(&glob_slot_remove_success);
    item->task_state = TASK_STATE_CLEANUP;
    nkn_rttask_free_task(item->task_id);
    return 0;
}

uint64_t glob_rtsched_task_time = 0;
/*
 * This function is a blocking call.
 * It is either blocked or one item is returned.
 */
struct nkn_task *
rtslot_get_next_item(void *p)
{
    nkn_slot_mgr *pslotmgr = (void *)p;
    struct nkn_task *pslotitem;
    int cur_slot;
    struct timespec ts;
    int time_inc = (pslotmgr->numof_slots/pslotmgr->num_slots_per_ms) *
	pslotmgr->slot_interval;
    int ret = 0;
    int64_t time_diff = 0;

    if (!pslotmgr)
	return 0;
    assert(pslotmgr->magic == SLOT_MAGIC);

    /* Blocked until some item can be returned
     */
    while (1) {
	clock_gettime(CLOCK_MONOTONIC, &ts);
	cur_slot = pslotmgr->cur_slot;
	if (pslotmgr->task_run) {
	    time_diff = (ts.tv_sec - pslotmgr->saved_ts.tv_sec) * 1000000000 +
		(ts.tv_nsec - pslotmgr->saved_ts.tv_nsec);
#if 0
	    if (glob_rtsched_task_time) {
		glob_rtsched_task_time =
		    (glob_rtsched_task_time + time_diff) / 2;
	    } else
#endif
		glob_rtsched_task_time = time_diff;
	}

	if (((ts.tv_sec - pslotmgr->next_ts.tv_sec) > MIN_SYNC_OOT) ||
	    ( pslotmgr->task_run && (time_diff > time_inc))) {
	    if (time_diff > time_inc)
		glob_rtsched_task_too_long++;
	    if ((ts.tv_sec - pslotmgr->next_ts.tv_sec) >
		glob_rtsched_max_time_drift)
		glob_rtsched_max_time_drift =
		    (ts.tv_sec - pslotmgr->next_ts.tv_sec);
	    if ((ts.tv_sec - pslotmgr->next_ts.tv_sec) > MIN_SYNC_OOT) {
		AO_fetch_and_add1(&glob_rtsched_resync);
		pslotmgr->next_ts.tv_nsec = ts.tv_nsec;
		pslotmgr->next_ts.tv_sec = ts.tv_sec;
	    }
	}

	pslotmgr->task_run = 0;

	/* Search for the next available slot*/
	NKN_MUTEX_LOCKSTAT(&pslotmgr->mut_avail[cur_slot/pslotmgr->slot_interval],
			   &slotavail_lockstat);
	NKN_MUTEX_LOCKSTAT(&pslotmgr->muts[cur_slot], &slotavail_lockstat);
	/* If we have some task to be executed return the task to scheduler */
	if ((pslotitem =  TAILQ_FIRST(&pslotmgr->head[cur_slot])) != NULL) {
	    if (ts.tv_sec > pslotmgr->next_ts.tv_nsec ||
		((ts.tv_nsec - pslotmgr->next_ts.tv_nsec) > time_inc)) {
		// TODO: Set task has taken a long time in pslotitem
	    }
	    pslotmgr->saved_ts.tv_sec = ts.tv_sec;
	    pslotmgr->saved_ts.tv_nsec = ts.tv_nsec;
	    TAILQ_REMOVE(&pslotmgr->head[cur_slot], pslotitem, slot_entry);
	    AO_fetch_and_sub1(&pslotmgr->num_tasks);
	    AO_fetch_and_add1(&glob_slot_remove_success);
	    pslotmgr->availability[cur_slot/pslotmgr->slot_interval]--;
	    pthread_mutex_unlock(&pslotmgr->muts[cur_slot]);
	    pthread_mutex_unlock(
		&pslotmgr->mut_avail[cur_slot / pslotmgr->slot_interval]);
	    pslotmgr->task_run = 1;
	    return (pslotitem);
	} else {
	    pslotmgr->saved_ts.tv_sec  = ts.tv_sec;
	    pslotmgr->saved_ts.tv_nsec = ts.tv_nsec;
	}
	pslotmgr->task_run = 0;
	pthread_mutex_unlock(&pslotmgr->muts[cur_slot]);
	pthread_mutex_unlock(
			&pslotmgr->mut_avail[cur_slot/pslotmgr->slot_interval]);
	if (ts.tv_nsec > 1000000000)
	    assert(0);
	if(AO_load(&pslotmgr->num_tasks)) {
	    /* Calculate the time for the next slot and do a cond_timedwait */
	    if (ts.tv_sec > pslotmgr->next_ts.tv_sec ||
		(ts.tv_sec == pslotmgr->next_ts.tv_sec &&
		((ts.tv_nsec > pslotmgr->next_ts.tv_nsec)))) {
		if ((ts.tv_sec - pslotmgr->next_ts.tv_sec) > 1)
		    glob_rtsched_sync = 0;
		/* non-atomic increment */
		pthread_mutex_lock(&pslotmgr->slotwait_lock);
		++cur_slot;
		cur_slot = cur_slot % pslotmgr->numof_slots;
		pslotmgr->cur_slot = cur_slot;
		pthread_mutex_unlock(&pslotmgr->slotwait_lock);
		pslotmgr->next_ts.tv_nsec += time_inc;
		if (pslotmgr->next_ts.tv_nsec >= (1000000000)) {
		    pslotmgr->next_ts.tv_nsec -= 1000000000;
		    pslotmgr->next_ts.tv_sec ++;
		}
		continue;
	    }
	} else {
	    pslotmgr->next_ts.tv_nsec = ts.tv_nsec;
	    pslotmgr->next_ts.tv_sec  = ts.tv_sec + 1;
	}
	glob_rtsched_sync = 1;
	ts.tv_nsec = pslotmgr->next_ts.tv_nsec;
	ts.tv_sec = pslotmgr->next_ts.tv_sec;
	/*
	 * Have to use timedwait because there is no guarantee
	 * that there is no waiting task when the sched thread
	 * goes into sleep. There is a small window when a task
	 * could be waiting and the sched thread goes to sleep.
	 */
	pthread_mutex_lock(&pslotmgr->slotwait_lock);
	ret = pthread_cond_timedwait(&pslotmgr->cond,
				     &pslotmgr->slotwait_lock, &ts);
	glob_rtsched_thread_waited++;
	pthread_mutex_unlock(&pslotmgr->slotwait_lock);
	pslotmgr->next_ts.tv_nsec += time_inc;
	if (pslotmgr->next_ts.tv_nsec >= (1000000000)) {
	    pslotmgr->next_ts.tv_nsec -= 1000000000;
	    pslotmgr->next_ts.tv_sec ++;
	}
    }
}

