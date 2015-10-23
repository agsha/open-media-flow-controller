/*
 *******************************************************************************
 * nkn_nodemap.c - Generic NodeMap functions
 *******************************************************************************
 */
#include "stdlib.h"
#include <string.h>
#include <strings.h>
#include <alloca.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <errno.h>
#include "nkn_namespace.h"
#include "nkn_nodemap.h"
#include "nkn_cmm_shm_api.h"
#include "nkn_nodemap_clustermap.h"
#include "nkn_stat.h"
#include "nkn_debug.h"

static int (*is_local_ip)(const char *ip, int ip_strlen);
static int current_node_status(const char *hostport, 
			       const namespace_config_t *nsc, int *online);

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
int 
extract_options_value(const char *str, const char *name, 
		      const char **value, int *value_len)
{
    char *p;
    char *p_valstart;

    if (!str || !name || !value || !value_len) {
    	return 1;
    }
    p = strstr(str, name);
    if (p) {
    	p += strlen(name);
	if (!(*p) || ((*p != ' ') && (*p != '='))) { // Substring or noval case
	    return 2;
	}

	while (*p && ((*p == ' ') || (*p == '='))) { // Find value start
	    p++;
	}
	if (!(*p)) { // Value start not found
	    return 3;
	}	
	p_valstart = p;
	while (*p && ((*p != ' ') && (*p != ','))) { // Find value end
	    p++;
	}
	*value = p_valstart;
	*value_len = p - p_valstart;
    } else {
    	return 4;
    }
    return 0;
}

/*
 *******************************************************************************
 * NodeMap "glob_" counter management
 *******************************************************************************
 */
#ifdef USE_TRIE_DIRECTORY
static pthread_mutex_t nm_stat_lock = PTHREAD_MUTEX_INITIALIZER;
nkn_trie_t nodemap_stat_trie;
static AO_t nodemap_stat_elements;

static nodemap_stat_t *new_nodemap_stat_t(const char *name)
{
    nodemap_stat_t *pstat;

    pstat = (nodemap_stat_t *)nkn_calloc_type(1, sizeof(nodemap_stat_t), 
					      mod_cl_stat_t);
    if (pstat) {
    	pstat->name = nkn_strdup_type(name, mod_cl_stat_t_name);
        pstat->refcnt = 1;
	AO_fetch_and_add1(&nodemap_stat_elements);
    }
    return pstat;
}

static void delete_nodemap_stat_t(nodemap_stat_t *pstat)
{
    if (pstat) {
        if (pstat->name) {
	    free(pstat->name);
	    pstat->name = NULL;
	}
    	free(pstat);
	AO_fetch_and_sub1(&nodemap_stat_elements);
    }
}

static void *trie_nm_stat_copy(nkn_trie_node_t nd)
{
    return nd;
}

static void trie_nm_stat_destruct(nkn_trie_node_t nd)
{
   nodemap_stat_t *pstat = (nodemap_stat_t *)nd;
   if (pstat) {
   	delete_nodemap_stat_t(pstat);
   }
}

nodemap_stat_t *get_nodemap_stat_t(const char *name)
{
    int rv;
    nodemap_stat_t *pstat;
    char statname[1024];

    pthread_mutex_lock(&nm_stat_lock);
    pstat = (nodemap_stat_t *)nkn_trie_exact_match(nodemap_stat_trie, 
						   (char *)name);
    if (pstat) {
	pstat->refcnt++;
	pthread_mutex_unlock(&nm_stat_lock);
	return pstat;
    }

    pstat = new_nodemap_stat_t(name);
    if (pstat) {
    	rv = nkn_trie_add(nodemap_stat_trie, (char *)name, 
			  (nkn_trie_node_t) pstat);
	if (!rv) {
	    // Allocate counter
	    snprintf(statname, sizeof(statname), "glob_%s", name);
	    statname[sizeof(statname)-1] = '\0';
	    nkn_mon_add(statname, NULL, &pstat->stat, sizeof(pstat->stat));
	} else {
	    delete_nodemap_stat_t(pstat);
	    pstat = NULL;
	}
    }
    pthread_mutex_unlock(&nm_stat_lock);
    return pstat;
}

void release_nodemap_stat_t(nodemap_stat_t *pstat)
{
    int rv;
    char statname[1024];
    nodemap_stat_t *removed_pstat;

    pthread_mutex_lock(&nm_stat_lock);
    if (pstat && (--pstat->refcnt == 0)) {
	snprintf(statname, sizeof(statname), "glob_%s", pstat->name);
	statname[sizeof(statname)-1] = '\0';

	rv = nkn_trie_remove(nodemap_stat_trie, pstat->name, 
			     (nkn_trie_node_t *)&removed_pstat);
	// Note: pstat is now deallocated
    	pthread_mutex_unlock(&nm_stat_lock);
	if ((rv >= 0) && (pstat == removed_pstat)) {
	    // Deallocate counter
	    nkn_mon_delete(statname, NULL);
	}
    } else {
    	pthread_mutex_unlock(&nm_stat_lock);
    }
}

#else /* !USE_TRIE_DIRECTORY */
static pthread_mutex_t nm_stat_lock = PTHREAD_MUTEX_INITIALIZER;
static nodemap_stat_t nodemap_stat_hdr;
static int nodemap_stat_hdr_elements;

#define NM_STAT_LINK_TAIL(_pstat) { \
	(_pstat)->next = &nodemap_stat_hdr; \
	(_pstat)->prev = nodemap_stat_hdr.prev; \
	nodemap_stat_hdr.prev->next = (_pstat); \
	nodemap_stat_hdr.prev = (_pstat); \
	nodemap_stat_hdr_elements++; \
}

#define ND_STAT_LINK_HEAD(_pstat) { \
	(_pstat)->next = nodemap_stat_hdr.next; \
	(_pstat)->prev = &nodemap_stat_hdr; \
	nodemap_stat_hdr.next->prev = (_pstat); \
	nodemap_stat_hdr.next = (_pstat); \
	nodemap_stat_hdr_elements++; \
}

#define NM_STAT_UNLINK(_pstat) { \
	(_pstat)->prev->next = (_pstat)->next; \
	(_pstat)->next->prev = (_pstat)->prev; \
	nodemap_stat_hdr_elements--; \
}

static nodemap_stat_t *new_nodemap_stat_t(const char *name)
{
    nodemap_stat_t *pstat;

    pstat = (nodemap_stat_t *)nkn_calloc_type(1, sizeof(nodemap_stat_t), 
					      mod_cl_stat_t);
    if (pstat) {
    	pstat->name = nkn_strdup_type(name, mod_cl_stat_t_name);
        pstat->refcnt = 1;
    }
    return pstat;
}

static void delete_nodemap_stat_t(nodemap_stat_t *pstat)
{
    if (pstat) {
        if (pstat->name) {
	    free(pstat->name);
	}
    	free(pstat);
    }
}

nodemap_stat_t *get_nodemap_stat_t(const char *name)
{
    nodemap_stat_t *pstat;
    char statname[1024];

    pthread_mutex_lock(&nm_stat_lock);
    for (pstat = nodemap_stat_hdr.next; pstat != &nodemap_stat_hdr; 
    	 pstat = pstat->next) {
	if (strcmp(pstat->name, name) == 0) {
	    pstat->refcnt++;
    	    pthread_mutex_unlock(&nm_stat_lock);
	    return pstat;
	}
    }

    pstat = new_nodemap_stat_t(name);
    if (pstat) {
        NM_STAT_LINK_TAIL(pstat);

	// Allocate counter
	snprintf(statname, sizeof(statname), "glob_%s", name);
	statname[sizeof(statname)-1] = '\0';
	nkn_mon_add(statname, NULL, &pstat->stat, sizeof(pstat->stat));
    }
    pthread_mutex_unlock(&nm_stat_lock);
    return pstat;
}

void release_nodemap_stat_t(nodemap_stat_t *pstat)
{
    char statname[1024];

    pthread_mutex_lock(&nm_stat_lock);
    if (pstat && (--pstat->refcnt == 0)) {
        NM_STAT_UNLINK(pstat);
    	pthread_mutex_unlock(&nm_stat_lock);

	// Deallocate counter
	snprintf(statname, sizeof(statname), "glob_%s", pstat->name);
	statname[sizeof(statname)-1] = '\0';
	nkn_mon_delete(statname, NULL);

    	delete_nodemap_stat_t(pstat);
    } else {
    	pthread_mutex_unlock(&nm_stat_lock);
    }
}
#endif /* !USE_TRIE_DIRECTORY */

/*
 *******************************************************************************
 * map_node_t and map_private_t init and deinit functions
 *******************************************************************************
 */
int init_map_node_t(map_node_t *mnp,
		    const char *host, // Persistent data
		    uint16_t port, 
		    const char *node_hearbeatpath, // Persistent data
		    int node_hearbeatpath_strlen,
		    const char *stat_basename,
		    const namespace_config_t *nsc)
{
    int retval = 0;
    int rv;
    int hoststrlen;
    int alloc_size;
    char buf[1024];
    uint16_t http_port = port;
    char *p;
    int colon_port_len;

    hoststrlen = strlen(host);
    mnp->local_node = (*is_local_ip)(host, hoststrlen);

    /* Consider heartbeathpath=:<port>/<absolute_path> */
    p = memchr(node_hearbeatpath, ':', node_hearbeatpath_strlen);
    if (p) {
    	port = atoi(++p);
    	p = memchr(p, '/', node_hearbeatpath_strlen);
	if (p) {
	    colon_port_len = (p - node_hearbeatpath);
	    node_hearbeatpath_strlen -= colon_port_len;
	    node_hearbeatpath += colon_port_len;
	} else {
	    DBG_LOG(MSG, MOD_CLUSTER, "Invalid :<port>/<heartbeatpath> syntax");
	    retval = 11;
	    goto err_exit;
	}
    }

    /* Heartbeat IP:Port */
    alloc_size = hoststrlen + 1 + 5 + 1; // {host}:{port}{null}
    mnp->node_host_port = nkn_malloc_type(alloc_size, mod_cl_node_host_port);
    if (!mnp->node_host_port) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"nkn_malloc_type(mod_cl_node_host_port) failed");
	retval = 1;
	goto err_exit;
    }
    rv = snprintf(mnp->node_host_port, alloc_size, "%s:%hu", host, port);
    mnp->node_host_port_strlen = rv;

    /* HTTP request  IP:Port */
    alloc_size = hoststrlen + 1 + 5 + 1; // {host}:{port}{null}
    mnp->http_node_host_port = 
    	nkn_malloc_type(alloc_size, mod_cl_node_host_port);
    if (!mnp->http_node_host_port) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"nkn_malloc_type(mod_cl_node_host_port) failed");
	retval = 12;
	goto err_exit;
    }
    rv = snprintf(mnp->http_node_host_port, alloc_size, "%s:%hu", 
		  host, http_port);
    mnp->http_node_host_port_strlen = rv;

    rv = current_node_status(mnp->node_host_port, nsc, &mnp->node_online);
    if (!rv) {
    	mnp->node_updates = 1;
    } else {
    	mnp->node_updates = 0;
    }

    mnp->node_host = host;
    mnp->node_host_strlen = hoststrlen;
    mnp->node_port = port;
    mnp->node_http_port = http_port;
    mnp->node_heartbeatpath = nkn_malloc_type(node_hearbeatpath_strlen+1, 
    					      mod_cl_node_heartbeatpath);
    if (mnp->node_heartbeatpath) {
    	memcpy(mnp->node_heartbeatpath, node_hearbeatpath, 
	       node_hearbeatpath_strlen);
    	mnp->node_heartbeatpath[node_hearbeatpath_strlen] = '\0';
    	mnp->node_heartbeatpath_strlen = node_hearbeatpath_strlen;
    } else {
    	DBG_LOG(MSG, MOD_CLUSTER, 
		"nkn_malloc_type(mod_cl_node_heartbeatpath) failed");
	retval = 2;
	goto err_exit;
    }

    snprintf(buf, sizeof(buf), "%s_%s", stat_basename, mnp->node_host_port);
    mnp->node_stat = get_nodemap_stat_t(buf);
    if (!mnp->node_stat) {
    	DBG_LOG(MSG, MOD_CLUSTER, "get_nodemap_stat_t(%s) failed", buf);
	retval = 3;
	goto err_exit;
    }
    return 0;

err_exit:

    deinit_map_node_t(mnp);
    return retval;
}

int deinit_map_node_t(map_node_t *mnp)
{
    if (mnp->node_host_port) {
    	free(mnp->node_host_port);
	mnp->node_host_port = NULL;
    }

    if (mnp->http_node_host_port) {
    	free(mnp->http_node_host_port);
	mnp->http_node_host_port = NULL;
    }

    if (mnp->node_heartbeatpath) {
    	free(mnp->node_heartbeatpath);
	mnp->node_heartbeatpath = NULL;
    }

    if (mnp->node_stat) {
    	release_nodemap_stat_t(mnp->node_stat);
    	mnp->node_stat = NULL;
    }
    return 0;
}

int init_map_private_t(map_private_t *mpriv, const char *cache_name_host, 
		       int *online)
{
    int retval = 0;
    int rv;
    int n;
    pthread_rwlockattr_t rwlock_attr;

    /* Setup hash table for Heartbeat IP:Port to node[] mapping */
    mpriv->ht_hostnames = ht_base_nocase_hash;
    mpriv->ht_hostnames.ht = mpriv->hash_hostnames;
    mpriv->ht_hostnames.size = 
    	sizeof(mpriv->hash_hostnames)/sizeof(hash_entry_t);

    /* Setup hash table for HTTP IP:Port to node[] mapping */
    mpriv->ht_http_hostnames = ht_base_nocase_hash;
    mpriv->ht_http_hostnames.ht = mpriv->hash_http_hostnames;
    mpriv->ht_http_hostnames.size = 
    	sizeof(mpriv->hash_http_hostnames)/sizeof(hash_entry_t);

    for (n = 0; mpriv->node[n]; n++) {
    	rv = (*mpriv->ht_hostnames.add_func)(&mpriv->ht_hostnames,
					mpriv->node[n]->node_host_port,
					strlen(mpriv->node[n]->node_host_port),
					n);
	if (rv) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "hash add_func failed, rv=%d, key=%s index=%d",
		    rv, mpriv->node[n]->node_host_port, n);
	    retval = 1;
	    goto err_exit;
    	}

    	rv = (*mpriv->ht_http_hostnames.add_func)(&mpriv->ht_http_hostnames,
				    mpriv->node[n]->http_node_host_port,
				    strlen(mpriv->node[n]->http_node_host_port),
				    n);
	if (rv) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "hash add_func failed, rv=%d, key=%s index=%d",
		    rv, mpriv->node[n]->http_node_host_port, n);
	    retval = 11;
	    goto err_exit;
    	}

	if (mpriv->node[n]->node_updates) {
	    if (mpriv->node[n]->node_online) {
    		mpriv->online_nodes++;
	    }
	} else {
	    mpriv->init_in_progress++;
	}
    }
    mpriv->defined_nodes = n;
    if (!mpriv->init_in_progress) {
    	*online = 1;
    } else {
    	*online = 0;
    }

    /* Generate the host portion of the cache name */
    mpriv->cache_name_host = nkn_strdup_type(cache_name_host, mod_cl_cname);
    mpriv->cache_name_host_strlen = strlen(mpriv->cache_name_host);

    /* Initialize rwlock and bias for writers. */
    rv = pthread_rwlockattr_init(&rwlock_attr);
    if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, "pthread_rwlockattr_init() failed, rv=%d", 
		rv);
	retval = 2;
	goto err_exit;
    }
    rv = pthread_rwlockattr_setkind_np(&rwlock_attr, 
    				PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
    if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, 
		"pthread_rwlockattr_setkind_np() failed, rv=%d", rv);
	retval = 3;
	goto err_exit;
    }
    rv = pthread_rwlock_init(&mpriv->rwlock, &rwlock_attr);
    pthread_rwlockattr_destroy(&rwlock_attr);
    if (rv) {
	DBG_LOG(MSG, MOD_CLUSTER, "pthread_rwlock_init() failed, rv=%d", rv);
	retval = 4;
	goto err_exit;
    }
    mpriv->flags = MP_FL_RWLOCK_INIT;

    return 0;

err_exit:

    deinit_map_private_t(mpriv);
    return retval;
}

int deinit_map_private_t(map_private_t *mpriv)
{
    int rv;

    /* Deallocate hash table data */
    if (mpriv->ht_hostnames.dealloc_func) {
    	(*mpriv->ht_hostnames.dealloc_func)(&mpriv->ht_hostnames);
    }

    if (mpriv->ht_http_hostnames.dealloc_func) {
    	(*mpriv->ht_http_hostnames.dealloc_func)(&mpriv->ht_http_hostnames);
    }

    if (mpriv->cache_name_host) {
    	free((char *)mpriv->cache_name_host);
	mpriv->cache_name_host = NULL;
    }

    if (mpriv->flags & MP_FL_RWLOCK_INIT) {
	rv = pthread_rwlock_destroy(&mpriv->rwlock);
	if (rv) {
	    DBG_LOG(MSG, MOD_CLUSTER, 
		    "pthread_rwlock_destroy() failed, rv=%d", rv);
	}
	mpriv->flags &= ~MP_FL_RWLOCK_INIT;
    }
    return 0;
}

/*
 *******************************************************************************
 * host_port_to_node() - Map Heartbeat Host:Port to map_node_t
 *
 *	Return: >=0 node index
 *		< 0 error
 *******************************************************************************
 */
int host_port_to_node(const char *hostport, map_private_t *clmp)
{
    int rv;
    int node_index;

    rv = (*clmp->ht_hostnames.lookup_func)(&clmp->ht_hostnames,
				           hostport, strlen(hostport), 
					   &node_index);
    if (!rv) {
    	return node_index;
    } else {
    	return -1;
    }
}

/*
 *******************************************************************************
 * http_host_port_to_node() - Map HTTP Host:Port to map_node_t
 *
 *	Return: >=0 node index
 *		< 0 error
 *******************************************************************************
 */
int http_host_port_to_node(const char *hostport, map_private_t *clmp)
{
    int rv;
    int node_index;

    rv = (*clmp->ht_http_hostnames.lookup_func)(&clmp->ht_http_hostnames,
				           hostport, strlen(hostport), 
					   &node_index);
    if (!rv) {
    	return node_index;
    } else {
    	return -1;
    }
}

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
		       char **hostport, int *cluster_init_now_complete)
{
    int n;
    int node;
    int update = 0;
    *cluster_init_now_complete = 0;

    for (n = 0; n < entries; n++) {
    	node = host_port_to_node(hostport[n], mp);
	if (node >= 0) {
	    if (mp->node[node]->node_online != online) {
	    	update++;
	    	mp->node[node]->node_online = online;
	    	if (online) {
		    mp->online_nodes++;
	    	} else {
		    mp->online_nodes--;
	    	}
	    }
	    if (!mp->node[node]->node_updates++) {
	    	mp->init_in_progress--;
		if (!mp->init_in_progress) {
		    *cluster_init_now_complete = 1; // One time event
		}
	    }
	}
    }
    return update;
}

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
		    const int **heartbeat_path_strlen)
{
    int n;
    map_node_t *mnp;

    *nodes = mp->defined_nodes;
    for (n = 0; n < mp->defined_nodes; n++) {
    	mnp = mp->node[n];
    	host_port[n] = mnp->node_host_port;
	host_port_strlen[n] = &mnp->node_host_port_strlen;
    	http_host_port[n] = mnp->http_node_host_port;
	http_host_port_strlen[n] = &mnp->http_node_host_port_strlen;
    	heartbeat_path[n] = mnp->node_heartbeatpath;
    	heartbeat_path_strlen[n] = &mnp->node_heartbeatpath_strlen;
    }
    return 0;
}

/*
 *******************************************************************************
 * dump_node_config() - Dump node configuration 
 *
 *	Return: 0 => Success
 *******************************************************************************
 */
int dump_node_config(const map_private_t *mp, 
		     int (*print)(map_node_t *, char *, int),
		     char *buf, int buflen)
{
    int rv;
    int n;
    int bytes_used = 0;

    rv = snprintf(buf, buflen - bytes_used, 
    		  "\ndefined_node=%d online_nodes=%d\n",
		  mp->defined_nodes, mp->online_nodes);
    if (rv >= (buflen - bytes_used)) {
    	return 1; // Truncation
    }
    bytes_used += rv;

    for (n = 0; n < mp->defined_nodes; n++) {
    	rv = snprintf(buf + bytes_used, buflen - bytes_used,
		      "Node[%d]: %s Online=%d\n",
		      n, mp->node[n]->node_host_port, mp->node[n]->node_online);
    	if (rv >= (buflen - bytes_used)) {
	    return 1; // Truncation
    	}
    	bytes_used += rv;

	if (print) {
	    rv = (*print)(mp->node[n], buf + bytes_used, buflen - bytes_used);
    	    if (rv >= (buflen - bytes_used)) {
	        return 1; // Truncation
	    }
	    bytes_used += rv;
	}
    }
    return 0;
}

/*
 *******************************************************************************
 * Generic node_map_descriptor_t destructor
 *******************************************************************************
 */
void delete_node_map_descriptor_t(node_map_descriptor_t *nmd)
{
    return nmd->free_nmd(nmd);
}

/*
 *******************************************************************************
 * cluster_nodemap_init() - Cluster NodeMap system initialization
 *
 *	Return: 0 => Success
 *******************************************************************************
 */
int cluster_nodemap_init(int (*is_local_ip_proc)(const char *ip, int ip_strlen))
{
    is_local_ip = is_local_ip_proc;
#ifdef USE_TRIE_DIRECTORY
    nodemap_stat_trie = nkn_trie_create(trie_nm_stat_copy, 
    					trie_nm_stat_destruct);
    if (!nodemap_stat_trie) {
    	DBG_LOG(SEVERE, MOD_CLUSTER, 
		"nodemap_stat_trie = nkn_trie_create() failed");
    	DBG_ERR(SEVERE, "nodemap_stat_trie = nkn_trie_create() failed");
    }
#else
    nodemap_stat_hdr.next = &nodemap_stat_hdr;
    nodemap_stat_hdr.prev = &nodemap_stat_hdr;
#endif

    nodemap_clustermap_init();
    return 0;
}

/*
 *******************************************************************************
 * 		P R I V A T E  F U N C T I O N S
 *******************************************************************************
 */

/*
 *******************************************************************************
 * current_node_status() - Get curent node status from namespace_config_t
 *
 *	Return: 0 => Found node status
 *******************************************************************************
 */
static int current_node_status(const char *hostport, 
			       const namespace_config_t *nsc, int *online)
{
    int retval;
    int n;
    int node;
    origin_server_t *oscfg;
    map_private_t *mp;
    struct node_map_descriptor *node_map;

    if (!nsc) {
	retval = 1;
    	goto exit;
    }
    retval = 2;

    /*
     *  Attempt to extract the current node status from the given 
     *  namespace_config_t by iterating through the node_map_descriptor_t(s).
     */
    if (nsc->http_config &&
    	(nsc->http_config->origin.select_method == OSS_HTTP_NODE_MAP)) {
    	oscfg = &nsc->http_config->origin;

    	for (n = 0; n < oscfg->u.http.node_map_entries; n++) {
	    mp = (map_private_t *)oscfg->u.http.node_map[n]->ctx.private_data;
	    node = host_port_to_node(hostport, mp);
	    if (node >= 0) {
	    	*online = mp->node[node]->node_online;
	    	retval = 0;
	    	break;
	    }
    	}
    }

    if (retval && nsc->cluster_config && nsc->cluster_config->num_cld) {
    	retval = 3;
    	for (n = 0; n < nsc->cluster_config->num_cld; n++) {
	    CLD2NODE_MAP(nsc->cluster_config->cld[n], node_map);
	    if (!node_map) {
	    	continue;
	    }
	    mp = (map_private_t *)node_map->ctx.private_data;
	    node = host_port_to_node(hostport, mp);
	    if (node >= 0) {
	    	*online = mp->node[node]->node_online;
	    	retval = 0;
	    	break;
	    }
	}
    }

exit:

    if (!retval) {
    	DBG_LOG(MSG, MOD_CLUSTER, "[%s] online=%d", hostport, *online);
    } else {
    	*online = 0;
    	DBG_LOG(MSG, MOD_CLUSTER, "[%s] no status", hostport);
    }
    return retval;
}

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
				     uint64_t *expected_vers)
{
    int rv;
    int retval;
    cmm_shm_chan_t shm_chan;
    void *data;
    int datalen;
    uint64_t vers;

    rv = cmm_shm_open_chan(node_handle_str, CMMSHM_CH_LOADMETRIC, &shm_chan);
    if (!rv) {
	rv = cmm_shm_chan_getdata_ptr(&shm_chan, &data, &datalen, &vers);
	if (!rv) {
	    if (actual_vers) {
	    	*actual_vers = vers;
	    }
	    if (expected_vers) {
	    	*expected_vers = CMM_NODE_STATUS_HTML_VERSION;
	    }
	    if (vers == CMM_NODE_STATUS_HTML_VERSION) {
	    	retval = 1; // True
	    } else {
	    	retval = 0; // False
	    }
	} else {
	    retval = -1; // Unknown
	}
	cmm_shm_close_chan(&shm_chan);
    } else {
    	retval = -1; // Unknown
    }
    return retval;
}

/*
 * End of nkn_nodemap.c
 */
