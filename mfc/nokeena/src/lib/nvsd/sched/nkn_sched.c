#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "cache_mgr.h"
#include "glib.h"
#include "nkn_sched_api.h"
#include "nkn_sched_task.h"
#include "nkn_mediamgr_api.h"
#include "nkn_hash_map.h"
#include "nkn_debug.h"
#include "nkn_assert.h"
#include "nkn_memalloc.h"
#include <sys/prctl.h>

TAILQ_HEAD(nkn_sched_mm_list, nkn_task)
    *nkn_sched_mm_lists;

pthread_cond_t *nkn_sched_mm_thrd_cond;

pthread_mutex_t *nkn_sched_mm_thrd_mutex;

__thread int t_scheduler_id;
/* The number of stat threads is based on the max number of disks that
 * can be added in the subsystem
 * The Get/Put threads numbers are based on the experiment done on disk io
 * tests
 */
uint32_t g_nkn_num_stat_threads = 32;
uint32_t g_nkn_num_get_threads = 80;
uint32_t g_nkn_num_put_threads = 10;
uint32_t g_nkn_num_val_threads = 8;
uint32_t g_nkn_num_upd_threads = 8;

AO_t nkn_mm_get_queue_thrd_cnt[1000];
uint32_t g_nkn_stat_thread_end;
uint32_t g_nkn_get_thread_end;
uint32_t g_nkn_put_thread_end;
uint32_t g_nkn_val_thread_end;
uint32_t g_nkn_upd_thread_end;

uint32_t g_nkn_max_mm_threads_per_sched;
uint32_t g_nkn_sched_max_mm_threads;

unsigned int nkn_stat_thread_id[NKN_SCHED_MAX_THREADS];
unsigned int nkn_get_thread_id[NKN_SCHED_MAX_THREADS];
unsigned int nkn_put_thread_id[NKN_SCHED_MAX_THREADS];
unsigned int nkn_upd_thread_id[NKN_SCHED_MAX_THREADS];
unsigned int nkn_val_thread_id[NKN_SCHED_MAX_THREADS];

extern void media_mgr_cleanup(nkn_task_id_t id);
extern void media_mgr_input(nkn_task_id_t id);
extern void media_mgr_output(nkn_task_id_t id);

extern void disk_io_req_hdl(gpointer data, gpointer user_data);

extern struct nkn_task_hdl nkn_task_hdl_array[NKN_SYSTEM_MAX_TASK_MODULES];
extern int nkn_choose_cpu(int i);
static int glob_sched_sched_init_flag = 0;
int glob_sched_num_core_threads = 2;
int glob_sched_max_mm_thread_cnt;

uint64_t glob_sched_possible_buf_corrupt_err = 0;
AO_t nkn_sched_task_cnt[TASK_TYPE_END][TASK_TYPE_END];
uint64_t nkn_sched_task_waited_cnt[TASK_TYPE_END][TASK_TYPE_END];
uint64_t nkn_sched_task_longtime_cnt[TASK_TYPE_END][TASK_TYPE_END];

void sched_dump_counters(void);
void *nkn_sched_core_thread(void *message);
void *nkn_sched_mm_thread(void *arg);
const char *s_nkn_find_mm_thread_type(int id, int sched_id);

/* Add new scheduler module names here */
char sched_module_type[TASK_TYPE_END+1][MAX_SCHED_MOD_NAME_SZ] = {
    "none",
    "socl",
    "socc",
    "http",
    "vfs",
    "rtsp",
    "chunk",
    "dm2",
    "medm",
    "movm",
    "om",
    "getm",
    "tfm",
    "rtspd",
    "mfpl",
    "enc",
    "authm",
    "authh",
    "clus",
    "nfstn",
    "unk",
};


const char *
s_nkn_find_mm_thread_type(int id, int sched_id)
{
    uint32_t thread_type = id - (sched_id * g_nkn_max_mm_threads_per_sched);
    if(thread_type < g_nkn_stat_thread_end) {
	return("nvsd-mm-stat");
    } else if(thread_type < g_nkn_get_thread_end) {
	return("nvsd-mm-get");
    } else if(thread_type < g_nkn_put_thread_end) {
	return("nvsd-mm-put");
    } else if(thread_type < g_nkn_val_thread_end) {
	return("nvsd-mm-val");
    } else if(thread_type < g_nkn_upd_thread_end) {
	return("nvsd-mm-upd");
    }
    return ("nvsd-unknown");
}

/*
 * Scheduler MM threads 
 */
void *
nkn_sched_mm_thread(void *arg)
{
    int tid = (int)((uint64_t)arg & 0xFFFFFFFF);
    struct nkn_task *ntask;
    char t_name[50];
    int  sched_id = (tid/g_nkn_max_mm_threads_per_sched);
    snprintf(t_name, 50, "%s-%d-%d", s_nkn_find_mm_thread_type(tid, sched_id),
		    sched_id, (tid - (sched_id * g_nkn_max_mm_threads_per_sched)));

    prctl(PR_SET_NAME, t_name, 0, 0, 0);

    while(1) {
	pthread_mutex_lock(&nkn_sched_mm_thrd_mutex[tid]);
	ntask = TAILQ_FIRST(&nkn_sched_mm_lists[tid]);
	if(ntask) {
	    TAILQ_REMOVE(&nkn_sched_mm_lists[tid], ntask, mm_entries);
	    pthread_mutex_unlock(&nkn_sched_mm_thrd_mutex[tid]);
	    mm_async_thread_hdl(ntask, NULL);
	    AO_fetch_and_sub1(&nkn_mm_get_queue_thrd_cnt[tid]);
	    continue;
	}
	pthread_cond_wait(&nkn_sched_mm_thrd_cond[tid],
			    &nkn_sched_mm_thrd_mutex[tid]);
	pthread_mutex_unlock(&nkn_sched_mm_thrd_mutex[tid]);
    }
}

/* 
 * API called from MM to queue tasks for MM threads
 * limit_read - limit the number of get threads for origin providers
 */
void
nkn_sched_mm_queue(struct nkn_task *ntask, int limit_read)
{
    int tid;

    /* Find the MM thread id with the formula - 
     * MM Thread id = sched id * max threads + sched - to determine the
     *							sched specific thread)
     *		      + previous operation thread end - to find the start of 
     *							start
     *		      + thread count - for round robin among the current op
     */

    switch (ntask->dst_module_op) {
        case MM_OP_READ:
	    if(limit_read == 1) {
		tid = (ntask->sched_id * g_nkn_max_mm_threads_per_sched) +
		    g_nkn_get_thread_end - 1;
	    } else if(limit_read) {
		tid = (ntask->sched_id * g_nkn_max_mm_threads_per_sched) +
		    (limit_read + g_nkn_stat_thread_end);
	    } else {
		tid = (ntask->sched_id * g_nkn_max_mm_threads_per_sched) +
		    (nkn_get_thread_id[ntask->sched_id] + g_nkn_stat_thread_end);
		nkn_get_thread_id[ntask->sched_id]++;
		if(nkn_get_thread_id[ntask->sched_id] >= g_nkn_num_get_threads)
		    nkn_get_thread_id[ntask->sched_id] = 0;
	    }
	    break;
        case MM_OP_WRITE:
	    tid = (ntask->sched_id * g_nkn_max_mm_threads_per_sched) +
		    (limit_read + g_nkn_get_thread_end);
	    nkn_put_thread_id[ntask->sched_id]++;
	    if(nkn_put_thread_id[ntask->sched_id] >= g_nkn_num_put_threads)
		nkn_put_thread_id[ntask->sched_id] = 0;
	    break;
        case MM_OP_STAT:
        case MM_OP_DELETE:
	    tid = (ntask->sched_id * g_nkn_max_mm_threads_per_sched) +
		nkn_stat_thread_id[ntask->sched_id];
	    nkn_stat_thread_id[ntask->sched_id]++;
	    if(nkn_stat_thread_id[ntask->sched_id] >= g_nkn_num_stat_threads)
		nkn_stat_thread_id[ntask->sched_id] = 0;
	    break;
        case MM_OP_UPDATE_ATTR:
	    if(limit_read == 1) {
		tid = (ntask->sched_id * g_nkn_max_mm_threads_per_sched) +
		    g_nkn_upd_thread_end - 1;
	    } else {
		tid = (ntask->sched_id * g_nkn_max_mm_threads_per_sched) +
		    (nkn_upd_thread_id[ntask->sched_id] + g_nkn_val_thread_end);
	    }
	    nkn_upd_thread_id[ntask->sched_id]++;
	    if(nkn_upd_thread_id[ntask->sched_id] >= (g_nkn_num_upd_threads - 1))
		nkn_upd_thread_id[ntask->sched_id] = 0;
	    break;
        case MM_OP_VALIDATE:
	    tid = (ntask->sched_id * g_nkn_max_mm_threads_per_sched) +
		(nkn_val_thread_id[ntask->sched_id] + g_nkn_put_thread_end);
	    nkn_val_thread_id[ntask->sched_id]++;
	    if(nkn_val_thread_id[ntask->sched_id] >= g_nkn_num_val_threads)
		nkn_val_thread_id[ntask->sched_id] = 0;
	    break;
    }
    if(tid > glob_sched_max_mm_thread_cnt) {
	NKN_ASSERT(0);
	tid = 0;
    }
    AO_fetch_and_add1(&nkn_mm_get_queue_thrd_cnt[tid]);
    pthread_mutex_lock(&nkn_sched_mm_thrd_mutex[tid]);
    TAILQ_INSERT_TAIL(&nkn_sched_mm_lists[tid], ntask, mm_entries);
    pthread_cond_signal(&nkn_sched_mm_thrd_cond[tid]);
    pthread_mutex_unlock(&nkn_sched_mm_thrd_mutex[tid]);
}

void*
nkn_sched_core_thread(void *message)
{
    struct nkn_task *ntask = NULL;
    int tnum = (int)(long)message;
    char mem_pool_str[64];

    /*
    nkn_choose_cpu(4);
    */

    t_scheduler_id = tnum;
    prctl(PR_SET_NAME, "nvsd-sched", 0, 0, 0);
    snprintf(mem_pool_str, sizeof(mem_pool_str), "schedmempool-%d", tnum);
    nkn_mem_create_thread_pool(mem_pool_str);

    do {
	    /* This function 'pops' the task from RUNNABLE queue */
	    ntask = nkn_task_pop_from_runnable_queue(tnum);

	    if(ntask) {
                if(unlikely(ntask->data_check != ~(uint64_t)(ntask->data))) {
                    glob_sched_possible_buf_corrupt_err++;
	            DBG_LOG(ERROR, MOD_SCHED, "Possible data corruption: task %ld src %d "
                                              "dst %d, data %p data_check %lx",
                                              ntask->task_id, ntask->src_module_id, 
                                              ntask->dst_module_id, ntask->data, ntask->data_check);
                }
		switch(ntask->task_action) {
		    case TASK_ACTION_IDLE:
			break;
		    case TASK_ACTION_INPUT:
			TASK_SET_DEBUG_TIME(ntask);
			ntask->dbg_dst_module_called ++;
			nkn_task_hdl_array[ntask->dst_module_id].inputTaskHdl(ntask->task_id);
			break;

		    case TASK_ACTION_OUTPUT:
			assert(ntask->dbg_dst_module_called != 0);
			TASK_SET_DEBUG_TIME(ntask);
			ntask->dbg_src_module_called ++;
    			nkn_task_hdl_array[ntask->src_module_id].outputTaskHdl(ntask->task_id);
			break;

		    case TASK_ACTION_CLEANUP:
			/* Task should be RUNNING state */
			ntask->dbg_cleanup_module_called ++;
                        assert(ntask->dbg_src_module_called != 0);
			TASK_SET_DEBUG_TIME(ntask);
    			nkn_task_hdl_array[ntask->dst_module_id].cleanupHdl(ntask->task_id);
			/* Mem leak check */
			nkn_task_free_task(ntask);
			break;

		    default:
			break;
		}
	    }
    } while (1);

}

static
void nkn_task_counter_init(void)
{
    int i;
    int j;
    char sched_counter[64];
    for(i=0;i<TASK_TYPE_END;i++) {
	for(j=0;j<TASK_TYPE_END;j++) {
	    if(i == j)
		continue;
	    snprintf(sched_counter, 64, "%s_to_%s_cnt", sched_module_type[i],
		    sched_module_type[j]);
	    (void)nkn_mon_add(sched_counter, "sched_running_task",
		    (void *)&nkn_sched_task_cnt[i][j],
		    sizeof(nkn_sched_task_cnt[i][j]));
	    (void)nkn_mon_add(sched_counter, "sched_waited_task",
		    (void *)&nkn_sched_task_waited_cnt[i][j],
		    sizeof(nkn_sched_task_waited_cnt[i][j]));
	    (void)nkn_mon_add(sched_counter, "sched_longtime_task",
		    (void *)&nkn_sched_task_longtime_cnt[i][j],
		    sizeof(nkn_sched_task_longtime_cnt[i][j]));
	}
    }
}


static int
nkn_sched_mm_thread_init(int sched_init_flags)
{
    int64_t         i, j;
    int             ret;
    pthread_attr_t  attr;
    pthread_t       sched_mm_thread;
    int             stacksize = 256 * KiBYTES;
    char            thread_counter[64];
    int             tid;

    g_nkn_stat_thread_end = g_nkn_num_stat_threads;
    g_nkn_get_thread_end  = g_nkn_stat_thread_end +
			    g_nkn_num_get_threads;
    g_nkn_put_thread_end  = g_nkn_get_thread_end +
			    g_nkn_num_put_threads;
    g_nkn_val_thread_end  = g_nkn_put_thread_end +
			    g_nkn_num_val_threads;
    g_nkn_upd_thread_end  = g_nkn_val_thread_end +
			    g_nkn_num_upd_threads;

    g_nkn_max_mm_threads_per_sched = g_nkn_num_stat_threads +
					    g_nkn_num_get_threads +
					    g_nkn_num_put_threads +
					    g_nkn_num_val_threads +
					    g_nkn_num_upd_threads;

    g_nkn_sched_max_mm_threads =
		g_nkn_num_stat_threads * NKN_SCHED_MAX_THREADS +
		g_nkn_num_get_threads * NKN_SCHED_MAX_THREADS +
		g_nkn_num_put_threads * NKN_SCHED_MAX_THREADS +
		g_nkn_num_val_threads * NKN_SCHED_MAX_THREADS +
		g_nkn_num_upd_threads * NKN_SCHED_MAX_THREADS;

    ret = pthread_attr_init(&attr);
    if (ret) {
	DBG_LOG(MSG, MOD_SCHED, "pthread_attr_init() failed, ret=%d", ret);
	return -1;
    }

    ret = pthread_attr_setstacksize(&attr, stacksize);
    if (ret) {
	DBG_LOG(MSG, MOD_SCHED,
		    "pthread_attr_setstacksize() failed, ret=%d", ret);
	return -1;
    }

    nkn_sched_mm_thrd_mutex = (pthread_mutex_t *)nkn_calloc_type(
						g_nkn_sched_max_mm_threads,
						sizeof(pthread_mutex_t),
						mod_sched_mm_thread_t);

    nkn_sched_mm_thrd_cond = (pthread_cond_t *)nkn_calloc_type(
						g_nkn_sched_max_mm_threads,
						sizeof(pthread_cond_t),
						mod_sched_mm_thread_t);

    nkn_sched_mm_lists = (struct nkn_sched_mm_list *)nkn_calloc_type(
						g_nkn_sched_max_mm_threads,
						sizeof(struct nkn_sched_mm_list),
						mod_sched_mm_thread_t);

    if (sched_init_flags & NKN_SCHED_START_MMTHREADS) {
	for(i = 0; i < glob_sched_num_core_threads; i++) {
	    for(j = i * g_nkn_max_mm_threads_per_sched;
		j < (g_nkn_max_mm_threads_per_sched * (i+1)); j++ ) {

		ret = pthread_mutex_init(&nkn_sched_mm_thrd_mutex[j], NULL);
		if(ret < 0) {
		    DBG_LOG(MSG, MOD_SCHED,
			    "pthread_mutex_init() failed, ret=%d", ret);
		    return -1;
		}

		pthread_cond_init(&nkn_sched_mm_thrd_cond[j], NULL);

		TAILQ_INIT(&nkn_sched_mm_lists[j]);

		if ((ret = pthread_create(&sched_mm_thread, &attr,
					  nkn_sched_mm_thread, (void *)j))) {
		    DBG_LOG(MSG, MOD_SCHED,
			    "pthread_create() failed, ret=%d", ret);
		    return -1;
		}
		snprintf(thread_counter, 64, "thread_%ld_cnt", j);
		(void)nkn_mon_add(thread_counter, "glob_mm_queue",
		    (void *)&nkn_mm_get_queue_thrd_cnt[j],
		    sizeof(nkn_mm_get_queue_thrd_cnt[j]));
		glob_sched_max_mm_thread_cnt ++;
		tid ++;
	    }
	}
    }
    return 0;
}

int
nkn_scheduler_init(GThread** gthreads, uint32_t sched_init_flags)
{
    int64_t         i;
    int             ret;

    gthreads = gthreads;
    if(glob_sched_sched_init_flag == 1) {
	DBG_LOG(MSG, MOD_SCHED, "\n SCHEDULER already inited. "
                "Returning success.");
	return 0;
    }

    glob_sched_sched_init_flag = 1;

    nkn_task_init();
    nkn_task_counter_init();

    nkn_task_register_task_type(
				TASK_TYPE_MEDIA_MANAGER,
                             	&media_mgr_input,
                             	&media_mgr_output,
                                &media_mgr_cleanup);


    nkn_task_register_task_type(
                                TASK_TYPE_MOVE_MANAGER,
                                &move_mgr_input,
                                &move_mgr_output,
                                &move_mgr_cleanup);

    nkn_task_register_task_type(
                                TASK_TYPE_CHUNK_MANAGER,
                                &chunk_mgr_input,
                                &chunk_mgr_output,
                                &chunk_mgr_cleanup);

    nkn_task_register_task_type(
				TASK_TYPE_HTTP_SRV,
                                &http_mgr_input,
                                &http_mgr_output,
                                &http_mgr_cleanup);

    nkn_task_register_task_type(
				TASK_TYPE_ORIGIN_MANAGER,
                                &om_mgr_input,
                                &om_mgr_output,
                                &om_mgr_cleanup);

    nkn_task_register_task_type(
                                TASK_TYPE_ENCRYPTION_MANAGER,
                                &enc_mgr_input,
                                &enc_mgr_output,
                                &enc_mgr_cleanup);


    nkn_task_register_task_type(
                                TASK_TYPE_AUTH_MANAGER,
                                &auth_mgr_input,
                                &auth_mgr_output,
                                &auth_mgr_cleanup);

    nkn_task_register_task_type(
                                TASK_TYPE_AUTH_HELPER,
                                &auth_helper_input,
                                &auth_helper_output,
                                &auth_helper_cleanup);

    nkn_task_register_task_type(
                                TASK_TYPE_NFS_TUNNEL_MANAGER,
                                &nfs_tunnel_mgr_input,
                                &nfs_tunnel_mgr_output,
                                &nfs_tunnel_mgr_cleanup);

    nkn_task_register_task_type(
                                TASK_TYPE_PE_GEO_MANAGER,
                                &pe_geo_mgr_input,
                                &pe_geo_mgr_output,
                                &pe_geo_mgr_cleanup);

    nkn_task_register_task_type(
                                TASK_TYPE_PE_HELPER,
                                &pe_helper_input,
                                &pe_helper_output,
                                &pe_helper_cleanup);

    nkn_task_register_task_type(
                                TASK_TYPE_PE_UCFLT_MANAGER,
                                &pe_ucflt_mgr_input,
                                &pe_ucflt_mgr_output,
                                &pe_ucflt_mgr_cleanup);

    nkn_task_register_task_type(
                                TASK_TYPE_COMPRESS_MANAGER,
                                &compress_mgr_input,
                                &compress_mgr_output,
                                &compress_mgr_cleanup);
    nkn_task_q_init();

    nkn_sched_tmout_init();

    if((ret = nkn_sched_mm_thread_init(sched_init_flags))) {
	return ret;
    }

    pthread_attr_t  attr;
    ret = pthread_attr_init(&attr);
    if (ret) {
	DBG_LOG(MSG, MOD_SCHED, "pthread_attr_init() failed, ret=%d", ret);
	return -1;
    }

    ret = pthread_attr_setstacksize(&attr, (128 * 1024));
    if (ret) {
	DBG_LOG(MSG, MOD_SCHED,
		    "pthread_attr_setstacksize() failed, ret=%d", ret);
	return -1;
    }

    pthread_t sched_core_thread;
    for(i = 0; i < glob_sched_num_core_threads; i++) {

	if ((ret = pthread_create(&sched_core_thread, &attr,
					  nkn_sched_core_thread, (void *)i))) {
		    DBG_LOG(MSG, MOD_SCHED,
			    "pthread_create() failed, ret=%d", ret);
		    return -1;
	}
    }

    return 0;
}

