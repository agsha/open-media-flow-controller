#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <libgen.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "nkn_debug.h"
#include "nkn_om.h"
#include "om_defs.h"
#include "nkn_defs.h"
#include "nkn_cfg_params.h"
#include "nkn_mediamgr_api.h"
#include "nkn_stat.h"
#include "nkn_errno.h"
#include "network.h"
#include "server.h"
#include "nkn_cod.h"
#include "atomic_ops.h"
#include "nkn_hash.h"
#include "nkn_authmgr.h"
#include "nkn_compressmgr.h"
#include "om_fp_defs.h"
#include "nkn_ssl.h"

#define F_FILE_ID	LT_OM_API

#include "om_port_mapper.h"

// #define SEND_UOL_URI 1

#define MAX_OM_PENDING_REQUESTS		(64 * 1024)
#define INT_MAX_OM_PENDING_REQUESTS	(2 * MAX_OM_PENDING_REQUESTS)
#define MAX_PENDING_REQ_T_ENTRIES	(64 * 1024)

/*
 * MM_get_resp_t.provider_flags definitions
 */
#define GET_PROVIDER_ALLOC_ATTRBUF	0x1

NKNCNT_DEF(om_open_get_task, int32_t, "", "Total open OM get tasks")
NKNCNT_DEF(om_tot_stat_called, int32_t, "", "Total OM stat called")
NKNCNT_DEF(om_tot_get_called, int32_t, "", "Total OM get called")
NKNCNT_DEF(om_open_validate_task, int32_t, "", "Total open OM validate tasks")
NKNCNT_DEF(om_tot_validate_called, int32_t, "", "Total OM validate called")
NKNCNT_DEF(om_err_get_task, int32_t, "", "failed OM get task")
NKNCNT_DEF(om_err_stat_api, int32_t, "", "Wrong OM stat API")
NKNCNT_DEF(om_err_validate_api, int32_t, "", "Wrong OM validate API")
NKNCNT_DEF(om_err_ask_partial_data, int32_t, "", "Other provider returns half data")
NKNCNT_DEF(om_call_tfm, int64_t, "", "total OM calls TFM")
NKNCNT_DEF(om_not_call_tfm, int64_t, "", "total OM not calls TFM")
NKNCNT_DEF(om_tot_attrbufs_alloced, int64_t, "", "total attribute buffers allocated")
NKNCNT_DEF(dns_too_many_parallel_lookup, int64_t, "", "")
NKNCNT_DEF(om_unchunk_complete, int32_t, "", "Total object unchunks")
NKNCNT_DEF(om_unchunk_fails, int32_t, "", "Total object unchunk failures")
NKNCNT_DEF(om_err_validate_no_lm, int32_t, "", "Total validates without last-modifed data")
NKNCNT_DEF(om_err_validate_attr2hdr, int32_t, "", "Total validate attr2hdr failures")
NKNCNT_DEF(om_get_start_fails, int64_t, "", "Total OM_get start failuress")
NKNCNT_DEF(om_get_skip_adns, int64_t, "", "Total OM_get skip ADNS")
NKNCNT_DEF(om_validate_start_fails, int64_t, "", "Total OM_validate start failures")
NKNCNT_DEF(om_num_eod_on_close, int32_t, "", "Total object with eod on close")
NKNCNT_DEF(om_err_resume_task_unknown, int32_t, "", "Total OM unknown task")
NKNCNT_DEF(om_err_resume_task_nop, int32_t, "", "Total OM nop")
NKNCNT_DEF(om_err_dns_failover_and_lookup, int32_t, "", "Total OM error DNS failover and lookup")
NKNCNT_DEF(om_ocon_invalid_physid, int32_t, "", "Total OM error on invalid om con free")
NKNCNT_EXTERN(dns_task_called, AO_t);
NKNCNT_EXTERN(dns_task_completed, AO_t);
NKNCNT_EXTERN(sched_task_create_fail_cnt, uint64_t);

/* hash table to store cod + offset */
NCHashTable *om_con_by_uri;
/* hash table to store om fd + mm fd */
NCHashTable *om_con_by_fd;
extern nkn_mutex_t omcon_mutex;

static pthread_mutex_t network_pending_request_mutex = PTHREAD_MUTEX_INITIALIZER;
static int max_pending_requests = INT_MAX_OM_PENDING_REQUESTS;
static int pending_requests_low_mark = 
		INT_MAX_OM_PENDING_REQUESTS - (INT_MAX_OM_PENDING_REQUESTS/2);
static int disable_new_requests;
static int32_t om_post_auth_task(void *data, int32_t type);
extern int dns_hash_and_lookup(char *domain, int32_t family, 
                                 uint8_t *resolved_ptr);
extern dns_ip_addr_t dns_hash_and_lookup46(char *domain, int32_t family, int *found);
int ssl_shm_mem_conn_pool_init(void);

#define LOCK_CNT() pthread_mutex_lock(&network_pending_request_mutex)
#define UNLOCK_CNT() pthread_mutex_unlock(&network_pending_request_mutex)
#define PENDING_REQUESTS() (glob_om_open_get_task + glob_om_open_validate_task)
#define INC_CNT(_c) inc_count((_c))
#define DEC_CNT(_c) dec_count((_c))

typedef enum {
        DNS_TASK_POSTED = 1,
        DNS_TASK_ISIP = 2,
        DNS_TASK_ISCACHE = 3,
        DNS_TASK_FAILED = 4,
	DNS_QUEUE_FULL = 5,
} dns_ret;

typedef struct TFM_put_privatedata {
	int flags;
	nkn_buffer_t *attrbuf;
	int num_bufs;
	nkn_buffer_t *bufs[MAX_CM_IOVECS];
	int num_iovecs;
	nkn_iovec_t content[MAX_CM_IOVECS];
} TFM_put_privatedata_t;

#define TFM_FLAGS_ALLOC_ATTRBUF	0x1

int32_t om_post_compression_task(omcon_t *data, TFM_put_privatedata_t *tfm_priv);
extern void *oomgr_req_func(void *arg);
char *ssl_shm_mem = NULL;
extern int max_dns_pend_q;

int OM_get_ret_task(omcon_t * ocon);
int OM_get_ret_error_to_task(omcon_t * ocon);
void free_tfm_privatedata(TFM_put_privatedata_t *tfm_data);
void om_mgr_input(nkn_task_id_t id);
void om_mgr_output(nkn_task_id_t id);
void om_mgr_cleanup(nkn_task_id_t id);

static void unchunk_and_cache(omcon_t *ocon);
static void compress_and_cache(omcon_t *ocon);
static void *alloc_buf(off_t *bufsize);
static char *buf2data(void *token);
static int free_buf(void *token);
static int build_unchunked_object_attrs(omcon_t *ocon, 
					TFM_put_privatedata_t *tfm_priv, 
					nkn_buffer_t *attrbuf,
					mime_header_t *trailer_hdrs);
#if 0
static int push_to_tfm(omcon_t *ocon, TFM_put_privatedata_t *tfm_priv, 
		       off_t off, off_t length, off_t total_length);
#endif

static void inc_count(int32_t *pcnt) 
{
    int prt_logmsg = 0;
    LOCK_CNT();
    if (!disable_new_requests && 
	(PENDING_REQUESTS() > max_pending_requests)) {
	    // Disable new requests but allow continuation requests
	    MM_stop_provider_queue(OriginMgr_provider, 0);
	    disable_new_requests = 1;
    	    prt_logmsg = 1;
    }
    (*pcnt)++;
    UNLOCK_CNT();

    if (prt_logmsg) {
    	DBG_LOG(WARNING, MOD_OM, "Disabled new request input queue");
    }
}

static void dec_count(int32_t *pcnt)
{
    int prt_logmsg = 0;

    LOCK_CNT();
    (*pcnt)--;
    if (disable_new_requests &&
	(PENDING_REQUESTS() < pending_requests_low_mark)) {
	    // Allow receipt of new requests
	    MM_run_provider_queue(OriginMgr_provider, 0);
	    disable_new_requests = 0;
    	    prt_logmsg = 1;
    }
    UNLOCK_CNT();

    if (prt_logmsg) {
    	DBG_LOG(WARNING, MOD_OM, "Enabled new request input queue");
    }
}

static void alloc_attrbuf(MM_get_resp_t * p_mm_task)
{
	p_mm_task->in_attr = nkn_buffer_alloc(NKN_BUFFER_ATTR, 0, 0);
	if (p_mm_task->in_attr) {
		p_mm_task->provider_flags |= GET_PROVIDER_ALLOC_ATTRBUF;
		glob_om_tot_attrbufs_alloced++;
	}
}

static void free_attrbuf(MM_get_resp_t * p_mm_task)
{
	if (p_mm_task->provider_flags & GET_PROVIDER_ALLOC_ATTRBUF) {
		p_mm_task->provider_flags &= ~GET_PROVIDER_ALLOC_ATTRBUF;
		nkn_buffer_release(p_mm_task->in_attr);
		p_mm_task->in_attr = 0;
		glob_om_tot_attrbufs_alloced--;
	}
}

void om_resume_task(omcon_t * ocon)
{
    	if (ocon && OM_CHECK_FLAG(ocon, OMF_WAIT_COND)) {
	    	// Resume task

	    	if (OM_CHECK_FLAG(ocon, OMF_DO_GET)) {
	    		nkn_task_set_action_and_state(ocon->p_mm_task->get_task_id, 
		    		TASK_ACTION_OUTPUT,
				TASK_STATE_RUNNABLE);
		    	DEC_CNT(&glob_om_open_get_task);
			ocon->p_mm_task = NULL;

	    	} else if (OM_CHECK_FLAG(ocon, OMF_DO_VALIDATE)) {
	    		nkn_task_set_action_and_state(ocon->p_validate->get_task_id, 
		    		TASK_ACTION_OUTPUT,
				TASK_STATE_RUNNABLE);
		    	DEC_CNT(&glob_om_open_validate_task);
			ocon->p_validate = NULL;
	    	} else {
		    	DBG_LOG(MSG, MOD_OM, 
			    "Unknown request type, ocon.flag=0x%lx",
			    ocon->flag);
			glob_om_err_resume_task_unknown++;
	    	}
	    	OM_UNSET_FLAG(ocon, OMF_WAIT_COND);
    	}
	else {
		glob_om_err_resume_task_nop++;
		DBG_LOG(MSG, MOD_OM, "Nop call ocon=%p", ocon);
		/* Don't do assert here. It could be right when server closes the connection */
		//assert(0);
	}
}

#if 0
static int OM_put(struct MM_put_data * pdata, uint32_t arg)
{
	UNUSED_ARGUMENT(pdata);
	UNUSED_ARGUMENT(arg);
	return -1;
}
#endif

/*
 * Since we don't know the size of URI content, we always return a very
 * big number (OM_STAT_SIG_TOT_CONTENT_LEN) for now.
 * return 0: success
 * return -1: failed
 */

static int OM_stat(nkn_uol_t uol, MM_stat_resp_t *in_ptr_resp)
{
	omcon_t * ocon=NULL;
	con_t * httpcon;
	int rv;
	struct network_mgr * pnm;
	omcon_t * stale_ocon = NULL;
	int uri_hit;
	off_t offset_delta = 0;
	int ret_val = 0;
	int ocon_fd;
	int lock_fd = -1;
	const namespace_config_t *ns_cfg;


	if (!VALID_NS_TOKEN(in_ptr_resp->ns_token)) {
		// No namespace associated with request
		return NKN_OM_NO_NAMESPACE;
	}

	httpcon=(con_t *)in_ptr_resp->in_proto_data;
	if (httpcon == NULL) {
		DBG_LOG(MSG, MOD_OM, "OM is called but data is not provided");
		glob_om_err_stat_api++;
		return NKN_OM_INTERNAL_ERR;
	}

	/*
	 * The design logic to reuse/create physid is:
	 * 1. find the same socket match. HTTP module fd with in-line OM module fd.
	 * 2. find the same uri/offset from queue.
	 * 3. create a new physid.
	 */
	ocon=om_lookup_con_by_fd(httpcon->fd, &uol, 1 /* ret_locked */, FALSE, &lock_fd);
	if(ocon==NULL) {
		/*
		 * server has sent us all responses and closed the connection.
		 * We mark end of transaction.
		 * Content length is unknown for this case.
		 */
		if (CHECK_CON_FLAG(httpcon, CONF_NO_CONTENT_LENGTH)) {
			return NKN_OM_ENDOF_TRANS;
		}

		ocon = om_lookup_con_by_uri(&uol,
			1/*ret lock*/, 0/*passin lock*/,
			&uri_hit, &offset_delta, &ret_val, &stale_ocon, &lock_fd);
		if (ocon) {
			rv = 0;
			in_ptr_resp->physid_len =
				snprintf(&in_ptr_resp->physid[0], NKN_PHYSID_MAXLEN,
					"OM_%lx_%lx", uol.cod, uol.offset);
			in_ptr_resp->physid_len = MIN(in_ptr_resp->physid_len, NKN_PHYSID_MAXLEN - 1);
			ocon_fd = lock_fd;
			lock_fd = -1;
			pnm = &gnm[ocon_fd];
			pthread_mutex_unlock(&pnm->mutex);
			NM_TRACE_UNLOCK(pnm, LT_OM_NETWORK);
		}
		else {
			rv = 1;
			in_ptr_resp->physid_len =
				snprintf(&in_ptr_resp->physid[0], NKN_PHYSID_MAXLEN,
					"OM_%lx_%lx", uol.cod, uol.offset);
			in_ptr_resp->physid_len = MIN(in_ptr_resp->physid_len, NKN_PHYSID_MAXLEN - 1);
		}

    		DBG_LOG(MSG, MOD_OM, "Physid: %s, %s uri=%s offset=%ld len=%ld",
			in_ptr_resp->physid,
			rv == 0 ? "Active" : "New",
			nkn_cod_get_cnp(uol.cod), uol.offset, uol.length);
	}
	else {
		pnm = &gnm[lock_fd];
		/* This must be continuous task of existing socket */
		if(OM_CHECK_FLAG(ocon, OMF_ENDOF_TRANS)) {
			// Transaction is completed.
			glob_om_err_stat_api++;
			NM_TRACE_UNLOCK(pnm, LT_OM_API);
        		pthread_mutex_unlock(&pnm->mutex);
			return NKN_OM_ENDOF_TRANS;
		}

		/* We have to reuse the same physid */
		in_ptr_resp->physid_len =
			snprintf(&in_ptr_resp->physid[0], NKN_PHYSID_MAXLEN,
				"OM_%lx_%lx", uol.cod, uol.offset);
		in_ptr_resp->physid_len = MIN(in_ptr_resp->physid_len, NKN_PHYSID_MAXLEN - 1);
    		DBG_LOG(MSG, MOD_OM, 
			"Reuse physid: %s uri=%s offset=%ld len=%ld",
			in_ptr_resp->physid,
			nkn_cod_get_cnp(uol.cod), uol.offset, uol.length);
		NM_TRACE_UNLOCK(pnm, LT_OM_API);
        	pthread_mutex_unlock(&pnm->mutex);
	}

	
	ns_cfg = namespace_to_config(in_ptr_resp->ns_token);
	if (ns_cfg && ns_cfg->http_config &&
		ns_cfg->http_config->policies.bulk_fetch) {  
        	in_ptr_resp->media_blk_len =
			ns_cfg->http_config->policies.bulk_fetch_pages
			* CM_MEM_PAGE_SIZE;
	} else { 
		if (!IS_PSEUDO_FD(httpcon->fd)) {
			pnm = &gnm[httpcon->fd];
			pthread_mutex_lock(&pnm->mutex);
			NM_TRACE_LOCK(pnm, LT_OM_API);
			if (NM_CHECK_FLAG(pnm, NMF_IN_USE)) {
				if(CHECK_CON_FLAG(httpcon, CONF_FIRST_TASK)) {
					UNSET_CON_FLAG(httpcon, CONF_FIRST_TASK);
        				in_ptr_resp->media_blk_len = CM_MEM_PAGE_SIZE;
				}
				else {
				/*
				 * BUG 8763: According to Cache Hit Ratio analysis,
				 * For T-Proxy case, we can use 32K always for blk size.
				 * So we will not waste memory when client goes away during download.
				 *
	        		 * in_ptr_resp->media_blk_len = 4 * CM_MEM_PAGE_SIZE;
				 */
				/*
				 * PR: 796149: Due to the new kernel change in 12.1, 1.2 - 1.5 gbps
				 * through put only met in cache-miss path in t-proxy for single page
				 * per task. 2 pagess per task has given around 5.5 gbs.so pages per
				 * task is changed to 2 from 1 for t-proxy. Since bug 8763 is fixed
				 * based on the field input, it is better to have a configuration
				 * option. So that if there is an issue in the field, it can be changed.
				 * Configuration can be changed only through the configuration file.
				 * By default om_tproxy_buf_pages_per_task value is 2.
				 */
					if (CHECK_HTTP_FLAG(&httpcon->http, HRF_TPROXY_MODE)) {
						in_ptr_resp->media_blk_len = 
									om_tproxy_buf_pages_per_task
									* CM_MEM_PAGE_SIZE;
					} else {
						in_ptr_resp->media_blk_len = 4 * CM_MEM_PAGE_SIZE;
					}
				}
			}
			NM_TRACE_UNLOCK(pnm, LT_OM_API);
			pthread_mutex_unlock(&pnm->mutex);
		} else {
			// Caller is Move Manager
			if(CHECK_CON_FLAG(httpcon, CONF_FIRST_TASK)) {
				UNSET_CON_FLAG(httpcon, CONF_FIRST_TASK);
        			in_ptr_resp->media_blk_len = CM_MEM_PAGE_SIZE;
			}
			else {
			/*
			 * BUG 8763: According to Cache Hit Ratio analysis,
			 * For T-Proxy case, we can use 32K always for blk size.
			 * So we will not waste memory when client goes away during download.
			 *
        		 * in_ptr_resp->media_blk_len = 4 * CM_MEM_PAGE_SIZE;
			 */
				in_ptr_resp->media_blk_len = 4 * CM_MEM_PAGE_SIZE;
			}
		}
	}
	release_namespace_token_t(in_ptr_resp->ns_token);

        in_ptr_resp->content_len = OM_STAT_SIG_TOT_CONTENT_LEN;
        in_ptr_resp->tot_content_len = OM_STAT_SIG_TOT_CONTENT_LEN;

	glob_om_tot_stat_called++;

	return 0;
}

static int OM_get (MM_get_resp_t * p_mm_task)
{
	omcon_t * ocon;
	int ret, ret_val=-1;

	con_t *httpcon;
	ip_addr_t originipv4v6 ;
	uint16_t originport = 0; // Network byte order
	int lock_fd = -1;
	memset(&originipv4v6, 0, sizeof(ip_addr_t));

	assert( p_mm_task != NULL );
	if ((p_mm_task->in_flags & MM_FLAG_NEW_REQUEST) && !p_mm_task->in_attr){
		alloc_attrbuf(p_mm_task);
	}

	DBG_LOG(MSG, MOD_OM, "Id=%ld Physid: %s uri=%s offset=%ld len=%ld",
		p_mm_task->get_task_id,
		p_mm_task->physid, nkn_cod_get_cnp(p_mm_task->in_uol.cod),
		p_mm_task->in_uol.offset, p_mm_task->in_uol.length);

	/*
	 * Avoid the DNS lookup if this is a continuation request 
	 * (reading body data) where an existing origin connnection exists.
	 * This is essential for an origin RR cluster since the lookup
	 * indirectly moves the next origin pointer resulting in 
	 * inaccurate load distribution.
	 */
	httpcon = (con_t *)p_mm_task->in_proto_data;
	if (httpcon) {
		ocon = om_lookup_con_by_fd(httpcon->fd, &p_mm_task->in_uol, 
					   1 /* ret_locked */, FALSE, &lock_fd);
		if (ocon) {
			cpcon_t *cp = (cpcon_t *)ocon->cpcon;
			//validate physid before using origin port and origin ip
			//if not valid then lookup dns
			if (cp && (ocon->physid[0] != '\0')) {
				memcpy(&originipv4v6, 
					&cp->remote_ipv4v6, sizeof(ip_addr_t));
				originport = cp->remote_port;
				p_mm_task->in_flags |= MM_FLAG_DNS_DONE;
				glob_om_get_skip_adns++;
			} else 
				glob_om_ocon_invalid_physid++;
			pthread_mutex_unlock(&gnm[lock_fd].mutex);
		}
        // bz 10558: Lookup by fd could fail in cache-fill aggressive mode or multiple client req to same object case.
        // However, URI lookup for omcon should succeed.
        else {
            int uri_hit = 0;
            off_t offset_delta = 0;
            omcon_t *stale_ocon = NULL;
            int rv = 0;
            struct network_mgr *pnm = NULL;

            DBG_LOG(MSG, MOD_OM, "om_lookup_con_by_fd failed for uri=%s", nkn_cod_get_cnp(p_mm_task->in_uol.cod));

            ocon = om_lookup_con_by_uri (&p_mm_task->in_uol, 1 /* ret_locked */, 0/*passin lock*/,
                    &uri_hit, &offset_delta, &rv, &stale_ocon, &lock_fd);

            if (ocon) {
		cpcon_t *cp = (cpcon_t *)ocon->cpcon;
		//validate physid before using origin port and origin ip
		//if not valid then lookup dns
                if (cp && (ocon->physid[0] != '\0')) {
                    originport = 
                        cp->remote_port;
                    memcpy(&originipv4v6, 
                            &cp->remote_ipv4v6, sizeof(ip_addr_t));
                    p_mm_task->in_flags |= MM_FLAG_DNS_DONE;
                    glob_om_get_skip_adns++;
                } else
		    glob_om_ocon_invalid_physid++; 
                pnm = &gnm[lock_fd];
                pthread_mutex_unlock(&pnm->mutex);
                NM_TRACE_UNLOCK(pnm, LT_OM_API);
                DBG_LOG(MSG, MOD_OM, "om_lookup_con_by_uri success for uri=%s", nkn_cod_get_cnp(p_mm_task->in_uol.cod));
            }
        }
	}

        /*Changes for adns*/
        if (adns_enabled && !(p_mm_task->in_flags & MM_FLAG_DNS_DONE))
        {
                ret=om_post_auth_task((void*)p_mm_task, AUTH_DO_OM_GET);
                if (ret==DNS_TASK_POSTED)
                {
                        /*post auth task actually posted a task, if it returned
                        1, it would mean there was no need and we should just
                        continue with the regular path*/
                        return 0;
                }
                else if ((ret == DNS_TASK_FAILED) || (ret == DNS_QUEUE_FULL))
                {
                        //Something went wrong, we should just return back
			glob_om_get_start_fails++;
			free_attrbuf(p_mm_task);
			if (http_cfg_fo_use_dns &&
			   (httpcon->os_failover.num_ip_index)) {
				p_mm_task->err_code = NKN_OM_ORIGIN_FAILOVER_ERROR;
				if (p_mm_task->in_flags & MM_FLAG_TRACE_REQUEST) {
					DBG_TRACE( "DNS_MULTI_IP_FO: OM_get, Origin failover error."
						   " httpcon sockfd:%d, ocon fd=%d",
						   (httpcon ? httpcon->fd:0),
						   (ocon ? ocon->fd: 0));
				}
			} else {
				p_mm_task->err_code = NKN_OM_GET_START_ERR;
				if (p_mm_task->in_flags & MM_FLAG_TRACE_REQUEST) {
					DBG_TRACE( "DNS_MULTI_IP_FO: OM_get, Start error."
						   " httpcon sockfd:%d, ocon fd=%d",
						   (httpcon ? httpcon->fd:0),
						   (ocon ? ocon->fd: 0));
				}
			}

			nkn_task_set_action_and_state(p_mm_task->get_task_id,
					    TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);
                        return -1;
                }
        }
        /* End adns changes */

        INC_CNT(&glob_om_open_get_task);
        glob_om_tot_get_called++;

        if ((ret = OM_do_socket_work(p_mm_task, &ocon, 
				     &originipv4v6, originport))) {
		glob_om_get_start_fails++;
		DBG_LOG(MSG, MOD_OM, "Socket setup error, rv=%d", ret);
		DEC_CNT(&glob_om_open_get_task);
		free_attrbuf(p_mm_task);

		// Resume task
		if (!p_mm_task->err_code) {
			p_mm_task->err_code = NKN_OM_GET_START_ERR;
		}
		if(p_mm_task->err_code == NKN_MM_CONF_RETRY_REQUEST ||
		    p_mm_task->err_code == NKN_MM_UNCOND_RETRY_REQUEST)
		    ret_val = p_mm_task->err_code;
		nkn_task_set_action_and_state(p_mm_task->get_task_id, TASK_ACTION_OUTPUT,
						TASK_STATE_RUNNABLE);
		return ret_val;
	}
	return 0;
}

#if 0
static int OM_delete (MM_delete_resp_t *delete)
{
	UNUSED_ARGUMENT(delete);
	assert(0);
	return 0;
}
#endif

static int OM_shutdown (void)
{
	assert(0);
	return 0;
}

static int OM_discover (struct mm_provider_priv * pprovider)
{
	UNUSED_ARGUMENT(pprovider);

	assert(0);
	return 0;
}

static int OM_evict (void)
{
	return 0;
}

static int OM_validate (MM_validate_t *pvalidate)
{
	omcon_t * ocon;
	int rv = 0;
	int ret;
	mime_header_t mh;

	// pvalidate.in_ptype = OriginMgr_provider
	// pvalidate.in_sptype = 0
	// pvalidate.in_uol.cod = associated COD
	// pvalidate.in_attr = attribute data
	// pvalidate.ns_token = namespace token
	// pvalidate.ret_code = status of request

	init_http_header(&mh, 0, 0);

	if (!pvalidate) {
		DBG_LOG(MSG, MOD_OM, "Invalid pvalidate=%p arg", pvalidate);
		glob_om_err_validate_api++;
		rv = -1;
		goto error_exit;
	}

	if (!pvalidate->in_attr) {
		DBG_LOG(MSG, MOD_OM,
			"Invalid pvalidate->in_attr=%p arg",
			pvalidate->in_attr);
		glob_om_err_validate_api++;
		rv = -2;
		goto error_exit;
	}

	if (!VALID_NS_TOKEN(pvalidate->ns_token)) {
		DBG_LOG(MSG, MOD_OM,
			"Invalid pvalidate->ns_token");
		glob_om_err_validate_api++;
		rv = -3;
		goto error_exit;
	}

        /*Changes for adns*/
        if (adns_enabled && !(pvalidate->in_flags & MM_FLAG_DNS_DONE))
        {
                ret=om_post_auth_task((void*)pvalidate, AUTH_DO_OM_VALIDATE);
                if (ret==DNS_TASK_POSTED)
                {
                        /*
			 * post auth task actually posted a task, if it returned
                         * 1, it would mean there was no need and we should just
                         * continue with the regular path
			 */
			shutdown_http_header(&mh);
                        return 0;
                }
                else if ((ret == DNS_TASK_FAILED) || (ret == DNS_QUEUE_FULL))
                {
                        // Something went wrong, we should just return back
			rv = -7;
			goto error_exit;
                }
		// else does nothing and continue to normal code path.
        }
        /* End adns changes */

	glob_om_tot_validate_called++;

	rv = nkn_attr2http_header(pvalidate->in_attr, 0, &mh);
	if (rv) {
		DBG_LOG(MSG, MOD_OM,
			"nkn_attr2http_header() failed, rv=%d", rv);
		glob_om_err_validate_attr2hdr++;
		rv = -4;
		goto error_exit;
	}

	if (!is_known_header_present(&mh, MIME_HDR_LAST_MODIFIED)) {
		const namespace_config_t *ns_cfg;
		ns_cfg = namespace_to_config(pvalidate->ns_token);
		if (!((ns_cfg && ns_cfg->http_config 
			&& (NKN_TRUE == ns_cfg->http_config->policies.validate_with_date_hdr))
			&& (is_known_header_present(&mh, MIME_HDR_DATE)))) {
			/* If ETag header is avaiable, use it to validate
			 * BZ 3716
			 */
			if ( !is_known_header_present(&mh, MIME_HDR_ETAG) ) {
				DBG_LOG(MSG, MOD_OM, 
					"HTTP Last-Modified attribute not present");
				glob_om_err_validate_no_lm++;
				rv = -5;
				if (ns_cfg) {
				    	release_namespace_token_t(
						pvalidate->ns_token);
				}
				goto error_exit;
			}
		}
		if (ns_cfg) {
			release_namespace_token_t(pvalidate->ns_token);
		}
	}

	INC_CNT(&glob_om_open_validate_task);
        if ((ret = OM_do_socket_validate_work(pvalidate, &mh, &ocon))) {
		glob_om_validate_start_fails++;
		DBG_LOG(MSG, MOD_OM, "Socket setup error, ocon=%p ret=%d", ocon, ret);
		DEC_CNT(&glob_om_open_validate_task);
		rv = -6;
		goto error_exit;
	}
	shutdown_http_header(&mh);
	return rv;

error_exit:

	// Resume task
	if (!pvalidate->ret_code) {
		pvalidate->ret_code = MM_VALIDATE_INVALID;
	}
	nkn_task_set_action_and_state(pvalidate->get_task_id, TASK_ACTION_OUTPUT,
					TASK_STATE_RUNNABLE);
	shutdown_http_header(&mh);

	return rv;
}



int ssl_shm_mem_conn_pool_init(void) 
{
	int shmid = 0;
	char *shm = NULL;
	uint64_t shm_size = 0;
	key_t shmkey;
	const char *producer = "/opt/nkn/sbin/nvsd";
	int max_cnt_space = MAX_SHM_SSL_ID_SIZE;

	if((shmkey = ftok(producer, NKN_SHMKEY)) < 0) {
		DBG_LOG(MSG, MOD_OM, "ftok failed, shared memory between ssld and nvsd could not be established");
		return 0;
	}

	shm_size = max_cnt_space * sizeof(char);
	shmid = shmget(shmkey, shm_size, IPC_CREAT | 0666);
	if(shmid < 0) {
		DBG_LOG(MSG, MOD_OM, "shmget error for ssld shatred memory");
		return 0;
	}
	
	shm = shmat(shmid, NULL, 0);
	if(shm == (char *)-1) {
		DBG_LOG(MSG, MOD_OM, "shmat error for ssld shared memory");
		return 0;
	}

	ssl_shm_mem = shm;
	return 1;
}

pmapper_t	pmap_ctxt;

/*
 * initialization
 */
int OM_init(void)
{
	int rv;
	pthread_attr_t attr;
	int stacksize = 128 * KiBYTES;

	// use _off functions since __urientry offset is 0
	om_con_by_uri = nchash_table_new(n_dtois_hash_arg, n_dtois_equal,
					512 * 1024,
					NKN_CHAINFIELD_OFFSET(omcon_t, __urientry),
					mod_om_con_by_uri_t);
	// Don't use _off functions __fdentry offset is non-zero
	om_con_by_fd = nchash_table_new(n_uint64_hash_arg, n_uint64_equal,
				        512 * 1024,
					NKN_CHAINFIELD_OFFSET(omcon_t, __fdentry),
					mod_om_con_by_fd_t);

	NKN_MUTEX_INIT(&omcon_mutex, NULL, "omcon-mutex");



	pmap_ctxt = om_pmapper_new();
	//om_pmapper_set_port_base(pmap_ctxt, 20000);
	om_pmapper_set_port_base(pmap_ctxt, u16_g_port_mapper_min);
	//om_pmapper_set_port_count(pmap_ctxt, 32768);
	om_pmapper_set_port_count(pmap_ctxt, (u16_g_port_mapper_max - u16_g_port_mapper_min));

	om_pmapper_init(pmap_ctxt);


	rv = pthread_attr_init(&attr);
	if (rv) {
		DBG_LOG(MSG, MOD_OM, "pthread_attr_init() failed, rv=%d", rv);
		return 0;
	}

	rv = pthread_attr_setstacksize(&attr, stacksize);
	if (rv) {
		DBG_LOG(MSG, MOD_OM,
			"pthread_attr_setstacksize() failed, rv=%d", rv);
		return 0;
	}
	MIME_init();
	MM_register_provider(OriginMgr_provider,
				0,
				NULL,
				OM_stat,
				OM_get,
				NULL,
				OM_shutdown,
				OM_discover,
				OM_evict,
				NULL,
				OM_validate,
				NULL,
				NULL,
				20 * 2 / 10, /* # of PUT threads */
				20 * 8 / 10, /* # of GET threads */
                		MAX_OM_PENDING_REQUESTS,
				0,
                		MM_THREAD_ACTION_IMMEDIATE);

	return ssl_shm_mem_conn_pool_init();

}

/*
 * Under certain negative conditions such as
 * OM cannot fetch the data from server, we should close the OM_get task
 * with an error.
 *
 * Only one of two functions OM_get_ret_error_to_task() and OM_get_ret_task()
 * should be called when OM_get task is returned.
 */
int OM_get_ret_error_to_task(omcon_t * ocon)
{
	glob_om_err_get_task++;
	OM_UNSET_FLAG(ocon, OMF_BUFFER_SWITCHED);
#if 1
	if (ocon->p_mm_task && OM_CHECK_FLAG(ocon, OMF_DO_GET)) {
		free_attrbuf(ocon->p_mm_task);
		om_resume_task(ocon);
	}
        return TRUE;
#else
	/*
	 * Task should be completed with error.
	 */
	if (ocon->p_mm_task && OM_CHECK_FLAG(ocon, OMF_DO_GET)) {
		free_attrbuf(ocon->p_mm_task);
		ocon->p_mm_task->err_code = errcode;
		om_resume_task(ocon);
	} else if (ocon->p_validate && OM_CHECK_FLAG(ocon, OMF_DO_VALIDATE)) {
		//ocon->p_validate->ret_code = MM_VALIDATE_ERROR;
		om_resume_task(ocon);
	} else {
		DBG_LOG(MSG, MOD_OM, "No task pending, ignore request fd=%d "
			"p_mm_task=%p p_validate=%p",
			OM_CON_FD(ocon), ocon->p_mm_task, ocon->p_validate);
	}
        return TRUE;
#endif
}
static int update_compress_end(omcon_t *ocon)
{
        nkn_buffer_t *attrbuf = NULL;
        nkn_buffer_t *bm_attrbuf = NULL;
        nkn_attr_t *pnknattr = NULL;
        mime_header_t hdrs;
	compress_msg_t *cp = (compress_msg_t *)ocon->comp_task_ptr;
        if(OM_CHECK_FLAG(ocon, OMF_COMPRESS_COMPLETE) && ocon->comp_offset == 0) {
                        char content_length_str[64];
			char content_encoding_str[64];
			char original_content_len_str[64];
                        int rv = 0;
                        ocon->content_length = cp->strm->total_out;
                        /* Add Content-Length header */
                        rv = snprintf(content_length_str, sizeof(content_length_str), "%ld",
                                  ocon->content_length);
                        content_length_str[rv] = '\0';
                        delete_known_header(&ocon->phttp->hdr,
                                    MIME_HDR_CONTENT_LENGTH);
                        rv = add_known_header(&ocon->phttp->hdr,
                                    MIME_HDR_CONTENT_LENGTH, content_length_str,
                                    strlen(content_length_str));
			if(ocon->comp_type == HRF_ENCODING_GZIP) {
                            rv = snprintf(content_encoding_str, sizeof(content_encoding_str), "%s",
	                              "gzip");
			} else if(ocon->comp_type == HRF_ENCODING_DEFLATE) {
                            rv = snprintf(content_encoding_str, sizeof(content_encoding_str), "%s",
	                              "deflate");
			}

                        rv = add_known_header(&ocon->phttp->hdr,
                                    MIME_HDR_CONTENT_ENCODING, content_encoding_str,
                                    strlen(content_encoding_str));
                        rv = snprintf(original_content_len_str, sizeof(original_content_len_str), "%ld",
                                  ocon->phttp->content_length);
                        original_content_len_str[rv] = '\0';
                        add_known_header(&ocon->phttp->hdr,
                                    MIME_HDR_X_NKN_UNCOMPRESSED_LENGTH, original_content_len_str,
                                    rv);
                    	if(ocon->p_mm_task && ocon->p_mm_task->in_attr) {
                        	attrbuf = ocon->p_mm_task->in_attr;
	                    } else {
        	                bm_attrbuf = nkn_buffer_get(&ocon->uol,
                	                                NKN_BUFFER_ATTR);
                        	attrbuf = bm_attrbuf;
                   	 }
	                if(attrbuf) {
                	        pnknattr = (nkn_attr_t *) nkn_buffer_getcontent(attrbuf);
        	                //pnknattr->content_length = ocon->content_length;
				pnknattr->encoding_type = ocon->comp_type; 
                        	init_http_header(&hdrs, 0, 0);
				if(ocon->comp_type == HRF_ENCODING_GZIP) {
	        	                pnknattr->na_flags |= NKN_OBJ_COMPRESSED;
				} else if(ocon->comp_type == HRF_ENCODING_DEFLATE) {
	        	                pnknattr->na_flags |= NKN_OBJ_COMPRESSED;
				}
                	        rv = nkn_attr2http_header(pnknattr, 0, &hdrs);
                        	if (!rv) {
	                            update_known_header(&hdrs, MIME_HDR_CONTENT_LENGTH,
        	                            content_length_str, strlen(content_length_str),
                	                    1 /* Replace */);
				    add_known_header(&hdrs, MIME_HDR_CONTENT_ENCODING,
					    content_encoding_str, strlen(content_encoding_str));
				    add_known_header(&hdrs, MIME_HDR_X_NKN_UNCOMPRESSED_LENGTH, 
					    original_content_len_str, strlen(original_content_len_str));
	                            mime_header2nkn_buf_attr(&hdrs, 0, attrbuf,
        	                                      NULL,
                	                            nkn_buffer_getcontent);
                        	}
	                        shutdown_http_header(&hdrs);
                	}
		        
	                if (bm_attrbuf) {
            	            nkn_buffer_release(bm_attrbuf);
		        }

		cp_add_event(ocon->fd, OP_CLOSE_CALLBACK, NULL);
		NS_INCR_STATS(ocon->httpreqcon->http.nsconf, PROTO_HTTP, server, compress_cnt);
		if(ocon->phttp->content_length > ocon->content_length) {
			NS_UPDATE_STATS((ocon->httpreqcon->http.nsconf), PROTO_HTTP, 
				server, compress_bytes_save, ocon->phttp->content_length - ocon->content_length);
		} else {
			NS_UPDATE_STATS((ocon->httpreqcon->http.nsconf), PROTO_HTTP, 
				server, compress_bytes_overrun, ocon->content_length - ocon->phttp->content_length);
		}

    }
    return 0;
}
/*
 * OM fetch the data from server successfully, we close OM_get task.
 */
int OM_get_ret_task(omcon_t * ocon)
{
	MM_get_resp_t * p_mm_task = ocon->p_mm_task;
	int i;
	nkn_uol_t uol;
	uint64_t keyhash1, keyhash2;
	dtois_t d;
	int len;
	void *ptr_vobj;
	off_t content_offset;
	off_t start_offset = 0;
	off_t offset_len;
	//off_t align_offset;
	TFM_put_privatedata_t *tfm_data = 0;
	MM_put_data_t *MM_put_data;
	nkn_task_id_t taskid;
	nkn_buffer_t *attrbuf = NULL;
	nkn_buffer_t *bm_attrbuf = NULL;
	nkn_attr_t *pnknattr = NULL;
	char content_length_str[64];
	int rv;
	mime_header_t hdrs;
	omcon_t *temp;

	assert( ocon!=NULL );


	if (!p_mm_task) {
		DBG_LOG(MSG, MOD_OM, "No task pending, ignore request fd=%d",
			OM_CON_FD(ocon));
		return TRUE;
	}

	if(OM_CHECK_FLAG(ocon, OMF_NEGATIVE_CACHING) && ocon->phttp->content_length == 0) {
	    ocon->p_mm_task->err_code = NKN_OM_SERVER_NO_DATA;	
	    // Setup the UOL
	    uol.cod = ocon->uol.cod;
	    uol.offset = 0;
	    uol.length = 0;


	    nkn_buffer_setid(ocon->p_mm_task->in_attr, &uol,
		OriginMgr_provider, 0);
	    goto task_return;
	}

	if (!OM_CHECK_FLAG(ocon, OMF_BUF_CPY_SKIP_OFFSET)) {
        	start_offset = (ocon->content_offset + ocon->content_start) % CM_MEM_PAGE_SIZE;
		p_mm_task->out_num_content_objects = 
                	(ocon->thistask_datalen + start_offset)/(CM_MEM_PAGE_SIZE);
		if((ocon->thistask_datalen + start_offset) % (CM_MEM_PAGE_SIZE) != 0) {
			p_mm_task->out_num_content_objects ++;
		}
	} else {
		p_mm_task->out_num_content_objects = 1; 
		OM_UNSET_FLAG(ocon, OMF_BUF_CPY_SKIP_OFFSET);
	}

	if(ocon->comp_status == 1) {
	    if(!OM_CHECK_FLAG(ocon, OMF_COMPRESS_TASKPOSTED| OMF_COMPRESS_TASKRETURN) ) {
		if (OM_CHECK_FLAG(ocon, (OMF_UNCHUNK_CACHE|OMF_UNCHUNK_NOCACHE))) {
		    unchunk_and_cache(ocon);
		} else {
		    om_post_compression_task(ocon, NULL);
		}
		return TRUE;
	    } else if(OM_CHECK_FLAG(ocon, OMF_COMPRESS_TASKPOSTED)) {
		return TRUE;
	    }

	    update_compress_end(ocon);
	}
	/*
	 * when origin does not have byte range request support
	 * streaming is enabled. once the desired content received, we reset
	 * streaming flag
	 */
	if (OM_CHECK_FLAG(ocon, OMF_HTTP_STREAMING_DATA)) {
		if(ocon->p_mm_task && ocon->p_mm_task->in_attr) {
			attrbuf = ocon->p_mm_task->in_attr;
		} else {
			bm_attrbuf = nkn_buffer_get(
				    &ocon->uol, NKN_BUFFER_ATTR);
			attrbuf = bm_attrbuf;
		}
		if (attrbuf) {
			pnknattr = (nkn_attr_t *)
					nkn_buffer_getcontent(attrbuf);
			if ((ocon->comp_status == 0) && (ocon->phttp->tot_reslen == ocon->phttp->content_length)) {
				pnknattr->content_length = ocon->phttp->content_length;
				nkn_attr_reset_streaming(pnknattr);
			}else if((ocon->comp_status == 1 )&& OM_CHECK_FLAG(ocon, OMF_COMPRESS_COMPLETE) ) {
				pnknattr->content_length = ocon->content_length;
				nkn_attr_reset_streaming(pnknattr);
			}else {
				nkn_attr_set_streaming(pnknattr);
			}
		}
		if (bm_attrbuf) {
			nkn_buffer_release(bm_attrbuf);
		}
	}

	if (OM_CHECK_FLAG(ocon, (OMF_UNCHUNK_CACHE|OMF_UNCHUNK_NOCACHE)) && (ocon->comp_status == 0)) {
		unchunk_and_cache(ocon);
	} else if ((ocon->comp_status == 1) && OM_CHECK_FLAG(ocon, OMF_COMPRESS_TASKRETURN)) {
		compress_and_cache(ocon);
		if(ocon->thistask_datalen == 0)
		    return TRUE;
	    } else {
	    if (OM_CHECK_FLAG(ocon, OMF_WAIT_COND) && ocon->httpreqcon &&
		OM_CHECK_FLAG(ocon, OMF_CON_CLOSE) &&
		ocon->oscfg.policies->eod_on_origin_close &&
		!(CHECK_HTTP_FLAG(&ocon->httpreqcon->http, HRF_BYTE_RANGE)) &&
		ocon->phttp->respcode == 200) {
		if(ocon->content_length != ocon->phttp->tot_reslen) {
		    glob_om_num_eod_on_close++;
		    ocon->content_length = ocon->phttp->tot_reslen;
		    ocon->phttp->content_length = ocon->phttp->tot_reslen;
		    ocon->httpreqcon->http.content_length = ocon->phttp->tot_reslen;
		    /* Add Content-Length header */
		    rv = snprintf(content_length_str, sizeof(content_length_str), "%ld",
				    ocon->content_length);
		    content_length_str[rv] = '\0';
		    delete_known_header(&ocon->phttp->hdr,
				    MIME_HDR_CONTENT_LENGTH);
		    rv = add_known_header(&ocon->phttp->hdr,
				    MIME_HDR_CONTENT_LENGTH, content_length_str,
				    strlen(content_length_str));
		    if(ocon->p_mm_task->in_attr) {
			attrbuf = ocon->p_mm_task->in_attr;
		    } else {
			bm_attrbuf = nkn_buffer_get(&ocon->uol,
						NKN_BUFFER_ATTR);
			attrbuf = bm_attrbuf;
		    }
		    if(attrbuf) {
			pnknattr = (nkn_attr_t *) nkn_buffer_getcontent(attrbuf);
			pnknattr->content_length = ocon->content_length;
			init_http_header(&hdrs, 0, 0);
			rv = nkn_attr2http_header(pnknattr, 0, &hdrs);
			if (!rv) {
			    update_known_header(&hdrs, MIME_HDR_CONTENT_LENGTH,
				    content_length_str, strlen(content_length_str),
				    1 /* Replace */);
			    mime_header2nkn_buf_attr(&hdrs, 0, attrbuf,
					      NULL,
					    nkn_buffer_getcontent);
			}
			shutdown_http_header(&hdrs);
		    }
		    if (bm_attrbuf) {
			nkn_buffer_release(bm_attrbuf);
		    }
		}
	    }
	}

	DBG_LOG(MSG, MOD_OM, "fd=%d start_offset=0x%lx(%ld) size=0x%lx(%ld)",
		OM_CON_FD(ocon), 
		ocon->uol.offset, ocon->uol.offset, 
		ocon->thistask_datalen, ocon->thistask_datalen);
	assert(ocon->thistask_datalen != 0);

	if (p_mm_task->in_flags & MM_FLAG_TRACE_REQUEST) {
		DBG_TRACE("OM returning %s bytes for object %s offset=%ld length=%ld",
			  OM_CHECK_FLAG(ocon, OMF_NO_CACHE) ? 
			  	"non-cacheable" : "cacheable",
			  nkn_cod_get_cnp(ocon->uol.cod), ocon->uol.offset, 
			  ocon->thistask_datalen);
	}

	if (OM_CHECK_FLAG(ocon, OMF_INLINE_INGEST)) {
		tfm_data = (TFM_put_privatedata_t *)
			nkn_calloc_type(1, sizeof(TFM_put_privatedata_t), mod_om_TFM_put_privatedata_t);
		if (!tfm_data) {
			DBG_LOG(MSG, MOD_OM, "nkn_calloc_type() failed, fd=%d",
				OM_CON_FD(ocon));
			free_attrbuf(p_mm_task);
			return FALSE;
		}
	} else {
		free_attrbuf(p_mm_task);
	}

	content_offset = ocon->content_offset;
	offset_len = 0;
	for(i=0; i < (int)p_mm_task->out_num_content_objects; i++) {

		if(ocon->thistask_datalen > CM_MEM_PAGE_SIZE) {
			len = ocon->phttp->content[i].iov_len;
		} else {
			len = MIN(ocon->thistask_datalen,
				ocon->phttp->content[i].iov_len);
		}
#if 0
		align_offset = 
			 (( ocon->uol.offset + offset_len )%CM_MEM_PAGE_SIZE);
		if ( (len + align_offset )> CM_MEM_PAGE_SIZE) {
			len = (CM_MEM_PAGE_SIZE - align_offset);
					 
			p_mm_task->out_num_content_objects++;
		}
#endif


		// Setup the UOL
		uol.cod = ocon->uol.cod;
		uol.offset = offset_len + ocon->uol.offset ;
		uol.length = len;

		ptr_vobj = p_mm_task->in_content_array[i];

		if (tfm_data) {
			nkn_buffer_hold(ptr_vobj);
			tfm_data->bufs[tfm_data->num_bufs++] = ptr_vobj;

			tfm_data->content[tfm_data->num_iovecs].iov_base = 
				(char *)nkn_buffer_getcontent(ptr_vobj) + 
					(uol.offset % CM_MEM_PAGE_SIZE);
			tfm_data->content[tfm_data->num_iovecs].iov_len = 
				uol.length;
			tfm_data->num_iovecs++;
		}
		/* set the attribute first */
		if ((ocon->range_offset == uol.offset) && 
			ocon->p_mm_task->in_attr) {

			nkn_uol_t attruol;
			attruol = uol;
			attruol.offset = 0;
			attruol.length = 0;
			if (tfm_data) {
				nkn_buffer_hold(ocon->p_mm_task->in_attr);
				tfm_data->attrbuf = ocon->p_mm_task->in_attr;
				if (ocon->p_mm_task->provider_flags & 
					GET_PROVIDER_ALLOC_ATTRBUF) {
					tfm_data->flags |= 
						TFM_FLAGS_ALLOC_ATTRBUF;
				}
			}
			nkn_buffer_setid(ocon->p_mm_task->in_attr, &attruol, 
				OriginMgr_provider,
				(OM_CHECK_FLAG(ocon, OMF_NO_CACHE) || 
				ocon->p_mm_task->in_flags & MM_FLAG_REQ_NO_CACHE)
						    ? NKN_BUFFER_SETID_NOCACHE : 0);
		}
		/* buffer setid */
		nkn_buffer_setid(ptr_vobj, &uol, OriginMgr_provider, 
			(OM_CHECK_FLAG(ocon, OMF_NO_CACHE) || 
			ocon->p_mm_task->in_flags & MM_FLAG_REQ_NO_CACHE)
					    ? NKN_BUFFER_SETID_NOCACHE : 0);

		content_offset += len;
		ocon->thistask_datalen -= len;
		offset_len += len;
		if(ocon->thistask_datalen <= 0) break;
	}

	if (tfm_data) {
		// Create MM_put task
		if (!tfm_data->attrbuf) {
			free_attrbuf(p_mm_task);
		}
		MM_put_data = (MM_put_data_t *)nkn_calloc_type(1, sizeof(MM_put_data_t), mod_om_MM_put_data_t);
		if (!MM_put_data) {
			DBG_LOG(MSG, MOD_OM, "calloc() failed, fd=%d", 
				OM_CON_FD(ocon));
			free_tfm_privatedata(tfm_data);
			return FALSE;
		}

#ifdef SEND_UOL_URI
		MM_put_data->uol.uri = (char *)nkn_cod_get_cnp(ocon->uol.cod);
#endif
		MM_put_data->uol.cod = nkn_cod_dup(ocon->uol.cod, NKN_COD_ORIGIN_MGR);
		MM_put_data->uol.offset = ocon->uol.offset;
		MM_put_data->uol.length = offset_len;

		MM_put_data->errcode = 0;
		MM_put_data->iov_data_len = tfm_data->num_iovecs;
		MM_put_data->domain = 0;
		MM_put_data->attr = tfm_data->attrbuf ?
			nkn_buffer_getcontent(tfm_data->attrbuf) : 0;
		MM_put_data->iov_data = tfm_data->content;
		MM_put_data->total_length = ocon->content_length;
		MM_put_data->flags = 0;
		MM_put_data->ptype = TFMMgr_provider;

		taskid = nkn_task_create_new_task(
						TASK_TYPE_MEDIA_MANAGER,
						TASK_TYPE_ORIGIN_MANAGER,
						TASK_ACTION_INPUT,
						MM_OP_WRITE,
						MM_put_data, 
						sizeof(*MM_put_data),
						0 /* no deadline */  );
		if (taskid == -1) {
                        DBG_LOG(MSG, MOD_OM, "task creation failed. fd=%d uri=%s off=%ld len=%ld",
                                OM_CON_FD(ocon), nkn_cod_get_cnp(MM_put_data->uol.cod),
                                MM_put_data->uol.offset, MM_put_data->uol.length);
                        free_tfm_privatedata(tfm_data);
                        glob_sched_task_create_fail_cnt++;
                        return FALSE;
		}
		DBG_LOG(MSG, MOD_OM, 
			"Created MM_put task=%ld fd=%d uri=%s off=%ld len=%ld",
			taskid, OM_CON_FD(ocon), 
			nkn_cod_get_cnp(MM_put_data->uol.cod), 
			MM_put_data->uol.offset, MM_put_data->uol.length);
		nkn_task_set_private(TASK_TYPE_ORIGIN_MANAGER, taskid, 
			(void *) tfm_data);
		nkn_task_set_state(taskid, TASK_STATE_RUNNABLE);
		ocon->tfm_taskid = taskid;
		glob_om_call_tfm ++;
	}
	else {
		glob_om_not_call_tfm ++;
	}

	if (p_mm_task->provider_flags & GET_PROVIDER_ALLOC_ATTRBUF) {
		p_mm_task->in_attr = 0;
	}

task_return:
	// Task complete
	keyhash1 = n_dtois_hash(&ocon->hash_cod);
	uint64_t offset = content_offset + ocon->content_start;
	
        d.ui = HASH_URI_CODEI(ocon->uol.cod, offset);
	d.us = HASH_URI_CODES(offset); 
	keyhash2 = n_dtois_hash(&d);
	NKN_MUTEX_LOCK(&omcon_mutex);
	if (ocon->hash_cod_flag == 0) {
		nchash_remove_off0(om_con_by_uri, keyhash1, &ocon->hash_cod);
		ocon->hash_cod = d;
		if ((temp = nchash_lookup_off0(om_con_by_uri, keyhash2, &ocon->hash_cod)) == NULL) {
			nchash_insert_new_off0(om_con_by_uri, keyhash2, &ocon->hash_cod, ocon);
		} else {
			keyhash1 = n_uint64_hash(&ocon->hash_fd);
			nchash_remove(om_con_by_fd, keyhash1, &ocon->hash_fd);
			ocon->hash_cod_flag = 1;
			ocon->hash_fd = -1;
		}
	}
	ocon->content_offset = content_offset;
	NKN_MUTEX_UNLOCK(&omcon_mutex);
	OM_UNSET_FLAG(ocon, OMF_BUFFER_SWITCHED);
	om_resume_task(ocon);
	ocon->p_mm_task = 0;

        return TRUE;
}

void free_tfm_privatedata(TFM_put_privatedata_t *tfm_data)
{
	int n;
	if (tfm_data->attrbuf) {
		nkn_buffer_release(tfm_data->attrbuf);
		if (tfm_data->flags & TFM_FLAGS_ALLOC_ATTRBUF) {
			nkn_buffer_release(tfm_data->attrbuf);
			glob_om_tot_attrbufs_alloced--;
		}
	}

	for (n = 0; n < tfm_data->num_iovecs; n++) {
		nkn_buffer_release(tfm_data->bufs[n]);
	}
	free(tfm_data);
}

/*
 * TASK_TYPE_ORIGIN_MANAGER functions
 */
void om_mgr_input(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
	return;
}

void om_mgr_output(nkn_task_id_t id)
{

        http_cb_t * phttp ;

        struct nkn_task *ntask=NULL;
        ntask = nkn_task_get_task(id);
	compress_msg_t *cp = nkn_task_get_data(id);
	if(ntask && ntask->dst_module_id==TASK_TYPE_COMPRESS_MANAGER)
	{
		omcon_t *ocon = (omcon_t *)cp->ocon_ptr;
		pthread_mutex_lock(&gnm[ocon->fd].mutex);
                if (!ocon) {
                        DBG_LOG(MSG,MOD_OM,
                        "No data from compressmgr for taskid=%ld", id);
			nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP, TASK_STATE_RUNNABLE);
			pthread_mutex_unlock(&gnm[ocon->fd].mutex);
                        return;
                }

		if(cp->comp_err) {
		    free(cp);
		    //ocon->comp_status = 0;
		    OM_UNSET_FLAG(ocon, (OMF_COMPRESS_TASKPOSTED|OMF_COMPRESS_INIT_DONE));
		    free(ocon->comp_buf);
		    ocon->comp_task_ptr = NULL;
		    ocon->comp_type  = -1;
		    ocon->comp_offset = 0;
		    ocon->p_mm_task->err_code = NKN_HTTP_COMPRESS_FAIL;
		    OM_get_ret_error_to_task(ocon);
		    pthread_mutex_unlock(&gnm[ocon->fd].mutex);

		    return ;
		}

                DBG_LOG(MSG, MOD_OM, "task returned");

        	phttp = ocon->phttp;
		if(cp->comp_init == 1) {
		    OM_SET_FLAG(ocon, OMF_COMPRESS_INIT_DONE);
		} else {
		    OM_UNSET_FLAG(ocon, OMF_COMPRESS_INIT_DONE);
		}
		if(cp->comp_end) {
		    OM_SET_FLAG(ocon, OMF_COMPRESS_COMPLETE);
		}

		ocon->comp_offset = cp->comp_offset;
	
	nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP,
                                       TASK_STATE_RUNNABLE);
#if 1
	if(!OM_CHECK_FLAG(ocon, OMF_COMPRESS_COMPLETE)) {
		cp_add_event(ocon->fd, OP_EPOLLOUT, NULL);
	}
#endif

	OM_UNSET_FLAG(ocon, OMF_COMPRESS_TASKPOSTED);
	OM_SET_FLAG(ocon, OMF_COMPRESS_TASKRETURN);
	if(OM_CHECK_FLAG(ocon, (OMF_UNCHUNK_NOCACHE|OMF_UNCHUNK_CACHE)) &&
		OM_CHECK_FLAG(ocon, OMF_COMPRESS_COMPLETE)) {
		uint32_t content_length = ocon->strm.total_in;
		if(content_length < ocon->oscfg.nsc->compress_config->min_obj_size || content_length > ocon->oscfg.nsc->compress_config->max_obj_size) {
			ocon->p_mm_task->err_code = NKN_HTTP_COMPRESS_FAIL;
			OM_get_ret_error_to_task(ocon);
		} else {
		    OM_get_ret_task(ocon);
		}
	} else {
		OM_get_ret_task(ocon);
	}
	OM_UNSET_FLAG(ocon, OMF_COMPRESS_TASKRETURN);
	free(cp);
	ocon->comp_task_ptr = NULL;

	resume_NM_socket(ocon->fd);


	if(OM_CHECK_FLAG(ocon, OMF_COMPRESS_COMPLETE) ) {
	    //ocon->comp_status = 0;
	    OM_UNSET_FLAG(ocon, (OMF_COMPRESS_FIRST_TASK|OMF_COMPRESS_INIT_DONE|OMF_COMPRESS_TASKPOSTED|OMF_COMPRESS_TASKRETURN|OMF_COMPRESS_COMPLETE));	
	    
	    if(ocon->comp_buf) {
		free(ocon->comp_buf);
		ocon->comp_buf = NULL;
	    }
	}

	pthread_mutex_unlock(&gnm[ocon->fd].mutex);
	return ;
	}
	
        /*For adns, we implemented async DNS and hence
        AUTH Mgr will return a task after completing DNS Lookup.
        Kickstart the OM_get or OM_validate here by posting it
        back to Media Manager*/

        if (ntask && ntask->dst_module_id==TASK_TYPE_AUTH_MANAGER)
        {
                auth_msg_t *am;
                MM_get_resp_t * p_mm_task;
                MM_validate_t *pvalidate;
                am=nkn_task_get_data(id);
                if (!am) {
                        DBG_LOG(MSG,MOD_OM,
                        "No data from authmgr for taskid=%ld", id);
			nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP, TASK_STATE_RUNNABLE);
                        return;
                }
                auth_dns_t *adns=(auth_dns_t*)(am->authdata);
                if (adns->caller==AUTH_DO_OM_GET)
                {
                        p_mm_task = (MM_get_resp_t *)
                                nkn_task_get_private(TASK_TYPE_ORIGIN_MANAGER, id);
                        if (!p_mm_task) {
                                DBG_LOG(MSG, MOD_OM, "No private data from authmgr taskid=%ld", id);
				nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP, TASK_STATE_RUNNABLE);
				free(adns);
				free(am);
                                return;
                        }
			p_mm_task->in_flags |= MM_FLAG_DNS_DONE;
			/******************************************************************
			 * bug 6994. This function is called by scheduler thread, so
			 * earlier, OM_get and OM_validatewas running under its context.
			 * Now kickstarting the MM task, so the MM threads will pick
			 * up this work. Need some kind of encoded cookie for MM book keeping
			 ****************************************************************/
                        //OM_get(p_mm_task);
			p_mm_task->in_ptype =
					 AM_encode_provider_cookie(p_mm_task->in_ptype,
                                                                   p_mm_task->in_sub_ptype, 0);
    			nkn_task_set_state(p_mm_task->get_task_id, TASK_STATE_RUNNABLE);
                }
                else if (adns->caller==AUTH_DO_OM_VALIDATE)
                {
                        pvalidate=(MM_validate_t*)nkn_task_get_private(TASK_TYPE_ORIGIN_MANAGER,
                                                id);
                        if (!pvalidate) {
                                DBG_LOG(MSG, MOD_OM, "No private data from authmgr taskid=%ld", id);
				nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP, TASK_STATE_RUNNABLE);
				free(adns);
				free(am);
                                return;
                        }
			pvalidate->in_flags |= MM_FLAG_DNS_DONE;
                        //OM_validate(pvalidate);
    			nkn_task_set_state(pvalidate->get_task_id, TASK_STATE_RUNNABLE);

                }
                nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP, TASK_STATE_RUNNABLE);
                free(adns);
                free(am);
                return ;
        }
	/*End adns changes*/

	MM_put_data_t *MM_put_data;
	TFM_put_privatedata_t *tfm_data;

	MM_put_data = nkn_task_get_data(id);
	if (!MM_put_data) {
		DBG_LOG(MSG, MOD_OM, "No data for taskid=%ld", id);
		assert(0);
		return;
	}

	tfm_data = (TFM_put_privatedata_t *)
		nkn_task_get_private(TASK_TYPE_ORIGIN_MANAGER, id);
	if (!tfm_data) {
		DBG_LOG(MSG, MOD_OM, "No private data for taskid=%ld", id);
		assert(0);
		return;
	}
	free_tfm_privatedata(tfm_data);

	if (MM_put_data->uol.cod != NKN_COD_NULL) {
		nkn_cod_close(MM_put_data->uol.cod, NKN_COD_ORIGIN_MGR);
	}
	free(MM_put_data);
	nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
}

void om_mgr_cleanup(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
	return;
}

/*
 *******************************************************************************
 * Interfaces to chunk_encoding_engine
 *******************************************************************************
 */
static void unchunk_and_cache(omcon_t *ocon)
{
	int rv;
	chunk_encoding_input_args_t ce_args;
	TFM_put_privatedata_t *tfm_priv = 0;
	off_t out_logical_data_offset = 0;
	off_t out_data_length = 0;
	off_t total_length;
	int   i;
	mime_header_t *trailer_hdrs = 0;
	nkn_attr_t *pnknattr;
	nkn_uol_t uol;
	off_t content_offset;
	off_t offset_len;
	nkn_uol_t attruol;
	nkn_buffer_t *attrbuf;
	nkn_buffer_t *bm_attrbuf=NULL;

	if (!ocon->chunk_encoding_data) {
		if (OM_CHECK_FLAG(ocon, OMF_UNCHUNK_CACHE)) {
			ocon->chunk_encoding_data = 
				ce_alloc_chunk_encoding_ctx_t(1 /* unchunk */, 
						alloc_buf, buf2data, free_buf);
		} else {
			ocon->chunk_encoding_data = 
				ce_alloc_chunk_encoding_ctx_t(-1 /* parse */, 
						0, 0, 0);
		}
		if (!ocon->chunk_encoding_data) {
			DBG_LOG(MSG, MOD_OM, 
				"ce_alloc_chunk_encoding_ctx_t() failed");
			goto err_exit;
		}
	}

	memset((void *)&ce_args, 0, sizeof(ce_args));
	tfm_priv = (TFM_put_privatedata_t *)
			nkn_calloc_type(1, sizeof(TFM_put_privatedata_t),
					mod_om_TFM_put_privatedata_t);
	if (!tfm_priv) {
		DBG_LOG(MSG, MOD_OM, "nkn_calloc_type failed");
		goto err_exit;
	}

	if (!ocon->uol.offset && ((ocon->comp_status ==0) ||
				(ocon->strm.total_in == 0) )) {
		ce_args.in_data_offset = ocon->phttp->cb_bodyoffset;
	}

	/* If its an internal MM request, we wont have the correct offset in
	 * the UOL. The UOL will have the actual data offset. So take the
	 * in_logical_data_offset from the one stored in the context
	 */
	if(IS_PSEUDO_FD(ocon->httpfd) && ((ocon->uol.offset) || 
				((ocon->comp_status == 1) && (ocon->strm.total_in > 0)))) {
	    ce_args.in_logical_data_offset = ocon->chunk_encoding_data->st_in_data_offset;
	} else {
	    ce_args.in_logical_data_offset = ocon->uol.offset;
	}
	ce_args.in_iovec = ocon->phttp->content;
	ce_args.in_iovec_len = ocon->p_mm_task->out_num_content_objects;

	ce_args.out_iovec = tfm_priv->content;
	ce_args.out_iovec_len = MAX_CM_IOVECS;

	ce_args.out_iovec_token = tfm_priv->bufs;
	ce_args.out_iovec_token_len = MAX_CM_IOVECS;

	ce_args.used_out_iovecs = &tfm_priv->num_iovecs;
	ce_args.out_logical_data_offset = &out_logical_data_offset;
	ce_args.out_data_length = &out_data_length;
	ce_args.trailer_hdrs = &trailer_hdrs;

	rv = ce_chunk2data(ocon->chunk_encoding_data, &ce_args);
	if (rv <= 0) {
		if (!OM_CHECK_FLAG(ocon, OMF_UNCHUNK_CACHE)) {
			if (rv < 0) {
				OM_SET_FLAG(ocon, OMF_ENDOF_TRANS);
			}
			free(tfm_priv);
		} else if (out_data_length &&
			   OM_CHECK_FLAG(ocon, OMF_UNCHUNK_CACHE)) {
			/* use the tfm_priv buffers and return them to the BM
			 * instead of BM buffers
			 */
		    if(ocon->comp_status == 0) {
			total_length = 0;
			offset_len = 0;
			content_offset = ocon->content_offset;
			for(i=0;i<tfm_priv->num_iovecs;i++) {
			    ocon->p_mm_task->out_data_buffers[i] =
				    tfm_priv->bufs[i];
			    total_length += tfm_priv->content[i].iov_len;

			    // Setup the UOL
			    uol.cod = ocon->uol.cod;
			    uol.offset = offset_len + ocon->uol.offset ;
			    uol.length = tfm_priv->content[i].iov_len;


			    nkn_buffer_setid(tfm_priv->bufs[i], &uol,
				OriginMgr_provider, 0);

			    if ((ocon->range_offset == uol.offset) &&
				    ocon->p_mm_task->in_attr) {

				attruol = uol;
				attruol.offset = 0;
				attruol.length = 0;
				nkn_buffer_setid(ocon->p_mm_task->in_attr,
					&attruol, OriginMgr_provider, 0);

			    }
			    content_offset += tfm_priv->content[i].iov_len;
			    offset_len += tfm_priv->content[i].iov_len;
			}

			NKN_MUTEX_LOCK(&omcon_mutex);
			ocon->content_offset = content_offset;
			NKN_MUTEX_UNLOCK(&omcon_mutex);
			ocon->p_mm_task->out_num_content_objects =
				0;
			ocon->p_mm_task->out_num_data_buffers =
			    tfm_priv->num_iovecs;
			ocon->phttp->content_length = total_length;
			ocon->thistask_datalen = total_length;
		    } 

			if(ocon->p_mm_task->in_attr) {
			    attrbuf = ocon->p_mm_task->in_attr;
			} else {
			    bm_attrbuf = nkn_buffer_get(&ocon->uol,
						    NKN_BUFFER_ATTR);
			    attrbuf = bm_attrbuf;
			}
			if (rv < 0) {
				// EOF, build attributes
				rv = build_unchunked_object_attrs(ocon,
						    tfm_priv,
						    attrbuf,
						    trailer_hdrs);
				if (!rv) {
					DBG_LOG(MSG, MOD_OM,
					"ce_chunk2data() eof, CTX=%p uri=%s "
					"off=%ld len=%ld",
					ocon->chunk_encoding_data,
					nkn_cod_get_cnp(ocon->uol.cod),
					    out_logical_data_offset,
					    out_data_length);
					/* unchunk completed, reset the
					 * streaming flag
					 */
					if(attrbuf && (ocon->comp_status == 0)) {
					    pnknattr = (nkn_attr_t *)
						nkn_buffer_getcontent(attrbuf);
					    nkn_attr_reset_streaming(pnknattr);
					}
					glob_om_unchunk_complete++;
				} else {
#if 0
					total_length = 0;
#endif
					DBG_LOG(MSG, MOD_OM,
					  "build_unchunked_object_attrs() "
					  "failed, rv=%d", rv);
					glob_om_unchunk_fails++;
				}
				OM_SET_FLAG(ocon, OMF_ENDOF_TRANS);

			} else {
				DBG_LOG(MSG, MOD_OM,
				"ce_chunk2data() continue, CTX=%p uri=%s "
				"off=%ld len=%ld",
				ocon->chunk_encoding_data,
				nkn_cod_get_cnp(ocon->uol.cod),
				out_logical_data_offset, out_data_length);

#if 0
				total_length = OM_STAT_SIG_TOT_CONTENT_LEN;
#endif
				if(attrbuf) {
				    pnknattr = (nkn_attr_t *)
					nkn_buffer_getcontent(attrbuf);
				    nkn_attr_set_streaming(pnknattr);
				}
			}
			    if(ocon->comp_status == 1) {
				  total_length = 0;
				  for(i=0;i<tfm_priv->num_iovecs;i++) {
				    total_length += tfm_priv->content[i].iov_len;
			    	  }
				  ocon->thistask_datalen = total_length;
				  om_post_compression_task(ocon, tfm_priv);
			    }
			if (OM_CHECK_FLAG(ocon, OMF_UNCHUNK_CACHE)) {
#if 0
				rv = push_to_tfm(ocon, tfm_priv, 
				         	out_logical_data_offset, 
					 	out_data_length, total_length);
				if (rv) {
					DBG_LOG(MSG, MOD_OM, 
						"push_to_tfm() failed, rv=%d", 
						rv);
				}
#endif
			    free(tfm_priv);
			}
			if (bm_attrbuf) {
			    nkn_buffer_release(bm_attrbuf);
			}
		} else {
			free(tfm_priv);
		}
	} else {
		DBG_LOG(MSG, MOD_OM, "ce_chunk2data() failed, rv=%d "
			"uol.uri=%s uol.offset=0x%lx",
			rv, nkn_cod_get_cnp(ocon->uol.cod), ocon->uol.offset);
		goto err_exit;
	}

	if (trailer_hdrs) {
		shutdown_http_header(trailer_hdrs);
		free(trailer_hdrs);
	}
	return;

err_exit:

	/* Abort further chunk encoding transformation action */
	OM_UNSET_FLAG(ocon, (OMF_UNCHUNK_CACHE|OMF_UNCHUNK_NOCACHE));
	if (ocon->chunk_encoding_data) {
		ce_free_chunk_encoding_ctx_t(ocon->chunk_encoding_data);
		ocon->chunk_encoding_data = 0;
	}

	if (tfm_priv) {
		free(tfm_priv);
	}

	if (trailer_hdrs) {
		shutdown_http_header(trailer_hdrs);
		free(trailer_hdrs);
	}
	glob_om_unchunk_fails++;
}
static void compress_and_cache(omcon_t *ocon)
{
	off_t total_length = 0;
	int   i;
	nkn_uol_t uol;
	off_t content_offset= 0;
	off_t offset_len = 0;
	nkn_uol_t attruol;
	compress_msg_t *cp = ocon->comp_task_ptr;
	int comp_datalen = cp->comp_out - cp->comp_offset;
	int len = 0;
	content_offset = ocon->content_offset;
	for(i=0;i<(int)cp->out_iovec_cnt;i++) {
	    ocon->p_mm_task->out_data_buffers[i] =
		    cp->out_bufs[i];
	    total_length += cp->out_content[i].iov_len;

	    if(comp_datalen > CM_MEM_PAGE_SIZE) {
		len = cp->out_content[i].iov_len;
            } else {
                len = MIN(comp_datalen,
                        cp->out_content[i].iov_len);
            }
 
	    // Setup the UOL
	    uol.cod = ocon->uol.cod;
	    uol.offset = offset_len + ocon->uol.offset ;
	    uol.length = len;


	    nkn_buffer_setid(cp->out_bufs[i], &uol,
		OriginMgr_provider, 0);

	    if ((ocon->range_offset == uol.offset) &&
		    ocon->p_mm_task->in_attr) {

		attruol = uol;
		attruol.offset = 0;
		attruol.length = 0;
		nkn_buffer_setid(ocon->p_mm_task->in_attr,
			&attruol, OriginMgr_provider, 0);

	    }
	    content_offset += len;
	    offset_len += len;
	    comp_datalen -= len;
	}

	NKN_MUTEX_LOCK(&omcon_mutex);
	ocon->content_offset = content_offset;
	NKN_MUTEX_UNLOCK(&omcon_mutex);
	ocon->p_mm_task->out_num_content_objects =
		0;
	ocon->p_mm_task->out_num_data_buffers =
	    cp->out_iovec_cnt;
	ocon->thistask_datalen = total_length;

	return;

}

static void *alloc_buf(off_t *bufsize)
{
	nkn_buffer_t *buf;

	buf = nkn_buffer_alloc(NKN_BUFFER_DATA, 0, 0);
	*bufsize = CM_MEM_PAGE_SIZE;
	return (void *) buf;
}

static char *buf2data(void *token)
{
	return (char *)nkn_buffer_getcontent((nkn_buffer_t *)token);
}

static int free_buf(void *token)
{
	nkn_buffer_release((nkn_buffer_t *)token);
	return 0;
}

static int build_unchunked_object_attrs(omcon_t *ocon, 
					TFM_put_privatedata_t *tfm_priv __attribute((unused)), 
					nkn_buffer_t *attrbuf,
					mime_header_t *trailer_hdrs)
{
	nkn_attr_t *pnknattr;
	const cooked_data_types_t *pckd;
	int datalen;
	mime_hdr_datatype_t dtype;
	int rv = 0;
	mime_header_t reqhdrs;
	int reqhdrs_serialbufsz;
	static int tokens[5] = {MIME_HDR_X_NKN_CLIENT_HOST, 
				MIME_HDR_X_NKN_CACHE_POLICY,
				MIME_HDR_X_NKN_PE_HOST_HDR,
				MIME_HDR_X_NKN_ORIGIN_IP,
				MIME_HDR_X_NKN_UNCOMPRESSED_LENGTH};

	init_http_header(&reqhdrs, 0, 0);

	if (!trailer_hdrs) {
		DBG_LOG(MSG, MOD_OM, "No trailer header provided"); 
		rv = 1;
		goto exit;
	}

	rv = deserialize(ocon->hdr_serialbuf, ocon->hdr_serialbufsz, 
			 &reqhdrs, (char *)1, 0);
	if (rv) {
		DBG_LOG(MSG, MOD_OM, "deserialize() failed, rv=%d", rv);
		rv = 2;
		goto exit;
	}
	delete_known_header(&reqhdrs, MIME_HDR_TRANSFER_ENCODING);
	rv = copy_http_headers(&reqhdrs, trailer_hdrs);
	if (rv) {
		DBG_LOG(MSG, MOD_OM, "copy_http_headers() failed, rv=%d", rv);
		rv = 21;
		goto exit;
	}
	//mime_hdr_remove_hop2hophdrs_except(&reqhdrs, MIME_HDR_X_NKN_CLIENT_HOST);
	mime_hdr_remove_hop2hophdrs_except_these(&reqhdrs, 4, tokens);

	reqhdrs_serialbufsz = serialize_datasize(&reqhdrs);
	if (!reqhdrs_serialbufsz) {
		DBG_LOG(MSG, MOD_OM, "serialize_datasize() failed, rv=%d", rv);
		rv = 3;
		goto exit;
	}

#if 0
	tfm_priv->attrbuf = nkn_buffer_alloc(NKN_BUFFER_ATTR, 0);
	if (tfm_priv->attrbuf) {
		nkn_buffer_set_bufsize(tfm_priv->attrbuf, reqhdrs_serialbufsz);
		nkn_buffer_hold(tfm_priv->attrbuf);
		tfm_priv->flags |= TFM_FLAGS_ALLOC_ATTRBUF;
		glob_om_tot_attrbufs_alloced++;
	} else {
		DBG_LOG(MSG, MOD_OM, 
			"nkn_buffer_alloc(NKN_BUFFER_ATTR) failed");
		rv = 4;
		goto exit;
	}
	rv = mime_header2nkn_buf_attr(&reqhdrs, 0, tfm_priv->attrbuf, 
				      nkn_buffer_set_attrbufsize, 
				      nkn_buffer_getcontent);
	if (rv) {
		DBG_LOG(MSG, MOD_OM, 
			"http_header2nkn_buf_attr() failed, rv=%d", rv);
		rv = 5;
		goto exit;
	}
	pnknattr = (nkn_attr_t *)nkn_buffer_getcontent(tfm_priv->attrbuf);

	rv = get_cooked_known_header_data(&reqhdrs, MIME_HDR_CONTENT_LENGTH, 
					  &pckd, &datalen, &dtype);
	if (!rv && (dtype == DT_SIZE)) {
	    pnknattr->content_length = pckd->u.dt_size.ll;
	} else {
	    pnknattr->content_length = 0;
	}
	pnknattr->obj_version = ocon->unchunked_obj_objv;
	pnknattr->cache_expiry = ocon->unchunked_obj_expire_time;
	pnknattr->cache_create = 
		ocon->request_response_time - 
		ocon->unchunked_obj_corrected_initial_age;
	if (pnknattr->cache_create < 0) {
		pnknattr->cache_create = ocon->request_response_time;
	}

#endif
	if(attrbuf) {

	    pnknattr = (nkn_attr_t *)nkn_buffer_getcontent(attrbuf);
	    rv = http_header2nkn_attr(&reqhdrs, 0, pnknattr);
	    if (rv) {
		    DBG_LOG(MSG, MOD_OM,
			    "http_header2nkn_attr() failed, rv=%d", rv);
		    rv = 4;
		    goto exit;
	    }

	    rv = get_cooked_known_header_data(&reqhdrs, MIME_HDR_CONTENT_LENGTH,
					    &pckd, &datalen, &dtype);
	    if (!rv && (dtype == DT_SIZE)) {
		    pnknattr->content_length = pckd->u.dt_size.ll;
	    } else {
		    pnknattr->content_length = 0;
	    }
	} else {
	    rv = 5;
	}

	if(ocon->comp_status == 1) {
		ocon->phttp->content_length = pnknattr->content_length;
	}
exit:

	shutdown_http_header(&reqhdrs);
	return rv;
}

static int32_t om_post_auth_task(void *data, int32_t type)
{
        con_t * request_con;
        const namespace_config_t *nsc;
        char ip[1024];
        char *p_ip=ip;
        int32_t ip_len;
	int32_t dns_pq;
        uint16_t port;
	int dns_found;
        auth_msg_t *am;
        auth_dns_t *adns;
        nkn_task_id_t taskid;
        int32_t ret;
        MM_get_resp_t* p_mm_task;
        MM_validate_t* pvalidate;
	const char *location;
	int locationlen;
	//int hostlen, portlen;
	u_int32_t attr;
	int hdrcnt;
	const char *buf;
	int buflen;
	int retcode;
	int redirect_host_flag = 0;
        uint8_t resolved;
        ip_addr_t ret_ip;
	int lookup_method;
	int origin_ip_family = AF_INET;
	nkn_client_data_t *client_data;

        if (type==AUTH_DO_OM_GET)
        {
                p_mm_task=(MM_get_resp_t*)data;
		client_data = (nkn_client_data_t *)(&p_mm_task->in_client_data);
                request_con= (con_t *)p_mm_task->in_proto_data;
                nsc = namespace_to_config(p_mm_task->ns_token);
	        if (nsc->http_config->origin.u.http.ip_version == FOLLOW_CLIENT) {
			origin_ip_family = client_data->client_ip_family;
        	} else if (nsc->http_config->origin.u.http.ip_version == FORCE_IPV6) {
			origin_ip_family = AF_INET6;
        	} else {
			origin_ip_family = AF_INET;
        	}
		if (CHECK_HTTP_FLAG(&request_con->http, HRF_TPROXY_MODE)) {
			origin_ip_family = client_data->client_ip_family;
		}
                if (CHECK_CON_FLAG(request_con, CONF_REFETCH_REQ) && nsc && nsc->http_config &&
                                nkn_http_is_tproxy_cluster(nsc->http_config)) {
                        lookup_method = REQ2OS_TPROXY_ORIGIN_SERVER_LOOKUP;
                } else {
                        lookup_method = REQ2OS_ORIGIN_SERVER_LOOKUP;
                }
                ret = request_to_origin_server(lookup_method, &p_mm_task->req_ctx,
                                      request_con ? &request_con->http.hdr : 0,
                                      nsc, 0, 0,
                                      &p_ip, sizeof(ip), &ip_len,
                                      &port, 1, 0);
                release_namespace_token_t(p_mm_task->ns_token);
		/* 302 Redirect.
		 * If the host/origin was a different one, then connect
		 * to a server other than what the namespace tells us to.
		 */
		if (CHECK_HTTP_FLAG(&request_con->http, HRF_302_REDIRECT)) {
			// Get the LOCATION header
			ret = get_known_header(&request_con->http.hdr,
					MIME_HDR_X_LOCATION, &location, &locationlen,
					&attr, &hdrcnt);
			if (!ret && strncmp(location, "http://", 7) == 0) {
				// Check if a host header exisits.. if so, swap out
				// the hold destinatio ip with the new one.
				ret = get_known_header(&request_con->http.hdr,
						MIME_HDR_X_REDIRECT_HOST, &buf, &buflen,
						&attr, &hdrcnt);
				if (!ret) {
					/* invalid free done here
					if (p_ip) {
					    free(p_ip);
					}
					*/
					p_ip = nkn_calloc_type(1, sizeof(char)*(buflen + 1), mod_om_strdup1);
					if (p_ip) {
						snprintf(p_ip, buflen+1, "%s", buf);
						ip_len = buflen;
						redirect_host_flag = 1;
					}
					else {
						retcode = DNS_TASK_FAILED;
						goto bail_error;
					}
				}
			}
		}
        }
        else if (type==AUTH_DO_OM_VALIDATE)
        {
                pvalidate=(MM_validate_t*)data;
		client_data = (nkn_client_data_t *)(&pvalidate->in_client_data);
                request_con= (con_t *)pvalidate->in_proto_data;
                nsc = namespace_to_config(pvalidate->ns_token);
                if (nsc->http_config->origin.u.http.ip_version == FOLLOW_CLIENT) {
			origin_ip_family = client_data->client_ip_family;
                } else if (nsc->http_config->origin.u.http.ip_version == FORCE_IPV6) {
                        origin_ip_family = AF_INET6;
                } else {
                        origin_ip_family = AF_INET;
                }
		if (CHECK_HTTP_FLAG(&request_con->http, HRF_TPROXY_MODE)) {
			origin_ip_family = client_data->client_ip_family;
		}
                if (CHECK_CON_FLAG(request_con, CONF_REFETCH_REQ) && nsc && nsc->http_config &&
                                nkn_http_is_tproxy_cluster(nsc->http_config)) {
                        lookup_method = REQ2OS_TPROXY_ORIGIN_SERVER_LOOKUP;
                } else {
                        lookup_method = REQ2OS_ORIGIN_SERVER_LOOKUP;
                }
                ret = request_to_origin_server(lookup_method, &pvalidate->req_ctx,
                                      request_con ? &request_con->http.hdr : 0,
                                      nsc, 0, 0,
                                      &p_ip, sizeof(ip), &ip_len,
                                      &port, 1, 0);
                release_namespace_token_t(pvalidate->ns_token);
        }

	if (ret) {
                DBG_LOG(ERROR, MOD_OM, "request_to_origin_server failed, ret=%d", ret);
		retcode = DNS_TASK_FAILED;
		goto bail_error;
	}
        ret = inet_pton(origin_ip_family, p_ip, &ret_ip.addr);
	if (ret == -1) {
		retcode = DNS_TASK_FAILED;
		DBG_LOG(ERROR, MOD_OM, "address family is not supported:%d", origin_ip_family);
		goto bail_error;
	}
        if (ret > 0)
        {
                /*No need to post a task in this and below case*/
		retcode = DNS_TASK_ISIP;
		SET_CON_FLAG(request_con, CONF_DNS_HOST_IS_IP);
		DBG_LOG(ERROR, MOD_OM, "domain name is already an ip:%s for family:%d", p_ip, origin_ip_family);
		goto bail_error;
        }
	if ((origin_ip_family == AF_INET) && ret == 0) {
	        ret=inet_addr(p_ip);
        	if (ret == 0) {
			/* address translation resulted in zero as ip*/
                	/*No need to post a task */
                	retcode = DNS_TASK_ISIP;
			SET_CON_FLAG(request_con, CONF_DNS_HOST_IS_IP);
			DBG_LOG(ERROR, MOD_OM, "inet_addr resulted in zero for domain:%s, family:%d", p_ip, origin_ip_family);
                	goto bail_error;
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
        dns_found = dns_hash_and_lookup(p_ip, origin_ip_family, &resolved);
        if (dns_found && resolved)
        {
                retcode = DNS_TASK_ISCACHE;
		goto bail_error; // Need to cleanup p_ip, if allocated.
        }
        /********************************************************
        This just means that we have already done DNS lookup once
        and it has failed, either because of timeout or DNS
        server gave a junk response. 
        *******************************************************/
	if (dns_found) {
        	DBG_LOG(ERROR, MOD_OM,
                       "dns_hash_and_lookup: found entry for '%s' in dns table, but its a failed attempt",
                       (p_ip ? p_ip: "NULL"));
		retcode = DNS_TASK_FAILED;
		goto bail_error; // Need to cleanup p_ip, if allocated.
	}

	if (!redirect_host_flag && 
		request_con->os_failover.num_ip_index) {

		DBG_LOG(ERROR, MOD_OM,
			"dns_hash_and_lookup: No DNS entry found and exceeded max retries for origin failover");
		glob_om_err_dns_failover_and_lookup++;
		retcode = DNS_TASK_FAILED;
		goto bail_error; // Need to cleanup p_ip, if allocated.
	}

        //we need to create a task for async DNS to be handled by authmgr
        adns = nkn_calloc_type(1, sizeof(auth_dns_t), mod_auth_uchar);
        if (!adns)
        {
                //todo
                DBG_LOG(ERROR, MOD_OM, "nkn_calloc_type() failed for auth_dns post_auth_task");
		retcode = DNS_TASK_FAILED;
		goto bail_error;
        }
        am = nkn_calloc_type(1, sizeof(auth_msg_t), mod_auth_msg_t);
        if (!am)
        {
                //todo
                DBG_LOG(ERROR, MOD_OM, "nkn_calloc_type() failed for auth_msg post_auth_task");
                free(adns);
		retcode = DNS_TASK_FAILED;
		goto bail_error;
        }

	dns_pq=glob_dns_task_called-glob_dns_task_completed;
	if (dns_pq > max_dns_pend_q)
	{
		glob_dns_too_many_parallel_lookup++;
		free(adns);
		free(am);
		retcode = DNS_QUEUE_FULL;
		goto bail_error;
	}

        am->magic=AUTH_MGR_MAGIC;
        am->authtype=DNSLOOKUP;
	ip_len=strlen(p_ip);
        memcpy(adns->domain,p_ip,ip_len);
	adns->domain_len = ip_len;
	if (nsc->http_config->origin.u.http.server[0]->dns_query_len) {
		memcpy(adns->dns_query,
			nsc->http_config->origin.u.http.server[0]->dns_query,
			nsc->http_config->origin.u.http.server[0]->dns_query_len);
		adns->dns_query_len = nsc->http_config->origin.u.http.server[0]->dns_query_len;
	}
        adns->ip[0].family = origin_ip_family;
        adns->caller=type;
        am->authdata=adns;
        taskid = nkn_task_create_new_task(TASK_TYPE_AUTH_MANAGER,
                                          TASK_TYPE_ORIGIN_MANAGER,
                                          TASK_ACTION_INPUT,
                                          0,
                                          am,
                                          sizeof(*am),
                                          0 /* no deadline */  );
	if (taskid == -1) {
                free(adns);
                free(am);
                glob_sched_task_create_fail_cnt++;
                DBG_LOG(MSG, MOD_OM, "task creation failed. domain:%s", p_ip);
		retcode = NKN_RESOURCE_TASK_FAILURE;
		goto bail_error;
	}
	DBG_LOG(MSG, MOD_OM,
		"Created authmgr task=%ld to resolve domain:%s",taskid, p_ip);
        nkn_task_set_private(TASK_TYPE_ORIGIN_MANAGER, taskid, data);
        nkn_task_set_state(taskid, TASK_STATE_RUNNABLE);

        retcode = DNS_TASK_POSTED;

bail_error:
	(redirect_host_flag) ? free(p_ip) : 0;
	return retcode;
}

int32_t om_post_compression_task(omcon_t *data, TFM_put_privatedata_t *tfm_priv)
{
        nkn_task_id_t taskid;
	compress_msg_t *cp = NULL;
	int retcode;
	nkn_buffer_t *attrbuf = NULL;
	nkn_buffer_t *bm_attrbuf = NULL;
	nkn_attr_t *pnknattr = NULL;
	int rv;
	mime_header_t hdrs;
	omcon_t *ocon = (omcon_t *)data;

        DBG_LOG(MSG, MOD_OM, "task posted");

	if(ocon->thistask_datalen <= 0) 
	    return 0;



	//TODO : Write code here 

	if(OM_CHECK_FLAG(ocon, OMF_COMPRESS_FIRST_TASK)) {
	    ocon->phttp->http_encoding_type = ocon->comp_type;
            if(ocon->p_mm_task)
		ocon->p_mm_task->encoding_type =  ocon->comp_type;

            OM_SET_FLAG(ocon, OMF_HTTP_STREAMING_DATA);


	    delete_known_header(&ocon->phttp->hdr,
			    MIME_HDR_CONTENT_LENGTH);
	    if(ocon->p_mm_task && ocon->p_mm_task->in_attr) {
		attrbuf = ocon->p_mm_task->in_attr;
	    } else {
		bm_attrbuf = nkn_buffer_get(&ocon->uol,
					NKN_BUFFER_ATTR);
		attrbuf = bm_attrbuf;
	    }
	    if(attrbuf) {
		pnknattr = (nkn_attr_t *) nkn_buffer_getcontent(attrbuf);
		init_http_header(&hdrs, 0, 0);
		rv = nkn_attr2http_header(pnknattr, 0, &hdrs);
		if (!rv) {
		    delete_known_header(&hdrs, MIME_HDR_CONTENT_LENGTH);
		    mime_header2nkn_buf_attr(&hdrs, 0, attrbuf,
				      NULL,
				    nkn_buffer_getcontent);
		}
		pnknattr->content_length = 0;
		shutdown_http_header(&hdrs);
	    }
	    if (bm_attrbuf) {
		nkn_buffer_release(bm_attrbuf);
	    }

	    OM_UNSET_FLAG(ocon, OMF_COMPRESS_FIRST_TASK);
	    ocon->comp_buf = (char *)nkn_malloc_type( CM_MEM_PAGE_SIZE , mod_compress_msg_t);
	}

	cp = nkn_calloc_type(1, sizeof(compress_msg_t), mod_compress_msg_t);
	cp->comp_type = ocon->comp_type;
	ocon->comp_task_ptr = cp;
	cp->strm = &ocon->strm;

	cp = ocon->comp_task_ptr;
	/* Use cp to pass input to compressor module , free cp */
	cp->comp_in_len = ocon->thistask_datalen;
	cp->comp_init = OM_CHECK_FLAG(ocon, OMF_COMPRESS_INIT_DONE) ? 1: 0;
	cp->comp_in_processed = 0;
	cp->comp_out = 0;
	cp->comp_buf = ocon->comp_buf;
	cp->comp_offset = ocon->comp_offset;
	cp->ocon_ptr = ocon;
	if (OM_CHECK_FLAG(ocon, (OMF_UNCHUNK_CACHE|OMF_UNCHUNK_NOCACHE))) {
	    if(OM_CHECK_FLAG(ocon, OMF_ENDOF_TRANS)) {
		cp->tot_expected_in =  ocon->phttp->content_length;
	    } else {
		cp->tot_expected_in = 0;
	    }
	    
	} else {
		cp->tot_expected_in = ocon->phttp->content_length;
	}
	cp->comp_err = 0;
	cp->alloc_buf = alloc_buf;
	cp->buf2data = buf2data;
	if(OM_CHECK_FLAG(ocon, (OMF_UNCHUNK_CACHE|OMF_UNCHUNK_NOCACHE)) && tfm_priv) {

	   cp->in_content = tfm_priv->content;
           cp->in_iovec_cnt = tfm_priv->num_iovecs;
	} else {
	   cp->in_content = ocon->phttp->content;
           cp->in_iovec_cnt = ocon->phttp->num_nkn_iovecs;
	}

	suspend_NM_socket(ocon->fd);
        taskid = nkn_task_create_new_task(TASK_TYPE_COMPRESS_MANAGER,
                                          TASK_TYPE_ORIGIN_MANAGER,
                                          TASK_ACTION_INPUT,
                                          0,
                                          cp,
                                          sizeof(*cp),
                                          0 /* no deadline */  );

	if (taskid == -1) {
                glob_sched_task_create_fail_cnt++;
		retcode = NKN_RESOURCE_TASK_FAILURE;
		goto bail_error;
	}
        nkn_task_set_private(TASK_TYPE_ORIGIN_MANAGER, taskid, data);
        nkn_task_set_state(taskid, TASK_STATE_RUNNABLE);

	OM_SET_FLAG(ocon, OMF_COMPRESS_TASKPOSTED);
        return TRUE;
bail_error:
	return retcode;

}


#if 0
static int push_to_tfm(omcon_t *ocon, TFM_put_privatedata_t *tfm_priv, 
		       off_t off, off_t length, off_t total_length)
{
	MM_put_data_t *MM_put_data;
	nkn_task_id_t taskid;

	MM_put_data = (MM_put_data_t *)nkn_calloc_type(1, sizeof(MM_put_data_t),
						mod_om_MM_put_data_t);
	if (!MM_put_data) {
		DBG_LOG(MSG, MOD_OM, "nkn_calloc_type() failed, fd=%d", 
			OM_CON_FD(ocon));
		free_tfm_privatedata(tfm_priv);
		return 1;
	}

#ifdef SEND_UOL_URI
	MM_put_data->uol.uri = (char *)nkn_cod_get_cnp(ocon->uol.cod);
#endif
	MM_put_data->uol.cod = nkn_cod_dup(ocon->uol.cod, NKN_COD_ORIGIN_MGR);
	MM_put_data->uol.offset = off;
	MM_put_data->uol.length = length;

	MM_put_data->errcode = 0;
	MM_put_data->iov_data_len = tfm_priv->num_iovecs;
	MM_put_data->domain = 0;
	MM_put_data->attr = tfm_priv->attrbuf ?
		nkn_buffer_getcontent(tfm_priv->attrbuf) : 0;
	MM_put_data->iov_data = tfm_priv->content;
	MM_put_data->total_length = total_length;
	MM_put_data->flags = MM_FLAG_PRIVATE_BUF_CACHE;
	MM_put_data->ptype = TFMMgr_provider;

	taskid = nkn_task_create_new_task(
					TASK_TYPE_MEDIA_MANAGER,
					TASK_TYPE_ORIGIN_MANAGER,
					TASK_ACTION_INPUT,
					MM_OP_WRITE,
					MM_put_data, 
					sizeof(*MM_put_data),
					0 /* no deadline */  );
	assert(taskid != -1);
	DBG_LOG(MSG, MOD_OM, 
		"Created unchunked MM_put task=%ld fd=%d uri=%s "
		"off=%ld len=%ld",
		taskid, OM_CON_FD(ocon), nkn_cod_get_cnp(MM_put_data->uol.cod), 
		MM_put_data->uol.offset, MM_put_data->uol.length);

	nkn_task_set_private(TASK_TYPE_ORIGIN_MANAGER, taskid, 
			     (void *) tfm_priv);
	nkn_task_set_state(taskid, TASK_STATE_RUNNABLE);
	glob_om_call_tfm ++;

	return 0;
}
#endif

/*
 * End of om_api.c
 */
