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
#include "nkn_authmgr.h"
#include "nkn_verimatrix_helper.h"
#undef __FD_SETSIZE
#define __FD_SETSIZE MAX_GNM 

NKNCNT_DEF(dns_task_called, AO_t, "", "total dns task")
NKNCNT_DEF(dns_task_completed, AO_t, "", "total dns task")
NKNCNT_DEF(dns_task_failed, AO_t, "", "total dns tasks failed")
NKNCNT_DEF(dns_servfail, AO_t, "", "total dns serv failures")
NKNCNT_DEF(dns_channel_reinit, AO_t, "", "total times dns challe re-inited")

#define DEFAULT_DNS_TTL 255
#define DEFAULT_DNS_ERR_TTL 60

pthread_mutex_t cares_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cares_cond = PTHREAD_COND_INITIALIZER;
nkn_mutex_t authreq_mutex; 			//Multiple threads to handle auth tasks
nkn_mutex_t adnsreq_mutex;			//Single thread to handle incoming DNS responses
pthread_t auththr[MAX_AM_THREADS];		
pthread_t adnsthr;
pthread_cond_t authreq_cond = PTHREAD_COND_INITIALIZER;
nkn_task_id_t auth_task_arr[MAX_AM_TASK_ARR];	//Task queue in which the scheduler puts auth tasks
volatile int auth_task_arr_tail;
volatile int auth_task_arr_head;
extern void auth_htbl_list_init(void);		//init function for hash and the list
extern int auth_fetch_local(auth_msg_t*);		//function to fetch from the hashtable
extern int auth_update_local(auth_msg_t*);	//update local hash table
extern int auth_htbl_sz(void);			//size of hash table
extern void dns_hash_and_insert(char *domain, ip_addr_t *ip_ptr, int32_t *ttl, 
uint8_t resolved, int num);
extern int nkn_max_domain_ips;

ares_channel channel;				//c-ares channel, which can handle upto 16 nameservers
extern int is_loopback(ip_addr_t *ip_ptr);
extern char *get_ip_str(ip_addr_t *ip_ptr);

extern int send_msg(struct auth_dns_t *ptr);
extern int receive_msg(struct auth_dns_t *ptr);
extern int adns_client_init(void);
extern void adns_daemon_input(nkn_task_id_t id);
int adnsd_enabled = 0;

int nkn_max_domain_ips = 3;
int http_cfg_fo_use_dns;
struct ares_options options;
int channel_ready;
int optmask;
static int enable_reinit = 1;

static int isam_task_arrFull( void )
{
    	if ((auth_task_arr_tail < auth_task_arr_head)
        	&& (auth_task_arr_head - auth_task_arr_tail) == 1)
	{
        	return 1;
	}
    	if ((auth_task_arr_tail == (MAX_AM_TASK_ARR - 1))
        	&& (auth_task_arr_head == 0))
	{
        	return 1;
	}
    	return 0;
}

static int isam_task_arrCnt(void )
{
    	if (auth_task_arr_head < auth_task_arr_tail)
	{
        	return auth_task_arr_tail - auth_task_arr_head;
	}
    	else
	{
        	if (auth_task_arr_head == auth_task_arr_tail)
		{
            		return 0;
		}
        	else
		{
            		return (auth_task_arr_tail + MAX_AM_TASK_ARR -
                    		auth_task_arr_head);
		}
    	}
}

/*Function to use appropriate client for fetching auth data*/
static int auth_mgr_fetch_auth_client(auth_msg_t *data)
{
        int ret=-1;
        switch(data->authtype)
        {
                case VERIMATRIX:
                {
		        ret = verimatrix_helper(data);
			if (ret != 200) {
			    data->errcode = -1;
                DBG_LOG(ERROR, MOD_AUTHMGR,
						"FAILED: fetching key from verimatrix");
			} else {
                DBG_LOG(MSG, MOD_AUTHMGR,
						"SUCCESS: fetching key from verimatrix");
			}
                        break;
                }
                case RADIUS:
                {
                        DBG_LOG(MSG, MOD_AUTHMGR,
                                "fetching auth from radius");
                        break;
                }
                default:
                {
                        DBG_LOG(MSG, MOD_AUTHMGR,
                                "invalid auth fetch client");
                        data->errcode=-1;
                        break;
                }
        }
        data->errcode=ret;
        return ret;

}

/*Function to lookup auth data locally and call client, if not found*/
static void auth_mgr_fetch_auth(auth_msg_t *data)
{
        int ret;
        ret=auth_fetch_local(data);
        if(!ret)
        {
                DBG_LOG(MSG, MOD_AUTHMGR, "fetched auth locally");
                return;
        }
        ret=auth_mgr_fetch_auth_client(data);
        if(ret == 0)
        {
                DBG_LOG(MSG, MOD_AUTHMGR, "fetched auth from remote");
                auth_update_local(data);
                DBG_LOG(MSG, MOD_AUTHMGR, "current local tbl size:%d",auth_htbl_sz());
        }
        else
        {
                DBG_LOG(ERROR, MOD_AUTHMGR, "KEY FETCH FAILED : Check input params");
        }
        return ;
}

/*Auth worker threads call this to process auth requests*/
static void auth_mgr_thread_input(nkn_task_id_t id)
{
        struct nkn_task     *ntask = nkn_task_get_task(id);
        assert(ntask);
        auth_msg_t *data = (auth_msg_t*)nkn_task_get_data(id);
        auth_mgr_fetch_auth(data);
	nkn_task_set_action_and_state(id, TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);
}

/*Call back function for ares_gethostbyname.
* This is called after there is a DNS response
* Not used anymore*/
#if 0
void cares_callback(void *argdata, int status, int timeouts, struct hostent *hostptr);
void cares_callback(void *argdata, int status, int timeouts, struct hostent *hostptr)
{
	UNUSED_ARGUMENT(timeouts);
	auth_dns_t* pdns=(auth_dns_t*)(((auth_msg_t*)(argdata))->authdata);
	int32_t ttl = DEFAULT_DNS_TTL;

	AO_fetch_and_add1(&glob_dns_task_completed);

        if(status!=ARES_SUCCESS)
        {
		pdns->ip[0]=INADDR_NONE;
		DBG_LOG(ERROR, MOD_AUTHMGR,
		   "Dnslookup for domain:%s failed:%s",pdns->domain,ares_strerror(status));
		AO_fetch_and_add1(&glob_dns_task_failed);
        }
	else
	{
                memcpy((struct in_addr *)&pdns->ip, hostptr->h_addr, sizeof(struct in_addr));
                if(0x0100007F == pdns->ip[0]) {
                        // change 0x0100007F to INADDR_NONE
                        pdns->ip[0]=INADDR_NONE;
                }
	}
	pdns->num_ips=1;
	pdns->ttl[0] = ttl;
	//Negative hashing, forcing a failure in find_origin_server
	dns_hash_and_insert((char*)pdns->domain,&pdns->ip[0], &pdns->ttl[0], 0, pdns->num_ips);
	nkn_task_set_action_and_state((nkn_task_id_t)(pdns->auth_task_id),
				TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);

	if (enable_reinit && (status == ARES_ESERVFAIL)) {
		/* 
		 * something is wrong here. BIND9 servers do not respond to this
		 * channel after encountering this error, so reset everything
		 */
		channel_ready = 0;
		AO_fetch_and_add1(&glob_dns_servfail);
	}
}
#endif
/*Call back function for ares_query*/
void cares_callback_req(void *argdata, int status, int timeouts, 
				unsigned char *abuf, int alen);
void cares_callback_req(void *argdata, int status, int timeouts,
				unsigned char *abuf, int alen)
{
	UNUSED_ARGUMENT(timeouts);
	struct hostent *hostptr = NULL;
	int is_a_reply = 0;
	int nttl, cnt = 0;
	int i;
        struct ares_addrttl *ttls = (struct ares_addrttl*)
                                         nkn_malloc_type(sizeof(struct ares_addrttl) * nkn_max_domain_ips,
                                        mod_auth_addrttl);
        struct ares_addr6ttl *ttls6 = (struct ares_addr6ttl*)
                                         nkn_malloc_type(sizeof(struct ares_addr6ttl) * nkn_max_domain_ips,
                                        mod_auth_addr6ttl);

	if (http_cfg_fo_use_dns)
		nttl = nkn_max_domain_ips;
	else
		nttl = 1;

	auth_dns_t* pdns = (auth_dns_t*)(((auth_msg_t*)(argdata))->authdata);
	if (pdns == NULL) {
		DBG_LOG(ERROR, MOD_AUTHMGR, "invalid pdns entry");
		free(ttls);
		free(ttls6);
		return;
	}    

	pdns->resolved = 0;
	if(status != ARES_SUCCESS) {
		ip_addr_t ip;
		ip.addr.v4.s_addr = INADDR_NONE;
		ip.family = AF_INET;
		memcpy(&pdns->ip[0], &ip, sizeof(ip_addr_t));
		pdns->ttl[0] = DEFAULT_DNS_ERR_TTL; //nvsd defaults
		pdns->num_ips = 1;
		DBG_LOG(ERROR, MOD_AUTHMGR,
			"Dnslookup for domain:%s failed:%s", pdns->domain,
			ares_strerror(status));
		AO_fetch_and_add1(&glob_dns_task_failed);
		AO_fetch_and_add1(&glob_dns_task_completed);
		free(ttls);
		free(ttls6);
	} else {
		if (pdns->ip[0].family == AF_INET) {
			status = ares_parse_a_reply(abuf, alen, &hostptr, ttls, &nttl);
			is_a_reply = 1;
		} else {
			status = ares_parse_aaaa_reply(abuf, alen, &hostptr, ttls6, &nttl);
		}
		if (status != ARES_SUCCESS || !hostptr) {
			ip_addr_t ip;
			ip.addr.v4.s_addr = INADDR_NONE;
			ip.family = AF_INET;
			memcpy(&pdns->ip[0], &ip, sizeof(ip_addr_t));
			pdns->ttl[0] = DEFAULT_DNS_ERR_TTL; //nvsd defaults
			pdns->num_ips = 1;
			DBG_LOG(ERROR, MOD_AUTHMGR,
				"Dnslookup for domain:%s failed:%s in parsing ",
				pdns->domain,ares_strerror(status));
        		AO_fetch_and_add1(&glob_dns_task_failed);
			AO_fetch_and_add1(&glob_dns_task_completed);
			free(ttls);
			free(ttls6);
		} else	{
			for (i=0, cnt=nttl-1; i<nttl && cnt>=0; i++, cnt--) {
				/* multiple RR are given and we are chosing only
				 * the first one, so ttls[0].ipaddr will also provide
				 * the ip, memcopy not needed, its just assignment*/
				if (hostptr->h_length > (int)sizeof(struct in6_addr)) {
					DBG_LOG(ERROR, MOD_AUTHMGR, "incorrect host->h_length");
				} else {
					if (is_a_reply) {
						memcpy(&(pdns->ip[i].addr.v4.s_addr), &ttls[cnt].ipaddr.s_addr, sizeof(in_addr_t));
						pdns->ip[i].family = AF_INET;
						//apply nvsd default if something is wrong
						pdns->ttl[i] = (ttls[cnt].ttl<0)? DEFAULT_DNS_TTL:ttls[cnt].ttl;
					} else {
						memcpy(&(pdns->ip[i].addr.v6), &ttls6[cnt].ip6addr, sizeof(struct in6_addr));
						pdns->ip[i].family = AF_INET6;
						//apply nvsd default if something is wrong
						pdns->ttl[i] = (ttls6[cnt].ttl<0)? DEFAULT_DNS_TTL:ttls6[cnt].ttl;
					}
					
					if (is_loopback(&(pdns->ip[i])) == 0) {
						pdns->resolved = 1;
					}
				}
			}
			pdns->num_ips = nttl;
			AO_fetch_and_add1(&glob_dns_task_completed);

			//free ares memory
			ares_free_hostent(hostptr);
			free(ttls);
			free(ttls6);
		}
	}

	/* Negative hashing, forcing a failure in find_origin_server*/
	dns_hash_and_insert((char*)pdns->domain, &pdns->ip[0], &pdns->ttl[0], pdns->resolved, pdns->num_ips);

	nkn_task_set_action_and_state((nkn_task_id_t)(pdns->auth_task_id),
		TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);

        if (enable_reinit && (status == ARES_ESERVFAIL)) {
                /*
                 * something is wrong here. BIND9 servers do not respond to this
                 * channel after encountering this error, so reset everything
                 */
                channel_ready = 0;
                AO_fetch_and_add1(&glob_dns_servfail);
        }
}

/*Scheduler input function*/
void auth_mgr_input(nkn_task_id_t id)
{
        nkn_task_set_state(id, TASK_STATE_EVENT_WAIT);
        struct nkn_task     *ntask = nkn_task_get_task(id);
	int dns_type;
	int ret=0;
	char domain_name[2048];
	char *p_domain_name;

        assert(ntask);
	auth_msg_t *data = (auth_msg_t*)nkn_task_get_data(id);
	/* If its DNS lookup, the scheduler thread will just fire this request
	and leave, so no need to put it into a queue like auth tasks*/
        if (data->authtype==DNSLOOKUP)
        {
	        if (adnsd_enabled) {
                	adns_daemon_input(id);
                	return;
        	}

                auth_dns_t* pdns=(auth_dns_t*)(data->authdata);
		pdns->auth_task_id=id;
		//Returns void
		pthread_mutex_lock(&cares_mutex);
		AO_fetch_and_add1(&glob_dns_task_called);
		/* Since we will support ttl provided by nameserver
		 * we will not be using gethostbyname() anymore 
		 * Also, I should be using nameser.h and have ns_c_in
		 * and ns_t_a for the below query, but lots of woes.
                 * ares_gethostbyname(channel,(char*)pdns->domain,AF_INET,
		 *		cares_callback,data);
		 */

		/* TODO: use nameser.h */ 
		if (pdns->ip[0].family == AF_INET) {
			dns_type = 1; // ns_t_a
		} else {
			dns_type = 28; // ns_t_aaaa
		}

		if (pdns->dns_query_len == 0) {
			p_domain_name = (char *)pdns->domain;
		} else {
			memcpy(domain_name, pdns->domain, pdns->domain_len);
			domain_name[pdns->domain_len] = '.';
			memcpy(domain_name + pdns->domain_len + 1, pdns->dns_query, pdns->dns_query_len);
			domain_name[pdns->domain_len + 1 + pdns->dns_query_len] = '\0';
			p_domain_name = domain_name;
		}
		ares_query(channel,
			p_domain_name,
			1, /*ns_c_in = 1 Class: Internet. */
			dns_type,
			cares_callback_req, data);
		/*
		 * Wake up the select loop.
		 */
		if (channel_ready == 0) {
			ares_destroy(channel);
			//re-init the channel to get a new port
			ret = ares_init_options(&channel, &options, optmask);
			if (ret != ARES_SUCCESS) {
				DBG_LOG(SEVERE, MOD_AUTHMGR,"ares_init: %d %s", ret, ares_strerror(ret));
				assert(0);
			}
			channel_ready = 1;
			AO_fetch_and_add1(&glob_dns_channel_reinit);
			pthread_mutex_unlock(&cares_mutex);
			return;
		}
		pthread_cond_signal(&cares_cond);
		pthread_mutex_unlock(&cares_mutex);
        }
	else {
		NKN_MUTEX_LOCK(&authreq_mutex);
		if (isam_task_arrFull()) {
			nkn_task_set_action_and_state(id, TASK_ACTION_OUTPUT,
						TASK_STATE_RUNNABLE);
			NKN_MUTEX_UNLOCK(&authreq_mutex);
			return;
		}
		auth_task_arr[auth_task_arr_tail] = id;
		if (auth_task_arr_tail == (MAX_AM_TASK_ARR - 1))
		{
			auth_task_arr_tail = 0;
		}
		else
		{
			auth_task_arr_tail++;
		}
		pthread_cond_signal(&authreq_cond);
		NKN_MUTEX_UNLOCK(&authreq_mutex);
	}

	return;
}

/*Dummy output function for scheduler*/
void auth_mgr_output(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
	return;
}

/*Dummy output function for scheduler*/
void auth_mgr_cleanup(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
	return;
}

/*This is the function for worker auth threads, which pick task from auth q*/
void *auth_mgr_input_handler_thread(void *arg)
{
	UNUSED_ARGUMENT(arg);
     	nkn_task_id_t taskid;
     	prctl(PR_SET_NAME, "nvsd-auth-input-work", 0, 0, 0);
     	while (1)
     	{
                NKN_MUTEX_LOCK(&authreq_mutex);
                if (isam_task_arrCnt() < 1) 
		{
                        pthread_cond_wait(&authreq_cond, &authreq_mutex.lock);
                        NKN_MUTEX_UNLOCK(&authreq_mutex);
                }
 		else
		{
                        taskid = auth_task_arr[auth_task_arr_head];
                        if ( auth_task_arr_head == (MAX_AM_TASK_ARR - 1))
                                auth_task_arr_head = 0;
                        else
                                auth_task_arr_head++ ;
                        NKN_MUTEX_UNLOCK(&authreq_mutex);
                        auth_mgr_thread_input(taskid);
                }
     	}
}

#if 0
/*Function to constantly look for DNS responses*/
void *auth_mgr_adns_handler(void *arg);
void *auth_mgr_adns_handler(void *arg)
{
	UNUSED_ARGUMENT(arg);
	int count=0;
	struct timeval *tvp, tv;
	fd_set read_fds, write_fds;
	int nfds;
        prctl(PR_SET_NAME, "nvsd-adns", 0, 0, 0);

	/*Infinite loop, to process all the dns responses. Each channel can handle
	 16 name servers in RR fashion. Inside each channel there is one socket
	dedicated for one nameserver. So if there are 2 nameservers in /etc/resol.conf
	, then c-ares will assign 2 fds to the channel*/
        while(1)
        {
		FD_ZERO(&read_fds);
     		FD_ZERO(&write_fds);
		pthread_mutex_lock(&cares_mutex);
                nfds = ares_fds(channel, &read_fds, &write_fds);
                if(nfds==0)
		{
			pthread_cond_wait(&cares_cond, &cares_mutex);
			pthread_mutex_unlock(&cares_mutex);
			continue;
		}
		tvp = ares_timeout(channel, NULL, &tv);
		pthread_mutex_unlock(&cares_mutex);
		/*********************************************************
		The default timeout was 5 seconds with default retries
		as 4. The timeout algorith that c-ares adopts, has
		timeout increasing linearly for every retry, and can 
		go upto 75secs(~5+~10+~20+~40). To avoid such a big
		block in our select, reduced the timeout to 3secs
		and retries to 2, so max blocking limited to 9 secs
		Changes in ares_init.
		******************************************************/
                count = select(nfds, &read_fds, &write_fds, NULL, tvp);
                if (count < 0) // && errno != EINVAL)
                {
			//Something is wrong here, lets sleep over it
	                DBG_LOG(SEVERE, MOD_AUTHMGR,
       		                 "select failed in adns for nfds=%d error:%s", nfds, strerror(errno));
			usleep(1000);
			continue;
                }
		pthread_mutex_lock(&cares_mutex);
                ares_process(channel, &read_fds, &write_fds);
		pthread_mutex_unlock(&cares_mutex);
        }
}
#endif

void *auth_mgr_adns_handler_epoll(void *arg);
void *auth_mgr_adns_handler_epoll(void *arg)
{
	//struct timeval *tvp, tv, tv_copy;
	ares_socket_t dns_client_fds[16] = {0};
	struct epoll_event ev, events[DNS_MAX_EVENTS];
	int i,bitmask,nfds, epollfd, timeout, fd_count, ret;

	UNUSED_ARGUMENT(arg);
	memset(dns_client_fds, 0, sizeof(dns_client_fds));

	memset((char *)&ev, 0, sizeof(struct epoll_event));
	memset((char *)&events[0], 0, sizeof(events));

        epollfd = epoll_create(DNS_MAX_SERVERS);
        if (epollfd < 0) {
                DBG_LOG(SEVERE, MOD_AUTHMGR, "epoll_create() error");
                assert(0);
        }

        prctl(PR_SET_NAME, "nvsd-adns", 0, 0, 0);

	/*Infinite loop, to process all the dns responses. Each channel can handle
	 16 name servers in RR fashion. Inside each channel there is one socket
	dedicated for one nameserver. So if there are 2 nameservers in /etc/resol.conf
	, then c-ares will assign 2 fds to the channel*/
        while(1)
        {
		nfds=0;
		bitmask=0;
		for (i =0; i < DNS_MAX_SERVERS ; i++) {
			if (dns_client_fds[i] > 0) {
				if (epoll_ctl(epollfd, EPOLL_CTL_DEL, dns_client_fds[i], NULL) < 0) {
					//not a serious problem, strange that we should hit this case
					continue;
				}
			}
		}
		memset(dns_client_fds, 0, sizeof(dns_client_fds));
		pthread_mutex_lock(&cares_mutex);
		bitmask = ares_getsock(channel, dns_client_fds, DNS_MAX_SERVERS);
                for (i =0; i < DNS_MAX_SERVERS ; i++) {
                        if (dns_client_fds[i] > 0) {
				ev.events = 0;
				if (ARES_GETSOCK_READABLE(bitmask, i)) {
					ev.events |= EPOLLIN;
				}
				if (ARES_GETSOCK_WRITABLE(bitmask, i)) {
					ev.events |= EPOLLOUT;
				}
                                ev.data.fd = dns_client_fds[i];
                                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, dns_client_fds[i], &ev) < 0) {
                                        if(errno == EEXIST) {
                                                nfds++;
                                                continue;
                                        }
                                        DBG_LOG(SEVERE, MOD_AUTHMGR, "%d fd has trouble when adding to epoll:%s\n",
						dns_client_fds[i], strerror(errno));
                                        continue;
                                }
                                nfds++;
                        }
                }
                if(nfds==0)
		{
			pthread_cond_wait(&cares_cond, &cares_mutex);
			pthread_mutex_unlock(&cares_mutex);
			continue;
		}
		//tvp = ares_timeout(channel, NULL, &tv);
		//memcpy(&tv_copy, tvp, sizeof(struct timeval));
		pthread_mutex_unlock(&cares_mutex);
		//timeout = (tv_copy.tv_sec)*1000;//millisecs
		timeout = 1000;//millisecs
		/*********************************************************
		The default timeout was 5 seconds with default retries
		as 4. The timeout algorith that c-ares adopts, has
		timeout increasing linearly for every retry, and can 
		go upto 75secs(~5+~10+~20+~40). To avoid such a big
		block in our select, reduced the timeout to 3secs
		and retries to 2, so max blocking limited to 9 secs
		Changes in ares_init.
		******************************************************/
		fd_count = epoll_wait(epollfd, events, DNS_MAX_EVENTS, timeout);
		if (fd_count < 0) {
			DBG_LOG(SEVERE, MOD_AUTHMGR, "epoll_wait failed:%s", strerror(errno));
			continue;
		}
		pthread_mutex_lock(&cares_mutex);
		if (fd_count > 0) {
			for (i = 0; i < fd_count; ++i) {
				ares_process_fd(channel,
                                                ((events[i].events) & (EPOLLIN) ?
                                                events[i].data.fd:ARES_SOCKET_BAD),
                                                ((events[i].events) & (EPOLLOUT)?
                                                events[i].data.fd:ARES_SOCKET_BAD));
			}
		} else {
			ares_process_fd(channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
                }
		if (channel_ready == 0 ) {
			ares_destroy(channel);
			//re-init the channel to get a new port
			ret = ares_init_options(&channel, &options, optmask);
			if (ret != ARES_SUCCESS) {
				DBG_LOG(SEVERE, MOD_AUTHMGR,"ares_init: %d %s", ret, ares_strerror(ret));
				assert(0);
			}
			channel_ready = 1;
			AO_fetch_and_add1(&glob_dns_channel_reinit);
		}
		pthread_mutex_unlock(&cares_mutex);
        }
}

/* Init function to start auth worker threads and adns thread. 
It also initializes the c-ares library and c-ares channel*/
int auth_mgr_init(uint32_t init_flags)
{
	int ret,flags;
	NKN_MUTEX_INIT(&authreq_mutex, NULL, "authreq_mutex");
	/***********************************************************
	 * We do not need this for now, but will eventually need
	 * to handle the auth taks, key management etcc..
	 ***********************************************************/
	if (init_flags & AUTH_MGR_INIT_START_WORKER_TH) {
	    int32_t i;
	    for (i=0; i<MAX_AM_THREADS; i++) {
        	DBG_LOG(MSG, MOD_AUTHMGR,
			"Thread %d created for Auth Manager",i);
                if(pthread_create(&auththr[i], NULL,
				  auth_mgr_input_handler_thread, (void *)&i) != 0) {
		    DBG_LOG(SEVERE, MOD_AUTHMGR,
			    "Failed to create authmgr threads %d",i);
		    DBG_ERR(SEVERE, "Failed to create authmgr threads %d",i);
		    return -1;
                }
	    }
	}
	auth_htbl_list_init();

        /*All the c-ares relates stuff goes in here*/

	if (adnsd_enabled) {
		return adns_client_init();
	}
	flags = ARES_LIB_INIT_ALL;
        ares_library_cleanup();
        ret = ares_library_init(flags);
        if (ret != ARES_SUCCESS) {
                DBG_LOG(SEVERE, MOD_AUTHMGR,"ares_library_init: %d %s", ret,
                         ares_strerror(ret));
                return -1;
        }
        optmask = ARES_OPT_FLAGS|ARES_OPT_TIMEOUT|ARES_OPT_TRIES;

        options.flags = ARES_FLAG_STAYOPEN|ARES_FLAG_IGNTC|ARES_FLAG_NOCHECKRESP;
        options.timeout = DNS_MAX_TIMEOUT;
        options.tries = DNS_MAX_RETRIES;
        ret = ares_init_options(&channel, &options, optmask);
        if (ret != ARES_SUCCESS) {
                DBG_LOG(SEVERE, MOD_AUTHMGR,"ares_init: %d %s", ret,
                         ares_strerror(ret) );
                return -1;
        }
        channel_ready = 1;

        /* Additionally one more thread needs to handle all
        the DNS queries, so starting it here*/
        if (pthread_create(&adnsthr, NULL, auth_mgr_adns_handler_epoll, NULL) != 0) {
                DBG_LOG(SEVERE, MOD_AUTHMGR,
                        "Failed to create adns thread");
                DBG_ERR(SEVERE,"Failed to create adns thread");
                return -1;
        }


        return 0;

}


