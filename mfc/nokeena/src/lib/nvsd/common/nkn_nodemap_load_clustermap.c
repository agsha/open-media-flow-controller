/*
 *******************************************************************************
 * nkn_nodemap_load_clustermap.c -- NodeMap Load ClusterMap specific functions
 *******************************************************************************
 *
 *  Load clustermap is built upon the ClusterMap NodeMap by using a simple
 *  interposition scheme by intercepting the NodeMap ops.
 */
#include "stdlib.h"
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <alloca.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include <atomic_ops.h>
#include "nkn_stat.h"
#include "nkn_namespace.h"
#include "nkn_nodemap.h"
#include "nkn_cmm_shm_api.h"
#include "nkn_debug.h"
#include "nkn_sched_api.h"

NKNCNT_DEF(load_cmap_alt_req_fails, int64_t, "", "load cmap alt request generate fails")
NKNCNT_DEF(load_cmap_req2origin_fails, int64_t, "", "load cmap request2origin fails")
NKNCNT_DEF(load_cmap_alt_uri_colls, int64_t, "", "load cmap remapped uri collisions")

/*
 *******************************************************************************
 * Data declarations 
 *******************************************************************************
 */
typedef struct load_cluster_map_task_private {
    AO_t node_handle_shm_chan[MAX_CLUSTER_MAP_NODES]; // Set by NodeMap
    AO_t load_metric_addr[MAX_CLUSTER_MAP_NODES]; // Set by helper task
    AO_t exit_task; // Set by NodeMap

    /* Task private data */
    cmm_shm_chan_t shm_chan[MAX_CLUSTER_MAP_NODES];
    char shm_chan_valid[roundup(MAX_CLUSTER_MAP_NODES, NBBY)/NBBY];
} load_cluster_map_task_private_t;

typedef struct load_cluster_map_private {
    node_map_descriptor_t nmd; /* Hold op ptrs to interposed NodeMap */
    load_cluster_map_task_private_t *task_data;
    nkn_task_id_t task_id;
    uint32_t rand_seed;
    const cl_redir_lb_attr_t *lb_cfg;
    nodemap_stat_t *node_stat[MAX_CLUSTER_MAP_NODES];
} load_cluster_map_private_t;

#define CTX2_LCMP_PRIVATE(_ctx) \
    (load_cluster_map_private_t *)((_ctx)->my_node_map->private_data)

 /*
  ******************************************************************************
  * Procedure declarations
  ******************************************************************************
  */
static void free_load_cluster_map_private(load_cluster_map_private_t *priv);
static int create_alt_request_uri(mime_header_t *alt_req, 
				  const mime_header_t *req, uint32_t variation);

/*
 *******************************************************************************
 * (node_map_descriptor_t.get_nodeid)() interface function
 *******************************************************************************
 */
static nm_node_id_t
load_cluster_map_get_nodeid(node_map_context_t *ctx, const char *host_port)
{
    load_cluster_map_private_t *priv;
    priv = CTX2_LCMP_PRIVATE(ctx);

    return priv->nmd.get_nodeid(ctx, host_port);
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.cache_name)() interface function
 *******************************************************************************
 */
static int
load_cluster_map_cache_name(node_map_context_t *ctx, const mime_header_t *req,
		       char **host, int *hostlen, uint16_t *port)
{
    load_cluster_map_private_t *priv;
    priv = CTX2_LCMP_PRIVATE(ctx);

    return priv->nmd.cache_name(ctx, req, host, hostlen, port);
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.request_to_origin)() interface function
 *******************************************************************************
 */
static int
load_cluster_map_request_to_origin(node_map_context_t *ctx,  
			      request_context_t *req_ctx,
			      long flags,
			      const namespace_config_t *nsc,
			      const mime_header_t *req,
			      char **host, int *hostlen, uint16_t *port,
			      nm_node_id_t *pnode_id, int *ponline_nodes)
{

#define GET_NODE_LOAD(_priv, _node, _load_metric) \
{ \
    AO_t _aval; \
    _aval = AO_load(&(_priv)->task_data->load_metric_addr[(_node)]); \
    (_load_metric) = _aval ? *((node_load_metric_t *)_aval) : 0; \
}

#define SAVE_REQ2ORIGIN_RET_DATA(_ret_data) \
{ \
    (_ret_data).host = *host; \
    (_ret_data).hostlen = *hostlen; \
    (_ret_data).port = *port; \
    (_ret_data).node_id = node_id; \
    (_ret_data).online_nodes = online_nodes; \
}

#define RESTORE_REQ2ORIGIN_RET_DATA(_ret_data) \
{ \
    *host = (_ret_data).host; \
    *hostlen = (_ret_data).hostlen; \
    *port = (_ret_data).port; \
    node_id = (_ret_data).node_id; \
    online_nodes = (_ret_data).online_nodes; \
}

#define IP_TO_STR(_ip, _iplen) \
{ \
    int _len = MIN((_iplen), ((long)sizeof(ipstr)-1)); \
    memcpy(ipstr, (_ip), _len); \
    ipstr[_len] = '\0'; \
}

    int config_random_node_on_overload;
    node_load_metric_t config_lm_threshold;

    int rv;
    int retval = 0;
    uint32_t variation;

    load_cluster_map_private_t *priv;
    node_load_metric_t lm;
    nm_node_id_t node_id;
    int online_nodes;

    char nodes_used[roundup(MAX_CLUSTER_MAP_NODES, NBBY)/NBBY];
    mime_header_t alt_req;
    int nodes_visited;

    /* request_to_origin() return state data */
    typedef struct return_state {
    	char *host;
    	int hostlen;
    	uint16_t port;
    	nm_node_id_t node_id;
    	int online_nodes;
    } return_state_t;

    return_state_t original_ret_data;
    return_state_t ret_data;
    return_state_t least_loaded_ret_data;
    node_load_metric_t lm_least_loaded;

    char ipstr[1024];

    priv = CTX2_LCMP_PRIVATE(ctx);

    /* Get node selection criteria from configuration */
    config_random_node_on_overload = 
    	(priv->lb_cfg->action == CL_REDIR_LB_REP_RANDOM);
    config_lm_threshold = priv->lb_cfg->load_metric_threshold;

    rv = priv->nmd.request_to_origin(ctx, req_ctx, flags, nsc, req, 
				     host, hostlen, 
				     port, &node_id, &online_nodes);
    if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, "nmd.request_to_origin() failed, "
		"req=%p rv=%d", req, rv);
	retval = rv;
    	goto exit;
    }

    GET_NODE_LOAD(priv, node_id, lm);
    if ((lm < config_lm_threshold) || (online_nodes == 1)) {
    	goto exit;
    }
    lm_least_loaded = ULONG_MAX;
    SAVE_REQ2ORIGIN_RET_DATA(original_ret_data)
    SAVE_REQ2ORIGIN_RET_DATA(ret_data)

    /*
     * Select an alternate online node based on configuration:
     *  1) Node below the configured load metric
     *  2) Random node
     */

    /* Initialize state for alternate node search */
    memset(nodes_used, 0, sizeof(nodes_used));
    setbit(nodes_used, node_id);

    mime_hdr_init(&alt_req, MIME_PROT_HTTP, 0, 0);

    variation = config_random_node_on_overload ? rand_r(&priv->rand_seed) : 1;
    nodes_visited = 1;


    IP_TO_STR(*host, *hostlen);
    DBG_LOG(MSG2, MOD_CLUSTER, 
	    "Seeking alternate req=%p [%s:%hu] nodeid=%d online=%d",
	    req, ipstr, *port, node_id, online_nodes);

    /* 
     * Optimistically assume that from the initial to final request_to_origin() 
     * call that the node configuration remains static.  
     * If a node reconfiguration occurs, use the path of least resistance 
     * to deal with change.   In some cases, this will result in a non
     * optimal selection of an alternate.
     *
     * Cases:
     * 1) Online nodes increase from initial config.
     *    Handled, no issue.
     *
     * 2) Online nodes decrease from initial config.
     *    May not consult remaining unvisited nodes in all cases.
     *
     * 3) Combinations of 1) and 2) 
     *    May not consult remaining unvisited nodes in all cases.
     */
    while (1) {
    	rv = create_alt_request_uri(&alt_req, req, variation);
    	if (rv) {
	    /* Not expected to fail, return last request_to_origin() data */
	    RESTORE_REQ2ORIGIN_RET_DATA(ret_data);
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "create_alt_request_uri() failed, req=%p rv=%d "
		    "variation=%d", req, rv, variation);
	    glob_load_cmap_alt_req_fails++;
	    break;
    	}

    	rv = priv->nmd.request_to_origin(ctx, req_ctx, flags, nsc, &alt_req, 
					 host, hostlen, port, 
					 &node_id, &online_nodes);
	if (!rv) {
	    IP_TO_STR(*host, *hostlen);
	    SAVE_REQ2ORIGIN_RET_DATA(ret_data);
	} else {
	    /* Return last request_to_origin() data */
	    RESTORE_REQ2ORIGIN_RET_DATA(ret_data);
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "alternate nmd.request_to_origin() failed, req=%p rv=%d "
		    "variation=%d", req, rv, variation);
	    glob_load_cmap_req2origin_fails++;
	    break;
	}

	if (!config_random_node_on_overload) {
	    GET_NODE_LOAD(priv, node_id, lm);
	    if (lm < config_lm_threshold) {
	    	/* Found alternate node below the load threshold */

		if (!isset(nodes_used, node_id) && (lm < lm_least_loaded)) {
		    /* Make this the current least loaded node */
		    lm_least_loaded = lm;
		    SAVE_REQ2ORIGIN_RET_DATA(least_loaded_ret_data);

		    DBG_LOG(MSG2, MOD_CLUSTER, 
			    "Using alternate req=%p [%s:%hu] variation=%d "
			    "nodeid=%d online=%d lm=%lu lm_thrshld=%lu",
			    req, ipstr, *port, variation, node_id, 
			    online_nodes, lm, config_lm_threshold);
	    	}
	    } else {
		DBG_LOG(MSG2, MOD_CLUSTER, 
			"Alternate overloaded req=%p [%s:%hu] variation=%d "
			"nodeid=%d online=%d lm=%lu lm_thrshld=%lu",
			req, ipstr, *port, variation, node_id, online_nodes, 
			lm, config_lm_threshold);
	    }
	} else {
	    if (!isset(nodes_used, node_id)) {
		DBG_LOG(MSG2, MOD_CLUSTER, 
			"Using alternate req=%p [%s:%hu] variation=%d "
			"nodeid=%d online=%d", 
			req, ipstr, *port, variation, node_id, online_nodes);
	    	break;
	    } else {
	    	/* Collision, try again */
		DBG_LOG(MSG2, MOD_CLUSTER, 
			"Alternate collision req=%p [%s:%hu] variation=%d "
			"nodeid=%d online=%d", 
			req, ipstr, *port, variation, node_id, online_nodes);
	    }
	}

	if (!isset(nodes_used, node_id)) {
	    setbit(nodes_used, node_id);
	    nodes_visited++;
	} else {
	    glob_load_cmap_alt_uri_colls++;
	}

	if ((online_nodes == 1) || (nodes_visited >= online_nodes)) {
	    if (!config_random_node_on_overload && 
	    	(lm_least_loaded < config_lm_threshold)) {
	    	RESTORE_REQ2ORIGIN_RET_DATA(least_loaded_ret_data);
		break;
	    }
	    /*
	     * Unable to locate an alternate node below the configured load
	     * metric, use the initial node since no other alternates exist.
	     */
	    RESTORE_REQ2ORIGIN_RET_DATA(original_ret_data);
	    IP_TO_STR(*host, *hostlen);
	    DBG_LOG(MSG2, MOD_CLUSTER, "No more alternates using "
		    "req=%p variation=%d [%s:%hu] nodeid=%d online=%d "
		    "nodes_visited=%d",
		    req, variation, ipstr, *port, node_id, online_nodes, 
		    nodes_visited);
	    break;
	}
	variation++;
    }
    mime_hdr_shutdown(&alt_req);

exit:

    if (!retval) {
    	if (pnode_id) {
	    *pnode_id = node_id;
	}
	if (ponline_nodes) {
	    *ponline_nodes = online_nodes;
	}
	INC_NODEMAP_STAT(priv->node_stat[node_id]);
    }
    return retval;

#undef GET_NODE_LOAD
#undef SAVE_REQ2ORIGIN_RET_DATA
#undef RESTORE_REQ2ORIGIN_RET_DATA
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.reply_action)() interface function
 *******************************************************************************
 */
static int
load_cluster_map_origin_reply_action(node_map_context_t *ctx,
			        request_context_t *req_ctx,
			      	const mime_header_t *req,
			      	const mime_header_t *reply, int http_reply_code)
{
    load_cluster_map_private_t *priv;
    priv = CTX2_LCMP_PRIVATE(ctx);

    return priv->nmd.origin_reply_action(ctx, req_ctx, req, reply, 
    					 http_reply_code);
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.get_config)() interface function
 *******************************************************************************
 */
static int 
load_cluster_map_get_config(node_map_context_t *ctx, int *nodes,
                       const char **host_port, const int **host_port_strlen,
                       const char **http_host_port, 
		       const int **http_host_port_strlen,
		       const const char **heartbeat_path, 
		       const int **heartbeat_path_strlen)
{
    load_cluster_map_private_t *priv;
    priv = CTX2_LCMP_PRIVATE(ctx);

    return priv->nmd.get_config(ctx, nodes, host_port, host_port_strlen, 
    				http_host_port, http_host_port_strlen,
		       		heartbeat_path, heartbeat_path_strlen);
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.get_node_local_data)() interface function
 *******************************************************************************
 */
static int 
load_cluster_map_get_node_local_data(node_map_context_t *ctx, 
				     uint32_t *node_local_ip)
{
    load_cluster_map_private_t *priv;
    priv = CTX2_LCMP_PRIVATE(ctx);

    return priv->nmd.get_node_local_data(ctx, node_local_ip);
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.node_update)() interface function
 *******************************************************************************
 */
static int
load_cluster_map_node_update(node_map_context_t *ctx, int online, int entries, 
			char **hostport, const char **node_handle, 
			const int *node_handle_strlen)
{
    int rv;
    int n;
    AO_t aval;
    char *node_handle_str;
    load_cluster_map_private_t *priv;
    nm_node_id_t node;
    uint64_t actual_vers;
    uint64_t expected_vers;

    /*
     * Note: Single threaded execution since the call is always made 
     *       from the node_status thread.
     */
    priv = CTX2_LCMP_PRIVATE(ctx);

    for (n = 0; n < entries; n++) {
    	node = ctx->my_node_map->get_nodeid(ctx, hostport[n]);
	if (node < 0) {
	    DBG_LOG(MSG, MOD_CLUSTER, "Invalid node=%d for n=%d '%s'", 
		    node, n, hostport[n]);
	    continue;
	}
	aval = AO_load(&priv->task_data->node_handle_shm_chan[node]);
	if (!aval) {
	    /* Allocate node handle string */
	    node_handle_str = nkn_malloc_type(node_handle_strlen[n]+1,
					      mod_cl_node_handle);
	    if (node_handle_str) {
	    	memcpy(node_handle_str, node_handle[n], node_handle_strlen[n]);
		node_handle_str[node_handle_strlen[n]] = '\0';
    		AO_store(&priv->task_data->node_handle_shm_chan[node], 
			 (long)node_handle_str);
		aval = (long)node_handle_str;
	    } else {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"Unable to allocate mod_cl_node_handle, size=%d n=%d",
			node_handle_strlen[n]+1, n);
	    }
	}

	if (online && aval) {
	    /* 
	     * Verify that the node is using the correct heartbeat 
	     * message protoocol version.
	     */
	    rv = valid_heartbeat_protocol_version((const char *)aval, 
						   &actual_vers, 
						   &expected_vers);
	    if (rv == 0) {
		DBG_LOG(ALARM, MOD_CLUSTER, 
			"Node: [%s] using heartbeat protocol vers=%ld "
			"should be vers=%ld",
			(const char *)aval, actual_vers, expected_vers);
		DBG_LOG(ERROR, MOD_CLUSTER, 
			"Node: [%s] using heartbeat protocol vers=%ld "
			"should be vers=%ld",
			(const char *)aval, actual_vers, expected_vers);
	    }
	}
    }

    return priv->nmd.node_update(ctx, online, entries, hostport, 
    				 node_handle, node_handle_strlen);
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.free_nmd)() interface function
 *******************************************************************************
 */
static void
load_cluster_map_free_nmd(struct node_map_descriptor *nmd)
{
    load_cluster_map_private_t *priv;
    priv = (load_cluster_map_private_t *)nmd->private_data;

    priv->nmd.free_nmd(nmd);
    free_load_cluster_map_private(priv);
}

/*
 *******************************************************************************
 * 		P R I V A T E  F U N C T I O N S 
 *******************************************************************************
 */
static void
free_load_cluster_map_private(load_cluster_map_private_t *priv)
{
    int n;

    if (priv->task_data) {
    	AO_store(&priv->task_data->exit_task, 1); // Terminate helper task
    }

    for (n = 0; n < MAX_CLUSTER_MAP_NODES; n++) {
        if (priv->node_stat[n]) {
	    release_nodemap_stat_t(priv->node_stat[n]);
	    priv->node_stat[n] = NULL;
	}
    }
    free(priv);
}

static int
load_cluster_map_task(load_cluster_map_task_private_t *priv)
{
    int rv;
    int n;
    AO_t aval;
    void *data;
    int datalen;

    while (1) {
        if (AO_load(&priv->exit_task)) {
	    break;
	}

	/* Establish or validate cmm_shm channels */
    	for (n = 0; n < MAX_CLUSTER_MAP_NODES; n++) {
	    aval = AO_load(&priv->node_handle_shm_chan[n]);
	    if (aval) {
	        if (!isset(priv->shm_chan_valid, n)) {
		    rv = cmm_shm_open_chan((const char *)aval, 
					   CMMSHM_CH_LOADMETRIC,
					   &priv->shm_chan[n]);
		    if (!rv) {
		    	rv = cmm_shm_chan_getdata_ptr(&priv->shm_chan[n], 
						      &data, &datalen, 0);
			if (!rv) {
			    AO_store(&priv->load_metric_addr[n], (long)data);
			    setbit(priv->shm_chan_valid, n);
			    DBG_LOG(MSG, MOD_CLUSTER, 
				    "Established cmm_shm chan [%s]",
				    (char *)aval);
			}
		    } else {
			DBG_LOG(ERROR, MOD_CLUSTER, 
				"cmm_shm_open_chan(%s) failed, rv=%d",
				(char *)aval, rv);
		    }
		} else {
		    rv = cmm_shm_is_chan_invalid(&priv->shm_chan[n]);
		    if (rv) {
		    	clrbit(priv->shm_chan_valid, n);
		    	AO_store(&priv->load_metric_addr[n], (long)NULL);
			DBG_LOG(MSG, MOD_CLUSTER, 
				"Invalidate cmm_shm chan [%s]", (char *)aval);
		    }
		}
	    }
	}

        return 1; // Sleep and resume task
    }

    /* Free task private data */
    for (n = 0; n < MAX_CLUSTER_MAP_NODES; n++) {
        aval = AO_load(&priv->node_handle_shm_chan[n]);
    	if (aval) {
	    free((void *)aval);
	}
    }
    free(priv);
    DBG_LOG(MSG, MOD_CLUSTER, 
	    "load_cluster_map_task(), arg=%p exiting...", priv);

    return 0; // Exit and terminate task
}

/*
 *******************************************************************************
 * Task interfaces
 *******************************************************************************
 */
static void
ld_cluster_task_input(nkn_task_id_t id)
{
    int rv;
    load_cluster_map_task_private_t *priv;

    priv = (load_cluster_map_task_private_t *)
	    nkn_task_get_private(TASK_TYPE_LD_CLUSTER, id);
    if (priv) {
    	rv = load_cluster_map_task(priv);
	if (rv) { // Sleep and retry
	    nkn_task_timed_wait(id, 1000 /* 1 sec */);
	    return;
	} else { // Terminate task
	    nkn_task_set_private(TASK_TYPE_LD_CLUSTER, id, (void *)NULL);
	}
    }
    nkn_task_set_action_and_state(id, TASK_ACTION_OUTPUT, TASK_STATE_RUNNABLE);
}

static void
ld_cluster_task_output(nkn_task_id_t id)
{
    nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP, TASK_STATE_RUNNABLE);
}

static void
ld_cluster_task_cleanup(nkn_task_id_t id)
{
    UNUSED_ARGUMENT(id);
}

typedef struct digit_data {
    const char *dstr;
    int dstrlen;
} digit_data_t;

static digit_data_t digit_str[10] = {
    { "Zero", 4 },
    { "One12", 5 },
    { "Two234", 6 },
    { "Three34", 7 },
    { "Four4567", 8 },
    { "Five56789", 9 },
    { "Six6789012", 10 },
    { "Seven789012", 11 },
    { "Eight8901234", 12 },
    { "Nine901234567", 13 }
};

static int
create_alt_request_uri(mime_header_t *alt_req, const mime_header_t *req, 
		       uint32_t variation)
{
    int retval = 0;
    int rv;
    const char *data;
    int datalen;
    u_int32_t attrs;
    int hdrcnt;

    int uri_prefix_strlen;
    char uri_prefix[1024]; // sized to support uint32_t max digit strings
    uint32_t val;
    int digit;

    int pbuf_len;
    char *pbuf;

    if (get_known_header(req, MIME_HDR_X_NKN_REMAPPED_URI,
		    	 &data, &datalen, &attrs, &hdrcnt)) {
	if (get_known_header(req, MIME_HDR_X_NKN_DECODED_URI,
			     &data, &datalen, &attrs, &hdrcnt)) {
	    if (get_known_header(req, MIME_HDR_X_NKN_URI,
			    	 &data, &datalen, &attrs, &hdrcnt)) {
		retval = 1;
		goto err_exit;
	    }
	}
    }

    /*
     * Generate a variation of the request URI by prepending the
     * URI with "/{ascii text mapping of 'variation'}" 
     * and placing the result in 'alt_req'.
     */
     uri_prefix[0] = '/';
     uri_prefix_strlen = 1;

     for (val = variation; val; val /= 10) {
         digit = val % 10;
         memcpy(&uri_prefix[uri_prefix_strlen], digit_str[digit].dstr, 
	 	digit_str[digit].dstrlen);
	 uri_prefix_strlen += digit_str[digit].dstrlen;
     }

     pbuf_len = uri_prefix_strlen + datalen;
     pbuf = alloca(pbuf_len+1);
     memcpy(pbuf, uri_prefix, uri_prefix_strlen);
     memcpy(&pbuf[uri_prefix_strlen], data, datalen);
     pbuf[pbuf_len] = '\0';

     rv = add_known_header(alt_req, MIME_HDR_X_NKN_URI, pbuf, pbuf_len);
     if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"add_known_header(MIME_HDR_X_NKN_URI), failed rv=%d uri=%s",
		rv, pbuf);
	retval = 2;
	goto err_exit;
     }

err_exit:

    return retval;
}

static int 
setup_stat_nodes(node_map_descriptor_t *nmd, const char *stat_prefix,
		 const char *namespace_name)
{
    int rv;
    int n;
    int nodes;
    const char *host_port[MAX_CLUSTER_MAP_NODES];
    const int *host_port_strlen[MAX_CLUSTER_MAP_NODES];
    const char *http_host_port[MAX_CLUSTER_MAP_NODES];
    const int *http_host_port_strlen[MAX_CLUSTER_MAP_NODES];
    const char *heartbeat_path[MAX_CLUSTER_MAP_NODES];
    const int *heartbeat_path_strlen[MAX_CLUSTER_MAP_NODES];

    load_cluster_map_private_t *priv;
    char buf[1024];

    rv = (*nmd->get_config)(&nmd->ctx, &nodes, &host_port[0], 
			    &host_port_strlen[0],
			    &http_host_port[0], &http_host_port_strlen[0],
			    &heartbeat_path[0], 
			    &heartbeat_path_strlen[0]);
    if (rv) {
    	DBG_LOG(MSG, MOD_CLUSTER, "get_config() failed, rv=%d", rv);
	return 1;
    }

    priv = (load_cluster_map_private_t *)nmd->private_data;
    for (n = 0; n < nodes; n++) {
        rv = snprintf(buf, sizeof(buf), "%s_%s_%s", stat_prefix, namespace_name,
		      host_port[n]);
	buf[sizeof(buf)-1] = '\0';
        priv->node_stat[n] = get_nodemap_stat_t(buf);
	if (!priv->node_stat[n]) {
	    DBG_LOG(MSG, MOD_CLUSTER, "get_nodemap_stat_t('%s') failed", buf);
	    return 2;
	}
    }
    return 0;
}

/*
 *******************************************************************************
 * 		P U B L I C  F U N C T I O N S 
 *******************************************************************************
 */

/*
 *******************************************************************************
 * new_nmd_load_cluster_map - Create node_map_descriptor_t for load ClusterMap
 *
 *	Return: 0 => Success
 *******************************************************************************
 */
node_map_descriptor_t *
new_nmd_load_cluster_map(const cluster_map_bin_t *data, int datalen,
		    const char *namespace_name, const char *namespace_uid,
		    const char *stat_prefix,
		    const cluster_hash_config_t *ch_cfg,
		    const cl_redir_lb_attr_t *lb_cfg,
		    namespace_config_t *nsc, const namespace_config_t *old_nsc,
		    int *online)
{
    int rv;
    int retval = 0;
    node_map_descriptor_t *nmd = 0;
    load_cluster_map_task_private_t *task_priv = 0;
    load_cluster_map_private_t *priv = 0;
    uuid_t uid;

    nmd = new_nmd_cluster_map(data, datalen, namespace_name, namespace_uid, 
			      ch_cfg, nsc, old_nsc, online);
    if (!nmd) {
	DBG_LOG(MSG, MOD_CLUSTER, "new_nmd_cluster_map() failed");
	retval = 1;
	goto err_exit;
    }

    priv = (load_cluster_map_private_t *)
    	  nkn_calloc_type(1, sizeof(load_cluster_map_private_t), 
	  		  mod_cl_load_cluster_map_private_t);
    if (!priv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"nkn_calloc_type(load_cluster_map_private_t) failed");
	retval = 2;
	goto err_exit;
    }

    priv->rand_seed = (*(uint32_t *)&uid[0]) ^ (*(uint32_t *)&uid[4]) ^
        	      (*(uint32_t *)&uid[8]) ^ (*(uint32_t *)&uid[12]);
    priv->lb_cfg = lb_cfg;

    /* Override ops in cluster_map NodeMap */
    priv->nmd.get_nodeid = nmd->get_nodeid;
    priv->nmd.cache_name = nmd->cache_name;
    priv->nmd.request_to_origin = nmd->request_to_origin;
    priv->nmd.origin_reply_action = nmd->origin_reply_action;
    priv->nmd.get_config = nmd->get_config;
    priv->nmd.get_node_local_data = nmd->get_node_local_data;
    priv->nmd.node_update = nmd->node_update;
    priv->nmd.free_nmd = nmd->free_nmd;

    nmd->private_data = priv;

    nmd->get_nodeid = load_cluster_map_get_nodeid;
    nmd->cache_name = load_cluster_map_cache_name;
    nmd->request_to_origin = load_cluster_map_request_to_origin;
    nmd->origin_reply_action = load_cluster_map_origin_reply_action;
    nmd->get_config = load_cluster_map_get_config;
    nmd->get_node_local_data = load_cluster_map_get_node_local_data;
    nmd->node_update = load_cluster_map_node_update;
    nmd->free_nmd = load_cluster_map_free_nmd;

    task_priv = (load_cluster_map_task_private_t *)
    	  nkn_calloc_type(1, sizeof(load_cluster_map_task_private_t), 
	  		  mod_cl_load_cluster_map_task_private_t);
    if (!task_priv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"nkn_calloc_type(load_cluster_map_task_private_t) failed");
	retval = 3;
	goto err_exit;
    }
    priv->task_data = task_priv;

    /*
     * Create nkncnt stat nodes
     */
    rv = setup_stat_nodes(nmd, stat_prefix, namespace_name);
    if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, "setup_stat_nodes() failed, rv=%d", rv);
    	retval = 5;
	goto err_exit;
    }

    /* Start helper task */
    priv->task_id = nkn_task_create_new_task(TASK_TYPE_LD_CLUSTER,
					    	TASK_TYPE_LD_CLUSTER,
						TASK_ACTION_INPUT,
						0,
						task_priv,
						sizeof(*task_priv),
						0 /* No deadline */);
    if (priv->task_id < 0) {
	DBG_LOG(MSG, MOD_CLUSTER, "nkn_task_create_new_task() failed, rv=%ld", 
		priv->task_id);
	assert(0);
    }
    nkn_task_set_private(TASK_TYPE_LD_CLUSTER, priv->task_id, 
    			 (void *)task_priv);
    nkn_task_set_state(priv->task_id, TASK_STATE_RUNNABLE);

    /* Note: err_exit not allowed at this point */

    DBG_LOG(MSG, MOD_CLUSTER, 
	    "Node map created, nmd=%p priv=%p task_priv=%p", 
	    nmd, priv, task_priv);
    return nmd;

err_exit:

    DBG_LOG(MSG, MOD_CLUSTER, "Failed, retval=%d", retval);
    if (nmd) {
    	nmd->free_nmd(nmd);
    }
    if (task_priv) {
    	free(task_priv);
    }
    return NULL;
}

/*
 * load_cluster_map_init() - System initialization
 */
int
load_cluster_map_init()
{
    nkn_task_register_task_type(TASK_TYPE_LD_CLUSTER,
                                &ld_cluster_task_input,
                                &ld_cluster_task_output,
                                &ld_cluster_task_cleanup);
    return 1; // Success
}

/*
 * End of nkn_nodemap_load_clustermap.c
 */
