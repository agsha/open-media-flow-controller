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
#include "nkn_adns.h"
#include "nkn_authmgr.h"

NKNCNT_EXTERN(dns_task_completed, AO_t);
NKNCNT_EXTERN(dns_task_called, AO_t);
NKNCNT_EXTERN(dns_task_failed, AO_t);

NKNCNT_DEF(dns_pending_q, AO_t, "", "total dns task")

#define MAX_DNSPENDQUERY     10000
#define MAX_DNSPENDDELAY     15 //secs


int adns_client_fd;
struct sockaddr_un dest_daemon_addr;

extern void dns_hash_and_insert(char *domain, ip_addr_t *ip_ptr, int32_t *ttl,
uint8_t resolved, int num);
typedef struct pend_adns_t {
        struct pend_adns_t* next;
        int64_t auth_task_id;
        uint64_t ts;
} pend_adns_t;

static struct pend_adns_t* dns_pend_hash[MAX_DNSPENDQUERY];
static pthread_mutex_t dns_pend_mutex[MAX_DNSPENDQUERY];

static int dns_pend_insert(int64_t tid) 
{
	uint32_t hash; 
	struct pend_adns_t* pdns;	

	hash = tid % MAX_DNSPENDQUERY;
	pdns = (struct pend_adns_t*)nkn_malloc_type(sizeof(struct pend_adns_t),mod_dns_pendq);
	if(!pdns) return -1;

	pdns->ts = nkn_cur_ts;
	pdns->auth_task_id = tid;
	pthread_mutex_lock(&dns_pend_mutex[hash]);
	dns_pend_hash[hash] = pdns;
	pdns->next = NULL;
	pthread_mutex_unlock(&dns_pend_mutex[hash]);
	AO_fetch_and_add1(&glob_dns_pending_q);
	return 0;
}

static inline void dns_pend_delete(uint32_t hash)
{
        struct pend_adns_t* padns, * padnsnext;

        padns = dns_pend_hash[hash];
        if( !padns ) return;

        /*
         * the link list is sorted by time. latest time on the top
         */
        if(nkn_cur_ts - padns->ts > MAX_DNSPENDDELAY) {
                dns_pend_hash[hash] = NULL;
                goto delete_chain;
        }
        while(padns->next) {
                if(nkn_cur_ts - padns->next->ts > MAX_DNSPENDDELAY) {
                        padnsnext = padns->next;
                        padns->next = NULL;
                        padns = padnsnext;
                        goto delete_chain;
                }
                padns = padns->next;
        }
        return;

delete_chain:
        /*
         * Time to delete the expired dns entry.
         */
        while(padns) {
                padnsnext = padns->next;
		DBG_LOG(MSG, MOD_AUTHMGR, "dns_pend_delete: Deleting authid = %ld", padns->auth_task_id);
                nkn_task_set_action_and_state((nkn_task_id_t)(padns->auth_task_id),
                                                TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);
                free(padns);
		AO_fetch_and_sub1(&glob_dns_pending_q);
		AO_fetch_and_add1(&glob_dns_task_completed);
		AO_fetch_and_add1(&glob_dns_task_failed);
                padns = padnsnext;
        }
}

static int32_t dns_pend_remove(int64_t tid)
{
        uint32_t hash;
        struct pend_adns_t* padns = NULL, *padnsprev = NULL;

        /* get hash value for this domain */
        hash = tid % MAX_DNSPENDQUERY;
        pthread_mutex_lock(&dns_pend_mutex[hash]);
        padns = dns_pend_hash[hash];

        while(padns) {
                if(padns->auth_task_id == tid) {
                        if (padnsprev==NULL) {/*First entry*/ 
                                dns_pend_hash[hash] = padns->next;

                        } else {
                                padnsprev->next = padns->next;
                        }
                        free(padns);
			AO_fetch_and_sub1(&glob_dns_pending_q);
                        pthread_mutex_unlock(&dns_pend_mutex[hash]);
                        return 0;
                }
                padnsprev = padns;
                padns = padns->next;
        }
        pthread_mutex_unlock(&dns_pend_mutex[hash]);
        return 1;
}

static void* dns_check_pendq(void * arg)
{
        int i;

	UNUSED_ARGUMENT(arg);
        prctl(PR_SET_NAME, "nvsd-dns-pendq", 0, 0, 0);

        while(1) {
                sleep(3); // sleep 3 seconds

                for (i=0; i<MAX_DNSPENDQUERY; i++) {
                        if (dns_pend_hash[i]) {
                                pthread_mutex_lock(&dns_pend_mutex[i]);
                                dns_pend_delete(i);
                                pthread_mutex_unlock(&dns_pend_mutex[i]);
                        }
                }
        }
        return NULL;
}


int open_sock_dgram(const char *path);
int open_sock_dgram(const char *path)
{
        struct sockaddr_un unix_socket_name;
        adns_client_fd = -1;
        memset(&unix_socket_name,0,sizeof(struct sockaddr_un));
        if (unlink(path) < 0)
        {
                if (errno != ENOENT)
                {
                        DBG_LOG(SEVERE, MOD_AUTHMGR, "Failed to unlink %s:%s",path, strerror(errno));
                        return -1;
                }
        }

        unix_socket_name.sun_family = AF_UNIX;
        strcpy(unix_socket_name.sun_path, path);
        adns_client_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
        if (adns_client_fd == -1)
        {
		DBG_LOG(SEVERE, MOD_AUTHMGR, "Unable to create domain socket:%s", strerror(errno));
                return -1;
        }
        if (bind(adns_client_fd, (struct sockaddr *)&unix_socket_name, sizeof(unix_socket_name)))
        {
		DBG_LOG(SEVERE, MOD_AUTHMGR, "Unable to bind socket:%s", strerror(errno));
                close(adns_client_fd);
                return -1;
        }
        return 0;
}

int receive_msg(struct auth_dns_t *ptr);
int receive_msg(struct auth_dns_t *ptr)
{
        struct sockaddr_un server_addr;
        int address_length;
        int rv = 0;
        address_length = sizeof(struct sockaddr_un);
        rv = recvfrom(adns_client_fd, ptr, sizeof(auth_dns_t),0,
                                (struct sockaddr *)&server_addr,
                                (socklen_t *)&address_length);
        if (rv == -1)
        {
                DBG_LOG(SEVERE, MOD_AUTHMGR, "Failure in recvmsg in recv_msg:%s", strerror(errno));
                return -1;
        }

        return rv;
}

int send_msg(struct auth_dns_t *ptr);
int send_msg(struct auth_dns_t *ptr)
{
        int rv = 0;
        rv = sendto(adns_client_fd, ptr, sizeof(auth_dns_t),0,
                                (struct sockaddr *)&dest_daemon_addr,
                                sizeof(struct sockaddr_un));
        if (rv == -1)
        {
		DBG_LOG(SEVERE, MOD_AUTHMGR, "Failure in sendto in send_msg:%s", strerror(errno));
                return -1;
        }
	dns_pend_insert(ptr->auth_task_id);
        return rv;
}

void *adns_client_func(void *arg);
void *adns_client_func(void *arg)
{
        prctl(PR_SET_NAME, "nvsd-adns-client", 0, 0, 0);
        int count, ret, epollfd, timeout;
        struct epoll_event ev, events[DNS_MAX_EVENTS];

	UNUSED_ARGUMENT(arg);

        epollfd = epoll_create(1); //single unix domain fd
        if (epollfd < 0) {
		DBG_LOG(SEVERE, MOD_AUTHMGR, "epoll_create() error:%s", strerror(errno));
                assert(0);
        }
        ev.events = 0;
        ev.events |= EPOLLIN;
        ev.data.fd = adns_client_fd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, adns_client_fd, &ev) < 0) {
		DBG_LOG(SEVERE, MOD_AUTHMGR, "%d fd has trouble when adding to epoll:%s",
			 adns_client_fd,  strerror(errno));
                assert(0);
        }
        auth_dns_t _local_auth_msg;
	auth_dns_t* pdns = &_local_auth_msg;
        while(1)
        {
                timeout=1000;
                count = epoll_wait(epollfd, events, DNS_MAX_EVENTS, timeout);
                if (count < 0) {
			DBG_LOG(SEVERE, MOD_AUTHMGR, "epoll_wait failed:%s", strerror(errno));
                        continue;
                }
                if (count == 0) {
                        continue;//timeout
                }
                ret  = receive_msg(pdns);
		if (ret == -1) {
			/* We don't know which pdns this is.
			   So let the pending queue cleaner handle it
			*/
			continue;
                } else { 
			dns_hash_and_insert((char*)pdns->domain, &pdns->ip[0], &pdns->ttl[0], pdns->resolved, pdns->num_ips);
		}
		dns_pend_remove(pdns->auth_task_id);
		AO_fetch_and_add1(&glob_dns_task_completed);
		nkn_task_set_action_and_state((nkn_task_id_t)(pdns->auth_task_id),
						TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);
        }
}

void adns_daemon_input(nkn_task_id_t id);
void adns_daemon_input(nkn_task_id_t id)
{
        auth_msg_t *data = (auth_msg_t*)nkn_task_get_data(id);
        auth_dns_t* pdns=(auth_dns_t*)(data->authdata);
        pdns->magic = DNS_MAGIC;
        pdns->auth_task_id=id;
        if (send_msg(pdns) == -1) {
		ip_addr_t ip;
		ip.addr.v4.s_addr = INADDR_NONE;
		ip.family = AF_INET;
        	AO_fetch_and_add1(&glob_dns_task_failed);
		memcpy(&pdns->ip, &ip, sizeof(ip_addr_t));
		dns_hash_and_insert((char*)pdns->domain, &pdns->ip[0], &pdns->ttl[0], pdns->resolved, pdns->num_ips);
		nkn_task_set_action_and_state((nkn_task_id_t)(pdns->auth_task_id),
						TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);
        } else {
		AO_fetch_and_add1(&glob_dns_task_called);
	}
        return;
}

int adns_client_init(void);
int adns_client_init(void)
{
	if (open_sock_dgram("/config/nkn/.adns_c") == -1) {
		return -1;
	}
        memset(&dest_daemon_addr, 0, sizeof(struct sockaddr_un));
        dest_daemon_addr.sun_family = AF_UNIX;
        strcpy(dest_daemon_addr.sun_path, "/config/nkn/.adns_d");
	pthread_t worker_adns;
        if (pthread_create(&worker_adns, NULL, adns_client_func, NULL) != 0) {
                DBG_LOG(SEVERE, MOD_AUTHMGR,
                        "Failed to create adns_client thread");
                DBG_ERR(SEVERE,"Failed to create adns thread");
                return -1;
        }

        pthread_t check_dnsq;
        if (pthread_create(&check_dnsq, NULL, dns_check_pendq, NULL) != 0) {
                DBG_LOG(SEVERE, MOD_AUTHMGR,
                        "Failed to create check dns pending q thread");
                DBG_ERR(SEVERE,"Failed to create check_dns_pending_q thread");
                return -1;
        }
	DBG_LOG(MSG, MOD_AUTHMGR, "ADNS client initialized");
	return 0;
}


