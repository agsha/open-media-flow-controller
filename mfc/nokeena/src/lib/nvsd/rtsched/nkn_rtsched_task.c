#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <atomic_ops.h>
#include "string.h"
#include "glib.h"
#include "nkn_rtsched_api.h"
#include "nkn_rtsched_task.h"
#include "assert.h"
#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_memalloc.h"
#include "queue.h"
#include "nkn_lockstat.h"
#include "nkn_slotapi.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

extern nkn_slot_mgr *nkn_rtsched_deadline_mgr[8];
extern AO_t glob_rtsched_main_task_q_count;
extern AO_t glob_rtsched_runnable_q_pushes;
AO_t glob_rtsched_task_del_count;
AO_t glob_rtsched_task_create_count;
AO_t glob_rtsched_task_delete_count;
AO_t glob_rtsched_task_delete_func_call_count;
AO_t glob_rtsched_task_create_idle;
AO_t glob_rtsched_task_create_input;
AO_t glob_rtsched_task_create_output;
AO_t glob_rtsched_task_create_cleanup;
AO_t glob_rtsched_task_create_delay;
AO_t glob_rtsched_task_create_none;

int glob_rtsched_task_check_time_too_long = 0;
int glob_rtsched_task_check_time_flag = 0;
int dbg_rtdeadline_miss_assert_enable = 0;
int glob_rtsched_task_deadline_missed = 0;
uint64_t glob_rtsched_invalid_task_id = 0;

static nkn_mutex_t nkn_rttask_mutex;

/* Glob_Sched_Schedal taks array that contains a record of all tasks */
struct nkn_task *nkn_glob_rtsched_task_array[NKN_SYSTEM_MAX_TASKS];

struct nkn_rttask_hdl {
    void (*inputTaskHdl)(nkn_task_id_t tid);
    void (*outputTaskHdl)(nkn_task_id_t tid);
    void (*cleanupHdl) (nkn_task_id_t tid);
};
struct nkn_rttask_hdl nkn_rttask_hdl_array[NKN_SYSTEM_MAX_TASK_MODULES];

struct nkn_task *nkn_rttask_free_task_q;

void rtsched_task_dump_counters(void);
static int s_prealloc_rttask_mem (void);
static int s_get_free_rttid(void);

static inline double
tv_diff (struct timeval *tv1, struct timeval *tv2)
{
    return ((tv2->tv_sec - tv1->tv_sec)
	    + (tv2->tv_usec - tv1->tv_usec) / 1e6);
}


/***************************** TASK STUFF ***********************************/
void
rtsched_task_dump_counters(void)
{
    printf("\nglob_rtsched_task_create_count = %ld", AO_load(&glob_rtsched_task_create_count));
    printf("\nglob_rtsched_task_delete_count = %ld", AO_load(&glob_rtsched_task_delete_count));
    printf("\nglob_rtsched_task_delete_func_call_count = %ld", AO_load(&glob_rtsched_task_delete_func_call_count));
}


int
nkn_rttask_init() {
    int ret_val = -1;
    int ttype;

    nkn_rttask_free_task_q = NULL;
    ret_val = s_prealloc_rttask_mem();

    ret_val = NKN_MUTEX_INIT(&nkn_rttask_mutex, NULL, "rttask-mutex");
    if(ret_val < 0) {
        DBG_LOG(SEVERE, MOD_TFM, "Sched Mutex not created. "
		"Severe Scheduler failure");
        return -1;
    }

    for(ttype = 0; ttype < NKN_SYSTEM_MAX_TASK_MODULES; ttype++) {
	nkn_rttask_hdl_array[ttype].inputTaskHdl = NULL;
	nkn_rttask_hdl_array[ttype].outputTaskHdl = NULL;
	nkn_rttask_hdl_array[ttype].cleanupHdl = NULL;
    }

    return ret_val;
}

void
nkn_rttask_set_debug_time(nkn_task_id_t tid)
{
    struct nkn_task *ntask = nkn_rttask_get_task(tid);

    if(!ntask)
	return;

    gettimeofday(&ntask->cur_time, NULL);
}

void
nkn_rttask_set_task_runnable_time(nkn_task_id_t tid)
{
    struct nkn_task *ntask = nkn_rttask_get_task(tid);

    if(!ntask)
	return;

    gettimeofday(&ntask->task_runnable_time, NULL);
}

int
nkn_rttask_check_deadline_miss(nkn_task_id_t tid)
{
    struct timeval tm;
    int ret = -1;

    struct nkn_task *ntask = nkn_rttask_get_task(tid);

    assert(ntask);

    gettimeofday(&tm, NULL);

    if((ret = tv_diff(&ntask->task_runnable_time, &tm)) >
       (double)ntask->deadline_in_msec)
	{
	    glob_rtsched_task_deadline_missed ++;
	    ntask->deadline_missed = 1;
	    if(dbg_rtdeadline_miss_assert_enable == 1)
		assert(0);
	}
    return 0;
}

void
nkn_rttask_check_debug_time(nkn_task_id_t tid,
			  int deadline __attribute((unused)),
			  const char *st1)
{
    struct timeval r;
    struct nkn_task *ntask = nkn_rttask_get_task(tid);
    double ret = 0.0;

    if(!ntask)
	return;

    gettimeofday(&r, NULL);

    if((ret = tv_diff(&ntask->cur_time, &r)) > 0.005) {
	glob_rtsched_task_check_time_too_long ++;
	if(glob_rtsched_task_check_time_flag != 0) {
	    printf("\n %s(): Task took > 5ms: time = %.6f", st1, ret);
	    printf("\n Source module id: %d, Dst module id: %d, dgb called: %d %d %d\n", ntask->src_module_id, ntask->dst_module_id, ntask->dbg_src_module_called, ntask->dbg_dst_module_called, ntask->dbg_cleanup_module_called);
	}
    }
}

/**
 * Description:
 *	This function obtains a unique ID for a task.
 * TBD: Need to find an optimized way to find the next free bit
 * in a bitmap.
 */
static int
s_get_free_rttid(void)
{
    struct nkn_task *ntask = NULL;

    /* The caller function obtains the lock */
    if ((ntask = nkn_rttask_free_task_q) != NULL) {
	    nkn_rttask_free_task_q = ntask->entries;
	    return (ntask->task_id);
    }
    return -1;
}

static void
s_insert_rttask(nkn_task_id_t a_in_task_id,
	      struct nkn_task *a_in_task)
{
    /* The caller function obtains the lock */
    assert(a_in_task);
    nkn_glob_rtsched_task_array[a_in_task_id] = a_in_task;
}

struct nkn_task *
nkn_rttask_get_task(nkn_task_id_t a_in_task_id)
{
    assert((a_in_task_id >= 0) && (a_in_task_id < NKN_SYSTEM_MAX_TASKS));
    return (nkn_glob_rtsched_task_array[a_in_task_id]);
}

/**
 * Description: This function will preallocate a set of tasks for the system.
 *  This is an optimization to the normal way of allocating tasks on-demand.
 *  Although it limits us on the number of tasks, it actually is ok because
 *  we need to limit the number of tasks that are 'live' in the system anyway,
 *  If we are not clearing them out, something must be terribly wrong.
 */
int s_prealloc_rttask_mem ()
{
	struct nkn_task *ntask = NULL;
	int i;

	ntask = (struct nkn_task *)
	    nkn_calloc_type (NKN_SYSTEM_MAX_TASKS - 1, sizeof(struct nkn_task),
	    mod_rtsched_nkn_task);
	if (ntask == NULL) {
	    return -1;
	}
	for(i=1; i < NKN_SYSTEM_MAX_TASKS; i++) {
		ntask->task_id = i;
		s_insert_rttask(ntask->task_id, ntask);
		ntask->entries = nkn_rttask_free_task_q;
		nkn_rttask_free_task_q = ntask;
		ntask++;
	}
	assert(nkn_glob_rtsched_task_array[0] == NULL);
	return 0;
}

/*
 * Description: Free all the tasks. This function should only be called
 * on cleanup of the whole system.
 */
#if 0
static void
s_rttask_cleanup(nkn_task_id_t a_in_task_id)
{
    int i;

    for(i=0; i < NKN_SYSTEM_MAX_TASKS; i++) {
	if(nkn_glob_rtsched_task_array[a_in_task_id] != NULL) {
	    free(nkn_glob_rtsched_task_array[a_in_task_id]);
	    nkn_glob_rtsched_task_array[a_in_task_id] = NULL;
	}
    }
}
#endif

/**
 * Description: This function obtains an available task and
 * sets the callback functions for that task.
 * Mutex protected.
 */
nkn_task_id_t
nkn_rttask_create_new_task(
			 nkn_task_module_t dst_module_id,
			 nkn_task_module_t src_module_id,
			 nkn_task_action_t task_action,
			 int dst_module_op,
			 void *data,
			 int data_len,
			 unsigned int deadline_in_msec
			 )
{
    struct nkn_task *ntask = NULL;
    nkn_task_id_t tid;

    /* Locking for creating a new task */
    NKN_MUTEX_LOCK(&nkn_rttask_mutex);
    tid = s_get_free_rttid();
    NKN_MUTEX_UNLOCK(&nkn_rttask_mutex);

    if(tid < 0) {
	assert(0);
	return -1;
    }
    ntask = nkn_rttask_get_task(tid);
    if(ntask == NULL) {
	DBG_LOG(SEVERE, MOD_SCHED,"\n Could not create rtscheduler task. ");
	glob_rtsched_invalid_task_id ++;
	return -1;
    }
    switch(task_action) {
	case TASK_ACTION_IDLE:
	    AO_fetch_and_add1(&glob_rtsched_task_create_idle);
	    break;
	case TASK_ACTION_INPUT:
	    AO_fetch_and_add1(&glob_rtsched_task_create_input);
	    break;
	case TASK_ACTION_OUTPUT:
	    AO_fetch_and_add1(&glob_rtsched_task_create_output);
	    break;
	case TASK_ACTION_CLEANUP:
	    AO_fetch_and_add1(&glob_rtsched_task_create_cleanup);
	    break;
	case TASK_ACTION_DELAY_FUNC:
	    AO_fetch_and_add1(&glob_rtsched_task_create_delay);
	    break;
	case TASK_ACTION_NONE:
	    AO_fetch_and_add1(&glob_rtsched_task_create_none);
	    break;
    }

    ntask->task_state = TASK_STATE_INIT;
    ntask->dst_module_id = dst_module_id;
    ntask->src_module_id = src_module_id;
    ntask->task_action = task_action;
    ntask->dst_module_op = dst_module_op;
    ntask->data = data;
    ntask->data_len = data_len;
    ntask->deadline_in_msec = deadline_in_msec;
    ntask->data_check = ~(uint64_t)data;

    ntask->dbg_src_data_set = 0;
    ntask->dbg_dst_data_set = 0;
    ntask->dbg_src_module_called = 0;
    ntask->dbg_dst_module_called = 0;
    ntask->dbg_cleanup_module_called = 0;
    ntask->dst_module_data = NULL;
    ntask->src_module_data = NULL;
    ntask->sched_priv = 0xaaaaaaaa;
    ntask->deadline_missed = 0;

    AO_fetch_and_add1(&glob_rtsched_task_create_count);


    return ntask->task_id;
}

/**
 * Description: This function obtains an available task and
 * sets the callback functions for that task.
 * Mutex protected.
 */
nkn_task_id_t
nkn_rttask_delayed_exec(unsigned int min_start_msec,
		unsigned int max_start_msec,
		void (*delay_func)(void *),
		void *data,
		unsigned int *next_avail
		)
{
    struct nkn_task *ntask = NULL;
    nkn_task_id_t tid;
    tid = nkn_rttask_create_new_task( 0, 0, TASK_ACTION_DELAY_FUNC, 0,
			 data, sizeof(int *), min_start_msec);
    if(tid == -1) {
    }

    ntask = nkn_rttask_get_task(tid);
    ntask->task_state = TASK_STATE_RUNNABLE;
    ntask->max_msec = max_start_msec;
    ntask->delay_func = delay_func;
    nkn_rttask_set_task_runnable_time(ntask->task_id);
    assert(ntask->sched_priv != 0xdeaddead);
    ntask->debug_q_status  = DEBUG_Q_STATUS_PUSH;
    /* Asynchronous task handling between network
	thread and scheduler thread.*/
    if(!rtslot_add(nkn_rtsched_deadline_mgr[ntask->task_id %
		glob_rtsched_num_core_threads],
		ntask->deadline_in_msec, ntask->max_msec, (void *)ntask,
		next_avail))
	return -1;

    AO_fetch_and_add1(&glob_rtsched_main_task_q_count);
    AO_fetch_and_add1(&glob_rtsched_runnable_q_pushes);

    return ntask->task_id;
}

/* Remove a task from Execution */
int
nkn_rttask_cancel_exec(nkn_task_id_t task_id)
{
    int ret;
    if(task_id < 0)
	return -1;
    ret = rtslot_remove(task_id);
    if(!ret) {
	AO_fetch_and_sub1(&glob_rtsched_main_task_q_count);
	AO_fetch_and_add1(&glob_rtsched_task_del_count);
    }
    return(ret);
}


/*
 * Description: Free the task because it is complete.
 * Mutex protected.
 */
void
nkn_rttask_free_task(nkn_task_id_t task_id)
{
    struct nkn_task *ntask = NULL;

    AO_fetch_and_add1(&glob_rtsched_task_delete_func_call_count);


    ntask = nkn_glob_rtsched_task_array[task_id];
    if(ntask != NULL) {
	assert(ntask->task_state != TASK_STATE_RUNNABLE);
	ntask->task_state = TASK_STATE_NONE;
#if 0
	ntask->dst_module_id = TASK_TYPE_NONE;
	ntask->src_module_id = TASK_TYPE_NONE;
#endif
	ntask->task_action = TASK_ACTION_IDLE;
	ntask->dst_module_op = 0;
	ntask->data = NULL;
	ntask->data_len = 0;
	ntask->deadline_in_msec = 0;
	ntask->dbg_src_data_set = 0;
	ntask->dbg_dst_data_set = 0;
	ntask->dbg_src_module_called = 0;
	ntask->dbg_dst_module_called = 0;
	ntask->dbg_cleanup_module_called = 0;
	ntask->dst_module_data = NULL;
	ntask->src_module_data = NULL;
	ntask->sched_priv = 0xdeaddead;
	AO_fetch_and_add1(&glob_rtsched_task_delete_count);
	NKN_MUTEX_LOCK(&nkn_rttask_mutex);
	ntask->entries = nkn_rttask_free_task_q;
	nkn_rttask_free_task_q = ntask;
	NKN_MUTEX_UNLOCK(&nkn_rttask_mutex);
    }
}

nkn_task_id_t nkn_rttask_get_id(struct nkn_task *ntask)
{
    assert (ntask);
    return ntask->task_id;
}

void nkn_rttask_set_state(nkn_task_id_t tid, nkn_task_state_t state)
{
    struct nkn_task *ntask = NULL;
    ntask = nkn_rttask_get_task(tid);
    assert(ntask);

    ntask->task_state = state;

    /* The following function is mutex protected already */
    nkn_rttask_add_to_queue(ntask);

    return;
}

nkn_task_state_t nkn_rttask_get_state(nkn_task_id_t tid)
{
    struct nkn_task *ntask = NULL;

    ntask = nkn_rttask_get_task(tid);
    assert(ntask);
    return (ntask->task_state);
}

void nkn_rttask_set_action(nkn_task_id_t tid, nkn_task_action_t action)
{
    struct nkn_task *ntask = NULL;

    ntask = nkn_rttask_get_task(tid);
    if(!ntask) {
        return;
    }
    ntask->task_action = action;
    return;
}

nkn_task_action_t nkn_rttask_get_action(nkn_task_id_t tid)
{
    struct nkn_task *ntask = NULL;

    ntask = nkn_rttask_get_task(tid);
    assert(ntask);
    return (ntask->task_action);
}

nkn_task_module_t nkn_rttask_get_dst_module_id(struct nkn_task *ntask)
{
    assert(ntask);
    return(ntask->dst_module_id);
}

nkn_task_module_t nkn_rttask_get_src_module_id(struct nkn_task *ntask)
{
    assert(ntask);
    return(ntask->src_module_id);
}

void nkn_rttask_set_private(nkn_task_module_t module_id,
			  nkn_task_id_t tid, void *in_data)
{
    struct nkn_task *ntask = NULL;
    /* Bug: 3452
     * Assert is removed because, OM sets the value as 0
     */
    /*
    assert(in_data);
    */

    ntask = nkn_rttask_get_task(tid);
    assert(ntask);

    if(module_id == ntask->src_module_id) {
	ntask->dbg_src_data_set = 1;
	ntask->src_module_data = in_data;
    }
    else if (module_id == ntask->dst_module_id){
	ntask->dbg_dst_data_set = 1;
	ntask->dst_module_data = in_data;
    }
    else {
	assert(0);
    }
}

void *nkn_rttask_get_private(nkn_task_module_t module_id, nkn_task_id_t tid)
{
    struct nkn_task *ntask = NULL;

    ntask = nkn_rttask_get_task(tid);
    assert(ntask);

    if(module_id == ntask->src_module_id) {
	return(ntask->src_module_data);
    }
    else if (module_id == ntask->dst_module_id){
	return(ntask->dst_module_data);
    }
    else {
	assert(0);
    }
}

void *nkn_rttask_get_data(nkn_task_id_t tid)
{
    struct nkn_task *ntask = NULL;

    ntask = nkn_rttask_get_task(tid);
    assert(ntask);

    return (ntask->data);
}

/********  TASK HANDLER STUFF: Based on specific task types  ***********/
int nkn_rttask_register_task_type(nkn_task_module_t ttype,
				void (*aInInputTaskHdl)(nkn_task_id_t),
				void (*aInOutputTaskHdl)(nkn_task_id_t),
				void (*aInCleanupHdl)(nkn_task_id_t) )
{
    DBG_LOG(MSG, MOD_SCHED, "\n%s(): inputTaskHdl: %p, "
	    "outputTaskHdl: %p, cleanHdl: %p ttype = %d",
	    __FUNCTION__, aInInputTaskHdl, aInOutputTaskHdl,
	    aInCleanupHdl, ttype);

    nkn_rttask_hdl_array[ttype].inputTaskHdl = aInInputTaskHdl;
    nkn_rttask_hdl_array[ttype].outputTaskHdl = aInOutputTaskHdl;
    nkn_rttask_hdl_array[ttype].cleanupHdl = aInCleanupHdl;

    return 0;
}

int
nkn_rttask_call_input_hdl_from_module_id(nkn_task_module_t ttype,
				       struct nkn_task *ntask)
{
    assert(ntask);
    assert(nkn_rttask_hdl_array[ttype].inputTaskHdl);
    nkn_rttask_hdl_array[ttype].inputTaskHdl(ntask->task_id);
    return 0;
}

int
nkn_rttask_call_output_hdl_from_module_id(nkn_task_module_t ttype,
					struct nkn_task *ntask)
{
    assert(ntask);
    assert(nkn_rttask_hdl_array[ttype].outputTaskHdl);
    nkn_rttask_hdl_array[ttype].outputTaskHdl(ntask->task_id);
    return 0;
}

int
nkn_rttask_call_cleanup_hdl_from_module_id(nkn_task_module_t ttype,
					 struct nkn_task *ntask)
{
    assert(nkn_rttask_hdl_array[ttype].cleanupHdl);
    nkn_rttask_hdl_array[ttype].cleanupHdl(ntask->task_id);
    return 0;
}


int nkn_rttask_timed_wait(nkn_task_id_t tid, int msecs)
{
    if(nkn_rtsched_tmout_add_to_tmout_queue(tid, msecs) != 0)
	return -1;
    nkn_rttask_set_state(tid, TASK_STATE_EVENT_WAIT);
    return 0;
}

struct timespec test_data[10];
int my_diff[10];
struct timespec my_ts;
void my_fn(void *);
void nkn_rt_test(void);
void my_fn(void *test)
{
    int val = (long)(test);
    clock_gettime(CLOCK_MONOTONIC, &test_data[val]);
    my_diff[val] = ((test_data[val].tv_sec - my_ts.tv_sec) * 1000000) +
	((test_data[val].tv_nsec - my_ts.tv_nsec) / 1000);
}

void nkn_rt_test(void)
{
    nkn_task_id_t tid;
    uint32_t next_avail;
    clock_gettime(CLOCK_MONOTONIC, &my_ts);
    tid = nkn_rttask_delayed_exec(5, 5, my_fn, (void *)0, &next_avail);
    tid = nkn_rttask_delayed_exec(4, 5, my_fn, (void *)1, &next_avail);
    tid = nkn_rttask_delayed_exec(2, 2, my_fn, (void *)2, &next_avail);
    tid = nkn_rttask_delayed_exec(1, 2, my_fn, (void *)3, &next_avail);
    tid = nkn_rttask_delayed_exec(100, 102, my_fn, (void *)4, &next_avail);
    tid = nkn_rttask_delayed_exec(1000, 1002, my_fn, (void *)5, &next_avail);
    tid = nkn_rttask_delayed_exec(11, 12, my_fn, (void *)6, &next_avail);
    tid = nkn_rttask_delayed_exec(15, 16, my_fn, (void *)7, &next_avail);
    tid = nkn_rttask_delayed_exec(15, 16, my_fn, (void *)8, &next_avail);
    tid = nkn_rttask_delayed_exec(1, 2, my_fn, (void *)9, &next_avail);
}


