/*
  Nokeena Networks Proprietary
  Author: Vikram Venkataraghavan
*/

#ifndef nkn_rtsched_api_h
#define nkn_rtsched_api_h

#include <glib.h>
#include <stdint.h>
#include <sys/time.h>
#include "queue.h"
#include "nkn_sched_api.h"

#if 0
typedef int64_t nkn_task_id_t;
typedef enum {
    TASK_TYPE_NONE,
    TASK_TYPE_SOCK_LISTNER,
    TASK_TYPE_SOCK_CONN,
    TASK_TYPE_HTTP_SRV,
    TASK_TYPE_RTSP_SVR,
    TASK_TYPE_CHUNK_MANAGER,
    TASK_TYPE_DISK_MANAGER,
    TASK_TYPE_MEDIA_MANAGER,
    TASK_TYPE_MOVE_MANAGER,
    TASK_TYPE_ORIGIN_MANAGER,
    TASK_TYPE_GET_MANAGER,
    TASK_TYPE_GET_MANAGER_TFM,
    /* Add new tasks above this line */
    TASK_TYPE_END
} nkn_task_module_t;

/*      TASK STATES
	- When task is created, the creator sets the task state to INIT.
	- After creation, the task will be put on the RUNNABLE queue only
	after a module changes the state to RUNNABLE.
	- When task state is RUNNABLE, scheduler changes to RUNNING
	before dispatching.
	- The callee needs to set state to RUNNABLE.
	(assert this after callee returns)
	- Task state is set to CLEANUP when task is set to be cleaned up.
*/
typedef enum {
    TASK_STATE_NONE,
    TASK_STATE_INIT,
    TASK_STATE_RUNNABLE,
    TASK_STATE_RUNNING,
    TASK_STATE_EVENT_WAIT,
    //TASK_STATE_XMIT_READY,
    //TASK_STATE_IO_READY,
    TASK_STATE_CLEANUP,
    //TASK_TYPE_CREATED_CHILD,
    /* Add new tasks above this line */
    TASK_STATE_MAX
} nkn_task_state_t;

/* This is the function selector; eg: dm_input, dm_output, dm_cleanup */
typedef enum {
    TASK_ACTION_IDLE,
    TASK_ACTION_INPUT,
    TASK_ACTION_OUTPUT,
    TASK_ACTION_CLEANUP,
    /* Add new above this line */
    TASK_ACTION_NONE
} nkn_task_action_t;

struct nkn_task {
    struct nkn_task *slot_next;    // In Timeout queue
    int task_id;
    nkn_task_module_t src_module_id;
    nkn_task_module_t dst_module_id;
    int dbg_src_module_called;
    int dbg_dst_module_called;
    int dbg_cleanup_module_called;
    nkn_task_state_t task_state; /* enum: nkn_task_state_t*/
    nkn_task_action_t task_action; /* enum: nkn_task_action_t */
    int dst_module_op;
    void *data; /* Memory allocated by caller */
    unsigned int data_len; /* Length of the data */
    void *src_module_data; /* Memory allocated by caller */
    int dbg_src_data_set;
    void *dst_module_data; /* Memory allocated by callee */
    int dbg_dst_data_set;
    uint32_t deadline_in_msec; /* Deadline to control constant bit rate func*/
    uint32_t deadline_q_idx;   /* which deadline queue this task was placed in*/
    int inUse; /* For internal use */
    int debug_q_status;
    struct timeval cur_time;
    struct timeval task_runnable_time;
    uint32_t sched_priv;
    uint32_t deadline_missed;
    uint64_t data_check;
    uint32_t dbg_start_time;
    uint32_t dbg_end_time;
    struct nkn_task *entries;
};
#endif

/* Initializes the scheduler */
int nkn_rtscheduler_init(GThread** gthreads);
extern int glob_rtsched_num_core_threads;

/*
  - This function creates a new task.
  - dst_module_id: the module that this task is destined to.
  - src_module_id: the module that is creating this task.
  - task_action: for example: call dm_input, dm_ouput, or dm_cleanup.
  - dst_module_op: the operation that the destination needs to run.
  for example: DM_OP_READ
  - data: data pointer shared (for read only) between caller and callee.
  - data_length: data length of the data inserted into the task.
  - deadline_in_msec: calculated by the creator of the task. This value
  is in millisecs.
*/
nkn_task_id_t
nkn_rttask_create_new_task(
			 nkn_task_module_t dst_module_id,
			 nkn_task_module_t src_module_id,
			 nkn_task_action_t task_action,
			 //nkn_task_state_t prev_task_state,
			 //unsigned int parent_task_id,
			 int dst_module_op, /* Specifies an operation for the callee function */
			 void *data, /* Caller allocates memory */
			 int data_len,
			 unsigned int deadline_in_msec /* Deadline calculated by the task creator*/
			 );
/*
	- This function adds a task back to the free pool.
	- It asserts whether data, caller_private, callee_private have been 
	  freed by caller/callee. 
	- Then it adds the task back to the free pool.
*/
void nkn_rttask_free_task(nkn_task_id_t a_in_task_id);

/*
  - Sets the task state.
  - This function is used to for example, go from INIT state i
  to RUNNABLE state.
  - When the scheduler schedules a task in RUNNABLE state, it i
  changes the state to RUNNING state.
*/
void nkn_rttask_set_state(nkn_task_id_t tid, nkn_task_state_t state);
nkn_task_state_t nkn_rttask_get_state(nkn_task_id_t tid);
void nkn_rttask_set_action(nkn_task_id_t tid, nkn_task_action_t action);
nkn_task_action_t nkn_rttask_get_action(nkn_task_id_t tid);

/*
  - Private data can be set by caller or callee module.
  - The below functions will set caller_private or callee_private
  based on the module_id provided.
  - The scheduler knows the caller and callee at any time.
  - Scheduler will NOT allocate memory for the private_data.
  - NOTE: CALLER AND CALLEE NEED TO ALLOCATE DATA for private_data.
  - CALLER AND CALLEE NEED TO FREE THIS DATA IN THEIR CLEANUP FUNCTIONS.
  - The nkn_sched_task_free( ) function will assert whether the data is
  freed.
*/
void nkn_rttask_set_private(nkn_task_module_t module_id,
			  nkn_task_id_t tid, void *private_data);
void *nkn_rttask_get_private(nkn_task_module_t module_id, nkn_task_id_t tid);

/*
  - Data entity is public data and can be manipulated by caller or callee.
  - It is set when the task is created. Caller allocates memory.
  - The below function is only a getter.
  - The data pointer will need to be freed by the original caller when
  the task is done.
  - The nkn_sched_task_free( ) function will assert whether the data is
  freed.
*/
void *nkn_rttask_get_data(nkn_task_id_t tid);

/*
  - This function registers an input, output, and cleanup handler with
  the scheduler.
  - At the moment, the scheduler controls the names of these functions.
  - Will change this functionality to register when module get woken up.
*/
int nkn_rttask_register_task_type(nkn_task_module_t ttype,
				void (*aInInputTaskHdl)(nkn_task_id_t),
				void (*aInOutputTaskHdl)(nkn_task_id_t),
				void (*aInCleanupHdl)(nkn_task_id_t) );

struct nkn_task *nkn_rttask_get_task(nkn_task_id_t a_in_task_id);

nkn_task_id_t nkn_rttask_get_id(struct nkn_task *ntask);

/* Take this out when sleeper support is there in cachemgr*/
void nkn_rttask_pop_sleepers(char *obj_id, GList **a_in_ptr);
GList *nkn_rttask_push_sleepers(char *obj_id, GList *in_list);
int nkn_rttask_helper_push_task_on_list(char *obj_id, void *in_ptr);

/* Task timeout function. Implemented with the help of RT Timer
 */
int nkn_rttask_timed_wait(nkn_task_id_t tid, int msecs);
int nkn_rtsched_tmout_init(void);
nkn_task_id_t nkn_rttask_delayed_exec(unsigned int min_start_msec,
		unsigned int max_start_msec,
		void (*delay_func)(void *),
		void *data, unsigned int *next_avail);
int nkn_rttask_cancel_exec(nkn_task_id_t tid);

#endif
