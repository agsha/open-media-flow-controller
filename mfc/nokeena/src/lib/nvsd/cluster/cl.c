/*
 * cl.c - Cluster subsystem support functions
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

#include "nkn_cl.h"
#include "cl.h"
#include "cl_cmm.h"
#include "cl_node_status.h"
#include "fqueue.h"
#include "http_header.h"
#include "nkn_cmm_request.h"
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_errno.h"
#include "nkn_namespace.h"

static int
node_map_monitor_action(node_map_descriptor_t *nmd, namespace_token_t token, 
			const namespace_config_t *nsc, int online, int instance)
{
    int rv;
    int retval = 0;
    int nodes;
    const char *host_port[MAX_CLUSTER_MAP_NODES];
    const int *host_port_strlen[MAX_CLUSTER_MAP_NODES];
    const char *http_host_port[MAX_CLUSTER_MAP_NODES];
    const int *http_host_port_strlen[MAX_CLUSTER_MAP_NODES];
    const char *heartbeat_path[MAX_CLUSTER_MAP_NODES];
    const int *heartbeat_path_strlen[MAX_CLUSTER_MAP_NODES];

    // Get node configuration
    rv = (*nmd->get_config)(&nmd->ctx, &nodes, 
			    &host_port[0], 
			    &host_port_strlen[0],
			    &http_host_port[0], 
			    &http_host_port_strlen[0],
			    &heartbeat_path[0], 
			    &heartbeat_path_strlen[0]);
    if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"get_config() failed, rv=%d instance=%d "
		"namespace=%s token=0x%lx", 
		rv, instance, nsc->namespace, token.u.token);
	retval = 1;
	goto exit;
    }

    if (online) {
	// Send monitor nodes message to CMM
	rv = send_cmm_monitor(nodes, host_port, host_port_strlen,
			      heartbeat_path, heartbeat_path_strlen,
			      token, instance);
	if (rv) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "send_cmm_monitor() failed, rv=%d instance=%d "
		    "namespace=%s token=0x%lx", 
		    rv, instance, nsc->namespace, token.u.token);
	    retval = 2;
	    goto exit;
	}
    } else {
	// Send cancel monitor nodes message to CMM
	rv = send_cmm_cancel_monitor(nodes, host_port, host_port_strlen,
				     token, instance);
	if (rv) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "send_cmm_cancel_monitor() failed, rv=%d instance=%d "
		    "namespace=%s token=0x%lx", 
		    rv, instance, nsc->namespace, token.u.token);
	    retval = 3;
	    goto exit;
	}
    }

exit:

    return retval;
}

int 
CL_namspace_update(namespace_token_t token, const namespace_config_t *nsc, 
		   int online)
{
    int rv;
    int n;
    origin_server_t *osc;
    struct node_map_descriptor *node_map;

    if (nsc->http_config &&
	(nsc->http_config->origin.select_method == OSS_HTTP_NODE_MAP)) {
    	osc = &nsc->http_config->origin;
	for (n = 0; n < osc->u.http.node_map_entries; n++) {
	    rv = node_map_monitor_action(osc->u.http.node_map[n], token, nsc, 
	    				 online, n);
	    if (rv) {
	    	DBG_LOG(MSG, MOD_CLUSTER, "node_map_monitor_action() failed, "
	    	    	"rv=%d online=%d instance=%d namespace=%s token=0x%lx",
		    	rv, online, n, nsc->namespace, token.u.token);
	    }
	}
    }

    if (nsc->cluster_config && nsc->cluster_config->num_cld) {
	for (n = 0; n < nsc->cluster_config->num_cld; n++) {
	    CLD2NODE_MAP(nsc->cluster_config->cld[n], node_map);
	    if (node_map) {
	    	rv = node_map_monitor_action(node_map, token, nsc, online, 
					     CLUSTER_NSC_INSTANCE_BASE + n);
	    	if (rv) {
		    DBG_LOG(MSG, MOD_CLUSTER, 
		    	    "node_map_monitor_action() failed, "
	    	    	    "rv=%d online=%d instance=%d namespace=%s "
			    "token=0x%lx",
			    rv, online, CLUSTER_NSC_INSTANCE_BASE + n, 
			    nsc->namespace, token.u.token);
	    	}
	    }
	}
    }

    return 0;
}

/*
 * CL_init() - Cluster subsystem initialization
 *
 *      Returns: !=0 Success
 */
int CL_init(void) 
{
    int rv;

    // Register namespace configuration update callout
    rv = nkn_add_ns_status_callback(CL_namspace_update);
    if (rv) {
    	DBG_LOG(SEVERE, MOD_CLUSTER, 
		"nkn_add_ns_status_callback() failed, rv=%d", rv);
	return 0;
    }

    // Initialize NVSD => CMM message handler
    rv = cl_cmm_init();
    if (rv) {
    	DBG_LOG(SEVERE, MOD_CLUSTER, "cl_cmm_init() failed, rv=%d", rv);
	return 0;
    }

    // Initialize CMM => NVSD message handler
    rv = cl_node_status_init();
    if (rv) {
    	DBG_LOG(SEVERE, MOD_CLUSTER, "cl_node_status_init() failed, rv=%d", rv);
	return 0;
    }
    return 1; // Success
}

/*
 * End of cl.c
 */
