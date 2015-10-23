#ifndef __NVSD_MGMT_NAMESPACE__H
#define __NVSD_MGMT_NAMESPACE__H

/*
 *
 * Filename:  nvsd_mgmt_namespace.h
 * Date:      2008/12/31
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-10 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include "nkn_common_config.h"
#include "nvsd_mgmt_virtual_player.h"
#include "nkn_regex.h"
#include "nvsd_mgmt_publish_point.h"
#include "nvsd_mgmt_cluster.h"
#include "nvsd_resource_mgr.h"
#include "nvsd_mgmt_pe.h"
/* Types */
#define	MAX_URIS			2
#define	MAX_HTTP_ORIGIN_SERVERS		4
#define	MAX_NFS_ORIGIN_SERVERS		4
#define	MAX_RTSTREAM_ORIGIN_SERVERS	1
#define	MAX_CONTENT_TYPES		5  // 4 for content-type + 1 for any
#define MAX_ORIGIN_REQUEST_HEADERS	4
#define MAX_CLIENT_REQUEST_HEADERS	8
#define MAX_CLIENT_RESPONSE_HEADERS	8
#define MAX_PUB_POINT			4
#define MAX_HTTP_SERVER_MAPS		3
#define MAX_CLUSTERS			1

#define MAX_USER_AGENT_GROUPS 4
typedef enum
{
	NVSD_PROTO_NONE = 0,
	NVSD_HTTP = 1,
	NVSD_NFS = 2,
	NVSD_SERVER_MAP = 3,
	NVSD_RTSTREAM = 4

} nvsd_protocols_en;

typedef enum
{
	OSF_NONE = 0,
	OSF_HOST = 1,	// TODO: needs to be removed. kept for backward compat
	OSF_HTTP_HOST = 1,
	OSF_SERVER_MAP = 2,  // TODO need to be removed once backend is fixed
	OSF_HTTP_ABS_URL = 2,
	OSF_HTTP_FOLLOW_HEADER = 3,
	OSF_HTTP_DEST_IP = 4,
	OSF_HTTP_SERVER_MAP = 5,
	OSF_NFS_HOST = 6,
	OSF_NFS_SERVER_MAP = 7,
	OSF_RTSP_HOST = 8,
	OSF_RTSP_DEST_IP = 9,
	OSF_HTTP_NODE_MAP = 10,
	OSF_HTTP_PROXY_HOST = 11 // CL7 node, internal use
} origin_server_format_en;


typedef enum
{
	MATCH_NONE = 0,
	MATCH_URI_NAME = 1,
	MATCH_URI_REGEX = 2,
	MATCH_HEADER_NAME = 3,
	MATCH_HEADER_REGEX = 4,
	MATCH_QUERY_STRING_NAME= 5,
        MATCH_QUERY_STRING_REGEX = 6,
	MATCH_VHOST = 7,
	MATCH_RTSP_URI_NAME = 8,
	MATCH_RTSP_URI_REGEX = 9,
	MATCH_RTSP_VHOST = 10
} match_type_en;

typedef enum
{
	CH_NONE = 0,
	CH_BASE_URL = 1,
	CH_COMPLETE_URL = 2,
} cluster_hash_url_en;

typedef enum
{
    VARY_HEADER_PHONE = 0,
    VARY_HEADER_TABLET = 1,
    VARY_HEADER_PC = 2,
    VARY_HEADER_OTHERS = 3,
} vary_header_user_agent_t;

#if 0 /*Moved to nvsd_mgmt_cluster.h*/
typedef struct
{
	int heartbeat_interval;
	int connect_timeout;
	int allowed_fails;
	int read_timeout;
}cluster_monitor_params_t;
#endif

typedef enum {
    RTSP_TRANSPORT_CLIENT = 0,
    RTSP_TRANSPORT_UDP,
    RTSP_TRANSPORT_RTSP,
} rtsp_transport_en;

typedef enum {
    FOLLOW_CLIENT = 0,
    FORCE_IPV4,
    FORCE_IPV6,
} ip_version_en;

#if 0 /*Moved to nvsd_mgmt_cluster.h*/
typedef enum
{
	SM_FT_NONE = 0,
 	SM_FT_HOST_ORIGIN_MAP = 1,
	SM_FT_CLUSTER_MAP = 2,
	SM_FT_ORIG_ESC_MAP = 3,
	SM_FT_NFS_MAP = 4,
} server_map_format_en;
#endif

typedef enum
{
	NS_OBJ_REVAL_NONE = 0,
	NS_OBJ_REVAL_ALL ,
} NS_OBJ_REVAL;	

typedef enum
{
	NS_OBJ_REVAL_OFFLINE = 0,
	NS_OBJ_REVAL_INLINE,
} NS_OBJ_REVAL_TRIGGER;

typedef enum
{
        AGGRESSIVE =1,
        CLIENT_DRIVEN =2
} cache_fill_en;

typedef enum
{
        LIFO =1,
        FIFO =2
} ingest_policy_en;

typedef enum
{
    NONE =0,
    MAXMIND =1,
    QUOVA =2
} geo_service_type;
#if 0 /*Moved to nvsd_mgmt_cluster.h*/
typedef struct server_map_node_data_st
{
	char			*name;
	server_map_format_en	format_type;
	char			*file_url;
	int			refresh;
	char			*binary_server_map;
	int			binary_size;
	cluster_monitor_params_t monitor_values;
} server_map_node_data_t;
#endif
#if 0
#define NKN_MAX_RESOURCES		6
typedef struct resource_st {
	AO_t			max;
	AO_t			used;
} resource_t;

typedef struct resource_pool_node_data_st {
    char					*name;
    struct resource_pool_node_data_st		*parent;
    resource_t					resources[NKN_MAX_RESOURCES];
} resource_pool_t;
#endif
struct fq_domain_name {
	char 		*fqdn;
	int		port;
	int		weight;
	boolean		keep_alive;
};
typedef struct fq_domain_name fqdn_t;

struct fq_domain_path {
	char 			*fqdn_with_path;  // this should be first add anything below this
	unsigned int 		port;
	boolean			local_cache;
} ;
typedef struct fq_domain_path fqdn_path_t;


#if 0
/*! follow header structure */
struct follow_name {
	follow_type_en 		follow_type;
	union {
		char 		*follow;
		int 		dest_ip;
		struct {
			char 	*follow_name;
			char 	*cli_ip_name;
		} follow_client_ip;
		struct {
			int 	dest_ip;
			int 	client_ip;
		} dest_client_ip;
	} follow_u;
} ;
typedef struct follow_name follow_name_t;

#endif

typedef struct {
	int	active;
	char	*access_key;
	char	*secret_key;
} md_aws_t;

typedef struct {
	char 	*host;
	char	*dns_query;
	int	port;
	int 	weight;
	boolean keep_alive;
	md_aws_t aws;
} origin_server_http_t;

typedef struct {
	char 	*host;
	int	port;
	boolean local_cache;
} origin_server_nfs_t;

typedef struct {
	char	*dest_header;
	char	*dns_query;
	md_aws_t aws;
} origin_server_follow_header_t;

typedef struct {
	char 	*host;
	int	port;
	int	alternate_http_port;
        rtsp_transport_en transport;
} origin_server_rtsp_t;


/*! added origin server */
typedef struct mgmt_origin_server {
	origin_server_format_en proto;

	origin_server_http_t	http;

	origin_server_nfs_t	nfs;

	origin_server_rtsp_t	rtsp;

	/* Server Map  - HTTP */
	char			*http_server_map_name[MAX_HTTP_SERVER_MAPS];
	server_map_node_data_t 	*http_server_map[MAX_HTTP_SERVER_MAPS];
	int http_smap_count;

	/* Server Map  - NFS */
	char			*nfs_server_map_name;
	server_map_node_data_t 	*nfs_server_map;

	/* Other types of origin server
	 * (absolute_url == true) => Forward proxy mode
	 */
	boolean			absolute_url;
	/* Could be follow-client, force-ipv4, force-ipv6 */
	int 			ip_version;

	int			http_idle_timeout;

	/* Other types of origin server (transparent/virtual proxy modes)
	 * follow.dest_header => Use header name to derive destination origin
	 */
	origin_server_follow_header_t follow;

    /* If this is set to true, then we are expected to be
     * working as a transparent proxy.
     * This is true if and only if use_dest_ip is also set
     */
    boolean     http_use_client_ip;
    boolean     rtsp_use_client_ip;
} mgmt_origin_server_t;

/* policy engine */
#define MAX_CALLOUTS    4

typedef struct policy_srvr_obj_st {
	policy_srvr_t   *policy_srvr;
	boolean callouts[MAX_CALLOUTS];
	policy_failover_type_t fail_over;
	uint16_t precedence;
	struct policy_srvr_obj_st *nxt;
} policy_srvr_obj_t;

typedef struct policy_engine_data_st
{
	char 		*policy_file;
	policy_srvr_obj_t *policy_srvr_obj;
} policy_engine_data_t;

typedef struct uri_prefix_data_st
{
	/* URI-PREFIX for this namespace.
	 * TODO: Remove this since uri-prefix has moved aboce one level
	 */
	char	*uri_prefix;

	char	*domain;
	char 	*domain_regex;
	boolean	proto_http;
	boolean	proto_rtmp;
	boolean	proto_rtsp;
	boolean	proto_rtp;
	boolean proto_rtstream;

	nvsd_protocols_en	origin_server_proto;
	origin_server_format_en	origin_server_format;

	origin_server_http_t 	origin_server_http [MAX_HTTP_ORIGIN_SERVERS];

	origin_server_nfs_t 	origin_server_nfs [MAX_NFS_ORIGIN_SERVERS];

	/* Server Map */
	char			*server_map_name;
	server_map_node_data_t 	*server_map;

	struct
	{
		char	*host;
		int	port;
		char	*alt_proto;
		int	alt_port;
	} origin_server_rtstream [MAX_RTSTREAM_ORIGIN_SERVERS];
} uri_prefix_data_t;

/*! added namespace match */
typedef struct mgmt_namespace_match {
	match_type_en type;   /*! case uri | header | query string | virtual-host */
	struct {
		struct {
			char		*uri_prefix;
			char 		*uri_prefix_regex;
		} uri;
		struct {
			char 		*name;
			char 		*value;
			char 		*regex;
		} header;
		struct {
			char 		*name;
			char 		*value;
			char 		*regex;
		} qstring;
		struct {
			char 		*fqdn;
			unsigned int 	port;
		} virhost;
	} match;
} mgmt_namespace_match_t;

typedef struct cache_age_content_type_st
{
	char				*content_type;
	int				cache_age;
} cache_age_content_type_t;

/* Object Re-validation */
typedef struct cache_revalidate_on_barrier_time_st
{
	time_t reval_time;
	NS_OBJ_REVAL reval_type;
	NS_OBJ_REVAL_TRIGGER reval_trigger;
} cache_barrier_revalidate_t;

/* Object Cache-Pin */
typedef struct cache_pin_st
{
	boolean 			cache_pin_ena;
	boolean                         cache_auto_pin;
	uint64_t			cache_resv_bytes;
	uint64_t			max_obj_bytes;
	char   				*pin_header;
	char  				*end_header;
} cache_pin_t;

typedef struct cache_object_st
{
	char 				*start_header;
	cache_pin_t 			cache_pin;
}cache_object_t;

typedef enum reval_method_et {
    REVAL_METHOD_HEAD = 0,
    REVAL_METHOD_GET = 1
} reval_method_t;

typedef struct cache_reval_st {
    boolean	cache_revalidate;
    reval_method_t	method;
    boolean	validate_header;
    char	*header_name;
    boolean     use_client_headers;
} cache_reval_t;

typedef struct cache_ingest_st {
	int hotness_threshold;
	int size_threshold;
	int incapable_byte_ran_origin;
} cache_ingest_t;

typedef struct compress_file_type {
	char *type;
	int type_len;
	int allowed;
}compress_file_type_t;

typedef struct unsuccessful_response_cache
{
    int status;
    uint32_t code;
    uint32_t age;
} unsuccessful_response_cache_t;

typedef struct origin_fetch_data_st
{
	char				*cache_directive_no_cache;
	int				cache_age;
	char				*custom_cache_control;
	boolean				partial_content;
	boolean 			date_header_modify;
	// DN: cache_revalidate deprecated and replaced by cache_reval_t
	//boolean				cache_revalidate;
	boolean				flush_intermediate_caches;
	int				content_store_cache_age_threshold;
	int				content_store_uri_depth_threshold;
	int				offline_fetch_size;
	boolean				offline_fetch_smooth_flow;
	boolean				convert_head;
	off_t				store_cache_min_size;
	boolean 			is_set_cookie_cacheable;
	boolean 			is_host_header_inheritable;
        char                            *host_header;
	int				content_type_count;
	cache_age_content_type_t 	cache_age_map[MAX_CONTENT_TYPES];
	cache_barrier_revalidate_t      cache_barrier_revalidate;
	boolean				ignore_s_maxage;
	boolean				enable_expired_delivery;
	boolean 			disable_cache_on_query;
	boolean				strip_query;
	boolean				use_date_header;
	boolean 			disable_cache_on_cookie;
	int				client_req_max_age;
	boolean				client_req_serve_from_origin;
	cache_fill_en			cache_fill;
	ingest_policy_en		ingest_policy;
	boolean				ingest_only_in_ram; // CL7 internal use
	boolean				eod_on_origin_close;
	cache_object_t 			object;
	boolean				redirect_302;
	boolean		    tunnel_all; // BZ 6682
	boolean 			object_correlation_etag_ignore;
	boolean 			object_correlation_validatorsall_ignore;
	uint32_t 			client_driven_agg_threshold;
	uint32_t cache_obj_size_min;
	uint32_t cache_obj_size_max;
	cache_reval_t			cache_reval;
	cache_ingest_t	cache_ingest;
	boolean				cacheable_no_revalidation;
	boolean				client_req_connect_handle;
	boolean				compression_status;
	uint32_t			compression_algorithm;
	uint64_t			compress_obj_min_size;
	uint64_t			compress_obj_max_size;
	int				compress_file_type_cnt;
	compress_file_type_t		compress_file_types[MAX_NS_COMPRESS_FILETYPES];
	unsuccessful_response_cache_t   unsuccess_response_cache[MAX_UNSUCCESSFUL_RESPONSE_CODES];
	boolean                         bulk_fetch;
	uint8_t                         bulk_fetch_pages;

} origin_fetch_data_t;


typedef struct origin_request_header_add_st
{
	char	*name;
	char 	*value;
} origin_request_header_add_t;

typedef struct
{
	int conn_timeout;
	int conn_retry_delay;
	int read_timeout;
	int read_retry_delay;
}origin_conn_params_t;

typedef struct origin_request_data_st
{
	origin_request_header_add_t 	add_headers[MAX_ORIGIN_REQUEST_HEADERS];
	boolean				x_forward_header;
	boolean				include_orig_ip_port;
	origin_conn_params_t		orig_conn_values;
} origin_request_data_t;

typedef struct vip_st
{
	char	*vip[NKN_MAX_VIPS];
	int	n_vips;
} vip_t;

typedef struct client_response_header_add_st
{
	char	*name;
	char 	*value;
} client_response_header_add_t;

typedef struct client_response_header_delete_st
{
	char	*name;
} client_response_header_delete_t;


typedef struct client_response_data_st
{
	client_response_header_add_t	add_headers[MAX_CLIENT_RESPONSE_HEADERS];
	client_response_header_delete_t delete_headers[MAX_CLIENT_RESPONSE_HEADERS];

} client_response_data_t;

typedef struct client_request_header_st
{
	char *name;
	char *value;
} client_request_header_t;

typedef struct client_request_data_st
{
	boolean passthrough_http_head; // Internal use, no CLI interface
	boolean http_head_to_get;
	client_request_header_t accept_headers[MAX_CLIENT_REQUEST_HEADERS];
	boolean dns_resp_mismatch;
} client_request_data_t;

#define MAX_LDN_NAME_SIZE	(32)
#define MAX_HEADER_NAMES	(8)
typedef struct cache_index_st
{
	char	ldn_name[MAX_LDN_NAME_SIZE];
	boolean	exclude_domain;
	boolean	include_header;
	boolean	include_object;
	uint32_t	checksum_bytes;
	uint32_t	checksum_offset;
	boolean		checksum_from_end;
	int		headers_count;
	char	*include_header_names[MAX_HEADER_NAMES];
} cache_index_t;

typedef struct tunnel_override_st
{
        boolean auth_header;
        boolean cookie;
        boolean query_string;
        boolean cache_control;
        boolean cc_notrans;
        boolean obj_expired;
} tunnel_override_t;

typedef struct dynamic_uri_st
{
	char *regex;
	char *map_string;
	boolean no_match_tunnel;
	char *tokenize_str;
} dynamic_uri_t;

typedef struct geo_service_st
{
    geo_service_type service;
    char *api_url;
    boolean failover_mode;
    uint32_t lookup_timeout;
} geo_service_t;

typedef struct seek_uri_st
{
	char *regex;
	char *map_string;
	boolean seek_uri_enable;
} seek_uri_t;

typedef struct request_security_st
{
    boolean secure;
    boolean non_secure;
}req_security_t;

typedef struct ns_access_log
{
    char *name;
    int al_record_cache_history;
    int al_resp_header_configured;
    int al_resp_hdr_md5_chksum_configured;
}ns_access_log_t;

typedef struct sas_st
{
	uint32_t read_size;
	uint32_t free_block_thres;
	size_t	 max_disk_usage; //size in mb
	boolean  group_read_enable;
}sas_t;

typedef struct sata_st
{
	uint32_t read_size;
	uint32_t free_block_thres;
	size_t	 max_disk_usage; //size in mb
	boolean  group_read_enable;
}sata_t;

typedef struct ssd_st
{
	uint32_t read_size;
	uint32_t free_block_thres;
	size_t	 max_disk_usage; //size in mb
	boolean  group_read_enable;
}ssd_t;

typedef struct media_cache_st
{
	sas_t sas;
	sata_t sata;
	ssd_t ssd;
} media_cache_t;

// URL Filter Definitions

typedef enum { // URL Filter data type
    	UF_UNDEF = 0,	// Undefined
    	UF_URL,		// Internal format (testing)
    	UF_IWF,		// Internet Watch Foundation (IWF) format
    	UF_CALEA	// US version
} uf_data_t;

typedef enum {
	UF_LT_UNDEF = 0,
	UF_LT_BLACK_LIST,
	UF_LT_WHITE_LIST
} uf_list_type;

typedef enum {
	UF_REJECT_UNDEF= 0,
	UF_REJECT_RESET,
	UF_REJECT_CLOSE,
	UF_REJECT_HTTP_403,
	UF_REJECT_HTTP_404,
	UF_REJECT_HTTP_301,
	UF_REJECT_HTTP_302,
	UF_REJECT_HTTP_200,
	UF_REJECT_HTTP_CUSTOM
} uf_reject_action_t;

typedef struct url_filter_data {
	char *uf_map_name;
	/* this is not required to be filled in namespace data structures */
	char *ns_name;
	uf_data_t uf_type;
      	char *uf_file_url;
	char *uf_local_file;
      	int uf_file_url_refresh;
	/* current state of url filter-map */
	int state;

	void *uf_trie_data; // Really url_filter_trie_t
	uf_list_type uf_list_type;

	uf_reject_action_t uf_reject_action;
	union {
	    struct uf_rej_reset {
	    } reset;

	    struct uf_rej_close {
	    } close;

	    struct uf_rej_403 {
	    } http_403;

	    struct uf_rej_404 {
	    } http_404;

	    struct uf_rej_301 {
	    	const char *redir_host;
	    	const char *redir_url;
	    } http_301;

	    struct uf_rej_302 {
	    	const char *redir_host;
	    	const char *redir_url;
	    } http_302;

	    struct uf_rej_200 {
	    	const char *response_body;
	    } http_200;

	    struct uf_rej_custom {
	    	int http_response_code;
	    	const char *response_body;
	    } http_custom;
	} u_rej, uri_size_rej;

	uint32_t uf_uri_size;
	uf_reject_action_t uf_uri_size_reject_action;
} url_filter_data_t;

/*! change notes to namespace_node_data_st:
  - to remove precedence variable - it is now moved under namespace match
  - to remove proxy-mode variable - it can be dynamically decided based on
	origin server configuration
  - to remove array-based uri attribute structure - it is now moved under
	namespace match
  - added namespace_match attribute structure - supports union of
	(uri,header,query_string,virtual host) matches
  - added origin_server attribute structure - supports union of (http/nfs)
	origin server types
  - added proto_http/proto_rtsp --- check TODO
*/
typedef struct namespace_node_data_st
{
	/* These are nodes in the namespace module */
	char				*namespace;
	boolean				active;
	boolean				deleted;
	char				*uid;

	/*! removed this as this is moved under namespace match */
	int				precedence;
	/* The proxy mode is set for this namespace based on the
	 * origin server specification.
	 */
	int 				proxy_mode;
	uint32_t 			tproxy_ipaddr;

	cluster_hash_url_en		hash_url;

	int				num_clusters;
	cluster_node_data_t		*clusterMap[MAX_CLUSTERS];
	char				*cluster_map_name[MAX_CLUSTERS];

	/*! check TODO proto_http and proto_rtsp */
	boolean				proto_http;
	boolean				proto_rtsp;
	boolean				proto_rtmp;
	boolean				proto_rtp;

	/* Domain Host name (Non-Regex). Default set to "*" */
	char				*domain;
	char				*domain_fqdns[NKN_MAX_FQDNS];
	int				num_domain_fqdns;

	/* Doman Regex. */
	char				*domain_regex;

	/* Precompiled regex context to assist while sorting
	 * the namespace when precedence changes occur
	 */
	nkn_regex_ctx_t 		*domain_comp_regex;

	/* Virtual Player */
	char				*virtual_player_name;
	virtual_player_node_data_t 	*virtual_player;

	/*! need to remove uri preix */
	uri_prefix_data_t		uri_prefix[MAX_URIS];

	/* Match criteria. Qualified by the match_type enum.
	 * Note that this is the new structure to follow
	 */
	mgmt_namespace_match_t 		ns_match;

	/* Origin server spec for this namespace.
	 * Qualified by the origin_server_type enum.
	 * Note that this is the new structure to follow.
	 */
	mgmt_origin_server_t 		origin_server;

	/* Origin Fetch parameters */
	origin_fetch_data_t		origin_fetch;

	/* Origin Request paramaters */
	origin_request_data_t		origin_request;

	/* Publish point  - DEPRECATED
	 * ---- THIS IS REPLACED WITH - pub_point_data_t[]
	 */
	char				*pub_point_name;
	pub_point_node_data_t		*pub_point;

	/* CLient Response Params */
	client_response_data_t		client_response;
	/* Client Request Params */
	client_request_data_t		client_request;

	/*------------- RTSP SPECIFIC DATA ---------------*/
	pub_point_data_t		rtsp_live_pub_point[MAX_PUB_POINT];
	//mgmt_origin_server_t 		rtsp_origin_server;
	mgmt_namespace_match_t 		rtsp_ns_match;
	origin_fetch_data_t		rtsp_origin_fetch;
	/* ---------------Adding backup counter for namespace------*/
	uint64_t			tot_request_bkup_count;

	uint32_t			http_max_conns;
	uint32_t			http_retry_after_sec;
	uint32_t			rtsp_max_conns;
	uint32_t			rtsp_retry_after_sec;

	/*---------------Policy Engine data------------------------*/
	policy_engine_data_t 		policy_engine;

	/*---------------Resource Pool data------------------------*/
	resource_pool_t			*resource_pool;
	uint64_t			rp_idx;
	cache_index_t			http_cache_index;
	cache_index_t			rtsp_cache_index;
	tunnel_override_t		tunnel_override;
	dynamic_uri_t 			dynamic_uri;
	geo_service_t 			geo_service;
	seek_uri_t			seek_uri;
	req_security_t           	client_req;
	req_security_t           	origin_req;
	ns_access_log_t			access_log;
	media_cache_t			media_cache;
        uint32_t                        md5_checksum_len;
        int                             dscp;
	vip_t				vip_obj;

	/* vary-header details */
	boolean				vary_enable;
	char				*user_agent_regex[MAX_USER_AGENT_GROUPS];

	/*---------------URL Filter data------------------------*/
	url_filter_data_t		url_filter;
} namespace_node_data_t;


/* Function Prototypes */
extern	void nvsd_mgmt_namespace_lock(void);
extern	void nvsd_mgmt_namespace_unlock(void);
extern	namespace_node_data_t *nvsd_mgmt_get_nth_active_namespace(int nth);
extern	namespace_node_data_t *nvsd_mgmt_get_nth_namespace(int nth);
extern  void nvsd_mgmt_get_cache_pin_config(namespace_node_data_t *,
					    uint64_t *max_obj_bytes,
					    uint64_t *resv_bytes,
					    int *pin_enabled);
extern  void nvsd_mgmt_ns_sas_config(namespace_node_data_t *,
					    uint32_t *read_size,
					    uint32_t *free_block_threshold,
					    int *group_read_enable,
					    size_t  *size_in_mb);
extern  void nvsd_mgmt_ns_sata_config(namespace_node_data_t *,
					    uint32_t *read_size,
					    uint32_t *free_block_threshold,
					    int *group_read_enable,
					    size_t  *size_in_mb);
extern  void nvsd_mgmt_ns_ssd_config(namespace_node_data_t *,
					    uint32_t *read_size,
					    uint32_t *free_block_threshold,
					    int *group_read_enable,
					    size_t  *size_in_mb);
extern  resource_pool_t* nvsd_mgmt_get_resource_pool(char *cpResourcePool);
extern	server_map_node_data_t *nvsd_mgmt_get_server_map (char *cpServerMap);
extern	cluster_node_data_t *get_cluster_element(const char *cpClusterMap);
#endif // __NVSD_MGMT_NAMESPACE__H
