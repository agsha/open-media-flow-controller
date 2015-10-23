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
#include "pe_geodb.h"
#include "nkn_trie.h"

NKNCNT_DEF(geoip_lookup_completed, AO_t, "", "total geoip lookup completed")
NKNCNT_DEF(geoip_lookup_called, AO_t, "", "total geoip lookup called")
NKNCNT_DEF(geoip_lookup_failed, AO_t, "", "total geoip lookup failed")
NKNCNT_DEF(geoip_lookup_pending_q, AO_t, "", "total lookup pending q")
NKNCNT_DEF(geoip_lookup_pq_remove_err, AO_t, "", "total remove error")
NKNCNT_DEF(geoip_lookup_msg_send_err, AO_t, "", "total send error")
NKNCNT_DEF(geoip_lookup_msg_receive_err, AO_t, "", "total receive error")
NKNCNT_DEF(geoip_entries_in_cache, AO_t, "", "total entries in cache");

#define MAX_PEPENDQUERY     10000
#define MAX_PEPENDDELAY     15 //secs
#define MAX_GEOIP_CACHE_ENTRIES 100000

extern int geodb_installed;

int pe_geo_client_fd;
struct sockaddr_un dest_daemon_addr;

void pe_geo_mgr_input(nkn_task_id_t id);          //puts into taskqueue
void pe_geo_mgr_output(nkn_task_id_t id);         //dummy
void pe_geo_mgr_cleanup(nkn_task_id_t id);        //dummy


typedef struct pend_pe_t {
        struct pend_pe_t* next;
        int64_t pe_task_id;
        int64_t ts;
} pend_pe_t;

static struct pend_pe_t* pe_pend_hash[MAX_PEPENDQUERY];
static pthread_mutex_t pe_pend_mutex[MAX_PEPENDQUERY];

static pthread_mutex_t geo_info_trie_mutex;
static nkn_trie_t nkn_geo_info_trie;
geo_info_t* get_geo_info_t(uint32_t ip, geo_info_t* p_geoinf);
int add_geo_info_t(uint32_t ip, geo_info_t* ginfo_resp);

static void *trie_geo_info_copy(nkn_trie_node_t nd)
{
        return nd;
}

static void delete_geo_info_t(geo_info_t *pginfo)
{
        if (pginfo) {
                free(pginfo);
        }
}

static void trie_geo_info_destruct(nkn_trie_node_t nd)
{
        geo_info_t *pginfo = (geo_info_t *)nd;
        if (pginfo) {
                delete_geo_info_t(pginfo);
        }
}

static geo_info_t* new_geo_info_t(geo_info_t* ginfo_resp)
{
        geo_info_t* pginfo;
        pginfo = (geo_info_t *)nkn_calloc_type(1, sizeof(geo_info_t),
                                              mod_geo_info_t);
        memcpy(pginfo, ginfo_resp, sizeof(geo_info_t));
        return pginfo;
}

static int pe_geo_pend_insert(int64_t tid, uint32_t timeout) 
{
	uint32_t hash; 
	struct pend_pe_t* ptrpe;

	hash = tid % MAX_PEPENDQUERY;
	ptrpe = (struct pend_pe_t*)nkn_malloc_type(sizeof(struct pend_pe_t),mod_pe_pendq);
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
	AO_fetch_and_add1(&glob_geoip_lookup_pending_q);
	return 0;
}

static inline void pe_geo_pend_delete(uint32_t hash)
{
        struct pend_pe_t* ptrpe, *ptrpenext;

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
		AO_fetch_and_sub1(&glob_geoip_lookup_pending_q);
		AO_fetch_and_add1(&glob_geoip_lookup_completed);
		AO_fetch_and_add1(&glob_geoip_lookup_failed);
                ptrpe = ptrpenext;
        }
}

static int32_t pe_geo_pend_remove(int64_t tid)
{
        uint32_t hash;
        struct pend_pe_t* ptrpe = NULL, *ptrpeprev = NULL;

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
			AO_fetch_and_sub1(&glob_geoip_lookup_pending_q);
                        pthread_mutex_unlock(&pe_pend_mutex[hash]);
                        return 0;
                }
                ptrpeprev = ptrpe;
                ptrpe = ptrpe->next;
        }
        AO_fetch_and_add1(&glob_geoip_lookup_pq_remove_err);
        pthread_mutex_unlock(&pe_pend_mutex[hash]);
        return 1;
}

static void* pe_geo_check_pendq(void * arg)
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
                                pe_geo_pend_delete(i);
                                pthread_mutex_unlock(&pe_pend_mutex[i]);
                        }
                }
                /* Clear the cache entries if the number of cache entries exceed
                 * MAX_GEOIP_CACHE_ENTRIES or a new GeoIP database is installed.
                 */
                if ((AO_load(&glob_geoip_entries_in_cache) > MAX_GEOIP_CACHE_ENTRIES)
                		|| geodb_installed ) {
			pthread_mutex_lock(&geo_info_trie_mutex);
			rv = nkn_trie_destroy(nkn_geo_info_trie);
			AO_store(&glob_geoip_entries_in_cache, 0);
			geodb_installed = 0;	// Clear the flag set on CLI install command.
			nkn_geo_info_trie = nkn_trie_create(trie_geo_info_copy, 
						trie_geo_info_destruct);	
			pthread_mutex_unlock(&geo_info_trie_mutex);
                }
        }
        return NULL;
}


int pe_geo_open_sock_dgram(const char *path);
int pe_geo_open_sock_dgram(const char *path)
{
	struct sockaddr_un unix_socket_name;
	pe_geo_client_fd = -1;
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
	pe_geo_client_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (pe_geo_client_fd == -1)
	{
		DBG_LOG(SEVERE, MOD_PEMGR, "Unable to create domain socket:%s", strerror(errno));
		return -1;
	}
	if (bind(pe_geo_client_fd, (struct sockaddr *)&unix_socket_name, sizeof(unix_socket_name)))
	{
		DBG_LOG(SEVERE, MOD_PEMGR, "Unable to bind socket:%s", strerror(errno));
		close(pe_geo_client_fd);
		return -1;
	}
	return 0;
}

int pe_geo_receive_msg(geo_ip_t *ptr);
int pe_geo_receive_msg(geo_ip_t *ptr)
{
	struct sockaddr_un server_addr;
	int address_length;
	int rv = 0;
	address_length = sizeof(struct sockaddr_un);
	rv = recvfrom(pe_geo_client_fd, ptr, sizeof(geo_ip_t),0,
				(struct sockaddr *)&server_addr,
				(socklen_t *)&address_length);
	if ((rv == -1) || (rv < (int)sizeof(geo_ip_t)) || (ptr->magic != GEODB_MAGIC))
	{
		AO_fetch_and_add1(&glob_geoip_lookup_msg_receive_err);
		DBG_LOG(SEVERE, MOD_PEMGR, "Failure in recvmsg in recv_msg:%s", strerror(errno));
		return -1;
	}

	return rv;
}

int pe_geo_send_msg(geo_ip_req_t *ptr);
int pe_geo_send_msg(geo_ip_req_t *ptr)
{
	int rv = 0;
	pe_geo_pend_insert(ptr->pe_task_id, ptr->lookup_timeout);
	rv = sendto(pe_geo_client_fd, ptr, sizeof(geo_ip_req_t),0,
				(struct sockaddr *)&dest_daemon_addr,
				sizeof(struct sockaddr_un));
	if (rv == -1)
	{
		AO_fetch_and_add1(&glob_geoip_lookup_msg_send_err);
		pe_geo_pend_remove(ptr->pe_task_id);
		DBG_LOG(SEVERE, MOD_PEMGR, "Failure in sendto in send_msg:%s", strerror(errno));
		return -1;
	}
	return rv;
}

void *pe_geo_client_func(void *arg);
void *pe_geo_client_func(void *arg)
{
	prctl(PR_SET_NAME, "nvsd-pe-client", 0, 0, 0);
	int count, ret, epollfd, timeout;
	struct epoll_event ev, events[PE_MAX_EVENTS];
	geo_ip_t* ptrpe = NULL;
	pe_con_info_t *pe_data;

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
	ev.data.fd = pe_geo_client_fd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, pe_geo_client_fd, &ev) < 0) {
		DBG_LOG(SEVERE, MOD_PEMGR, "%d fd has trouble when adding to epoll:%s",
			 pe_geo_client_fd,  strerror(errno));
		assert(0);
	}
	geo_ip_t _local_pe_msg;
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
		ret  = pe_geo_receive_msg(ptrpe);
		if (ret == -1) {
			/* We don't know which ptrpe this is.
			   So let the pending queue cleaner handle it
			*/
			continue;
		} else { 
			//todo whatever needs to be done with response
			//like hash_and_insert for dns
			add_geo_info_t(htonl(ptrpe->ip), &ptrpe->ginf);
			
		}
		ret = pe_geo_pend_remove(ptrpe->pe_task_id);
		if (ret == 0) {
			AO_fetch_and_add1(&glob_geoip_lookup_completed);
			pe_data = nkn_task_get_private(TASK_TYPE_PE_HELPER, ptrpe->pe_task_id);
			if (pe_data && (pe_data->incarn == gnm[pe_data->fd].incarn) 
					&& (pe_data->con->magic == CON_MAGIC_USED) 
					&& CHECK_CON_FLAG(pe_data->con, CONF_GEOIP_TASK)) {
				memcpy(&pe_data->con->ginf, &ptrpe->ginf, sizeof(geo_info_t));
			}
			nkn_task_set_action_and_state((nkn_task_id_t)(ptrpe->pe_task_id),
							TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);
		}
	}
}

void pe_geo_mgr_input(nkn_task_id_t id)
{
	geo_ip_req_t *data = (geo_ip_req_t*)nkn_task_get_data(id);
	pe_con_info_t *pe_data = NULL;
	
	data->magic = GEODB_MAGIC;
	data->pe_task_id = id;
	data->op_code = GEODB_LOOKUP;
	AO_fetch_and_add1(&glob_geoip_lookup_called);
	if (pe_geo_send_msg(data) == -1) {
		AO_fetch_and_add1(&glob_geoip_lookup_failed);
		//todo error condition insert negative hash
		pe_data = nkn_task_get_private(TASK_TYPE_PE_HELPER, id);
		if (pe_data) {
			pe_data->con->ginf.status_code = 500;
		}
		nkn_task_set_action_and_state((nkn_task_id_t)(data->pe_task_id),
						TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);
	}
	return;
}
/*Dummy output function for scheduler*/
void pe_geo_mgr_output(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
	return;
}

/*Dummy output function for scheduler*/
void pe_geo_mgr_cleanup(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
	return;
}

int pe_geo_client_init(void);
int pe_geo_client_init(void)
{
	if (pe_geo_open_sock_dgram("/config/nkn/.pemgr_c") == -1) {
		return -1;
	}
	memset(&dest_daemon_addr, 0, sizeof(struct sockaddr_un));
	dest_daemon_addr.sun_family = AF_UNIX;
	strcpy(dest_daemon_addr.sun_path, "/config/nkn/.pemgr_d");
	pthread_t worker_pethr;
	if (pthread_create(&worker_pethr, NULL, pe_geo_client_func, NULL) != 0) {
		DBG_LOG(SEVERE, MOD_PEMGR,
			"Failed to create pe_client thread");
		DBG_ERR(SEVERE,"Failed to create pe thread");
		return -1;
	}

	pthread_t check_peq;
	if (pthread_create(&check_peq, NULL, pe_geo_check_pendq, NULL) != 0) {
		DBG_LOG(SEVERE, MOD_PEMGR,
			"Failed to create check pe pending q thread");
		DBG_ERR(SEVERE,"Failed to create check_pe_pending_q thread");
		return -1;
	}
	
	nkn_geo_info_trie = nkn_trie_create(trie_geo_info_copy, 
						trie_geo_info_destruct);	
	pthread_mutex_init(&geo_info_trie_mutex, NULL);
	DBG_LOG(MSG, MOD_PEMGR, "PE client initialized");
	return 0;
}

geo_info_t* get_geo_info_t(uint32_t ip, geo_info_t* p_geoinf)
{
	geo_info_t* pginfo = NULL;
	char ip_key[32];
	struct in_addr in;

	in.s_addr = ip;
	memcpy(ip_key, inet_ntoa(in), sizeof(ip_key));
	pthread_mutex_lock(&geo_info_trie_mutex);
	pginfo = (geo_info_t *)nkn_trie_exact_match(nkn_geo_info_trie,
	                                           (char *)ip_key);
	// If in cache copy the data within lock, as it might be deleted
	// 	anytime.
	if (pginfo) {
		memcpy(p_geoinf, pginfo, sizeof(geo_info_t));
	}
	pthread_mutex_unlock(&geo_info_trie_mutex);
	if (pginfo) {
		//todo something else if need be
		return pginfo;
	} else {
		return NULL;
	}
}

int add_geo_info_t(uint32_t ip, geo_info_t* ginfo_resp)
{
	int rv;
	geo_info_t* pginfo = NULL;
	char ip_key[32];
	struct in_addr in;

	in.s_addr = ip;
	memcpy(ip_key, inet_ntoa(in), sizeof(ip_key));
	pginfo = new_geo_info_t(ginfo_resp);
	if (pginfo) {
		pthread_mutex_lock(&geo_info_trie_mutex);
		rv = nkn_trie_add(nkn_geo_info_trie, (char *)ip_key,
			  (nkn_trie_node_t) pginfo);
		pthread_mutex_unlock(&geo_info_trie_mutex);
		if (!rv) {
			//todo something else if need be
			AO_fetch_and_add1(&glob_geoip_entries_in_cache);
			return 0;
		} else {
			delete_geo_info_t(pginfo);
			pginfo = NULL;
		}
	}
	return 1;
}
