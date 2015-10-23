/*
 * cl_node_status.c - Handler for Cluster Membership Manager (CMM) messages
 */

#define ENABLE_TRIGGER_SNMP_EVENT 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <alloca.h>
#include <assert.h>
#include <sys/queue.h>

#include "cl.h"
#include "cl_node_status.h"
#include "fqueue.h"
#include "http_header.h"
#include "nkn_cmm_request.h"
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_errno.h"
#include "nkn_stat.h"
#include "nkn_cfg_params.h"
#include "nkn_namespace.h"

#ifdef ENABLE_TRIGGER_SNMP_EVENT
#include "mdc_wrapper.h"
#endif /* ENABLE_TRIGGER_SNMP_EVENT */

static pthread_t ns_thread_id;
static fqueue_element_t qe;

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
        memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

NKNCNT_DEF(nodestatus_gattr1_err, int64_t, "", "Node status FQueue getattr err")
NKNCNT_DEF(nodestatus_data_dsz_err, int64_t, "", "Node status deserialize err")
NKNCNT_DEF(nodestatus_msgfmt_err, int64_t, "", "Node status msg fmt err")
NKNCNT_DEF(nodestatus_hdldsz_err, int64_t, "", "Node status hdl dsz err")
NKNCNT_DEF(nodestatus_ns_err, int64_t, "", "Node status namespace map err")
NKNCNT_DEF(nodestatus_no_nmap, int64_t, "", "Node status no node map")
NKNCNT_DEF(nodestatus_update_err, int64_t, "", "Node status, node update err")

NKNCNT_DEF(nodestatus_resend_msgs, int64_t, "", "Node status resend msgs")
NKNCNT_DEF(nodestatus_online_msgs, int64_t, "", "Node status online msgs")
NKNCNT_DEF(nodestatus_offline_msgs, int64_t, "", "Node status offline msgs")

#ifdef ENABLE_TRIGGER_SNMP_EVENT
extern md_client_context *nvsd_mcc;
#endif

static int handle_resend_monitor(void)
{
    int n;
    int active_namespaces;
    namespace_token_context_t ctx;
    namespace_token_t token;
    const namespace_config_t *nsc;
    char name[128];
    int rv;

    active_namespaces = get_active_namespace_token_ctx(&ctx);

    for (n = 0; n < active_namespaces; n++) {
    	token = get_nth_active_namespace_token(&ctx, n);
	nsc = namespace_to_config(token);
	if (nsc) {
	    strncpy(name, nsc->namespace, sizeof(name));
	    rv = CL_namspace_update(token, nsc, 1);
	    release_namespace_token_t(token);
	    if (!rv) {
    	    	DBG_LOG(MSG, MOD_CLUSTER, 
		    	"namespace=[%s] token=%u,%u (%ld) index=%d", 
		    	name, token.u.token_data.gen, token.u.token_data.val, 
		    	token.u.token, n);
	    }
    	}
    }
    return 0;
}

#ifdef ENABLE_TRIGGER_SNMP_EVENT
static int trigger_snmp_event(const char *namespace, int cluster_instance,
			      const char *node, int online)
{
    int ret = 0;
    int err;
    const char *event_node = NULL;
    bn_request *req = NULL;
    char valstr[64];
    int rv;
    gcl_session *sess;

    while (1) {

    lc_log_basic(LOG_NOTICE, "node: %s under namespace: %s, cluster instance: %d "
	    "is %s", node, namespace, cluster_instance, online ? "up": "down");
    if (online) {
	event_node = "/nkn/nvsd/cluster/node_up";
    } else {
	event_node = "/nkn/nvsd/cluster/node_down";
    }
    err = bn_event_request_msg_create(&req, event_node);
    if (err) {
    	ret = 1;
    	break;
    }
    err = bn_event_request_msg_append_new_binding(req, 0, 
						  "/nkn/monitor/cluster-namespace", bt_string,
						  namespace, NULL);
    if (err) {
    	ret = 2;
    	break;
    }
    rv = snprintf(valstr, sizeof(valstr), "%d", cluster_instance);
    err = bn_event_request_msg_append_new_binding(req, 0, 
						  "/nkn/monitor/cluster-instance", bt_int32,
						  valstr, NULL);
    if (err) {
    	ret = 3;
    	break;
    }
    err = bn_event_request_msg_append_new_binding(req, 0, 
						  "/nkn/monitor/nodename", bt_string,
						  node, NULL);
    if (err) {
    	ret = 4;
    	break;
    }

    if (nvsd_mcc) {
    	sess = mdc_wrapper_get_gcl_session(nvsd_mcc);
	if (sess) {
	    err = bn_request_msg_send(sess, req);
	    if (err) {
	    	ret = 6;
	    }
	} else {
	    ret = 7;
	}
    } else {
    	ret = 8;
    }
    break;

    } // End of while

    if (err) {
    	DBG_LOG(MSG, MOD_CLUSTER, "trigger_snmp_event(), err=%d ret=%d",
		err, ret);
    }

    if (req) {
    	bn_request_msg_free(&req);
    }

    return ret;
}
#endif /* ENABLE_TRIGGER_SNMP_EVENT */

static int handle_node_status(int online, const fqueue_element_t *qe_l)
{
    int rv;
    int retval = 0;
    int n;
    const char *data;
    int datalen;
    int nodes;
    const char *node_handle[MAX_CMM_DATA_ELEMENTS];
    int node_handle_strlen[MAX_CMM_DATA_ELEMENTS];

    namespace_token_t token;
    int instance;
    char host_port[1024];
    const namespace_config_t *nsc;
    node_map_descriptor_t *nmd;
    char *pp;

    rv = get_nvattr_fqueue_element_by_name(qe_l, REQ_NVSD_DATA, 
					   REQ_NVSD_DATA_STRLEN,
					   &data, &datalen);
    if (rv) {
    	DBG_LOG(MSG, MOD_CLUSTER, 
		"get_nvattr_fqueue_element_by_name(REQ_NVSD_DATA) failed, "
		"rv=%d online=%d", rv, online);
	retval = 1;
	glob_nodestatus_gattr1_err++;
	goto exit;
    }

    rv = nvsd_request_data_deserialize(data, datalen, &nodes,
				       node_handle, node_handle_strlen);
    if (rv) {
    	DBG_LOG(MSG, MOD_CLUSTER, 
		"nvsd_request_data_deserialize() failed, "
		"rv=%d data=%p datalen=%d online=%d", 
		rv, data, datalen, online);
	retval = 2;
	glob_nodestatus_data_dsz_err++;
	goto exit;
    }

    for (n = 0; n < nodes; n++) {
        if (node_handle_strlen[n] >= (int) sizeof(host_port)) {
	    // Bad message, ignore it
	    DBG_LOG(MSG, MOD_CLUSTER, 
	    	    "node_handle_strlen[%d](%d) >= sizeof(host_port)(%ld), "
		    "request ignored", n, node_handle_strlen[n], 
		    sizeof(host_port));
	    glob_nodestatus_msgfmt_err++;
	    continue;
	}

    	rv = cmm_node_handle_deserialize(node_handle[n], node_handle_strlen[n],
					 &token.u.token, &instance, host_port);
	if (rv) {
	    TSTR(node_handle[n], node_handle_strlen[n], node_handle_str);
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "cmm_node_handle_deserialize() failed, rv=%d n=%d "
		    "online=%d node_handle=%s, request ignored", 
		    rv, n, online, node_handle_str);
	    glob_nodestatus_hdldsz_err++;
	    continue;
	}

	nsc = namespace_to_config(token);
	if (!nsc) {
	    TSTR(node_handle[n], node_handle_strlen[n], node_handle_str);
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "namespace_to_config() failed, n=%d online=%d "
		    "node_handle=%s, request ignored", 
		    n, online, node_handle_str);
	    glob_nodestatus_ns_err++;
	    continue;
	}

	if (nsc->http_config && 
	    (instance < nsc->http_config->origin.u.http.node_map_entries)) {
	    nmd = nsc->http_config->origin.u.http.node_map[instance];
	} else if (nsc->cluster_config && nsc->cluster_config->num_cld && 
		   ((instance >= CLUSTER_NSC_INSTANCE_BASE) &&
		    (instance < (CLUSTER_NSC_INSTANCE_BASE + 
		    		 nsc->cluster_config->num_cld)))) {
	    CLD2NODE_MAP(
	    	nsc->cluster_config->cld[instance - CLUSTER_NSC_INSTANCE_BASE], 
		nmd);
	} else {
	    nmd = NULL;
	    TSTR(node_handle[n], node_handle_strlen[n], node_handle_str);
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "No NodeMap n=%d online=%d "
		    "node_handle=%s, request ignored", 
		    n, online, node_handle_str);
	    glob_nodestatus_no_nmap++;
	}

	if (nmd) {
	    // Invoke node status callout
	    pp = host_port;
	    rv = (*nmd->node_update)(&nmd->ctx, online, 1, &pp,
				     &node_handle[n], &node_handle_strlen[n]);
	    if (!rv) {
	    	TSTR(node_handle[n], node_handle_strlen[n], node_handle_str);
	    	DBG_LOG(MSG, MOD_CLUSTER, 
			"token=%u,%u (%ld) instance=%d node_handle=%s "
			"online=%d",
			token.u.token_data.gen, token.u.token_data.val,
			token.u.token,
			instance, node_handle_str, online);

#ifdef ENABLE_TRIGGER_SNMP_EVENT
		rv = trigger_snmp_event(nsc->namespace, instance,
					node_handle_str, online);
		if (rv) {
		    DBG_LOG(MSG, MOD_CLUSTER, 
			    "trigger_snmp_event() failed, rv=%d "
			    "token=%u,%u (%ld) instance=%d node_handle=%s "
			    "online=%d",
			    rv, token.u.token_data.gen, token.u.token_data.val,
			    token.u.token,
			    instance, node_handle_str, online); 
		}
#endif
	    } else {
	    	TSTR(node_handle[n], node_handle_strlen[n], node_handle_str);
	    	DBG_LOG(MSG, MOD_CLUSTER, 
		    	"(*node_update)() failed, n=%d online=%d "
		    	"node_handle=%s, request ignored", 
		    	n, online, node_handle_str);
	    	glob_nodestatus_update_err++;
	    }
	}
	release_namespace_token_t(token);
    }

exit:

    return retval;
}

static int process_node_status_request(const fqueue_element_t *qe_l)
{
    int rv;
    const char *data;
    int datalen;

    rv = get_nvattr_fqueue_element_by_name(qe_l, REQ_NVSD_RESEND, 
					   REQ_NVSD_RESEND_STRLEN,
					   &data, &datalen);
    if (!rv) {
    	glob_nodestatus_resend_msgs++;
    	return handle_resend_monitor();
    }

    rv = get_nvattr_fqueue_element_by_name(qe_l, REQ_NVSD_ONLINE, 
					   REQ_NVSD_ONLINE_STRLEN,
					   &data, &datalen);
    if (!rv) {
    	glob_nodestatus_online_msgs++;
    	return handle_node_status(1, qe_l);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_l, REQ_NVSD_OFFLINE, 
					   REQ_NVSD_OFFLINE_STRLEN,
					   &data, &datalen);
    if (!rv) {
    	glob_nodestatus_offline_msgs++;
    	return handle_node_status(0, qe_l);
    }
    return 0;
}

static void *ns_req_func(void *arg)
{
    int rv;
    fhandle_t fh;
    struct pollfd pfd;

    UNUSED_ARGUMENT(arg);
    prctl(PR_SET_NAME, "nvsd-node", 0, 0, 0);

    // Create input fqueue
    do {
    	rv = create_recover_fqueue(node_status_queuefile, 
				   node_status_queue_maxsize, 
				   1 /* delete old entries */);
	if (rv) {
	    DBG_LOG(SEVERE, MOD_CLUSTER,
		    "Unable to create input queuefile [%s], rv=%d",
		    fmgr_queuefile, rv);
	    sleep(10);
	}
    } while (rv);

    // Open input fqueue
    do {
    	fh = open_queue_fqueue(node_status_queuefile);
	if (fh < 0) {
	    DBG_LOG(SEVERE, MOD_CLUSTER, "Unable to open input queuefile [%s]", 
		    node_status_queuefile);
	    sleep(10);
	}
    } while (fh < 0);

    while (1) {
    	rv = dequeue_fqueue_fh_el(fh, &qe, 1);
	if (rv == -1) {
	    continue;
	} else if (rv) {
	    DBG_LOG(SEVERE, MOD_CLUSTER, 
	    	    "dequeue_fqueue_fh_el(%s) failed, rv=%d", 
		    node_status_queuefile, rv);
	    poll(&pfd, 0, 100);
	    continue;
	}
	rv = process_node_status_request(&qe);
	if (rv) {
	    DBG_LOG(SEVERE, MOD_CLUSTER,
		    "process_node_status_request() failed, rv=%d", rv);
	}
    }
    return 0;
}

/*
 * cl_node_status_init() - system initialization
 *	Return: 0 => Success
 */
int cl_node_status_init(void)
{
    int rv;
    pthread_attr_t attr;
    int stacksize = 512 * 1024; // 512MB for libketama

    rv = pthread_attr_init(&attr);
    if (rv) {
	DBG_LOG(SEVERE, MOD_CLUSTER, "pthread_attr_init() failed, rv=%d", rv);
	return 1;
    }

    rv = pthread_attr_setstacksize(&attr, stacksize);
    if (rv) {
	DBG_LOG(SEVERE, MOD_CLUSTER, 
		"pthread_attr_setstacksize() failed, rv=%d", rv);
	return 2;
    }

    if ((rv = pthread_create(&ns_thread_id, &attr, ns_req_func, NULL))) {
	DBG_LOG(SEVERE, MOD_CLUSTER, "pthread_create() failed, rv=%d", rv);
	return 3;
    }
    return 0;
}

/*
 * End of cl_node_status.c
 */
