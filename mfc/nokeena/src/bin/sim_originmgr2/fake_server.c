#include <sys/time.h>
#include "nkn_sched_api.h"
#include "nkn_mediamgr_api.h"

int cm_max_cache_size;
void * p_data_mmap=NULL;

void http_mgr_cleanup(nkn_task_id_t id) ;
void http_mgr_input(nkn_task_id_t id) ;
void http_mgr_output(nkn_task_id_t id) ;
int OM_stat(nkn_uol_t uol, MM_stat_resp_t *in_ptr_resp) ;
void om_get_cleanup(nkn_task_id_t id) ;
void om_get_input(nkn_task_id_t id) ;
void om_get_output(nkn_task_id_t id) ;
#if 0
int32_t dbg_log_level=4;
int32_t dbg_log_mod=0;
void nkn_errorlog(const char * fmt, ...);

int errorlog_socket;
void nkn_errorlog(const char * fmt, ...)
{
        return ;
}
#endif // 0

int nkn_mon_add(const char *name, char *instance, void *obj, uint32_t size);
int nkn_mon_add(const char *name, char *instance, void *obj, uint32_t size)
{
	return 0;
}


void http_mgr_cleanup(nkn_task_id_t id)
{
	assert(0);
        return ;
}

void http_mgr_input(nkn_task_id_t id)
{
	assert(0);
        return ;
}

void http_mgr_output(nkn_task_id_t id)
{
	assert(0);

}

int OM_stat(nkn_uol_t uol, MM_stat_resp_t *in_ptr_resp)
{
	assert(0);
	return 1;
}

void om_get_cleanup(nkn_task_id_t id)
{
	assert(0);
}

void om_get_input(nkn_task_id_t id) 
{
	assert(0);
}

void om_get_output(nkn_task_id_t id)
{
	assert(0);
}

int NFS_init(void);
int NFS_init(void)
{
	assert(0);
	return 0;
}
