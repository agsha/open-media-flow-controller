#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <glib.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <ares.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <string.h>

#include "nvsd_mgmt.h"
#include "nkn_defs.h"
#include "nkn_sched_api.h"
#include "nkn_debug.h"
#include "server.h"
#include "network.h"
#include "nkn_mgmt_agent.h"
#include "nkn_cfg_params.h"
#include "nkn_log_strings.h"
#include "nkn_trace.h"
#include "nkn_am_api.h"
#include "ssp.h"
#include "nkn_namespace.h"
#include "nkn_vpemgr_defs.h"
#include "nkn_cl.h"
#include "nkn_mediamgr_api.h"
#include "nkn_rtsched_api.h"
#include "network_hres_timer.h"
#include "pe_ucflt.h"
#include "nkn_trie.h"

NKNCNT_DEF(ucflt_lookup_completed, AO_t, "", "total url cat lookup completed")
NKNCNT_DEF(ucflt_lookup_called, AO_t, "", "total url cat lookup called")
NKNCNT_DEF(ucflt_lookup_failed, AO_t, "", "total url cat lookup failed")
NKNCNT_DEF(ucflt_lookup_pending_q, AO_t, "", "total lookup pending q")
NKNCNT_DEF(ucflt_lookup_pq_remove_err, AO_t, "", "total remove error")
NKNCNT_DEF(ucflt_lookup_msg_send_err, AO_t, "", "total send error")
NKNCNT_DEF(ucflt_lookup_msg_receive_err, AO_t, "", "total receive error")
NKNCNT_DEF(ucflt_entries_in_cache, AO_t, "", "total entries in cache");

#define MAX_PEPENDQUERY     10000
#define MAX_PEPENDDELAY     15 //secs
#define MAX_UCFLT_CACHE_ENTRIES 100000

//extern int ucfltdb_installed;

int pe_ucflt_client_fd;
struct sockaddr_un ucflt_daemon_addr;

void pe_ucflt_mgr_input(nkn_task_id_t id);          //puts into taskqueue
void pe_ucflt_mgr_output(nkn_task_id_t id);         //dummy
void pe_ucflt_mgr_cleanup(nkn_task_id_t id);        //dummy


typedef struct pend_pe_s {
        struct pend_pe_s* next;
        int64_t pe_task_id;
        int64_t ts;
} pend_pe_t;

static pend_pe_t* pe_pend_hash[MAX_PEPENDQUERY];
static pthread_mutex_t pe_pend_mutex[MAX_PEPENDQUERY];

static pthread_mutex_t ucflt_info_trie_mutex;
static nkn_trie_t nkn_ucflt_info_trie;
ucflt_info_t* get_ucflt_info_t(char *url, ucflt_info_t* p_ucflt_inf);
int add_ucflt_info_t(char *url, ucflt_info_t* p_ucflt_inf);

static void *trie_ucflt_info_copy(nkn_trie_node_t nd)
{
        return nd;
}

static void delete_ucflt_info_t(ucflt_info_t *p_info)
{
        if (p_info) {
                free(p_info);
        }
}

static void trie_ucflt_info_destruct(nkn_trie_node_t nd)
{
        ucflt_info_t *p_info = (ucflt_info_t *)nd;
        if (p_info) {
                delete_ucflt_info_t(p_info);
        }
}

static ucflt_info_t* new_ucflt_info_t(ucflt_info_t* p_ucflt_inf)
{
        ucflt_info_t* p_info;
        p_info = (ucflt_info_t *)nkn_calloc_type(1, sizeof(ucflt_info_t),
                                              mod_ucflt_info_t);
        memcpy(p_info, p_ucflt_inf, sizeof(ucflt_info_t));
        return p_info;
}

static int pe_ucflt_pend_insert(int64_t tid, uint32_t timeout) 
{
	uint32_t hash; 
	pend_pe_t* ptrpe;

	hash = tid % MAX_PEPENDQUERY;
	ptrpe = (pend_pe_t*)nkn_malloc_type(sizeof(pend_pe_t),mod_pe_pendq);
	if(!ptrpe) return -1;

	ptrpe->ts = nkn_cur_ts + timeout/1000;
	ptrpe->pe_task_id = tid;
	pthread_mutex_lock(&pe_pend_mutex[hash]);
	if (pe_pend_hash[hash]) {
		ptrpe->next = pe_pend_hash[hash];
		pe_pend_hash[hash] = ptrpe;
	}
	else {
		pe_pend_hash[hash] = ptrpe;
		ptrpe->next = NULL;
	}
	pthread_mutex_unlock(&pe_pend_mutex[hash]);
	AO_fetch_and_add1(&glob_ucflt_lookup_pending_q);
	return 0;
}

static inline void pe_ucflt_pend_delete(uint32_t hash)
{
	pend_pe_t* ptrpe, *ptrpenext;

	ptrpe = pe_pend_hash[hash];
	if( !ptrpe ) return;

	/*
	* the link list is sorted by time. latest time on the top
	*/
	if (nkn_cur_ts > ptrpe->ts) {
		pe_pend_hash[hash] = NULL;
		goto delete_chain;
	}
	while (ptrpe->next) {
		if (nkn_cur_ts > ptrpe->next->ts) {
			ptrpenext = ptrpe->next;
			ptrpe->next = NULL;
			ptrpe = ptrpenext;
			goto delete_chain;
		}
		ptrpe = ptrpe->next;
	}
	return;

delete_chain:
	/*
	* Time to delete the expired dns entry.
	*/
	while (ptrpe) {
		ptrpenext = ptrpe->next;
		DBG_LOG(MSG, MOD_PEMGR, "pe_pend_delete: Deleting peid = %ld", ptrpe->pe_task_id);
		nkn_task_set_action_and_state((nkn_task_id_t)(ptrpe->pe_task_id),
		                                TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);
		free(ptrpe);
		AO_fetch_and_sub1(&glob_ucflt_lookup_pending_q);
		AO_fetch_and_add1(&glob_ucflt_lookup_completed);
		AO_fetch_and_add1(&glob_ucflt_lookup_failed);
		ptrpe = ptrpenext;
	}
}

static int32_t pe_ucflt_pend_remove(int64_t tid)
{
	uint32_t hash;
	pend_pe_t* ptrpe = NULL, *ptrpeprev = NULL;

	/* get hash value for this domain */
	hash = tid % MAX_PEPENDQUERY;
	pthread_mutex_lock(&pe_pend_mutex[hash]);
	ptrpe = pe_pend_hash[hash];

	while (ptrpe) {
		if(ptrpe->pe_task_id == tid) {
			if (ptrpeprev==NULL) {/*First entry*/ 
				pe_pend_hash[hash] = ptrpe->next;
			} else {
				ptrpeprev->next = ptrpe->next;
			}
			free(ptrpe);
			AO_fetch_and_sub1(&glob_ucflt_lookup_pending_q);
			pthread_mutex_unlock(&pe_pend_mutex[hash]);
			return 0;
		}
		ptrpeprev = ptrpe;
		ptrpe = ptrpe->next;
	}
	AO_fetch_and_add1(&glob_ucflt_lookup_pq_remove_err);
	pthread_mutex_unlock(&pe_pend_mutex[hash]);
	return 1;
}

static void* pe_ucflt_check_pendq(void * arg)
{
	int i;
	int rv;

	UNUSED_ARGUMENT(arg);
	prctl(PR_SET_NAME, "nvsd-pe-pendq", 0, 0, 0);

	while(1) {
		sleep(1); // sleep 1 second

		for (i=0; i<MAX_PEPENDQUERY; i++) {
			if (pe_pend_hash[i]) {
				pthread_mutex_lock(&pe_pend_mutex[i]);
				pe_ucflt_pend_delete(i);
				pthread_mutex_unlock(&pe_pend_mutex[i]);
			}
		}
		/* Clear the cache entries if the number of cache entries exceed
		 * MAX_GEOIP_CACHE_ENTRIES or a new GeoIP database is installed.
		 */
		if ((AO_load(&glob_ucflt_entries_in_cache) > MAX_UCFLT_CACHE_ENTRIES)) {
			pthread_mutex_lock(&ucflt_info_trie_mutex);
			rv = nkn_trie_destroy(nkn_ucflt_info_trie);
			AO_store(&glob_ucflt_entries_in_cache, 0);
			//ucfltdb_installed = 0;	// Clear the flag set on CLI install command.
			nkn_ucflt_info_trie = nkn_trie_create(trie_ucflt_info_copy, 
						trie_ucflt_info_destruct);	
			pthread_mutex_unlock(&ucflt_info_trie_mutex);
	        }
	}
	return NULL;
}


int pe_ucflt_open_sock_dgram(const char *path);
int pe_ucflt_open_sock_dgram(const char *path)
{
	struct sockaddr_un unix_socket_name;
	
	pe_ucflt_client_fd = -1;
	memset(&unix_socket_name,0,sizeof(struct sockaddr_un));
	if (unlink(path) < 0)
	{
		if (errno != ENOENT)
		{
			DBG_LOG(SEVERE, MOD_PEMGR, "Failed to unlink %s:%s",path, strerror(errno));
			return -1;
		}
	}

	unix_socket_name.sun_family = AF_UNIX;
	strcpy(unix_socket_name.sun_path, path);
	pe_ucflt_client_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (pe_ucflt_client_fd == -1)
	{
		DBG_LOG(SEVERE, MOD_PEMGR, "Unable to create domain socket:%s", strerror(errno));
		return -1;
	}
	if (bind(pe_ucflt_client_fd, (struct sockaddr *)&unix_socket_name, sizeof(unix_socket_name)))
	{
		DBG_LOG(SEVERE, MOD_PEMGR, "Unable to bind socket:%s", strerror(errno));
		close(pe_ucflt_client_fd);
		return -1;
	}
	return 0;
}

int pe_ucflt_receive_msg(ucflt_resp_t *ptr);
int pe_ucflt_receive_msg(ucflt_resp_t *ptr)
{
	struct sockaddr_un server_addr;
	int address_length;
	int rv = 0;
	address_length = sizeof(struct sockaddr_un);
	rv = recvfrom(pe_ucflt_client_fd, ptr, sizeof(ucflt_resp_t),0,
				(struct sockaddr *)&server_addr,
				(socklen_t *)&address_length);
	if ((rv == -1) || (rv < (int)sizeof(ucflt_resp_t)) || (ptr->magic != UCFLT_MAGIC))
	{
		AO_fetch_and_add1(&glob_ucflt_lookup_msg_receive_err);
		DBG_LOG(SEVERE, MOD_PEMGR, "Failure in recvmsg in recv_msg:%s", strerror(errno));
		return -1;
	}

	return rv;
}

int pe_ucflt_send_msg(ucflt_req_t *ptr);
int pe_ucflt_send_msg(ucflt_req_t *ptr)
{
	int rv = 0;
	pe_ucflt_pend_insert(ptr->pe_task_id, ptr->lookup_timeout);
	rv = sendto(pe_ucflt_client_fd, ptr, sizeof(ucflt_req_t),0,
				(struct sockaddr *)&ucflt_daemon_addr,
				sizeof(struct sockaddr_un));
	if (rv == -1)
	{
		AO_fetch_and_add1(&glob_ucflt_lookup_msg_send_err);
		pe_ucflt_pend_remove(ptr->pe_task_id);
		DBG_LOG(SEVERE, MOD_PEMGR, "Failure in sendto in send_msg:%s", strerror(errno));
		return -1;
	}
	return rv;
}

void *pe_ucflt_client_func(void *arg);
void *pe_ucflt_client_func(void *arg)
{
	prctl(PR_SET_NAME, "nvsd-pe-client", 0, 0, 0);
	int count, ret, epollfd, timeout;
	struct epoll_event ev, events[PE_MAX_EVENTS];
	ucflt_resp_t* ptrpe = NULL;
	pe_con_info_t *pe_data;
	ucflt_req_t *req;

	UNUSED_ARGUMENT(arg);

	memset((char *)&ev, 0, sizeof(struct epoll_event));
	memset((char *)&events[0], 0, sizeof(events));

	epollfd = epoll_create(1); //single unix domain fd
	if (epollfd < 0) {
		DBG_LOG(SEVERE, MOD_PEMGR, "epoll_create() error:%s", strerror(errno));
		assert(0);
	}
	ev.events = 0;
	ev.events |= EPOLLIN;
	ev.data.fd = pe_ucflt_client_fd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, pe_ucflt_client_fd, &ev) < 0) {
		DBG_LOG(SEVERE, MOD_PEMGR, "%d fd has trouble when adding to epoll:%s",
			 pe_ucflt_client_fd,  strerror(errno));
		assert(0);
	}
	ucflt_resp_t _local_pe_msg;
	ptrpe = &_local_pe_msg;
	while(1)
	{
		timeout = 1000;
		count = epoll_wait(epollfd, events, PE_MAX_EVENTS, timeout);
		if (count < 0) {
			if (errno == EINTR)
				continue;
			DBG_LOG(SEVERE, MOD_PEMGR, "epoll_wait failed:%s", strerror(errno));
			continue;
		}
		if (count == 0) {
			continue;//timeout
		}
		ret  = pe_ucflt_receive_msg(ptrpe);
		if (ret == -1) {
			/* We don't know which ptrpe this is.
			   So let the pending queue cleaner handle it
			*/
			continue;
		} else { 
			//todo whatever needs to be done with response
			//like hash_and_insert for dns
			struct nkn_task *ntask=NULL;
			ntask = nkn_task_get_task(ptrpe->pe_task_id);

			req = nkn_task_get_data(ptrpe->pe_task_id);
			if (req) {
				add_ucflt_info_t(req->url, &ptrpe->info);
			}
			
		}
		ret = pe_ucflt_pend_remove(ptrpe->pe_task_id);
		if (ret == 0) {
			AO_fetch_and_add1(&glob_ucflt_lookup_completed);
			pe_data = nkn_task_get_private(TASK_TYPE_PE_HELPER, ptrpe->pe_task_id);
			if (pe_data && (pe_data->incarn == gnm[pe_data->fd].incarn) 
					&& (pe_data->con->magic == CON_MAGIC_USED) 
					&& CHECK_CON_FLAG(pe_data->con, CONF_UCFLT_TASK)) {
				memcpy(&pe_data->con->uc_inf, &ptrpe->info, sizeof(ucflt_info_t));
			}
			nkn_task_set_action_and_state((nkn_task_id_t)(ptrpe->pe_task_id),
							TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);
		}
	}
}

void pe_ucflt_mgr_input(nkn_task_id_t id)
{
	ucflt_req_t *data = (ucflt_req_t *)nkn_task_get_data(id);
	pe_con_info_t *pe_data = NULL;
	
	data->magic = UCFLT_MAGIC;
	data->pe_task_id = id;
	data->op_code = UCFLT_LOOKUP;
	AO_fetch_and_add1(&glob_ucflt_lookup_called);
	if (pe_ucflt_send_msg(data) == -1) {
		AO_fetch_and_add1(&glob_ucflt_lookup_failed);
		//todo error condition insert negative hash
		pe_data = nkn_task_get_private(TASK_TYPE_PE_HELPER, id);
		if (pe_data) {
			pe_data->con->uc_inf.status_code = 500;
		}
		nkn_task_set_action_and_state((nkn_task_id_t)(data->pe_task_id),
						TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);
	}
	return;
}

/*Dummy output function for scheduler*/
void pe_ucflt_mgr_output(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
	return;
}

/*Dummy output function for scheduler*/
void pe_ucflt_mgr_cleanup(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
	return;
}

int pe_ucflt_client_init(void);
int pe_ucflt_client_init(void)
{
	if (pe_ucflt_open_sock_dgram("/config/nkn/.ucflt_c") == -1) {
		return -1;
	}
	memset(&ucflt_daemon_addr, 0, sizeof(struct sockaddr_un));
	ucflt_daemon_addr.sun_family = AF_UNIX;
	strcpy(ucflt_daemon_addr.sun_path, "/config/nkn/.ucflt_d");
	pthread_t worker_pethr;
	if (pthread_create(&worker_pethr, NULL, pe_ucflt_client_func, NULL) != 0) {
		DBG_LOG(SEVERE, MOD_PEMGR,
			"Failed to create pe_client thread");
		DBG_ERR(SEVERE,"Failed to create pe thread");
		return -1;
	}

	pthread_t check_peq;
	if (pthread_create(&check_peq, NULL, pe_ucflt_check_pendq, NULL) != 0) {
		DBG_LOG(SEVERE, MOD_PEMGR,
			"Failed to create check pe pending q thread");
		DBG_ERR(SEVERE,"Failed to create check_pe_pending_q thread");
		return -1;
	}

	nkn_ucflt_info_trie = nkn_trie_create(trie_ucflt_info_copy, 
						trie_ucflt_info_destruct);
	pthread_mutex_init(&ucflt_info_trie_mutex, NULL);
	DBG_LOG(MSG, MOD_PEMGR, "PE ucflt client initialized");
	return 0;
}

ucflt_info_t* get_ucflt_info_t(char *url, ucflt_info_t* p_ucflt_inf)
{
	ucflt_info_t* p_info = NULL;

	pthread_mutex_lock(&ucflt_info_trie_mutex);
	p_info = (ucflt_info_t *)nkn_trie_exact_match(nkn_ucflt_info_trie,
	                                           url);
	// If in cache copy the data within lock, as it might be deleted
	// 	anytime.
	if (p_info) {
		memcpy(p_ucflt_inf, p_info, sizeof(ucflt_info_t));
	}
	pthread_mutex_unlock(&ucflt_info_trie_mutex);
	if (p_info) {
		//todo something else if need be
		return p_info;
	} else {
		return NULL;
	}
}

int add_ucflt_info_t(char *url, ucflt_info_t* p_ucflt_inf)
{
	int rv;
	ucflt_info_t* p_info = NULL;

	p_info = new_ucflt_info_t(p_ucflt_inf);
	if (p_info) {
		pthread_mutex_lock(&ucflt_info_trie_mutex);
		rv = nkn_trie_add(nkn_ucflt_info_trie, url,
			  (nkn_trie_node_t) p_info);
		pthread_mutex_unlock(&ucflt_info_trie_mutex);
		if (!rv) {
			//todo something else if need be
			AO_fetch_and_add1(&glob_ucflt_entries_in_cache);
			return 0;
		} else {
			delete_ucflt_info_t(p_info);
			p_info = NULL;
		}
	}
	return 1;
}
