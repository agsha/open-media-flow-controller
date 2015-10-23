#ifndef _NKN_ADNSD_API_H
#define _NKN_ADNSD_API_H


#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <assert.h>
#include <stdint.h>
#include <ctype.h>

#include "atomic_ops.h"
#include "nkn_lockstat.h"
#include "nkn_stat.h"
#include "nkn_debug.h"
#include "nkn_memalloc.h"
#include "nkn_defs.h"

#define UNUSED_ARGUMENT(x)      (void)x

#define DNS_MAX_TIMEOUT 	3  //in seconds
#define DNS_MAX_RETRIES 	2  
#define DNS_MAX_SERVERS         16
#define DNS_MAX_EVENTS          65535
#define DNS_MAGIC          0xaddaadda
#define DNS_MAGIC_DEAD     0xdeadadda
#define DNS_MAX_IPS		3  //This should match the value of nkn_max_domain_ips.

/*The below structure is used for async dns and has members
needed to kickstart waiting processes and other required stuff*/
typedef struct auth_dns_t
{
    uint32_t magic;
    ip_addr_t ip[DNS_MAX_IPS]; //DNS_MAX_IPS should match the value of nkn_max_domain_ips.
    uint8_t resolved;
    int32_t ttl[DNS_MAX_IPS];  //DNS_MAX_IPS should match the value of nkn_max_domain_ips.
    int num_ips;
    uint8_t domain[1024];
    int domain_len;
    uint8_t dns_query[64];
    int dns_query_len;
    /*Below member is needed to demultiplex incoming auth tasks in case
    of OM_get and OM_validate paths and will be one of the AUTH_DO*
    defined above. Other data needed is part of the
    task data or private data */
    uint32_t caller;
    /*Below are some members required to kick start tunnel path*/
    int32_t cp_sockfd;
    uint32_t cp_incarn;
    uint32_t con_incarn;
    int32_t con_fd;

    /*Additional task_id of auth_help task to be passed for the callback 
    function of c-ares*/
    int64_t auth_task_id;
    int64_t task_id;

    uint32_t reinit_channel;

} auth_dns_t;

void *adns_handler(void *arg);	//picks tasks from queue
int adns_init(uint32_t init_flags);		//initializes the worker threads
void adns_input(auth_dns_t *data);

#endif /* _NKN_ADNSD_API_H */
