/*
 * cl_cmm.c - Push requests to Cluster Membership Manager (CMM) via FQueue
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <alloca.h>
#include <assert.h>
#include <sys/queue.h>

#include "cl_cmm.h"
#include "fqueue.h"
#include "http_header.h"
#include "nkn_cmm_request.h"
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_errno.h"
#include "nkn_stat.h"
#include "nkn_cfg_params.h"
#include "nkn_namespace.h"

NKNCNT_DEF(cmm_tot_reqs, int64_t, "", "CMM total requests")
NKNCNT_DEF(cmm_err_dropped, int64_t, "", "dropped CMM requests")

NKNCNT_DEF(cmm_sndcmm_qeinit_err, int64_t, "", "Snd CMM qele init queue errror")
NKNCNT_DEF(cmm_sndcmm_addattr1_err, int64_t, "", "Snd CMM add attr1 err ")
NKNCNT_DEF(cmm_sndcmm_addattr2_err, int64_t, "", "Snd CMM add attr2 err")
NKNCNT_DEF(cmm_sndcmm_srz_err, int64_t, "", "CMM hdr serialize error")
NKNCNT_DEF(cmm_sndcmm_addattr3_err, int64_t, "", "Snd CMM add attr3 err")
NKNCNT_DEF(cmm_sndcmm_addattr4_err, int64_t, "", "Snd CMM add attr4 err")
NKNCNT_DEF(cmm_sndcmm_alloc_err, int64_t, "", "CMM malloc error")

NKNCNT_DEF(cmm_sndmon_ns_err, int64_t, "", "Snd Mon namespace err")
NKNCNT_DEF(cmm_sndmon_hd_err, int64_t, "", "Snd Mon handle err")
NKNCNT_DEF(cmm_sndmon_srz_err, int64_t, "", "Snd Mon serialize hdr err ")
NKNCNT_DEF(cmm_sndmon_sndcmm_err, int64_t, "", "Snd Mon send err")

NKNCNT_DEF(cmm_sndcan_hd_err, int64_t, "", "Snd Can handle err")
NKNCNT_DEF(cmm_sndcan_srz_err, int64_t, "", "Snd Can serialize err")
NKNCNT_DEF(cmm_sndcan_sndcmm_err, int64_t, "", "Snd Can send err")

typedef struct cmm_request {
    fqueue_element_t fq_element;
    SIMPLEQ_ENTRY(cmm_request) queue;
} cmm_request_t;

SIMPLEQ_HEAD(cmm_req_queue, cmm_request);
struct cmm_req_queue cmm_req_head;

static pthread_t cmm_thread_id;
static pthread_mutex_t cmm_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cmm_queue_cv;
static int cmm_thread_wait;

static void *cmm_thread_func(void *arg)
{
    int rv;
    fhandle_t fh;
    cmm_request_t *cmm_req = 0;
    int retries = 0;
    prctl(PR_SET_NAME, "nvsd-cmm", 0, 0, 0);

    UNUSED_ARGUMENT(arg);

    // Open fqueue
    do {
    	fh = open_queue_fqueue(cmm_queuefile);
	if (fh < 0) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "Unable to open queuefile [%s]", cmm_queuefile);
	    sleep(2);

	    if (((++retries) % 5) == 0) {
	    	// Create FQueue to avoid wait
	    	rv = create_recover_fqueue(cmm_queuefile, 1024,
					   1 /* delete old entries */);
		if (!rv) {
		    DBG_LOG(MSG, MOD_CLUSTER, 
			    "Created queuefile [%s]", cmm_queuefile);
		}
	    }
	}
    } while (fh < 0);

    // Processing loop
    pthread_mutex_lock(&cmm_queue_mutex);
    while (1) {
	SIMPLEQ_FOREACH(cmm_req, &cmm_req_head, queue)
	{
	    SIMPLEQ_REMOVE(&cmm_req_head, cmm_req, cmm_request, queue);
	    break;
	}
	if (cmm_req) {
    	    pthread_mutex_unlock(&cmm_queue_mutex);

	    retries = cmm_queue_retries;
	    while (retries--) {
	    	rv = enqueue_fqueue_fh_el(fh, &cmm_req->fq_element);
	    	if (!rv) {
		    glob_cmm_tot_reqs++;
		    break;
		} else if (rv == -1) {
		    if (retries) {
		    	sleep(cmm_queue_retry_delay);
		    }
		} else {
		    glob_cmm_err_dropped++;
	    	    DBG_LOG(MSG, MOD_CLUSTER, 
			    "enqueue_fqueue_fh_el(fd=%d) failed rv=%d, "
			    "Dropping fqueue_element_t", fh, rv);
		    break;
	    	}
	    }
	    free(cmm_req);
    	    pthread_mutex_lock(&cmm_queue_mutex);
	} else {
	    cmm_thread_wait = 1;
            pthread_cond_wait(&cmm_queue_cv, &cmm_queue_mutex);
	    cmm_thread_wait = 0;
	}
    }
    pthread_mutex_unlock(&cmm_queue_mutex);

    return NULL;
}

static int send_cmm_request(const char *cmm_req_type, int cmm_req_type_strlen,
			    const char *ns_name, int ns_name_strlen,
			    const mime_header_t *hdr,
			    const char *data, int data_strlen)
{
    int retval = 0;
    int rv;
    cmm_request_t *cmm_req;
    fqueue_element_t *qe;
    char *serialbuf;
    int serialbufsz;

    cmm_req = (cmm_request_t *)nkn_malloc_type(sizeof(cmm_request_t), 
    					       mod_cl_cmm_req_t);
    if (cmm_req) {
    	qe = &cmm_req->fq_element;

    	rv = init_queue_element(qe, 0, "", "http://cmm");
	if (rv) {
	    DBG_LOG(MSG, MOD_CLUSTER, "init_queue_element() failed, rv=%d", rv);
	    retval = 1;
	    glob_cmm_sndcmm_qeinit_err++;
	    goto exit;
	}

	rv = add_attr_queue_element_len(qe, cmm_req_type, cmm_req_type_strlen,
					"1", 1);
	if (rv) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
	    	    "add_attr_queue_element_len() failed, rv=%d req_type=%s",
		    rv, cmm_req_type);
	    retval = 2;
	    glob_cmm_sndcmm_addattr1_err++;
	    goto exit;
	}

	if (ns_name) {
	    rv = add_attr_queue_element_len(qe, REQ_CMM_NAMESPACE,
					    REQ_CMM_NAMESPACE_STRLEN,
					    ns_name, ns_name_strlen);
	    if (rv) {
	    	DBG_LOG(MSG, MOD_CLUSTER, 
	    	    	"add_attr_queue_element_len(REQ_CMM_NAMESPACE) "
			"failed, "
			"rv=%d data=%p datalen=%d", 
			rv, ns_name, ns_name_strlen);
	    	retval = 21;
	    	glob_cmm_sndcmm_addattr2_err++;
	    	goto exit;
	    }
	}

	if (hdr) {
	    serialbufsz = serialize_datasize(hdr);
	    serialbuf = (char *)alloca(serialbufsz);
	    rv = serialize(hdr, serialbuf, serialbufsz);
	    if (rv) {
	    	DBG_LOG(MSG, MOD_CLUSTER, 
			"serialize() failed, rv=%d serialbufsz=%d", 
			rv, serialbufsz);
	    	retval = 3;
	    	glob_cmm_sndcmm_srz_err++;
	    	goto exit;
	    }
	    rv = add_attr_queue_element_len(qe, REQ_CMM_HTTP_HEADER, 
					    REQ_CMM_HEADER_STRLEN,
					    serialbuf, serialbufsz);
	    if (rv) {
	    	DBG_LOG(MSG, MOD_CLUSTER, 
	    	    	"add_attr_queue_element_len(REQ_CMM_HTTP_HEADER) "
			"failed, "
			"rv=%d data=%p datalen=%d", 
			rv, serialbuf, serialbufsz);
	    	retval = 4;
	    	glob_cmm_sndcmm_addattr3_err++;
	    	goto exit;
	    }
	}

	if (data) {
	    rv = add_attr_queue_element_len(qe, REQ_CMM_DATA, 
					    REQ_CMM_DATA_STRLEN,
					    data, data_strlen);
	    if (rv) {
	    	DBG_LOG(MSG, MOD_CLUSTER, 
	    	    	"add_attr_queue_element_len(REQ_CMM_DATA) failed, "
			"rv=%d "
			"data=%p datalen=%d", rv, data, data_strlen);
	    	retval = 5;
	    	glob_cmm_sndcmm_addattr4_err++;
	    	goto exit;
	    }
	}

	// Enqueue request to CMM push thread
	pthread_mutex_lock(&cmm_queue_mutex);
	SIMPLEQ_INSERT_TAIL(&cmm_req_head, cmm_req, queue);
	if (cmm_thread_wait) {
	    pthread_cond_signal(&cmm_queue_cv);
	}
	pthread_mutex_unlock(&cmm_queue_mutex);
    } else {
	DBG_LOG(MSG, MOD_CLUSTER, "nkn_malloc_type(mod_cl_cmm_req) failed");
	retval = 7;
	glob_cmm_sndcmm_alloc_err++;
	goto exit;
    }

exit:

    return retval;
}

/*
 * send_cmm_monitor() - Push monitor nodes message to CMM Fqueue
 *
 *	Input:
 *	  nodes - Nodes in configuration
 *	  host_port - host:port sring
 *	  host_port_strlen - strlen
 *	  heartbeat_path - path portion of the heartbeat URL
 *	  heartbeat_path_strlen - strlen
 *	  hdr - Headers to send in heartbeat request
 *	  token - Associated token
 *	  instance - Associated instance
 *
 * 	Returns: 0 => Success
 */
int send_cmm_monitor(int nodes, const char **host_port, 
		     const int **host_port_strlen,
		     const char **heartbeat_path,
		     const int **heartbeat_path_strlen,
		     namespace_token_t token, int instance)
{
    int rv;
    int retval = 0;
    int total_bytes;
    int bytes_used;
    int n;
    char node_handle[MAX_CMM_NODE_HANDLE_LEN];
    char heartbeat_url[2048];
    char *buf;
    mime_header_t hdr;

    cmm_node_config_t *cmm_config;
    const namespace_config_t *nsc = NULL;
    struct node_map_descriptor *nmp;

    mime_hdr_init(&hdr, MIME_PROT_HTTP, NULL, 0);

    nsc = namespace_to_config(token);
    if (!nsc) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"namespace_to_config(token=%ld) failed", token.u.token);
	retval = 1;
	glob_cmm_sndmon_ns_err++;
	goto err_exit;
    }

    cmm_config = alloca(nodes * sizeof(cmm_node_config_t));
    memset(cmm_config, 0, nodes * sizeof(cmm_node_config_t));

    // Estimate data buffer size
    total_bytes = 0;
    for (n = 0; n < nodes; n++) {
    	total_bytes += (*host_port_strlen[n] + *heartbeat_path_strlen[n] + 512);
    }
    buf = alloca(total_bytes);
    bytes_used = 0;

    for (n = 0; n < nodes; n++) {
	// Build node handle
    	rv = cmm_node_handle(token.u.token, instance, host_port[n], 
			     node_handle, sizeof(node_handle));
	if (rv) {
	    DBG_LOG(MSG, MOD_CLUSTER, "cmm_node_handle() failed, rv=%d", rv);
	    retval = 2;
	    glob_cmm_sndmon_hd_err++;
	    goto err_exit;
	}

	// Build HTTP heartbeat URL
	rv = snprintf(heartbeat_url, sizeof(heartbeat_url),
		      "http://%s%s", host_port[n], heartbeat_path[n]);
	heartbeat_url[sizeof(heartbeat_url)-1] = '\0';

        cmm_config[n].node_handle = node_handle;
        cmm_config[n].heartbeaturl = heartbeat_url;

	if (instance < CLUSTER_NSC_INSTANCE_BASE) {
	    nmp = nsc->http_config->origin.u.http.node_map[instance];
	} else {
	    CLD2NODE_MAP(
		nsc->cluster_config->cld[instance-CLUSTER_NSC_INSTANCE_BASE], 
		nmp);
	}
        cmm_config[n].heartbeat_interval = nmp->cmm_heartbeat_interval;
        cmm_config[n].connect_timeout = nmp->cmm_connect_timeout;
        cmm_config[n].allowed_connect_failures = 
	    nmp->cmm_allowed_connect_failures;
        cmm_config[n].read_timeout = nmp->cmm_read_timeout;

	// Serialize entry
	rv = cmm_request_monitor_data_serialize(&cmm_config[n], 
					&buf[bytes_used], 
					total_bytes - bytes_used);
	if (rv >= (total_bytes - bytes_used)) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "cmm_request_data_serialize() buffer size exceeded, "
		    "rv=%d bufavail=%d n=%d", rv, total_bytes - bytes_used, n);
	    retval = 3;
	    glob_cmm_sndmon_srz_err++;
	    goto err_exit;
	}
	bytes_used += rv;
    }

    rv = send_cmm_request(REQ_CMM_MONITOR, REQ_CMM_MONITOR_STRLEN, 
			  nsc->namespace, nsc->namespace_strlen,
			  &hdr, buf, bytes_used);
    if (!rv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"send_cmm_request() ns=%s token=%u,%u (%ld) instance=%d",
		nsc->namespace,
		token.u.token_data.gen, token.u.token_data.val, token.u.token, 
		instance);
    } else {
	DBG_LOG(MSG, MOD_CLUSTER, "send_cmm_request() failed, rv=%d", rv);
	retval = 4;
	glob_cmm_sndmon_sndcmm_err++;
	goto err_exit;
    }

err_exit:

    if (nsc) {
    	release_namespace_token_t(token);
    }
    mime_hdr_shutdown(&hdr);

    return retval;
}

/*
 * send_cmm_cancel_monitor() - Push cancel monitor nodes message to CMM Fqueue
 *
 *	Input:
 *	  nodes - Nodes in configuration
 *	  host_port - host:port sring
 *	  host_port_strlen - strlen
 *	  token - Associated token
 *	  instance - Associated instance
 *
 * 	Returns: 0 => Success
 */
int send_cmm_cancel_monitor(int nodes, const char **host_port, 
			    const int **host_port_strlen,
			    namespace_token_t token, int instance)
{
    int rv;
    int retval = 0;
    int n;
    int total_bytes;
    int bytes_used;
    char *buf;
    char current_node_handle[MAX_CMM_NODE_HANDLE_LEN];
    const char *node_handle[MAX_CMM_DATA_ELEMENTS];

    // Estimate data buffer size
    total_bytes = 0;
    for (n = 0; n < nodes; n++) {
    	total_bytes += (*host_port_strlen[n] + 512);
    }
    buf = alloca(total_bytes);
    bytes_used = 0;

    for (n = 0; n < nodes; n++) {
	// Build node handle
    	rv = cmm_node_handle(token.u.token, instance, host_port[n], 
			     current_node_handle, sizeof(current_node_handle));
    	if (rv) {
	    DBG_LOG(MSG, MOD_CLUSTER, "cmm_node_handle() failed, rv=%d", rv);
	    retval = 1;
	    glob_cmm_sndcan_hd_err++;
	    goto err_exit;
    	}

	node_handle[0] = current_node_handle;
	rv = cmm_request_cancel_data_serialize(1, node_handle,
					&buf[bytes_used], 
					total_bytes - bytes_used);
	if (rv >= (total_bytes - bytes_used)) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "cmm_request_cancel_data_serialize() buffer size exceeded, "
		    "rv=%d bufavail=%d n=%d", rv, total_bytes - bytes_used, n);
	    retval = 2;
	    glob_cmm_sndcan_srz_err++;
	    goto err_exit;
	}
	bytes_used += rv;
    }

    rv = send_cmm_request(REQ_CMM_CANCEL, REQ_CMM_CANCEL_STRLEN, NULL, 0,
			  NULL, buf, bytes_used);
    if (!rv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"send_cmm_request() token=%u,%u (%ld) instance=%d",
		token.u.token_data.gen, token.u.token_data.val, token.u.token,
		instance);
    } else {
	DBG_LOG(MSG, MOD_CLUSTER, "send_cmm_request() failed, rv=%d", rv);
	retval = 3;
	glob_cmm_sndcan_sndcmm_err++;
	goto err_exit;
    }

err_exit:

    return retval;
}

/*
 * cl_cmm_init() - system initialization
 *	Return: 0 => Success
 */
int cl_cmm_init(void)
{
    int rv;
    pthread_attr_t attr;
    int stacksize = 128 * 1024;

    SIMPLEQ_INIT(&cmm_req_head);

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

    if (pthread_create(&cmm_thread_id, &attr, cmm_thread_func, NULL)) {
	DBG_LOG(SEVERE, MOD_CLUSTER, "Failed to create cmm thread");
	return 3;
    }
    return 0; // Success
}

/*
 * End of cl_cmm.c
 */
