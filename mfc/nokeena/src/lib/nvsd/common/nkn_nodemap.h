/*
 *******************************************************************************
 * nkn_nodemap.h - Internal generic NodeMap functions
 *******************************************************************************
 */

#ifndef _NKN_NODEMAP_H
#define _NKN_NODEMAP_H

#include <atomic_ops.h>
#include "nkn_namespace.h"

/*
 * NodeMap base class definitions
 */
#define USE_TRIE_DIRECTORY 1 // Enabled

typedef struct nodemap_stat {
#ifndef USE_TRIE_DIRECTORY
    struct nodemap_stat *next;
    struct nodemap_stat *prev;
#endif
    char *name;
    int refcnt;
    AO_t stat;
} nodemap_stat_t;

typedef struct map_node {
    int node_updates;
    int node_online;
    int local_node; // "node_host" is local IP
    char *node_host_port; // heartbeat host:port
    int node_host_port_strlen;
    char *http_node_host_port; // http request host:port 
    int http_node_host_port_strlen;
    const char *node_host; // Pointer to binary data, do not free()
    int node_host_strlen;
    uint16_t node_port; // heartbeat port
    uint16_t node_http_port;
    char *node_heartbeatpath;
    int node_heartbeatpath_strlen;
    nodemap_stat_t *node_stat;
} map_node_t;

typedef struct map_private {
    int flags;
    int init_in_progress;
    int defined_nodes;
    int online_nodes;
    map_node_t *node[MAX_CLUSTER_MAP_NODES+1];
    const char *cache_name_host;
    int cache_name_host_strlen;
    pthread_rwlock_t rwlock;

    // Map Heartbeat IP:Port to node[]
    hash_table_def_t ht_hostnames;
    hash_entry_t hash_hostnames[128];

    // Map HTTP IP:Port to node[]
    hash_table_def_t ht_http_hostnames;
    hash_entry_t hash_http_hostnames[128];
} map_private_t;

 /* map_private_t.flags definitions */
 #define MP_FL_RWLOCK_INIT     (1 << 0)

/* Macro definitions */
#define MP_READ_LOCK(_mp) { \
    int _rv = pthread_rwlock_rdlock(&(_mp)->rwlock); \
    assert(_rv == 0); \
}

#define MP_WRITE_LOCK(_mp) { \
    int _rv = pthread_rwlock_wrlock(&(_mp)->rwlock); \
    assert(_rv == 0); \
}

#define MP_UNLOCK(_mp) { \
    int _rv = pthread_rwlock_unlock(&(_mp)->rwlock); \
    assert(_rv == 0); \
}

/*
 *******************************************************************************
 * map_node_t and map_private_t init and deinit functions
 *******************************************************************************
 */
int init_map_node_t(map_node_t *mnp,
		    const char *host,
		    uint16_t port, // Persistent data
		    const char *node_hearbeatpath, // Persistent data
		    int node_heartbeatpath_strlen,
		    const char *stat_basename, const namespace_config_t *nsc);
int deinit_map_node_t(map_node_t *mnp);

int init_map_private_t(map_private_t *mpriv, const char *cache_name_host, 
		       int *online /* Output: NodeMap status */);
int deinit_map_private_t(map_private_t *mpriv);

/*
 *******************************************************************************
 * "glob_" stat management interfaces
 *******************************************************************************
 */
#define INC_NODEMAP_STAT(_pstat) AO_fetch_and_add1(&(_pstat)->stat)

nodemap_stat_t *get_nodemap_stat_t(const char *name);
void release_nodemap_stat_t(nodemap_stat_t *pstat);

/*
 *******************************************************************************
 * host_port_to_node() - Map Heartbeat Host:Port to map_node_t
 *
 *	Return: >=0 node index
 *		< 0 error
 *******************************************************************************
 */
int host_port_to_node(const char *hostport, map_private_t *clmp);

/*
 *******************************************************************************
 * http_host_port_to_node() - Map HTTP Host:Port to map_node_t
 *
 *	Return: >=0 node index
 *		< 0 error
 *******************************************************************************
 */
int http_host_port_to_node(const char *hostport, map_private_t *clmp);

/*
 ******************************************************************************
 * update_node_config() - Update node configuration using the given update data
 *
 *	Input:
 *	  clmp - map_private_t context
 *	  online - State transition (online / offline)
 *	  entries - entries in hostport[]
 *	  hostport - host:port strings
 *	Output:
 *	  cluster_init_now_complete - Transition from init to online.
 *				      (one time event).
 *	  Return: >0 => Configuration update occurred
 *	 
 ******************************************************************************
 */
int update_node_config(map_private_t *mp, int online, int entries,
		       char **hostport, int *cluster_init_now_complete);

/*
 *******************************************************************************
 * get_node_config() - Get node configuration
 *
 *	Returns: 0 => Success
 *	Returned data points to persistent data.  
 *	No caller deallocation required.
 *******************************************************************************
 */
int get_node_config(const map_private_t *mp, int *nodes, 
		    const char **host_port, const int **host_port_strlen,
		    const char **http_host_port, 
		    const int **http_host_port_strlen,
		    const const char **heartbeat_path, 
		    const int **heartbeat_path_strlen);

/*
 *******************************************************************************
 * dump_node_config() - Dump node configuration 
 *
 *	Return: 0 => Success
 *******************************************************************************
 */
int dump_node_config(const map_private_t *mp, 
		     int (*print)(map_node_t *, char *, int),
		     char *buf, int buflen);

/*
 *******************************************************************************
 * extract_options_value - Extract name/value data from "options" attribute
 *
 *  Input:
 *	str - input
 *	name - attribute name
 *  Output:
 *	value - value associate with given name
 *	value_len - string length
 *
 *	Return: 0 => Success
 *******************************************************************************
 */
int extract_options_value(const char *str, const char *name, 
		      	  const char **value, int *value_len);

/*
 *******************************************************************************
 * valid_heartbeat_protocol_vers() - Compare node heartbeat message protocol
 *				     version against expected version.
 *
 * Only for use with L7 NodeMap(s).
 *
 *	Return: >0 => True
 *	Return:  0 => False
 *	Return: <0 => Unknown
 *
 *	actual_vers - Optional return, actual version used
 *	expected_vers - Optional return, expect version
 *
 *******************************************************************************
 */
int valid_heartbeat_protocol_version(const char *node_handle_str, 
				     uint64_t *actual_vers, 
				     uint64_t *expected_vers);

#endif /* _NKN_NODEMAP_H */

/*
 * End of nkn_nodemap.h
 */
