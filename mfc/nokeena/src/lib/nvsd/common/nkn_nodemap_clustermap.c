/*
 *******************************************************************************
 * nkn_nodemap_clustermap.c -- NodeMap ClusterMap specific functions
 *******************************************************************************
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include "nkn_namespace.h"
#include "nkn_nodemap.h"
#include "nkn_nodemap_clustermap.h"
#include "cluster_map_bin.h"
#include "nkn_debug.h"
#include "ketama.h"

/* 
 * Private ClusterMap context data 
 */
typedef struct cluster_map_node {
     map_node_t base;
} cluster_map_node_t;

#define CH_LIBKETAMA_CONFIG_DIR "/nkn/tmp/cluster_hash"

typedef struct cluster_map_private {
     map_private_t base;
     const char *ketama_config_file;
     ketama_continuum ketama_cont;
     const cluster_hash_config_t *ch_cfg;
     uint32_t local_node_ip; // Host byte order
     uint32_t local_node_port; // Host byte order
 } cluster_map_private_t;

/* Forward references */
static int delete_ketama_cont(ketama_continuum cont);
static int detach_delete_ketama_shm(const char *ketama_config_file);
static int update_libketama_config_file(cluster_map_private_t *clm_priv, 
					int create);
static int init_cluster_map_private_t(node_map_descriptor_t *nmd,
	    	       	   	      const char *namespace_name, 
		       	   	      const char *namespace_uid,
				      const cluster_hash_config_t *ch_cfg,
				      const namespace_config_t *nsc,
				      int *online);

/*
 *******************************************************************************
 * (node_map_descriptor_t.get_nodeid)() interface function
 *******************************************************************************
 */
static nm_node_id_t
cluster_map_get_nodeid(node_map_context_t *ctx, const char *host_port)
{
    nm_node_id_t node;
    cluster_map_private_t *clmp = (cluster_map_private_t *)ctx->private_data;

    MP_READ_LOCK(&clmp->base);
    node = host_port_to_node(host_port, &clmp->base);
    MP_UNLOCK(&clmp->base);
    return node;
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.cache_name)() interface function
 *******************************************************************************
 */
static int
cluster_map_cache_name(node_map_context_t *ctx, const mime_header_t *req,
		       char **host, int *hostlen, uint16_t *port)
{
    UNUSED_ARGUMENT(req);
    cluster_map_private_t *clmp = (cluster_map_private_t *)ctx->private_data;

    *host = (char *)clmp->base.cache_name_host;
    *hostlen = clmp->base.cache_name_host_strlen;
    *port = 80;

    return 0;
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.request_to_origin)() interface function
 *******************************************************************************
 */
static int
cluster_map_request_to_origin(node_map_context_t *ctx,  
			      request_context_t *req_ctx,
			      long flags,
			      const namespace_config_t *nsc,
			      const mime_header_t *req,
			      char **host, int *hostlen, uint16_t *port,
			      nm_node_id_t *pnode_id, int *ponline_nodes)
{
    /*
     *	Typical flow (Origin Cluster):
     *	------------------------------
     *	1) cluster_map_cache_name()
     *
     *	2) Async DNS REQ2OS_ORIGIN_SERVER_LOOKUP
     *
     *	3) om_init_client() REQ2OS_ORIGIN_SERVER
     *
     *	Typical flow (L7 Cluster):
     *	--------------------------
     *	1) namespace_request_intercept() flags=0
     */
    UNUSED_ARGUMENT(flags);
    UNUSED_ARGUMENT(nsc);

    int retval = 0;
    cluster_map_private_t *clmp = (cluster_map_private_t *)ctx->private_data;
    const char *data;
    int datalen;
    u_int32_t attrs;
    int hdrcnt;

    mcs *pmcs;
    char *uri;
    int node;
    char *p;
    int online_nodes;
    int lookup = flags & R2OR_FL_LOOKUP;

    if (ctx->my_node_map->nsc->init_in_progress) {
    	return -1; // Not online, retry
    }

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

    /* Use base or full URL based on cluster configuration */

    uri = alloca(datalen+1);
    memcpy(uri, data, datalen);
    uri[datalen] = '\0';
    if (clmp->ch_cfg->ch_uri_base) {
    	if (is_known_header_present(req, MIME_HDR_X_NKN_QUERY)) {
	    /* Drop query string */
	    p = memchr(uri, '?', datalen);
	    if (p) {
	    	*p = '\0';
		datalen = p - uri;
	    }
	}
    	p = memrchr(uri, '/', datalen);
	if (p) {
	    *(p+1) = '\0'; // keep trailing slash
	}
    } else {
	/* Eliminate page anchor */
    	p = memchr(uri, '#', datalen);
	if (p) {
	    *p = '\0';
	    datalen = p - uri;
	}
    }

    MP_READ_LOCK(&clmp->base);
    if ((online_nodes = clmp->base.online_nodes)) {
    	pmcs = ketama_get_server(uri, clmp->ketama_cont);
    } else {
    	pmcs = NULL;
    }

    if (!pmcs) {
    	retval = 2;
    	MP_UNLOCK(&clmp->base);
	goto err_exit;
    }
    node = http_host_port_to_node(pmcs->ip, &clmp->base);
    MP_UNLOCK(&clmp->base);
    if (node < 0) {
    	retval = 3;
	goto err_exit;
    }

    if (!lookup) {
    	INC_NODEMAP_STAT(clmp->base.node[node]->node_stat);
    }
    *host = (char *)clmp->base.node[node]->node_host;
    *hostlen = clmp->base.node[node]->node_host_strlen;
    *port = clmp->base.node[node]->node_http_port;
    if (pnode_id) {
    	*pnode_id = node;
    }
    if (ponline_nodes) {
    	*ponline_nodes = online_nodes;
    }
    if (req_ctx) {
    	SET_REQUEST_CTX_TO_NODE_MAP_CTX(req_ctx, ctx);
    }

    DBG_LOG(MSG2, MOD_CLUSTER, "URI: [%s] => %s", uri, 
	    clmp->base.node[node]->node_host_port);
    return 0;

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
cluster_map_origin_reply_action(node_map_context_t *ctx,
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
cluster_map_get_config(node_map_context_t *ctx, int *nodes,
                       const char **host_port, const int **host_port_strlen,
                       const char **http_host_port, 
		       const int **http_host_port_strlen,
		       const const char **heartbeat_path, 
		       const int **heartbeat_path_strlen)
{
    cluster_map_private_t *clmp = (cluster_map_private_t *)ctx->private_data;

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
cluster_map_get_node_local_data(node_map_context_t *ctx, 
				uint32_t *node_local_ip)
{
    cluster_map_private_t *clmp = (cluster_map_private_t *)ctx->private_data;

    *node_local_ip = clmp->local_node_ip; // host byte order
    return 0;
}

/*
 *******************************************************************************
 * (node_map_descriptor_t.node_update)() interface function
 *******************************************************************************
 */
static int
cluster_map_node_update(node_map_context_t *ctx, int online, int entries, 
			char **hostport, const char **node_handle, 
			const int *node_handle_strlen)
{
    UNUSED_ARGUMENT(node_handle);
    UNUSED_ARGUMENT(node_handle_strlen);
    int retval = 0;
    int rv;
    int update;
    int cluster_init_now_complete;
    cluster_map_private_t *clmp = (cluster_map_private_t *)ctx->private_data;
    char buf[8192];

    MP_WRITE_LOCK(&clmp->base);
    update = update_node_config(&clmp->base, online, entries, hostport, 
    				&cluster_init_now_complete);
    if (update) {
    	rv = update_libketama_config_file(clmp, 0 /* Update */);
    	MP_UNLOCK(&clmp->base);
	if (rv) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "update_libketama_config_file() failed, rv=%d", rv);
	    retval = 1;
	}
	dump_node_config(&clmp->base, NULL, buf, sizeof(buf));
	DBG_LOG(MSG, MOD_CLUSTER, "Node configuration update: %s", buf);
    } else {
    	MP_UNLOCK(&clmp->base);
    }

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
cluster_map_free_nmd(struct node_map_descriptor *nmd)
{
    int rv;
    int n;
    cluster_map_private_t *clm_priv;

    clm_priv = (cluster_map_private_t *) nmd->ctx.private_data;

    if (clm_priv) {
    	if (clm_priv->ketama_cont) {
	    rv = delete_ketama_cont(clm_priv->ketama_cont);
	    if (rv) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"delete_ketama_cont() failed, rv=%d", rv);
	    }
	    clm_priv->ketama_cont = NULL;
	}

	if (clm_priv->ketama_config_file) {
	    rv = detach_delete_ketama_shm(clm_priv->ketama_config_file);
	    if (rv) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"detach_delete_ketama_shm(%s) failed, rv=%d",
			clm_priv->ketama_config_file, rv);
	    }
	    rv = unlink(clm_priv->ketama_config_file);
	    if (rv) {
		DBG_LOG(MSG, MOD_CLUSTER, "unlink(%s) failed, errno=%d",
			clm_priv->ketama_config_file, errno);
	    }
	    free((char *)clm_priv->ketama_config_file);
	    clm_priv->ketama_config_file = NULL;
	}

	for (n = 0; n < clm_priv->base.defined_nodes; n++) {
	    rv = deinit_map_node_t(clm_priv->base.node[n]);
	    if (rv) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"deinit_map_node_t(n=%d) failed, rv=%d", n, rv);
	    }
	    free(clm_priv->base.node[n]);
	    clm_priv->base.node[n] = NULL;
	}

	rv = deinit_map_private_t(&clm_priv->base);
	if (rv) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"deinit_map_private_t() failed, rv=%d", rv);
	}

	free(clm_priv);
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
 * delete_ketama_cont()
 *   Destroy ketama continuum
 *
 *   Return: 0 => Success
 */
static int
delete_ketama_cont(ketama_continuum cont)
{
    int *shmaddr;
    int rv;

    shmaddr = cont->modtime;
    shmaddr--;

    rv = shmdt((char *)shmaddr);
    if (rv == -1) {
	DBG_LOG(MSG, MOD_CLUSTER, "shmdt(%p) failed errno=%d",
		shmaddr, errno);
    }
    ketama_smoke(cont);
    return 0;
}

/*
 * detach_delete_ketama_shm()
 *   Detach and delete the shared memory segment associated with the
 *   ketama configuration file.
 *
 *   Return: 0 => Success
 */
static int 
detach_delete_ketama_shm(const char *ketama_config_file)
{
    int rv;
    int retval = 0;
    key_t key;
    int shmid;
    int semid;

    key = ftok(ketama_config_file, 'R');
    if (key == -1) {
	DBG_LOG(MSG, MOD_CLUSTER, "ftok(%s) failed", ketama_config_file);
	retval = 1;
	goto exit;
    }

    /* Delete associated shared memory segment */
    shmid = shmget(key, 0, 0);
    if (shmid != -1) {
    	rv = shmctl(shmid, IPC_RMID, NULL);
    	if (rv == -1) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "shmctl(shmid=%d, IPC_RMID) failed, errno=%d",
		    shmid, errno);
    	}
    } else {
	DBG_LOG(MSG, MOD_CLUSTER, "shmget(key=%d) failed, errno=%d",
		key, errno);
    }

    /* Delete associated semaphore */
    semid = semget(key, 1, 0);
    if  (semid != -1) {
    	rv = semctl(semid, 0, IPC_RMID);
	if (rv == -1) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "semctl(semid=%d, IPC_RMID) failed, errno=%d",
		    semid, errno);
	}
    } else {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"semget(key=%d) failed, errno=%d", key, errno);
    }

exit:

    return retval;
}

/*
 * invalidate_ketama_shm() 
 *   Invalidate the shared memory segment data to force libketama to regenerate
 *   the continuum.
 *
 *   Return: 0 => Success
 */
static int
invalidate_ketama_shm(const char *ketama_config_file)
{
    int rv;
    int retval = 0;
    key_t key;
    int shmid;
    int *data;
    
    key = ftok(ketama_config_file, 'R');
    if (key == -1) {
	DBG_LOG(MSG, MOD_CLUSTER, "ftok(%s) failed", ketama_config_file);
	retval = 1;
	goto exit;
    }

    shmid = shmget(key, 0, 0);
    if (shmid != -1) {
    	data = shmat(shmid, (void *)0, 0);
	if (data != (void *)-1) {
	    /* Invalidate the shared memory segment data */
	    memset(data, 0, sizeof(int) + sizeof(time_t));
	} else {
	    DBG_LOG(MSG, MOD_CLUSTER, "shmat(shmid=%d) failed, errno=%d",
		    shmid, errno);
	    retval = 2;
	    goto exit;
	}
	rv = shmdt(data);
	if (rv == -1) {
	    DBG_LOG(MSG, MOD_CLUSTER, "shmdt(%p) failed, errno=%d",
		    data, errno);
	    retval = 3;
	}
    } else {
	DBG_LOG(MSG, MOD_CLUSTER, "shmget(key=%d) failed, errno=%d",
		key, errno);
	retval = 4;
    }

exit:

    return retval;
}

/*
 * update_libketama_config_file() - Update or create libketama cluster context
 *
 *	Return: 0 => Return
 */
static int 
update_libketama_config_file(cluster_map_private_t *clm_priv, int create)
{
    int fd = -1;
    int retval;
    int n;
    char buf[1024];
    int bytes;
    ssize_t bytes_written;
    ssize_t total_bytes_written;
    char *err_str;
    int rv;

    if (clm_priv->base.defined_nodes > MAX_CLUSTERMAP_NODES) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"defined_nodes(%d) > MAX_CLUSTERMAP_NODES(%d)",
		clm_priv->base.defined_nodes, MAX_CLUSTERMAP_NODES);
	retval = 10;
	goto err_exit;
    }

    if (!create) {
    	fd = open(clm_priv->ketama_config_file, O_TRUNC|O_RDWR);
	if (fd < 0) {
	    DBG_LOG(MSG, MOD_CLUSTER, "open(%s) failed errno=%d", 
		    clm_priv->ketama_config_file, errno);
	    retval = 1;
	    goto err_exit;
	}

	rv = invalidate_ketama_shm(clm_priv->ketama_config_file);
	if (rv) {
	    /*
	     * Note: At initialization, when receiving the first node update,
	     *	     failure is expected since filesize=0. 
	     */
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "invalidate_ketama_shm(%s) failed, rv=%d",
		    clm_priv->ketama_config_file, rv);
	}
    } else {
    	fd = mkstemp((char *)clm_priv->ketama_config_file);
	if (fd < 0) {
	    DBG_LOG(MSG, MOD_CLUSTER, "mkstemp(%s) failed errno=%d", 
		    clm_priv->ketama_config_file, errno);
	    retval = 2;
	    goto err_exit;
	}
    }

    total_bytes_written = 0;
    for (n = 0; n < clm_priv->base.defined_nodes; n++) {
	if (!clm_priv->base.node[n]->node_online) {
	    continue;
	}
    	bytes = snprintf(buf, sizeof(buf), "%s\t10\n", 
			 clm_priv->base.node[n]->http_node_host_port);
	bytes_written = write(fd, buf, bytes);
	if (bytes_written != bytes) {
	    DBG_LOG(MSG, MOD_CLUSTER, "write(%s) failed, written=%lu sent=%d",
	    	    clm_priv->ketama_config_file, bytes_written, bytes);
	    retval = 4;
	    goto err_exit;
	}
    	total_bytes_written += bytes_written;
    }
    close(fd);
    fd = -1;

    if (total_bytes_written) {
    	if (clm_priv->ketama_cont) {
	    rv = delete_ketama_cont(clm_priv->ketama_cont);
	    if (rv) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"delete_ketama_cont() failed, rv=%d", rv);
	    }
	    clm_priv->ketama_cont = NULL;
	    rv = detach_delete_ketama_shm(clm_priv->ketama_config_file);
	    if (rv) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"detach_delete_ketama_shm(%s) failed, rv=%d",
			clm_priv->ketama_config_file, rv);
	    }
	}
    	rv = ketama_roll(&clm_priv->ketama_cont, 
    		     	 (char *)clm_priv->ketama_config_file);
    } else {
    	rv = 1;
    }

    if (!rv) {
    	err_str = ketama_error();

	DBG_LOG(MSG, MOD_CLUSTER, "ketama_roll(%s) failed, ketama_error=[%s]",
		clm_priv->ketama_config_file, err_str ? err_str : "");
	retval = 5;
	goto err_exit;
    }
    return 0;

err_exit:

    if (fd >= 0) {
    	close(fd);
    }
    return retval;
}

/*
 * init_cluster_map_private_t() - Initialize ClusterMap specific data
 *
 *	Return: 0 => Return
 */
static int 
init_cluster_map_private_t(node_map_descriptor_t *nmd,
	    	       	   const char *namespace_name, 
		       	   const char *namespace_uid,
			   const cluster_hash_config_t *ch_cfg,
			   const namespace_config_t *nsc,
			   int *online)
{
    int rv;
    int retval = 0;
    int n;
    char *phost;
    char *poptions;
    int hostlen;
    const char *val;
    int vallen;
    char buf[1024];
    const char *uid_ns;
    int uid_ns_len;
    in_addr_t ret_ipaddr;

    cluster_map_bin_t *clm_bin = (cluster_map_bin_t *)nmd->ctx.map_data;
    cluster_map_private_t *clm_priv = (cluster_map_private_t *) 
    					nmd->ctx.private_data;

    if (!clm_bin->num_entries) {
	DBG_LOG(MSG, MOD_CLUSTER, "No IP addresses");
    	retval = 1;
    	goto err_exit;
    }

    if (clm_bin->num_entries > MAX_CLUSTER_MAP_NODES) {
	DBG_LOG(MSG, MOD_CLUSTER, "Max nodes exceeded, given=%d max=%d",
		clm_bin->num_entries, MAX_CLUSTER_MAP_NODES);
    	retval = 2;
    	goto err_exit;
    }

    /* Process binary node data */
    for (n = 0; n < (int)clm_bin->num_entries; n++) {
	phost = ISTR(clm_bin, string_table_offset, clm_bin->entry[n].ipaddr);
	hostlen = phost ? strlen(phost) : 0;

    	if (phost && valid_IPaddr(phost, hostlen)) {
	    /* Note: Allocating cluster_map_node_t */
	    clm_priv->base.node[n] = (map_node_t *)
	    			nkn_calloc_type(1, sizeof(cluster_map_node_t),
	    				      	mod_cl_cluster_map_node_t);
	    if (!clm_priv->base.node[n]) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"nkn_calloc_type(cluster_map_node_t) failed");
		retval = 3;
		goto err_exit;
	    }

	    /* Options */
	    poptions = 
	    	ISTR(clm_bin, string_table_offset, clm_bin->entry[n].options);
	    if (extract_options_value(poptions, CLM_OPTIONS_HEARTBEATHPATH, 
	    			      &val, &vallen)) {
	    	val = "";
		vallen = 1;
	    }

	    /* Create "glob_" stat for node */
	    snprintf(buf, sizeof(buf), "CH_%s", namespace_name);

	    rv = init_map_node_t(clm_priv->base.node[n], phost, 
	    			 clm_bin->entry[n].port, val, vallen, buf, nsc);
	    if (rv) {
		DBG_LOG(MSG, MOD_CLUSTER, 
			"init_map_node_t() failed, rv=%d n=%d host=%p port=%hu",
			rv, n, phost, clm_bin->entry[n].port);
		retval = 4;
		goto err_exit;
	    }

	    /* Setup this node's local HTTP IP/Port */
	    if (clm_priv->base.node[n]->local_node) {
	    	ret_ipaddr = inet_addr(phost);
	    	clm_priv->local_node_ip = ntohl(ret_ipaddr);
	    	clm_priv->local_node_port = clm_priv->base.node[n]->node_port;

	    }
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "Added Namespace=%s Node[%d]: "
		    "Node=%s LocalNode=%d Online=%d Heartbeatpath=%s",
		    namespace_name, n, clm_priv->base.node[n]->node_host_port,
		    clm_priv->base.node[n]->local_node,
		    clm_priv->base.node[n]->node_online,
		    (clm_priv->base.node[n]->node_heartbeatpath ?
		    	clm_priv->base.node[n]->node_heartbeatpath : ""));
	} else {
	    DBG_LOG(MSG, MOD_CLUSTER, 
	    	    "Entry[%d]: Invalid IP address [%s]", 
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
    	snprintf(buf, sizeof(buf), "ch-%s", nm);
    } else {
	DBG_LOG(MSG, MOD_CLUSTER, "Invalid namespace_uid (%s)", namespace_uid);
	retval = 6;
	goto err_exit;
    }

    rv = init_map_private_t(&clm_priv->base, buf, online);
    if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"init_map_private_t failed, rv=%d", rv);
	retval = 7;
	goto err_exit;
    }

    /* Generate libketama cluster configuration file base name */
    snprintf(buf, sizeof(buf), "%s/%s_XXXXXX",
    	     CH_LIBKETAMA_CONFIG_DIR, namespace_uid+1);

    clm_priv->ketama_config_file = nkn_strdup_type(buf, mod_cl_filename);

    rv = update_libketama_config_file(clm_priv, 1); // Create file
    if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"update_libketama_config_file(%s) create failed, rv=%d",
		clm_priv->ketama_config_file, rv);
	retval = 8;
	goto err_exit;
    }
    clm_priv->ch_cfg = ch_cfg;

    return 0;

err_exit:

    return retval;
}

/*
 *******************************************************************************
 * 		P U B L I C  F U N C T I O N S 
 *******************************************************************************
 */

/*
 *******************************************************************************
 * new_nmd_cluster_map - Create node_map_descriptor_t for ClusterMap
 *
 *	Return: 0 => Success
 *******************************************************************************
 */
node_map_descriptor_t *
new_nmd_cluster_map(const cluster_map_bin_t *data, int datalen,
		    const char *namespace_name, const char *namespace_uid,
		    const cluster_hash_config_t *ch_cfg,
		    namespace_config_t *nsc, const namespace_config_t *old_nsc,
		    int *online)
{
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
    nmd->get_nodeid = cluster_map_get_nodeid;
    nmd->cache_name = cluster_map_cache_name;
    nmd->request_to_origin = cluster_map_request_to_origin;
    nmd->origin_reply_action = cluster_map_origin_reply_action;
    nmd->get_config = cluster_map_get_config;
    nmd->get_node_local_data = cluster_map_get_node_local_data;
    nmd->node_update = cluster_map_node_update;
    nmd->free_nmd = cluster_map_free_nmd;

    nmd->ctx.map_datalen = datalen;
    nmd->ctx.map_data = nkn_malloc_type(datalen, mod_cl_cluster_map_bin);
    if (!nmd->ctx.map_data) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"nkn_calloc_type(mod_cl_cluster_map_bin) failed");
	retval = 3;
	goto err_exit;
    }
    memcpy(nmd->ctx.map_data, data, datalen);

    nmd->ctx.private_data = nkn_calloc_type(1, sizeof(cluster_map_private_t), 
				   	    mod_cl_cluster_map_private_t);
    if (!nmd->ctx.private_data) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"nkn_calloc_type(mod_cl_cluster_map_private) failed");
	retval = 4;
	goto err_exit;
    }
    nmd->ctx.my_node_map = nmd;
    nmd->nsc = nsc;
    
    rv = init_cluster_map_private_t(nmd, namespace_name, namespace_uid, ch_cfg,
				    old_nsc, online);
    if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, "init_cluster_map_private_t() failed, rv=%d",
		rv);
	retval = 5;
	goto err_exit;
    }
    return nmd;

err_exit:

    DBG_LOG(MSG, MOD_CLUSTER, "Failed, retval=%d", retval);
    if (nmd) {
    	cluster_map_free_nmd(nmd);
    }
    return NULL;
}

/*
 *******************************************************************************
 * nodemap_clustermap_init() - NodeMap ClusterMap system initialization
 *
 *	Return: 0 => Success
 *******************************************************************************
 */
int nodemap_clustermap_init(void) 
{
    DIR *dir = 0;
    struct dirent *dent;
    char ketama_config_filename[1024];
    int rv;

    /*
     * Delete shared memory segments and semaphores associated with 
     * the libketama cluster config files.
     */
    dir = opendir(CH_LIBKETAMA_CONFIG_DIR);
    if (!dir) {
    	DBG_LOG(MSG, MOD_CLUSTER, "opendir(%s) failed", 
		CH_LIBKETAMA_CONFIG_DIR);
	goto exit;
    }

    while ((dent = readdir(dir)) != NULL) {
    	if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) {
	    continue;
	}
    	snprintf(ketama_config_filename, sizeof(ketama_config_filename), 
		 "%s/%s", CH_LIBKETAMA_CONFIG_DIR, dent->d_name);
	rv = detach_delete_ketama_shm(ketama_config_filename);
	if (rv) {
	    DBG_LOG(MSG, MOD_CLUSTER, "detach_delete_ketama_shm(%s), rv=%d",
		    ketama_config_filename, rv);
	}
	rv = unlink(ketama_config_filename);
	if (rv) {
	    DBG_LOG(MSG, MOD_CLUSTER, "unlink(%s) failed, errno=%d",
		    ketama_config_filename, errno);
	}
    }

exit:

    if (dir) {
    	closedir(dir);
    }
    return 0;
}

/*
 * End of nkn_nodemap_clustermap.c
 */
