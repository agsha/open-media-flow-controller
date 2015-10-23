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
#include "nkn_authmgr.h"
#include "nkn_http.h"
#include "network.h"

#include "nkn_om.h"
#include "om_defs.h"
#include "om_fp_defs.h"

#define F_FILE_ID	LT_AUTH_HELPER

NKNCNT_DEF(dns_err_httpcon, uint64_t, "", "total dns task")
NKNCNT_EXTERN(dns_task_called, AO_t);
NKNCNT_EXTERN(dns_task_completed, AO_t);
NKNCNT_EXTERN(sched_task_create_fail_cnt, uint64_t);
NKNCNT_EXTERN(dns_too_many_parallel_lookup, int64_t);

extern int max_dns_pend_q;
auth_msg_t *authmsg_init(void);
void auth_helper_get_auth(con_t *con);
int auth_helper_get_dns(con_t* taskdata, uint32_t cp_sockfd, uint32_t incarn, char *p_ip, int origin_ip_family);
void auth_helper_cleanup(nkn_task_id_t id);
void auth_helper_input(nkn_task_id_t id);
void auth_helper_output(nkn_task_id_t id);
extern int am_fetch_local(auth_msg_t*);
extern int dns_hash_and_lookup(char *domain, int32_t family, uint8_t *resolved_ptr);

auth_msg_t *authmsg_init(void)
{
        auth_msg_t *am = nkn_calloc_type(1, sizeof(*am), mod_auth_msg_t);
	am->magic=AUTH_MGR_MAGIC;
        return am;
}

void auth_helper_get_auth(con_t *con)
{
	UNUSED_ARGUMENT(con);
	/*
        const char *authdata;
        int authdatalen;
        uint32_t authattr;
        int authheader_cnt;
        get_known_header(&con->http.hdr, MIME_HDR_AUTHORIZATION, &authdata, &authdatalen,
                        &authattr, &authheader_cnt);
        DBG_LOG(MSG, MOD_AUTHHLP,
                        "Authorization header found with:%s", authdata);

        int _taskid;
        auth_msg_t *am = authmsg_init();
	//strip out the Basic word from this so we get just
	//the base64 encoding and push it
	authdata+=6;
	authdatalen=strlen(authdata);
        strncpy((char*)am->uniqueid,authdata,authdatalen);

	am->con=con;//keep a track to kick-start after auth
        am->uniqueid_len=authdatalen;
        am->svrinfo.svrtype=LOCALFILE;
	if(am_fetch_local(am))
	{
        	_taskid=nkn_task_create_new_task(
                                        TASK_TYPE_AUTH_MANAGER,
                                        TASK_TYPE_AUTH_HELPER,
                                        TASK_ACTION_INPUT,
                                        0,
                                        am,
                                        sizeof(*am),
                                        0);//deadline

        	DBG_LOG(MSG, MOD_AUTHHLP,"create an auth manager task %d",_taskid);
        	nkn_task_set_state(_taskid, TASK_STATE_RUNNABLE);
	}
	else
	{
        	DBG_LOG(MSG, MOD_AUTHHLP,
              		"short-circuit to hash returned with \
			content as %s and outlen=%d",
                   am->key, am->key_len);
	        if(am->key_len>0)
        	        nkn_post_sched_a_task(am->con);//kick start http task
        	else
        	{
                	http_build_res_401(&(am->con->http), NKN_HTTP_AUTH_FAILED);//need to change this subcode
                	http_close_conn(am->con,NKN_HTTP_AUTH_FAILED);
        	}
        	free(am);
	}
        return;
	*/
}

int auth_helper_get_dns(con_t* taskdata, uint32_t cp_sockfd, uint32_t incarn, char *p_ip, int origin_ip_family)
{
	auth_msg_t *am;
	auth_dns_t *adns;
	nkn_task_id_t taskid;
	int ip_len;
	int dns_found;
	uint8_t resolved;
	int dns_pq;
    	ip_addr_t ret;
	int ret_val =0;

	ret_val = inet_pton(origin_ip_family, p_ip, &ret.addr);
        if (ret_val > 0) {         
            /*No need to post a task in this and below case*/
            return 2;
        }
        if ((origin_ip_family == AF_INET) && ret_val == 0) {
                ret_val=inet_addr(p_ip);
                if (ret_val == 0) {
                        /* address translation resulted in zero as ip*/
                        /*No need to post a task */
			return 2;
                }
        }



        /*Thought of doing this optimization too, but the cache
        might be at the verge of being deleted. So this request
        will hit the gethostbyname, since there will be a
        dns cache miss in dns_domain_to_ip(), so no point.
        We should store the dest_ip before opening the socket
        so we dont have to lookup hash again. Will implemenet it
        later*/
	dns_found = 0;

        /* TODO: need to retrieve family from taskdata */
        dns_found=dns_hash_and_lookup(p_ip, origin_ip_family, &resolved);
        if (dns_found && resolved)
        {
                return 0;
        }
        /********************************************************
        This just means that we have already done DNS lookup once
        and it has failed, either because of timeout or DNS
        server gave a junk response. So we added an entry into
        our local DNS table, but with ip as NULL
        *******************************************************/
        if (dns_found) {
                        DBG_LOG(ERROR, MOD_OM,
                                "dns_hash_and_lookup46: found entry in dns table, but its a failed attempt");
                        return -1;
        }
	adns = nkn_calloc_type(1, sizeof(auth_dns_t), mod_auth_uchar);
	if (!adns)
	{
		DBG_LOG(ERROR, MOD_AUTHHLP, "Failed in calloc for auth_dns");
		return -1;
	}
	am = nkn_calloc_type(1, sizeof(*am), mod_auth_msg_t);
        if (!am)
        {
                DBG_LOG(ERROR, MOD_AUTHHLP, "Failed in calloc for auth_msg");
		free(adns);
                return -1;
        }

        dns_pq=glob_dns_task_called-glob_dns_task_completed;
        if (dns_pq > max_dns_pend_q)
        {
		glob_dns_too_many_parallel_lookup++;
                free(adns);
                free(am);
                return -3;
        }

	am->magic=AUTH_MGR_MAGIC;
	am->authtype=DNSLOOKUP;
	ip_len=strlen(p_ip);
	memcpy(adns->domain,p_ip,ip_len);
	adns->domain_len = ip_len;
    	adns->ip[0].family = origin_ip_family;
	if (taskdata->http.nsconf->http_config->origin.u.http.server[0]->dns_query_len) {
		memcpy(adns->dns_query,
		    taskdata->http.nsconf->http_config->origin.u.http.server[0]->dns_query,
		    taskdata->http.nsconf->http_config->origin.u.http.server[0]->dns_query_len);
		adns->dns_query_len =
		    taskdata->http.nsconf->http_config->origin.u.http.server[0]->dns_query_len;
	}
	adns->caller=AUTH_DO_TUNNEL;
	adns->cp_sockfd=cp_sockfd;
	adns->cp_incarn=incarn;
	adns->con_incarn=gnm[taskdata->fd].incarn;
	adns->con_fd = taskdata->fd;
	am->authdata=adns;
	taskid = nkn_task_create_new_task(TASK_TYPE_AUTH_MANAGER, 
					  TASK_TYPE_AUTH_HELPER,
					  TASK_ACTION_INPUT,
					  0,am,sizeof(*am),0);
	if (taskid == -1) {
                free(adns);
                free(am);
                glob_sched_task_create_fail_cnt++;
                DBG_LOG(MSG, MOD_AUTHHLP, "task creation failed. domain:%s", p_ip);
                return -2;
	}
	DBG_LOG(MSG, MOD_AUTHHLP,
		"Created authmgr task=%ld to resolve domain:%s",taskid, p_ip);
	nkn_task_set_private(TASK_TYPE_AUTH_HELPER, taskid,
		(void *)taskdata);
	nkn_task_set_state(taskid, TASK_STATE_RUNNABLE);
	SET_CON_FLAG(taskdata, CONF_TUNNEL_PROGRESS);

	return 1;
}

void auth_helper_cleanup(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
        return ;
}

void auth_helper_input(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
        return ;
}

void auth_helper_output(nkn_task_id_t id)
{
        /*For adns, we implemented async DNS and hence
        AUTH Mgr will return a task after completing DNS Lookup.
        Kickstart the tunnel process here*/
	int ret = NKN_FP_DNS_ERR;
        struct nkn_task *ntask=NULL;
        ntask = nkn_task_get_task(id);
	struct network_mgr *pnm;

	auth_msg_t *am;
	am=(auth_msg_t *)nkn_task_get_data(id);
        if (!am)
	{
        	DBG_LOG(ERROR,MOD_AUTHHLP,
                        "No data from authmgr for taskid=%ld", id);
		nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP, TASK_STATE_RUNNABLE);
                return;
        }
	auth_dns_t *adns=(auth_dns_t*)(am->authdata);
	con_t* httpcon=nkn_task_get_private(TASK_TYPE_AUTH_HELPER, id);
	if (!httpcon || httpcon->magic != CON_MAGIC_USED)
	{
		if (!httpcon) {
			DBG_LOG(ERROR, MOD_AUTHHLP, "No private data from authmgr taskid=%ld", id);
		} else {
			glob_dns_err_httpcon++;
		}
		free(adns);
		free(am);
		nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP, TASK_STATE_RUNNABLE);
                return;
        }
	pnm = &gnm[adns->con_fd];
	pthread_mutex_lock(&pnm->mutex);
	NM_TRACE_LOCK(pnm, LT_AUTH_HELPER);
	if( !NM_CHECK_FLAG(pnm, NMF_IN_USE) ||
	    (httpcon->fd < 0 && httpcon->fd >= MAX_GNM) ||
	    (httpcon->fd != adns->con_fd) ||
	    (gnm[httpcon->fd].incarn != adns->con_incarn) ) {
		glob_dns_err_httpcon++;
		free(adns);
		free(am);
		nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP, TASK_STATE_RUNNABLE);
		NM_TRACE_UNLOCK(pnm, LT_AUTH_HELPER);
		pthread_mutex_unlock(&pnm->mutex);
		return;
	}

	DBG_LOG(MSG, MOD_AUTHHLP,
		"scheduler returns a task from AUTH MGR %ld", id)
	SET_CON_FLAG((con_t*)(httpcon),CONF_DNS_DONE);

	resume_NM_socket(httpcon->fd);

        if( (adns->resolved == 0) /* DNS lookup failure case */  ||
	    ((ret=fp_make_tunnel(httpcon,adns->cp_sockfd,adns->cp_incarn, 0) ) != 0) )
	{
         	DBG_LOG(ERROR, MOD_AUTHHLP, "failed to tunnel");
		CLEAR_HTTP_FLAG(&httpcon->http, HRF_CONNECTION_KEEP_ALIVE);
                http_build_res_503(&httpcon->http, ret);
                http_close_conn(httpcon, ret);
	}
	free(adns);
	free(am);
	nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP, TASK_STATE_RUNNABLE);
	NM_TRACE_UNLOCK(pnm, LT_AUTH_HELPER);
	pthread_mutex_unlock(&pnm->mutex);
	/*End adns changes*/

	/*TODO
	auth_msg_t *aptr;
	aptr=(auth_msg_t *)nkn_task_get_data(id);
        DBG_LOG(MSG, MOD_AUTHHLP,
              "scheduler returns a task from AUTH MGR %d with content as %s and outlen=%d",
        	  (int)id, aptr->key, aptr->key_len);
	if(aptr->key_len>0)
		nkn_post_sched_a_task(aptr->con);//kick start http task	
	else
	{
	        http_build_res_401(&(aptr->con->http), NKN_HTTP_AUTH_FAILED);//need to change this subcode
                http_close_conn(aptr->con,NKN_HTTP_AUTH_FAILED); 
	}
        free(aptr);
	*/
}


