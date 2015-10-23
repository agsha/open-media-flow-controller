#undef __FD_SETSIZE
#define __FD_SETSIZE  65536

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
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#define __USE_GNU
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <stdint.h>


#include <sys/resource.h>
#include <ares.h>

#include "nkn_defs.h"
#include "nkn_debug.h"
//#include "network.h"
#include "nkn_mgmt_agent.h"
#include "nkn_cfg_params.h"
#include "nkn_log_strings.h"
#include "nkn_trace.h"
#include "nkn_adns.h"

NKNCNT_DEF(adns_task_called, AO_t, "", "total dns task")
NKNCNT_DEF(adns_task_completed, AO_t, "", "total dns task")
NKNCNT_DEF(adns_task_failed, AO_t, "", "total dns tasks failed")
NKNCNT_DEF(adns_servfail, AO_t, "", "total dns tasks failed")
NKNCNT_DEF(adns_channel_reinit, AO_t, "", "total times dns challe re-inited")


#define DEFAULT_DNS_TTL 255
#define DEFAULT_DNS_ERR_TTL 60

pthread_mutex_t cares_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cares_cond = PTHREAD_COND_INITIALIZER;
pthread_t adnsthr;
extern int send_msg(auth_dns_t*);
ares_channel channel;				//c-ares channel, which can handle upto 16 nameservers
volatile int channel_ready;
struct ares_options options;
int optmask;
static int enable_reinit = 1;

/*Call back function for ares_gethostbyname.
* This is called after there is a DNS response
*Not used anymore*/
void cares_callback(void *argdata, int status, int timeouts, struct hostent *hostptr);
void cares_callback(void *argdata, int status, int timeouts, struct hostent *hostptr)
{
	UNUSED_ARGUMENT(timeouts);
	auth_dns_t *_am_data = argdata;
	//memcpy(&_am_data, argdata, sizeof(auth_dns_t));
	
	AO_fetch_and_add1(&glob_adns_task_completed);
        if(status!=ARES_SUCCESS)
        {
		IPv4(_am_data->ip[0])=INADDR_NONE;
		DBG_LOG(ERROR, MOD_ADNS,
		   "Dnslookup for domain:%s failed:%s",_am_data->domain,ares_strerror(status));
        }
	else {
		memcpy((struct in_addr *)&_am_data->ip, hostptr->h_addr, sizeof(struct in_addr));
	}
	send_msg(_am_data);
        if (enable_reinit && (status == ARES_ESERVFAIL)) {
                /*
                 * something is wrong here. BIND9 servers do not respond to this
                 * channel after encountering this error, so reset everything
                 */
                channel_ready = 0;
                AO_fetch_and_add1(&glob_adns_servfail);
        }
}

/*Call back function for ares_query*/
void cares_callback_areq(void *argdata, int status, int timeouts, 
				unsigned char *abuf, int alen);
void cares_callback_areq(void *argdata, int status, int timeouts,
				unsigned char *abuf, int alen)
{
        UNUSED_ARGUMENT(timeouts);
        auth_dns_t *_am_data= argdata;

	struct hostent *hostptr = NULL;
	struct ares_addrttl *ttls = (struct ares_addrttl*)
					 malloc(sizeof(struct ares_addrttl));
	int nttl=1;//number of resource records
	if(status != ARES_SUCCESS)
	{
		IPv4(_am_data->ip[0]) = INADDR_NONE;
		_am_data->ttl[0] = DEFAULT_DNS_ERR_TTL; //nvsd defaults
		DBG_LOG(ERROR, MOD_ADNS,
			"Dnslookup for domain:%s failed:%s", _am_data->domain,
			ares_strerror(status));
		AO_fetch_and_add1(&glob_adns_task_failed);
	}
	else
	{
		status = ares_parse_a_reply(abuf, alen, &hostptr, ttls, &nttl);
		if (status != ARES_SUCCESS || !hostptr)
		{
			IPv4(_am_data->ip[0])=INADDR_NONE;
			_am_data->ttl[0] = DEFAULT_DNS_ERR_TTL; //nvsd defaults
			DBG_LOG(ERROR, MOD_ADNS,
				"Dnslookup for domain:%s failed:%s in parsing ",
				_am_data->domain,ares_strerror(status));
        		AO_fetch_and_add1(&glob_adns_task_failed);
		}
		else
		{
			/* multiple RR are given and we are chosing only
			 * the first one, so ttls[0].ipaddr will also provide
			 * the ip, memcopy not needed, its just assignment*/
        		memcpy((struct in_addr *)(&_am_data->ip), hostptr->h_addr,
				sizeof(struct in_addr));
				 
			//apply nvsd default if something is wrong
			_am_data->ttl[0] = (ttls[0].ttl<0)? DEFAULT_DNS_TTL:ttls[0].ttl; 
			//free ares memory
			ares_free_hostent(hostptr);
		}
	}
	free(ttls);
	AO_fetch_and_add1(&glob_adns_task_completed);
	send_msg(_am_data);
	if (enable_reinit && (status == ARES_ESERVFAIL)) {
                /*
                 * something is wrong here. BIND9 servers do not respond to this
                 * channel after encountering this error, so reset everything
                 */
                channel_ready = 0;
                AO_fetch_and_add1(&glob_adns_servfail);
        }
}


/*Scheduler input function*/
void adns_input(auth_dns_t *data)
{
	int ret,i;

	//Returns void
	pthread_mutex_lock(&cares_mutex);
	/* Since we will support ttl provided by nameserver
	 * we will not be using gethostbyname() anymore 
	 * Also, I should be using nameser.h and have ns_c_in
	 * and ns_t_a for the below query, but lots of woes.
         * ares_gethostbyname(channel,(char*)pdns->domain,AF_INET,
	 *		cares_callback,data);
	 */
	AO_fetch_and_add1(&glob_adns_task_called);
	ares_query(channel,
        	(char*)data->domain,
        	1, /*ns_c_in = 1 Class: Internet. */
        	1, /*ns_t_a = 1 Type:Host address. */
        	cares_callback_areq, data);
	if (channel_ready == 0) {
		ares_destroy(channel);
			//re-init the channel to get a new port
		ret = ares_init_options(&channel, &options, optmask);
		if (ret != ARES_SUCCESS) {
			DBG_LOG(SEVERE, MOD_ADNS,"ares_init: %d %s", ret, ares_strerror(ret));
			assert(0);
		}
		channel_ready = 1;
		AO_fetch_and_add1(&glob_adns_channel_reinit);
		pthread_mutex_unlock(&cares_mutex);
		return;
        }
	pthread_cond_signal(&cares_cond);
	pthread_mutex_unlock(&cares_mutex);
	return;
}

/*Function to constantly look for DNS responses*/
void *adns_handler(void *arg)
{
	UNUSED_ARGUMENT(arg);
	int count=0;
	struct timeval *tvp, tv;
	fd_set read_fds, write_fds;
	int nfds;
        prctl(PR_SET_NAME, "adns-hdlr", 0, 0, 0);

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
	                DBG_LOG(SEVERE, MOD_ADNS,
       		                 "select failed in adns");
			continue;
                }
		pthread_mutex_lock(&cares_mutex);
                ares_process(channel, &read_fds, &write_fds);
		pthread_mutex_unlock(&cares_mutex);
        }
}
void *adns_handler_epoll(void *arg);
void *adns_handler_epoll(void *arg)
{
        //struct timeval *tvp, tv, tv_copy;
        ares_socket_t dns_client_fds[16] = {0};
        struct epoll_event ev, events[DNS_MAX_EVENTS];
        int i,bitmask,nfds, epollfd, timeout, fd_count, ret;

        UNUSED_ARGUMENT(arg);
        memset(dns_client_fds, 0, sizeof(dns_client_fds));
        epollfd = epoll_create(DNS_MAX_SERVERS);
        if (epollfd < 0) {
                DBG_LOG(SEVERE, MOD_ADNS, "epoll_create() error");
                assert(0);
        }

        prctl(PR_SET_NAME, "adns-hdlr", 0, 0, 0);

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
                                        DBG_LOG(SEVERE, MOD_ADNS, "%d fd has trouble when adding to epoll:%s\n",
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
                        DBG_LOG(SEVERE, MOD_ADNS, "epoll_wait failed:%s", strerror(errno));
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
                                DBG_LOG(SEVERE, MOD_ADNS,"ares_init: %d %s", ret, ares_strerror(ret));
                                assert(0);
                        }
                        channel_ready = 1;
			AO_fetch_and_add1(&glob_adns_channel_reinit);
                }
                pthread_mutex_unlock(&cares_mutex);
        }
}


/* Init function to start auth worker threads and adns thread. 
It also initializes the c-ares library and c-ares channel*/
int adns_init(uint32_t init_flags)
{
	int ret;
	int flags = ARES_LIB_INIT_ALL;
	/*All the c-ares relates stuff goes in here*/
	ares_library_cleanup();
        ret = ares_library_init(flags);
        if (ret != ARES_SUCCESS) {
                DBG_LOG(SEVERE, MOD_ADNS,"ares_library_init: %d %s", ret,
			 ares_strerror(ret));
                return -1;
        }
        optmask = ARES_OPT_FLAGS|ARES_OPT_TIMEOUT|ARES_OPT_TRIES;

        options.flags = ARES_FLAG_STAYOPEN|ARES_FLAG_IGNTC|ARES_FLAG_NOCHECKRESP;
	options.timeout = DNS_MAX_TIMEOUT;
	options.tries = DNS_MAX_RETRIES;
        ret = ares_init_options(&channel, &options, optmask);
        if (ret != ARES_SUCCESS) {
                DBG_LOG(SEVERE, MOD_ADNS,"ares_init: %d %s", ret,
			 ares_strerror(ret) );
                return -1;
        }
	channel_ready = 1;
	/* Additionally one more thread needs to handle all
	the DNS queries, so starting it here*/
	if (pthread_create(&adnsthr, NULL, adns_handler_epoll, NULL) != 0) {
		DBG_LOG(SEVERE, MOD_ADNS,
			"Failed to create adns thread");
		DBG_ERR(SEVERE,"Failed to create adns thread");
		return -1;
	}
        return 0;
}


