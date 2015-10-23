#ifndef nkn_rtsched_task_h
#define nkn_rtsched_task_h

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "glib.h"
#include "nkn_rtsched_api.h"
#include <time.h>
#include "nkn_debug.h"

#define NKN_SYSTEM_MAX_TASKS 256000
#define NKN_SYSTEM_MAX_TASK_MODULES 16

#define DEBUG_Q_STATUS_PUSH 1
#define DEBUG_Q_STATUS_POP 2
#define NKN_MAX_MGR_ID 16


typedef struct nkn_rtsched_tmout_obj_s {
    int msecs;
    nkn_task_id_t tid;
    struct timespec abstime;
    TAILQ_ENTRY(nkn_rtsched_tmout_obj_s) xfer_entry;
    TAILQ_ENTRY(nkn_rtsched_tmout_obj_s) tmout_entry;
} nkn_rtsched_tmout_obj_t;

/* Globals */
extern int glob_rttask_create_count;
extern int glob_rttask_delete_count;

#define NKN_DEBUG_LVL_ON 0

#if (NKN_DEBUG_LVL_ON == 0)
#define __NKN_FUNC_ENTRY__ printf("\n%s(): ENTRY", __FUNCTION__); 
#define __NKN_FUNC_EXIT__ printf("\n%s(): ENTRY", __FUNCTION__); 
#else
#define __NKN_FUNC_ENTRY__  
#define __NKN_FUNC_EXIT__  
#endif

struct nkn_task * nkn_rttask_pop_from_queue(int tnum, nkn_task_state_t a_in_task_state);
int nkn_rttask_init(void);

int nkn_rttask_call_input_hdl_from_module_id(nkn_task_module_t ttype,
                            struct nkn_task *ntask);
int nkn_rttask_call_output_hdl_from_module_id(nkn_task_module_t ttype,
                            struct nkn_task *ntask);
int nkn_rttask_call_cleanup_hdl_from_module_id(nkn_task_module_t ttype,
                                    struct nkn_task *ntask);

nkn_task_module_t nkn_rttask_get_dst_module_id(struct nkn_task *ntask);
nkn_task_module_t nkn_rttask_get_src_module_id(struct nkn_task *ntask);

void nkn_rttask_q_init(void);

void nkn_rttask_set_debug_time(nkn_task_id_t tid);
void nkn_rttask_check_debug_time(nkn_task_id_t tid, int deadline, const char *st1);
void nkn_rttask_set_task_runnable_time(nkn_task_id_t tid);
int nkn_rttask_check_deadline_miss(nkn_task_id_t tid);

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

void nkn_rttask_add_to_queue(struct nkn_task *task);

void add_to_worker_thread(void (*func) ( void *), void *arg);

void * nkn_rtsched_timeout_thread(void *dummy);
void nkn_rtsched_tmout_add_to_tmout_q(nkn_rtsched_tmout_obj_t *tmout_objp);
void nkn_rtsched_tmout_task_timed_wakeup(nkn_task_id_t tid);
int nkn_rtsched_tmout_add_to_tmout_queue(nkn_task_id_t tid, int msecs);
void nkn_rtsched_tmout_cleanup_tmout_obj(nkn_rtsched_tmout_obj_t *tmout_objp);
nkn_rtsched_tmout_obj_t * nkn_rtsched_tmout_create_tmout_obj(void);

#if 0
void nkn_task_pop_sleepers(char *obj_id, GList **a_in_ptr);
GList *nkn_task_push_sleepers(char *obj_id, GList *in_list);
int nkn_task_helper_push_task_on_list(char *obj_id, void *in_ptr);
//int nkn_diskmgr_get_data_from_disk(video_obj_t *vobj);
#endif
#endif
