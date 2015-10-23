/* File : nkn_dpi.c
 * Copyright (C) 2014 Juniper Networks, Inc. 
 * All rights reserved.
 */

#ifndef _STUB_
int nkn_cfg_dpi_uri_mode = 0;

int nkn_dpi_init()
{
    return 0;
}

#else /* _STUB_ */

/**
 * @file nkn_dpi.c
 * @brief Utility functions for Qosmos related functions
 * @author
 * @version 1.00
 * @date 2014-04-10
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>	    /* for NF_ACCEPT */
#include <pthread.h>
#ifdef NFQUEUE_URI_FILTER
#include <libnetfilter_queue/libnetfilter_queue.h>
#endif
#include <linux/if_ether.h>
#include <dpi/modules/uevent.h>
#include <dpi/modules/uevent_hooks.h>
#include <dpi/modules/uapplication.h>
#include <dpi/protodef.h>
#include <dpi/libafc.h>
#include <dpi/libctl.h>
#include <libwrapper/uwrapper.h>
#include <sys/prctl.h>


#include "nkn_assert.h"
#include "network.h"
#include "nkn_namespace.h"
#include "nkn_http.h"
#include "nkn_accesslog.h"
#include "nknlog_pub.h"
#include "accesslog_pub.h"
#include "nkn_dpi_netfilter.h"
#include "nkn_dpi.h"
#include "nkn_dpi_priv.h"
#include "nkn_dpi_eventhandler.h"

DECLARE_DPI_ENGINE_DATA(deng_data);

int nkn_cfg_dpi_uri_mode = 0;
uint64_t nkn_dpi_filter_rej_cnt[MAX_NFQUEUE_THREADS];
uint64_t nkn_dpi_filter_acp_cnt[MAX_NFQUEUE_THREADS];
uint64_t nkn_dpi_filter_err_cnt[MAX_NFQUEUE_THREADS];
uint64_t nkn_dpi_filter_unk_cnt[MAX_NFQUEUE_THREADS];
uint64_t nkn_dpi_filter_bytes_processed[MAX_NFQUEUE_THREADS];

/*
 * Wrapper function to invoke DPI module for PFring
 * Modified to get filter status after the DPI parsing
 */
static inline int dpi_pf_data_main_mod(unsigned char *pdata,
				size_t len, const struct timeval *ts, void* u,
				struct dpi_engine_data *ext,
				struct dpi_metadata *dpi_metadata,
				uint32_t src, uint32_t dst, uint64_t thread_id)
{
    unsigned char *packet_data = (pdata + ETH_HLEN);
    struct app_pktdata   app_pkt;
    struct app_flowdata  app_flow;
    struct app_reassdata app_rs;
    void *ucnx = NULL;
    int ret;
    struct app_modules *app = ext->st + ctb_current_cpuid();
    http_dpi_sesn_data_t *sesn_data;
    int filt_status;

    nkn_dpi_filter_bytes_processed[thread_id] += len;
    DEFINE_DPI_FLOW_SIG(s);


    app_pktdata_init(&app_pkt, packet_data, len,
		     &s, FLOW_PROTO_IP, ts);

    /* step 1 : bounds packet */
    ret = app_packet_bounds(app, &app_pkt, NULL);
    if(ret) {
	/* TODO: Not sure in case of error - Currently accept the packet */
	NKN_ASSERT(0);
	nkn_pf_accept(pdata, len, u);
	nkn_dpi_filter_err_cnt[thread_id]++;
        goto end;
    }
    /* step 2 : flow retrieval */
    ret = app_flow_get(app, &app_pkt, &app_flow, dpi_metadata);

    /* step 3 : reassembly */
    ret = app_reass(app, &app_pkt, &app_flow, &app_rs, dpi_metadata);

    /* step 4 : stream injection */
    app_flow_inject(app, &app_pkt, &app_flow, &app_rs, &ucnx, NULL);

    filt_status = nkn_dpi_eventhandler_get_sess_status(ucnx, src, dst);
    if (filt_status == DPI_SESN_HTTP_LS_ACP) {
	nkn_pf_accept(pdata, len, u);
	nkn_dpi_filter_acp_cnt[thread_id]++;
    } else if (filt_status == DPI_SESN_HTTP_LS_REJ) {
	nkn_pf_reject(pdata, len, u);
	nkn_dpi_filter_rej_cnt[thread_id]++;
    } else if (filt_status == DPI_SESN_HTTP_UNK_ACP) {
	nkn_pf_accept(pdata, len, u);
	nkn_dpi_filter_unk_cnt[thread_id]++;
    } else if (!filt_status) {
	ret = nkn_dpi_eventhandler_link_pfpkt_to_session(ucnx, u, len, pdata,
						         src, dst, thread_id);
	if (ret) {
	    /* 
	     * No session data 
	     * Its better to accept the packet 
	     */
	    nkn_pf_accept(pdata, len, u);
	    nkn_dpi_filter_unk_cnt[thread_id]++;
	}
    } else {
	assert(0);
    }
end:

    /* step 5 : context cleanup*/
    app_pktdata_exit(&app_pkt);

    /* step 6 : timer processing */
    app_timer_loop(app, &app_pkt);
    return 0;

}

#ifdef NFQUEUE_URI_FILTER
/*
 * Wrapper function to invoke DPI module
 * Modified to get filter status after the DPI parsing
 */
static inline int dpi_data_main_mod(unsigned char *pdata,
				size_t len, const struct timeval *ts, void* u,
				struct dpi_engine_data *ext,
				struct dpi_metadata *dpi_metadata,
				nfqueue_dpi_xfer_t *xfer_data,
				uint64_t thread_id)
{
    struct app_pktdata   app_pkt;
    struct app_flowdata  app_flow;
    struct app_reassdata app_rs;
    void *ucnx = NULL;
    int ret;
    struct app_modules *app = ext->st + ctb_current_cpuid();
    http_dpi_sesn_data_t *sesn_data;
    int filt_status;

    nkn_dpi_filter_bytes_processed[thread_id] += len;
    DEFINE_DPI_FLOW_SIG(s);


    app_pktdata_init(&app_pkt, pdata, len,
            &s, FLOW_PROTO_IP, ts);

    /* step 1 : bounds packet */
    ret = app_packet_bounds(app, &app_pkt, NULL);
    if(ret) {
	/* TODO: Not sure in case of error - Currently accept the packet */
	NKN_ASSERT(0);
	nfq_set_verdict(xfer_data->qh, xfer_data->pkt_id, NF_ACCEPT, 0, NULL);
	nkn_dpi_filter_err_cnt[thread_id]++;
        goto end;
    }
    /* step 2 : flow retrieval */
    ret = app_flow_get(app, &app_pkt, &app_flow, dpi_metadata);

    /* step 3 : reassembly */
    ret = app_reass(app, &app_pkt, &app_flow, &app_rs, dpi_metadata);

    /* step 4 : stream injection */
    app_flow_inject(app, &app_pkt, &app_flow, &app_rs, &ucnx, NULL);

    filt_status = nkn_dpi_eventhandler_get_sess_status(ucnx, xfer_data->ip_src,
						       xfer_data->ip_dst);
    if (filt_status == DPI_SESN_HTTP_LS_ACP) {
	nfq_set_verdict(xfer_data->qh, xfer_data->pkt_id, NF_ACCEPT, 0, NULL);
	nkn_dpi_filter_acp_cnt[thread_id]++;
    } else if (filt_status == DPI_SESN_HTTP_LS_REJ) {
	nfq_set_verdict2(xfer_data->qh, xfer_data->pkt_id,
			 NF_ACCEPT, 0xFFFFFFFF, 0, NULL);
	nkn_dpi_filter_rej_cnt[thread_id]++;
    } else if (filt_status == DPI_SESN_HTTP_UNK_ACP) {
	nfq_set_verdict(xfer_data->qh, xfer_data->pkt_id, NF_ACCEPT, 0, NULL);
	nkn_dpi_filter_unk_cnt[thread_id]++;
    } else if (!filt_status) {
	ret = nkn_dpi_eventhandler_link_pkt_to_session(ucnx,
						       xfer_data->qh,
						       xfer_data->pkt_id,
						       xfer_data->ip_src,
						       xfer_data->ip_dst,
						       thread_id);
	if (ret) {
	    /* 
	     * No session data 
	     * Its better to accept the packet 
	     */
	    nfq_set_verdict(xfer_data->qh, xfer_data->pkt_id, NF_ACCEPT,
			    0, NULL);
	    nkn_dpi_filter_unk_cnt[thread_id]++;
	}
    } else {
	assert(0);
    }
end:

    /* step 5 : context cleanup*/
    app_pktdata_exit(&app_pkt);

    /* step 6 : timer processing */
    app_timer_loop(app, &app_pkt);
    return 0;

}
#endif

inline
void nkn_dpi_process_pf_packet(unsigned char *buffer, void *user_data, int len,
			       uint32_t ip_src, uint32_t ip_dst, uint64_t thread_id)
{
    struct timeval ts;

    ts.tv_sec  = nkn_cur_ts * 100;
    ts.tv_usec = 0;
    dpi_pf_data_main_mod(buffer,
			 len,
			 &ts,
			 user_data,
			 &deng_data,
			 NULL, ip_src, ip_dst, thread_id);
}


extern pthread_t pd_thread[32];
void nkn_dpi_pf_set_cpu_id(uint64_t thread_id)
{
    uint64_t cpu_id[32] = { 0,1,2,3,4,5,6,7,16,17,18,19,20,21,22,23,8,9,10,11,12,13,14,15,24,25,26,27,28,29,30,31 };
//    cpu_set_t cpu_set;

    afc_set_cpuid(cpu_id[thread_id]);
    /* Set the CPU affinity */
//   CPU_ZERO(&cpu_set);
    //CPU_SET(cpu_id[thread_id], &cpu_set);
//    CPU_SET(cpu_id, &cpu_set);
    /* Set the CPU affinity */
//    pthread_setaffinity_np(pd_thread[thread_id], sizeof(cpu_set_t), &cpu_set);
}

inline void nkn_dpi_event_handle_init(uint64_t thread_id)
{
    uint64_t cpu_id;

    if (thread_id >= 32)
	cpu_id = thread_id - 32;
    else
	cpu_id = thread_id;

    afc_set_cpuid(cpu_id);
    return;
}

#ifdef NFQUEUE_URI_FILTER

inline
void nkn_dpi_process_packet(nfqueue_dpi_xfer_t *xfer_data, uint64_t thread_id)
{
    struct timeval ts;

    if (xfer_data->qh) {
	ts.tv_sec  = nkn_cur_ts * 100;
	ts.tv_usec = 0;
	dpi_data_main_mod(xfer_data->buffer,
			  xfer_data->pkt_len,
			  &ts,
			  (void *)xfer_data,
			  &deng_data,
			  NULL,
			  xfer_data,
			  thread_id);
    }
    free(xfer_data);
}

/*
 * Thread to process packeks from the NF thread and
 * invoke DPI module
 */
static void *nkn_dpi_thread(void *arg)
{
    int64_t thread_id = (int64_t)arg;
    nfqueue_dpi_xfer_t *xfer_data;
    char t_name[50];
    uint64_t cpu_id;
#ifdef PROCESS_IN_NF_THREAD
    char mem_pool_str[64];
#endif

    if (thread_id >= 32)
	cpu_id = thread_id - 32;
    else
	cpu_id = thread_id;

    snprintf(t_name, 50, "nkndpi-%ld", thread_id);
    prctl(PR_SET_NAME, t_name, 0, 0, 0);
#ifndef PROCESS_IN_NF_THREAD
    snprintf(mem_pool_str, sizeof(mem_pool_str), "dpimempool-%d",
	     thread_id);
    nkn_mem_create_thread_pool(mem_pool_str);
#endif

    afc_set_cpuid(cpu_id);
    while (1) {
	xfer_data = nkn_nf_dpi_get_entry(thread_id);
	if (xfer_data) {
	    nkn_dpi_process_packet(xfer_data, thread_id);
	}
    }

    return NULL;
}
#endif

/*
 * DPI related exception handlers
 */
static void
on_expire_session(DPI_ENGINE_SESSIONCB_PARAM(u, dev))
{
    void *user_handle = DPI_ENGINE_SESSIONCB_HANDLE(u);
    int thread_id;

    /* Get the thread id to reference into the global array */
    thread_id = afc_get_cpuid();

    // printf ("[%d]@on_expire_session (%p) \n", thread_id, user_handle);
    if(user_handle) {
	user_handle = NULL;
    }
} /* end of on_expire_session() */

static void
exception_handler(ctb_uint32 exception_type, void *args, void *params)
{
    (void)args;
    (void)params;
    if (exception_type == CTB_EXCEPTION_NO_MEMORY){
	DBG_LOG(ERROR, MOD_DPI_URIF, "ixengine exception: no memory!");
    }
} /* end of exception_handler() */


/*
 * DPI init functions
 */
static int
dpi_performance_tuning(void)
{
    //Performance tuning.
    clep_proto_t proto;

    PROTO_CLEAR(&proto);

    proto.unique_id = Q_PROTO_HTTP;
    //Instruct the DPI engine to ignore the http content
    //
    if (ctl_proto_set_proto_tune(&proto, "server_content_bypass", 1) < 0)
	DBG_LOG(ERROR, MOD_DPI_URIF,
		"Not able to tune the http protocol\n");
    if (ctl_proto_set_proto_tune(&proto, "disable_fast_cases", 1) < 0)
	DBG_LOG(ERROR, MOD_DPI_URIF,
		"Not able to tune the http protocol\n");

    /* Return */
    return 0;
} /* end of dpi_performance_tuning() */

/*
 * DPI init functions
 */
static int
nkn_dpi_engine_init(void)
{
    struct clep_config_params cp;
    clep_proto_t _proto;
    int err;


    /* Init the config parameters */
    config_params_init(&cp);

    //cp.td_maxcnx = 5000;

    if (uafc_init_params(&cp) < 0) {
	DBG_LOG(ERROR, MOD_DPI_URIF, "DPI init error");
        return -1;
    }
    if (ctl_proto_enable_all() < 0) { // enable all protocols ix DPI
	DBG_LOG(ERROR, MOD_DPI_URIF, "Error enabling protocols");
        return -1;
    }

    //Enable only the http protocol
    ctl_proto_disable_all();
    PROTO_CLEAR(&_proto);
    strcpy(_proto.name, "http");
    err = ctl_proto_enable(&_proto);
    if (err < 0) {
	DBG_LOG(ERROR, MOD_DPI_URIF, "Error enabling HTTP protocol");
	return -1;
    }

    afc_set_callback_expire_session(on_expire_session);
    afc_set_callback_exception(exception_handler, NULL);

    event_handler_init(); // initialize our event handler
    ALLOC_DPI_ENGINE_DATA_ENGINE(&deng_data, 256);
    dpi_engine_init_nr(&deng_data, 256);

    dpi_performance_tuning();
    return 0;

}

const namespace_config_t *catchall_nsconf = NULL;
static
void dpi_set_nsconf(const namespace_config_t *nsconf)
{
    catchall_nsconf = nsconf;
}

/*
 * Counter thread
 */
static
void *dpi_counter_thread(void *arg)
{
#define PF_COUNTER_DELAY 5
    uint64_t dpi_acp_cnt;
    uint64_t dpi_rej_cnt;
    uint64_t dpi_byt_cur_cnt;
    uint64_t dpi_byt_pre_cnt = 0;
    uint64_t dpi_pkt_bw;
    int thread_id;
    while (1) {
	dpi_acp_cnt = 0;
	dpi_rej_cnt = 0;
	dpi_pkt_bw = 0;
	dpi_byt_cur_cnt = 0;
	sleep(PF_COUNTER_DELAY);
	
	for (thread_id = 0; thread_id < MAX_NFQUEUE_THREADS; thread_id++) {
	    dpi_acp_cnt += nkn_dpi_filter_acp_cnt[thread_id];
	    dpi_rej_cnt += nkn_dpi_filter_rej_cnt[thread_id];
	    dpi_byt_cur_cnt += nkn_dpi_filter_bytes_processed[thread_id];
	}
	if (dpi_byt_pre_cnt && dpi_byt_cur_cnt) {
	    dpi_pkt_bw = ((dpi_byt_cur_cnt - dpi_byt_pre_cnt) * 8)/ PF_COUNTER_DELAY / 1000 / 1000;
	}
	if (catchall_nsconf) {
	    AO_store(&(catchall_nsconf->stat->urif_stats.rej_cnt), dpi_rej_cnt);
	    AO_store(&(catchall_nsconf->stat->urif_stats.acp_cnt), dpi_acp_cnt);
	    AO_store(&(catchall_nsconf->stat->urif_stats.bw), dpi_pkt_bw);
	}
	dpi_byt_pre_cnt = dpi_byt_cur_cnt;
    }
    return NULL;
}

/*
 * NKN DPI module init 
 * Called from server_init
 */
int nkn_dpi_init()
{
    uint64_t thread;
#ifdef NFQUEUE_URI_FILTER
    pthread_attr_t attr;
    pthread_t dpi_pt[32];
#endif
    pthread_t pd_counter_thread;
    int ret;
    cpu_set_t cpu_set;
    uint64_t cpu_id;
    char counter_str[512];

    ret = nkn_dpi_engine_init();
    if (ret < 0) {
	DBG_LOG(SEVERE, MOD_DPI_URIF, "DPI Engine init failed");
	return -1;
    }

    for (thread = 0; thread < MAX_NFQUEUE_THREADS; thread++) {
#ifdef NFQUEUE_URI_FILTER
	ret = pthread_attr_init(&attr);
	if (ret < 0) {
	    DBG_LOG(SEVERE, MOD_DPI_URIF,
		    "Failed to init pthread attribute");
	    return -1;
	}
#endif

	snprintf(counter_str, 512, "dpi_url_filter_rej_cnt_%ld", thread);
	(void)nkn_mon_add(counter_str, NULL,
			    (void *)&nkn_dpi_filter_rej_cnt[thread],
			    sizeof(nkn_dpi_filter_rej_cnt[thread]));
	snprintf(counter_str, 512, "dpi_url_filter_accept_cnt_%ld", thread);
	(void)nkn_mon_add(counter_str, NULL,
			    (void *)&nkn_dpi_filter_acp_cnt[thread],
			    sizeof(nkn_dpi_filter_acp_cnt[thread]));
	snprintf(counter_str, 512, "dpi_err_accept_cnt_%ld", thread);
	(void)nkn_mon_add(counter_str, NULL,
			    (void *)&nkn_dpi_filter_err_cnt[thread],
			    sizeof(nkn_dpi_filter_err_cnt[thread]));
	snprintf(counter_str, 512, "dpi_unk_accept_cnt_%ld", thread);
	(void)nkn_mon_add(counter_str, NULL,
			    (void *)&nkn_dpi_filter_unk_cnt[thread],
			    sizeof(nkn_dpi_filter_unk_cnt[thread]));


#ifdef NFQUEUE_URI_FILTER
	pthread_create(&dpi_pt[thread], &attr, nkn_dpi_thread,
		       (void *)thread);
	if (thread >= 32)
	    cpu_id = thread - 32;
	else
	    cpu_id = thread;
	/* Set the CPU affinity */
	CPU_ZERO(&cpu_set);
	CPU_SET(cpu_id, &cpu_set);
	/* Set the CPU affinity */
	pthread_setaffinity_np(dpi_pt[thread], sizeof(cpu_set_t), &cpu_set);
#endif
    }
    pthread_create(&pd_counter_thread, NULL, dpi_counter_thread, NULL);
    return 0;
}

/*
 * This function is called by HTTP module.
 */
void dpi_accesslog_write(char *uri,
			 int urilen,
			 char *host,
			 int hostlen,
			 uint64_t method,
			 uint32_t ip_src,
			 uint32_t ip_dst,
			 uint64_t sub_errcode)
{
	al_info_t *pinfo = NULL;
	NLP_data *pdata = NULL;
	struct accesslog_item *item = NULL;
	char *p = NULL;
	char *ps = NULL;
	char *pname = NULL;
	const char *pvalue = NULL;
	int32_t pvalue_len = 0;
	int32_t list_len = 0;
	int32_t buf_len = 0;
	int32_t i = 0;
	static al_info_t *ali = NULL;
	int rv = 0 ;
	int token = 0;
	const namespace_config_t *nsconf;
	namespace_token_t ns_token = NULL_NS_TOKEN;
	static char namespace_name[512];
	static int namespace_name_len = 0;
	mime_header_t mh;
	static char server_name[256];
	static int server_name_len = 0;

	if (ali == NULL) {
		rv = mime_hdr_init(&mh, MIME_PROT_HTTP, 0, 0);

		if (rv) {
			DBG_LOG(MSG, MOD_DPI_URIF,
				"mime_hdr_init() failed, rv=%d", rv);
			return;
		}

		rv = mime_hdr_add_known(&mh, MIME_HDR_HOST, host, hostlen);
		if (rv) {
			DBG_LOG(MSG, MOD_DPI_URIF,
				"mime_hdr_add_known(MIME_HDR_HOST) failed, "
				"rv=%d host=%p hostlen=%d", rv, host, hostlen);
			return;
		}

		rv = mime_hdr_add_known(&mh, MIME_HDR_X_NKN_URI,
					uri, urilen);
		if (rv) {
			DBG_LOG(MSG, MOD_DPI_URIF,
				"mime_hdr_add_known(MIME_HDR_X_NKN_URI) failed,"
				" rv=%d uri_abs_path=%p uri_abs_pathlen=%d",
				rv, uri, urilen);
			return;
		}

		if (server_name_len == 0) {
		    server_name[0] = '\0';
		    gethostname(server_name, sizeof(server_name));
		    server_name_len = strlen(server_name);
		}

		ns_token = http_request_to_namespace(&mh, &nsconf);
		if (!VALID_NS_TOKEN(ns_token)) {
			DBG_LOG(MSG, MOD_DPI_URIF,
			    "No namespace, http_request_to_namespace() failed");
			return;
		}
		if (nsconf && nsconf->acclog_config->name) {
			if (nsconf->namespace_strlen < 512) {
				memcpy(namespace_name, nsconf->namespace,
				       nsconf->namespace_strlen);
				namespace_name[nsconf->namespace_strlen] = '\0';
				namespace_name_len = nsconf->namespace_strlen;
			} else {
				memcpy(namespace_name, nsconf->namespace,
				       (512 - 1));
				namespace_name[512-1] = '\0';
				namespace_name_len = 512;
			}

			// Lookup AccLog Info
			ALP_HASH_RLOCK();
			ali = nhash_lookup(al_profile_tbl,
					   nsconf->acclog_config->keyhash,
					   nsconf->acclog_config->key);
			ALP_HASH_RUNLOCK();
		}

#if 0
		/* Do not release the nstoken as the nsconf is used
		 * for statistics update
		 */
		if (VALID_NS_TOKEN(ns_token)) {
		    release_namespace_token_t(ns_token);
		}
#else
		dpi_set_nsconf(nsconf);
#endif

		if (ali == NULL)
			return;
	}


	glob_al_tot_logged++;

	ALI_HASH_WLOCK();
	if (ali->in_use == 0) {
	    ALI_HASH_WUNLOCK();
	    return;
	}

	buf_len = MAX_ACCESSLOG_BUFSIZE - ali->al_buf_end;

	if (buf_len < (int)(sizeof(NLP_data) + sizeof(struct accesslog_item))) {
	    glob_al_err_buf_overflow++;
	    goto skip_dpi_logit;
	}

	pinfo = ali;
	pdata = (NLP_data *) &(ali->al_buf[ali->al_buf_end]);
	ps = (char *)(pdata) + sizeof(NLP_data);
	pdata->channelid = pinfo->channelid;
	buf_len -= (sizeof(NLP_data) + sizeof(struct accesslog_item));
	item = (struct accesslog_item *) ps;


	gettimeofday(&(item->start_ts), NULL);
	item->end_ts.tv_sec =	    item->start_ts.tv_sec;
	item->end_ts.tv_usec =	    item->start_ts.tv_usec;
	item->ttfb_ts.tv_sec =	    0;
	item->ttfb_ts.tv_usec =	    0;
	item->in_bytes =	    0;
	item->out_bytes =	    0;
	item->resp_hdr_size =	    0;
	item->status =		    0;
	item->subcode =		    sub_errcode;
	item->dst_port =	    0;
	item->dst_ipv4v6.family =   AF_INET;
	item->dst_ipv4v6.addr.v4.s_addr = ip_dst;
	item->src_ipv4v6.family =   AF_INET;
	item->src_ipv4v6.addr.v4.s_addr = ip_src;
	item->flag =		    0;

	if( method == HRF_METHOD_GET) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_GET);
	} else if( method == HRF_METHOD_HEAD) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_HEAD);
	} else if( method == HRF_METHOD_POST) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_POST);
	} else if( method == HRF_METHOD_CONNECT) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_CONNECT);
	} else if( method == HRF_METHOD_OPTIONS) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_OPTIONS);
	} else if( method == HRF_METHOD_PUT) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_PUT);
	} else if( method == HRF_METHOD_DELETE) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_DELETE);
	} else if( method == HRF_METHOD_TRACE) {
	    SET_ALITEM_FLAG(item, ALF_METHOD_TRACE);
	}

	SET_ALITEM_FLAG(item, ALF_HTTP_11);

	p = ps + sizeof(struct accesslog_item);
	for (i=0; i < pinfo->num_al_list; i++) {
	    pvalue = NULL;
	    list_len = pinfo->al_listlen[i];

	    switch (*(unsigned int *)(&(pinfo->al_list[i][0]))) {
	    case 0x6972752e:
		if (method ==  HRF_METHOD_CONNECT) {
		    /* It is CONNECT method */
		    pvalue = NULL;
		    pvalue_len = 0;
		} else {
		    pvalue = uri;
		    pvalue_len = urilen;
		}
		break;
	    case 0x6c71722e:    /* ".rql" */
		pvalue = NULL;
		pvalue_len = 0;
		break;
	    //provide namespace info, only name at this point
	    case 0x70736e2e:	/* ".nsp" */
		if (namespace_name_len) {
		    pvalue = namespace_name;
		    pvalue_len = namespace_name_len;
		}
		break;
	    //find MFC name or servername
	    case 0x6d6e732e:	/* ".snm" */
		if (server_name_len) {
		    pvalue_len = server_name_len;
		    pvalue = server_name;
		}
		break;
	    case 0x7165722e:	/* ".req" */
		pvalue_len = 0;
		pvalue = NULL;
		pname = &pinfo->al_list[i][4];
		list_len -= 4;
		token = pinfo->al_token[i];
		if (token >= 0) {
		    /*
		    * some tokens have been deleted from get_known_header.
		    * But they are still stored in namevalue_header.
		    */
		    switch (token) {
			case MIME_HDR_HOST:
			    pvalue = host;
			    pvalue_len = hostlen;
			break;
		    }
		}
		break;
	    //find filename
	    case 0x6d6e662e:	/* ".fnm" */
		pvalue = NULL;
		pvalue_len = 0;
		if (method != HRF_METHOD_CONNECT) {
		    const char *temp;
		    int temp_len, j;
		    char c;
		    temp = uri;
		    temp_len = urilen;
		    if (temp_len > 1) {
			for(j = temp_len - 1; j >= 0; j--) {
			    c = temp[j];
			    if(c == '/')
				break;
			}
			pvalue_len = temp_len - (j+1); //Don't print /
			pvalue = temp + j + 1 ;
		    }
		}
		break;
	    }

	    /*
	    * if yes we found pvalue, copy it.
	    * if not, we return "-"
	    */
	    if (pvalue) {
		if (buf_len < pvalue_len+1) {
		    glob_al_err_buf_overflow++;
		    goto skip_dpi_logit;
		}
		buf_len -= pvalue_len+1;
		memcpy(p, pvalue, pvalue_len);
		p[pvalue_len] = '\0';
		p += pvalue_len + 1;
	    } else {
		if (buf_len < 2) {
		    glob_al_err_buf_overflow++;
		    goto skip_dpi_logit;
		}
		buf_len -= 2;
		*p++ = '-';
		*p++ = 0;
	    }
	}

	pdata->len = p - ps;
	ali->al_buf_end += (pdata->len + sizeof(NLP_data));

skip_dpi_logit:
	if (ali->al_buf_end > (MAX_ACCESSLOG_BUFSIZE / 4)) {
	    NM_add_event_epollout(ali->al_fd);
	}

	ALI_HASH_WUNLOCK();
	return;
}

#endif /* _STUB_ */
