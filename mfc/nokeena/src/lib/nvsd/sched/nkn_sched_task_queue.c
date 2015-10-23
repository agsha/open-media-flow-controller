/*
  Nokeena Networks proprietary
  Author: Vikram Venkataragahvan
  07/01/2008
*/

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "nkn_sched_task.h"
#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <atomic_ops.h>
#include "nkn_slotapi.h"
#include "nkn_lockstat.h"
#include "nkn_assert.h"
#include "nkn_rw_queue.h"

#ifdef RWQUEUE
nkn_rw_queue_t *nkn_sched_deadline_mgr[NKN_SCHED_MAX_THREADS];
#else
nkn_slot_mgr *nkn_sched_deadline_mgr[NKN_SCHED_MAX_THREADS];
#endif


/* We can have deadlines to upto 60 seconds */
#define S_DEADLINE_CHUNK 100 /* In milliseconds */
#define NKN_DEADLINE_MAX_SLOTS 600 /* 600 * 100 milliseconds = 60 seconds */


static uint64_t nkn_sched_runnable_q_pushes[NKN_SCHED_MAX_THREADS];
static uint64_t nkn_sched_runnable_q_pops[NKN_SCHED_MAX_THREADS];
AO_t glob_sched_runnable_q_pushes;
AO_t glob_sched_runnable_q_pops;

#if 0
static void
s_dbg_find_duplicate_task(GAsyncQueue *in_queue, nkn_task_id_t tid);
#endif

struct sched_queue
{
    GMutex *mutex;
    GCond *cond;
    GQueue *queue;
    GDestroyNotify item_free_func;
    guint waiting_threads;
    gint32 ref_count;
};

void sched_queue_dump_counters (int sched_num);
void nkn_task_q_cleanup (void);
void nkn_task_q_insert_sorted (GAsyncQueue *in_async_queue,
			       struct nkn_task *in_ntask);


void sched_queue_dump_counters(int sched_num)
{
    printf("\nglob_sched_runnable_q_pushes = %ld", AO_load(&nkn_sched_runnable_q_pushes[sched_num]));
    printf("\nglob_sched_runnable_q_pops = %ld", AO_load(&nkn_sched_runnable_q_pops[sched_num]));
}

/*
 * Description: This function initializes an array of queues.
 */
void
nkn_task_q_init()
{
    int i;
    char name[50];

    assert(glob_sched_num_core_threads <= NKN_SCHED_MAX_THREADS);
    for (i = 0; i < glob_sched_num_core_threads; i++) {
#ifdef RWQUEUE
	nkn_sched_deadline_mgr[i] = nkn_rw_queue_init(NKN_SYSTEM_MAX_TASKS);
#else
	nkn_sched_deadline_mgr[i] = slot_init(100, 10, 0);
#endif
	snprintf(name, 50, "sched_q_push_%d", i);
	(void)nkn_mon_add(name, NULL, (void *)&nkn_sched_runnable_q_pushes[i], sizeof(uint64_t));
	snprintf(name, 50, "sched_q_pop_%d", i);
	(void)nkn_mon_add(name, NULL, (void *)&nkn_sched_runnable_q_pops[i], sizeof(uint64_t));
    }
    return;
}


/**
 * Description: This function pops the next function from queues
 * based on a 'deadline' if the task state is RUNNABLE.
 * If the task state is not RUNNABLE
 */
struct nkn_task *
nkn_task_pop_from_runnable_queue(int tnum)
{
    struct nkn_task *ntask;
#ifdef RWQUEUE
    ntask  =	(struct nkn_task *)
		nkn_rw_queue_get_next_element((void *)nkn_sched_deadline_mgr[tnum]);
#else
    ntask  =	(struct nkn_task *)
		slot_get_next_item(nkn_sched_deadline_mgr[tnum]);
#endif
    if(unlikely((ntask->sched_priv == 0xdeaddead) ||
	(ntask->task_state != TASK_STATE_RUNNABLE))){
	assert(0);
    }
    ++nkn_sched_runnable_q_pops[ntask->sched_id];
    ntask->task_state = TASK_STATE_RUNNING;
    ntask->debug_q_status  = DEBUG_Q_STATUS_POP;
    // currently we are not supporting this if needed enable
    //nkn_task_check_deadline_miss(ntask->task_id);
    AO_fetch_and_add1(&glob_sched_runnable_q_pops);
    return (ntask);
}


/**
 *      - This function pushes a new task into the array of queues.
 *	- The tasks will be prioritized based on a deadline.
 */
void
nkn_task_add_to_queue(struct nkn_task *ntask)
{
    if (ntask->task_state == TASK_STATE_RUNNABLE) {
	if (unlikely(ntask->sched_priv == 0xdeaddead))
	    assert(0);
	++nkn_sched_runnable_q_pushes[ntask->sched_id];
	ntask->debug_q_status  = DEBUG_Q_STATUS_PUSH;
	TASK_SET_RUNNABLE_TIME(ntask);
#ifdef RWQUEUE
	nkn_rw_queue_insert_task(nkn_sched_deadline_mgr[ntask->sched_id],
		(void *)ntask);
#else
	slot_add(nkn_sched_deadline_mgr[ntask->sched_id],
		ntask->deadline_in_msec, (void *)ntask);

#endif
	AO_fetch_and_add1(&glob_sched_runnable_q_pushes);
    }
}


