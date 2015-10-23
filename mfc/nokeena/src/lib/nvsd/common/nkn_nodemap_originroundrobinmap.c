/*
 *******************************************************************************
 * nkn_nodemap_originroundrobinnmap.c -- NodeMap OriginRoundRobinMap
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
#include "nkn_nodemap_originroundrobinmap.h"
#include "origin_round_robin_map_bin.h"
#include "nkn_debug.h"

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
	memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

typedef struct request_context_data_t { // 8 bytes, see request_context_t def
    int32_t last_node;
    int32_t unused;
} request_context_data_t;

typedef struct origin_roundrobin_map_node {
    map_node_t base;
    int node_round_robin_weight;
} origin_roundrobin_map_node_t;

typedef struct rr_context { // Round Robin scheduler context
    AO_t index;
    int total_members;
    int member[MAX_ORIGINRR_MAP_NODES];
    int weight[MAX_ORIGINRR_MAP_NODES];
    int sequence[MAX_ORIGINRR_MAP_NODES * MAX_ORIGINRR_MAP_RRWEIGHT];
    int max_weight;
    int sum_weights;
    int gcd;
} rr_context_t;

typedef struct origin_roundrobin_map_private {
    map_private_t base;
    // Round Robin scheduler data
    rr_context_t rr_ctx;
} origin_roundrobin_map_private_t;

/* Forward references */
static int
print_origin_roundrobin_map_node_t(map_node_t *mp, char *buf, int buflen);

static int 
update_roundrobin_sched_context(origin_roundrobin_map_private_t *orrm_priv,
			    	int create);
static int 
dump_rr_context(rr_context_t *ctx, char *buf, int buflen);
 
/*
 *******************************************************************************
 * (node_map_descriptor_t.get_nodeid)() interface function
 *******************************************************************************
 */
static nm_node_id_t
origin_roundrobin_get_nodeid(node_map_context_t *ctx, const char *host_port)
{
    nm_node_id_t node;
    origin_roundrobin_map_private_t *oemp = 
    	(origin_roundrobin_map_private_t *)ctx->private_data;

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
origin_roundrobin_cache_name(node_map_context_t *ctx, const mime_header_t *req,
		       	     char **host, int *hostlen, uint16_t *port)
{
    UNUSED_ARGUMENT(req);

    origin_roundrobin_map_private_t *oemp = 
    	(origin_roundrobin_map_private_t *)ctx->private_data;

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
origin_roundrobin_request_to_origin(node_map_context_t *ctx,  
				    request_context_t *req_ctx,
				    long flags,
				    const namespace_config_t *nsc,
				    const mime_header_t *req,
				    char **host, int *hostlen, uint16_t *port,
				    nm_node_id_t *pnode_id, int *ponline_nodes)
{
    /*
     *	Typical flow:
     *	-------------
     *	1) origin_roundrobin_cache_name()
     *
     *	2) Async DNS REQ2OS_ORIGIN_SERVER_LOOKUP
     *
     *	3) om_init_client() REQ2OS_ORIGIN_SERVER
     */
    UNUSED_ARGUMENT(req_ctx);
    UNUSED_ARGUMENT(flags);
    UNUSED_ARGUMENT(nsc);
    UNUSED_ARGUMENT(req);

    int lookup = flags & R2OR_FL_LOOKUP;
    int retval = 0;
    int online_nodes;
    int64_t sched_index;
    int node;

    request_context_data_t *req_ctx_data;
    origin_roundrobin_map_private_t *orrmp = 
    	(origin_roundrobin_map_private_t *)ctx->private_data;

    if (ctx->my_node_map->nsc->init_in_progress) {
    	return -1; // Not online, retry
    }

    if (req_ctx) {
    	req_ctx_data = (request_context_data_t *)REQUEST_CTX_DATA(req_ctx);
    }

    if (lookup) {
    	node = -1;
    } else {
	if (req_ctx) {
	    if (REQUEST_CTX_TO_NODE_MAP_CTX(req_ctx) == ctx) {
	    	node = req_ctx_data->last_node;
	    } else {
	   	node = -1;
	    }
    	} else {
	    node = -1;
    	}
    }

    MP_READ_LOCK(&orrmp->base);
    if ((online_nodes = orrmp->base.online_nodes) == 0) {
	retval = 1;
	MP_UNLOCK(&orrmp->base);
	goto err_exit;
    }
    if (node < 0) {
    	sched_index = 
	    AO_fetch_and_add1(&orrmp->rr_ctx.index) % orrmp->rr_ctx.sum_weights;
    	node = orrmp->rr_ctx.sequence[sched_index];
    }
    MP_UNLOCK(&orrmp->base);

    if (!lookup) {
    	INC_NODEMAP_STAT(orrmp->base.node[node]->node_stat);
    }
    *host = (char *)orrmp->base.node[node]->node_host;
    *hostlen = orrmp->base.node[node]->node_host_strlen;
    *port = orrmp->base.node[node]->node_http_port;
    if (pnode_id) {
        *pnode_id = node;
    }
    if (ponline_nodes) {
        *ponline_nodes = online_nodes;
    }
    if (req_ctx) {
    	if (lookup) {
	    SET_REQUEST_CTX_TO_NODE_MAP_CTX(req_ctx, ctx);
	    req_ctx_data->last_node = node;
	} else {
	    SET_REQUEST_CTX_TO_NODE_MAP_CTX(req_ctx, NULL);
	    req_ctx_data->last_node = -1;
	}
    }
    DBG_LOG(MSG2, MOD_CLUSTER, "Select: %s", 
	    orrmp->base.node[node]->node_host_port);
    return retval;

err_exit:

    DBG_LOG(MSG2, MOD_CLUSTER, "Failed, reason=%d", retval);
    return retval;
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.reply_action)() interface function
 *******************************************************************************
 */
static int
origin_roundrobin_origin_reply_action(node_map_context_t *ctx,
				request_context_t *req_ctx,
			      	const mime_header_t *req,
			      	const mime_header_t *reply, int http_reply_code)
{
    UNUSED_ARGUMENT(ctx);
    UNUSED_ARGUMENT(req_ctx);
    UNUSED_ARGUMENT(req);
    UNUSED_ARGUMENT(reply);
    UNUSED_ARGUMENT(http_reply_code);
    if (req_ctx) {
    	SET_REQUEST_CTX_TO_NODE_MAP_CTX(req_ctx, NULL);
    }
    return 0; // No retry
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.get_config)() interface function
 *******************************************************************************
 */
static int 
origin_roundrobin_get_config(node_map_context_t *ctx, int *nodes,
                       	     const char **host_port, 
			     const int **host_port_strlen,
                       	     const char **http_host_port, 
			     const int **http_host_port_strlen,
			     const const char **heartbeat_path, 
			     const int **heartbeat_path_strlen)
{
    origin_roundrobin_map_private_t *clmp = 
    	(origin_roundrobin_map_private_t *)ctx->private_data;

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
origin_roundrobin_get_node_local_data(node_map_context_t *ctx,
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
origin_roundrobin_node_update(node_map_context_t *ctx, int online, int entries, 
			      char **hostport, const char **node_handle, 
			      const int *node_handle_strlen)
{
    UNUSED_ARGUMENT(node_handle);
    UNUSED_ARGUMENT(node_handle_strlen);

    int retval = 0;
    int rv;
    int update;
    int cluster_init_now_complete;
    char buf[8192];
    origin_roundrobin_map_private_t *orrmp = 
    	(origin_roundrobin_map_private_t *)ctx->private_data;

    MP_WRITE_LOCK(&orrmp->base);
    update = update_node_config(&orrmp->base, online, entries, hostport, 
    				&cluster_init_now_complete);
    if (update) {
	dump_node_config(&orrmp->base, print_origin_roundrobin_map_node_t,
			 buf, sizeof(buf));
	DBG_LOG(MSG, MOD_CLUSTER, "Node configuration update: %s", buf);

	rv = update_roundrobin_sched_context(orrmp, 0);
    	if (!rv) {
	    dump_rr_context(&orrmp->rr_ctx, buf, sizeof(buf));
	    DBG_LOG(MSG, MOD_CLUSTER, "Round Robin context: %s", buf);
	} else {
	    DBG_LOG(MSG, MOD_CLUSTER,
		    "update_roundrobin_sched_context() failed, rv=%d", rv);
	    retval = 1;
    	}
    }
    MP_UNLOCK(&orrmp->base);

    if (cluster_init_now_complete) {
    	/* Reflect the init => online transition in namespace_config */
    	AO_fetch_and_sub1(&ctx->my_node_map->nsc->init_in_progress);
    }

    return retval;
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.free_nmd)() interface function
 *******************************************************************************
 */
static void
origin_roundrobin_free_nmd(struct node_map_descriptor *nmd)
{
    int rv;
    int n;
    origin_roundrobin_map_private_t *orrm_priv =
    	(origin_roundrobin_map_private_t *) nmd->ctx.private_data;

    if (orrm_priv) {
	for (n = 0; n < orrm_priv->base.defined_nodes; n++) {

	    /* Free origin_roundrobin_map_node_t specific data */

	    rv = deinit_map_node_t(orrm_priv->base.node[n]);
	    if (rv) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"deinit_map_node_t(n=%d) failed, rv=%d", n, rv);
	    }
	    free(orrm_priv->base.node[n]);
	    orrm_priv->base.node[n] = NULL;
	}

	rv = deinit_map_private_t(&orrm_priv->base);
	if (rv) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "deinit_map_private_t() failed, rv=%d", rv);
	}

	free(orrm_priv);
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
 * dump_rr_context() - Dump Round Robin context
 *
 *	Return: bytes written
 */
static int 
dump_rr_context(rr_context_t *ctx, char *buf, int buflen)
{
#define UPDATEBUF(_bytes) \
{ \
    bytes_used += (_bytes); \
    if (bytes_used >= buflen) { \
    	goto err_exit; \
    } \
}

    int rv;
    int n;
    int bytes_used = 0;

    rv = snprintf(&buf[bytes_used], buflen - bytes_used,
		  "\n\tMembers=%d MaxWeight=%d SumWeights=%d GCD=%d\n",
		  ctx->total_members, ctx->max_weight, ctx->sum_weights, 
		  ctx->gcd);
    UPDATEBUF(rv);

    rv = snprintf(&buf[bytes_used], buflen - bytes_used, "\tmember[]= ");
    UPDATEBUF(rv);

    for (n = 0; n < ctx->total_members; n++) {
    	rv = snprintf(&buf[bytes_used], buflen - bytes_used, "%d ",
		      ctx->member[n]);
    	UPDATEBUF(rv);
    }

    rv = snprintf(&buf[bytes_used], buflen - bytes_used, "\n\tweight[]= ");
    UPDATEBUF(rv);

    for (n = 0; n < ctx->total_members; n++) {
    	rv = snprintf(&buf[bytes_used], buflen - bytes_used, "%d ",
		      ctx->weight[n]);
    	UPDATEBUF(rv);
    }

    rv = snprintf(&buf[bytes_used], buflen - bytes_used, "\n\tsequence[]= ");
    UPDATEBUF(rv);

    for (n = 0; n < ctx->sum_weights; n++) {
    	rv = snprintf(&buf[bytes_used], buflen - bytes_used, "%d ",
		      ctx->sequence[n]);
    	UPDATEBUF(rv);
    }

    rv = snprintf(&buf[bytes_used], buflen - bytes_used, "\n");
    UPDATEBUF(rv);

err_exit:

    return bytes_used;
}

/*
 * compute_weighted_rr() - Compute next entry in the given weighted round robin 
 *			   context.
 *	The input values: entries, weight[], max_weight and gcd are > 0.
 *
 *	Return: index in weight[] array.
 */
static int
compute_weighted_rr(int entries, int *weight, int max_weight, int gcd,
		    int *current_index, int *current_weight)
{
    int node_index;

    while (1) {
	node_index = *current_index;
   	if (!node_index) {
	    *current_weight = *current_weight - gcd;
	    if (*current_weight <= 0) {
	    	*current_weight = max_weight;
	    }
   	}
	*current_index = (*current_index + 1) % entries;
	if (weight[node_index] >= *current_weight) {
	    break;
	}
    }
    return node_index;
}

/*
 * greatest_common_divisor() - Compute GCD for given int(s).
 *
 *	All input data values are > 0.
 *
 *	Return: GCD
 */
static int 
greatest_common_divisor(int numvals, int *values, int maxval)
{
    int n;
    int divisor = 0;

    for (divisor = maxval; divisor != 0; divisor--) {
	for (n = 0; n < numvals; n++) {
	    if (values[n] % divisor) {
		break;
	    }
	}
	if (n >= numvals) {
	    break;
	}
    }
    return divisor;
}

/*
 * update_roundrobin_sched_context() - Update or create the round robin 
 *				       scheduler context.
 *	Return: 0 => Success
 */
static int 
update_roundrobin_sched_context(origin_roundrobin_map_private_t *orrm_priv,
			    	int create)
{
    UNUSED_ARGUMENT(create);

    int n;
    int rr_node;
    int ix;
    int current_index;
    int current_weight;

    memset((char *)&orrm_priv->rr_ctx, 0, sizeof(orrm_priv->rr_ctx));

    for (n = 0; n < orrm_priv->base.defined_nodes; n++) {
	if (orrm_priv->base.node[n]->node_online) { // Online node
	    rr_node = orrm_priv->rr_ctx.total_members;
	    orrm_priv->rr_ctx.member[rr_node] = n;
	    orrm_priv->rr_ctx.weight[rr_node] =
	    	((origin_roundrobin_map_node_t *)
		    orrm_priv->base.node[n])->node_round_robin_weight;

	    if (orrm_priv->rr_ctx.weight[rr_node] > 
		orrm_priv->rr_ctx.max_weight) {
	    	orrm_priv->rr_ctx.max_weight = 
		    orrm_priv->rr_ctx.weight[rr_node];
	    }
	    orrm_priv->rr_ctx.sum_weights += orrm_priv->rr_ctx.weight[rr_node];
	    orrm_priv->rr_ctx.total_members++;
	} else {
	    continue;
	}
    }

    if (orrm_priv->rr_ctx.total_members) {
    	orrm_priv->rr_ctx.gcd = 
	    greatest_common_divisor(orrm_priv->rr_ctx.total_members,
				    orrm_priv->rr_ctx.weight,
				    orrm_priv->rr_ctx.max_weight);
	current_index = 0;
	current_weight = 0;

	for (n = 0; n < orrm_priv->rr_ctx.sum_weights; n++) {
	    ix = compute_weighted_rr(orrm_priv->rr_ctx.total_members,
				     orrm_priv->rr_ctx.weight, 
				     orrm_priv->rr_ctx.max_weight,
				     orrm_priv->rr_ctx.gcd,
				     &current_index, &current_weight);
	    orrm_priv->rr_ctx.sequence[n] = orrm_priv->rr_ctx.member[ix];
	}
	    
    }
    return 0;
}

/*
 * init_origin_roundrobin_map_private_t() - Initialize OriginRoundRobinMap  
 *					    specific data
 *
 *	Return: 0 => Return
 */
static int
init_origin_roundrobin_map_private_t(node_map_descriptor_t *nmd,
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
    origin_roundrobin_map_node_t *orrm_node;
    char buf[8192];
    const char *uid_ns;
    int uid_ns_len;

    origin_round_robin_map_bin_t *orrm_bin = 
    	(origin_round_robin_map_bin_t *) nmd->ctx.map_data;

    origin_roundrobin_map_private_t *orrm_priv =
    	(origin_roundrobin_map_private_t *) nmd->ctx.private_data;

    if (!orrm_bin->num_entries) {
	DBG_LOG(MSG, MOD_CLUSTER, "No entries");
    	retval = 1;
    	goto err_exit;
    }

    if (orrm_bin->num_entries > MAX_ORIGINRR_MAP_NODES) {
	DBG_LOG(MSG, MOD_CLUSTER, "Max nodes exceeded, given=%d max=%d",
		orrm_bin->num_entries, MAX_ORIGINRR_MAP_NODES);
    	retval = 2;
    	goto err_exit;
    }

    /* Process binary node data */
    for (n = 0; n < (int)orrm_bin->num_entries; n++) {
    	phost = ISTR(orrm_bin, string_table_offset, orrm_bin->entry[n].origin);
	hostlen = phost ? strlen(phost) : 0;

	if (phost && valid_FQDN(phost, hostlen)) {
	    /* Note: Allocating origin_roundrobin_map_node_t */
	    orrm_priv->base.node[n] = (map_node_t *)
		    nkn_calloc_type(1, sizeof(origin_roundrobin_map_node_t),
				    mod_cl_origin_roundrobin_map_node_t);
	    if (!orrm_priv->base.node[n]) {
		DBG_LOG(MSG, MOD_CLUSTER, "nkn_calloc_type(map_node_t) failed");
		retval = 3;
		goto err_exit;
	    }

	    /* Heartbeat option */
	    poptions = 
	    	ISTR(orrm_bin, string_table_offset, orrm_bin->entry[n].options);
	    if (extract_options_value(poptions, ORRRM_OPTIONS_HEARTBEATHPATH, 
	    			      &val, &vallen)) {
	    	val = "";
		vallen = 1;
	    }

	    /* Create "glob_" stat for node */
	    snprintf(buf, sizeof(buf), "RR_%s", namespace_name);

	    rv = init_map_node_t(orrm_priv->base.node[n], phost, 
	    			 orrm_bin->entry[n].port, val, vallen, 
				 buf, nsc);
	    if (rv) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"init_map_node_t() failed, rv=%d n=%d host=%p port=%hu",
			rv, n, phost, orrm_bin->entry[n].port);
		retval = 4;
		goto err_exit;
	    }

	    /* Setup origin_roundrobin_map_node_t specific data */

	    orrm_node = (origin_roundrobin_map_node_t *)orrm_priv->base.node[n];

	    if (!extract_options_value(poptions, ORRRM_OPTIONS_RRWEIGHT, 
	    			       &val, &vallen)) {
	    	lval = atol_len(val, vallen);
	    	if (lval <= 0) {
		    orrm_node->node_round_robin_weight = 1;
	    	} else {
		    if (lval > MAX_ORIGINRR_MAP_RRWEIGHT) {
		    	orrm_node->node_round_robin_weight = 
			    MAX_ORIGINRR_MAP_RRWEIGHT;
		    } else {
		    	orrm_node->node_round_robin_weight = lval;
		    }
	    	}
	    } else {
		orrm_node->node_round_robin_weight = 1;
	    }

	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "Added Namespace=%s Node[%d]: %s "
		    "Online=%d Heartbeatpath=%s rr_weight=%d",
		    namespace_name, n, orrm_priv->base.node[n]->node_host_port,
		    orrm_priv->base.node[n]->node_online,
		    (orrm_priv->base.node[n]->node_heartbeatpath ?
		    	orrm_priv->base.node[n]->node_heartbeatpath : ""),
		    orrm_node->node_round_robin_weight);
	} else {
	    DBG_LOG(MSG, MOD_CLUSTER,
		    "Entry[%d]: Invalid FQDN address [%s]",
		    n, (phost ? phost : ""));
	    retval = 5;
	    goto err_exit;
	}
    }

    /* Generate the host portion of the cache name */
    decompose_ns_uri(namespace_uid, &uid_ns, &uid_ns_len, 0, 0, 0, 0, 0, 0, 0);
    if (uid_ns) {
        char *nm = alloca(uid_ns_len + 1);
	memcpy(nm, uid_ns, uid_ns_len);
	nm[uid_ns_len] = '\0';
    	snprintf(buf, sizeof(buf), "rr-%s", nm);
    } else {
    	DBG_LOG(MSG, MOD_CLUSTER, "Invalid namespace_uid (%s)", namespace_uid);
	retval = 6;
	goto err_exit;
    }

    rv = init_map_private_t(&orrm_priv->base, buf, online);
    if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"init_map_private_t failed, rv=%d", rv);
	retval = 7;
	goto err_exit;
    }

    /* Generate round robin scheduler context */
    rv = update_roundrobin_sched_context(orrm_priv, 1);
    if (rv) {
    	DBG_LOG(MSG, MOD_CLUSTER,
		"update_roundrobin_sched_context() failed, rv=%d", rv);
	retval = 8;
	goto err_exit;
    }

    dump_node_config(&orrm_priv->base, print_origin_roundrobin_map_node_t, 
    		     buf, sizeof(buf));
    DBG_LOG(MSG, MOD_CLUSTER, "Node configuration: %s", buf);

    dump_rr_context(&orrm_priv->rr_ctx, buf, sizeof(buf));
    DBG_LOG(MSG, MOD_CLUSTER, "Round Robin context: %s", buf);

err_exit:

    return retval;
}

/* dump_node_config() print callback */
static int
print_origin_roundrobin_map_node_t(map_node_t *mp, char *buf, int buflen)
{
    int rv;
    origin_roundrobin_map_node_t *orrmp = (origin_roundrobin_map_node_t *) mp;
    rv = snprintf(buf, buflen, "\tround_robin_weight=%d\n",
    		  orrmp->node_round_robin_weight);
    return rv;
}

/*
 *******************************************************************************
 * 		P U B L I C  F U N C T I O N S 
 *******************************************************************************
 */

/*
 *******************************************************************************
 * new_nmd_origin_roundrobin_map - Create node_map_descriptor_t for 
 *				   OriginRoundRobinMap
 *
 *	Return: 0 => Success
 *******************************************************************************
 */
node_map_descriptor_t *
new_nmd_origin_roundrobin_map(const origin_round_robin_map_bin_t *data, 
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
    nmd->get_nodeid = origin_roundrobin_get_nodeid;
    nmd->cache_name = origin_roundrobin_cache_name;
    nmd->request_to_origin = origin_roundrobin_request_to_origin;
    nmd->origin_reply_action = origin_roundrobin_origin_reply_action;
    nmd->get_config = origin_roundrobin_get_config;
    nmd->get_node_local_data = origin_roundrobin_get_node_local_data;
    nmd->node_update = origin_roundrobin_node_update;
    nmd->free_nmd = origin_roundrobin_free_nmd;

    nmd->ctx.map_datalen = datalen;
    nmd->ctx.map_data = nkn_malloc_type(datalen, 
    					mod_cl_origin_roundrobin_map_bin);
    if (!nmd->ctx.map_data) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"nkn_calloc_type(mod_cl_origin_roundrobin_map_bin) failed");
	retval = 3;
	goto err_exit;
    }
    memcpy(nmd->ctx.map_data, data, datalen);

    nmd->ctx.private_data = 
    	nkn_calloc_type(1, sizeof(origin_roundrobin_map_private_t), 
	   	    	mod_cl_origin_roundrobin_map_private_t);
    if (!nmd->ctx.private_data) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"nkn_calloc_type(mod_cl_origin_roundrobin_map_private) failed");
	retval = 4;
	goto err_exit;
    }
    nmd->ctx.my_node_map = nmd;
    nmd->nsc = nsc;
    
    rv = init_origin_roundrobin_map_private_t(nmd, namespace_name, 
					      namespace_uid, old_nsc, online);
    if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"init_origin_roundrobin_map_private_t() failed, rv=%d", rv);
	retval = 5;
	goto err_exit;
    }
    return nmd;

err_exit:

    DBG_LOG(MSG, MOD_CLUSTER, "Failed, retval=%d", retval);
    if (nmd) {
    	origin_roundrobin_free_nmd(nmd);
    }
    return NULL;
}

/*
 * End of nkn_nodemap_originroundrobinnmap.c
 */
