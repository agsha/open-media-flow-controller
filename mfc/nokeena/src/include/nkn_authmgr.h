#ifndef _NKN_AUTHM_API_H
#define _NKN_AUTHM_API_H


#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <assert.h>
#include <stdint.h>
#include <ctype.h>

#include "nkn_sched_api.h"
#include "atomic_ops.h"
#include "nkn_lockstat.h"
#include "nkn_stat.h"
#include "nkn_debug.h"
#include "nkn_memalloc.h"
#include "server.h"
#include "nkn_adns.h"

#define UNUSED_ARGUMENT(x)      (void)x
#define MAX_AM_THREADS 		8
#define MAX_AM_TASK_ARR 	256001
#define AUTH_MGR_MAGIC 		0xaddaadda
#define AUTH_MGR_MAGIC_DEAD 	0xdeadadda

#define AUTH_DO_OM_GET		1
#define AUTH_DO_OM_VALIDATE	2
#define AUTH_DO_TUNNEL		3

#define AUTH_MGR_INIT_START_WORKER_TH 0x00000001

typedef enum 
{
	VERIMATRIX,
	RADIUS,
	DNSLOOKUP,
	NATIVE

}AUTHTYPE;

#define VERIMATRIX_KEY_CHAIN_CREATE 0x00000001
typedef struct tag_kms_out {
    char key[128];
    char key_location[256];
    char iv[32];
    int32_t iv_len;
}kms_out_t;

typedef struct kms_info_t
{
    uint8_t type;
    unsigned char kmsaddr[64];
    int kmsport;

    unsigned char kmsbkupaddr[64];
    int kmsbkupport;

    uint32_t flag;
    uint32_t iv_len;
} kms_info_t;

typedef struct auth_info_t
{
    uint8_t username[64];
    uint8_t pass[64];
    uint8_t uniqueid[128];
    uint8_t key[128];
    int32_t uniqueid_len;
    int32_t key_len;
} auth_info_t;

/*The below structure is used for async dns and has members
needed to kickstart waiting processes and other required stuff*/
#if 0
typedef struct auth_dns_t
{
    uint8_t domain[1024];
    uint8_t resolved;
    ip_addr_t ip;
    int32_t ttl;
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

} auth_dns_t;
#endif
typedef struct auth_msg_t
{
    int32_t magic;
    uint8_t errcode;
    uint8_t errstr[1024]; //Error string returned by third party;
    int64_t task_id;    //For holding the task ID of the caller
    uint8_t authtype; 
    void*   authdata;	//Based on AUTHTYPE, different structs could be deref

	uint32_t seq_num;

    /*Add further auth details here, like auth policies
     User needs to set additional data as private data, if 
     necessary for restarting task*/

} auth_msg_t;

void auth_mgr_input(nkn_task_id_t id);		//puts into taskqueue
void auth_mgr_output(nkn_task_id_t id);		//dummy
void auth_mgr_cleanup(nkn_task_id_t id);	//dummy
void *auth_mgr_input_handler_thread(void *arg);	//picks tasks from queue
int auth_mgr_init(uint32_t init_flags);		//initializes the worker threads

#endif /* _NKN_AUTHM_API_H */
