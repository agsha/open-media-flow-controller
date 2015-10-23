#ifndef nkn_sched_task_h
#define nkn_sched_task_h

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "glib.h"
#include "nkn_sched_api.h"
#include <time.h>
#include "nkn_debug.h"
#include "nkn_lockstat.h"
#include "nkn_defs.h"

#define NKN_SYSTEM_MAX_TASKS 2048000
#define NKN_SYSTEM_MAX_TASK_MODULES 32

#define DEBUG_Q_STATUS_PUSH 1
#define DEBUG_Q_STATUS_POP 2
#define NKN_MAX_MGR_ID 16

//macro for task runtime/debugtime/checktime
#define TASK_SET_RUNNABLE_TIME(task)				\
	    do {						\
		task->task_runnable_time.tv_sec = nkn_cur_ts;	\
	    } while (0);

#define TASK_SET_DEBUG_TIME(task)				\
	    do {						\
		task->cur_time.tv_sec = nkn_cur_ts;		\
	    } while(0);


typedef struct nkn_sched_tmout_obj_s {
    int msecs;
    nkn_task_id_t tid;
    struct timespec abstime;
    TAILQ_ENTRY(nkn_sched_tmout_obj_s) xfer_entry;
    TAILQ_ENTRY(nkn_sched_tmout_obj_s) tmout_entry;
} nkn_sched_tmout_obj_t;

struct nkn_task_hdl {
    void (*inputTaskHdl)(nkn_task_id_t tid);
    void (*outputTaskHdl)(nkn_task_id_t tid);
    void (*cleanupHdl) (nkn_task_id_t tid);
};


/* Globals */
extern int glob_task_create_count;
extern int glob_task_delete_count;

#define NKN_DEBUG_LVL_ON 0

#if (NKN_DEBUG_LVL_ON == 0)
#define __NKN_FUNC_ENTRY__ printf("\n%s(): ENTRY", __FUNCTION__); 
#define __NKN_FUNC_EXIT__ printf("\n%s(): ENTRY", __FUNCTION__); 
#else
#define __NKN_FUNC_ENTRY__  
#define __NKN_FUNC_EXIT__  
#endif

struct nkn_task * nkn_task_pop_from_runnable_queue(int tnum);
int nkn_task_init(void);

nkn_task_module_t nkn_task_get_dst_module_id(struct nkn_task *ntask);
nkn_task_module_t nkn_task_get_src_module_id(struct nkn_task *ntask);

void nkn_task_q_init(void);

void nkn_task_set_debug_time(nkn_task_id_t tid);
void nkn_task_check_debug_time(nkn_task_id_t tid, int deadline, const char *st1);
void nkn_task_set_task_runnable_time(nkn_task_id_t tid);
int nkn_task_check_deadline_miss(nkn_task_id_t tid);

extern void disk_mgr_cleanup(nkn_task_id_t id);
extern void disk_mgr_input(nkn_task_id_t id);
extern void disk_mgr_output(nkn_task_id_t id);
extern void http_mgr_cleanup(nkn_task_id_t id);
extern void http_mgr_input(nkn_task_id_t id);
extern void http_mgr_output(nkn_task_id_t id);
extern void chunk_mgr_cleanup(nkn_task_id_t id);
extern void chunk_mgr_input(nkn_task_id_t id);
extern void chunk_mgr_output(nkn_task_id_t id);
extern void om_mgr_cleanup(nkn_task_id_t id);
extern void om_mgr_input(nkn_task_id_t id);
extern void om_mgr_output(nkn_task_id_t id);
extern void enc_mgr_input(nkn_task_id_t id);
extern void enc_mgr_output(nkn_task_id_t id);
extern void enc_mgr_cleanup(nkn_task_id_t id);
extern void auth_mgr_input(nkn_task_id_t id);
extern void auth_mgr_output(nkn_task_id_t id);
extern void auth_mgr_cleanup(nkn_task_id_t id);
extern void auth_helper_input(nkn_task_id_t id);
extern void auth_helper_output(nkn_task_id_t id);
extern void auth_helper_cleanup(nkn_task_id_t id);
extern void nfs_tunnel_mgr_input(nkn_task_id_t id);
extern void nfs_tunnel_mgr_output(nkn_task_id_t id);
extern void nfs_tunnel_mgr_cleanup(nkn_task_id_t id);
extern void pe_geo_mgr_input(nkn_task_id_t id);
extern void pe_geo_mgr_output(nkn_task_id_t id);
extern void pe_geo_mgr_cleanup(nkn_task_id_t id);
extern void pe_helper_input(nkn_task_id_t id);
extern void pe_helper_output(nkn_task_id_t id);
extern void pe_helper_cleanup(nkn_task_id_t id);
extern void pe_ucflt_mgr_input(nkn_task_id_t id);
extern void pe_ucflt_mgr_output(nkn_task_id_t id);
extern void pe_ucflt_mgr_cleanup(nkn_task_id_t id);
extern void compress_mgr_input(nkn_task_id_t id);
extern void compress_mgr_output(nkn_task_id_t id);
extern void compress_mgr_cleanup(nkn_task_id_t id);
 

void nkn_task_add_to_queue(struct nkn_task *task);

/*
	- This function adds a task back to the free pool.
	- It asserts whether data, caller_private, callee_private have been 
	  freed by caller/callee. 
	- Then it adds the task back to the free pool.
*/
void nkn_task_free_task(struct nkn_task *task);

void * nkn_sched_timeout_thread(void *dummy);
void nkn_sched_tmout_add_to_tmout_q(nkn_sched_tmout_obj_t *tmout_objp);
void nkn_sched_tmout_task_timed_wakeup(nkn_task_id_t tid);
int nkn_sched_tmout_add_to_tmout_queue(nkn_task_id_t tid, int msecs);
void nkn_sched_tmout_cleanup_tmout_obj(nkn_sched_tmout_obj_t *tmout_objp);
nkn_sched_tmout_obj_t * nkn_sched_tmout_create_tmout_obj(void);
void *nkn_sched_monitor_thread(void *dummy);

#if 0
void nkn_task_pop_sleepers(char *obj_id, GList **a_in_ptr);
GList *nkn_task_push_sleepers(char *obj_id, GList *in_list);
int nkn_task_helper_push_task_on_list(char *obj_id, void *in_ptr);
//int nkn_diskmgr_get_data_from_disk(video_obj_t *vobj);
#endif
#endif
