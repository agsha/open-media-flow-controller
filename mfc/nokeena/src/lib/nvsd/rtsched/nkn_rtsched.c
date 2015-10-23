#include <stdio.h>
#include <stdlib.h>
#include "cache_mgr.h"
#include "glib.h"
#include "nkn_rtsched_api.h"
#include "nkn_rtsched_task.h"
#include "nkn_mediamgr_api.h"
#include "nkn_hash_map.h"
#include "nkn_debug.h"
#include "nkn_assert.h"
#include "nkn_cfg_params.h"
#include <sys/prctl.h>

extern void media_mgr_cleanup(nkn_task_id_t id);
extern void media_mgr_input(nkn_task_id_t id);
extern void media_mgr_output(nkn_task_id_t id);

extern void disk_io_req_hdl(gpointer data, gpointer user_data);

extern int nkn_choose_cpu(int i);
static int glob_rtsched_sched_init_flag = 0;
const int glob_rtsched_runnable_q_slice = 20;
const int glob_rtsched_cleanup_q_slice = 20;
const int glob_rtsched_io_ready_q_slice = 20;
const int glob_rtsched_xmit_ready_q_slice = 20;
int glob_rtsched_num_core_threads = 4;

int glob_rtsched_num_sched_loops = 0;
int glob_rtsched_num_http_req = 100;
uint64_t glob_rtsched_possible_buf_corrupt_err = 0;
AO_t glob_rtsched_task_type_unknown;
AO_t glob_rtsched_task_type_delay;
AO_t glob_rtsched_task_type_cleanup;
AO_t glob_rtsched_task_type_output;
AO_t glob_rtsched_task_type_input;

int nkn_rtsched_core_thread(void *message);

int
nkn_rtsched_core_thread(void *message)
{
    int i ;
    struct nkn_task *ntask = NULL;
    int count = 0;
    int tnum = (int)(long)message;
    char name[100];
    //nkn_choose_cpu(tnum + 2); 
    snprintf(name, 64, "rt%d_mempool", tnum);
    nkn_mem_create_thread_pool((const char *)name);

    prctl(PR_SET_NAME, "nvsd-rtsched", 0, 0, 0);

    while (1) {
	for(i = 0; i < glob_rtsched_runnable_q_slice; i++) {
	    /* This function 'pops' the task from RUNNABLE queue */
	    ntask = nkn_rttask_pop_from_queue(tnum, TASK_STATE_RUNNABLE);

	    if(ntask == NULL) {
		break;
	    }
	    else {
                if(ntask->data_check != ~(uint64_t)(ntask->data)) {
                    glob_rtsched_possible_buf_corrupt_err++;
	            DBG_LOG(ERROR, MOD_SCHED, "Possible data corruption: task %ld src %d "
                                              "dst %d, data %p data_check %lx",
                                              ntask->task_id, ntask->src_module_id, 
                                              ntask->dst_module_id, ntask->data, ntask->data_check);
                }
		switch(ntask->task_action) {
		    case TASK_ACTION_INPUT:
			AO_fetch_and_add1(&glob_rtsched_task_type_input);
			assert(ntask->sched_priv != 0xdeaddead);
			nkn_rttask_set_debug_time(ntask->task_id);
			ntask->dbg_dst_module_called ++;
                        //assert(ntask->dbg_dst_module_called == 1);
			nkn_rttask_call_input_hdl_from_module_id(
							       nkn_rttask_get_dst_module_id(ntask),
							       ntask);
			nkn_rttask_check_debug_time(ntask->task_id, 1, "rtsched_input");
			break;

		    case TASK_ACTION_OUTPUT:
			AO_fetch_and_add1(&glob_rtsched_task_type_output);
			assert(ntask->sched_priv != 0xdeaddead);
			assert(ntask->dbg_dst_module_called != 0);
			nkn_rttask_set_debug_time(ntask->task_id);
			ntask->dbg_src_module_called ++;
			nkn_rttask_call_output_hdl_from_module_id(
								nkn_rttask_get_src_module_id(ntask),
								ntask);
			nkn_rttask_check_debug_time(ntask->task_id, 1, "rtsched_output");
			break;

		    case TASK_ACTION_CLEANUP:
			AO_fetch_and_add1(&glob_rtsched_task_type_cleanup);
			assert(ntask->sched_priv != 0xdeaddead);
			/* Task should be RUNNING state */
			ntask->dbg_cleanup_module_called ++;
                        //assert(ntask->dbg_dst_module_called == 1);
                        //assert(ntask->dbg_src_module_called != 0);
			nkn_rttask_set_debug_time(ntask->task_id);
			nkn_rttask_call_cleanup_hdl_from_module_id(
								 nkn_rttask_get_dst_module_id(ntask),
								 ntask);
			nkn_rttask_check_debug_time(ntask->task_id, 1, "rtsched_cleanup");
			/* Mem leak check */
			nkn_rttask_free_task(ntask->task_id);
			break;
		    case TASK_ACTION_DELAY_FUNC:
			AO_fetch_and_add1(&glob_rtsched_task_type_delay);
			NKN_ASSERT(ntask->delay_func);
			if(ntask->delay_func)
			    (*ntask->delay_func)(ntask->data);
			//add_to_worker_thread(ntask->delay_func, ntask->data);
			nkn_rttask_free_task(ntask->task_id);
			break;

		    default:
			AO_fetch_and_add1(&glob_rtsched_task_type_unknown);
			break;
		}
	    }

	}
	count ++;
    }

}

#if 1
int
nkn_rtscheduler_init(GThread** gthreads )
{
    int i;
    GError           *err1 = NULL ;

    if (rtsched_enable == 0) return 0;

    if(glob_rtsched_sched_init_flag == 1) {
	DBG_LOG(MSG, MOD_SCHED, "\n SCHEDULER already inited. "
                "Returning success.");
	return 0;
    }

    glob_rtsched_sched_init_flag = 1;

    nkn_rttask_init();

    nkn_rttask_register_task_type(
				TASK_TYPE_MEDIA_MANAGER,
				&media_mgr_input,
				&media_mgr_output,
                                &media_mgr_cleanup);


    nkn_rttask_register_task_type(
                                TASK_TYPE_MOVE_MANAGER,
                                &move_mgr_input,
                                &move_mgr_output,
                                &move_mgr_cleanup);

    nkn_rttask_register_task_type(
                                TASK_TYPE_CHUNK_MANAGER,
                                &chunk_mgr_input,
                                &chunk_mgr_output,
                                &chunk_mgr_cleanup);

    nkn_rttask_register_task_type(
				TASK_TYPE_HTTP_SRV,
                                &http_mgr_input,
                                &http_mgr_output,
                                &http_mgr_cleanup);

    nkn_rttask_register_task_type(
				TASK_TYPE_ORIGIN_MANAGER,
                                &om_mgr_input,
                                &om_mgr_output,
                                &om_mgr_cleanup);

    nkn_rttask_q_init();

    nkn_rtsched_tmout_init();

    for(i = 0; i < glob_rtsched_num_core_threads; i++) {
	if( (gthreads[i] = g_thread_create_full(
				(GThreadFunc)nkn_rtsched_core_thread,
				(void *)(long)i, (128*1024), TRUE, FALSE,
				G_THREAD_PRIORITY_URGENT, &err1)) == NULL) {
	    g_error_free ( err1 ) ;
	    DBG_LOG(SEVERE, MOD_SCHED,"\n SEVERE ERROR: "
		    "Could not initialize rtscheduler. "
		    "System is not runnable. The system may "
		    "be out of memory.");
	    exit(0);
	}
    }

    return 0;
}
#endif
