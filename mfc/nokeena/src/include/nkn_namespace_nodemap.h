/*
 *******************************************************************************
 * nkn_namespace_nodemap.h -- NodeMap support functions
 *******************************************************************************
 */

#ifndef _NKN_NAMESPACE_NODEMAP_H
#define _NKN_NAMESPACE_NODEMAP_H

#include "nkn_namespace.h"
#include "cluster_map_bin.h"
#include "origin_escalation_map_bin.h"
#include "origin_round_robin_map_bin.h"

/*
 * NodeMap is a generic term for a class of request to origin server map
 * types which require node status to perform the mapping.
 */
#define MAX_CLUSTER_MAP_NODES 256 // always equal to MAX_CMM_DATA_ELEMENTS

/*
 * Generic context data associated with a particular NodeMap type
 */
typedef struct node_map_context {
    void *map_data;
    int map_datalen;
    void *private_data; // Always pointer to xxx_map_private_t
    struct node_map_descriptor *my_node_map;
} node_map_context_t;

typedef int nm_node_id_t;

/*
 * NodeMap descriptor, which is the public interface, associated with a 
 * particular NodeMap type
 */
typedef struct node_map_descriptor {
    /* 
     * nm_node_id_t get_nodeid(const char *host_port) - Return >=0 Success
     */
    nm_node_id_t (*get_nodeid)(node_map_context_t *ctx, const char *host_port);
    /* 
     * int (*cache_name)() - Return: 0 => Success
     */
    int (*cache_name)(node_map_context_t *ctx, const mime_header_t *req, 
    		      char **host, int *hostlen, uint16_t *port);
    /* 
     * int (*request_to_origin)() - Return: 0 => Success
     *					  < 0 => Init in progress, retry
     * 	pnode_id and ponline_nodes are optional args (ignore with NULL)
     */
    int (*request_to_origin)(node_map_context_t *ctx, 
			     request_context_t *req_ctx,
			     long flags, // See R2OR_FL_XXX defines
			     const namespace_config_t *nsc,
			     const mime_header_t *req, 
    			     char **host, int *hostlen, uint16_t *port,
			     nm_node_id_t *pnode_id, int *ponline_nodes);
#define R2OR_FL_LOOKUP 0x1 // req_ctx is readonly
#define R2OR_FL_SET 0x2 // Set origin server in req_ctx
    /* 
     * int (*origin_reply_action)() - Return: !=0 => Retry request
     */
    int (*origin_reply_action)(node_map_context_t *ctx,
			       request_context_t *req_ctx,
    			       const mime_header_t *req,
    			       const mime_header_t *reply, int http_reply_code);
#define HTTP_REPLY_CODE_CONNECT_TIMEOUT -1
#define HTTP_REPLY_CODE_READ_TIMEOUT -2

    /*
     * int (*get_config)() - Returns: 0 => Success
     */
    int (*get_config)(node_map_context_t *ctx, int *nodes, 
    		      const char **host_port, const int **host_port_strlen,
    		      const char **http_host_port, 
		      const int **http_host_port_strlen,
		      const char **heartbeat_path, 
		      const int **heartbeat_path_strlen);
    /*
     * int (*get_node_local_data() - Returns: 0 => Success
     */
    int (*get_node_local_data)(node_map_context_t *ctx, 
    			       uint32_t *node_local_ip);
    /* 
     * int (*node_update)() - Return: 0 => Success
     */
    int (*node_update)(node_map_context_t *ctx, int online, int entries, 
    		       char **hostport, 
		       const char **node_handle, const int *node_handle_strlen);

    /* 
     * int (*free_nmd)()
     */
    void (*free_nmd)(struct node_map_descriptor *nmd);
    node_map_context_t ctx;
    namespace_config_t *nsc;

    /*
     * CMM configurables
     */
    int cmm_heartbeat_interval;
    int cmm_connect_timeout;
    int cmm_allowed_connect_failures;
    int cmm_read_timeout;

    /*
     * Private data for NodeMap interposer
     */
    void *private_data;
} node_map_descriptor_t;

#define REQUEST_CTX_TO_NODE_MAP_CTX(_req_ctx) \
    (node_map_context_t *)((_req_ctx)->opaque[0])

#define SET_REQUEST_CTX_TO_NODE_MAP_CTX(_req_ctx, _nm_ctx) \
    (_req_ctx)->opaque[0] = (uint64_t)(_nm_ctx);

#define REQUEST_CTX_DATA(_req_ctx) &(_req_ctx)->opaque[1]

/*
 * cluster_nodemap_init()
 *   Cluster NodeMap system initialization
 */
int cluster_nodemap_init(int (*is_local_ip_proc)(const char *ip, int ipstrlen));

/*
 * new_nmd_cluster_map()
 *   Passed data, datalen is valid only during invocation.
 *   Return: !=0 => Success (valid memory address)
 *	     online != 0 => init complete, available for access
 */
node_map_descriptor_t *
new_nmd_cluster_map(const cluster_map_bin_t *data, int datalen,
		    const char *namespace_name, const char *namespace_uid,
		    const cluster_hash_config_t *ch_cfg,
		    namespace_config_t *nsc, const namespace_config_t *old_nsc,
		    int *online);

/*
 * new_nmd_noload_cluster_map()
 *   Passed data, datalen is valid only during invocation.
 *   Return: !=0 => Success (valid memory address)
 *	     online != 0 => init complete, available for access
 */
node_map_descriptor_t *
new_nmd_noload_cluster_map(const cluster_map_bin_t *data, int datalen,
		    const char *namespace_name, const char *namespace_uid,
		    const cluster_hash_config_t *ch_cfg,
		    namespace_config_t *nsc, const namespace_config_t *old_nsc,
		    int *online);

/*
 * new_nmd_load_cluster_map()
 *   Passed data, datalen is valid only during invocation.
 *   Return: !=0 => Success (valid memory address)
 *	     online != 0 => init complete, available for access
 */
node_map_descriptor_t *
new_nmd_load_cluster_map(const cluster_map_bin_t *data, int datalen,
		    const char *namespace_name, const char *namespace_uid,
		    const char *stat_prefix,
		    const cluster_hash_config_t *ch_cfg,
		    const cl_redir_lb_attr_t *lb_cfg,
		    namespace_config_t *nsc, const namespace_config_t *old_nsc,
		    int *online);
/*
 * load_cluster_map_init()
 *   System initialization.
 */
int
load_cluster_map_init(void);

/*
 * new_nmd_origin_escalation_map()
 *   Passed data, datalen is valid only during invocation.
 *   Return: !=0 => Success (valid memory address)
 *	     online != 0 => init complete, available for access
 */
node_map_descriptor_t *
new_nmd_origin_escalation_map(const origin_escalation_map_bin_t *data, 
			      int datalen,
			      const char *namespace_name, 
			      const char *namespace_uid,
			      namespace_config_t *nsc,
			      const namespace_config_t *old_nsc,
			      int *online);

/*
 * new_nmd_origin_round_robin_map()
 *   Passed data, datalen is valid only during invocation.
 *   Return: !=0 => Success (valid memory address)
 *	     online != 0 => init complete, available for access
 */
node_map_descriptor_t *
new_nmd_origin_roundrobin_map(const origin_round_robin_map_bin_t *data, 
			      int datalen,
			      const char *namespace_name, 
			      const char *namespace_uid,
			      namespace_config_t *nsc,
			      const namespace_config_t *old_nsc,
			      int *online);

/*
 * delete_node_map_descriptor_t()
 *   Destroy node_map_descriptor_t
 */
void delete_node_map_descriptor_t(node_map_descriptor_t *nmd);

#endif  /* _NKN_NAMESPACE_NODEMAP_H */

/*
 * End of nkn_namespace_nodemap.h
 */
