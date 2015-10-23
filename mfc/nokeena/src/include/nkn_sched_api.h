/*
  Nokeena Networks Proprietary
  Author: Vikram Venkataraghavan
*/

#ifndef nkn_sched_api_h
#define nkn_sched_api_h

#include <glib.h>
#include <stdint.h>
#include <sys/time.h>
#include <atomic_ops.h>
#include "queue.h"

/* Monitor timeout of 5 mins */
#define NKN_SCHED_MON_TIMEOUT (60 * 10)

#define NKN_SCHED_MAX_THREADS 8

/* Task ID is 64 bits 
 * 24 bits are for the actual task number
 * 4 bits are for revision ID
 * 4 bits are for scheduler ID
 */
/* UUUUUUUUSRTTTTTT 
 * U - Unused
 * S - Schecduler ID
 * R - Revision ID
 * T - Task ID
 * */

#define NKN_SCHED_ID_MASK 0xF
#define NKN_REV_ID_MASK 0xF
#define NKN_TID_MASK 0xFFFFFF

#define NKN_SCHED_ID_SHIFT 24
#define NKN_REV_ID_SHIFT 28
#define SCHED_ID(tid) ((tid >> NKN_SCHED_ID_SHIFT) & NKN_SCHED_ID_MASK)
#define REV_ID(tid) ((tid >> NKN_REV_ID_SHIFT) & NKN_REV_ID_MASK)

/* SCHEDULER INIT FLAGS */
#define NKN_SCHED_START_MMTHREADS 0x00000001
#define NKN_SCHED_START_MONITOR_THREAD 0x00000002



typedef int64_t nkn_task_id_t;
/* NOTE: When adding new enum types 
 * add the corresponding strings in the sched_module_type
 * in nkn_sched.c
 */
typedef enum {
    TASK_TYPE_NONE,
    TASK_TYPE_SOCK_LISTNER,
    TASK_TYPE_SOCK_CONN,
    TASK_TYPE_HTTP_SRV,
    TASK_TYPE_VFS_SVR,
    TASK_TYPE_RTSP_SVR,
    TASK_TYPE_CHUNK_MANAGER,
    TASK_TYPE_DISK_MANAGER,
    TASK_TYPE_MEDIA_MANAGER,
    TASK_TYPE_MOVE_MANAGER,
    TASK_TYPE_ORIGIN_MANAGER,
    TASK_TYPE_GET_MANAGER,
    TASK_TYPE_GET_MANAGER_TFM,
    TASK_TYPE_RTSP_DESCRIBE_HANDLER,
    TASK_TYPE_MFP_LIVE,
    TASK_TYPE_ENCRYPTION_MANAGER,
    TASK_TYPE_AUTH_MANAGER,
    TASK_TYPE_AUTH_HELPER,
    TASK_TYPE_LD_CLUSTER,
    TASK_TYPE_NFS_TUNNEL_MANAGER,
    TASK_TYPE_PE_GEO_MANAGER,
    TASK_TYPE_PE_HELPER,
    TASK_TYPE_PE_UCFLT_MANAGER,
    TASK_TYPE_VC_SVR,
    TASK_TYPE_COMPRESS_MANAGER,
    TASK_TYPE_PUSH_INGEST_MANAGER,
    /* Add new tasks above this line */
    TASK_TYPE_END
} nkn_task_module_t;

#define MAX_SCHED_MOD_NAME_SZ 8

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
    TASK_ACTION_DELAY_FUNC,
    /* Add new above this line */
    TASK_ACTION_NONE
} nkn_task_action_t;

struct nkn_task {
    TAILQ_ENTRY(nkn_task) slot_entry;    // In Timeout queue
    int sched_id; /* scheduler id */
    int rev_id; /* Revision ID */
    nkn_task_id_t task_id;
    nkn_task_module_t src_module_id;
    nkn_task_module_t dst_module_id;
    nkn_task_state_t task_state; /* enum: nkn_task_state_t*/
    nkn_task_action_t task_action; /* enum: nkn_task_action_t */
    pthread_mutex_t task_entry_mutex;
    int dbg_src_module_called;
    int dbg_dst_module_called;
    int dbg_cleanup_module_called;
    int dst_module_op;
    void *data; /* Memory allocated by caller */
    unsigned int data_len; /* Length of the data */
    void *src_module_data; /* Memory allocated by caller */
    int dbg_src_data_set;
    void *dst_module_data; /* Memory allocated by callee */
    int dbg_dst_data_set;
    uint32_t deadline_in_msec; /* Deadline to control constant bit rate func*/
    uint32_t deadline_q_idx;   /* which deadline queue this task was placed in*/
    uint32_t index; /* For internal use */
    int debug_q_status;
    struct timeval cur_time;
    struct timeval task_runnable_time;
    uint32_t wait_flag;
    uint32_t wait_start_time;
    uint32_t sched_priv;
    uint32_t deadline_missed;
    uint64_t data_check;
    uint32_t dbg_start_time;
    uint32_t dbg_end_time;
    uint32_t max_msec;
    void (*delay_func)(void *);
    int cur_slot;
    void *pslotmgr;
    //pid_t prev_pid;  // not needed for now
    //pid_t curr_pid;  // not needed for now
    struct nkn_task *entries;
    TAILQ_ENTRY(nkn_task) wait_entries;
    TAILQ_ENTRY(nkn_task) mm_entries;
};

extern __thread int t_scheduler_id;

/* Scheduler debug counters */
extern AO_t nkn_sched_task_cnt[TASK_TYPE_END][TASK_TYPE_END];
extern uint64_t nkn_sched_task_waited_cnt[TASK_TYPE_END][TASK_TYPE_END];
extern uint64_t nkn_sched_task_longtime_cnt[TASK_TYPE_END][TASK_TYPE_END];
extern uint64_t glob_nkn_sched_mm_stat_task;
extern uint64_t glob_nkn_sched_mm_origin_read_task;
extern uint64_t glob_nkn_sched_mm_origin_validate_task;
extern uint64_t glob_nkn_sched_mm_dm2_read_task;
extern uint64_t glob_nkn_sched_mm_dm2_delete_task;
extern uint64_t glob_nkn_sched_mm_dm2_update_attr_task;

/* Initializes the scheduler */
int nkn_scheduler_init(GThread** gthreads, uint32_t sched_init_flags);
extern int glob_sched_num_core_threads;
extern int glob_sched_task_waited_long;
extern AO_t glob_sched_task_deadline_missed;
extern AO_t glob_sched_runnable_q_pushes;
extern AO_t glob_sched_runnable_q_pops;
extern int glob_sched_task_total_cnt;
extern uint32_t g_nkn_num_stat_threads;
extern uint32_t g_nkn_num_get_threads;
extern uint32_t g_nkn_num_put_threads;
extern uint32_t g_nkn_num_val_threads;
extern uint32_t g_nkn_num_upd_threads;


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
nkn_task_create_new_task(
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
  - Sets the task state.
  - This function is used to for example, go from INIT state i
  to RUNNABLE state.
  - When the scheduler schedules a task in RUNNABLE state, it i
  changes the state to RUNNING state.
*/
void nkn_task_set_state(nkn_task_id_t tid, nkn_task_state_t state);
nkn_task_state_t nkn_task_get_state(nkn_task_id_t tid);
void nkn_task_set_action_and_state(nkn_task_id_t tid, nkn_task_action_t action,
				    nkn_task_state_t state);
nkn_task_action_t nkn_task_get_action(nkn_task_id_t tid);

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
void nkn_task_set_private(nkn_task_module_t module_id,
			  nkn_task_id_t tid, void *private_data);
void *nkn_task_get_private(nkn_task_module_t module_id, nkn_task_id_t tid);

/*
  - Data entity is public data and can be manipulated by caller or callee.
  - It is set when the task is created. Caller allocates memory.
  - The below function is only a getter.
  - The data pointer will need to be freed by the original caller when
  the task is done.
  - The nkn_sched_task_free( ) function will assert whether the data is
  freed.
*/
void *nkn_task_get_data(nkn_task_id_t tid);

/*
  - This function registers an input, output, and cleanup handler with
  the scheduler.
  - At the moment, the scheduler controls the names of these functions.
  - Will change this functionality to register when module get woken up.
*/
int nkn_task_register_task_type(nkn_task_module_t ttype,
				void (*aInInputTaskHdl)(nkn_task_id_t),
				void (*aInOutputTaskHdl)(nkn_task_id_t),
				void (*aInCleanupHdl)(nkn_task_id_t) );

struct nkn_task *nkn_task_get_task(nkn_task_id_t a_in_task_id);

nkn_task_id_t nkn_task_get_id(struct nkn_task *ntask);

/* Take this out when sleeper support is there in cachemgr*/
void nkn_task_pop_sleepers(char *obj_id, GList **a_in_ptr);
GList *nkn_task_push_sleepers(char *obj_id, GList *in_list);
int nkn_task_helper_push_task_on_list(char *obj_id, void *in_ptr);

/* Task timeout function. Implemented with the help of RT Timer
 */
int nkn_task_timed_wait(nkn_task_id_t tid, int msecs);
int nkn_sched_tmout_init(void);

GThreadPool *old_dm_disk_thread_pool;

void nkn_sched_mm_queue(struct nkn_task *ntask, int limit_read);

#endif
