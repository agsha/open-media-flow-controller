/*
 *******************************************************************************
 * nkn_nodemap_noload_clustermap.c -- NodeMap noload ClusterMap specific 
 *				      functions
 *******************************************************************************
 *
 *  Noload clustermap is built upon the ClusterMap NodeMap by using a simple
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

/*
 *******************************************************************************
 * Data declarations 
 *******************************************************************************
 */
typedef struct noload_cluster_map_private {
    node_map_descriptor_t nmd; /* Hold op ptrs to interposed NodeMap */
} noload_cluster_map_private_t;

#define CTX2_LCMP_PRIVATE(_ctx) \
    (noload_cluster_map_private_t *)((_ctx)->my_node_map->private_data)

 /*
  ******************************************************************************
  * Procedure declarations
  ******************************************************************************
  */
static void free_noload_cluster_map_private(noload_cluster_map_private_t *priv);

/*
 *******************************************************************************
 * (node_map_descriptor_t.get_nodeid)() interface function
 *******************************************************************************
 */
static nm_node_id_t
noload_cluster_map_get_nodeid(node_map_context_t *ctx, const char *host_port)
{
    noload_cluster_map_private_t *priv;
    priv = CTX2_LCMP_PRIVATE(ctx);

    return priv->nmd.get_nodeid(ctx, host_port);
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.cache_name)() interface function
 *******************************************************************************
 */
static int
noload_cluster_map_cache_name(node_map_context_t *ctx, const mime_header_t *req,
		       char **host, int *hostlen, uint16_t *port)
{
    noload_cluster_map_private_t *priv;
    priv = CTX2_LCMP_PRIVATE(ctx);

    return priv->nmd.cache_name(ctx, req, host, hostlen, port);
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.request_to_origin)() interface function
 *******************************************************************************
 */
static int
noload_cluster_map_request_to_origin(node_map_context_t *ctx,  
			      request_context_t *req_ctx,
			      long flags,
			      const namespace_config_t *nsc,
			      const mime_header_t *req,
			      char **host, int *hostlen, uint16_t *port,
			      nm_node_id_t *pnode_id, int *ponline_nodes)
{
    noload_cluster_map_private_t *priv;
    priv = CTX2_LCMP_PRIVATE(ctx);

    return priv->nmd.request_to_origin(ctx, req_ctx, flags, nsc, req, 
    				       host, hostlen, 
    				       port, pnode_id, ponline_nodes);
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.reply_action)() interface function
 *******************************************************************************
 */
static int
noload_cluster_map_origin_reply_action(node_map_context_t *ctx,
			        request_context_t *req_ctx,
			      	const mime_header_t *req,
			      	const mime_header_t *reply, int http_reply_code)
{
    noload_cluster_map_private_t *priv;
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
noload_cluster_map_get_config(node_map_context_t *ctx, int *nodes,
                       const char **host_port, const int **host_port_strlen,
                       const char **http_host_port, 
		       const int **http_host_port_strlen,
		       const const char **heartbeat_path, 
		       const int **heartbeat_path_strlen)
{
    noload_cluster_map_private_t *priv;
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
noload_cluster_map_get_node_local_data(node_map_context_t *ctx,
				       uint32_t *node_local_ip)
{
    noload_cluster_map_private_t *priv;
    priv = CTX2_LCMP_PRIVATE(ctx);

    return priv->nmd.get_node_local_data(ctx, node_local_ip);
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.node_update)() interface function
 *******************************************************************************
 */
static int
noload_cluster_map_node_update(node_map_context_t *ctx, int online, int entries,
			       char **hostport, const char **node_handle, 
			       const int *node_handle_strlen)
{
    int rv;
    int n;
    int len;
    char node_handle_str[1024];
    noload_cluster_map_private_t *priv;
    uint64_t actual_vers;
    uint64_t expected_vers;

    /*
     * Note: Single threaded execution since the call is always made 
     *       from the node_status thread.
     */
    priv = CTX2_LCMP_PRIVATE(ctx);

    if (online) {
    	for (n = 0; n < entries; n++) {
	    len = MIN(node_handle_strlen[n], (long)(sizeof(node_handle_str)-1));
	    memcpy(node_handle_str, node_handle[n], len);
	    node_handle_str[len] = '\0';

	    /* 
	     * Verify that the node is using the correct heartbeat 
	     * message protoocol version.
	     */
	    rv = valid_heartbeat_protocol_version(node_handle_str,
	    					  &actual_vers, &expected_vers);
	    if (rv == 0) {
		DBG_LOG(ALARM, MOD_CLUSTER, 
			"Node: [%s] using heartbeat protocol vers=%ld "
			"should be vers=%ld",
			node_handle_str, actual_vers, expected_vers);
		DBG_LOG(ERROR, MOD_CLUSTER, 
			"Node: [%s] using heartbeat protocol vers=%ld "
			"should be vers=%ld",
			node_handle_str, actual_vers, expected_vers);
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
noload_cluster_map_free_nmd(struct node_map_descriptor *nmd)
{
    noload_cluster_map_private_t *priv;
    priv = (noload_cluster_map_private_t *)nmd->private_data;

    priv->nmd.free_nmd(nmd);
    free_noload_cluster_map_private(priv);
}

/*
 *******************************************************************************
 * 		P R I V A T E  F U N C T I O N S 
 *******************************************************************************
 */
static void
free_noload_cluster_map_private(noload_cluster_map_private_t *priv)
{
    free(priv);
}

/*
 *******************************************************************************
 * 		P U B L I C  F U N C T I O N S 
 *******************************************************************************
 */

/*
 *******************************************************************************
 * new_nmd_noload_cluster_map - Create node_map_descriptor_t for noload 
 *				ClusterMap
 *
 *	Return: 0 => Success
 *******************************************************************************
 */
node_map_descriptor_t *
new_nmd_noload_cluster_map(const cluster_map_bin_t *data, int datalen,
		    const char *namespace_name, const char *namespace_uid,
		    const cluster_hash_config_t *ch_cfg,
		    namespace_config_t *nsc, const namespace_config_t *old_nsc,
		    int *online)
{
    int retval = 0;
    node_map_descriptor_t *nmd = 0;
    noload_cluster_map_private_t *priv = 0;

    nmd = new_nmd_cluster_map(data, datalen, namespace_name, namespace_uid, 
			      ch_cfg, nsc, old_nsc, online);
    if (!nmd) {
	DBG_LOG(MSG, MOD_CLUSTER, "new_nmd_cluster_map() failed");
	retval = 1;
	goto err_exit;
    }

    priv = (noload_cluster_map_private_t *)
    	  nkn_calloc_type(1, sizeof(noload_cluster_map_private_t), 
	  		  mod_cl_noload_cluster_map_private_t);
    if (!priv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"nkn_calloc_type(noload_cluster_map_private_t) failed");
	retval = 2;
	goto err_exit;
    }


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

    nmd->get_nodeid = noload_cluster_map_get_nodeid;
    nmd->cache_name = noload_cluster_map_cache_name;
    nmd->request_to_origin = noload_cluster_map_request_to_origin;
    nmd->origin_reply_action = noload_cluster_map_origin_reply_action;
    nmd->get_config = noload_cluster_map_get_config;
    nmd->get_node_local_data = noload_cluster_map_get_node_local_data;
    nmd->node_update = noload_cluster_map_node_update;
    nmd->free_nmd = noload_cluster_map_free_nmd;

    /* Note: err_exit not allowed at this point */

    DBG_LOG(MSG, MOD_CLUSTER, 
	    "Node map created, nmd=%p priv=%p", nmd, priv);
    return nmd;

err_exit:

    DBG_LOG(MSG, MOD_CLUSTER, "Failed, retval=%d", retval);
    if (nmd) {
    	nmd->free_nmd(nmd);
    }
    return NULL;
}

/*
 * End of nkn_nodemap_noload_clustermap.c
 */
