/*
 *******************************************************************************
 * nkn_nodemap_originescalationmap.c -- NodeMap OriginEscalationMap
 *	specific functions
 *******************************************************************************
 */
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>
#include <strings.h>
#include <alloca.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include "nkn_namespace.h"
#include "nkn_nodemap.h"
#include "nkn_nodemap_originescalationmap.h"
#include "origin_escalation_map_bin.h"
#include "nkn_debug.h"

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
	memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

typedef struct origin_escalation_map_node {
    map_node_t base;
    int node_weight;
    int num_http_response_fail_codes;
    int *http_response_fail_codes;
} origin_escalation_map_node_t;

typedef struct origin_escalation_map_private {
    map_private_t base;
} origin_escalation_map_private_t;

typedef struct request_context_data_t {
    int32_t last_node;
    int32_t flags;
#define REQ_CTX_FL_REQ_COMPLETE 	(1 << 0)
#define REQ_CTX_FL_CONNECT_TIMEOUT 	(1 << 1)
#define REQ_CTX_FL_READ_TIMEOUT 	(1 << 2)
#define REQ_CTX_FL_USE_CURRENT_OS	(1 << 3)
} request_context_data_t;

/* Forward references */
static int
print_origin_escalation_map_node_t(map_node_t *mp, char *buf, int buflen);
 
/*
 *******************************************************************************
 * (node_map_descriptor_t.get_nodeid)() interface function
 *******************************************************************************
 */
static nm_node_id_t
origin_escalation_get_nodeid(node_map_context_t *ctx, const char *host_port)
{
    nm_node_id_t node;
    origin_escalation_map_private_t *oemp = 
    	(origin_escalation_map_private_t *)ctx->private_data;

    MP_READ_LOCK(&oemp->base);
    node = host_port_to_node(host_port, &oemp->base);
    MP_UNLOCK(&oemp->base);
    return node;
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.cache_name)() interface function
 *******************************************************************************
 */
static int
origin_escalation_cache_name(node_map_context_t *ctx, const mime_header_t *req,
		       	     char **host, int *hostlen, uint16_t *port)
{
    UNUSED_ARGUMENT(req);

    origin_escalation_map_private_t *oemp = 
    	(origin_escalation_map_private_t *)ctx->private_data;

    *host = (char *)oemp->base.cache_name_host;
    *hostlen = oemp->base.cache_name_host_strlen;
    *port = 80;

    return 0;
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.request_to_origin)() interface function
 *******************************************************************************
 */
static int
origin_escalation_request_to_origin(node_map_context_t *ctx,  
				    request_context_t *req_ctx,
				    long flags,
				    const namespace_config_t *nsc,
				    const mime_header_t *req,
				    char **host, int *hostlen, uint16_t *port,
				    nm_node_id_t *pnode_id, int *ponline_nodes)
{
    /*
     *	Callers:
     *	--------
     *	1) REQ2OS_ORIGIN_SERVER 
     *	Selects tht next origin server or the current one denoted in the 
     *	context if REQ_CTX_FL_USE_CURRENT_OS is set and clears 
     *	REQ_CTX_FL_USE_CURRENT_OS.  
     *	Updates the context to reflect the selected node.
     *		
     *	2) REQ2OS_ORIGIN_SERVER_LOOKUP
     *	Same as 1) but treats the context as read-only so no updates performed.
     *
     *	3) REQ2OS_SET_ORIGIN_SERVER
     *	Selects the next origin server and updates the context and sets
     *	REQ_CTX_FL_USE_CURRENT_OS
     *
     *	Typical flow:
     *	-------------
     *	1) origin_escalation_cache_name()
     *
     *	2) Async DNS REQ2OS_ORIGIN_SERVER_LOOKUP
     *
     *	3) om_init_client() REQ2OS_ORIGIN_SERVER
     *
     *	4) cp_callback_http_header_received() REQ2OS_SET_ORIGIN_SERVER, 
     *	if escalation is requested via origin_escalation_origin_reply_action()
     */
    UNUSED_ARGUMENT(nsc);
    UNUSED_ARGUMENT(req);

    int retval = 0;
    int node;
    int online_nodes;
    origin_escalation_map_private_t *oemp = 
    	(origin_escalation_map_private_t *)ctx->private_data;
    request_context_data_t *req_ctx_data;
    int lookup = flags & R2OR_FL_LOOKUP;
    int set_os = flags & R2OR_FL_SET;

    if (ctx->my_node_map->nsc->init_in_progress) {
	return -1; // Not online, retry
    }

    if (req_ctx) {
        req_ctx_data = (request_context_data_t *)REQUEST_CTX_DATA(req_ctx);
    	if (REQUEST_CTX_TO_NODE_MAP_CTX(req_ctx) == ctx) {
	    if (req_ctx_data->flags & 
	    	(REQ_CTX_FL_CONNECT_TIMEOUT|REQ_CTX_FL_READ_TIMEOUT)) {
		if (!lookup) {
		    req_ctx_data->flags &= 
		    	~(REQ_CTX_FL_CONNECT_TIMEOUT|REQ_CTX_FL_READ_TIMEOUT);
		}
	    	node = req_ctx_data->last_node;
	    } else {
	    	if (req_ctx_data->flags & REQ_CTX_FL_USE_CURRENT_OS) {
		    node = req_ctx_data->last_node;
		} else {
		    node = req_ctx_data->last_node + 1;
		}
		if (!lookup && !set_os &&
		    (req_ctx_data->flags & REQ_CTX_FL_USE_CURRENT_OS)) {
		    req_ctx_data->flags &= ~REQ_CTX_FL_USE_CURRENT_OS;
		}
	    }
	    if (!lookup) {
	    	req_ctx_data->flags &= ~REQ_CTX_FL_REQ_COMPLETE;
	    }
	} else {
	    if (REQUEST_CTX_TO_NODE_MAP_CTX(req_ctx) == NULL) {
	    	node = 0;
	    } else {
	    	/* Not the current map */
		req_ctx = NULL;
	    	retval = 10;
		goto err_exit;
	    }
	}
    } else {
	node = 0;
    }

    MP_READ_LOCK(&oemp->base);
    if (!(online_nodes = oemp->base.online_nodes)) {
    	retval = 1;
    	MP_UNLOCK(&oemp->base);
	goto err_exit;
    }

    for ( ; node < oemp->base.defined_nodes; node++) {
        if (oemp->base.node[node]->node_online) {
	    break;
	}
    }
    if (node >= oemp->base.defined_nodes) {
    	node = -1;
    }
    MP_UNLOCK(&oemp->base);

    if (node >= 0) {
	if (!lookup && !set_os) {
	    INC_NODEMAP_STAT(oemp->base.node[node]->node_stat);
	}
    	*host = (char *)oemp->base.node[node]->node_host;
    	*hostlen = oemp->base.node[node]->node_host_strlen;
    	*port = oemp->base.node[node]->node_http_port;
	if (pnode_id) {
	    *pnode_id = node;
	}
	if (ponline_nodes) {
	    *ponline_nodes = online_nodes;
	}
    	if (!lookup && req_ctx) {
	    SET_REQUEST_CTX_TO_NODE_MAP_CTX(req_ctx, ctx);
	    req_ctx_data->last_node = node;
    	}
    } else {
	retval = 2;
	goto err_exit;
    }

    if (set_os) {
    	req_ctx_data->flags |= REQ_CTX_FL_USE_CURRENT_OS;
    }
    DBG_LOG(MSG2, MOD_CLUSTER, "Use %s, lookup=%d set_os=%d", 
	    oemp->base.node[node]->node_host_port, lookup, set_os);
    return 0;

err_exit:

    if (!lookup && req_ctx) {
	req_ctx_data->flags &= ~REQ_CTX_FL_USE_CURRENT_OS;
	SET_REQUEST_CTX_TO_NODE_MAP_CTX(req_ctx, NULL);
    }
    DBG_LOG(MSG2, MOD_CLUSTER, "Failed, reason=%d lookup=%d set_os=%d", 
	    retval, lookup, set_os);
    return retval;
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.reply_action)() interface function
 *******************************************************************************
 */
static int
origin_escalation_origin_reply_action(node_map_context_t *ctx,
				request_context_t *req_ctx,
			      	const mime_header_t *req,
			      	const mime_header_t *reply, int http_reply_code)
{
    UNUSED_ARGUMENT(req);
    UNUSED_ARGUMENT(reply);

    int n;
    origin_escalation_map_private_t *oemp = 
    	(origin_escalation_map_private_t *)ctx->private_data;

    request_context_data_t *req_ctx_data = 
    	(request_context_data_t *)REQUEST_CTX_DATA(req_ctx);

    origin_escalation_map_node_t *oem_node;

    if (!req_ctx_data->flags & REQ_CTX_FL_REQ_COMPLETE) {
    	req_ctx_data->flags |= REQ_CTX_FL_REQ_COMPLETE;
    } else {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"req_ctx=%p, Already processed, timeout_action=%d", 
		req_ctx, http_reply_code);
    	return 0; // Completion already processed, no action required
    }

    if (http_reply_code < 0) {
    	/* Connect or read request timeout, reflect this in the context */
	if (http_reply_code == HTTP_REPLY_CODE_CONNECT_TIMEOUT) {
	    req_ctx_data->flags |= REQ_CTX_FL_CONNECT_TIMEOUT;
	    DBG_LOG(MSG, MOD_CLUSTER, "req_ctx=%p, Connect timeout", req_ctx);
	} else if (http_reply_code == HTTP_REPLY_CODE_READ_TIMEOUT) {
	    req_ctx_data->flags |= REQ_CTX_FL_READ_TIMEOUT;
	    DBG_LOG(MSG, MOD_CLUSTER, "req_ctx=%p, Read timeout", req_ctx);
	}
	return 2; // Retry request on same node if online
    }

    oem_node = (origin_escalation_map_node_t *)
    	oemp->base.node[req_ctx_data->last_node];
    if (oem_node->http_response_fail_codes) {
    	for (n = 0; n < oem_node->num_http_response_fail_codes; n++) {
	    if (http_reply_code == oem_node->http_response_fail_codes[n]) {
	    	DBG_LOG(MSG, MOD_CLUSTER, "req_ctx=%p, Escalate, http_code=%d",
			req_ctx, http_reply_code);
	    	return 1; // Retry request on next node
	    }
	}
    } 

    SET_REQUEST_CTX_TO_NODE_MAP_CTX(req_ctx, NULL);
    DBG_LOG(MSG, MOD_CLUSTER, "req_ctx=%p, No escalation", req_ctx);
    return 0; // No retry
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.get_config)() interface function
 *******************************************************************************
 */
static int 
origin_escalation_get_config(node_map_context_t *ctx, int *nodes,
                       	     const char **host_port, 
			     const int **host_port_strlen,
                       	     const char **http_host_port, 
			     const int **http_host_port_strlen,
			     const const char **heartbeat_path, 
			     const int **heartbeat_path_strlen)
{
    origin_escalation_map_private_t *clmp = 
    	(origin_escalation_map_private_t *)ctx->private_data;

    return get_node_config(&clmp->base, nodes, host_port, host_port_strlen,
			   http_host_port, http_host_port_strlen,
			   heartbeat_path, heartbeat_path_strlen);
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.get_node_local_data)() interface function
 *******************************************************************************
 */
static int 
origin_escalation_get_node_local_data(node_map_context_t *ctx,
				      uint32_t *node_local_ip)
{
    UNUSED_ARGUMENT(ctx);
    UNUSED_ARGUMENT(node_local_ip);

    return 1; // Not supported
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.node_update)() interface function
 *******************************************************************************
 */
static int
origin_escalation_node_update(node_map_context_t *ctx, int online, int entries, 
			      char **hostport, const char **node_handle, 
			      const int *node_handle_strlen)
{
    UNUSED_ARGUMENT(node_handle);
    UNUSED_ARGUMENT(node_handle_strlen);

    int update;
    int cluster_init_now_complete;
    char buf[8192];
    origin_escalation_map_private_t *oemp = 
    	(origin_escalation_map_private_t *)ctx->private_data;

    MP_WRITE_LOCK(&oemp->base);
    update = update_node_config(&oemp->base, online, entries, hostport, 
    				&cluster_init_now_complete);
    if (update) {
	dump_node_config(&oemp->base, print_origin_escalation_map_node_t,
			 buf, sizeof(buf));
	DBG_LOG(MSG, MOD_CLUSTER, "Node configuration update: %s", buf);
    }
    MP_UNLOCK(&oemp->base);

    if (cluster_init_now_complete) {
    	/* Reflect the init => online transition in namespace_config */
    	AO_fetch_and_sub1(&ctx->my_node_map->nsc->init_in_progress);
    }

    return 0;
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.free_nmd)() interface function
 *******************************************************************************
 */
static void
origin_escalation_free_nmd(struct node_map_descriptor *nmd)
{
    int rv;
    int n;
    origin_escalation_map_private_t *oem_priv =
    	(origin_escalation_map_private_t *) nmd->ctx.private_data;
    origin_escalation_map_node_t *oem_node;

    if (oem_priv) {
	for (n = 0; n < oem_priv->base.defined_nodes; n++) {
	    oem_node = (origin_escalation_map_node_t *)oem_priv->base.node[n];

	    /* Free origin_escalation_map_node_t specific data */

	    if (oem_node->http_response_fail_codes) {
	    	free(oem_node->http_response_fail_codes);
	    	oem_node->http_response_fail_codes = NULL;
	    }

	    rv = deinit_map_node_t(oem_priv->base.node[n]);
	    if (rv) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"deinit_map_node_t(n=%d) failed, rv=%d", n, rv);
	    }
	    free(oem_priv->base.node[n]);
	    oem_priv->base.node[n] = NULL;
	}

	rv = deinit_map_private_t(&oem_priv->base);
	if (rv) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"deinit_map_private_t() failed, rv=%d", rv);
	}

	free(oem_priv);
	nmd->ctx.private_data = NULL;
    }

    if (nmd->ctx.map_data) {
    	free(nmd->ctx.map_data);
	nmd->ctx.map_data = NULL;
    }

    free(nmd);
    return;
}

/*
 *******************************************************************************
 * 		P R I V A T E  F U N C T I O N S 
 *******************************************************************************
 */

/*
 * setup_http_resp_failure_codes() - Parse HTTP failure codes
 *
 *	Data is decimal data separated with ';' (e.g. 404;500;503)
 *
 *	Return: 0 => Success
 */
static int 
setup_http_resp_failure_codes(origin_escalation_map_node_t *oem_node, 
			      const char *val, int vallen)
{
    int retval = 0;
    const char *p = val;
    const char *p_end = &val[vallen];
    const char *p_valend;
    long lval;
    int response_codes[1024];
    int num_response_codes = 0;

    while (p < p_end) {
        p_valend = memchr(p, ';', p_end - p);
        if (p_valend) {
	    lval = atol_len(p, p_valend - p);
	    p = p_valend + 1;
	} else {
	    lval = atol_len(p, p_end - p);
	    p = p_end;
	}
	if (lval > 0) {
	    if (num_response_codes < 
	    	(int)(sizeof(response_codes)/sizeof(int))) {
	    	response_codes[num_response_codes] = lval;
		num_response_codes++;
	    } else {
	    	TSTR(val, vallen, pcodes);
	    	DBG_LOG(MSG, MOD_CLUSTER, 
			"given response codes(%d) exceeds max(%lu) codes=[%s]",
			num_response_codes, 
			(sizeof(response_codes)/sizeof(int)), pcodes);
	    	retval = 1;
		goto exit;
	    }
	}
    }

    if (num_response_codes) {
    	oem_node->http_response_fail_codes = 
		nkn_calloc_type(1, sizeof(int) * num_response_codes, 
				mod_cl_origin_escalation_respcodes);
	if (!oem_node->http_response_fail_codes) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "nkn_calloc_type(mod_cl_origin_escalation_respcodes) "
		    "failed");
	    retval = 2;
	    goto exit;
	}
	memcpy((char *)oem_node->http_response_fail_codes,
	       (char *)response_codes, sizeof(int) * num_response_codes);
	oem_node->num_http_response_fail_codes = num_response_codes;
    }

exit:

    return retval;
}

/* qsort_cmp_origin_escalation_map_node_t - qsort compare function */
static int
qsort_cmp_origin_escalation_map_node_t(const void *p1, const void *p2)
{
    origin_escalation_map_node_t *oem_p1;
    origin_escalation_map_node_t *oem_p2;

    oem_p1 = *(origin_escalation_map_node_t **)p1;
    oem_p2 = *(origin_escalation_map_node_t **)p2;

    if (oem_p1->node_weight > oem_p2->node_weight) {
    	return 1;
    } else if (oem_p1->node_weight < oem_p2->node_weight) {
    	return -1;
    } else {
    	return 0;
    }
}

/*
 * init_origin_escalaton_map_private_t() - Initialize OriginEsclationMap  
 *					   specific data
 *
 *	Return: 0 => Return
 */
static int
init_origin_escalaton_map_private_t(node_map_descriptor_t *nmd,
				    const char *namespace_name,
				    const char *namespace_uid,
				    const namespace_config_t *nsc,
				    int *online)
{
    UNUSED_ARGUMENT(namespace_uid);

    int rv;
    int retval = 0;
    int n;
    char *phost;
    int hostlen;
    char *poptions;
    const char *val;
    int vallen;
    long lval;
    origin_escalation_map_node_t *oem_node;
    char buf[1024];
    const char *uid_ns;
    int uid_ns_len;

    origin_escalation_map_bin_t *oem_bin = 
    	(origin_escalation_map_bin_t *) nmd->ctx.map_data;

    origin_escalation_map_private_t *oem_priv =
    	(origin_escalation_map_private_t *) nmd->ctx.private_data;

    if (!oem_bin->num_entries) {
	DBG_LOG(MSG, MOD_CLUSTER, "No entries");
    	retval = 1;
    	goto err_exit;
    }

    if (oem_bin->num_entries > MAX_ORIGINESCAL_MAP_NODES) {
	DBG_LOG(MSG, MOD_CLUSTER, "Max nodes exceeded, given=%d max=%d",
		oem_bin->num_entries, MAX_ORIGINESCAL_MAP_NODES);
    	retval = 2;
    	goto err_exit;
    }

    /* Process binary node data */
    for (n = 0; n < (int)oem_bin->num_entries; n++) {
    	phost = ISTR(oem_bin, string_table_offset, oem_bin->entry[n].origin);
	hostlen = phost ? strlen(phost) : 0;

	if (phost && valid_FQDN(phost, hostlen)) {
	    /* Note: Allocating origin_escalation_map_node_t */
	    oem_priv->base.node[n] = (map_node_t *)
		    nkn_calloc_type(1, sizeof(origin_escalation_map_node_t),
				    mod_cl_origin_escalation_map_node_t);
	    if (!oem_priv->base.node[n]) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"nkn_calloc_type(cluster_map_node_t) failed");
		retval = 3;
		goto err_exit;
	    }

	    /* Heartbeat option */
	    poptions = 
	    	ISTR(oem_bin, string_table_offset, oem_bin->entry[n].options);
	    if (extract_options_value(poptions, OREM_OPTIONS_HEARTBEATHPATH, 
	    			      &val, &vallen)) {
	    	val = "";
		vallen = 1;
	    }

	    /* Create "glob_" stat for node */
	    snprintf(buf, sizeof(buf), "LO_%s", namespace_name);

	    rv = init_map_node_t(oem_priv->base.node[n], phost, 
	    			 oem_bin->entry[n].port, val, vallen, buf, nsc);
	    if (rv) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"init_map_node_t() failed, rv=%d n=%d host=%p port=%hu",
			rv, n, phost, oem_bin->entry[n].port);
		retval = 4;
		goto err_exit;
	    }

	    /* Setup origin_escalation_map_node_t specific data */

	    oem_node = (origin_escalation_map_node_t *)oem_priv->base.node[n];

	    if (!extract_options_value(poptions, OREM_OPTIONS_WEIGHT, 
	    			       &val, &vallen)) {
	    	lval = atol_len(val, vallen);
	    	if (lval < 0) {
		    oem_node->node_weight = 0;
	    	} else {
		    oem_node->node_weight = lval;
	    	}
	    } else {
		oem_node->node_weight = 0;
	    }

	    if (!extract_options_value(poptions, 
	    			OREM_OPTIONS_HTTP_RESPONSE_FAILURE_CODES, 
	    			&val, &vallen)) {
		rv = setup_http_resp_failure_codes(oem_node, val, vallen);
		if (rv) {
		    DBG_LOG(MSG, MOD_CLUSTER, 
			    "setup_http_resp_failure_codes() failed, rv=%d",
			    rv);
		    retval = 5;
		    goto err_exit;
		}
	    } else {
	    	oem_node->num_http_response_fail_codes = 0;
		val = "";
		vallen = 1;
	    }

	    TSTR(val, vallen, respcode_str);
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "Added Namespace=%s Node[%d]: %s "
		    "Online=%d Heartbeatpath=%s weight=%d "
		    "num_failure_response_codes=%d failure_response_codes=%s",
		    namespace_name, n, oem_priv->base.node[n]->node_host_port,
		    oem_priv->base.node[n]->node_online,
		    (oem_priv->base.node[n]->node_heartbeatpath ?
		    	oem_priv->base.node[n]->node_heartbeatpath : ""),
		    oem_node->node_weight,
		    oem_node->num_http_response_fail_codes, respcode_str);
	} else {
	    DBG_LOG(MSG, MOD_CLUSTER,
		    "Entry[%d]: Invalid FQDN address [%s]",
		    n, (phost ? phost : ""));
	    retval = 6;
	    goto err_exit;
	}
    }

    /* Sort origin_escalation_map_node_t entries based on node_weight */
    qsort((void *)oem_priv->base.node, oem_bin->num_entries,
	  sizeof(origin_escalation_map_node_t *),
    	  qsort_cmp_origin_escalation_map_node_t);

    /* Generate the host portion of the cache name */
    decompose_ns_uri(namespace_uid, &uid_ns, &uid_ns_len, 0, 0, 0, 0, 0, 0, 0);
    if (uid_ns) {
        char *nm = alloca(uid_ns_len + 1);
	memcpy(nm, uid_ns, uid_ns_len);
	nm[uid_ns_len] = '\0';
    	snprintf(buf, sizeof(buf), "lo-%s", nm);
    } else {
    	DBG_LOG(MSG, MOD_CLUSTER, "Invalid namespace_uid (%s)", namespace_uid);
	retval = 7;
	goto err_exit;
    }

    rv = init_map_private_t(&oem_priv->base, buf, online);
    if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"init_map_private_t failed, rv=%d", rv);
	retval = 8;
	goto err_exit;
    }
    dump_node_config(&oem_priv->base, print_origin_escalation_map_node_t, 
    		     buf, sizeof(buf));
    DBG_LOG(MSG, MOD_CLUSTER, "Sorted node configuration: %s", buf);

err_exit:

    return retval;
}

/* dump_node_config() print callback */
static int
print_origin_escalation_map_node_t(map_node_t *mp, char *buf, int buflen)
{
    int rv;
    origin_escalation_map_node_t *oemp = (origin_escalation_map_node_t *) mp;
    rv = snprintf(buf, buflen, "\tweight=%d num_response_fail_codes=%d\n",
    		  oemp->node_weight, oemp->num_http_response_fail_codes);
    return rv;
}

/*
 *******************************************************************************
 * 		P U B L I C  F U N C T I O N S 
 *******************************************************************************
 */

/*
 *******************************************************************************
 * new_nmd_origin_escalation_map - Create node_map_descriptor_t for 
 *				   OriginEscalationMap
 *
 *	Return: 0 => Success
 *******************************************************************************
 */
node_map_descriptor_t *
new_nmd_origin_escalation_map(const origin_escalation_map_bin_t *data, 
			      int datalen,
		    	      const char *namespace_name, 
			      const char *namespace_uid,
			      namespace_config_t *nsc,
			      const namespace_config_t *old_nsc,
			      int *online)
{
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(datalen);
    UNUSED_ARGUMENT(namespace_name);
    UNUSED_ARGUMENT(namespace_uid);

    int retval = 0;
    int rv;
    node_map_descriptor_t *nmd = 0;

    if (!data || !datalen) {
    	retval = 1;
	goto err_exit;
    }

    nmd = (node_map_descriptor_t *)
    	  nkn_calloc_type(1, sizeof(node_map_descriptor_t), 
	  		  mod_cl_node_map_descriptor);
    if (!nmd) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"nkn_calloc_type(node_map_descriptor_t) failed");
	retval = 2;
	goto err_exit;
    }
    nmd->get_nodeid = origin_escalation_get_nodeid;
    nmd->cache_name = origin_escalation_cache_name;
    nmd->request_to_origin = origin_escalation_request_to_origin;
    nmd->origin_reply_action = origin_escalation_origin_reply_action;
    nmd->get_config = origin_escalation_get_config;
    nmd->get_node_local_data = origin_escalation_get_node_local_data;
    nmd->node_update = origin_escalation_node_update;
    nmd->free_nmd = origin_escalation_free_nmd;

    nmd->ctx.map_datalen = datalen;
    nmd->ctx.map_data = nkn_malloc_type(datalen, 
    					mod_cl_origin_escalation_map_bin);
    if (!nmd->ctx.map_data) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"nkn_calloc_type(mod_cl_origin_escalation_map_bin) failed");
	retval = 3;
	goto err_exit;
    }
    memcpy(nmd->ctx.map_data, data, datalen);

    nmd->ctx.private_data = 
    	nkn_calloc_type(1, sizeof(origin_escalation_map_private_t), 
	   	    	mod_cl_origin_escalation_map_private_t);
    if (!nmd->ctx.private_data) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"nkn_calloc_type(mod_cl_origin_escalation_map_private) failed");
	retval = 4;
	goto err_exit;
    }
    nmd->ctx.my_node_map = nmd;
    nmd->nsc = nsc;
    
    rv = init_origin_escalaton_map_private_t(nmd, namespace_name, 
					     namespace_uid, old_nsc, online);
    if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"init_origin_escalaton_map_private_t() failed, rv=%d", rv);
	retval = 5;
	goto err_exit;
    }
    return nmd;

err_exit:

    DBG_LOG(MSG, MOD_CLUSTER, "Failed, retval=%d", retval);
    if (nmd) {
    	origin_escalation_free_nmd(nmd);
    }
    return NULL;
}

/*
 * End of nkn_nodemap_originescalationmap.c
 */
