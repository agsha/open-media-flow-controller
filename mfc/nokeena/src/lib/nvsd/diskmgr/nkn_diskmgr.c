#include "nkn_diskmgr_intf.h"
#include "nkn_sched_api.h"
#include "nkn_mediamgr_api.h"
#include <sys/types.h>
#include <unistd.h>

int glob_dm_cleanup = 0;
int glob_dm_input = 0;
int glob_dm_output = 0;
int glob_dm_thread_hdl = 0;

void dm_dump_counters(void);

void
dm_dump_counters(void)
{
	printf("\nglob_dm_cleanup = %d", glob_dm_cleanup);	
	printf("\nglob_dm_input = %d", glob_dm_input);	
	printf("\nglob_dm_output = %d", glob_dm_output);	
	printf("\nglob_dm_thread_hdl = %d", glob_dm_thread_hdl);	

}

void disk_mgr_cleanup(nkn_task_id_t id) 
{
	glob_dm_cleanup ++;
	return;
}

void
disk_mgr_input(nkn_task_id_t id)
{
	struct nkn_task *ntask = nkn_task_get_task(id);

	glob_dm_input ++;
	assert(ntask);
	nkn_task_set_state(id, TASK_STATE_EVENT_WAIT);
	g_thread_pool_push(old_dm_disk_thread_pool, ntask, NULL);
}

void
disk_mgr_output(nkn_task_id_t id)
{
	glob_dm_output ++;
	return;
}

void
disk_io_req_hdl(gpointer data, gpointer user_data)
{
	struct nkn_task *ntask = (struct nkn_task *) data;
	MM_get_resp_t *in_ptr_resp;	
	nkn_task_id_t task_id;

	glob_dm_thread_hdl ++;
	assert(ntask);

	task_id = nkn_task_get_id(ntask);

	in_ptr_resp = (MM_get_resp_t *) nkn_task_get_data (task_id);
	assert(in_ptr_resp);
          
	DM_get((MM_get_resp_t *) in_ptr_resp);

	nkn_task_set_action_and_state(task_id, TASK_ACTION_OUTPUT,
					TASK_STATE_RUNNABLE);

	return;
}

