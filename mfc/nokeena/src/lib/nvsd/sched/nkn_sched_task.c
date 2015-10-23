#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "string.h"
#include "glib.h"
#include "nkn_sched_api.h"
#include "nkn_sched_task.h"
#include "assert.h"
#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_memalloc.h"
#include "queue.h"
#include "nkn_lockstat.h"
#include "nkn_assert.h"
#include <sys/prctl.h>
#include "cache_mgr.h"
#ifdef RWQUEUE
#include "nkn_rw_queue.h"
#endif
#include "zvm_api.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define NKN_GET_TASK(task, task_id)					    \
	do {								    \
	    int s_num = SCHED_ID(task_id);				    \
	    int t_id = task_id & NKN_TID_MASK;				    \
	    task = (likely((t_id >= 0) &&				    \
		(t_id < glob_sched_task_total_cnt))?			    \
	    (nkn_glob_sched_task_array[t_id][s_num]) : NULL);		    \
	} while (0);


#define NKN_GET_RW_FREE_TID(tid, sched_num)				    \
	do {								    \
	    struct nkn_task *ntask1 = (struct nkn_task *)		    \
	    nkn_rw_queue_get_next((void *)nkn_sched_task_free_q[sched_num]);\
	    tid = (ntask1? ntask1->task_id: -1);			    \
	} while (0);

#define NKN_GET_FREE_TID(tid, sched_num)				    \
	do {								    \
	    struct nkn_task *ntask1 = nkn_task_free_task_q[sched_num];	    \
	    if (ntask1) {						    \
		nkn_task_free_task_q[sched_num] = ntask1->entries;	    \
		tid = ntask1->task_id;					    \
	    } else {							    \
		tid = -1;						    \
	    }								    \
	} while(0);

int nkn_sched_task_create_count[NKN_SCHED_MAX_THREADS];
int nkn_sched_task_delete_count[NKN_SCHED_MAX_THREADS];
int glob_sched_task_total_cnt = 256000;

int nkn_sched_task_check_time_too_long[NKN_SCHED_MAX_THREADS];
int nkn_sched_task_check_time_flag;
int nkn_num_tasks_monitored[NKN_SCHED_MAX_THREADS];
uint64_t nkn_sched_invalid_task_id[NKN_SCHED_MAX_THREADS];
uint64_t glob_sched_task_deadline_missed;

AO_t nkn_sched_run_num;
static nkn_mutex_t nkn_task_mutex[NKN_SCHED_MAX_THREADS];

#define MAX_WAIT_POOL	20
nkn_mutex_t nkn_task_wait_mutex[NKN_SCHED_MAX_THREADS][MAX_WAIT_POOL];
TAILQ_HEAD(task_wait_list, nkn_task) nkn_task_wait_list[NKN_SCHED_MAX_THREADS][MAX_WAIT_POOL];
int nkn_sched_task_waited_long[NKN_SCHED_MAX_THREADS][MAX_WAIT_POOL];
uint32_t nkn_task_wait_counter[NKN_SCHED_MAX_THREADS];
int nkn_sched_mon_timeout = NKN_SCHED_MON_TIMEOUT;

uint64_t glob_nkn_sched_mm_stat_task = 0;
uint64_t glob_nkn_sched_mm_origin_read_task = 0;
uint64_t glob_nkn_sched_mm_origin_validate_task = 0;

uint64_t glob_nkn_sched_mm_dm2_read_task = 0;
uint64_t glob_nkn_sched_mm_dm2_delete_task = 0;
uint64_t glob_nkn_sched_mm_dm2_update_attr_task = 0;
uint64_t glob_sched_task_alloc_failed = 0;

#ifdef RWQUEUE
nkn_rw_queue_t *nkn_sched_task_free_q[NKN_SCHED_MAX_THREADS];
#endif

/* Glob_Sched_Schedal taks array that contains a record of all tasks */
struct nkn_task *nkn_glob_sched_task_array[NKN_SYSTEM_MAX_TASKS][NKN_SCHED_MAX_THREADS];

struct nkn_task_hdl nkn_task_hdl_array[NKN_SYSTEM_MAX_TASK_MODULES];
#ifndef RWQUEUE
struct nkn_task *nkn_task_free_task_q[NKN_SCHED_MAX_THREADS];
#endif

void sched_task_dump_counters(int sched_num);
static int s_prealloc_task_mem (int sched_num);



/***************************** TASK STUFF ***********************************/
void
sched_task_dump_counters(int sched_num)
{
    printf("\nglob_sched_task_create_count = %d",
	   nkn_sched_task_create_count[sched_num]);
    printf("\nglob_sched_task_delete_count = %d",
	   nkn_sched_task_delete_count[sched_num]);
}


int
nkn_task_init(void)
{
    int ret_val = -1;
    int ttype, i, j;
    pthread_t sched_mon_thrd_obj;
    char task_mutex_str[64];

    for (i=0;i<glob_sched_num_core_threads;i++) {
#ifndef RWQUEUE
	nkn_task_free_task_q[i] = NULL;
#endif
	ret_val = s_prealloc_task_mem(i);

	for (j = 0; j < MAX_WAIT_POOL; ++j) {
		snprintf(task_mutex_str,
			 sizeof(task_mutex_str),"task-wait-mutex-%d-%d", i, j);
		ret_val = NKN_MUTEX_INIT(&nkn_task_wait_mutex[i][j], NULL, task_mutex_str);
		if (ret_val < 0) {
	    		DBG_LOG(SEVERE, MOD_TFM, "Sched Wait Mutex not created. "
		    		"Severe Scheduler failure");
	    		DBG_ERR(SEVERE, "Sched Wait Mutex not created. "
		    		"Severe Scheduler failure");
	    		return -1;
		}
		TAILQ_INIT(&nkn_task_wait_list[i][j]);
		nkn_task_wait_counter[i] = 0;
	}
    }

    for (ttype = 0; ttype < NKN_SYSTEM_MAX_TASK_MODULES; ttype++) {
	nkn_task_hdl_array[ttype].inputTaskHdl = NULL;
	nkn_task_hdl_array[ttype].outputTaskHdl = NULL;
	nkn_task_hdl_array[ttype].cleanupHdl = NULL;
    }

    if ((ret_val = pthread_create(&sched_mon_thrd_obj, NULL,
			    nkn_sched_monitor_thread, NULL))) {
	return -1;
    }
    return ret_val;
}

#if 0
int
nkn_task_check_deadline_miss(nkn_task_id_t tid)
{
    struct timeval tm;
    int ret = -1;

    struct nkn_task *ntask = nkn_task_get_task(tid);

    if (!ntask) {
        NKN_ASSERT(0);
        return ret;
    }

    gettimeofday(&tm, NULL);

    if ((ret = tv_diff(&ntask->task_runnable_time, &tm)) >
       (double)ntask->deadline_in_msec)
        {
            AO_fetch_and_add1(&glob_sched_task_deadline_missed);
            ntask->deadline_missed = 1;
            if (dbg_deadline_miss_assert_enable == 1)
                assert(0);
        }
    return 0;
}
#endif


/**
 * Description:
 *	This function obtains a unique ID for a task.
 * TBD: Need to find an optimized way to find the next free bit
 * in a bitmap.
 */


static void
s_insert_task(nkn_task_id_t a_in_task_id,
	      struct nkn_task *a_in_task,
	      int sched_num)
{
    /* The caller function obtains the lock */
    assert(a_in_task);
    nkn_glob_sched_task_array[a_in_task_id][sched_num] = a_in_task;
}

struct nkn_task *
nkn_task_get_task(nkn_task_id_t a_in_task_id)
{
    struct nkn_task *ntask;
    NKN_GET_TASK(ntask, a_in_task_id)
    return ntask;
}

/**
 * Description: This function will preallocate a set of tasks for the system.
 *  This is an optimization to the normal way of allocating tasks on-demand.
 *  Although it limits us on the number of tasks, it actually is ok because
 *  we need to limit the number of tasks that are 'live' in the system anyway,
 *  If we are not clearing them out, something must be terribly wrong.
 */
int s_prealloc_task_mem(int sched_num)
{
    struct nkn_task *ntask = NULL;
    int i;

    ntask = (struct nkn_task *)
	nkn_calloc_type (glob_sched_task_total_cnt - 1,
			 sizeof(struct nkn_task),
			 mod_sched_nkn_task);
    if (ntask == NULL) {
	return -1;
    }
#ifdef RWQUEUE
    nkn_sched_task_free_q[sched_num] =
	    nkn_rw_queue_init(glob_sched_task_total_cnt - 1);
#endif
    for (i=1; i < glob_sched_task_total_cnt; i++) {
	ntask->task_id = i;
	s_insert_task(ntask->task_id, ntask, sched_num);
#ifdef RWQUEUE
	nkn_rw_queue_insert(nkn_sched_task_free_q[sched_num],
			    (void *)ntask);
#else
	ntask->entries = nkn_task_free_task_q[sched_num];
	nkn_task_free_task_q[sched_num] = ntask;
#endif
	pthread_mutex_init(&ntask->task_entry_mutex, NULL);
	ntask++;
    }
    assert(nkn_glob_sched_task_array[0][sched_num] == NULL);
    return 0;
}


/**
 * Description: This function obtains an available task and
 * sets the callback functions for that task.
 * Mutex protected.
 */
nkn_task_id_t
nkn_task_create_new_task(
			 nkn_task_module_t dst_module_id,
			 nkn_task_module_t src_module_id,
			 nkn_task_action_t task_action,
			 int dst_module_op,
			 void *data,
			 int data_len,
			 unsigned int deadline_in_msec
			 )
{
    struct nkn_task *ntask;
    uint64_t sched_num, unique_num;
    nkn_task_id_t tid;

    sched_num = (AO_fetch_and_add1(&nkn_sched_run_num)) % glob_sched_num_core_threads;
#ifndef RWQUEUE
    NKN_MUTEX_LOCK(&nkn_task_mutex[sched_num]);
    NKN_GET_FREE_TID(tid, sched_num)
#else
    NKN_GET_RW_FREE_TID(tid, sched_num)
#endif
#ifndef RWQUEUE
    NKN_MUTEX_UNLOCK(&nkn_task_mutex[sched_num]);
#endif    
    if (tid < 0) {
	glob_sched_task_alloc_failed ++;
	return -1;
    }
    tid &= NKN_TID_MASK;
    tid |= ((sched_num & NKN_SCHED_ID_MASK) << NKN_SCHED_ID_SHIFT);

    NKN_GET_TASK(ntask, tid)
    if (unlikely(ntask == NULL)) {
	DBG_LOG(SEVERE, MOD_SCHED,"\n Could not create scheduler task. ");
	DBG_ERR(SEVERE, "\n Could not create scheduler task. ");
	nkn_sched_invalid_task_id[sched_num] ++;
	return -1;
    }
    ZVM_ENQUEUE(ntask);

    AO_fetch_and_add1(&nkn_sched_task_cnt[src_module_id][dst_module_id]);
    unique_num = ntask->rev_id;
    tid |= ((unique_num & NKN_REV_ID_MASK) << NKN_REV_ID_SHIFT);
    ntask->task_state = TASK_STATE_INIT;
    ntask->dst_module_id = dst_module_id;
    ntask->src_module_id = src_module_id;
    ntask->task_action = task_action;
    ntask->dst_module_op = dst_module_op;
    ntask->data = data;
    ntask->data_len = data_len;
    ntask->deadline_in_msec = deadline_in_msec;
    ntask->data_check = ~(uint64_t)data;
    ntask->sched_id = sched_num;

    ntask->dbg_src_data_set = 0;
    ntask->dbg_dst_data_set = 0;
    ntask->dbg_src_module_called = 0;
    ntask->dbg_dst_module_called = 0;
    ntask->dbg_cleanup_module_called = 0;
    ntask->dst_module_data = NULL;
    ntask->src_module_data = NULL;
    ntask->sched_priv = 0xaaaaaaaa;
    ntask->task_id = tid;

    nkn_sched_task_create_count[sched_num] ++;
    ZVM_DEQUEUE(ntask);

    return ntask->task_id;
}

/*
 * Description: Free the task because it is complete.
 * Mutex protected.
 */
void
nkn_task_free_task(struct nkn_task *ntask)
{
    int sched_num = SCHED_ID(ntask->task_id);
    assert(ntask->task_state != TASK_STATE_RUNNABLE);
    AO_fetch_and_sub1(
	    &nkn_sched_task_cnt[ntask->src_module_id][ntask->dst_module_id]);
    ntask->task_state = TASK_STATE_NONE;
    ntask->rev_id ++;
    ntask->rev_id &= NKN_REV_ID_MASK;
    ntask->task_action = TASK_ACTION_IDLE;
    ntask->data = NULL;
    ntask->dst_module_data = NULL;
    ntask->src_module_data = NULL;
    ntask->sched_priv = 0xdeaddead;
    nkn_sched_task_delete_count[sched_num] ++;
#ifdef RWQUEUE
    nkn_rw_queue_insert(nkn_sched_task_free_q[sched_num],
	(void *)ntask);
#else
    NKN_MUTEX_LOCK(&nkn_task_mutex[sched_num]);
    ntask->entries = nkn_task_free_task_q[sched_num];
    nkn_task_free_task_q[sched_num] = ntask;
    ZVM_ENQUEUE(ntask);
    NKN_MUTEX_UNLOCK(&nkn_task_mutex[sched_num]);
#endif
}

nkn_task_id_t nkn_task_get_id(struct nkn_task *ntask)
{
    return ntask->task_id;
}

void nkn_task_set_state(nkn_task_id_t tid, nkn_task_state_t state)
{
    struct nkn_task *ntask;
    uint32_t t_index;
    NKN_GET_TASK(ntask, tid)
    int rev_id = REV_ID(tid);
    int sched_num = SCHED_ID(tid);

    if (unlikely(!ntask || ntask->rev_id != rev_id)) {
	NKN_ASSERT(0);
	return;
    }
    pthread_mutex_lock(&ntask->task_entry_mutex);
    if (ntask->task_state == TASK_STATE_EVENT_WAIT) {
	t_index = ntask->index;
	NKN_MUTEX_LOCK(&nkn_task_wait_mutex[sched_num][t_index]);
	TAILQ_REMOVE(&nkn_task_wait_list[sched_num][t_index], ntask, wait_entries);
	NKN_MUTEX_UNLOCK(&nkn_task_wait_mutex[sched_num][t_index]);
	ntask->wait_start_time = 0;
	ntask->wait_flag = 0;
    }
    if (state == TASK_STATE_EVENT_WAIT) {
	ntask->wait_start_time = nkn_cur_ts;
	t_index = nkn_task_wait_counter[sched_num]++ % MAX_WAIT_POOL;
	ntask->index = t_index;
	NKN_MUTEX_LOCK(&nkn_task_wait_mutex[sched_num][t_index]);
	TAILQ_INSERT_TAIL(&nkn_task_wait_list[sched_num][t_index], ntask, wait_entries);
	NKN_MUTEX_UNLOCK(&nkn_task_wait_mutex[sched_num][t_index]);
    }

    if (unlikely(ntask->task_state == TASK_STATE_RUNNABLE &&
	    state == TASK_STATE_RUNNABLE))
    {
	NKN_ASSERT(0);
	pthread_mutex_unlock(&ntask->task_entry_mutex);
	return;
    }
    ntask->task_state = state;

    /* The following function is mutex protected already */
    nkn_task_add_to_queue(ntask);
    pthread_mutex_unlock(&ntask->task_entry_mutex);

    return;
}

void nkn_task_set_action_and_state(nkn_task_id_t tid, nkn_task_action_t action,
				    nkn_task_state_t state)
{
    struct nkn_task *ntask;
    int rev_id = REV_ID(tid);
    int sched_num = SCHED_ID(tid);
    uint32_t t_index;

    NKN_GET_TASK(ntask, tid)

    if (unlikely (!ntask ||
	    (ntask->rev_id != rev_id))) {
	NKN_ASSERT(0);
	return;
    }

    pthread_mutex_lock(&ntask->task_entry_mutex);

    /* Do an assert if cleanup is called before the source module is called back
     */
    if (action == TASK_ACTION_CLEANUP)
        assert(ntask->dbg_src_module_called != 0);

    ntask->task_action = action;

    if (ntask->task_state == TASK_STATE_EVENT_WAIT) {
	t_index = ntask->index;
	NKN_MUTEX_LOCK(&nkn_task_wait_mutex[sched_num][t_index]);
	TAILQ_REMOVE(&nkn_task_wait_list[sched_num][t_index], ntask, wait_entries);
	NKN_MUTEX_UNLOCK(&nkn_task_wait_mutex[sched_num][t_index]);
	ntask->wait_start_time = 0;
	ntask->wait_flag = 0;
    }
    if (state == TASK_STATE_EVENT_WAIT) {
	ntask->wait_start_time = nkn_cur_ts;
	t_index = nkn_task_wait_counter[sched_num]++ % MAX_WAIT_POOL;
	ntask->index = t_index;
	NKN_MUTEX_LOCK(&nkn_task_wait_mutex[sched_num][t_index]);
	TAILQ_INSERT_TAIL(&nkn_task_wait_list[sched_num][t_index], ntask, wait_entries);
	NKN_MUTEX_UNLOCK(&nkn_task_wait_mutex[sched_num][t_index]);
    }

    ntask->task_state = state;

    /* The following function is mutex protected already */
    nkn_task_add_to_queue(ntask);
    pthread_mutex_unlock(&ntask->task_entry_mutex);

    return;
}

nkn_task_state_t nkn_task_get_state(nkn_task_id_t tid)
{
    struct nkn_task *ntask;

    NKN_GET_TASK(ntask, tid)
    if (ntask) {
	return (ntask->task_state);
    }
    NKN_ASSERT(0);
    return TASK_STATE_NONE;
}

nkn_task_action_t nkn_task_get_action(nkn_task_id_t tid)
{
    struct nkn_task *ntask;

    NKN_GET_TASK(ntask, tid)
    if (ntask) {
	return (ntask->task_action);
    }
    NKN_ASSERT(0);
    return TASK_ACTION_NONE;
}

void nkn_task_set_private(nkn_task_module_t module_id,
			  nkn_task_id_t tid, void *in_data)
{
    struct nkn_task *ntask;
    /* Bug: 3452
     * Assert is removed because, OM sets the value as 0
     */
    /*
    assert(in_data);
    */

    NKN_GET_TASK(ntask, tid)
    if (ntask) {
	if (module_id == ntask->src_module_id) {
	    ntask->dbg_src_data_set = 1;
	    ntask->src_module_data = in_data;
	}
	else if (module_id == ntask->dst_module_id){
	    ntask->dbg_dst_data_set = 1;
	    ntask->dst_module_data = in_data;
	}
	return;
    }
    NKN_ASSERT(0);
    return;
}

void *nkn_task_get_private(nkn_task_module_t module_id, nkn_task_id_t tid)
{
    struct nkn_task *ntask;

    NKN_GET_TASK(ntask, tid)
    if (ntask) {
	if (module_id == ntask->src_module_id) {
	    return(ntask->src_module_data);
	}
	else if (module_id == ntask->dst_module_id){
	    return(ntask->dst_module_data);
	}
	else {
	    assert(0);
	}
    } else {
	NKN_ASSERT(0);
	return NULL;
    }
}

void *nkn_task_get_data(nkn_task_id_t tid)
{
    struct nkn_task *ntask;

    ntask = nkn_task_get_task(tid);
    if (ntask) {
	return (ntask->data);
    }
    NKN_ASSERT(0);
    return NULL;
}


/********  TASK HANDLER STUFF: Based on specific task types  ***********/
int nkn_task_register_task_type(nkn_task_module_t ttype,
				void (*aInInputTaskHdl)(nkn_task_id_t),
				void (*aInOutputTaskHdl)(nkn_task_id_t),
				void (*aInCleanupHdl)(nkn_task_id_t) )
{
    DBG_LOG(MSG, MOD_SCHED, "\n%s(): inputTaskHdl: %p, "
	    "outputTaskHdl: %p, cleanHdl: %p ttype = %d",
	    __FUNCTION__, aInInputTaskHdl, aInOutputTaskHdl,
	    aInCleanupHdl, ttype);

    nkn_task_hdl_array[ttype].inputTaskHdl = aInInputTaskHdl;
    nkn_task_hdl_array[ttype].outputTaskHdl = aInOutputTaskHdl;
    nkn_task_hdl_array[ttype].cleanupHdl = aInCleanupHdl;

    return 0;
}

int nkn_task_timed_wait(nkn_task_id_t tid, int msecs)
{
    nkn_task_set_state(tid, TASK_STATE_EVENT_WAIT);
    if (nkn_sched_tmout_add_to_tmout_queue(tid, msecs) != 0)
	return -1;
    return 0;
}

void *
nkn_sched_monitor_thread(void *dummy __attribute((unused)))
{
    struct nkn_task *ntask, *ntask_tmp;
    int sched_num;
    int no_tasks_checked = 0;
    int pool;

    prctl(PR_SET_NAME, "nvsd-sched-mon", 0, 0, 0);
    while (1) {
	for (sched_num=0;sched_num<glob_sched_num_core_threads;sched_num++) {
	/* Reset the counters. This are running counters taken for every check */
	    memset(nkn_sched_task_waited_cnt, 0,
		   sizeof(nkn_sched_task_waited_cnt));
	    glob_nkn_sched_mm_stat_task = 0;
	    glob_nkn_sched_mm_origin_read_task = 0;
	    glob_nkn_sched_mm_origin_validate_task = 0;
	    glob_nkn_sched_mm_dm2_read_task = 0;
	    glob_nkn_sched_mm_dm2_delete_task = 0;
	    glob_nkn_sched_mm_dm2_update_attr_task = 0;

	    for (pool = 0 ; pool < MAX_WAIT_POOL; ++pool) { 
	    	nkn_sched_task_waited_long[sched_num][pool] = 0;
	    	NKN_MUTEX_LOCK(&nkn_task_wait_mutex[sched_num][pool]);
	    	TAILQ_FOREACH_SAFE(ntask, &nkn_task_wait_list[sched_num][pool],
			       	   wait_entries, ntask_tmp) {
		    no_tasks_checked ++;
		    if (no_tasks_checked > 200) {
			NKN_MUTEX_UNLOCK(&nkn_task_wait_mutex[sched_num][pool]);
		    	goto out;
                    }  			
		    if (ntask->wait_flag) {
		    	nkn_sched_task_waited_cnt[ntask->src_module_id][ntask->dst_module_id] ++;
		    	nkn_sched_task_waited_long[sched_num][pool] ++;
		    	if ((ntask->src_module_id == TASK_TYPE_CHUNK_MANAGER) &&
			    (ntask->dst_module_id == TASK_TYPE_MEDIA_MANAGER)) {
			    cache_update_debug_data(ntask);
		    	}
		    	continue;
		    }
		    if ((nkn_cur_ts - ntask->wait_start_time) >
			(nkn_sched_mon_timeout)) {
		        nkn_sched_task_waited_long[sched_num][pool] ++;
		        nkn_sched_task_waited_cnt[ntask->src_module_id][ntask->dst_module_id] ++;
		        if ((ntask->src_module_id == TASK_TYPE_CHUNK_MANAGER) &&
			    (ntask->dst_module_id == TASK_TYPE_MEDIA_MANAGER)) {
			    cache_update_debug_data(ntask);
		        }
		        ntask->wait_flag = 1;
		    } else {
			NKN_MUTEX_UNLOCK(&nkn_task_wait_mutex[sched_num][pool]);
		    	goto out;
		    }
	        }
	        NKN_MUTEX_UNLOCK(&nkn_task_wait_mutex[sched_num][pool]);
	        if (nkn_sched_task_waited_long[sched_num][pool])
		    NKN_ASSERT(0);
	    }
out:
	    nkn_num_tasks_monitored[sched_num] = no_tasks_checked;
	    no_tasks_checked = 0;
	}
	sleep(nkn_sched_mon_timeout);
    }
}
