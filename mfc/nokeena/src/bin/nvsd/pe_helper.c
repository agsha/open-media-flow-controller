#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdint.h>
#include <dlfcn.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>

#include "nvsd_mgmt.h"
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_cfg_params.h"
#include "nkn_sched_api.h"
#include "server.h"
#include "nkn_stat.h"
#include "nkn_trace.h"
#include "rtsp_session.h"
#include "http_header.h"
#include "nkn_cod.h"
#include "nkn_pseudo_con.h"
#include "mime_header.h"
#include "pe_geodb.h"
#include "nkn_http.h"
#include "network.h"
#include "pe_ucflt.h"

#define PE_TASK_TYPE_GEOIP		1
#define PE_TASK_TYPE_UCFLT		2

NKNCNT_EXTERN(sched_task_create_fail_cnt, uint64_t);
void pe_helper_cleanup(nkn_task_id_t id);
void pe_helper_input(nkn_task_id_t id);
void pe_helper_output(nkn_task_id_t id);
int pe_helper_get_geostat(geo_ip_req_t *data, void *pvtdata);
int pe_helper_get_url_cat(void *data, void *pvtdata);

int http_handle_request_post_pe(con_t *con);
int http_handle_request_url_cat_lookup(con_t *con);
void pe_add_req(pe_con_info_t *con_info);


int pe_helper_get_geostat(geo_ip_req_t *data, void *pvtdata)
{
	nkn_task_id_t taskid;
	pe_con_info_t *p_data;

	p_data = nkn_malloc_type(sizeof(pe_con_info_t), mod_pe_con_info_t);
	if (p_data == NULL) {
		DBG_LOG(ERROR, MOD_PEMGR, "Out of memory");
		return -1;
	}
	p_data->con = (con_t *)pvtdata;
	p_data->fd = p_data->con->fd;
	p_data->incarn = gnm[p_data->fd].incarn;
	p_data->type = PE_TASK_TYPE_GEOIP;

	taskid = nkn_task_create_new_task(TASK_TYPE_PE_GEO_MANAGER, 
					  TASK_TYPE_PE_HELPER,
					  TASK_ACTION_INPUT,
					  0, data, sizeof(geo_ip_req_t),0);
	if (taskid == -1) {
		glob_sched_task_create_fail_cnt++;
		DBG_LOG(MSG, MOD_PEMGR, "task creation failed");
		return -2;
	}
	DBG_LOG(MSG, MOD_PEMGR,
		"Created pemgr task=%ld",taskid);

	SET_CON_FLAG(p_data->con, CONF_GEOIP_TASK);
	nkn_task_set_private(TASK_TYPE_PE_HELPER, taskid, (void *)p_data);
	nkn_task_set_state(taskid, TASK_STATE_RUNNABLE);
	return 1;
}

int pe_helper_get_url_cat(void *data, void *pvtdata)
{
	nkn_task_id_t taskid;
	pe_con_info_t *p_data;

	DBG_LOG(MSG, MOD_HTTP, "Entering %s\n", __FUNCTION__) ;
	p_data = nkn_malloc_type(sizeof(pe_con_info_t), mod_pe_con_info_t);
	if (p_data == NULL) {
		DBG_LOG(ERROR, MOD_PEMGR, "Out of memory");
		return -1;
	}
	p_data->con = (con_t *)pvtdata;
	p_data->fd = p_data->con->fd;
	p_data->incarn = gnm[p_data->fd].incarn;
	p_data->type = PE_TASK_TYPE_UCFLT;

	taskid = nkn_task_create_new_task(TASK_TYPE_PE_UCFLT_MANAGER, 
					  TASK_TYPE_PE_HELPER,
					  TASK_ACTION_INPUT,
					  0, data, sizeof(ucflt_req_t),0);	//<<<< Change here
	if (taskid == -1) {
		glob_sched_task_create_fail_cnt++;
		DBG_LOG(MSG, MOD_PEMGR, "task creation failed");
		return -2;
	}
	DBG_LOG(MSG, MOD_PEMGR,
		"Created pemgr task=%ld",taskid);

	SET_CON_FLAG(p_data->con, CONF_UCFLT_TASK);
	nkn_task_set_private(TASK_TYPE_PE_HELPER, taskid, (void *)p_data);
	nkn_task_set_state(taskid, TASK_STATE_RUNNABLE);
	return 1;
}

void pe_helper_cleanup(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
	return ;
}

void pe_helper_input(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
	return ;
}

void pe_helper_output(nkn_task_id_t id)
{
	struct nkn_task *ntask=NULL;
	ntask = nkn_task_get_task(id);
	void *tdata = NULL;
	pe_con_info_t *p_data = NULL;

	tdata = nkn_task_get_data(id);

	p_data = nkn_task_get_private(TASK_TYPE_PE_HELPER, id);
	free(tdata);
	nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP, TASK_STATE_RUNNABLE);

	DBG_LOG(MSG, MOD_PEMGR,
		"scheduler returns a task from PE Mgr, id=%ld", id);
	pe_add_req(p_data);
	return;
}



/* ==================================================================== */
/*
 * Connection Pool Socket Close Event Queue
 * The reason to introduce this queue is to avoid other sockets to call cp_internal_close_conn() directly.
 */
static struct pe_req_q_t {
        struct pe_req_q_t * next;
	pe_con_info_t *data;
} * g_pe_req_q[MAX_EPOLL_THREADS] =  { NULL };

static pthread_mutex_t peq_mutex[MAX_EPOLL_THREADS] =
{
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER
};

/**
 *
 * caller should own the gnm[fd].mutex lock
 *
 * OP_EPOLLOUT event:
 * Application receives second task and inform connection pool socket
 * to continue to send more data.
 *
 * OP_CLOSE event:
 * If other threads try to close connection poll socket,
 * they should use this function.
 *
 * The design goal is to separate two sockets httpcon from cpcon
 * any socket related operations should be limited to be run by epoll thread.
 *
 * cp_internal_close_conn() is an internal function to socket
 *
 * OP_TUNNEL_CONT event:
 * Tunnel is ready for forwarding data.
 */
void pe_add_req(pe_con_info_t *con_info) {
	struct pe_req_q_t * pq;
	int num;
	struct network_mgr * pnm;

	if (con_info->fd == -1) return;
	pnm = &gnm[con_info->fd];

	if( NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {
		pq = (struct pe_req_q_t *)nkn_malloc_type(sizeof(struct pe_req_q_t), mod_pe_req_q_t);
		if(!pq) {
			//DBG_LOG(MSG, MOD_CP, "fd=%d event %d lost (3)", op);
			return; // We lost the socket here
		}

		num = pnm->pthr->num;
		pthread_mutex_lock(&peq_mutex[num]);
		pq->next = g_pe_req_q[num];
		pq->data = con_info;
		g_pe_req_q[num] = pq;
		pthread_mutex_unlock(&peq_mutex[num]);
	}
	return;
}

/* this function should be called only from cp_internal_close_conn() */
void pe_remove_req(int fd);
void pe_remove_req(int fd) 
{
	struct pe_req_q_t * pq, * pq_del;
	int num;
	struct network_mgr * pnm;

	//NM_EVENT_TRACE(fd, FT_CP_REMOVE_EVENT);
        pnm = &gnm[fd];
	num = pnm->pthr->num;
	pthread_mutex_lock(&peq_mutex[num]);

	pq = g_pe_req_q[num];
	if (pq == NULL) {
		pthread_mutex_unlock(&peq_mutex[num]);
		return;
	}

	if (pq->data->fd == fd) {
		g_pe_req_q[num] = pq->next;
		free(pq->data);
		free(pq);
		pthread_mutex_unlock(&peq_mutex[num]);
		return;
	}

	while (pq->next) {
		if(pq->next->data->fd == fd) {
			pq_del = pq->next;
			pq->next = pq_del->next;
			free(pq_del->data);
			free(pq_del);
			break;
		}
		pq = pq->next;
	}
	pthread_mutex_unlock(&peq_mutex[num]);
	return;
}

void check_out_pe_req_queue(int num_of_epoll);
void check_out_pe_req_queue(int num_of_epoll) {
        struct pe_req_q_t * pq;
	struct network_mgr * pnm;

again:
	pthread_mutex_lock(&peq_mutex[num_of_epoll]);
        while (g_pe_req_q[num_of_epoll]) {
                pq = g_pe_req_q[num_of_epoll];
                g_pe_req_q[num_of_epoll] = g_pe_req_q[num_of_epoll]->next;
		pthread_mutex_unlock(&peq_mutex[num_of_epoll]);//need release now

		/*
		 * All network tasks should get mutex locker first.
		 */
		pnm = &gnm[pq->data->fd];
		pthread_mutex_lock(&pnm->mutex);
		//NM_TRACE_LOCK(pnm, LT_OM_CONNPOLL);
		if (NM_CHECK_FLAG(pnm, NMF_IN_USE) && (pq->data->incarn == pnm->incarn)
				&& ((void *)pq->data->con == pnm->private_data)) {
			if (CHECK_CON_FLAG(pq->data->con, CONF_GEOIP_TASK)) {
				UNSET_CON_FLAG(pq->data->con, CONF_GEOIP_TASK);
				resume_NM_socket(pq->data->con->fd);
				http_handle_request_url_cat_lookup(pq->data->con);
			}
			else if (CHECK_CON_FLAG(pq->data->con, CONF_UCFLT_TASK)) {
				UNSET_CON_FLAG(pq->data->con, CONF_UCFLT_TASK);
				resume_NM_socket(pq->data->con->fd);
				http_handle_request_post_pe(pq->data->con);
			}
		}
		//NM_TRACE_UNLOCK(pnm, LT_OM_CONNPOLL);
		pthread_mutex_unlock(&pnm->mutex);

		free(pq->data);
                free(pq);
		goto again;
        }
	pthread_mutex_unlock(&peq_mutex[num_of_epoll]);
}


