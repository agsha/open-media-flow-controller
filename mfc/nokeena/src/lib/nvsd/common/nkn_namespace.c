/*
 *******************************************************************************
 * nkn_namespace.c -- Interface between TM config data and nokeena excecutables
 *******************************************************************************
 */
#include "stdlib.h"
#include <string.h>
#include <strings.h>
#include <alloca.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include <openssl/md5.h>
#include <ctype.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "nkn_nfs_api.h"
#include "nkn_namespace.h"
#include "nkn_namespace_utils.h"
#include "nkn_namespace_nodemap.h"
#include "nkn_stat.h"
#include "nkn_debug.h"
#include "nkn_cod.h"
#include "nkn_lockstat.h"
#include "nkn_tunnel_defs.h"
#include "nkn_mgmt_defs.h"
#include "nkn_cfg_params.h"
#include "nkn_time.h"

/*
 *******************************************************************************
 * Local defines
 *******************************************************************************
 */
#define STATIC static
#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
        memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

#define NSD_LINK_TAIL(_nsd) { \
	(_nsd)->next = &NSD_HEAD; \
	(_nsd)->prev = NSD_HEAD.prev; \
	NSD_HEAD.prev->next = (_nsd); \
	NSD_HEAD.prev = (_nsd); \
	NSD_HEAD_ELEMENTS++; \
}

#define NSD_LINK_HEAD(_nsd) { \
	(_nsd)->next = NSD_HEAD.next; \
	(_nsd)->prev = &NSD_HEAD; \
	NSD_HEAD.next->prev = (_nsd); \
	NSD_HEAD.next = (_nsd); \
	NSD_HEAD_ELEMENTS++; \
}

#define NSD_UNLINK(_nsd) { \
	(_nsd)->prev->next = (_nsd)->next; \
	(_nsd)->next->prev = (_nsd)->prev; \
	NSD_HEAD_ELEMENTS--; \
}

#define DEFAULT_HOSTNAME "*" // use invalid hostname fmt
#define DEFAULT_HOSTNAME_LEN 1
#define HOSTNAME_REGEX_FMT "{regex-%d}"
#define HOSTNAME_REGEX_FMT_MAXLEN 1024

#define NS_STAT_LINK_TAIL(_pstat) { \
	(_pstat)->next = &namespace_stat_hdr; \
	(_pstat)->prev = namespace_stat_hdr.prev; \
	namespace_stat_hdr.prev->next = (_pstat); \
	namespace_stat_hdr.prev = (_pstat); \
	namespace_stat_hdr_elements++; \
}

#define ND_STAT_LINK_HEAD(_pstat) { \
	(_pstat)->next = namespace_stat_hdr.next; \
	(_pstat)->prev = &namespace_stat_hdr; \
	namespace_stat_hdr.next->prev = (_pstat); \
	namespace_stat_hdr.next = (_pstat); \
	namespace_stat_hdr_elements++; \
}

#define NS_STAT_UNLINK(_pstat) { \
	(_pstat)->prev->next = (_pstat)->next; \
	(_pstat)->next->prev = (_pstat)->prev; \
	namespace_stat_hdr_elements--; \
}

#define CONFIG_UPDATE_INTERVAL_SECS 15
/*
 *******************************************************************************
 * Type definitions
 *******************************************************************************
 */
typedef struct prefix_to_namespace_map {
    int alloced_entries;
    int used_entries;
    int *namespace_map;
} prefix_to_namespace_map_t;

typedef struct request_intercept_handler {
    const char *key;
    cl_req_intercept_attr_t *attr;
    cl_redir_attrs_t *rd_attr;
    intercept_type_t ret_code;
    int namespace_index; // index 'namespace_descriptor_t.namespaces[]'
    int cluster_index; // index 'namespace_config_t.cluster_config->cld[]'
} request_intercept_handler_t;

#define INTERCEPT_KEY_FMT "%d:%d"
#define WILDCARD_INTERCEPT_KEY_FMT "wc-%d"

typedef struct namespace_descriptor {
    int magicno;
    struct namespace_descriptor *next;
    struct namespace_descriptor *prev;

    int flags;
    AO_t refcnt;
    namespace_token_t token_start;
    namespace_token_t token_end;

    hash_entry_t hash_hostnames[128];
    hash_table_def_t ht_hostnames;

    int alloced_namespaces;
    int used_namespaces;
    int hostname_regex_namespaces;
    namespace_config_t **namespaces;

    int alloced_prefix_maps;
    int used_prefix_maps;
    prefix_to_namespace_map_t **prefix_maps;

    int num_intercept_handlers;
    int num_wildcard_intercept_handlers;
    request_intercept_handler_t **intercept; // sized to 'used_namespaces' only
    					     //  allow 1 per namespace 
    hash_entry_t hash_intercept[128];
    hash_table_def_t ht_intercept;
} namespace_descriptor_t;

// namespace_descriptor_t definitions

#define NSD_MAGICNO		0x01122009

#define NSD_FLAGS_DELETE	0x1

/*
 *******************************************************************************
 * External Global data
 *******************************************************************************
 */
namespace_token_t NULL_NS_TOKEN;


extern pthread_key_t namespace_key;

extern uint32_t glob_namespaces_changed;

extern int
search_namespace (const char *ns, int *return_idx);

extern int nkn_http_is_tproxy_cluster(http_config_t *http_config_p);
/*
 *******************************************************************************
 * Global data
 *******************************************************************************
 */
static int namespace_init = 0;
static nkn_rwmutex_t ns_list_lock;
static namespace_descriptor_t NSD_HEAD;
static int NSD_HEAD_ELEMENTS;
static namespace_descriptor_t *CURRENT_NSD;
static namespace_descriptor_t *NSD_NO_INTERCEPT;

static pthread_mutex_t ns_token_lock = PTHREAD_MUTEX_INITIALIZER;
static namespace_token_t NS_TOKEN_GENERATOR;

static pthread_mutex_t ns_config_lock = PTHREAD_MUTEX_INITIALIZER;
static int NS_CONFIG_UPDATE = 0;
static struct timespec LAST_NS_CONFIG_UPDATE_TIME;

static pthread_mutex_t ns_update_lock = PTHREAD_MUTEX_INITIALIZER;

static namespace_stat_t namespace_stat_hdr;
static int namespace_stat_hdr_elements;
static pthread_mutex_t ns_stat_lock = PTHREAD_MUTEX_INITIALIZER;

NKNCNT_DEF(om_ingest_check_nocache_already_expired, int32_t, "", "ingest check no-cache already expired")
NKNCNT_DEF(om_ingest_check_nocache_set_cookie, int32_t, "", "ingest check no-cache set_cookie")
NKNCNT_DEF(om_ingest_check_object_size_hit_cnt, int32_t, "", "ingest check object size")
NKNCNT_DEF(pe_choose_origin_server, uint64_t, "", "pe choose os")

/*
 *******************************************************************************
 * Private function declarations
 *******************************************************************************
 */
STATIC namespace_descriptor_t *ref_namespace_descriptor_t(
						namespace_descriptor_t *my_nsd,
						int *delflag);
STATIC void unref_namespace_descriptor_t(namespace_descriptor_t *nsd, 
					int have_lock);

STATIC int update_namespace_data(int system_init);
STATIC int init_namespace_descriptor_t(namespace_descriptor_t *nsd);

STATIC namespace_config_t *new_namespace_config_t(namespace_node_data_t *nd);
STATIC void delete_namespace_config_t(namespace_config_t *nsc);

STATIC prefix_to_namespace_map_t *new_prefix_to_namespace_map_t(void);
STATIC void delete_prefix_to_namespace_map_t(prefix_to_namespace_map_t *pfm);

STATIC namespace_descriptor_t *new_namespace_descriptor_t(void);
STATIC void delete_namespace_descriptor_t(namespace_descriptor_t *nsd);

STATIC namespace_stat_t *get_namespace_stat_t(const char *name);
STATIC void release_namespace_stat_t(namespace_stat_t *stat);
STATIC namespace_stat_t *new_namespace_stat_t(const char *name);
STATIC void delete_namespace_stat_t(namespace_stat_t *stat);

STATIC int init_hashtables(namespace_descriptor_t *nsd);

STATIC const namespace_config_t *int_namespace_to_config(
					namespace_token_t token,
					namespace_descriptor_t **ppnsd,
					int return_locked,
					int *delflag);
STATIC void process_namespace_update(void);
STATIC namespace_token_t nsuid_to_nstoken(const char *p_nsuid);

STATIC int compute_deleted_NFS_entries(const namespace_descriptor_t *needle, 
			    const namespace_descriptor_t *haystack,
			    namespace_token_t **deleted_token_list);
STATIC int notify_nfs(namespace_token_t *deleted_nfs_token_list, 
		      int deleted_nfs_namespaces);
STATIC int checkfor_namespace_match(const namespace_config_t *nsc, 
			     	const mime_header_t *hdr,
				const char *uristr,
				uint32_t ns_thread);
STATIC void ns_status_callback(namespace_descriptor_t *nsd, int online);

STATIC namespace_token_t int_http_request_to_namespace(mime_header_t *hdr,
					    namespace_descriptor_t *my_nsd,
					    const namespace_config_t **ppnsc);
STATIC namespace_token_t request_to_namespace(mime_header_t * hdr,
					      const char *host,
					      int hostlen, const char *uri,
					      int urilen,
					      ns_stat_type_t access_method,
					      namespace_descriptor_t *my_nsd,
					      const namespace_config_t **ppnsc);

STATIC const namespace_config_t *ns_uid_to_config_t(namespace_descriptor_t *nsd,
						    const char *ns_uid);
STATIC int init_intercept_handler(namespace_descriptor_t *nsd);

// Internal Cluster L7 Proxy node namespace generation interfaces

STATIC namespace_node_data_t *new_CL7_node_namespace_node_data_t(
				    const char *host_port, 
				    const int host_port_strlen, int node_num,
				    const namespace_config_t *parent_nsd);

STATIC void delete_CL7_node_namespace_node_data_t(namespace_node_data_t *nd);

STATIC int create_cluster_L7_node_namespaces(namespace_descriptor_t *nsd, 
					     int parent_ns_index,
					     cluster_descriptor_t *cld,
					     node_map_descriptor_t *nmd);

STATIC int add_cluster_L7_node_namespaces(namespace_descriptor_t *nsd);

STATIC int ns_get_cache_key(const namespace_config_t *nsc,
			    req2os_context_t caller_context,
			    const char **data,
			    int *datalen);
/*
 *******************************************************************************
 * 		P U B L I C  F U N C T I O N S
 *******************************************************************************
 */
void nkn_namespace_config_update(void *arg) 
{
    UNUSED_ARGUMENT(arg);

    // TM interface to signal update to namespace data
    pthread_mutex_lock(&ns_config_lock);
    NS_CONFIG_UPDATE++;
    pthread_mutex_unlock(&ns_config_lock);
}

int nkn_init_namespace(int (*is_local_ip_proc)(const char *ip, int ip_strlen))
{
    uuid_t uid;
    pthread_rwlockattr_t rwlock_attr;
    int rv, macro_ret;
    // System initialization
    rv = pthread_rwlockattr_init(&rwlock_attr);
    rv = pthread_rwlockattr_setkind_np(&rwlock_attr,
					PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);

    NKN_RWLOCK_INIT(&ns_list_lock, &rwlock_attr, "nslist-mutex");
    pthread_rwlockattr_destroy(&rwlock_attr);
    cluster_nodemap_init(is_local_ip_proc);
    uuid_generate(uid);
    NS_TOKEN_GENERATOR.u.token_data.gen = 
    	(*(uint32_t *)&uid[0]) ^ (*(uint32_t *)&uid[4]) ^ 
    	(*(uint32_t *)&uid[8]) ^ (*(uint32_t *)&uid[12]);
    NS_TOKEN_GENERATOR.u.token_data.val = 1;

    NSD_HEAD.next = &NSD_HEAD;
    NSD_HEAD.prev = &NSD_HEAD;

    DBG_LOG(MSG, MOD_NAMESPACE, 
    	"NS_TOKEN_GENERATOR gen=%u val=%u",
	NS_TOKEN_GENERATOR.u.token_data.gen,
	NS_TOKEN_GENERATOR.u.token_data.val);

    namespace_stat_hdr.next = &namespace_stat_hdr;
    namespace_stat_hdr.prev = &namespace_stat_hdr;

    rv = update_namespace_data(1);
    namespace_init = 1;
    return rv;
}

void immediate_namespace_update(void)
{
    if (namespace_init && NS_CONFIG_UPDATE) {
    	process_namespace_update();
    }
}

intercept_type_t 
namespace_request_intercept(int sock_fd, mime_header_t *hdr, 
			    uint32_t intf_dest_ip, uint32_t intf_dest_port, 
			    char **target_host, int *target_hostlen,
			    uint16_t *target_port,
			    const cl_redir_attrs_t **rdp,
			    char **ns_name, namespace_token_t *pns_token,
			    namespace_token_t *pnode_ns_token,
			    cluster_descriptor_t **pcd)
{
    UNUSED_ARGUMENT(hdr);

#define LOOPBACK_IP_HOSTORDER 0x7f000002 // 127.0.0.2

    struct sockaddr_in dip;
    socklen_t dlen;
    uint32_t dest_ip;
    uint16_t dest_port;
    namespace_token_t nstoken = NULL_NS_TOKEN;
    namespace_token_t node_nstoken = NULL_NS_TOKEN;
    namespace_descriptor_t *nsd;
    intercept_type_t ret_code = INTERCEPT_NONE;
    int delflag;
    int n = -1;
    int i;
    int size;
    int rv;
    char key[1024];
    namespace_config_t *nsc = NULL;
    cluster_descriptor_t *cd = NULL;
    node_map_descriptor_t *nmap = NULL;
    request_context_t req_ctx;
    int cix;
    uint32_t cluster_node_ip;
    cl_redir_attrs_t *redir_attr = NULL;
    cl_req_intercept_attr_t *intr_attr;
    nm_node_id_t node_id;

    u_int32_t attrs;
    int hdrcnt;
    const char *data;
    int datalen;

    if (NS_CONFIG_UPDATE) {
	process_namespace_update();
    }

    // No defined intercept handlers fast path
    if (CURRENT_NSD == NSD_NO_INTERCEPT) {
	*pns_token = nstoken;
	*pnode_ns_token = node_nstoken;
	return INTERCEPT_NONE;
    }

    nsd = ref_namespace_descriptor_t(NULL, &delflag);
    if (!nsd) {
	DBG_LOG(MSG, MOD_NAMESPACE, "No namespace descriptor defined");
	goto exit;
    }

    if (!nsd->num_intercept_handlers) {
	goto exit;
    }

    if (intf_dest_ip == LOOPBACK_IP_HOSTORDER) {
    	// MFC probe is never subject to interception
    	rv = get_known_header(hdr, MIME_HDR_X_NKN_URI, &data, &datalen, 
			      &attrs, &hdrcnt);
    	if (!rv && (datalen == PROBE_OBJ_STRLEN) 
	    && !strncmp(PROBE_OBJ, data, PROBE_OBJ_STRLEN)) {
	    goto exit;
    	}
    }

    dest_ip = intf_dest_ip;
    dest_port = intf_dest_port;

    if (nsd->num_intercept_handlers - nsd->num_wildcard_intercept_handlers) {
    	size = snprintf(key, sizeof(key), INTERCEPT_KEY_FMT, dest_ip, 
			dest_port);
    	if (size >= (int)sizeof(key)) {
	    key[sizeof(key)-1] = '\0';
	    size = sizeof(key)-1;
    	}

    	// Hash lookup
    	rv = (*nsd->ht_intercept.lookup_func)(&nsd->ht_intercept, key, 
					      size, &n);
    	if (!rv) {
	    // Match
	    goto exit;
    	}
    }

    // Wildcard lookup
    for (i = 0; i < nsd->num_wildcard_intercept_handlers; i++) {
    	size = snprintf(key, sizeof(key), WILDCARD_INTERCEPT_KEY_FMT, i);
    	if (size >= (int)sizeof(key)) {
	    key[sizeof(key)-1] = '\0';
	    size = sizeof(key)-1;
    	}

    	// Hash lookup
    	rv = (*nsd->ht_intercept.lookup_func)(&nsd->ht_intercept, key, 
					      size, &n);
    	if (!rv) {
	    if ((!nsd->intercept[n]->attr->ip || 
	     	 (dest_ip == nsd->intercept[n]->attr->ip)) &&
            	(!nsd->intercept[n]->attr->port || 
	     	 (dest_port == nsd->intercept[n]->attr->port))) {
	     	// Match
	     	goto exit;
	    } else {
		n = -1;
	    }
	}
    }

    // Use client request to map to namespace
    nstoken = int_http_request_to_namespace(hdr, nsd, NULL);
    if (VALID_NS_TOKEN(nstoken)) {
    	// Reference count is now +2, reduce to +1
    	unref_namespace_descriptor_t(nsd, delflag);
    } else {
	goto exit;
    }
    nsc = nsd->namespaces[NS_TOKEN_VAL(nstoken) -
			    NS_TOKEN_VAL(nsd->token_start)];

    if (nsc && nsc->cluster_config && nsc->cluster_config->num_cld) {
	// Get real destination IP
	dlen = sizeof(dip);
	memset(&dip, 0, dlen);
	if (!getsockname(sock_fd, (struct sockaddr *)&dip, &dlen)) {
	    dest_ip = ntohl(dip.sin_addr.s_addr);
	}

	for (cix = 0; cix < nsc->cluster_config->num_cld; cix++) {
	    cd = nsc->cluster_config->cld[cix];
	    CLD2NODE_MAP(cd, nmap);
	    if (nmap) {
		// Get Local Cluster Node IP
		cluster_node_ip = 0;
		rv = (*nmap->get_node_local_data)(&nmap->ctx, 
						  &cluster_node_ip);
		// Cluster L7 Redirect/Proxy
		CLD2INTR_ATTR(cd, intr_attr);
		if ((intr_attr->ip == L7_NOT_INTERCEPT_IP) &&
		    (intr_attr->port == L7_NOT_INTERCEPT_PORT) &&
		    (dest_ip != cluster_node_ip)) {
		    CLD2REDIR_ATTR(cd, redir_attr);
		    break;
		}
	    }
	}
	if (cix >= nsc->cluster_config->num_cld) {
	    // No success
	    nsc = NULL;
	}
    }

exit:

    if ((n >= 0) || (nsc && cd && nmap && redir_attr)) {
    	if (n >= 0) {
	    nsc = nsd->namespaces[nsd->intercept[n]->namespace_index];
	    cd = nsc->cluster_config->cld[nsd->intercept[n]->cluster_index];
	    CLD2NODE_MAP(cd, nmap);
	    redir_attr = nsd->intercept[n]->rd_attr;
	    ret_code = nsd->intercept[n]->ret_code;
	    nstoken = nsd->token_start;
	    nstoken.u.token_data.val += nsd->intercept[n]->namespace_index;
	} else {
	    CLD2INTERCEPT_TYPE(cd, ret_code);
	}
	*ns_name = nsc->namespace;

	rv = (*nmap->request_to_origin)(&nmap->ctx, &req_ctx, 0, nsc, hdr,
					target_host, target_hostlen, 
					target_port, &node_id, NULL);
	if (!rv) {
	    *rdp = redir_attr;
	    node_nstoken = nsd->token_start;
	    node_nstoken.u.token_data.val += cd->node_id_to_ns_index[node_id];
	    ref_namespace_descriptor_t(nsd, &delflag); // Ref count for node_nstoken
	    *pcd = cd;
	} else if (rv < 0) {
	    ret_code = INTERCEPT_RETRY;
	    *pcd = NULL;
	} else {
	    ret_code = INTERCEPT_ERR;
	    *pcd = NULL;

	    DBG_LOG(MSG, MOD_NAMESPACE, "request_to_origin() failed, "
		    "req=%p rv=%d", hdr, rv);
	}
    } else {
	*ns_name = NULL;
    }

    if (nsd && (ret_code == INTERCEPT_NONE)) {
    	unref_namespace_descriptor_t(nsd, delflag);
    }

    *pns_token = nstoken;
    *pnode_ns_token = node_nstoken;
    return ret_code;
}

namespace_token_t http_request_to_namespace(mime_header_t *hdr, 
					    const namespace_config_t **ppnsc)
{
    return int_http_request_to_namespace(hdr, NULL, ppnsc);
}

void release_namespace_token_t(namespace_token_t token)
{
    const namespace_config_t *nsc;
    namespace_descriptor_t *nsd;
    int delflag;
    if (!token.u.token) {
	return;
    }
    nsd = 0;

    nsc = int_namespace_to_config(token, &nsd, 0, &delflag);
    if (nsc && nsd) {
	unref_namespace_descriptor_t(nsd, delflag);
    }
}

const namespace_config_t *namespace_to_config(namespace_token_t token)
{
    const namespace_config_t *nsc;
    namespace_descriptor_t *nsd;
    int delflag;

    if (!token.u.token) {
	return NULL;
    }
    nsd = 0;

    nsc = int_namespace_to_config(token, &nsd, 1, &delflag);
    if (nsc && nsd) {
	AO_fetch_and_add1(&nsd->refcnt);
    }
    NKN_RWLOCK_UNLOCK(&ns_list_lock);
    return nsc;
}

char *ns_uri2uri(namespace_token_t ns_token, char *ns_uri,
			int *result_strlen)
{
    char *p_ns_uid;
    char *p_ns_uri;
    const namespace_config_t *cfg;
    int   exact_match = 1;
    char * p;

    cfg = namespace_to_config(ns_token);
    if (cfg) {
	    p_ns_uid = (char *)cfg->ns_uid;
	    p_ns_uri = (char *)ns_uri; 
        
            while (*p_ns_uid && *p_ns_uri) {
	        if (*(p_ns_uid++) != *(p_ns_uri++)) {
                    exact_match = 0;
	    	    break;
	        }
	    }

	    p = strchr(p_ns_uri, '/');
	    if (p) {
		p_ns_uri = p;
	    }

	    if (exact_match) {
	        if (result_strlen) {
	    	    *result_strlen = strlen(p_ns_uri);
	        }
		release_namespace_token_t(ns_token);
	        return p_ns_uri;
	    }
    }

    if (result_strlen) {
	*result_strlen = strlen(ns_uri);
    }
    release_namespace_token_t(ns_token);
    return ns_uri;
}

static namespace_token_t
uol_or_cachekey_to_nstoken(nkn_uol_t *uol, char *cachekey)
{
    namespace_token_t ns_token;
    const char *p;
    char *p1;
    char *p_nsuid;
    int nsuid_len;
    char *uri;
    int  len, ret;

    ns_token.u.token = 0;

    if (uol) {
	ret = nkn_cod_get_cn(uol->cod, &uri, &len);
	if(ret == NKN_COD_INVALID_COD) {
	    return ns_token;
	}
	if (!uri) {
	   return ns_token;
	}
    } else if (cachekey) {
	uri = cachekey;
    } else
	return ns_token;

    p = strchr(uri + 1, '/');
    if (!p) {
	return ns_token;
    }
    nsuid_len = p - uri;
    p_nsuid = alloca(nsuid_len + 1);
    memcpy(p_nsuid, uri, nsuid_len);
    p_nsuid[nsuid_len] = '\0';

    /*
     * uri format: /namespace:uuid_domain:80/uri
     */
    p1 = strchr(p_nsuid, ':');
    if (p1) { 
	p1 = strchr(p1, '_');
	if (p1) { 
	    *p1 = 0; 
	}
    }

    return nsuid_to_nstoken(p_nsuid);
}

namespace_token_t uol_to_nstoken(nkn_uol_t *uol)
{
    return uol_or_cachekey_to_nstoken(uol, NULL);
}

namespace_token_t cachekey_to_nstoken(char *cachekey)
{
    return uol_or_cachekey_to_nstoken(NULL, cachekey);
}


static char *nstoken_to_uol_uri_common(namespace_token_t token, const char *uri, int urilen,
			 const char * host, int hostlen, char *stack, int stackbufsz,
			 const namespace_config_t *pnsc, size_t *cacheurilen);

int nstoken_to_uol_uri_stack(namespace_token_t token, const char *uri, int urilen,
                              const char * host, int hostlen, char *stack, int stackbufsz,
			      const namespace_config_t *pnsc, size_t *cacheurilen)
{
	return (nstoken_to_uol_uri_common(token, uri,
			urilen, host, hostlen, stack, stackbufsz, pnsc,
			cacheurilen) != 0);
}

char *nstoken_to_uol_uri(namespace_token_t token, const char *uri, int urilen,
			 const char * host, int hostlen, size_t *cacheurilen)
{
	return nstoken_to_uol_uri_common(token, uri, urilen, host, hostlen, NULL, 0, 0,
					 cacheurilen);
}
char *nstoken_to_uol_uri_common(namespace_token_t token, const char *uri, int urilen,
			 const char * host, int hostlen, char *stack, int stackbufsz,
			 const namespace_config_t *pnsc, size_t *cacheurilen)
{
#define OUTBUF(_data, _datalen, _release_ns_on_error) { \
    if (((_datalen)+bytesused) >= outbufsz) { \
   	DBG_LOG(MSG, MOD_NAMESPACE, \
		"datalen(%d)+bytesused(%d) >= outbufsz(%d)",  \
		(_datalen), bytesused, outbufsz); \
	outbuf[outbufsz-1] = '\0'; \
	if ((_release_ns_on_error)) { \
    		release_namespace_token_t(token); \
	} \
	return outbuf; \
    } \
    if ((_datalen) > 1) { \
    	memcpy((void *)&outbuf[bytesused], (_data), (_datalen)); \
    } else { \
    	outbuf[bytesused] = *(_data); \
    } \
    bytesused += (_datalen); \
}

    char *outbuf = 0;
    int outbufsz;
    int bytesused = 0;
    const namespace_config_t *nsc;
    const char *p_uri;
    int p_urilen;
    int last_was_slash = 0;
    int is_ipv6 = 0;
    int actual_hostlen = 0;
    int actual_colonportlen = 0;

    if (!uri || !urilen) {
	DBG_LOG(MSG, MOD_NAMESPACE, "Invalid input uri=%p urilen=%d", 
		uri, urilen);
    	return 0;
    }
    p_uri = uri;
    p_urilen = urilen;

    if (pnsc) {
	nsc = pnsc;
    } else {
	nsc = namespace_to_config(token);
    }

#define D_IPV6 0xF
#define D_IPV6_DELIM 0xF0

    if (nsc) {
    	if (hostlen) {
	    char *p;
        int n = 0;

        if ('[' == host[0])
        {

            for (n = 0; n < hostlen; n++)
            {
                if (']' == host [n])
                {
                    is_ipv6 |= (D_IPV6 | D_IPV6_DELIM);
                    n++;
                    break;
                }
            }

            if (!is_ipv6)
            {
                DBG_LOG(MSG, MOD_NAMESPACE, "Invalid IPv6 address (%s)",
                        host);
                release_namespace_token_t(token);
                return 0;
            }
        }

        if (is_ipv6)
        {
            p = strchr (host + n, ':');
        }
        else
        {
            p = strchr(host, ':');

            if (p && (p != strrchr (host, ':')))
            {
                is_ipv6 |= D_IPV6;
                actual_hostlen = hostlen;
                actual_colonportlen = 0;
            }
        }

        if ((is_ipv6 & D_IPV6_DELIM) || (!is_ipv6))
        {
            actual_hostlen = p ? (p - host) : hostlen;
            actual_colonportlen = p ? hostlen - (p - host) : 0;
        }

	    if (actual_hostlen > MAX_URI_HDR_HOST_SIZE) {
	    	DBG_LOG(MSG, MOD_NAMESPACE, 
		    	"hostlen(%d) > MAX_URI_HDR_HOST_SIZE(%d)",
		    	actual_hostlen, MAX_URI_HDR_HOST_SIZE);
    	    	release_namespace_token_t(token);
	    	return 0;
	    }
	    if (actual_colonportlen > 6) {
	    	DBG_LOG(MSG, MOD_NAMESPACE, "port len(%d) > 6",
		    	actual_colonportlen);
    	    	release_namespace_token_t(token);
	    	return 0;
	    }
	}
    	outbufsz = nsc->ns_uid_strlen + urilen + 16;
    	if (hostlen != 0) {
	    outbufsz += hostlen + 4; // make space for host
	}
	if (stack) {
	    if (outbufsz >= stackbufsz) {
		release_namespace_token_t(token);
		DBG_LOG(MSG, MOD_NAMESPACE,
		    "buffer size >= stack size failed, buflen=%d stacklen=%d",
		    outbufsz, stackbufsz);
		return 0;
	    }
	    outbuf = stack;
	} else {
	    outbuf = nkn_malloc_type(outbufsz, mod_ns_nstoken_to_uol_uri_outbuf);
	}
    	if (!outbuf) {
    	    release_namespace_token_t(token);
	    DBG_LOG(MSG, MOD_NAMESPACE, "malloc() failed, len=%d", outbufsz);
	    return 0;
    	}
	OUTBUF(nsc->ns_uid, nsc->ns_uid_strlen, 1);

	if (hostlen != 0) {
	    char convert_domain[300]; // should be more than enough
	    char * p1, * p2;
	    int hascollen, totlen;

	    /* tolowercase and append ":80" if not existing*/
	    p1 = convert_domain;
	    p2 = (char *)host;
        if (is_ipv6 && !(is_ipv6 & D_IPV6_DELIM))
        {
            *p1 = '[';
            p1++;
        }
	    totlen = hostlen;
	    hascollen = 0;
	    while (totlen) {
		*p1 = tolower(*p2); 
		p1++; p2++; totlen--;
	    }
        if (is_ipv6 && !(is_ipv6 & D_IPV6_DELIM))
        {
            *p1 = ']';
            p1++;
            hostlen += 2; // 1 + 1 for '[' and ']'
        }
	    if (!actual_colonportlen) 
        {
		*p1=':'; p1++;
		*p1='8'; p1++;
		*p1='0'; p1++;
		hostlen += 3;
	    }
	    // *p1=0; NULL terminator is not needed

	    OUTBUF("_", 1, 1);
	    OUTBUF(convert_domain, hostlen, 1);
	}
	if (!pnsc)
	    release_namespace_token_t(token);

	// Compress consecutive slashes
	while (p_urilen && *p_uri) {
	    if (*p_uri == '/') {
	    	if (!last_was_slash) {
	    	    last_was_slash = 1;
		    OUTBUF("/", 1, 0);
		}
	    } else {
		last_was_slash = 0;
		OUTBUF(p_uri, 1, 0);
	    }
	    p_uri++;
	    p_urilen--;
	}
    	OUTBUF("", 1, 0);
    }
    if (cacheurilen)
	*cacheurilen = (bytesused - 1); // remove terminal character
    return outbuf;
#undef OUTBUF
}

int decompose_ns_uri_dir(const char *ns_uri_dir,
			 const char **namespace, int *namespace_len,
			 const char **uid, int *uid_len,
			 const char **host, int *host_len,
			 const char **port, int *port_len)
{
    const char *p;
    const char *p_end;

    /*
     * namespace uri format: /namespace:uuid_host:port/
     */
    if (!ns_uri_dir) {
        return 1;
    }
    p = ns_uri_dir + 1;
    p_end = strchr(p, ':');
    if (p_end) {
	if (namespace) *namespace = p;
	if (namespace_len) *namespace_len = p_end - p;
    } else {
	if (namespace) *namespace = NULL;
	if (namespace_len) *namespace_len = 0;
	return 2;
    }

    p = p_end + 1;
    p_end = strchr(p, '_');
    if (p_end) {
	if (uid) *uid = p;
	if (uid_len) *uid_len = p_end - p;
    } else {
	if (uid) *uid = NULL;
	if (uid_len) *uid_len = 0;
	return 3;
    }

    p = p_end + 1;
    p_end = strchr(p, ':');
    if (p_end) {
	if (host) *host = p;
	if (host_len) *host_len = p_end - p;
    } else {
	if (host) *host = NULL;
	if (host_len) *host_len = 0;
	return 4;
    }

    p = p_end + 1;
    p_end = strchr(p, '/');
    if (p_end) {
	if (port) *port = p;
	if (port_len) *port_len = p_end - p;
    } else {
	p_end = strchr(p, '\0');
	if (p_end) {
	   if (port) *port = p;
	   if (port_len) *port_len = p_end - p;
	} else {
	   if (port) *port = NULL;
	   if (port_len) *port_len = 0;
	   return 5;
	}
    }

    return 0;
}
int decompose_ns_uri(const char *ns_uri,
		     const char **namespace, int *namespace_len,
		     const char **uid, int *uid_len,
		     const char **host, int *host_len,
		     const char **port, int *port_len,
		     const char **uri)
{
    const char *p;
    const char *p_end;

    /*
     * namespace uri format: /namespace:uuid_host:port/uri
     */
    if (!ns_uri) {
        return 1;
    }
    p = ns_uri + 1; 
    p_end = strchr(p, ':');
    if (p_end) {
    	if (namespace) *namespace = p;
	if (namespace_len) *namespace_len = p_end - p;
    } else {
    	if (namespace) *namespace = NULL;
	if (namespace_len) *namespace_len = 0;
	return 2;
    }

    p = p_end + 1;
    p_end = strchr(p, '_');
    if (p_end) {
    	if (uid) *uid = p;
    	if (uid_len) *uid_len = p_end - p;
    } else {
    	if (uid) *uid = NULL;
	if (uid_len) *uid_len = 0;
	return 3;
    }

    p = p_end + 1;
    p_end = strchr(p, ':');
    if (p_end) {
    	if (host) *host = p;
    	if (host_len) *host_len = p_end - p;
    } else {
    	if (host) *host = NULL;
    	if (host_len) *host_len = 0;
	return 4;
    }

    p = p_end + 1;
    p_end = strchr(p, '/');
    if (p_end) {
    	if (port) *port = p;
    	if (port_len) *port_len = p_end - p;
    } else {
    	if (port) *port = NULL;
    	if (port_len) *port_len = 0;
	return 5;
    }

    if (uri) *uri = p_end;
    return 0;
}

static const char *origin_select_method_str[OSS_MAX_ENTRIES] = {
    "OSS_UNDEFINED",
    "OSS_HTTP_CONFIG",
    "OSS_HTTP_ABS_URL",
    "OSS_HTTP_FOLLOW_HEADER",
    "OSS_HTTP_DEST_IP",
    "OSS_HTTP_SERVER_MAP",
    "OSS_NFS_CONFIG",
    "OSS_NFS_SERVER_MAP",
    "OSS_RTSP_CONFIG",
    "OSS_RTSP_DEST_IP",
    "OSS_HTTP_NODE_MAP",
    "OSS_HTTP_PROXY_CONFIG"
};

int request_to_origin_follow_header(
                         req2os_context_t caller_context,
                         const mime_header_t *req,
                         const namespace_config_t *nsc,
                         const char *client_req_ip_port,
                         int client_req_ip_port_len,
                         uint16_t *origin_port,
                         int origin_port_network_byte_order,
                         u_int32_t *attrs_p,
                         int *hdrcnt_p,
                         const char **data_p,
                         int *datalen_p);

int request_to_origin_follow_header(
                         req2os_context_t caller_context,
                         const mime_header_t *req,
                         const namespace_config_t *nsc,
                         const char *client_req_ip_port,
                         int client_req_ip_port_len,
                         uint16_t *origin_port,
                         int origin_port_network_byte_order,
                         u_int32_t *attrs_p,
                         int *hdrcnt_p,
                         const char **data_p,
                         int *datalen_p)
{
    int retval = 0;
    int rv;

    if (!client_req_ip_port) {
        if (nsc->http_config->policies.client_req_connect_handle
            && is_known_header_present(req, MIME_HDR_X_NKN_FP_SVRHOST)) {
            // its a CONNECT method and handle CONNECT is enabled, so we need SVRHOST
            rv = get_known_header(req, MIME_HDR_X_NKN_FP_SVRHOST, data_p, datalen_p,
                                  attrs_p, hdrcnt_p);
        } else {
            rv = get_known_header(req, MIME_HDR_HOST, data_p, datalen_p,
                                  attrs_p, hdrcnt_p);
            if (rv) {
                retval = 16;
            }
        }

    } else {
        rv = 0;
        *data_p = client_req_ip_port;
        *datalen_p = client_req_ip_port_len;
    }

    if (!rv) {
        rv = parse_host_header(*data_p, *datalen_p, datalen_p, origin_port,
                               origin_port_network_byte_order);
        if (rv) {
            retval = 4;
        }
    }

    if (!rv) {
        retval = ns_get_cache_key(nsc, caller_context,
				  data_p, datalen_p);
    }

    return retval;            
}


/*
 * request_to_origin_server()
 *	Given either the HTTP request headers or the client_req_ip_port
 *	and the corresponding namespace_config_t 
 *	return the associated origin server IP and Port.
 *
 *      Note: Caller must pass either a valid 'req' or 'client_ip_port'.
 *	      (i.e. one or the other but not both)
 *
 *	caller_context - Request IP/Port for cache key or origin server
 *	req_ctx - (caller_context=REQ2OS_ORIGIN_SERVER) only else NULL, 
 *		  current request context, returned on unconditional retry
 *	req -  If valid, client_ip_port = NULL
 *	nsc -  Required namespace_config_t 
 *	client_ip_port/client_ip_port_len - If valid, req = NULL
 *	origin_ip - Output buffer for IP
 *	origin_ip_bufsize - size of origin_ip buffer,  0 => malloc() buffer
 *	origin_ip_len - Actual length (includes NULL) of origin IP
 *	origin_port - Origin Port
 *	origin_port_network_byte_order - (!= 0) => Return network byte order
 *	append_port_to_ip - (!= 0) => Add ":<port>" to origin_ip
 *
 *	Return: 0 => Success
 */
int request_to_origin_server(req2os_context_t caller_context,
			 request_context_t *req_ctx,
			 const mime_header_t *req, 
			 const namespace_config_t *nsc,
			 const char *client_req_ip_port, 
			 int client_req_ip_port_len,
			 char **origin_ip, int origin_ip_bufsize,
			 int *origin_ip_len, 
			 uint16_t *origin_port, 
			 int origin_port_network_byte_order,
			 int append_port_to_ip)
{
    int retval = 0;
    int rv;
    u_int32_t attrs;
    int hdrcnt;
    const char *data;
    int datalen;
    int port_chars;

    if (!(req || client_req_ip_port) || !nsc || !origin_ip || 
    	!origin_ip_len || !origin_port) {
	DBG_LOG(MSG, MOD_NAMESPACE, 
		"Invalid input, req=%p client_req_ip_port=%p nsc=%p "
		"origin_ip=%p origin_ip_len=%p "
		"origin_port=%p",
		req, client_req_ip_port, nsc, origin_ip, origin_ip_len, 
		origin_port);
	retval = 1;
	goto exit;
    }


    /* 
     * Origin server set by user policy engine.
     * According to design, policy engine has the final decision power.
     */
    if ((nsc->http_config->origin.select_method == OSS_HTTP_CONFIG) ||
    	(nsc->http_config->origin.select_method == OSS_HTTP_ABS_URL) ||
    	(nsc->http_config->origin.select_method == OSS_HTTP_FOLLOW_HEADER) ||
    	(nsc->http_config->origin.select_method == OSS_HTTP_SERVER_MAP) ||
    	(nsc->http_config->origin.select_method == OSS_HTTP_DEST_IP) ||
    	(nsc->http_config->origin.select_method == OSS_HTTP_NODE_MAP)) {
	/*
	 * Note: OSS_HTTP_PROXY_CONFIG is implicit, cannot be overridden.
	 */
	rv = get_known_header(req, MIME_HDR_X_NKN_ORIGIN_SVR, &data, &datalen, 
			      &attrs, &hdrcnt);
	if (!rv) {
	    rv = parse_host_header(data, datalen, &datalen, origin_port,
				   origin_port_network_byte_order);
	    if (!rv) {
		glob_pe_choose_origin_server++;
		goto done;
	    }
	}
    }

    switch(nsc->http_config->origin.select_method) {
    case OSS_HTTP_CONFIG:
    {
	data = nsc->http_config->origin.u.http.server[0]->hostname;
	datalen = nsc->http_config->origin.u.http.server[0]->hostname_strlen;
	retval = ns_get_cache_key(nsc, caller_context, &data, &datalen);
	*origin_port = origin_port_network_byte_order ? 
		htons(nsc->http_config->origin.u.http.server[0]->port) :
		nsc->http_config->origin.u.http.server[0]->port;
    	break;
    }
    case OSS_HTTP_PROXY_CONFIG:
    {
    	if (caller_context == REQ2OS_CACHE_KEY) {
	    // Return X-NKN-CL7-CACHEKEY-HOST data
	    rv = get_known_header(req,  MIME_HDR_X_NKN_CL7_CACHEKEY_HOST, 
				  &data, &datalen, &attrs, &hdrcnt);
	    if (!rv) {
	    	rv = parse_host_header(data, datalen, &datalen, origin_port,
				   origin_port_network_byte_order);
	    	if (rv) {
		    retval = 100;
	    	}
	    } else {
	    	retval = 101;
	    }
	} else {
	    rv = get_known_header(req,  MIME_HDR_X_NKN_CL7_ORIGIN_HOST, 
				  &data, &datalen, &attrs, &hdrcnt);
	    if (!rv && *data) {
	    	rv = parse_host_header(data, datalen, &datalen, origin_port,
				   origin_port_network_byte_order);
	    	if (rv) {
		    retval = 102;
	    	}
	    } else {
	    	data = nsc->http_config->origin.u.http.server[0]->hostname;
	    	datalen = 
		    nsc->http_config->origin.u.http.server[0]->hostname_strlen;
	    	*origin_port = origin_port_network_byte_order ? 
			htons(nsc->http_config->origin.u.http.server[0]->port) :
			nsc->http_config->origin.u.http.server[0]->port;
	    }
	}
	break;
    }
    case OSS_HTTP_ABS_URL:
    {
    	if (!client_req_ip_port) {
	    rv = get_known_header(req, MIME_HDR_X_NKN_ABS_URL_HOST, &data, &datalen, 
			      	  &attrs, &hdrcnt);
	} else {
	    rv = 0;
	    data = client_req_ip_port;
	    datalen = client_req_ip_port_len;
	}
	if (!rv) {
	    rv = parse_host_header(data, datalen, &datalen, origin_port,
				   origin_port_network_byte_order);
	    if (rv) {
	    	retval = 2;
	    }
	} else {
	    retval = 3;
	}
	break;
    }
    case OSS_HTTP_SERVER_MAP:
    {
    	struct OS_http *osc = &nsc->http_config->origin.u.http;
	int map_data_index;
	char *p;

    	if ((caller_context == REQ2OS_CACHE_KEY) && 
	    nsc->http_config->cache_index.exclude_domain) {
	    data = osc->ho_ldn_name;
	    datalen = osc->ho_ldn_name_strlen;
	    *origin_port = origin_port_network_byte_order ?  htons(80) : 80;
	    break;
	}

    	if (!client_req_ip_port) {
	    rv = get_known_header(req, MIME_HDR_HOST, &data, &datalen, 
			      	  &attrs, &hdrcnt);
	} else {
	    rv = 0;
	    data = client_req_ip_port;
	    datalen = client_req_ip_port_len;
	}

	if (!rv) {
	    p = memchr(data, ':', datalen);
	    if (p) {
	    	datalen = p - data;
	    }
	    rv = (*osc->map_ht_hostnames->lookup_func)(osc->map_ht_hostnames,
	    					       data, datalen,
						       &map_data_index);
	    if (!rv) {
	    	data = ISTR(osc->map_data, string_table_offset,
			osc->map_data->entry[map_data_index].origin_host);
		datalen = strlen(data);
		*origin_port = origin_port_network_byte_order ?
		    htons(osc->map_data->entry[map_data_index].origin_port) :
		    osc->map_data->entry[map_data_index].origin_port;
	    } else {
		TSTR(data, datalen, tstr);
		DBG_LOG(MSG, MOD_NAMESPACE,
			"server map lookup failed, rv=%d hostname=%s", 
			rv, tstr);
	    	retval = 6;
	    }
	} else {
	    retval = 7;
	}
    	break;
    }
    case OSS_HTTP_FOLLOW_HEADER: // For now, header == "Host"
    {
        retval = request_to_origin_follow_header(
                         caller_context,
                         req,
                         nsc,
                         client_req_ip_port,
                         client_req_ip_port_len,
                         origin_port,
                         origin_port_network_byte_order,
                         &attrs,
                         &hdrcnt,
                         &data,
                         &datalen);

    	break;
    }
    case OSS_RTSP_DEST_IP:
    case OSS_HTTP_DEST_IP: 
    {
    	if (!client_req_ip_port) {
	    int hdr_token;
	    if (req->protocol == MIME_PROT_HTTP) {
                if (nsc->http_config->policies.client_req_connect_handle
                                && is_known_header_present(req, MIME_HDR_X_NKN_FP_SVRHOST)) {
                        // its a CONNECT method and handle CONNECT is enabled, so we need SVRHOST
                        hdr_token = (caller_context == REQ2OS_CACHE_KEY)
                                ? MIME_HDR_HOST : MIME_HDR_X_NKN_FP_SVRHOST;
                } else {
                        hdr_token = (caller_context == REQ2OS_CACHE_KEY)
                                ? MIME_HDR_HOST : MIME_HDR_X_NKN_REQ_REAL_DEST_IP;
                }
                rv = get_known_header(req, hdr_token, &data,
                                      &datalen, &attrs, &hdrcnt);
	    } else if (req->protocol == MIME_PROT_RTSP) {
		hdr_token = (caller_context == REQ2OS_CACHE_KEY)
		    ? RTSP_HDR_X_HOST : RTSP_HDR_X_NKN_REQ_REAL_DEST_IP;
	        rv = mime_hdr_get_known(req, hdr_token, &data, 
	    			      &datalen, &attrs, &hdrcnt);
	    }
	} else {
	    rv = 0;
	    data = client_req_ip_port;
	    datalen = client_req_ip_port_len;
	}

	if (!rv) {
	    rv = parse_host_header(data, datalen, &datalen, origin_port,
				   origin_port_network_byte_order);
	    if (rv) {
	    	retval = 8;
	    }
	} else {
	    retval = 9;
	}

	if (!rv) {
	    retval = ns_get_cache_key(nsc, caller_context,
				      &data, &datalen);
	}

	break;
    }
    case OSS_NFS_CONFIG:
    case OSS_NFS_SERVER_MAP:
    {
	char *p;
	data = nsc->http_config->origin.u.nfs.server[0]->hostname_path;
	datalen = nsc->http_config->origin.u.nfs.server[0]->hostname_pathlen;
	p = memchr(data, ':', datalen);
	datalen = p ? (p - data) : datalen;
	*origin_port = nsc->http_config->origin.u.nfs.server[0]->port;
    	break;
    }
    case OSS_RTSP_CONFIG:
    {
	    data = nsc->http_config->origin.u.rtsp.server[0]->hostname;
	    datalen = nsc->http_config->origin.u.rtsp.server[0]->hostname_strlen;
	    *origin_port = origin_port_network_byte_order ?
		    htons(nsc->http_config->origin.u.rtsp.server[0]->port) :
		    nsc->http_config->origin.u.rtsp.server[0]->port;

	    break;
    }
    case OSS_HTTP_NODE_MAP:
    {
	int n;
	node_map_descriptor_t *nmd;
	int node_map_entries = nsc->http_config->origin.u.http.node_map_entries;

    	switch(caller_context) {
	case REQ2OS_TPROXY_ORIGIN_SERVER_LOOKUP:
            retval = 17;
            rv = get_known_header(req, MIME_HDR_HOST, &data, &datalen,
                                  &attrs, &hdrcnt);
            if (!rv) {
                rv = parse_host_header(data, datalen, &datalen, origin_port,
                                       origin_port_network_byte_order);
                if (!rv) {
                        retval = 0;
                }
            }
            break;

	case REQ2OS_CACHE_KEY:
            if (nsc->http_config->origin.u.http.include_orig_ip_port) {
                //TProxy Cluster                

                retval = request_to_origin_follow_header(
                         caller_context,
                         req,
                         nsc,
                         client_req_ip_port,
                         client_req_ip_port_len,
                         origin_port,
                         origin_port_network_byte_order,
                         &attrs,
                         &hdrcnt,
                         &data,
                         &datalen);
                
                if (retval) {
                    retval += 100;        
                }

                break;
            }
    
	    retval = 10;
	    for (n = 0; n < node_map_entries; n++) {
	    	nmd = nsc->http_config->origin.u.http.node_map[n];
		rv = (*nmd->cache_name)(&nmd->ctx, req, (char **)&data, 
					&datalen, origin_port);
		if (!rv) {
		    if (origin_port_network_byte_order) {
		    	*origin_port = htons(*origin_port);
		    }
		    retval = 0;
		    break;
		} 
	    }
	    break;

	case REQ2OS_ORIGIN_SERVER:
	case REQ2OS_ORIGIN_SERVER_LOOKUP:
	case REQ2OS_SET_ORIGIN_SERVER:
	{
	    long flags = 0;
	    retval = 11;

	    if (caller_context == REQ2OS_ORIGIN_SERVER_LOOKUP) {
	    	flags |= R2OR_FL_LOOKUP;
	    }
	    if (caller_context == REQ2OS_SET_ORIGIN_SERVER) {
	    	flags |= R2OR_FL_SET;
	    }

	    for (n = 0; n < node_map_entries; n++) {
	    	nmd = nsc->http_config->origin.u.http.node_map[n];
		rv = (*nmd->request_to_origin)(&nmd->ctx, req_ctx, 
				flags, nsc, req,
			   	(char **)&data, &datalen, origin_port, 
				NULL, NULL);
		if (!rv) {
		    if (origin_port_network_byte_order) {
		    	*origin_port = htons(*origin_port);
		    }
		    retval = 0;
		    break;
		} else if (rv < 0) {
		    retval = -11; // NodeMap initializing, retry
		    break;
		}
	    }
	    break;
	}
	default:
	    retval = 12;
	    break;
	}
	break;
    }
    default:
    {
    	retval = 13;
    	break;
    }
    } // End of switch

done:

    if (!retval) {
    	int reqsize = datalen + 1 /* : */ + 5 /* port */ + 1 /* null */ + 2 /* '[' ']'*/;
    	if (origin_ip_bufsize) {
	    if (origin_ip_bufsize < reqsize) {
	    	retval = 14;
	    }
	} else {
            /* origin_ip_bufsize is 0 only in one case, when this method is 
	     * called from find_origin_server. In this case we have to free
	     * oscfg->ip before allocation.
	     */
	    if (*origin_ip) {
	        free(*origin_ip);
	    }

	    *origin_ip = nkn_malloc_type(reqsize, mod_ns_origin_ip_buf);
	    if (*origin_ip == 0) {
		retval = 15;
	    }
	}
	if (!retval) {

        int delim_len = 0;
        char *p = strchr (data, ':');
        char *p1 = strrchr (data, ':');

        if (('[' == data[0]) && !append_port_to_ip)
        {
            copy_ipv6_host (*origin_ip, data, datalen);
        }
        else if (('[' != data[0]) && (p != p1) && append_port_to_ip)
        {
            delim_len = snprintf (*origin_ip, reqsize, "[%s]", data);
            delim_len -= datalen;
        }
        else
        {
            memcpy(*origin_ip, data, datalen);
        }
	    if (append_port_to_ip) {
	    	port_chars = snprintf(&(*origin_ip)[datalen + delim_len], reqsize-(datalen + delim_len), 
				      ":%hu",
			 	origin_port_network_byte_order ? 
			 		ntohs(*origin_port) : *origin_port);
	    } else {
	    	port_chars = 0;
	    	(*origin_ip)[datalen] = '\0';
	    }
	    *origin_ip_len = datalen + port_chars + delim_len;
	}

    }
exit:
    if (!retval) {
    	DBG_LOG(MSG, MOD_NAMESPACE, 
		"origin_select_method: %s caller_context: %d "
		"origin_host: %s origin_port: %hu",
	    origin_select_method_str[nsc->http_config->origin.select_method],
	    caller_context, *origin_ip, 
	    origin_port_network_byte_order ? 
	    	ntohs(*origin_port) : *origin_port);

    } else {
    	DBG_LOG(MSG, MOD_NAMESPACE, "Failed, rv=%d", retval);
    }
    return retval;
}

/*
 * save_request_validate_state() 
 *
 *  Depending on origin server selection mode, note the client "Host:" header 
 *  or origin server IP/Port to allow a subsequent validate to be 
 *  performed without the client request headers.
 *
 *  Returns: 0 => Success
 */
int save_request_validate_state(mime_header_t *reqhdr, 
			        mime_header_t *resphdr,
			        const namespace_config_t *nsc)
{
	int rv;
	int retval = 0;
	u_int32_t attrs;
	int hdrcnt;
	const char *data, *data2, * data3;
	int datalen, datalen2, datalen3;
	char *tbuf;

	switch(nsc->http_config->origin.select_method) {
	case OSS_HTTP_CONFIG:
	case OSS_HTTP_PROXY_CONFIG:
	case OSS_HTTP_SERVER_MAP:
	case OSS_HTTP_NODE_MAP:
	    rv = get_known_header(reqhdr, MIME_HDR_HOST,
				  &data, &datalen, &attrs, &hdrcnt);
	    if (rv) {
		data = "";
		datalen = 1;
	    }
	    break;

	case OSS_HTTP_FOLLOW_HEADER: // For now, use "Host:" header
	    rv = get_known_header(reqhdr, MIME_HDR_HOST,
				  &data, &datalen, &attrs, &hdrcnt);
	    if (rv) {
	    	retval = 2;
	    }
	    break;

	case OSS_HTTP_ABS_URL:
	    rv = get_known_header(reqhdr, MIME_HDR_X_NKN_ABS_URL_HOST,
				  &data, &datalen, &attrs, &hdrcnt);
	    if (rv) {
	    	retval = 3;
	    }
	    break;

	case OSS_HTTP_DEST_IP:
	   /*
	    * BUG 4455:
	    * For TProxy, use this format: .real_IP:Port:domain
	    * The leading '.' is a signature.
	    * in om_build_http_validate_request(): use domain field.
	    * in OM_do_socket_validate_work(): use real_IP:Port field
	    */
	    rv = get_known_header(reqhdr, MIME_HDR_HOST,
				  &data3, &datalen3, &attrs, &hdrcnt);
	    if (rv) {
		retval = 7;
		break;
	    }

	    rv = get_known_header(reqhdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
				  &data, &datalen, &attrs, &hdrcnt);
	    if (rv) {
	    	retval = 4;
		break;
	    }

	    rv = get_known_header(reqhdr, MIME_HDR_X_NKN_REQ_REAL_DEST_PORT,
				  &data2, &datalen2, &attrs, &hdrcnt);
	    if (!rv) {
	    	tbuf = alloca(datalen + datalen2 + datalen3 + 3);
		tbuf[0]='.';
		memcpy(&tbuf[1], data, datalen); datalen++;
		tbuf[datalen] = ':';  datalen++;
		memcpy(&tbuf[datalen], data2, datalen2); datalen += datalen2;
		tbuf[datalen] = ':';  datalen++;
		memcpy(&tbuf[datalen], data3, datalen3); datalen += datalen3;
		data = tbuf;
	    }
	    break;

	case OSS_RTSP_CONFIG: // Dont know which header to pick - use HOST?
	    rv = get_known_header(reqhdr, MIME_HDR_HOST,
				  &data, &datalen, &attrs, &hdrcnt);
	    if (rv) {
	    	retval = 5;
	    }
	default:
	    retval = 6;
	    break;
	}

	if (!retval) {
	    rv = add_known_header(resphdr, MIME_HDR_X_NKN_CLIENT_HOST,
			          data, datalen);
	    if (rv) {
	    	retval = 7;
	    }
	}
	rv = get_known_header(reqhdr, MIME_HDR_X_NKN_ORIGIN_SVR,
			      &data, &datalen, &attrs, &hdrcnt);
	if (!rv) {
	    add_known_header(resphdr, MIME_HDR_X_NKN_ORIGIN_SVR,
			     data, datalen);
	}

	rv = get_known_header(reqhdr, MIME_HDR_X_NKN_CACHE_POLICY,
			      &data, &datalen, &attrs, &hdrcnt);
	if (!rv) {
	    add_known_header(resphdr, MIME_HDR_X_NKN_CACHE_POLICY,
			     data, datalen);
	}
	rv = get_known_header(reqhdr, MIME_HDR_X_NKN_CACHE_NAME,
			      &data, &datalen, &attrs, &hdrcnt);
	if (!rv) {
	    add_known_header(resphdr, MIME_HDR_X_NKN_CACHE_NAME,
			     data, datalen);
	}
	rv = get_known_header(reqhdr, MIME_HDR_X_NKN_PE_HOST_HDR,
			      &data, &datalen, &attrs, &hdrcnt);
	if (!rv) {
	    add_known_header(resphdr, MIME_HDR_X_NKN_PE_HOST_HDR,
			     data, datalen);
	}

	return retval;
}

/*
 * get_active_namespace_uids()
 *  Return the ns_uid(s) associated with the active namespaces.
 *
 *  Returns: ns_active_uid_list_t which must be free'ed by the caller.
 */
ns_active_uid_list_t  *get_active_namespace_uids(void)
{
    int n, delflag;
    namespace_descriptor_t *nsd = NULL;
    ns_active_uid_list_t *uid_list = 0;

    nsd = ref_namespace_descriptor_t(NULL, &delflag);
    if (!nsd || !nsd->used_namespaces) {
    	goto exit;
    }

    uid_list = nkn_calloc_type(1, sizeof(ns_active_uid_list_t) + 
			          ((nsd->used_namespaces-1) * sizeof(char *)),
			       mod_ns_uid_list);
    if (!uid_list) {
    	goto exit;
    }

    uid_list->num_entries = nsd->used_namespaces;
    for (n = 0; n < nsd->used_namespaces; n++) {
    	uid_list->ns_uid_list[n] = 
		nkn_strdup_type(nsd->namespaces[n]->ns_uid, mod_ns_uid_strdup);
    }
exit:
    unref_namespace_descriptor_t(nsd, delflag);
    return uid_list;
}

void free_active_uid_list(ns_active_uid_list_t *ns_list)
{
    int i;

    for (i=0; i<ns_list->num_entries; i++) {
	free((void *)ns_list->ns_uid_list[i]);
	ns_list->ns_uid_list[i] = NULL;
    }
    free(ns_list);
}

/*
 * nkn_add_ns_status_callback() - Register callback for namespace
 *				  online/offline notification
 *
 *	Returns: 0 => Success
 */
static int num_ns_calback_procs = 0;
static ns_calback_proc_t *cb_procs[16];

int nkn_add_ns_status_callback(ns_calback_proc_t *proc)
{
    if (num_ns_calback_procs < 
    		(int)(sizeof(cb_procs)/sizeof(ns_calback_proc_t*))) {
    	cb_procs[num_ns_calback_procs++] = proc;
	return 0;
    } else {
	return 1;
    }
}

/*
 * get_active_namespace_token_ctx() - Get the namespace_token_context_t
 *				associated with the active namespaces.
 *
 *	Returns: active number namespaces + namespace_token_context_t
 */
int get_active_namespace_token_ctx(namespace_token_context_t *ctx)
{
    namespace_descriptor_t *nsd = NULL;
    int used_namespaces;
    int delflag;

    nsd = ref_namespace_descriptor_t(NULL, &delflag);
    if (!nsd || !nsd->used_namespaces) {
	used_namespaces = 0;
    } else {
    	used_namespaces = nsd->used_namespaces;
    	*((namespace_token_t *)ctx) = nsd->token_start;
    }
    unref_namespace_descriptor_t(nsd, delflag);
    return used_namespaces;
}

/*
 * get_nth_active_namespace_token() - Get the nth active  namespace_token_t 
 *				given namespace_token_context_t.
 */
namespace_token_t 
get_nth_active_namespace_token(namespace_token_context_t *ctx, int nth)
{
    namespace_token_t token = *((namespace_token_t *)ctx);
    token.u.token_data.val += nth;
    return token;
}

/*
 *******************************************************************************
 * 		P R I V A T E  F U N C T I O N S
 *******************************************************************************
 */
STATIC namespace_descriptor_t *ref_namespace_descriptor_t(
						namespace_descriptor_t *my_nsd,
						int *delflag)
{
    namespace_descriptor_t *nsd;
    *delflag = 0;

    NKN_RWLOCK_RLOCK(&ns_list_lock);
    nsd = my_nsd ? my_nsd : CURRENT_NSD;
    if (nsd) {
	AO_fetch_and_add1(&nsd->refcnt);
	*delflag = nsd->flags & NSD_FLAGS_DELETE;
    }
    NKN_RWLOCK_UNLOCK(&ns_list_lock);

    return nsd;
}

STATIC void unref_namespace_descriptor_t(namespace_descriptor_t *nsd, 
					 int delflag)
{
    int delete_nsd = 0;
    int old_ref;
    int deleted_nfs_namespaces = 0;
    namespace_token_t *deleted_nfs_token_list;

    if (!nsd)
	    return;
    if ((delflag & NSD_FLAGS_DELETE)) {
	NKN_RWLOCK_WLOCK(&ns_list_lock);
	old_ref = AO_fetch_and_sub1(&nsd->refcnt);
	if((old_ref - 1) == 0) {
	    NSD_UNLINK(nsd);
	    delete_nsd = 1;
	    deleted_nfs_namespaces = 
		compute_deleted_NFS_entries(nsd, CURRENT_NSD,
					    &deleted_nfs_token_list);
	}
    } else {
	AO_fetch_and_sub1(&nsd->refcnt);
    }

    if (delete_nsd) {
	// Generate namespace offline event
    	ns_status_callback(nsd, 0);
    	delete_namespace_descriptor_t(nsd);
    }

    if (deleted_nfs_namespaces) {
    	notify_nfs(deleted_nfs_token_list, deleted_nfs_namespaces);
	free(deleted_nfs_token_list);
    }
    if ((delflag & NSD_FLAGS_DELETE)) {
	NKN_RWLOCK_UNLOCK(&ns_list_lock);
    }
}

STATIC int update_namespace_data(int system_init)
{
    int rv;
    int n;
    namespace_descriptor_t *nsd;
    namespace_descriptor_t *old_nsd = 0;
    int deleted_nfs_namespaces = 0;
    namespace_token_t *deleted_nfs_token_list;
    namespace_token_t nstoken;

    nsd = new_namespace_descriptor_t();
    if (!nsd) {
	DBG_LOG(SEVERE, MOD_NAMESPACE, "new_namespace_descriptor_t() failed");
	DBG_ERR(SEVERE,  "new_namespace_descriptor_t() failed");
	return 1;
    }

    rv = init_namespace_descriptor_t(nsd);
    if (!rv) {
	if (!nsd->num_intercept_handlers) {
	    NSD_NO_INTERCEPT = nsd;
	} else {
	    NSD_NO_INTERCEPT = NULL;
	}
    	if (!system_init) {
	    NKN_RWLOCK_WLOCK(&ns_list_lock);
	    if (CURRENT_NSD) {
	    	if (AO_load(&CURRENT_NSD->refcnt)) {
	    	    CURRENT_NSD->flags |= NSD_FLAGS_DELETE;
	    	} else {
	    	    old_nsd = CURRENT_NSD;
		    NSD_UNLINK(old_nsd);
		    deleted_nfs_namespaces = 
			compute_deleted_NFS_entries(old_nsd, nsd,
					    &deleted_nfs_token_list);
	    	}
	    }
	    CURRENT_NSD = nsd;
	    NSD_LINK_TAIL(nsd);
	    NKN_RWLOCK_UNLOCK(&ns_list_lock);

	    // Note: Order matters here. 
	    // Minimize nvsd/cmm config memory foot print by
	    // deleting the old config before creating the new config.

	    if (old_nsd) {
	    	// Generate namespace offline event
	    	ns_status_callback(old_nsd, 0);
    		delete_namespace_descriptor_t(old_nsd);
	    }

	    // Generate namespace online event
	    ns_status_callback(nsd, 1);
	} else {
	    CURRENT_NSD = nsd;
	    NSD_LINK_TAIL(nsd);

	    // Generate namespace online event
	    ns_status_callback(nsd, 1);
	}
    } else {
	DBG_LOG(SEVERE, MOD_NAMESPACE, "init_namespace_descriptor_t() failed");
	DBG_ERR(SEVERE, "init_namespace_descriptor_t() failed");
    	delete_namespace_descriptor_t(nsd);
	return 2;
    }

    if (deleted_nfs_namespaces) {
    	notify_nfs(deleted_nfs_token_list, deleted_nfs_namespaces);
	free(deleted_nfs_token_list);
    }

    for (n = 0; n < CURRENT_NSD->used_namespaces; n++) {
    	if (CURRENT_NSD->namespaces[n]->http_config->origin.select_method == 
		OSS_NFS_SERVER_MAP) {
    	    nstoken = CURRENT_NSD->token_start;
	    nstoken.u.token_data.val += n;
	    NFS_configure(nstoken, CURRENT_NSD->namespaces[n]);
	}
    }
    return 0;
}

STATIC void init_ns_node_data_from_config(namespace_node_data_t *nd)
{
    // Origin Fetch
    nd->origin_fetch.cache_directive_no_cache = 
    	(char *)cl7pxyns_origin_fetch_cache_directive_no_cache;

    nd->origin_fetch.cache_age =
	cl7pxyns_origin_fetch_cache_age;

    nd->origin_fetch.custom_cache_control =
	(char *)cl7pxyns_origin_fetch_custom_cache_control;

    nd->origin_fetch.partial_content =
    	cl7pxyns_origin_fetch_partial_content;

    nd->origin_fetch.date_header_modify =
    	cl7pxyns_origin_fetch_date_header_modify;

    nd->origin_fetch.flush_intermediate_caches =
	cl7pxyns_origin_fetch_flush_intermediate_caches;

    nd->origin_fetch.content_store_cache_age_threshold =
	cl7pxyns_origin_fetch_content_store_cache_age_threshold;

    nd->origin_fetch.content_store_uri_depth_threshold =
	cl7pxyns_origin_fetch_content_store_uri_depth_threshold;

    nd->origin_fetch.offline_fetch_size =
	cl7pxyns_origin_fetch_offline_fetch_size;

    nd->origin_fetch.offline_fetch_smooth_flow =
	cl7pxyns_origin_fetch_offline_fetch_smooth_flow;

    nd->origin_fetch.convert_head =
	cl7pxyns_origin_fetch_convert_head;

    nd->origin_fetch.store_cache_min_size =
	cl7pxyns_origin_fetch_store_cache_min_size;

    nd->origin_fetch.is_set_cookie_cacheable =
	cl7pxyns_origin_fetch_is_set_cookie_cacheable;

    nd->origin_fetch.is_host_header_inheritable =
	cl7pxyns_origin_fetch_is_host_header_inheritable;

    assert(MAX_CONTENT_TYPES >= 5);
    nd->origin_fetch.content_type_count =
	cl7pxyns_origin_fetch_content_type_count;

    nd->origin_fetch.cache_age_map[0].content_type =
	(char *)cl7pxyns_origin_fetch_cache_age_map_0_content_type;

    nd->origin_fetch.cache_age_map[0].cache_age =
	cl7pxyns_origin_fetch_cache_age_map_0_cache_age;

    nd->origin_fetch.cache_age_map[1].content_type =
	(char *)cl7pxyns_origin_fetch_cache_age_map_1_content_type;

    nd->origin_fetch.cache_age_map[1].cache_age =
	cl7pxyns_origin_fetch_cache_age_map_1_cache_age;

    nd->origin_fetch.cache_age_map[2].content_type =
	(char *)cl7pxyns_origin_fetch_cache_age_map_2_content_type;

    nd->origin_fetch.cache_age_map[2].cache_age =
	cl7pxyns_origin_fetch_cache_age_map_2_cache_age;

    nd->origin_fetch.cache_age_map[3].content_type =
	(char *)cl7pxyns_origin_fetch_cache_age_map_3_content_type;

    nd->origin_fetch.cache_age_map[3].cache_age =
	cl7pxyns_origin_fetch_cache_age_map_3_cache_age;

    nd->origin_fetch.cache_age_map[4].content_type =
	(char *)cl7pxyns_origin_fetch_cache_age_map_4_content_type;

    nd->origin_fetch.cache_age_map[4].cache_age =
	cl7pxyns_origin_fetch_cache_age_map_4_cache_age;

    nd->origin_fetch.cache_barrier_revalidate.reval_time =
	cl7pxyns_origin_fetch_cache_barrier_revalidate_reval_time;

    nd->origin_fetch.cache_barrier_revalidate.reval_type =
	cl7pxyns_origin_fetch_cache_barrier_revalidate_reval_type;

    nd->origin_fetch.disable_cache_on_query =
	cl7pxyns_origin_fetch_disable_cache_on_query;

    nd->origin_fetch.strip_query =
	cl7pxyns_origin_fetch_strip_query;

    nd->origin_fetch.use_date_header =
	cl7pxyns_origin_fetch_use_date_header;

    nd->origin_fetch.disable_cache_on_cookie =
	cl7pxyns_origin_fetch_disable_cache_on_cookie;

    nd->origin_fetch.client_req_max_age =
	cl7pxyns_origin_fetch_client_req_max_age;

    nd->origin_fetch.client_req_serve_from_origin =
	cl7pxyns_origin_fetch_client_req_serve_from_origin;

    nd->origin_fetch.cache_fill =
	cl7pxyns_origin_fetch_cache_fill;

    nd->origin_fetch.ingest_policy =
	cl7pxyns_origin_fetch_ingest_policy;

    nd->origin_fetch.ingest_only_in_ram =
	cl7pxyns_origin_fetch_ingest_only_in_ram;

    nd->origin_fetch.eod_on_origin_close =
	cl7pxyns_origin_fetch_eod_on_origin_close;

    nd->origin_fetch.object.start_header =
	(char *)cl7pxyns_origin_fetch_object_start_header;

    nd->origin_fetch.object.cache_pin.cache_pin_ena =
	cl7pxyns_origin_fetch_object_cache_pin_cache_pin_ena;

    nd->origin_fetch.object.cache_pin.cache_auto_pin =
	cl7pxyns_origin_fetch_object_cache_pin_cache_auto_pin;

    nd->origin_fetch.object.cache_pin.cache_resv_bytes =
	cl7pxyns_origin_fetch_object_cache_pin_cache_resv_bytes;

    nd->origin_fetch.object.cache_pin.max_obj_bytes =
	cl7pxyns_origin_fetch_object_cache_pin_max_obj_bytes;

    nd->origin_fetch.object.cache_pin.pin_header =
	(char *)cl7pxyns_origin_fetch_object_cache_pin_pin_header;

    nd->origin_fetch.object.cache_pin.end_header =
	(char *)cl7pxyns_origin_fetch_object_cache_pin_end_header;

    nd->origin_fetch.redirect_302 =
	cl7pxyns_origin_fetch_redirect_302;

    nd->origin_fetch.tunnel_all =
	cl7pxyns_origin_fetch_tunnel_all;

    nd->origin_fetch.object_correlation_etag_ignore =
	cl7pxyns_origin_fetch_object_correlation_etag_ignore;

    nd->origin_fetch.object_correlation_validatorsall_ignore =
	cl7pxyns_origin_fetch_object_correlation_validatorsall_ignore;

    nd->origin_fetch.client_driven_agg_threshold =
	cl7pxyns_origin_fetch_client_driven_agg_threshold;

    nd->origin_fetch.cache_obj_size_min =
	cl7pxyns_origin_fetch_cache_obj_size_min;

    nd->origin_fetch.cache_obj_size_max =
	cl7pxyns_origin_fetch_cache_obj_size_max;

    nd->origin_fetch.cache_reval.cache_revalidate =
	cl7pxyns_origin_fetch_cache_reval_cache_revalidate;

    nd->origin_fetch.cache_reval.method =
	cl7pxyns_origin_fetch_cache_reval_method;

    nd->origin_fetch.cache_reval.validate_header =
	cl7pxyns_origin_fetch_cache_reval_validate_header;

    nd->origin_fetch.cache_reval.header_name =
	(char *)cl7pxyns_origin_fetch_cache_reval_header_name;

    nd->origin_fetch.cache_ingest.hotness_threshold =
	cl7pxyns_origin_fetch_cache_ingest_hotness_threshold;

    nd->origin_fetch.cache_ingest.size_threshold =
	cl7pxyns_origin_fetch_cache_ingest_size_threshold;

    nd->origin_fetch.cacheable_no_revalidation =
	cl7pxyns_origin_fetch_cacheable_no_revalidation;

    nd->origin_fetch.client_req_connect_handle =
	cl7pxyns_origin_fetch_client_req_connect_handle;

	

    // Origin Request
    assert(MAX_ORIGIN_REQUEST_HEADERS >= 4);
    nd->origin_request.add_headers[0].name =
	(char *)cl7pxyns_origin_request_add_headers_name_0;

    nd->origin_request.add_headers[0].value =
	(char *)cl7pxyns_origin_request_add_headers_value_0;

    nd->origin_request.add_headers[1].name =
	(char *)cl7pxyns_origin_request_add_headers_name_1;

    nd->origin_request.add_headers[1].value =
	(char *)cl7pxyns_origin_request_add_headers_value_1;

    nd->origin_request.add_headers[2].name =
	(char *)cl7pxyns_origin_request_add_headers_name_2;

    nd->origin_request.add_headers[2].value =
	(char *)cl7pxyns_origin_request_add_headers_value_2;

    nd->origin_request.add_headers[3].name =
	(char *)cl7pxyns_origin_request_add_headers_name_3;

    nd->origin_request.add_headers[3].value =
	(char *)cl7pxyns_origin_request_add_headers_value_3;

    nd->origin_request.x_forward_header =
	cl7pxyns_origin_request_x_forward_header;

    nd->origin_request.orig_conn_values.conn_timeout =
	cl7pxyns_origin_request_orig_conn_values_conn_timeout;

    nd->origin_request.orig_conn_values.conn_retry_delay =
	cl7pxyns_origin_request_orig_conn_values_conn_retry_delay;

    nd->origin_request.orig_conn_values.read_timeout =
	cl7pxyns_origin_request_orig_conn_values_read_timeout;

    nd->origin_request.orig_conn_values.read_retry_delay =
	cl7pxyns_origin_request_orig_conn_values_read_retry_delay;
}

STATIC namespace_node_data_t *
new_CL7_node_namespace_node_data_t(const char *host_port, 
				   const int host_port_strlen, int node_num,
				   const namespace_config_t *parent_nsc)
{
    int rv;
    namespace_node_data_t *nd = NULL;
    int domain_len;
    uint16_t port;
    char namespace_name[NKN_MAX_NAMESPACE_LENGTH+1];
    char namespace_uid[NKN_MAX_UID_LENGTH+1];
    char *puuid;

    if (!host_port || !host_port_strlen) {
    	goto error;
    }

    nd = (namespace_node_data_t *)nkn_calloc_type(1, 
    				sizeof(namespace_node_data_t),
				mod_ns_tmp_ns_node_data_t);
    if (!nd) {
    	goto error;
    }

    // Initialize origin_fetch and origin_request from config
    init_ns_node_data_from_config(nd);

   /*
    * Construct a namespace_node_data_t for the specified Cluster L7 Node
    *
    * - namespace name: cl7-<node #>-<parent namespace name>
    * - namespace uid: 
    *        /cl7-<node #>-<parent namespace name>:<parent namespace uuid>
    * - Domain: *
    * - Protocol HTTP
    * - Match virtual host 255.255.255.255:65535
    * - Origin Selection: OSF_HTTP_PROXY_HOST to given host:port
    * - Allow client "Cache-Control: max-age=0" serve from origin
    * - Allow Host header inheritance on OM requests
    * - Disable cache on query
    * - Disable cache revalidate
    * - Cache only in RAM
    * - Pass through HTTP HEAD
    * - Origin fetch cache_directive_no_cache, always tunnel
    */
    /*
     ***************************************************************************
     * Note: The Cluster L7 Proxy namespaces are hand crafted to tailor
     *       CL7 Proxy request processing to perform the redirect action
     *       on behalf of the client as defined by CL7 Redirect.   
     *       Before modifying the namespace template you must study 
     *       the implications of a change by reviewing the request processing 
     *       code path.
     ***************************************************************************
     */

    rv = snprintf(namespace_name, sizeof(namespace_name), "CL7-%03d-%s",
    		  node_num, parent_nsc->namespace);
    namespace_name[NKN_MAX_NAMESPACE_LENGTH] = '\0';

    puuid = strchr(parent_nsc->ns_uid, ':');
    if (puuid) {
    	puuid++;
    } else {
    	puuid = (char *)"00000000";
    }
    rv = snprintf(namespace_uid, sizeof(namespace_uid), "/%s:%s",
    		  namespace_name, puuid);
    namespace_uid[NKN_MAX_UID_LENGTH] = '\0';

    nd->namespace = nkn_strdup_type(namespace_name, mod_ns_cl7_ns_name);
    nd->active = 1;
    nd->uid = nkn_strdup_type(namespace_uid, mod_ns_cl7_ns_uid);

    nd->domain_fqdns[0] = (char *)"*";
    nd->num_domain_fqdns = 1;
    nd->proto_http = 1;

    nd->ns_match.type = MATCH_VHOST;
    nd->ns_match.match.virhost.fqdn = (char *)"255.255.255.255";
    nd->ns_match.match.virhost.port = 65535;

    nd->origin_server.proto = OSF_HTTP_PROXY_HOST;
    rv = parse_host_header(host_port, host_port_strlen, &domain_len, &port, 0);
    if (rv) {
    	goto error;
    }
    nd->origin_server.http.host = nkn_malloc_type(domain_len+1, 
    						mod_ns_node_data_host);
    if (!nd->origin_server.http.host) {
    	goto error;
    }
    memcpy(nd->origin_server.http.host, host_port, domain_len);
    nd->origin_server.http.host[domain_len] = '\0';
    nd->origin_server.http.port = port;

    nd->client_request.passthrough_http_head = 1;

    return nd;

error:
    if (nd) {
    	delete_CL7_node_namespace_node_data_t(nd);
    }
    return NULL;
}

STATIC void
delete_CL7_node_namespace_node_data_t(namespace_node_data_t *nd)
{
    if (!nd) {
        return;
    }

    if (nd->namespace) {
    	free(nd->namespace);
    	nd->namespace = NULL;
    }
    if (nd->uid) {
    	free(nd->uid);
    	nd->uid = NULL;
    }
    if (nd->origin_server.http.host) {
    	free(nd->origin_server.http.host);
    	nd->origin_server.http.host = NULL;
    }
    free(nd);
}

STATIC int create_cluster_L7_node_namespaces(namespace_descriptor_t *nsd, 
					     int parent_ns_index,
					     cluster_descriptor_t *cld,
					     node_map_descriptor_t *nmd)
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
    int nth;
    namespace_node_data_t *ns_nd;
    namespace_config_t **t_ns;

    // Get node configuration
    rv = (*nmd->get_config)(&nmd->ctx, &nodes, 
			    &host_port[0], 
			    &host_port_strlen[0],
			    &http_host_port[0], 
			    &http_host_port_strlen[0],
			    &heartbeat_path[0], 
			    &heartbeat_path_strlen[0]);
    while (1) {

    if (rv) {
	DBG_LOG(SEVERE, MOD_NAMESPACE, "(*nmd->get_config)() failed, rv=%d",
		rv);
	DBG_ERR(SEVERE, "(*nmd->get_config(() failed, rv=%d", rv);
    	retval = 1;
	break;
    }

    for (nth = 0; nth < nodes; nth++) {
    	ns_nd = new_CL7_node_namespace_node_data_t(http_host_port[nth],
					   *http_host_port_strlen[nth],
					   nth,
					   nsd->namespaces[parent_ns_index]);
	if (!ns_nd) {
	    DBG_LOG(SEVERE, MOD_NAMESPACE, 
	    	    "new_CL7_node_namespace_node_data_t() failed");
	    DBG_ERR(SEVERE, "new_CL7_node_namespace_node_data_t() failed");
	    retval = 2;
	    break;
	}
	if (nsd->used_namespaces >= nsd->alloced_namespaces) {
	    nsd->alloced_namespaces *= 2;
	    t_ns = (namespace_config_t **)nkn_realloc_type(nsd->namespaces,
			sizeof(namespace_config_t *) * nsd->alloced_namespaces,
			mod_ns_config_t);
	    if (!t_ns) {
	    	DBG_LOG(SEVERE, MOD_NAMESPACE, "realloc() failed");
	    	DBG_ERR(SEVERE, "realloc() failed");
		retval = 3;
		break;
	    }
	    nsd->namespaces = t_ns;
	}

	nsd->namespaces[nsd->used_namespaces] = new_namespace_config_t(ns_nd);
	delete_CL7_node_namespace_node_data_t(ns_nd);

	if (!nsd->namespaces[nsd->used_namespaces]) {
	    DBG_LOG(SEVERE, MOD_NAMESPACE, "new_namespace_config_t() failed");
	    DBG_ERR(SEVERE, "new_namespace_config_t() failed");
	    retval = 4;
	    break;
	}
	cld->node_id_to_ns_index[nth] = nsd->used_namespaces;
	nsd->used_namespaces++;
    }
    break;

    } // End of while


    return retval;
}

STATIC int add_cluster_L7_node_namespaces(namespace_descriptor_t *nsd)
{
    int nth_ns;
    int nth_cl;
    int defined_namespaces = nsd->used_namespaces;
    namespace_config_t *nsc;
    cluster_descriptor_t *cld;
    node_map_descriptor_t *nmd;
    int rv;
    int retval = 0;

    for (nth_ns = 0; nth_ns < defined_namespaces; nth_ns++) {
	nsc = nsd->namespaces[nth_ns];
	if (!nsc) {
	    // Should never happen
	    DBG_LOG(SEVERE, MOD_NAMESPACE, "NULL namespace_config_t, index=%d",
		    nth_ns);
	    DBG_ERR(SEVERE, "NULL namespace_config_t, index=%d", nth_ns);
	    retval = 1;
	    break;
	}
	for (nth_cl = 0; nth_cl < nsc->cluster_config->num_cld; nth_cl++) {
	    cld = nsc->cluster_config->cld[nth_cl];
	    if (!cld) {
	    	// Should never happen
		DBG_LOG(SEVERE, MOD_NAMESPACE, 
			"NULL cluster_descriptor_t, index=%d", nth_cl);
		DBG_ERR(SEVERE, "NULL cluster_descriptor_t, index=%d", nth_cl);
	    	retval = 2;
	    	break;
	    }

	    CLD2NODE_MAP(cld, nmd);
	    if (nmd) {
		rv = create_cluster_L7_node_namespaces(nsd, nth_ns, cld, nmd);
		if (rv) {
		    DBG_LOG(SEVERE, MOD_NAMESPACE, 
			    "create_cluster_L7_node_namespaces() failed, rv=%d",
			    rv);
		    DBG_ERR(SEVERE, 
			    "create_cluster_L7_node_namespaces() failed, rv=%d",
			    rv);
		    retval = 3;
		}
	    }
	    if (retval) {
	    	break; // error
	    }
	}
    }
    return retval;
}

STATIC int init_namespace_descriptor_t(namespace_descriptor_t *nsd)
{
    namespace_node_data_t *tm_namespace_info;
    namespace_config_t **t_ns;
    int nth = -1;
    int rv;
    struct timespec now, later;
    int64_t time_diff = 0;

    clock_gettime(CLOCK_MONOTONIC, &now);
    nvsd_mgmt_namespace_lock();
    while (1) {
        // Get nth namespace from TM
	nth++;
    	tm_namespace_info = nvsd_mgmt_get_nth_active_namespace(nth);
	if (!tm_namespace_info) {
	    break;
	}

	if (!tm_namespace_info->active) {
	    // Ignore disabled namespaces
	    continue;
	}

	if (nsd->used_namespaces >= nsd->alloced_namespaces) {
	    nsd->alloced_namespaces *= 2;
	    t_ns = (namespace_config_t **)nkn_realloc_type(nsd->namespaces,
			sizeof(namespace_config_t *) * nsd->alloced_namespaces,
			mod_ns_config_t);
	    if (!t_ns) {
	    	DBG_LOG(SEVERE, MOD_NAMESPACE, "realloc() failed");
	    	DBG_ERR(SEVERE, "realloc() failed");
		nvsd_mgmt_namespace_unlock();

		clock_gettime(CLOCK_MONOTONIC, &later);
		time_diff = nkn_timespec_diff_msecs(&now, &later);
		DBG_LOG(MSG, MOD_NAMESPACE, "nvsd-mgmt-lock time-diff :"
			" %ld", time_diff);

		return 1;
	    }
	    nsd->namespaces = t_ns;
	}
	nsd->namespaces[nsd->used_namespaces] = 
		new_namespace_config_t(tm_namespace_info);

	if (!nsd->namespaces[nsd->used_namespaces]) {
	    DBG_LOG(SEVERE, MOD_NAMESPACE, "new_namespace_config_t() failed");
	    DBG_ERR(SEVERE, "new_namespace_config_t() failed");
	    nvsd_mgmt_namespace_unlock();

	    clock_gettime(CLOCK_MONOTONIC, &later);
	    time_diff = nkn_timespec_diff_msecs(&now, &later);
	    DBG_LOG(MSG, MOD_NAMESPACE, "nvsd-mgmt-lock time-diff :"
		    " %ld", time_diff);

	    return 2;
	}

	nsd->used_namespaces++;
    }
    nvsd_mgmt_namespace_unlock();

    clock_gettime(CLOCK_MONOTONIC, &later);
    time_diff = nkn_timespec_diff_msecs(&now, &later);
    DBG_LOG(MSG, MOD_NAMESPACE, "nvsd-mgmt-lock time-diff : %ld", time_diff);

    rv = init_hashtables(nsd);
    if (rv) {
	DBG_LOG(SEVERE, MOD_NAMESPACE, "init_hashtables() failed");
	DBG_ERR(SEVERE, "init_hashtables() failed");
	return 3;
    }

    rv = init_intercept_handler(nsd);
    if (rv) {
	DBG_LOG(SEVERE, MOD_NAMESPACE, "init_intercept_handler() failed");
	DBG_ERR(SEVERE, "init_intercept_handler() failed");
	return 4;
    }

    // Add Cluster L7 implict node namespaces after the hash computation
    // since these should never be accessible in request to namespace lookup.

    rv = add_cluster_L7_node_namespaces(nsd);
    if (rv) {
	DBG_LOG(SEVERE, MOD_NAMESPACE, 
		"add_cluster_L7_node_namespaces() failed, rv=%d", rv);
	DBG_ERR(SEVERE, "add_cluster_L7_node_namespaces() failed, rv=%d", rv);
	return 5;
    }

    if (nsd->used_namespaces) {
    	pthread_mutex_lock(&ns_token_lock);
	if ((NS_TOKEN_GENERATOR.u.token_data.val + nsd->used_namespaces) <
	    NS_TOKEN_GENERATOR.u.token_data.val) {
	    NS_TOKEN_GENERATOR.u.token_data.val = 1;
	}

    	nsd->token_start = NS_TOKEN_GENERATOR;
    	NS_TOKEN_GENERATOR.u.token_data.val += nsd->used_namespaces;
    	pthread_mutex_unlock(&ns_token_lock);

    	nsd->token_end = nsd->token_start;
    	nsd->token_end.u.token_data.val += (nsd->used_namespaces - 1);
    } else {
    	nsd->token_start = NULL_NS_TOKEN;
    	nsd->token_end = NULL_NS_TOKEN;
    }
    DBG_LOG(MSG, MOD_NAMESPACE, "gen=%u token_start=%u token_end=%u",
    	    nsd->token_start.u.token_data.gen,
    	    nsd->token_start.u.token_data.val, nsd->token_end.u.token_data.val);

    return 0;
}

STATIC namespace_config_t *new_namespace_config_t(namespace_node_data_t *nd)
{
    namespace_config_t *nsc;
    const namespace_config_t *old_nsc;
    int rv;
    int ns_idx, domain_idx; 
    uint32_t ind;
    char errorbuf[1024];

    nsc = (namespace_config_t *)nkn_calloc_type(1, sizeof(namespace_config_t), 
    						mod_ns_config_t);
    if (!nsc) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	goto err_exit;
    }

    if (!nd->uid) {
    	DBG_LOG(MSG, MOD_NAMESPACE, "namespace uid==0");
	goto err_exit;
    }

    if (nd->domain_regex && *nd->domain_regex) {
	nsc->hostname_regex = nkn_strdup_type(nd->domain_regex, 
					      mod_ns_hostname_regex);
	// default regex context is 0
	// network thread will take from 1 to NM_tot_threads + 1
	nsc->hostname_regex_tot_cnt = (NM_tot_threads + 1);
    	nsc->hostname_regex_ctx =
		nkn_regex_ctx_arr_alloc(nsc->hostname_regex_tot_cnt);
	if (!nsc->hostname_regex_ctx) {
    	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_regex_ctx_alloc() failed");
	    goto err_exit;
	}
	for (ind = 0; ind < nsc->hostname_regex_tot_cnt; ++ind) {
    	    rv = nkn_regcomp(nkn_regaindex(nsc->hostname_regex_ctx, ind),
			     nsc->hostname_regex,
			     errorbuf, sizeof(errorbuf));
	    if (rv) {
	    	errorbuf[sizeof(errorbuf)-1] = '\0';
    	    	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_regcomp() failed, rv=%d, "
	    	    "regex=[\"%s\"] error=[%s]", 
		    rv, nsc->hostname_regex, errorbuf);
	    	goto err_exit;
	     }
	}
    } else {
	if (nd->num_domain_fqdns == 0) {
	    DBG_LOG(MSG, MOD_NAMESPACE, "!domain");
	    goto err_exit;
	}

	
	for (domain_idx = 0; domain_idx < nd->num_domain_fqdns ; ++domain_idx) {
    	    if (strcmp("*", nd->domain_fqdns[domain_idx]) == 0) {
    	    	nsc->hostname[domain_idx] = 
			nkn_strdup_type(DEFAULT_HOSTNAME, mod_ns_hostname);
    	    } else {
    	    	nsc->hostname[domain_idx] = 
			nkn_strdup_type(nd->domain_fqdns[domain_idx],
			mod_ns_hostname);
    	    }
	}
	nsc->hostname_cnt = nd->num_domain_fqdns;
    }

    switch(nd->ns_match.type) {
    case MATCH_URI_NAME:
	if (!nd->ns_match.match.uri.uri_prefix) {
	    DBG_LOG(MSG, MOD_NAMESPACE, "MATCH_URI_NAME uri_prefix=NULL");
	    goto err_exit;
	}
	nsc->sm.up.uri_prefix = nkn_strdup_type(
					nd->ns_match.match.uri.uri_prefix,
				      	mod_ns_uri_prefix);
	nsc->select_method = NSM_URI_PREFIX;
	break;

    case MATCH_URI_REGEX:
	if (!nd->ns_match.match.uri.uri_prefix_regex) {
	    DBG_LOG(MSG, MOD_NAMESPACE, 
	    	    "MATCH_URI_REGEX uri_prefix_regex=NULL");
	    goto err_exit;
	}
    	nsc->sm.upr.uri_prefix_regex = 
		nkn_strdup_type(nd->ns_match.match.uri.uri_prefix_regex, 
				mod_ns_uri_regex);
	// default regex context is 0
	// network thread will take from 1 to NM_tot_threads + 1
	nsc->sm.upr.uri_prefix_regex_tot_cnt = (NM_tot_threads + 1);	
    	nsc->sm.upr.uri_prefix_regex_ctx =
		nkn_regex_ctx_arr_alloc(nsc->sm.upr.uri_prefix_regex_tot_cnt);
	if (!nsc->sm.upr.uri_prefix_regex_ctx) {
    	    DBG_LOG(MSG, MOD_NAMESPACE, "nkn_regex_ctx_alloc() failed");
	    goto err_exit;
	}
	for (ind = 0 ; ind < nsc->sm.upr.uri_prefix_regex_tot_cnt; ++ind) { 
    		rv = nkn_regcomp(nkn_regaindex(nsc->sm.upr.uri_prefix_regex_ctx, ind), 
			 	 nsc->sm.upr.uri_prefix_regex,
			 	 errorbuf, sizeof(errorbuf));
		if (rv) {
	    		errorbuf[sizeof(errorbuf)-1] = '\0';
    	    		DBG_LOG(MSG, MOD_NAMESPACE, "nkn_regcomp() failed, rv=%d, "
	    	    	"regex=[\"%s\"] error=[%s]", 
		    	rv, nsc->hostname_regex, errorbuf);
	    		goto err_exit;
		}
	}
	nsc->select_method = NSM_URI_PREFIX_REGEX;
	break;

    case MATCH_HEADER_NAME:
	if (!nd->ns_match.match.header.name || 
	    !nd->ns_match.match.header.value) {
	    DBG_LOG(MSG, MOD_NAMESPACE, 
		    "MATCH_HEADER_NAME header.name=%p header.value=%p",
		    nd->ns_match.match.header.name, 
		    nd->ns_match.match.header.value);
	    goto err_exit;
	}
	nsc->sm.hv.header_name = nkn_strdup_type(
			                 nd->ns_match.match.header.name,
					 mod_ns_header_name);
	nsc->sm.hv.header_value = nkn_strdup_type(
			                 nd->ns_match.match.header.value,
					 mod_ns_header_value);
	nsc->select_method = NSM_HEADER_VALUE;

	break;

    case MATCH_HEADER_REGEX:
	if (!nd->ns_match.match.header.regex) {
	    DBG_LOG(MSG, MOD_NAMESPACE, 
		    "MATCH_HEADER_REGEX header.regex=NULL");
	    goto err_exit;
	}
	
	nsc->sm.hvr.header_name_value_regex = 
		nkn_strdup_type(nd->ns_match.match.header.regex,
				mod_ns_header_regex);
	// default regex context is 0
	// network thread will take from 1 to NM_tot_threads + 1
	nsc->sm.hvr.header_name_value_regex_tot_cnt = (NM_tot_threads + 1);
	nsc->sm.hvr.header_name_value_regex_ctx = 
	     nkn_regex_ctx_arr_alloc(nsc->sm.hvr.header_name_value_regex_tot_cnt);
	if (!nsc->sm.hvr.header_name_value_regex_ctx) {
		DBG_LOG(MSG, MOD_NAMESPACE, "nkn_regex_ctx_alloc() failed");
		goto err_exit;
	}
	for (ind = 0 ; ind < nsc->sm.hvr.header_name_value_regex_tot_cnt; ++ind) {
		rv = nkn_regcomp(nkn_regaindex(
				 nsc->sm.hvr.header_name_value_regex_ctx, ind),
			 	 nsc->sm.hvr.header_name_value_regex,
			 	 errorbuf, sizeof(errorbuf));
		if (rv) {
	    		errorbuf[sizeof(errorbuf)-1] = '\0';
	    		DBG_LOG(MSG, MOD_NAMESPACE, "nkn_regcomp() failed, rv=%d, "
			"regex=[\"%s\"] error=[%s]",
			rv, nsc->sm.hvr.header_name_value_regex, errorbuf);
	    		goto err_exit;
		}
	}
	nsc->select_method = NSM_HEADER_VALUE_REGEX;

	break;

    case MATCH_QUERY_STRING_NAME:
	if (!nd->ns_match.match.qstring.name || 
	    !nd->ns_match.match.qstring.value) {
	    DBG_LOG(MSG, MOD_NAMESPACE, 
		    "MATCH_QUERY_STRING_NAME qstring.name=%p "
		    "qstring.value=%p", nd->ns_match.match.qstring.name, 
		    nd->ns_match.match.qstring.value);
	    goto err_exit;
	}
	nsc->sm.qnv.query_string_nv_name = 
		nkn_strdup_type(nd->ns_match.match.qstring.name,
				mod_ns_query_name);
	nsc->sm.qnv.query_string_nv_value = 
		nkn_strdup_type(nd->ns_match.match.qstring.value,
			        mod_ns_query_value);

	nsc->select_method = NSM_QUERY_NAME_VALUE;
	break;

    case MATCH_QUERY_STRING_REGEX:
	if (!nd->ns_match.match.qstring.regex) {
	    DBG_LOG(MSG, MOD_NAMESPACE, 
		    "MATCH_QUERY_STRING_REGEX qstring.regex=NULL");
	    goto err_exit;
	}

	nsc->sm.qnvr.query_string_regex = 
		nkn_strdup_type(nd->ns_match.match.qstring.regex,
				mod_ns_query_regex);
	// default regex context is 0
	// network thread will take from 1 to NM_tot_threads + 1
	nsc->sm.qnvr.query_string_regex_tot_cnt = (NM_tot_threads + 1);
	nsc->sm.qnvr.query_string_regex_ctx =
		nkn_regex_ctx_arr_alloc(nsc->sm.qnvr.query_string_regex_tot_cnt);
	if (!nsc->sm.qnvr.query_string_regex_ctx) {
		DBG_LOG(MSG, MOD_NAMESPACE, "nkn_regex_ctx_alloc() failed");
		goto err_exit;
	}
	for (ind = 0 ; ind < nsc->sm.qnvr.query_string_regex_tot_cnt; ++ind) {
		rv = nkn_regcomp(nkn_regaindex(
				 nsc->sm.qnvr.query_string_regex_ctx, ind), 
			 	 nsc->sm.qnvr.query_string_regex,
			 	 errorbuf, sizeof(errorbuf));
		if (rv) {
	    		errorbuf[sizeof(errorbuf)-1] = '\0';
	    		DBG_LOG(MSG, MOD_NAMESPACE, "nkn_regcomp() failed, rv=%d, "
			    "regex=[\"%s\"] error=[%s]",
			rv, nsc->sm.qnvr.query_string_regex, errorbuf);
	    		goto err_exit;
		}
	}
	nsc->select_method = NSM_QUERY_NAME_VALUE_REGEX;

	break;

    case MATCH_VHOST:
	if (!nd->ns_match.match.virhost.fqdn || 
	    !nd->ns_match.match.virhost.port) {
	    DBG_LOG(MSG, MOD_NAMESPACE, 
		    "MATCH_VHOST virhost.fqdn=%p virhost.port=%d",
		    nd->ns_match.match.virhost.fqdn, 
		    nd->ns_match.match.virhost.port);
	    goto err_exit;
	}

	nsc->sm.vip.virtual_host_ip = 
		inet_addr(nd->ns_match.match.virhost.fqdn);
	nsc->sm.vip.virtual_host_port = htons(nd->ns_match.match.virhost.port);
	nsc->select_method = NSM_VIRTUAL_IP_PORT;
	break;

    default:
	// Invalid type
	break;
	//goto err_exit;
    } // End of switch

    nsc->namespace = nkn_strdup_type(nd->namespace, mod_ns_namespace);
    nsc->namespace_strlen = strlen(nd->namespace);
    nsc->tproxy_ipaddr = nd->tproxy_ipaddr;
    nsc->proxy_mode = nd->proxy_mode;


    // UID = /{namespace name - 16 char max}:XXXXXXXX (8 hex char max)
    nsc->ns_uid = nkn_strdup_type(nd->uid, mod_ns_uid);
    nsc->ns_uid_strlen = strlen(nsc->ns_uid);

    nsc->md5_checksum_len = nd->md5_checksum_len ;

    //Tunnel over-ride settings
    nsc->tunnel_req_override = 0;
    nsc->tunnel_res_override = 0;



    if (nd->tunnel_override.auth_header) {
        SET_TR_REQ_OVERRIDE(nsc->tunnel_req_override, NKN_TR_REQ_AUTH_HDR);
    }
    if (nd->tunnel_override.cookie) {
        SET_TR_REQ_OVERRIDE(nsc->tunnel_req_override, NKN_TR_REQ_COOKIE);
    }
    if (nd->tunnel_override.query_string) {
        SET_TR_REQ_OVERRIDE(nsc->tunnel_req_override, NKN_TR_REQ_QUERY_STR);
    }
    if (nd->tunnel_override.cache_control) {
        SET_TR_REQ_OVERRIDE(nsc->tunnel_req_override, NKN_TR_REQ_CC_NO_CACHE);
        SET_TR_REQ_OVERRIDE(nsc->tunnel_req_override, NKN_TR_REQ_PRAGMA_NO_CACHE);
    }
    if (nd->tunnel_override.cc_notrans) {
        SET_TR_RES_OVERRIDE(nsc->tunnel_res_override, NKN_TR_RES_CC_NO_TRANSFORM);
    }
    if (nd->tunnel_override.obj_expired) {
        SET_TR_RES_OVERRIDE(nsc->tunnel_res_override, NKN_TR_RES_OBJ_EXPIRED);
    }

    nsc->ssp_config = new_ssp_config_t(nd);
    if (!nsc->ssp_config) {
	DBG_LOG(MSG, MOD_NAMESPACE, "new_ssp_config_t() failed");
	goto err_exit;
    }

    /*
     * Get the old namespace_config_t if present.
     * Note: CURRENT_NSD is guaranteed to be valid and consistent since 
     *       we are in configuration update.
     */
    old_nsc = ns_uid_to_config_t(CURRENT_NSD, nsc->ns_uid);
 
    nsc->http_config = new_http_config_t(nd, nsc, old_nsc);
    if (!nsc->http_config) {
	DBG_LOG(MSG, MOD_NAMESPACE, "new_http_config_t() failed");
	goto err_exit;
    }

    nsc->pub_point_config = new_pub_point_config_t(nd);
    if(!nsc->pub_point_config) {
	DBG_LOG(MSG, MOD_NAMESPACE, "new_pub_point_config_t() failed");
	goto err_exit;
    }

    nsc->rtsp_config = new_rtsp_config_t(nd);
    if (!nsc->http_config) {
	DBG_LOG(MSG, MOD_NAMESPACE, "new_rtsp_config_t() failed");
	goto err_exit;
    }

    nsc->cluster_config = new_cluster_config_t(nd, nsc, old_nsc);
    if (!nsc->cluster_config) {
	DBG_LOG(MSG, MOD_NAMESPACE, "new_cluster_config_t() failed");
	goto err_exit;
    }

    nsc->cache_pin_config = new_cache_pin_config_t(nd);
    if (!nsc->cache_pin_config) {
	DBG_LOG(MSG, MOD_NAMESPACE, "new_cache_pin_config_t() failed");
	goto err_exit;
    }

    nsc->acclog_config = new_accesslog_config_t(nd);
    if (!nsc->acclog_config) {
	DBG_LOG(MSG, MOD_NAMESPACE, "new_accesslog_config_t() failed");
	goto err_exit;
    }


    nsc->compress_config = new_compress_config_t(nd);
    if (!nsc->compress_config) {
	DBG_LOG(MSG, MOD_NAMESPACE, "new_compress_config_t() failed");
	goto err_exit;
    }

    nsc->non2xx_cache_cfg = new_non2xx_cache_config_t(nd);
    if (!nsc->non2xx_cache_cfg) {
	DBG_LOG(MSG, MOD_NAMESPACE, "new_non2xx_cache_config_t() failed");
	goto err_exit;
    }
    nsc->stat = get_namespace_stat_t(nsc->namespace);

    //Associate the namespace index here
    /* If its a not a cluster namespace
     * we should be having that namespace
     * in node_data_t list,
     * else its a clusgter l7 namespace
     * if its not a cluster l7 namespace
     * get the rp index from node_data_t
     * else set the rp_index to -1
     */
    if (search_namespace(nsc->namespace, &ns_idx) == 0)
    {
	     nsc->rp_index = nd->rp_idx;
    } else {
	     nsc->rp_index = -1; // Implicit namespace, CL L7 node namespace
    }

    if (nd->vip_obj.n_vips) {
        int i32index;
        if (nd->vip_obj.n_vips > NKN_MAX_VIPS) {
            nsc->ip_bind_list.n_ips = NKN_MAX_VIPS;
        } else {
            nsc->ip_bind_list.n_ips = nd->vip_obj.n_vips;
        }
        for (i32index = 0; i32index < nsc->ip_bind_list.n_ips; i32index++) {
            // for 12.2, IPv4 alone is supported. Also no IPv6 type support in mgmtd.
            nsc->ip_bind_list.ip_addr[i32index].ip_type = AF_INET;
            inet_pton(AF_INET, nd->vip_obj.vip[i32index], nsc->ip_bind_list.ip_addr[i32index].ipaddr);
        }
    }
    else {
        nsc->ip_bind_list.n_ips = 0;
    }

    nsc->uf_config = new_url_filter_config_t(nd);
    if (!nsc->uf_config) {
	DBG_LOG(MSG, MOD_NAMESPACE, "new_url_filter_config_t() failed");
	goto err_exit;
    }

    // Note: Add new constructors before this comment
#if 0
    // Make callouts

    if (nsc->http_config->origin.type == OS_NFS) {
    	NFS_configure(nsc);
    }
#endif

    return nsc;

err_exit:

    delete_namespace_config_t(nsc);
    return 0;
}

STATIC void delete_namespace_config_t(namespace_config_t *nsc)
{
    int host_idx;
    if (!nsc) {
    	return;
    }

#if 0
    // Make callouts

    if (nsc->http_config && nsc->http_config->origin.type == OS_NFS) {
    	NFS_delete_config(nsc);
    }
#endif

    delete_ssp_config_t(nsc->ssp_config);
    delete_http_config_t(nsc->http_config);
    delete_rtsp_config_t(nsc->rtsp_config);
    delete_pub_point_config_t(nsc->pub_point_config);
    delete_cluster_config_t(nsc->cluster_config);
    delete_cache_pin_config_t(nsc->cache_pin_config);
    delete_compress_config_t(nsc->compress_config);
    delete_non2xx_cache_config_t(nsc->non2xx_cache_cfg);
    delete_accesslog_config_t(nsc->acclog_config);
    delete_url_filter_config_t(nsc->uf_config);
    // unregister stats/counters for this namespace
    release_namespace_stat_t(nsc->stat);

    // Note: Add new destructore before this comment

    for ( host_idx = 0; host_idx < nsc->hostname_cnt; ++host_idx) {  
    	if (nsc->hostname[host_idx]) {
    		free(nsc->hostname[host_idx]);
		nsc->hostname[host_idx] = 0;
    	}
    }

    nsc->hostname_cnt = 0;

    if (nsc->hostname_regex) {
    	free(nsc->hostname_regex);
	nsc->hostname_regex = 0;
    }

    if (nsc->hostname_regex_ctx) {
    	nkn_regex_ctx_free(nsc->hostname_regex_ctx);
    	nsc->hostname_regex_ctx = 0;
    }

    if (nsc->regex_int_hostname) {
    	free(nsc->regex_int_hostname);
	nsc->regex_int_hostname = 0;
    }

    switch(nsc->select_method) {
    case MATCH_URI_NAME:
	if (nsc->sm.up.uri_prefix) {
	    free(nsc->sm.up.uri_prefix);
	    nsc->sm.up.uri_prefix = 0;
	}
	break;

    case MATCH_URI_REGEX:
    	if (nsc->sm.upr.uri_prefix_regex) {
	    free(nsc->sm.upr.uri_prefix_regex);
	    nsc->sm.upr.uri_prefix_regex = 0;
	}
    	if (nsc->sm.upr.uri_prefix_regex_ctx) {
	    nkn_regex_ctx_free(nsc->sm.upr.uri_prefix_regex_ctx);
	    nsc->sm.upr.uri_prefix_regex_ctx = 0;
	}
	break;

    case MATCH_HEADER_NAME:
    	if (nsc->sm.hv.header_name) {
	    free(nsc->sm.hv.header_name);
	    nsc->sm.hv.header_name = 0;
	}
    	if (nsc->sm.hv.header_value) {
	    free(nsc->sm.hv.header_value);
	    nsc->sm.hv.header_value = 0;
	}
	break;

    case MATCH_HEADER_REGEX:
    	if (nsc->sm.hvr.header_name_value_regex) {
	    free(nsc->sm.hvr.header_name_value_regex);
	    nsc->sm.hvr.header_name_value_regex = 0;
	}
	if (nsc->sm.hvr.header_name_value_regex_ctx) {
	    nkn_regex_ctx_free(nsc->sm.hvr.header_name_value_regex_ctx);
	    nsc->sm.hvr.header_name_value_regex_ctx = 0;
	}
	break;

    case MATCH_QUERY_STRING_NAME:
    	if (nsc->sm.qnv.query_string_nv_name) {
	    free(nsc->sm.qnv.query_string_nv_name);
	    nsc->sm.qnv.query_string_nv_name = 0;
	}
    	if (nsc->sm.qnv.query_string_nv_value) {
	    free(nsc->sm.qnv.query_string_nv_value);
	    nsc->sm.qnv.query_string_nv_value = 0;
	}
	break;

    case MATCH_QUERY_STRING_REGEX:
    	if (nsc->sm.qnvr.query_string_regex) {
	    free(nsc->sm.qnvr.query_string_regex);
	    nsc->sm.qnvr.query_string_regex = 0;
	}
	if (nsc->sm.qnvr.query_string_regex_ctx) {
	    nkn_regex_ctx_free(nsc->sm.qnvr.query_string_regex_ctx);
	    nsc->sm.qnvr.query_string_regex_ctx = 0;
	}
	break;

    case MATCH_VHOST:
    	// Nothing to free
	break;

    default:
    	break;
    } // End of switch
    nsc->select_method = 0;

    if (nsc->namespace) {
    	free(nsc->namespace);
	nsc->namespace = 0;
    }

    if (nsc->ns_uid) {
    	free(nsc->ns_uid);
    	nsc->ns_uid = 0;
    }
    free(nsc);
}

STATIC void register_namespace_counters(namespace_stat_t *pstat, 
					const char *name)
{
    char instance[256];
    char statname[1024];
    statname[sizeof(statname) - 1] = '\0';

    /* Register the old counter name *_gets */
    snprintf(statname, sizeof(statname), "glob_namespace_%s_gets", name);
    statname[sizeof(statname) - 1] = '\0';
    nkn_mon_add(statname, NULL, &pstat->stats[GET_REQS], 
		sizeof(pstat->stats[GET_REQS]));

#define BUILD_NAME(cnt_name, var) { \
    snprintf(statname, sizeof(statname), "%s", cnt_name); \
    nkn_mon_add(statname, instance, &(var), sizeof(var)); \
}

    snprintf(instance, sizeof(instance), "ns.%s", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("http.sessions", pstat->g_http_sessions);
    BUILD_NAME("rtsp.sessions", pstat->g_rtsp_sessions);
    BUILD_NAME("tunnels", pstat->g_tunnels);

    snprintf(instance, sizeof(instance), "ns.%s.http.client", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("requests", pstat->client[PROTO_HTTP].requests);
    BUILD_NAME("responses", pstat->client[PROTO_HTTP].responses);
    BUILD_NAME("cache_hits", pstat->client[PROTO_HTTP].cache_hits);
    BUILD_NAME("cache_miss", pstat->client[PROTO_HTTP].cache_miss);
    BUILD_NAME("cache_partial", pstat->client[PROTO_HTTP].cache_partial);
    BUILD_NAME("in_bytes", pstat->client[PROTO_HTTP].in_bytes);
    BUILD_NAME("out_bytes", pstat->client[PROTO_HTTP].out_bytes);
    BUILD_NAME("conns", pstat->client[PROTO_HTTP].conns);
    BUILD_NAME("active_conns", pstat->client[PROTO_HTTP].active_conns);
    BUILD_NAME("idle_conns", pstat->client[PROTO_HTTP].idle_conns);
    BUILD_NAME("resp_2xx", pstat->client[PROTO_HTTP].resp_2xx);
    BUILD_NAME("resp_200", pstat->client[PROTO_HTTP].resp_200);
    BUILD_NAME("resp_206", pstat->client[PROTO_HTTP].resp_206);
    BUILD_NAME("resp_3xx", pstat->client[PROTO_HTTP].resp_3xx);
    BUILD_NAME("resp_302", pstat->client[PROTO_HTTP].resp_302);
    BUILD_NAME("resp_304", pstat->client[PROTO_HTTP].resp_304);
    BUILD_NAME("resp_4xx", pstat->client[PROTO_HTTP].resp_4xx);
    BUILD_NAME("resp_5xx", pstat->client[PROTO_HTTP].resp_5xx);
    BUILD_NAME("resp_404", pstat->client[PROTO_HTTP].resp_404);
    BUILD_NAME("out_bytes_origin", pstat->client[PROTO_HTTP].out_bytes_origin);
    BUILD_NAME("out_bytes_disk", pstat->client[PROTO_HTTP].out_bytes_disk);
    BUILD_NAME("out_bytes_ram", pstat->client[PROTO_HTTP].out_bytes_ram);
    BUILD_NAME("out_bytes_tunnel", pstat->client[PROTO_HTTP].out_bytes_tunnel);
    BUILD_NAME("compress_cnt", pstat->client[PROTO_HTTP].compress_cnt);
    BUILD_NAME("compress_bytes_save", pstat->client[PROTO_HTTP].compress_bytes_save);
    BUILD_NAME("compress_bytes_overrun", pstat->client[PROTO_HTTP].compress_bytes_overrun);
    BUILD_NAME("expired_objs_delivered", pstat->client[PROTO_HTTP].expired_objects);
    // Server stats
    snprintf(instance, sizeof(instance), "ns.%s.http.server", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("requests", pstat->server[PROTO_HTTP].requests);
    BUILD_NAME("responses", pstat->server[PROTO_HTTP].responses);
    BUILD_NAME("in_bytes", pstat->server[PROTO_HTTP].in_bytes);
    BUILD_NAME("out_bytes", pstat->server[PROTO_HTTP].out_bytes);
    BUILD_NAME("conns", pstat->server[PROTO_HTTP].conns);
    BUILD_NAME("active_conns", pstat->server[PROTO_HTTP].active_conns);
    BUILD_NAME("idle_conns", pstat->server[PROTO_HTTP].idle_conns);
    BUILD_NAME("resp_2xx", pstat->server[PROTO_HTTP].resp_2xx);
    BUILD_NAME("resp_3xx", pstat->server[PROTO_HTTP].resp_3xx);
    BUILD_NAME("resp_4xx", pstat->server[PROTO_HTTP].resp_4xx);
    BUILD_NAME("resp_5xx", pstat->server[PROTO_HTTP].resp_5xx);
    BUILD_NAME("nonbyterange_cnt", pstat->server[PROTO_HTTP].nonbyterange_cnt);
    BUILD_NAME("compress_cnt", pstat->server[PROTO_HTTP].compress_cnt);
    BUILD_NAME("compress_bytes_save", pstat->server[PROTO_HTTP].compress_bytes_save);
    BUILD_NAME("compress_bytes_overrun", pstat->server[PROTO_HTTP].compress_bytes_overrun);


    snprintf(instance, sizeof(instance), "ns.%s.rtsp.client", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("requests", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("responses", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("cache_hits", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("cache_miss", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("cache_partial", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("in_bytes", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("out_bytes", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("conns", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("active_conns", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("idle_conns", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("resp_2xx", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("resp_200", pstat->client[PROTO_RTSP].resp_200);
    BUILD_NAME("resp_206", pstat->client[PROTO_RTSP].resp_206);
    BUILD_NAME("resp_3xx", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("resp_302", pstat->client[PROTO_RTSP].resp_302);
    BUILD_NAME("resp_304", pstat->client[PROTO_RTSP].resp_304);
    BUILD_NAME("resp_4xx", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("resp_404", pstat->client[PROTO_RTSP].resp_404);
    BUILD_NAME("resp_5xx", pstat->client[PROTO_RTSP].requests);
    BUILD_NAME("out_bytes_origin", pstat->client[PROTO_RTSP].out_bytes_origin);
    BUILD_NAME("out_bytes_disk", pstat->client[PROTO_RTSP].out_bytes_disk);
    BUILD_NAME("out_bytes_ram", pstat->client[PROTO_RTSP].out_bytes_ram);
    BUILD_NAME("out_bytes_tunnel", pstat->client[PROTO_RTSP].out_bytes_tunnel);
    // Server stats
    snprintf(instance, sizeof(instance), "ns.%s.rtsp.server", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("requests", pstat->server[PROTO_RTSP].requests);
    BUILD_NAME("responses", pstat->server[PROTO_RTSP].requests);
    BUILD_NAME("in_bytes", pstat->server[PROTO_RTSP].requests);
    BUILD_NAME("out_bytes", pstat->server[PROTO_RTSP].requests);
    BUILD_NAME("conns", pstat->server[PROTO_RTSP].requests);
    BUILD_NAME("active_conns", pstat->server[PROTO_RTSP].requests);
    BUILD_NAME("idle_conns", pstat->server[PROTO_RTSP].requests);
    BUILD_NAME("resp_2xx", pstat->server[PROTO_RTSP].requests);
    BUILD_NAME("resp_3xx", pstat->server[PROTO_RTSP].requests);
    BUILD_NAME("resp_4xx", pstat->server[PROTO_RTSP].requests);
    BUILD_NAME("resp_5xx", pstat->server[PROTO_RTSP].requests);

    snprintf(instance, sizeof(instance), "ns.%s.cache.tier_1", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("block_size", pstat->dm_tier[DM_TIER_1].block_size);
    BUILD_NAME("block_used", pstat->dm_tier[DM_TIER_1].block_used);
    BUILD_NAME("page_size", pstat->dm_tier[DM_TIER_1].page_size);
    BUILD_NAME("page_used", pstat->dm_tier[DM_TIER_1].page_used);

    snprintf(instance, sizeof(instance), "ns.%s.cache.tier_2", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("block_size", pstat->dm_tier[DM_TIER_2].block_size);
    BUILD_NAME("block_used", pstat->dm_tier[DM_TIER_2].block_used);
    BUILD_NAME("page_size", pstat->dm_tier[DM_TIER_2].page_size);
    BUILD_NAME("page_used", pstat->dm_tier[DM_TIER_2].page_used);

    snprintf(instance, sizeof(instance), "ns.%s.cache.tier_3", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("block_size", pstat->dm_tier[DM_TIER_3].block_size);
    BUILD_NAME("block_used", pstat->dm_tier[DM_TIER_3].block_used);
    BUILD_NAME("page_size", pstat->dm_tier[DM_TIER_3].page_size);
    BUILD_NAME("page_used", pstat->dm_tier[DM_TIER_3].page_used);

    snprintf(instance, sizeof(instance), "ns.%s.urlfilter", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("acp_cnt", pstat->urif_stats.acp_cnt);
    BUILD_NAME("rej_cnt", pstat->urif_stats.rej_cnt);
    BUILD_NAME("bw", pstat->urif_stats.bw);
#undef BUILD_NAME

    glob_namespaces_changed++;

    return;
}

STATIC void unregister_namespace_counters(namespace_stat_t *pstat, 
					  const char *name)
{
    char instance[256];
    char statname[1024];

    UNUSED_ARGUMENT(pstat);

    statname[sizeof(statname) - 1] = '\0';

    /* unregister the old counter name *_gets */
    snprintf(statname, sizeof(statname), "glob_namespace_%s_gets", name);
    nkn_mon_delete(statname, NULL);

#define BUILD_NAME(cnt_name) { \
    snprintf(statname, sizeof(statname), "%s", cnt_name); \
    nkn_mon_delete(statname, instance); \
}

    snprintf(instance, sizeof(instance), "ns.%s", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("http.sessions");
    BUILD_NAME("rtsp.sessions");
    BUILD_NAME("tunnels");

    snprintf(instance, sizeof(instance), "ns.%s.http.client", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("requests");
    BUILD_NAME("responses");
    BUILD_NAME("cache_hits");
    BUILD_NAME("cache_miss");
    BUILD_NAME("cache_partial");
    BUILD_NAME("in_bytes");
    BUILD_NAME("out_bytes");
    BUILD_NAME("conns");
    BUILD_NAME("active_conns");
    BUILD_NAME("idle_conns");
    BUILD_NAME("resp_2xx");
    BUILD_NAME("resp_200");
    BUILD_NAME("resp_206");
    BUILD_NAME("resp_3xx");
    BUILD_NAME("resp_302");
    BUILD_NAME("resp_304");
    BUILD_NAME("resp_4xx");
    BUILD_NAME("resp_5xx");
    BUILD_NAME("resp_404");
    BUILD_NAME("out_bytes_origin");
    BUILD_NAME("out_bytes_disk");
    BUILD_NAME("out_bytes_ram");
    BUILD_NAME("out_bytes_tunnel");
    BUILD_NAME("compress_cnt");
    BUILD_NAME("compress_bytes_save");
    BUILD_NAME("compress_bytes_overrun");
    BUILD_NAME("expired_objs_delivered");
    // Server stats
    snprintf(instance, sizeof(instance), "ns.%s.http.server", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("requests");
    BUILD_NAME("responses");
    BUILD_NAME("in_bytes");
    BUILD_NAME("out_bytes");
    BUILD_NAME("conns");
    BUILD_NAME("active_conns");
    BUILD_NAME("idle_conns");
    BUILD_NAME("resp_2xx");
    BUILD_NAME("resp_3xx");
    BUILD_NAME("resp_4xx");
    BUILD_NAME("resp_5xx");
    BUILD_NAME("compress_cnt");
    BUILD_NAME("compress_bytes_save");
    BUILD_NAME("compress_bytes_overrun");


    snprintf(instance, sizeof(instance), "ns.%s.rtsp.client", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("requests");
    BUILD_NAME("responses");
    BUILD_NAME("cache_hits");
    BUILD_NAME("cache_miss");
    BUILD_NAME("cache_partial");
    BUILD_NAME("in_bytes");
    BUILD_NAME("out_bytes");
    BUILD_NAME("conns");
    BUILD_NAME("active_conns");
    BUILD_NAME("idle_conns");
    BUILD_NAME("resp_2xx");
    BUILD_NAME("resp_200");
    BUILD_NAME("resp_206");
    BUILD_NAME("resp_3xx");
    BUILD_NAME("resp_302");
    BUILD_NAME("resp_304");
    BUILD_NAME("resp_4xx");
    BUILD_NAME("resp_404");
    BUILD_NAME("resp_5xx");
    BUILD_NAME("out_bytes_origin");
    BUILD_NAME("out_bytes_disk");
    BUILD_NAME("out_bytes_ram");
    BUILD_NAME("out_bytes_tunnel");
    // Server stats
    snprintf(instance, sizeof(instance), "ns.%s.rtsp.server", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("requests");
    BUILD_NAME("responses");
    BUILD_NAME("in_bytes");
    BUILD_NAME("out_bytes");
    BUILD_NAME("conns");
    BUILD_NAME("active_conns");
    BUILD_NAME("idle_conns");
    BUILD_NAME("resp_2xx");
    BUILD_NAME("resp_3xx");
    BUILD_NAME("resp_4xx");
    BUILD_NAME("resp_5xx");

    snprintf(instance, sizeof(instance), "ns.%s.cache.tier_1", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("block_size");
    BUILD_NAME("block_used");
    BUILD_NAME("page_size");
    BUILD_NAME("page_used");

    snprintf(instance, sizeof(instance), "ns.%s.cache.tier_2", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("block_size");
    BUILD_NAME("block_used");
    BUILD_NAME("page_size");
    BUILD_NAME("page_used");

    snprintf(instance, sizeof(instance), "ns.%s.cache.tier_3", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("block_size");
    BUILD_NAME("block_used");
    BUILD_NAME("page_size");
    BUILD_NAME("page_used");

    snprintf(instance, sizeof(instance), "ns.%s.urlfilter", name);
    instance[sizeof(instance) - 1] = '\0';
    BUILD_NAME("filter_acp_count");
    BUILD_NAME("filter_rej_count");
    BUILD_NAME("filter_bw");
#undef BUILD_NAME

    glob_namespaces_changed++;
    return;
}

STATIC namespace_stat_t *get_namespace_stat_t(const char *name)
{
    namespace_stat_t *pstat;

    pthread_mutex_lock(&ns_stat_lock);
    for (pstat = namespace_stat_hdr.next; pstat != &namespace_stat_hdr; 
    	 pstat = pstat->next) {
	if (strcmp(pstat->name, name) == 0) {
	    pstat->refcnt++;
    	    pthread_mutex_unlock(&ns_stat_lock);
	    return pstat;
	}
    }

    pstat = new_namespace_stat_t(name);
    if (pstat) {
        NS_STAT_LINK_TAIL(pstat);

	/* As part of providing additional counters for 
	 * namespace, the original logic to register
	 * just the GET counter for namespace has been
	 * moved to a common registration function - 
	 * register_namespace_counters() - which calls the
	 * api - nkn_mon_add() - to register the counters
	 * for this namespace. Hence, any new counters added
	 * for this namespace should be added in the 
	 * register_namespace_counters() function only.
	 */
	register_namespace_counters(pstat, name);

    }
    pthread_mutex_unlock(&ns_stat_lock);
    return pstat;
}

STATIC void release_namespace_stat_t(namespace_stat_t *pstat)
{

    pthread_mutex_lock(&ns_stat_lock);
    if (pstat && (--pstat->refcnt == 0)) {
        NS_STAT_UNLINK(pstat);
    	pthread_mutex_unlock(&ns_stat_lock);

	/* As part of providing additional counters for 
	 * namespace, the original logic to unregister
	 * just the GET counter for namespace has been
	 * moved to a common unregistration function - 
	 * unregister_namespace_counters() - which calls the
	 * api - nkn_mon_delete() - to unregister the counters
	 * for this namespace. Hence, any counters deleted
	 * for this namespace should be added in the 
	 * unregister_namespace_counters() function only.
	 */
	unregister_namespace_counters(pstat, pstat->name);

    	delete_namespace_stat_t(pstat);
    } else {
    	pthread_mutex_unlock(&ns_stat_lock);
    }
}

STATIC namespace_stat_t *new_namespace_stat_t(const char *name)
{
    namespace_stat_t *pstat;

    pstat = (namespace_stat_t *)nkn_calloc_type(1, sizeof(namespace_stat_t), mod_ns_stat_t);
    if (pstat) {
    	pstat->name = nkn_strdup_type(name, mod_ns_stat_t_name);
        pstat->refcnt = 1;
    }
    return pstat;
}

STATIC void delete_namespace_stat_t(namespace_stat_t *pstat)
{
    if (pstat) {
        if (pstat->name) {
	    free(pstat->name);
	}
    	free(pstat);
    }
}

STATIC prefix_to_namespace_map_t *new_prefix_to_namespace_map_t(void)
{
    prefix_to_namespace_map_t *pfm;
    pfm = (prefix_to_namespace_map_t *)
    		nkn_calloc_type(1, sizeof(prefix_to_namespace_map_t),
				mod_ns_prefix_to_namespace_map_t);
    if (!pfm) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	goto err_exit;
    }
    pfm->alloced_entries = 1024;
    pfm->namespace_map = (int *)nkn_calloc_type(1, sizeof(int) * pfm->alloced_entries,
    					mod_ns_map_t);
    if (!pfm->namespace_map) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	goto err_exit;
    }
    return pfm;

err_exit:

    delete_prefix_to_namespace_map_t(pfm);
    return 0;
}

STATIC void delete_prefix_to_namespace_map_t(prefix_to_namespace_map_t *pfm)
{
    if (!pfm) {
      	return;
    }

    if (pfm->namespace_map) {
    	free(pfm->namespace_map);
    	pfm->namespace_map = 0;
    }
    free(pfm);
}

STATIC namespace_descriptor_t *new_namespace_descriptor_t(void)
{
    namespace_descriptor_t *nsd;
    nsd = (namespace_descriptor_t *)nkn_calloc_type(1, sizeof(namespace_descriptor_t),
    						mod_ns_descriptor_t);
    if (!nsd) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	goto err_exit;
    }
    nsd->magicno = NSD_MAGICNO;

    nsd->ht_hostnames = ht_base_nocase_hash;
    nsd->ht_hostnames.ht = nsd->hash_hostnames;
    nsd->ht_hostnames.size = sizeof(nsd->hash_hostnames)/sizeof(hash_entry_t);

    nsd->alloced_namespaces = 1024;
    nsd->namespaces = (namespace_config_t **)
    	nkn_calloc_type(1, sizeof(namespace_config_t *) * nsd->alloced_namespaces, mod_ns_config_t);
    if (!nsd->namespaces) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	goto err_exit;
    }

    nsd->alloced_prefix_maps = 1024;
    nsd->prefix_maps = (prefix_to_namespace_map_t **)
    	nkn_calloc_type(1, sizeof(prefix_to_namespace_map_t *) * 
		nsd->alloced_prefix_maps, mod_ns_prefix_maps_t);
    if (!nsd->alloced_prefix_maps) {
	DBG_LOG(MSG, MOD_NAMESPACE, "nkn_calloc_type() failed");
	goto err_exit;
    }

    nsd->ht_intercept = ht_base_nocase_hash;
    nsd->ht_intercept.ht = nsd->hash_intercept;
    nsd->ht_intercept.size = sizeof(nsd->hash_intercept)/sizeof(hash_entry_t);
    return nsd;

err_exit:

    delete_namespace_descriptor_t(nsd);
    return 0;
}

STATIC void delete_namespace_descriptor_t(namespace_descriptor_t *nsd)
{
    int nth;

    if (!nsd) {
    	return;
    }

    if (nsd->prefix_maps) {
    	for (nth = 0; nth < nsd->used_prefix_maps; nth++) {
    	    delete_prefix_to_namespace_map_t(nsd->prefix_maps[nth]);
    	    nsd->prefix_maps[nth] = 0;
	}
	free(nsd->prefix_maps);
	nsd->prefix_maps = 0;
    }

    if (nsd->namespaces) {
    	for (nth = 0; nth < nsd->used_namespaces; nth++) {
    	    delete_namespace_config_t(nsd->namespaces[nth]);
    	    nsd->namespaces[nth] = 0;
    	}
    	free(nsd->namespaces);
    	nsd->namespaces = 0;
    }
    (*nsd->ht_hostnames.dealloc_func)(&nsd->ht_hostnames);

    if (nsd->intercept) {
    	(*nsd->ht_intercept.dealloc_func)(&nsd->ht_intercept);

    	for (nth = 0; nth < nsd->num_intercept_handlers; nth++) {
	    if (nsd->intercept[nth]) {
	    	if (nsd->intercept[nth]->key) {
		    free((char *)nsd->intercept[nth]->key);
		    nsd->intercept[nth]->key = NULL;
		}
	    	free(nsd->intercept[nth]);
	    	nsd->intercept[nth] = NULL;
	    }
	}
	free(nsd->intercept);
    }
    nsd->magicno = -1;

    free(nsd);
}

STATIC void dump_namespace_select_method(const namespace_config_t *nsc, 
				    	 char *buf, int buflen)
{
    char *dest_ip_str;

    switch(nsc->select_method) {
    case NSM_URI_PREFIX:
    	snprintf(buf, buflen, "select=NSM_URI_PREFIX(%d) uri_prefix=%s ",
		 nsc->select_method, nsc->sm.up.uri_prefix);
	break;
    case NSM_URI_PREFIX_REGEX:
    	snprintf(buf, buflen, 
		 "select=NSM_URI_PREFIX_REGEX(%d) uri_prefix_regex=%s ",
		 nsc->select_method, nsc->sm.upr.uri_prefix_regex);
	break;
    case NSM_HEADER_VALUE:
    	snprintf(buf, buflen, "select=NSM_HEADER_VALUE(%d) name=%s value=%s ",
		 nsc->select_method, nsc->sm.hv.header_name, 
		 nsc->sm.hv.header_value);
	break;
    case NSM_HEADER_VALUE_REGEX:
    	snprintf(buf, buflen, "select=NSM_HEADER_VALUE_REGEX(%d) regex=%s ",
		 nsc->select_method, nsc->sm.hvr.header_name_value_regex);
	break;
    case NSM_QUERY_NAME_VALUE:
    	snprintf(buf, buflen, 
		 "select=NSM_QUERY_NAME_VALUE(%d) name=%s value=%s ",
		 nsc->select_method,
		 nsc->sm.qnv.query_string_nv_name,
		 nsc->sm.qnv.query_string_nv_value);
	break;
    case NSM_QUERY_NAME_VALUE_REGEX:
    	snprintf(buf, buflen, "select=NSM_QUERY_NAME_VALUE_REGEx(%d) regex=%s ",
		 nsc->select_method, nsc->sm.qnvr.query_string_regex);
 	break;
    case NSM_VIRTUAL_IP_PORT:
	dest_ip_str = alloca(INET_ADDRSTRLEN+1);
	if (inet_ntop(AF_INET, &nsc->sm.vip.virtual_host_ip, dest_ip_str, 
		      INET_ADDRSTRLEN+1)) {
		snprintf(buf, buflen, "select=NSM_VIRTUAL_IP_PORT(%d) "
			 "IP=%s Port=%d", nsc->select_method, dest_ip_str,
		         ntohl(nsc->sm.vip.virtual_host_port));
	}
	break;
    default:
    	snprintf(buf, buflen, "select=unknown(%d) ", nsc->select_method);
	break;
    }
}


STATIC int add_domain_hashtable(const char *hostname, int hostname_len,
				int nth, namespace_descriptor_t *nsd)
{

    prefix_to_namespace_map_t *pfm;
    int *t_nsm;
    prefix_to_namespace_map_t **t_ppfm;
    int prefix_index;
    int rv;
    char prtbuf[1024];
    char *hostname_regex;
    const char *namespace;

    hostname_regex = nsd->namespaces[nth]->hostname_regex;
    namespace = nsd->namespaces[nth]->namespace;

    rv = (*nsd->ht_hostnames.lookup_func)(&nsd->ht_hostnames, 
		hostname, hostname_len, &prefix_index);
    if (!rv) {
	// Hostname entry exists

	pfm = nsd->prefix_maps[prefix_index];
	if (pfm->used_entries >= pfm->alloced_entries) {
	    pfm->alloced_entries *= 2;
	    t_nsm = nkn_realloc_type(pfm->namespace_map,
					 sizeof(int) * pfm->alloced_entries,
					 mod_ns_prefix_to_namespace_map_t);
	    if (!t_nsm) {
		DBG_LOG(SEVERE, MOD_NAMESPACE, "realloc() failed");
		DBG_ERR(SEVERE, "realloc() failed");
		return 1;
	    }
	    pfm->namespace_map = t_nsm;
	}
	pfm->namespace_map[pfm->used_entries++] = nth;
    } else {
	// Hostname entry does not exist

	if (nsd->used_prefix_maps >= nsd->alloced_prefix_maps) {
	    nsd->alloced_prefix_maps *= 2;
	    t_ppfm = nkn_realloc_type(nsd->prefix_maps,
			sizeof(prefix_to_namespace_map_t *) *
				nsd->alloced_prefix_maps,
			mod_ns_prefix_to_namespace_map_t);
	    if (!t_ppfm) {
		DBG_LOG(SEVERE, MOD_NAMESPACE, "realloc() failed");
		DBG_ERR(SEVERE, "realloc() failed");
		return 2;
	    }
	}

	nsd->prefix_maps[nsd->used_prefix_maps] = 
	    		new_prefix_to_namespace_map_t();

	if (!nsd->prefix_maps[nsd->used_prefix_maps]->namespace_map) {
	    DBG_LOG(SEVERE, MOD_NAMESPACE, 
	    	    	"new_prefix_to_namespace_map_t() failed");
	    DBG_ERR(SEVERE, "new_prefix_to_namespace_map_t() failed");
	    return 3;
	}
	pfm = nsd->prefix_maps[nsd->used_prefix_maps];
	pfm->namespace_map[pfm->used_entries++] = nth;

	rv = (*nsd->ht_hostnames.add_func)(&nsd->ht_hostnames, hostname, 
	    		hostname_len, nsd->used_prefix_maps);
	nsd->used_prefix_maps++;
    }

    dump_namespace_select_method(nsd->namespaces[nth], prtbuf, 
				 sizeof(prtbuf));
    DBG_LOG(MSG, MOD_NAMESPACE,
		"Added hostname=%s hostname_regex=\"%s\" "
		"%s namespace=%s "
		"ns_index=%d prefix_index=%d",
		hostname, (hostname_regex ? hostname_regex : ""),
		prtbuf, namespace, nth, pfm->used_entries-1);

    return 0;

}

STATIC int init_hashtables(namespace_descriptor_t *nsd)
{
    int nth, dom_idx;
    int rv;
    const char *hostname;
    int hostname_len;

    for (nth = 0; nth < nsd->used_namespaces; nth++) {

	// check valid hostname and select method
	if ((nsd->namespaces[nth]->hostname_cnt == 0 &&
	    nsd->namespaces[nth]->hostname_regex == NULL) ||
	    ((nsd->namespaces[nth]->select_method == NSM_UNDEFINED) &&
	    (nsd->namespaces[nth]->rtsp_select_method == NSM_UNDEFINED))) {
		continue;
	}

	if (nsd->namespaces[nth]->hostname_cnt) {	
		for (dom_idx = 0; dom_idx < nsd->namespaces[nth]->hostname_cnt ;
			 ++dom_idx) {
			if (nsd->namespaces[nth]->hostname[dom_idx]) {
    	    			hostname = 
				nsd->namespaces[nth]->hostname[dom_idx];
	    			hostname_len = strlen(hostname);
			} else {
		    		DBG_LOG(SEVERE, MOD_NAMESPACE, 
			    	"Bad namespace_descriptor_t nth=%d domain index=%d",
				nth, dom_idx);
		    		return 11;
			}
			if (!hostname || hostname_len == 0)
				continue;
	    		rv = add_domain_hashtable(hostname,
						  hostname_len, nth, nsd); 
			if (rv) {
				return rv;
			}
		}
	} else if (nsd->namespaces[nth]->hostname_regex) {
	    nsd->namespaces[nth]->regex_int_hostname = 
	    	nkn_calloc_type(1, HOSTNAME_REGEX_FMT_MAXLEN, 
				mod_ns_regex_int_hostname);
	    if (!nsd->namespaces[nth]->regex_int_hostname) {
		DBG_LOG(SEVERE, MOD_NAMESPACE, "malloc() failed");
		DBG_ERR(SEVERE, "malloc() failed");
		return 10;
	    }
	    snprintf(nsd->namespaces[nth]->regex_int_hostname, 
	    	     HOSTNAME_REGEX_FMT_MAXLEN-1, HOSTNAME_REGEX_FMT,
	    	     nsd->hostname_regex_namespaces);

	    hostname = nsd->namespaces[nth]->regex_int_hostname;
	    hostname_len = strlen(hostname);
	    nsd->hostname_regex_namespaces++;
	    if (!hostname || hostname_len == 0)
		continue;

	    rv = add_domain_hashtable(hostname,
				      hostname_len, nth, nsd); 
	    if (rv) {
		return rv;
	    }
	} else {
	    DBG_LOG(SEVERE, MOD_NAMESPACE, 
	    	    "Bad namespace_descriptor_t nth=%d", nth);
	    DBG_ERR(SEVERE, "Bad namespace_descriptor_t nth=%d", nth);
	    return 11;
	}
    }
    return 0;
}

STATIC int init_intercept_handler(namespace_descriptor_t *nsd)
{
    int n;
    int cix;
    namespace_config_t *nsc;
    cluster_descriptor_t *cd;
    request_intercept_handler_t *ih;
    char key[1024];
    int size;
    int rv;
    int ret_index;

    nsd->intercept = nkn_calloc_type(1, 
    			sizeof(request_intercept_handler_t *) * 
			MAX(nsd->used_namespaces, 1),
			mod_nsd_intercept_handler_t);

    if (!nsd->intercept) {
	DBG_LOG(MSG, MOD_NAMESPACE, "mod_nsd_intercept_handler_t alloc failed");
	return 1;
    }

    for (n = 0; n < nsd->used_namespaces; n++) {
    	nsc = nsd->namespaces[n];
	if (nsc->cluster_config && nsc->cluster_config->num_cld) {
	    for (cix = 0; cix < nsc->cluster_config->num_cld; cix++) {
    		ih = nkn_calloc_type(1, sizeof(request_intercept_handler_t),
				     mod_nsd_intercept_handler_t);
		if (!ih) {
		    DBG_LOG(MSG, MOD_NAMESPACE, 
			    "mod_nsd_intercept_handler_t alloc failed");
		    return 2;
		}
		cd = nsc->cluster_config->cld[cix];

	    	switch(cd->type) {
		case CLT_CH_REDIRECT:
		    ih->attr = &cd->u.ch_redirect.intercept_attr;
		    ih->rd_attr = &cd->u.ch_redirect.redir_attr;
		    ih->ret_code = INTERCEPT_REDIRECT;
		    ih->namespace_index = n;
		    ih->cluster_index = cix;
		    break;

		case CLT_CH_REDIRECT_LB:
		    ih->attr = &cd->u.ch_redirect_lb.intercept_attr;
		    ih->rd_attr = &cd->u.ch_redirect_lb.redir_attr;
		    ih->ret_code = INTERCEPT_REDIRECT;
		    ih->namespace_index = n;
		    ih->cluster_index = cix;
		    break;

		case CLT_CH_PROXY:
		    ih->attr = &cd->u.ch_proxy.intercept_attr;
		    ih->rd_attr = &cd->u.ch_proxy.redir_attr;
		    ih->ret_code = INTERCEPT_PROXY;
		    ih->namespace_index = n;
		    ih->cluster_index = cix;
		    break;

		case CLT_CH_PROXY_LB:
		    ih->attr = &cd->u.ch_proxy_lb.intercept_attr;
		    ih->rd_attr = &cd->u.ch_proxy_lb.redir_attr;
		    ih->ret_code = INTERCEPT_PROXY;
		    ih->namespace_index = n;
		    ih->cluster_index = cix;
		    break;

		default:
		    free(ih);
		    ih = NULL;
		    break;
		}

		if (ih) {
		    nsd->intercept[nsd->num_intercept_handlers++] = ih;

		    if (!ih->attr->ip || !ih->attr->port) {
		    	// Wildcard
			size = snprintf(key, sizeof(key), 
				      	WILDCARD_INTERCEPT_KEY_FMT,
				      	nsd->num_wildcard_intercept_handlers++);
		    } else if ((ih->attr->ip == L7_NOT_INTERCEPT_IP) &&
		    		(ih->attr->ip == L7_NOT_INTERCEPT_PORT)) {
			// Client Request => namespace case, no hash entry req
			continue;
		    } else {
			size = snprintf(key, sizeof(key), 
				      	INTERCEPT_KEY_FMT,
				      	ih->attr->ip, ih->attr->port);
		    }

		    if (size >= (int)sizeof(key)) {
		    	key[sizeof(key)-1] = '\0';
			size = sizeof(key)-1;
		    }
		    ih->key = nkn_strdup_type(key, mod_ns_intercept_key);

		    rv = (*nsd->ht_intercept.lookup_func)(&nsd->ht_intercept,
						    ih->key, size, &ret_index);
		    if (rv) {
		    	rv = (*nsd->ht_intercept.add_func)(&nsd->ht_intercept, 
						ih->key, size, 
						nsd->num_intercept_handlers-1);
			if (rv) {
			    DBG_LOG(MSG, MOD_NAMESPACE, 
				    "add_func() failed, rv=%d key=%s", rv, key);
			    return 3;
			}
		    } else  { // Exists, ignore it
		    	DBG_LOG(MSG, MOD_NAMESPACE, 
				"Duplicate entry [%s], ignore it", key);
		    }

		    break; // Only 1 allowed per namespace
		}
	    }
	}
    }

    return 0;
}

STATIC const 
namespace_config_t *int_namespace_to_config(namespace_token_t token,
					namespace_descriptor_t **ppnsd,
					int return_locked,
					int *delflag)
{
    namespace_config_t *nsc;
    namespace_descriptor_t *nsd;
    *delflag = 0;

    NKN_RWLOCK_RLOCK(&ns_list_lock);
    if ((token.u.token_data.gen != 
    		NS_TOKEN_GENERATOR.u.token_data.gen)) {
	DBG_LOG(MSG, MOD_NAMESPACE,
		"Invalid token incarnation, gen=%u current gen=%u",
		token.u.token_data.gen, NS_TOKEN_GENERATOR.u.token_data.gen);
    	nsc = 0;
	goto exit;
    }

    if (CURRENT_NSD) {
    	if ((NS_TOKEN_VAL(token) >= NS_TOKEN_VAL(CURRENT_NSD->token_start)) && 
		(NS_TOKEN_VAL(token) <= NS_TOKEN_VAL(CURRENT_NSD->token_end))) {
	    nsc = CURRENT_NSD->namespaces[NS_TOKEN_VAL(token) -
	    			  NS_TOKEN_VAL(CURRENT_NSD->token_start)];
	    if (ppnsd) {
	    	*ppnsd = CURRENT_NSD;
		*delflag = CURRENT_NSD->flags & NSD_FLAGS_DELETE;
	    }
	} else {
	    nsc = 0;
	    nsd = NSD_HEAD.next;

	    while (nsd != &NSD_HEAD) {
	    	if ((NS_TOKEN_VAL(token) >= NS_TOKEN_VAL(nsd->token_start)) &&
			(NS_TOKEN_VAL(token) <= NS_TOKEN_VAL(nsd->token_end))) {
	    	    nsc = nsd->namespaces[NS_TOKEN_VAL(token) - 
		    			  NS_TOKEN_VAL(nsd->token_start)];
		    if (ppnsd) {
		    	*ppnsd = nsd;
			*delflag = nsd->flags & NSD_FLAGS_DELETE;
		    }
		    break;
		}
	    	nsd = nsd->next;
	    }
	}
    } else {
    	nsc = 0;
    }
exit:
    if (!return_locked) {
	NKN_RWLOCK_UNLOCK(&ns_list_lock);
    }

    return nsc;
}

STATIC void process_namespace_update(void)
{
    int rv;
    int last_ns_config_update;
    struct timespec now;
    struct timespec last_update_time;
    int64_t msecs;

    clock_gettime(CLOCK_MONOTONIC, &now);

    pthread_mutex_lock(&ns_config_lock);
    if ((last_ns_config_update = NS_CONFIG_UPDATE) != 0) {
    	last_update_time = LAST_NS_CONFIG_UPDATE_TIME;
	pthread_mutex_unlock(&ns_config_lock);

	// Space updates at CONFIG_UPDATE_INTERVAL_SECS to
	// prevent update churn.
 	msecs = nkn_timespec_diff_msecs(&last_update_time, &now);
	if ((msecs / MILLI_SECS_PER_SEC) < CONFIG_UPDATE_INTERVAL_SECS) {
	    return;
	}

	pthread_mutex_lock(&ns_update_lock);

	pthread_mutex_lock(&ns_config_lock);
    	clock_gettime(CLOCK_MONOTONIC, &now);
 	msecs = nkn_timespec_diff_msecs(&LAST_NS_CONFIG_UPDATE_TIME, &now);

	if ((NS_CONFIG_UPDATE >= last_ns_config_update) &&
	    (((msecs / MILLI_SECS_PER_SEC) >= CONFIG_UPDATE_INTERVAL_SECS))) {
	    last_ns_config_update = NS_CONFIG_UPDATE;
	    pthread_mutex_unlock(&ns_config_lock);

	    do {
		rv = update_namespace_data(0);
		if (!rv) {
		    DBG_LOG(MSG, MOD_NAMESPACE, 
			    "Namespace configuration updated, pending=%d",
			    NSD_HEAD_ELEMENTS-1);
		} else {
		    DBG_LOG(MSG, MOD_NAMESPACE, 
			    "update_namespace_data() failed, rv=%d pending=%d",
			    rv, NSD_HEAD_ELEMENTS-1);
		}

		pthread_mutex_lock(&ns_config_lock);
		clock_gettime(CLOCK_MONOTONIC, &LAST_NS_CONFIG_UPDATE_TIME);
		if (last_ns_config_update == NS_CONFIG_UPDATE) {
		    NS_CONFIG_UPDATE = 0;
		    pthread_mutex_unlock(&ns_config_lock);
		    break;
		} else {
		    last_ns_config_update = NS_CONFIG_UPDATE;
		    // Note: Avoid immediate update, defer to next interval
		    pthread_mutex_unlock(&ns_config_lock);
		    break;
		}
		pthread_mutex_unlock(&ns_config_lock);
	    } while (1);

	} else {
	    pthread_mutex_unlock(&ns_config_lock);
	}

	pthread_mutex_unlock(&ns_update_lock);
    } else {
	pthread_mutex_unlock(&ns_config_lock);
    }
}

STATIC namespace_token_t nsuid_to_nstoken(const char *p_nsuid)
{
    namespace_token_t ns_token;
    ns_token.u.token = 0;
    int pfix;
    int nsix;
    prefix_to_namespace_map_t *pfxmap;
    namespace_config_t *nsc;

    NKN_RWLOCK_RLOCK(&ns_list_lock);
    if (CURRENT_NSD) {
    	for (pfix = 0; pfix < CURRENT_NSD->used_prefix_maps; pfix++) {
	    pfxmap = CURRENT_NSD->prefix_maps[pfix];

    	    for (nsix = 0; nsix < pfxmap->used_entries; nsix++) {
		nsc = CURRENT_NSD->namespaces[pfxmap->namespace_map[nsix]];
		if (strcmp(p_nsuid, nsc->ns_uid) == 0) {
	    	    ns_token = CURRENT_NSD->token_start;
	    	    ns_token.u.token_data.val += pfxmap->namespace_map[nsix];
		    break;
		}
	    }
	}
    }
    NKN_RWLOCK_UNLOCK(&ns_list_lock);

    return ns_token;
}

/*
 * Given two namespace_descriptor_t(s), determine which "needle" NFS
 * entries no longer exist in "haystack".
 *
 * Return the number of deleted entries along with the corresponding 
 * namespace_token_t.
 * Caller must free() returned deleted_token_list.
 */
STATIC int compute_deleted_NFS_entries(const namespace_descriptor_t *needle, 
			    const namespace_descriptor_t *haystack,
			    namespace_token_t **deleted_token_list)
{
    int n_needle;
    int n_haystack;
    namespace_config_t *needle_cfg;
    namespace_config_t *haystack_cfg;
    int exists;
    int deleted_tokens = 0;
    namespace_token_t *token_list;
    namespace_token_t *tmp_token_list;
    int token_list_size;

    token_list_size = 128;
    token_list = nkn_malloc_type(sizeof(namespace_token_t) * token_list_size,
				 mod_ns_comp_del_nfs_entries);
    if (!token_list) {
    	DBG_LOG(MSG, MOD_NAMESPACE, "malloc() failed");
	goto err_exit;
    }

    for (n_needle = 0; n_needle < needle->used_namespaces; n_needle++) {
    	needle_cfg = needle->namespaces[n_needle];
    	if ((needle_cfg->http_config->origin.select_method == OSS_NFS_CONFIG) ||
    	   (needle_cfg->http_config->origin.select_method == 
	   					OSS_NFS_SERVER_MAP)) {
	    exists = 0;
	    for (n_haystack = 0; n_haystack < haystack->used_namespaces;
	    						n_haystack++) {
	     	haystack_cfg = haystack->namespaces[n_haystack];
    		if ((haystack_cfg->http_config->origin.select_method != 
						OSS_NFS_CONFIG) &&
    		(haystack_cfg->http_config->origin.select_method != 
						OSS_NFS_SERVER_MAP)) {
		    continue;
		}
		if ((needle_cfg->ns_uid_strlen == haystack_cfg->ns_uid_strlen)
		    && (strncmp(needle_cfg->ns_uid, haystack_cfg->ns_uid,
		    	        needle_cfg->ns_uid_strlen) == 0)) {
		    exists = 1;
		    break;
		}
	    }
	    if (!exists) {
	        if (deleted_tokens >= token_list_size) {
		    token_list_size *= 2;
    		    tmp_token_list =
		    	nkn_realloc_type(token_list,
				sizeof(namespace_token_t) * token_list_size,
				mod_ns_comp_del_nfs_entries);
		    if (!tmp_token_list) {
    			DBG_LOG(MSG, MOD_NAMESPACE, "realloc() failed");
			goto err_exit;
		    }
		    token_list = tmp_token_list;
		}
	    	token_list[deleted_tokens] = needle->token_start;
	    	token_list[deleted_tokens].u.token_data.val += n_needle;
		deleted_tokens++;
	    }
	}
    }

    if (deleted_tokens) {
    	*deleted_token_list = token_list;
    } else {
    	free(token_list);
    	*deleted_token_list = 0;
    }
    return deleted_tokens;

err_exit:

    if (token_list) {
    	free(token_list);
    }
    *deleted_token_list = 0;
    return 0;
}

STATIC int notify_nfs(namespace_token_t *deleted_nfs_token_list, 
		      int deleted_nfs_namespaces)
{
    int n;

    for (n = 0; n < deleted_nfs_namespaces; n++) {
    	DBG_LOG(MSG, MOD_NAMESPACE, "Deleting NFS token: gen=%u val=%u",
		deleted_nfs_token_list[n].u.token_data.gen,
		deleted_nfs_token_list[n].u.token_data.val);
    }
    NFS_config_delete(deleted_nfs_token_list, deleted_nfs_namespaces);
    return 0;
}

/* 
 * This function is used to validate the namespace policy 
 * during ingest. Based on the result, decision will be made
 * whether to ingest the object or not
 */
int
nkn_is_object_cache_worthy(nkn_attr_t *attr, namespace_token_t ns_token,
			    int flag)
{
    int				object_has_min_expire_time = 0;
    int				object_size_cacheable;
    mime_header_t		hdr;
    int				cache_object;
    int				rv = 0;
    const namespace_config_t	*nsc;
    time_t			object_expire_time;
    time_t			object_corrected_initial_age;
    const char 			*data;
    int 			data_len;
    u_int32_t 			attrs;
    int 			hdr_cnt;
    int				policy_cache = 0;
    int				s_maxage_ignore = 0;

    if(!attr || !ns_token.u.token)
	return NKN_MM_OBJ_NOT_CACHE_WORTHY;
    init_http_header(&hdr, 0, 0);
    rv = nkn_attr2http_header(attr, 0, &hdr);
    if (!rv) {
	nsc = namespace_to_config(ns_token);
	if(!nsc) {
	    DBG_LOG(WARNING, MOD_NAMESPACE, "No namespace data.");
	    shutdown_http_header(&hdr);
	    return NKN_MM_OBJ_NOT_CACHE_WORTHY;
	}

	if(!nsc->http_config) {
	    DBG_LOG(WARNING, MOD_NAMESPACE, "No origin server config data.");
	    release_namespace_token_t(ns_token);
	    shutdown_http_header(&hdr);
	    return NKN_MM_OBJ_NOT_CACHE_WORTHY;
	}
	if (nsc->http_config->policies.ignore_s_maxage) {
		s_maxage_ignore = 1;
	}
	cache_object = compute_object_expiration(&hdr,
				s_maxage_ignore,
				nkn_cur_ts,
				nkn_cur_ts,
				nkn_cur_ts,
				&object_corrected_initial_age,
				&object_expire_time);

	// Check policy engine cache header
	rv = mime_hdr_get_known(&hdr, MIME_HDR_X_NKN_CACHE_POLICY, &data, 
			&data_len, &attrs, &hdr_cnt);
	if (!rv) {
	    if (strncmp(data, "cache", data_len) == 0) {
		cache_object = 1;
		policy_cache = 1;
	    }
	}

	if (attr->cache_expiry <= nkn_cur_ts && (policy_cache == 0)) {
	    DBG_LOG(MSG, MOD_NAMESPACE,
			"Object already expired, caching disabled ");
	    glob_om_ingest_check_nocache_already_expired++;
	    cache_object = 0;
	}

	if (cache_object &&
	    (is_known_header_present(&hdr, MIME_HDR_SET_COOKIE) || 
	     is_known_header_present(&hdr, MIME_HDR_SET_COOKIE2)) &&
	    !nsc->http_config->policies.is_set_cookie_cacheable &&
	    (policy_cache == 0)) {
		DBG_LOG(MSG, MOD_NAMESPACE,
			"Set-Cookie present, caching disabled ");
	    glob_om_ingest_check_nocache_set_cookie++;
	    cache_object = 0;
	}

	if ((nsc->http_config->policies.cache_no_cache == NO_CACHE_FOLLOW) ||
	    (nsc->http_config->policies.cache_no_cache == NO_CACHE_OVERRIDE)) {
	    cache_object = 1;
	}

	if(nsc->http_config->policies.store_cache_min_age) {
	    object_has_min_expire_time  = ((attr->cache_expiry - nkn_cur_ts) >
			    nsc->http_config->policies.store_cache_min_age);
	} else {
	    /* Special case where the age-theshold is set to 0
	     * Need to cache the object
	     */
	    cache_object = 1;
	    object_has_min_expire_time = 1;
	}

	if(!object_has_min_expire_time)
	    glob_om_ingest_check_nocache_already_expired ++;

	if((flag & NAMESPACE_WORTHY_CHECK_INGNORE_SIZE) ||
	    nkn_attr_is_streaming(attr))
	    object_size_cacheable = 1;
	else
	    object_size_cacheable = (attr->content_length >=
			(unsigned)nsc->http_config->policies.store_cache_min_size);

	if(!object_size_cacheable)
	    glob_om_ingest_check_object_size_hit_cnt ++;

	if (CHECK_TR_RES_OVERRIDE(nsc->tunnel_res_override, NKN_TR_RES_OBJ_EXPIRED)) {
		DBG_LOG(MSG, MOD_NAMESPACE,
				"Tunnel override configuration set to cache objects expired."
				"Enabling cache action for this object.");
		cache_object = 1; //Tunnel override has been set. Cache the object.
	}

	if (cache_object &&
	    object_has_min_expire_time && object_size_cacheable) {
	    release_namespace_token_t(ns_token);
	    shutdown_http_header(&hdr);
	    return 0;
	}
	release_namespace_token_t(ns_token);
    }
    shutdown_http_header(&hdr);
    return NKN_MM_OBJ_NOT_CACHE_WORTHY;
}

/*
 * nkn_get_namespace_auto_pin_config() - Get the attribute
 *     cache_pin_config->cache_auto_pin
 *
 * Input: ns_token - Namespace token to get the value from.
 *
 * Returns: uint32_t - Value of the attribute cache_pin_config->cache_auto_pin
 */
uint32_t
nkn_get_namespace_auto_pin_config(namespace_token_t ns_token)
{
    const namespace_config_t	*nsc;
    uint32_t pin_object = 0;
    nsc = namespace_to_config(ns_token);
    if(nsc) {
	pin_object = nsc->cache_pin_config->cache_auto_pin;
	release_namespace_token_t(ns_token);
    }
    return pin_object;
}

/*
 * nkn_get_namespace_cache_ingest_size_threshold() - Get the attribute
 *     cache_ingest.size_threshold.
 *
 * Input: ns_token - Namespace token to get the value from.
 *
 * Returns: uint32_t - Value of the attribute cache_ingest.size_threshold
 */
uint32_t
nkn_get_namespace_cache_ingest_size_threshold(namespace_token_t ns_token)
{
    const namespace_config_t	*nsc;
    uint32_t size_threshold = 0;
    nsc = namespace_to_config(ns_token);
    if(nsc) {
	size_threshold = nsc->http_config->policies.cache_ingest.size_threshold;
	release_namespace_token_t(ns_token);
    }
    return size_threshold;
}

/*
 * nkn_get_namespace_cache_ingest_hotness_threshold() - Get the attribute
 *     cache_ingest.hotness_threshold.
 *
 * Input: ns_token - Namespace token to get the value from.
 *
 * Returns: int - Value of the attribute cache_ingest.hotness_threshold
 */
uint32_t
nkn_get_namespace_cache_ingest_hotness_threshold(namespace_token_t ns_token)
{
    const namespace_config_t	*nsc;
    uint32_t hotness_threshold = 0;
    nsc = namespace_to_config(ns_token);
    if(nsc) {
	hotness_threshold =
	    nsc->http_config->policies.cache_ingest.hotness_threshold;
	release_namespace_token_t(ns_token);
    }
    return hotness_threshold;
}

/*
 * nkn_get_namespace_ingest_policy() - Get the attribute
 *     ingest_policy
 *
 * Input: ns_token - Namespace token to get the value from.
 *
 * Returns: ingest_policy_t - Value of the attribute ingest_policy
 */
ingest_policy_t
nkn_get_namespace_ingest_policy(namespace_token_t ns_token)
{
    const namespace_config_t	*nsc;
    ingest_policy_t		ingest_policy = INGEST_POLICY_LIFO;
    nsc = namespace_to_config(ns_token);
    if(!nsc)
	return ingest_policy;
    ingest_policy = nsc->http_config->policies.ingest_policy;
    release_namespace_token_t(ns_token);
    return ingest_policy;
}

/*
 * nkn_get_namespace_client_driven_aggr_threshold() - Get the attribute
 *     client_driven_agg_threshold
 *
 * Input: ns_token - Namespace token to get the value from.
 *
 * Returns: int - Value of the attribute client_driven_agg_threshold
 */
int
nkn_get_namespace_client_driven_aggr_threshold(namespace_token_t ns_token)
{
    const namespace_config_t	*nsc;
    int ret_val = 0;

    nsc = namespace_to_config(ns_token);
    if(!nsc)
	return ret_val;

    if(nsc->http_config->policies.cache_fill == INGEST_CLIENT_DRIVEN) {
	ret_val = nsc->http_config->policies.client_driven_agg_threshold;
    }
    release_namespace_token_t(ns_token);
    return ret_val;
}

/*
 * nkn_get_proxy_mode_ingest_policy() - Get the attribute ingest_policy
 *
 * Input: ns_token - Namespace token to get the value from.
 *
 * Returns: ingest_policy_t - Value of the attribute ingest_policy
 */
ingest_policy_t
nkn_get_proxy_mode_ingest_policy(namespace_token_t ns_token)
{
    const namespace_config_t	*nsc;
    ingest_policy_t		ingest_policy = INGEST_POLICY_LIFO;
    nsc = namespace_to_config(ns_token);
    if(!nsc)
	return ingest_policy;
    switch(nsc->http_config->origin.select_method) {
	case OSS_HTTP_ABS_URL:
	case OSS_HTTP_FOLLOW_HEADER:
	case OSS_HTTP_DEST_IP:
	    ingest_policy = INGEST_POLICY_LIFO;
	    break;
	default:
	    ingest_policy = INGEST_POLICY_FIFO;
	    break;
    }
    release_namespace_token_t(ns_token);
    return ingest_policy;
}

/*
 * checkfor_namespace_match() -- Apply namespace selection criteria
 * Returns: 0 => No match; 1 => Match
 */
STATIC
int checkfor_namespace_match(const namespace_config_t *nsc, 
			     const mime_header_t *hdr,
			     const char *uristr,
			     uint32_t ns_thread)
{
    int retval = 0;
    int rv;
    char errorbuf[1024];
    const char *data = 0;
    int datalen = 0;
    u_int32_t attrs = 0;
    int hdr_cnt;

    switch(nsc->select_method) {
    case NSM_URI_PREFIX:
    {
        char *p;

        p = strstr(uristr, nsc->sm.up.uri_prefix);
        if (p == uristr) {
            retval = 1; // Match
        }
        break;
    }
    case NSM_URI_PREFIX_REGEX:
    {
	if (ns_thread >= nsc->sm.upr.uri_prefix_regex_tot_cnt) {
		ns_thread = 0;	
	}
        rv = nkn_regexec(nkn_regaindex(nsc->sm.upr.uri_prefix_regex_ctx,
			 ns_thread)
		         , (char *)uristr, 
    		         errorbuf, sizeof(errorbuf));
        if (!rv) {
            retval = 1; // Match
        } 
	
        break;
    }
    case NSM_HEADER_VALUE:
    {
        int namelen = strlen(nsc->sm.hv.header_name);
        int vallen = strlen(nsc->sm.hv.header_value);
        int token;
        /* Check whether this is a known or unknown header */
        rv = mime_hdr_strtohdr(hdr->protocol, nsc->sm.hv.header_name, namelen,
    		           &token);
        if (!rv) {
            /* Known header specified in the namespace config */
            rv = mime_hdr_get_known(hdr, token, &data, &datalen,
        		                &attrs, &hdr_cnt);
            if(rv) {
        	    DBG_LOG(MSG, MOD_NAMESPACE, 
        		    "failed to lookup known header, rv=%d", rv);
            }
        } 
        else {
            /* Unknown header */
            rv = mime_hdr_get_unknown(hdr, nsc->sm.hv.header_name, namelen,
        		                  &data, &datalen, &attrs);
            if (rv) {
        	    DBG_LOG(MSG, MOD_NAMESPACE, 
        	    	    "failed to lookup unknown header, rv=%d", rv);
            }
        }
	if (!rv) {
	    if ((datalen == vallen) && 
	        (strncasecmp(data, nsc->sm.hv.header_value, datalen) == 0)) {
	 
	   	retval = 1; // Match
	    }
	    else if ((vallen == 1) && (*nsc->sm.hv.header_value == '*')) {
	        retval = 1; // Match for any
	    }
	}
    	break;
    }
    case NSM_HEADER_VALUE_REGEX:
    {
	/* NOTE:
	 * Case where multiple header values are present, 
	 * such as like this
	 * 	Via: proxy1:80, proxy2:8080, proxyN:9090
	 * we dont handle these.
	 */
#define DEFAULT_OUTBUF_BUFSIZE (1*1024)
	char default_outbuf[DEFAULT_OUTBUF_BUFSIZE];
	char *outbuf = default_outbuf;
	char *tmp_outbuf = 0;
	int outbufsize = DEFAULT_OUTBUF_BUFSIZE;
	int tmp_outbufsize = 0;
	int bytesused = 0;
	int cnt_known_hdrs, cnt_unknown_hdrs, cnt_nv_hdrs, cnt_qrystr_hdrs;
	int i;
	const char *value = 0;
	int vallen;
#define OUTBUF(_data, _datalen, _bytesused) { \
	if (((_datalen)+(_bytesused)) >= outbufsize) { \
		tmp_outbuf = outbuf; \
		tmp_outbufsize = outbufsize; \
		if (outbufsize == DEFAULT_OUTBUF_BUFSIZE) { \
			outbuf = 0; \
		} \
		outbufsize = MAX(outbufsize * 2, (_datalen) + (_bytesused) + 1); \
		outbuf = nkn_realloc_type(outbuf, outbufsize, mod_ns_header_name); \
		if(outbuf) { \
			if (tmp_outbufsize == DEFAULT_OUTBUF_BUFSIZE) { \
				memcpy(outbuf, tmp_outbuf, (_bytesused)); \
			} \
			memcpy((void*)&outbuf[(_bytesused)], (_data), (_datalen)); \
			(_bytesused) += (_datalen); \
		} else { \
			outbuf = tmp_outbuf; \
			outbufsize = tmp_outbufsize; \
		} \
	} else { \
		memcpy((void *)&outbuf[(_bytesused)], (_data), (_datalen)); \
		(_bytesused) += (_datalen); \
	} \
}
	/* Get the number of headers */
	rv = get_header_counts(hdr, &cnt_known_hdrs, &cnt_unknown_hdrs, 
	    	           &cnt_nv_hdrs, &cnt_qrystr_hdrs);
	if (!rv) {

	    if (ns_thread >= nsc->sm.hvr.header_name_value_regex_tot_cnt) {
		ns_thread = 0;	
	    }
	    for(i = 0; (i < MIME_HDR_MAX_DEFS) && (cnt_known_hdrs); i++) {
	        rv = is_known_header_present(hdr, i);
	        if (rv) {
	            /* this header is present */
	            bytesused = 0;
	            rv = get_known_header(hdr, i, &data, &datalen, &attrs, 
	    		    	      &hdr_cnt);
	            if (!rv) {
	    	    /* We dont want to keep looping.. Only check for 
	    	     * known number of headers and break
	    	     */
	    	    cnt_known_hdrs--;
	    	    /* Reconstruct back header */
	    	    OUTBUF(http_known_headers[i].name, 
	    		   http_known_headers[i].namelen, bytesused);
	    	    OUTBUF(": ", 2, bytesused);
	    	    OUTBUF(data, datalen, bytesused);
	    	    outbuf[bytesused] = '\0';

	    	    DBG_LOG(MSG3, MOD_NAMESPACE, 
	    		    "Checking header [%s]", outbuf);
	    	    /* Check if we can match against the regex */
	    	    rv = nkn_regexec(nkn_regaindex(
				     nsc->sm.hvr.header_name_value_regex_ctx,
				     ns_thread), 
	    			     outbuf, errorbuf, sizeof(errorbuf));
	    	    if (outbuf != default_outbuf) {
	    	        /* perhaps this buffer was re-alloced?? */
	    	        free(outbuf);

	    	        /* Reset back to use the stack buffer.
	    	         * NOTE: This may be a performance hit, since
	    	         * we keep alloc'ing/free'ing the buffer. */
	    	        outbuf = default_outbuf;
	    	    }
	    	    if (!rv) {
	    	        retval = 1; // Match
	    	        break;
	    	    }
	            }
	        }
	    }
	    /* If we find a match, then break out of case.
	     * No need to search for headers again.
	     */
	    if (retval) {
	        break;
	    }
	    /* No match in the known headers. Try unknown headers */
	    for(i = 0; i < cnt_unknown_hdrs; i++) {
	        bytesused = 0;
	        rv = mime_hdr_get_nth_unknown(hdr, i, &data, &datalen,
	    			      &value, &vallen, &attrs);
	        if (!rv) {
	            OUTBUF(data, datalen, bytesused);
	            OUTBUF(": ", 2, bytesused);
	            OUTBUF(value, vallen, bytesused);
	            outbuf[bytesused] = '\0';

	            DBG_LOG(MSG3, MOD_NAMESPACE,
			    "Checking header [%s]", outbuf);
	            /* Check if we can match against the regex */
	            rv = nkn_regexec(nkn_regaindex(
				     nsc->sm.hvr.header_name_value_regex_ctx,
				     ns_thread), 
				     outbuf, errorbuf, sizeof(errorbuf));
	            if (outbuf != default_outbuf) {
	    	    /* perhaps this buffer was re-alloced?? */
	    	    free(outbuf);
	    	    /* Reset back to use the stack buffer.
	    	     * NOTE: This may be a performance hit, since
	    	     * we keep alloc'ing/free'ing the buffer. */
	    	    outbuf = default_outbuf;
	            }
	            if (!rv) {
	    	    retval = 1; //Match
	    	    break;
	            }
	        }
	    }
	}
	break;
    }
    case NSM_QUERY_NAME_VALUE:
    {
	int namelen = strlen(nsc->sm.qnv.query_string_nv_name);
	int vallen = strlen(nsc->sm.qnv.query_string_nv_value);

        rv = mime_hdr_get_query_arg_by_name((mime_header_t*) hdr,
					    nsc->sm.qnv.query_string_nv_name,
					    namelen, &data, &datalen);
        if (!rv) {
	    DBG_LOG(MSG3, MOD_NAMESPACE, "got query name = %s, value = %s",
		    nsc->sm.qnv.query_string_nv_name, data);

    	    /* query name found */
            if (strcmp(nsc->sm.qnv.query_string_nv_value, "*") == 0) {
                /* match any value.. */
		retval = 1; // Match
	    } else if ((datalen == vallen) &&
    	            (strncasecmp(data, nsc->sm.qnv.query_string_nv_value,
    			 datalen) == 0)) {
    		retval = 1; // Match
            }
        } else {
	    DBG_LOG(MSG3, MOD_NAMESPACE, "didnt find a query name/val match");
        }

	    break;
    }
    case NSM_QUERY_NAME_VALUE_REGEX:
    {
	rv = mime_hdr_get_known(hdr, MIME_HDR_X_NKN_QUERY, &data, &datalen,
			&attrs, &hdr_cnt);
	if (!rv && datalen) {
	    if (ns_thread >= nsc->sm.qnvr.query_string_regex_tot_cnt) {
		ns_thread = 0;
	    }	
	    rv = nkn_regexec(nkn_regaindex(nsc->sm.qnvr.query_string_regex_ctx, 
			     ns_thread), (char *) data,
			     errorbuf, sizeof(errorbuf));
	    if (!rv) {
	        retval = 1;
	        break;
	    }
	}
	break;
    }
    case NSM_VIRTUAL_IP_PORT:
    {
	const char *request_dest_ip;
        int request_dest_ip_len;
        const char *request_dest_port;
        int request_dest_port_len;
	u_int32_t attr;
	int header_cnt;
        u_int32_t req_dest_ip;
        u_int32_t req_dest_port;

	    rv = get_known_header(hdr, MIME_HDR_X_NKN_REQ_DEST_IP,
				  &request_dest_ip,
				  &request_dest_ip_len, &attr, &header_cnt);
        if (!rv) {
            req_dest_ip = atol_len(request_dest_ip, request_dest_ip_len);
        } else {
            req_dest_ip = 0;
        }

	    rv = get_known_header(hdr, MIME_HDR_X_NKN_REQ_DEST_PORT,
				  &request_dest_port,
        		          &request_dest_port_len, &attr, &header_cnt);
        if (!rv) {
            req_dest_port = atol_len(request_dest_port, request_dest_port_len);
        } else {
            req_dest_port = 0;
        }

        if ((!nsc->sm.vip.virtual_host_ip ||
            (nsc->sm.vip.virtual_host_ip == req_dest_ip)) &&
            (nsc->sm.vip.virtual_host_port == req_dest_port)) {
            retval = 1; // Match
        }
	    break;
    }
    default:
    {
	    // Should never happen
	    break;
    }
    } // End of switch

#undef OUTBUF
#undef DEFAULT_OUTBUF_BUFSIZE
    return retval;
}

namespace_token_t rtsp_request_to_namespace(mime_header_t * hdr)
{
    int rv;
    const char *host;
    int hostlen;
    const char *uri;
    int urilen;
    u_int32_t attr;
    int header_cnt;
    const char *method;
    int methodlen;
    const char *port;
    int portlen;
    ns_stat_type_t access_method;
    //char errorbuf[1024];

    /* Check whether mime protocol is set correctly */
    if (hdr->protocol != MIME_PROT_RTSP) {
	DBG_LOG(MSG, MOD_NAMESPACE,
		"rtsp_request_to_namespace() failed, bad mime protocol type");
	return NULL_NS_TOKEN;
    }

    rv = get_known_header(hdr, RTSP_HDR_X_HOST,
			  &host, &hostlen, &attr, &header_cnt);
    if (rv) {
	host = 0;
	hostlen = 0;
    }

    rv = get_known_header(hdr, RTSP_HDR_X_ABS_PATH,
			  &uri, &urilen, &attr, &header_cnt);
    if (rv) {
	DBG_LOG(MSG, MOD_NAMESPACE,
		"get_known_header(RTSP_HDR_X_ABS_PATH) failed, rv=%d", rv);
	return NULL_NS_TOKEN;
    }

    rv = get_known_header(hdr, RTSP_HDR_X_METHOD, &method, &methodlen,
			  &attr, &header_cnt);

    if (!rv) {
	access_method = GET_REQS;
    } else {
	access_method = OTHER_REQS;
    }

    rv = get_known_header(hdr, RTSP_HDR_X_PORT, &port, &portlen,
                          &attr, &header_cnt);

    namespace_token_t ret = request_to_namespace(hdr, host,
			hostlen, uri, urilen, access_method, NULL, NULL);
    if (!VALID_NS_TOKEN(ret)) {
	if (portlen) {
		hostlen = hostlen + (portlen + 1);
    		ret = request_to_namespace(hdr, host, hostlen, uri, urilen,
				access_method, NULL, NULL);
	} else {
		char* hostname_str = alloca(hostlen + 4 + 1);
		strncpy(hostname_str, host, hostlen);
		strncpy(hostname_str+hostlen, ":554", 4);
    		ret = request_to_namespace(hdr, hostname_str,
			strlen(hostname_str), uri, urilen, access_method, 
			NULL, NULL);
	}
    }
    return ret;
}

STATIC namespace_token_t int_http_request_to_namespace(mime_header_t *hdr,
					    namespace_descriptor_t *my_nsd,
					    const namespace_config_t **ppnsc)
{
    int rv;
    //int n, m;
    //int prefix_index;
    const char *host;
    int hostlen;
    const char *uri;
    int urilen;
    u_int32_t attr;
    int header_cnt;
    //int namespace_index;
    //char *p;
    //namespace_descriptor_t *nsd;
    //namespace_token_t nstoken;
    const char *method;
    int methodlen;
    ns_stat_type_t access_method;
    //char errorbuf[1024];

    rv = get_known_header(hdr, MIME_HDR_HOST, &host, &hostlen, &attr, &header_cnt);
    if (rv) {
    	host = 0;
	hostlen = 0;
    }
#if 0
    /* uncomment out these lines and re-open bug 1495 */
    if (host && hostlen && (p = memchr(host, ':', hostlen)) != NULL) {
        hostlen = p - host;
    }
#endif

    rv = get_known_header(hdr, MIME_HDR_X_NKN_DECODED_URI, &uri, &urilen, &attr, 
    			  &header_cnt);
    if (rv) {
    	rv = get_known_header(hdr, MIME_HDR_X_NKN_URI, &uri, &urilen, &attr, 
    			      &header_cnt);
    	if (rv) {
	    DBG_LOG(MSG, MOD_NAMESPACE, 
		    "get_known_header(MIME_HDR_X_NKN_URI) failed, rv=%d", rv);
	    return NULL_NS_TOKEN;
    	}
    }

    rv = get_known_header(hdr, MIME_HDR_X_NKN_METHOD, &method, &methodlen, 
			  &attr, &header_cnt);
    if (!rv) {
        if ((strncasecmp(method, "GET", 3) == 0) ||
	    (strncasecmp(method, "HEAD", 4) == 0)) {
	    access_method = GET_REQS;
	} else if (strncasecmp(method, "PUT", 3) == 0) {
	    access_method = PUT_REQS;
	} else {
	    access_method = OTHER_REQS;
	}
    } else {
        access_method = OTHER_REQS;
    }

    return request_to_namespace(hdr, host, hostlen, uri, urilen,
				access_method, my_nsd, ppnsc);
}

STATIC namespace_token_t request_to_namespace(mime_header_t * hdr,
					      const char *host,
					      int hostlen, const char *uri,
					      int urilen,
					      ns_stat_type_t access_method,
					      namespace_descriptor_t *my_nsd,
					      const namespace_config_t **ppnsc)
{
    int rv;
    int n, m;
    int prefix_index;
    int namespace_index, delflag;
    char *p;
    namespace_descriptor_t *nsd;
    namespace_token_t nstoken;
    uint64_t ns_thread, cur_ns_thread;
    char errorbuf[1024];


    if (!my_nsd && NS_CONFIG_UPDATE) {
	process_namespace_update();
    }
 
    void *ns_tkey = pthread_getspecific(namespace_key);
    ns_thread = (uint64_t)ns_tkey;

    nsd = ref_namespace_descriptor_t(my_nsd, &delflag);
    if (!nsd) {
	DBG_LOG(MSG, MOD_NAMESPACE, "No namespace descriptor defined");
	return NULL_NS_TOKEN;
    }

    rv = host ? (*nsd->ht_hostnames.lookup_func) (&nsd->ht_hostnames,
						  host, hostlen,
						  &prefix_index) : 1;

    if (rv && nsd->hostname_regex_namespaces) {
	// Look for match using hostname regex

	char *hostname_str;
	hostname_str = alloca(hostlen + 4 + 1); // max(":80", ":554") => 4, null => 1
	mapstr2lower(host, hostname_str, hostlen);
	hostname_str[hostlen] = '\0';
	p = strchr(hostname_str, ':');
	if (!p) {
	    if (hdr->protocol == MIME_PROT_HTTP)
		    strcpy(&hostname_str[hostlen], ":80");
	    else if (hdr->protocol == MIME_PROT_RTSP)
		    strcpy(&hostname_str[hostlen], ":554");
	}

	for (n = 0; n < nsd->hostname_regex_namespaces; n++) {
	    char hostname_regex[1024];
	    int hostname_regex_buflen;

	    snprintf(hostname_regex, sizeof(hostname_regex),
		     HOSTNAME_REGEX_FMT, n);
	    hostname_regex[sizeof(hostname_regex) - 1] = '\0';
	    hostname_regex_buflen = strlen(hostname_regex);

	    rv = (*nsd->ht_hostnames.lookup_func) (&nsd->ht_hostnames,
						   hostname_regex,
						   hostname_regex_buflen,
						   &prefix_index);
	    if (!rv) {
		// Apply regex expression
		namespace_index =
		    nsd->prefix_maps[prefix_index]->namespace_map[0];
		if (ns_thread >= 
			nsd->namespaces[namespace_index]
			->hostname_regex_tot_cnt) {
		    cur_ns_thread = 0;
		} else {
		    cur_ns_thread = ns_thread;
		}
		rv = nkn_regexec(nkn_regaindex(nsd->namespaces[namespace_index]->
				 hostname_regex_ctx, cur_ns_thread),
				 hostname_str,
				 errorbuf, sizeof(errorbuf));
		if (!rv) {
		    DBG_LOG(MSG, MOD_NAMESPACE, "regex match: "
			    "regex=[\"%s\"] string=[%s] "
			    "prefix_index=%d namespace_index=%d",
			    nsd->namespaces[namespace_index]->
			    hostname_regex, hostname_str, prefix_index,
			    namespace_index);

		    /* BZ 4456
		     * OK. domain matches.. what about the real match criteria
		     */
                    TSTR(uri, urilen, uristr);
                    for (m = 0; m < nsd->prefix_maps[prefix_index]->used_entries; m++) {
    	                namespace_index =
			    nsd->prefix_maps[prefix_index]->namespace_map[m];

		        if (checkfor_namespace_match(nsd->namespaces[namespace_index],
					hdr, uristr, ns_thread)) {
		            nstoken = nsd->token_start;
		            nstoken.u.token_data.val += namespace_index; // Success, hold ref
		            if (nsd->namespaces[namespace_index]->stat) {
			         AO_fetch_and_add1(&nsd->namespaces[namespace_index]->stat->stats[access_method]);
		            }
			    if (ppnsc) {
			    	*ppnsc = nsd->namespaces[namespace_index];
			    }
		            return nstoken;
		        }
	    	    }
		    /* BZ 4456
		     * no match. continue onwards and see if some
		     * other regex matches this domain?
		     */
		    continue;
		}
	    } else {
		DBG_LOG(MSG, MOD_NAMESPACE,
			"hostname regex lookup(%s) failed, rv=%d",
			hostname_regex, rv);
	    }
	}

	if (n < nsd->hostname_regex_namespaces) {
	    rv = 0;		// Match
	} else {
	    rv = 1;		// No match
	}
    }

    if (rv) {
	rv = (*nsd->ht_hostnames.lookup_func) (&nsd->ht_hostnames,
					       DEFAULT_HOSTNAME,
					       DEFAULT_HOSTNAME_LEN,
					       &prefix_index);
	if (rv) {
	    TSTR(host, hostlen, thost);
	    DBG_LOG(MSG, MOD_NAMESPACE, "lookup(%s) failed, rv=%d", thost,
		    rv);
	    unref_namespace_descriptor_t(nsd, delflag);
	    return NULL_NS_TOKEN;
	}
    }

    TSTR(uri, urilen, uristr);
    for (n = 0; n < nsd->prefix_maps[prefix_index]->used_entries; n++) {
	namespace_index = nsd->prefix_maps[prefix_index]->namespace_map[n];

	if (checkfor_namespace_match(nsd->namespaces[namespace_index],
				     hdr, uristr, ns_thread)) {
	    nstoken = nsd->token_start;
	    nstoken.u.token_data.val += namespace_index;	// Success, hold ref
	    if (nsd->namespaces[namespace_index]->stat) {
		AO_fetch_and_add1(&nsd->namespaces[namespace_index]->stat->stats[access_method]);
	    }
	    if (ppnsc) {
		*ppnsc = nsd->namespaces[namespace_index];
	    }
	    return nstoken;
	}
    }
    unref_namespace_descriptor_t(nsd, delflag);
    return NULL_NS_TOKEN;
}


/*
 * ns_status_callback() - Notify subscribers of namespace state change
 */
STATIC 
void ns_status_callback(namespace_descriptor_t *nsd, int online)
{
    int ns;
    int n;
    namespace_token_t token;
    namespace_config_t *nsc;

    for (ns = 0; ns < nsd->used_namespaces; ns++) {
	token = nsd->token_start;
	token.u.token_data.val += ns;
    	nsc = nsd->namespaces[ns];

    	for (n = 0; n < num_ns_calback_procs; n++) {
	    if (cb_procs[n]) {
	    	(*cb_procs[n])(token, nsc, online);
	    }
    	}
    }
}

/*
 * ns_uid_to_config_t() - Given a namespace_descriptor_t, return the
 *			  namespace_config_t associated with the given ns_uid.
 *
 *	Note: Internal use only.
 *	      Caller has acquired the appropriate locks to insure consistent
 *	      access to the namespace_descriptor_t.
 */
STATIC 
const namespace_config_t *ns_uid_to_config_t(namespace_descriptor_t *nsd, 
					     const char *ns_uid)
{
    int n;

    if (!nsd || !ns_uid) {
    	return NULL;
    }

    for (n = 0; n < nsd->used_namespaces; n++) {
    	if (strcmp(ns_uid, nsd->namespaces[n]->ns_uid) == 0) {
	    return nsd->namespaces[n];
	}
    }
    return NULL;
}

/*
 * ns_get_cache_key() - Given a namespace, and call context is for CACHE_KEY
 *			return a cache-key
 *
 *	Note: Internal use only.
 */
STATIC
int ns_get_cache_key(const namespace_config_t *nsc,
		     req2os_context_t caller_context,
		     const char **data,
		     int *datalen)
{
    int retval = 0;

    switch(caller_context)
    {
    case REQ2OS_CACHE_KEY:
	if (nsc->http_config->cache_index.exclude_domain) {
	    const char *p =
		(nsc->http_config->origin.select_method == OSS_HTTP_DEST_IP || 
                 nkn_http_is_tproxy_cluster(nsc->http_config)) ? 
		 (nsc->http_config->cache_index.ldn_name) :
		nsc->rtsp_config->cache_index.ldn_name;
	    /* Config explicitly tells to remove the
	     * domain name and return a fake domain name
	     * for the cache key. We replace the data,
	     * datalen, origin_port variables to a fake
	     * one here..
	     */
	    *datalen = strlen(p);
	    *data = p;
	}
	/* No else case, fall through to code below
	 * to return the caller actual origin server/ip
	 * instead of a fake one.
	 */
	break;
    case REQ2OS_ORIGIN_SERVER_LOOKUP:
    case REQ2OS_ORIGIN_SERVER:
	/* do nothing.. fall through to code below,
	 * grab the origin server ip or name and return
	 * to caller
	 */
	break;
    default:
	retval = 16;
    }
    return retval;
}

void
nkn_get_namespace_cache_limitation(namespace_token_t ns_token,
				   off_t *cache_min_size,
				   int *uri_min_depth,
				   int *cache_age_threshold,
				   uint64_t *cache_ingest_size_threshold,
				   int *cache_ingest_hotness_threshold,
				   int *cache_fill_policy)
{
    const namespace_config_t	*nsc;

    /* Fill in default values */
    *cache_age_threshold = 0;
    *uri_min_depth       = 10; /* Default value of 10 */
    *cache_min_size      = 0;
    *cache_fill_policy   = INGEST_CLIENT_DRIVEN;
    *cache_ingest_size_threshold    = 0;
    *cache_ingest_hotness_threshold = 3; /* AM default hotness threshold */

    nsc = namespace_to_config(ns_token);

    if(nsc) {
	*cache_age_threshold =	nsc->http_config->policies.store_cache_min_age;
	*uri_min_depth       =	nsc->http_config->policies.uri_depth_threshold;
	*cache_min_size      =	nsc->http_config->policies.store_cache_min_size;
	*cache_ingest_size_threshold =
				nsc->http_config->policies.cache_ingest.size_threshold;
	*cache_ingest_hotness_threshold =
				nsc->http_config->policies.cache_ingest.hotness_threshold;
	*cache_fill_policy   =	nsc->http_config->policies.cache_fill;

	release_namespace_token_t(ns_token);
    }
}

/* caller has to ensure nsc is not NULL */
inline int nkn_namespace_use_client_ip(const namespace_config_t *nsc)
{
    return (nsc->http_config->origin.u.http.use_client_ip ||
            nkn_http_is_tproxy_cluster(nsc->http_config));
}

/* check whether mfc is acting as full/semi-transparent mode.
 * (ie, when dest-ip/follow-header-host is configured)
 */
inline int nkn_http_is_transparent_proxy(const namespace_config_t *nsc)
{
    return ((nsc->http_config->origin.select_method == OSS_HTTP_DEST_IP) ||
	    (nsc->http_config->origin.select_method == OSS_HTTP_FOLLOW_HEADER) ||
	    (nsc->http_config->origin.u.http.use_client_ip) ||
            nkn_http_is_tproxy_cluster(nsc->http_config));
}

/*
 * End of nkn_namespace.c
 */
