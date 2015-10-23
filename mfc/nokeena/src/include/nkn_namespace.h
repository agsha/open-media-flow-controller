/*
 *******************************************************************************
 * nkn_namespace.h -- Interface between TM config data and nokeena executables.
 *******************************************************************************
 */

#ifndef _NKN_NAMESPACE_H
#define _NKN_NAMESPACE_H

#include "nkn_defs.h"
#include "http_header.h"
#include "rtsp_def.h"
#include "nkn_common_config.h"
#include "nkn_regex.h"
#include "nkn_regex.h"
#include "host_origin_map_bin.h"
#include "nkn_trie.h"
#include "atomic_ops.h"
#include "nvsd_mgmt_pe.h"
#include "uf_fmt_url_bin.h"

typedef struct namespace_token {
    union {
        struct ns_token_data {
	    uint32_t gen;
	    uint32_t val;
	} token_data;
	uint64_t token;
    } u;
} namespace_token_t;

typedef uint64_t namespace_token_context_t;

#define NS_TOKEN_VAL(_tok) ((_tok).u.token_data.val)
#define VALID_NS_TOKEN(_tok) ((_tok).u.token != 0)

#define L7_SWT_NODE_NS(_http) \
    if ((_http)->ns_token.u.token != (_http)->cl_l7_node_ns_token.u.token) { \
    	release_namespace_token_t((_http)->ns_token); \
    	(_http)->ns_token = (_http)->cl_l7_node_ns_token; \
	(_http)->nsconf = namespace_to_config((_http)->ns_token); \
    }

#define L7_SWT_PARENT_NS(_http) \
    if ((_http)->ns_token.u.token != (_http)->cl_l7_ns_token.u.token) { \
    	release_namespace_token_t((_http)->ns_token); \
    	(_http)->ns_token = (_http)->cl_l7_ns_token; \
	(_http)->nsconf = namespace_to_config((_http)->ns_token); \
    }

extern namespace_token_t NULL_NS_TOKEN;

#define MAX_RATE_MAP_ENTRIES	50
#define MAX_SSP_BW_FILE_TYPES 	(2)
#define MAX_USER_AGENT_GROUPS	4

/*
 *******************************************************************************
 * Virtual Player (SSP) CLI configuration
 *******************************************************************************
 */

typedef enum param_enable_type {
    PARAM_ENABLE_TYPE_QUERY = 0,
    PARAM_ENABLE_TYPE_SIZE = 1,
    PARAM_ENABLE_TYPE_TIME = 2,
    PARAM_ENABLE_TYPE_RATE = 3,
    PARAM_ENABLE_TYPE_AUTO = 4,
} param_enable_type;


typedef enum rc_scheme {
	VP_RC_MBR = 1,
	VP_RC_AFR,
}rc_scheme;


typedef	struct	hash_verify_st
{
    boolean enable;

    char	*digest_type;
    char *datastr;
    int uol_offset;
    int uol_length;
    char	*secret_key;
    char	*secret_position; //append | prefix
    char	*uri_query;
    char        *expirytime_query;
    char        *url_type;
} hash_verify_t;

typedef	struct fast_start_st
{
    boolean enable;

    char	*uri_query;
    int		size; //Kilobytes
    int		time; //seconds

    param_enable_type active_param;
} fast_start_t;

typedef	struct seek_start_st
{
    boolean enable;
    boolean activate_tunnel;
    char	*uri_query;
    char	*query_length;
    char	*flv_off_type;
    char	*mp4_off_type;
} seek_start_t;

typedef	struct	assured_flow_st
{
    boolean enable;

    char	*uri_query;
    boolean	automatic;
    int	rate; //kbps
    boolean use_mbr;
    char *overhead;     // Delivery overhead

    param_enable_type active_param;
} assured_flow_t;

typedef struct rate_control_st
{
    boolean active;
    boolean auto_flag;
    rc_scheme scheme; // MBR/AFR
    //    vp_rc_scheme_en scheme;
    param_enable_type active_param; // query/ static / auto
    char *qstr;
    int qrate_unit; // kbps kBps mbps mBps
    int static_rate;

    float burst_factor;
} rate_control_t;

typedef	struct	full_download_st
{
    boolean enable;
    boolean always;
    char *matchstr;
    char *uri_query;
    char *uri_hdr;

} full_download_t;

typedef	struct	smooth_flow_st
{
    boolean enable;
    char *uri_query;
} smooth_flow_t;

typedef struct profile_entry_st {
    int	rate; //kbps
    char *matchstr;
    char *uri_query;
    int uol_offset;
    int uol_length;

} profile_entry_t;

typedef	struct	rate_map_st
{
    boolean enable;
    int entries;
    profile_entry_t profile_list[MAX_RATE_MAP_ENTRIES];
} rate_map_t;

typedef struct health_probe_st
{
    boolean enable;
    char    *uri_query;
    char    *matchstr;
} health_probe_t;

typedef struct req_auth_st
{
    boolean enable;
    char    *digest_type;
    int     time_interval;

    char    *shared_secret;
    char    *stream_id_uri_query;
    char    *auth_id_uri_query;
    char    *match_uri_query;
} req_auth_t;

typedef struct signals_smoothflow_st
{
    boolean enable;
    char    *session_id_uri_query;
    char    *state_uri_query;
    char    *profile_uri_query;
    char    *chunk_uri_query;
} signals_smoothflow_t;

typedef struct video_id_st
{
    boolean enable;

    char    *video_id_uri_query;
    char    *format_tag;
} video_id_t;

/* used by virtual player type 6 */
typedef struct smoothstream_pub_st
{
    char    *quality_tag;
    char    *fragment_tag;
} smoothstream_pub_t;

/* virtual player for zeri*/
typedef struct flashstream_pub_st
{
    char    *seg_tag;
    char    *seg_frag_delimiter;
    char    *frag_tag;
} flashstream_pub_t;

/* Bandwidth optimization nodes */
typedef struct bw_opt_filetype_st
{
    char    *file_type;
    char    *transcode_comp_ratio;
    int     hotness_threshold;
    char    *switch_rate;
    int     switch_limit_rate;
} bw_opt_filetype_t;

/* Bandwith optimization based on type */
typedef struct bw_opt_st
{
    boolean enable;
    bw_opt_filetype_t info[MAX_SSP_BW_FILE_TYPES];
} bw_opt_t;

typedef struct ssp_config_st {

    char *player_name;
    int player_type;

    hash_verify_t hash_verify;
    fast_start_t fast_start;

    uint64_t con_max_bandwidth; //kbps

    seek_start_t seek;
    // assured_flow_t assured_flow; //depreciated
    rate_control_t rate_control;
    full_download_t full_download;
    smooth_flow_t smooth_flow;
    rate_map_t rate_map;

    health_probe_t health_probe;
    req_auth_t	req_auth;

    char *sf_control;
    signals_smoothflow_t sf_signals;

    video_id_t video_id; // Youtube video id
    smoothstream_pub_t smoothstream_pub;  //smooth stream pub-silverlight

    flashstream_pub_t flashstream_pub; // zeri pub - flash

    // New feature for Youtube Up/Downgrade
    bw_opt_t bandwidth_opt; // New feature for Youtube/Generic trancoding
} ssp_config_t;

/*
 *******************************************************************************
 * HTTP configuration
 *******************************************************************************
 */
typedef enum {
    ACT_ADD=1,
    ACT_DELETE,
    ACT_REPLACE
} header_action_t;

typedef struct header_descriptor {
    header_action_t action;
    char *name;
    int name_strlen;
    char *value;
    int value_strlen;
} header_descriptor_t;

typedef enum {
    OSS_UNDEFINED = 0,
    OSS_HTTP_CONFIG,
    OSS_HTTP_ABS_URL,
    OSS_HTTP_FOLLOW_HEADER,
    OSS_HTTP_DEST_IP,
    OSS_HTTP_SERVER_MAP,
    OSS_NFS_CONFIG,
    OSS_NFS_SERVER_MAP,
    OSS_RTSP_CONFIG,
    OSS_RTSP_DEST_IP,
    OSS_HTTP_NODE_MAP,
    OSS_HTTP_PROXY_CONFIG, // Origin for Cluster L7 Proxy node
    OSS_MAX_ENTRIES // Always last
} origin_server_select_method_t;

typedef enum ingest_policy_t {
    INGEST_POLICY_LIFO =1,
    INGEST_POLICY_FIFO =2
}ingest_policy_t;

// Have this enum sorted in alphabetical order of token types.
typedef enum {
	TOKEN_FLVSEEK_OFFSET = 0,
	TOKEN_HTTP_RANGE_END,
	TOKEN_HTTP_RANGE_START,
	TOKEN_URI_END = 10,
} uri_token_name_t;

typedef enum {
	COMPRESS_DEFLATE = 0,
	COMPRESS_GZIP,
	COMPRESS_UNKNOWN,
} compression_type_t;

typedef struct cache_ingest_threshold_st {
	int      hotness_threshold;
	uint32_t size_threshold;
	int 	 incapable_byte_ran_origin;
} cache_ingest_threshold_t;

typedef struct aws {
	int active;
	const char *access_key;
	int access_key_len;
	const char *secret_key;
	int secret_key_len;
} aws_t;

typedef struct origin_server_HTTP {
    const char *hostname;
    const char *dns_query;
    int hostname_strlen;
    int dns_query_len;
    unsigned short port; // Native byte order
    int weight;
    int keepalive;
    aws_t aws;
} origin_server_HTTP_t;

typedef struct origin_server_NFS {
    const char *hostname_path;
    int hostname_pathlen;
    unsigned short port; // Native byte order
    int cache_locally;
} origin_server_NFS_t;

typedef enum {
    rtsp_transport_client = 0,
    rtsp_transport_udp,
    rtsp_transport_rtsp,
} rtsp_transport_t;

typedef struct origin_server_RTSP {
    const char *hostname;
    int hostname_strlen;
    unsigned short port;
    unsigned short alt_http_port;
    rtsp_transport_t transport;
} origin_server_RTSP_t;

typedef struct nfs_server_map_node_data_st
{
    char *name;
    char *file_url;
    int	refresh;
    int protocol;
} nfs_server_map_node_data_t;

typedef struct origin_server {
    origin_server_select_method_t select_method;
    union {
        struct OS_http {
	    int entries;
	    origin_server_HTTP_t **server;

	    // Hostname to origin server map
	    int			   map_data_size;
	    host_origin_map_bin_t *map_data;
	    hash_entry_t          *map_hash_hostnames;
	    hash_table_def_t      *map_ht_hostnames;
	    const char		  *ho_ldn_name;
	    int	  		  ho_ldn_name_strlen;

	    // NodeMap
	    int node_map_entries;
	    struct node_map_descriptor **node_map;
	    char **node_map_private_data;

	    // Origin request header
	    boolean		  send_x_forward_header;
	    boolean		  include_orig_ip_port;
	    int			  num_headers;
	    header_descriptor_t	  *header;

	    boolean		  use_client_ip;
	    int			  ip_version;
	} http;
	struct OS_nfs {
	    int entries;
	    origin_server_NFS_t **server;
	    nfs_server_map_node_data_t *server_map;
	    char *server_map_name;
	} nfs;
	struct OS_rtsp {
	    int entries;
	    origin_server_RTSP_t **server;

	    boolean  use_client_ip;
	} rtsp;
    } u;
    int http_idle_timeout;
} origin_server_t;

typedef struct content_type_max_age {
    char *content_type; // NULL is "any"
    int content_type_strlen;
    char *max_age; // "max-age=xxx"
    int max_age_strlen;
} content_type_max_age_t;

typedef struct cache_age_policies {
    int entries;
    content_type_max_age_t **content_type_to_max_age;
} cache_age_policies_t;

typedef enum cache_fill_t {
    INGEST_AGGRESSIVE =1,
    INGEST_CLIENT_DRIVEN =2
}cache_fill_t;

typedef struct cache_redirect {
    int	hdr_len;
    char *hdr_name;
}cache_redirect_t;

typedef enum method_t {
    METHOD_HEAD = 0,
    METHOD_GET = 1
} method_t;

typedef enum {
	NO_CACHE_FOLLOW = 0,
	NO_CACHE_OVERRIDE = 1,
	NO_CACHE_TUNNEL = 2
} origin_fetch_no_cache_t;

typedef struct cache_reval_policies {
    // top-level qualifier. If true, then access this policy list
    boolean	allow_cache_revalidate;
    // See enum method_t
    method_t	method;
    // true: use header_name to get Header to
    // use for validating the object
    boolean	validate_header;
    // valid only if validate_header == true
    char	*header_name;
    int		header_len;
    boolean     use_client_headers;
} cache_reval_policies_t;

typedef struct origin_server_policies {
    int cache_no_cache;
    int cache_age_default;
    int cache_partial_content; // Currently not used
    boolean disable_cache_on_query; // Disable Caching if query string is present - B1750
    boolean validate_with_date_hdr; // Validate with Date Header if Last-Modified is not present
    int modify_date_header;
    int ignore_s_maxage;
    //int allow_cache_revalidate; - deprecated. Use cache_reval_policies_t instead
    int store_cache_min_age;
    int offline_om_fetch_filesize; // > results in offline OM fetch
    int offline_om_smooth_flow;
    off_t store_cache_min_size;
    int convert_head;
    int is_set_cookie_cacheable;
    int cache_cookie_noreval;
    cache_age_policies_t cache_age_policies;
    int is_host_header_inheritable;
    int client_req_max_age;
    boolean client_req_serve_from_origin;
    cache_fill_t cache_fill;
    int reval_type;
    time_t reval_barrier;
    int reval_trigger;
    int delivery_expired;
    ingest_policy_t ingest_policy;
    int client_driven_agg_threshold;
    boolean eod_on_origin_close;
    cache_redirect_t cache_redirect;
    boolean redirect_302; // Enable/Disable OM handling of 302 redirects
    boolean flush_ic_cache; // Add a Cache-Contro: max-age=0 in the outbound reval request
    int uri_depth_threshold;
    cache_reval_policies_t  cache_revalidate;
    int cache_min_size;
    int cache_max_size;
    boolean ingest_only_in_ram; // CL7 internal use
    int object_correlation_etag_ignore;
    int object_correlation_validatorsall_ignore;
    cache_ingest_threshold_t cache_ingest;
    int client_req_connect_handle;
    char *host_hdr_value ;
    int bulk_fetch;
    int bulk_fetch_pages;
} origin_server_policies_t;

typedef struct dynamic_uri_remap {
    char *regex;
    char *mapping_string;
    char *tokenize_string;
    nkn_regex_ctx_t *regctx;
    boolean tunnel_no_match;
} dynamic_uri_remap_t;

typedef struct seek_uri_remap {
    char *regex;
    char *mapping_string;
    nkn_regex_ctx_t *regctx;
    boolean seek_uri_enable;
} seek_uri_remap_t;

typedef struct nkn_uri_token {
    uint16_t token_flag;
    uint64_t flvseek_byte_start;
    uint64_t http_range_start;
    uint64_t http_range_end;
} nkn_uri_token_t;

typedef struct request_policies {
    boolean strip_query; // Strip query string - B1750
    boolean tunnel_req_with_cookie; // true: will tunnel the request.

    // Socket connect and read timeout configurables
    int connect_timer_msecs;
    int connect_retry_delay_msecs;
    int read_timer_msecs;
    int read_retry_delay_msecs;

    boolean tunnel_all; // BZ 6682: tunnel all client requests

    dynamic_uri_remap_t dynamic_uri;
    seek_uri_remap_t seek_uri;
    boolean passthrough_http_head; // Internal use only, no CLI interface
    boolean http_head_to_get;
    int num_accept_headers;
    header_descriptor_t *accept_headers;
    boolean dns_resp_mismatch;
} request_policies_t;

typedef struct response_policies {
	int dscp;	
} response_policies_t;

/* policy engine*/
/*
#define NKN_MAX_PE_SRVR_HST     8

typedef enum policy_srvr_proto {
	ICAP,

	OUT_OF_BOUNDS,
}policy_srvr_proto_t;

typedef enum policy_srvr_callouts {
	CLIENT_REQ,
	CLIENT_RSP,
	ORIGIN_REQ,
	ORIGIN_RSP,

	MAX_CALLOUTS,
}policy_srvr_callouts_t;


typedef enum policy_failover_type {
	BYPASS,
	REJECT,

}policy_failover_type_t;

typedef struct policy_ep_st {
	char *domain;
	uint16_t port;
} policy_ep_t;


typedef struct policy_srvr_st {
	char *name;
	char *uri;
	boolean enable;
	policy_srvr_proto_t proto;
	policy_ep_t endpoint[NKN_MAX_PE_SRVR_HST];
} policy_srvr_t;


typedef struct policy_srvr_list_s {
	policy_srvr_t   *policy_srvr;
	boolean callouts[MAX_CALLOUTS];
	policy_failover_type_t fail_over;
	uint16_t precedence;
	struct policy_srvr_list_s *nxt;
} policy_srvr_list_t;
*/

typedef struct policy_engine_config {
    char		*policy_file;
    //policy_srvr_list_t	*policy_server_list;
    int			update;
    void		*policy_data;
} policy_engine_config_t;

typedef enum
{
    GEOIP_NONE =0,
    GEOIP_MAXMIND =1,
    GEOIP_QUOVA =2
} geo_server_type;

typedef struct geo_ip_st
{
    geo_server_type server;
    char *api_url;
    boolean failover_bypass;
    uint32_t lookup_timeout;
} geo_ip_config_t;


#define MAX_HEADER_NAMES	(8)
typedef struct cache_index_policies {
    char *ldn_name;
    boolean exclude_domain;
    boolean include_header;
    boolean include_object;
    uint32_t checksum_bytes;
    uint32_t checksum_offset;
    boolean checksum_from_end;
    int headers_count;
    char *include_header_names[MAX_HEADER_NAMES];
} cache_index_policies_t;

enum delivery_protocol { // power of 2 values
	DP_HTTP = 1,
	DP_RTMP = 2,
	DP_RTSP = 4,
	DP_RTP = 8,
	DP_RTSTREAM = 16
};
typedef enum delivery_protocol delivery_protocol_t;

#define DP_CLIENT_RES_SECURE 		0x1
#define DP_CLIENT_RES_NON_SECURE	0x2

#define DP_ORIGIN_REQ_SECURE 		0x1
#define DP_ORIGIN_REQ_NON_SECURE	0x2

typedef struct vary_hdr_config {
    int enable;
    char *user_agent_regex[MAX_USER_AGENT_GROUPS];
    nkn_regex_ctx_t *user_agent_regex_ctx[MAX_USER_AGENT_GROUPS];
} vary_hdr_t;

typedef struct http_config {
    origin_server_t		origin;
    origin_server_policies_t	policies;
    request_policies_t		request_policies;
    response_policies_t		response_policies;
    int				num_delete_response_headers;
    header_descriptor_t		*delete_response_headers;
    int				num_add_response_headers;
    header_descriptor_t		*add_response_headers;
    delivery_protocol_t		response_protocol;
    int 			client_delivery_type;
    int 			origin_request_type;
    uint32_t			max_sessions;
    uint32_t			retry_after_time;
    policy_engine_config_t      policy_engine_config;
    cache_index_policies_t	cache_index;
    geo_ip_config_t		geo_ip_cfg;
    vary_hdr_t			vary_hdr_cfg;
} http_config_t;

typedef struct cluster_hash_config {
    boolean	ch_uri_base; // ClusterHash(basename(uri))
} cluster_hash_config_t;


typedef struct rtsp_config {
    origin_server_policies_t	policies;
    request_policies_t		request_policies;
    uint32_t			max_sessions;
    uint32_t			retry_after_time;
    cache_index_policies_t	cache_index;
} rtsp_config_t;


typedef enum {
	PS_MODE_UNDEFINED = -1,
	PS_MODE_PUSH = 0,
	PS_MODE_PULL,
	PS_MODE_TIMED,
	PS_MODE_MAX_ENTRIES
} pub_server_mode_t;

typedef enum {
    PP_CACHE_FORMAT_UNDEFINED = 0,
    PP_CACHE_FORMAT_CHUNKED_TS,
    PP_CACHE_FORMAT_MOOF,
    PP_CACHE_FORMAT_FRAG_MP4,
    PP_CACHE_FORMAT_CHUNK_FLV,

    PP_CACHE_FORMAT_MAX_ENTRIES
} pub_point_cache_format_t;

typedef struct pub_point_st {
    char		*name;
    boolean		enable;
    pub_server_mode_t	mode;
    char		*sdp_static;
    char		*start_time;
    char		*end_time;
    boolean		cacheable;
    pub_point_cache_format_t cache_format;

} pub_point_t;

typedef struct pub_point_config_st {
    int entries;
    pub_point_t  **events;
} pub_point_config_t;

typedef enum ns_stat_type {
    GET_REQS = 0,
    PUT_REQS,
    OTHER_REQS,
    MAX_STATS
} ns_stat_type_t;

typedef enum {
    DM_TIER_1 = 0,
    DM_TIER_2,
    DM_TIER_3,
    MAX_DM_TIERS
} tier_t;

typedef struct dm_tier_stats_s {
    AO_t block_size;
    AO_t block_used;
    AO_t page_size;
    AO_t page_used;

} dm_tier_stats_t;

typedef struct cmn_server_stats_s {
    AO_t requests;
    AO_t responses;
    AO_t in_bytes;
    AO_t out_bytes;
    AO_t conns;
    AO_t active_conns;
    AO_t idle_conns;
    AO_t resp_2xx;
    AO_t resp_3xx;
    AO_t resp_4xx;
    AO_t resp_5xx;
    AO_t nonbyterange_cnt;
    AO_t compress_cnt;
    AO_t compress_bytes_save;
    AO_t compress_bytes_overrun;
} cmn_server_stats_t;

typedef struct cmn_client_stats_s {
    AO_t requests;
    AO_t responses;
    AO_t cache_hits;
    AO_t cache_miss;
    AO_t cache_partial;
    AO_t in_bytes;
    AO_t out_bytes;
    AO_t out_bytes_origin;
    AO_t out_bytes_disk;
    AO_t out_bytes_ram;
    AO_t out_bytes_tunnel;
    AO_t conns;
    AO_t active_conns;
    AO_t idle_conns;
    AO_t resp_2xx;
    AO_t resp_200;
    AO_t resp_206;
    AO_t resp_3xx;
    AO_t resp_302;
    AO_t resp_304;
    AO_t resp_4xx;
    AO_t resp_404;
    AO_t resp_5xx;
    AO_t compress_cnt;
    AO_t compress_bytes_save;
    AO_t compress_bytes_overrun;
    AO_t expired_objects;
} cmn_client_stats_t;

typedef struct uri_filter_stats_s {
    AO_t rej_cnt;
    AO_t acp_cnt;
    AO_t bw;
} uri_filter_stats_t;

typedef enum {
	PROTO_HTTP = 0,
	PROTO_RTSP,

	MAX_PROTOS
} protocol_t;

typedef struct namespace_stat {
    struct namespace_stat *next;
    struct namespace_stat *prev;
    char *name;
    int refcnt;
    uint64_t stats[MAX_STATS];

    AO_t g_http_sessions;
    AO_t g_rtsp_sessions;
    AO_t g_tunnels;

    cmn_client_stats_t	client[MAX_PROTOS];
    cmn_server_stats_t	server[MAX_PROTOS];
    dm_tier_stats_t     dm_tier[MAX_DM_TIERS];
    uri_filter_stats_t  urif_stats;

} namespace_stat_t;

#define NS_UPDATE_STATS(nsc, proto, type, name, val) \
    do {\
	if ((NULL != ((namespace_config_t *)nsc)) && (NULL != ((namespace_config_t*)nsc)->stat)) {\
	    AO_fetch_and_add(&( ((namespace_config_t *)nsc)->stat->type[proto].name), val);\
	}\
    } while(0)

#define NS_INCR_STATS(nsc, proto, type, name) \
    do {\
	if ((NULL != ((namespace_config_t*)nsc)) && (NULL != ((namespace_config_t*)nsc)->stat)) {\
	    AO_fetch_and_add1(&(((namespace_config_t *)nsc)->stat->type[proto].name));\
	}\
    } while(0)

#define NS_DECR_STATS(nsc, proto, type, name) \
   do {\
	if ((NULL != ((namespace_config_t*)nsc)) && (NULL != ((namespace_config_t*)nsc)->stat)) {\
	    if ((((namespace_config_t *)nsc)->stat->type[proto].name) != 0) \
		AO_fetch_and_sub1(&(((namespace_config_t *)nsc)->stat->type[proto].name));\
	}\
   } while(0)

/*
 *******************************************************************************
 * Cluster configuration descriptor
 *******************************************************************************
 */
typedef enum {
    CLT_NONE=0,
    CLT_CH_REDIRECT,
    CLT_CH_REDIRECT_LB,
    CLT_CH_PROXY,
    CLT_CH_PROXY_LB,
    CLT_CACHE_CLUSTER
} cluster_type_t;

typedef struct cluster_req_intercept_attrs {
    uint32_t ip; // Host byte order
    uint32_t port; // Host byte order

/* Intercept namespace determined via client request to namespace interface */
#define L7_NOT_INTERCEPT_IP 0xffffffff
#define L7_NOT_INTERCEPT_PORT 0xffff
} cl_req_intercept_attr_t;

typedef struct cluster_redirect_attrs {
    boolean allow_redir_local;
    const char *path_prefix;
    int path_prefix_strlen;
} cl_redir_attrs_t;

typedef enum {
    CL_REDIR_LB_REP_NONE=0,
    CL_REDIR_LB_REP_LEAST_LOADED,
    CL_REDIR_LB_REP_RANDOM
} cl_redir_lb_replicate_t;

typedef struct cluster_redirect_lb_attrs {
    cl_redir_lb_replicate_t action;
    int load_metric_threshold;
} cl_redir_lb_attr_t;

#define CH_REDIRECT_PROXY \
	cl_req_intercept_attr_t intercept_attr; \
	struct node_map_descriptor *node_map; \
	cluster_hash_config_t ch_attr; \
	cl_redir_attrs_t redir_attr;

#define CH_REDIRECT_PROXY_LB \
	cl_req_intercept_attr_t intercept_attr; \
	struct node_map_descriptor *node_map; \
	cluster_hash_config_t ch_attr; \
	cl_redir_attrs_t redir_attr; \
	cl_redir_lb_attr_t lb_attr; \

typedef struct cluster_descriptor {
    int *node_id_to_ns_index;
    nkn_trie_t *suffix_map;
    cluster_type_t type;
    union { // discriminated by "type"
        struct ch_redirect_st {
	    CH_REDIRECT_PROXY
	} ch_redirect;

        struct ch_redirect_lb_st {
	    CH_REDIRECT_PROXY_LB
	} ch_redirect_lb;

	struct ch_proxy_st {
	    CH_REDIRECT_PROXY
	} ch_proxy;

	struct ch_proxy_lb_st {
	    CH_REDIRECT_PROXY_LB
	} ch_proxy_lb;

	struct cache_cluster {
	} cache_cluster;
    } u;
} cluster_descriptor_t;

#define CLUSTER_NSC_INSTANCE_BASE 1024

#define CLD2NODE_MAP(_cld, _nmap) \
{ \
    switch((_cld)->type) { \
    case CLT_CH_REDIRECT: \
        (_nmap) = (_cld)->u.ch_redirect.node_map; \
	break; \
    case CLT_CH_REDIRECT_LB: \
        (_nmap) = (_cld)->u.ch_redirect_lb.node_map; \
	break; \
    case CLT_CH_PROXY: \
        (_nmap) = (_cld)->u.ch_proxy.node_map; \
	break; \
    case CLT_CH_PROXY_LB: \
        (_nmap) = (_cld)->u.ch_proxy_lb.node_map; \
	break; \
    default: \
        (_nmap) = 0; \
	break; \
    } \
}

#define CLD2INTR_ATTR(_cld, _intr_attr) \
{ \
    switch((_cld)->type) { \
    case CLT_CH_REDIRECT: \
        (_intr_attr) = &(_cld)->u.ch_redirect.intercept_attr; \
	break; \
    case CLT_CH_REDIRECT_LB: \
        (_intr_attr) = &(_cld)->u.ch_redirect_lb.intercept_attr; \
	break; \
    case CLT_CH_PROXY: \
        (_intr_attr) = &(_cld)->u.ch_proxy.intercept_attr; \
	break; \
    case CLT_CH_PROXY_LB: \
        (_intr_attr) = &(_cld)->u.ch_proxy_lb.intercept_attr; \
	break; \
    default: \
        (_intr_attr) = 0; \
	break; \
    } \
}

#define CLD2REDIR_ATTR(_cld, _redir_attr) \
{ \
    switch((_cld)->type) { \
    case CLT_CH_REDIRECT: \
        (_redir_attr) = &(_cld)->u.ch_redirect.redir_attr; \
	break; \
    case CLT_CH_REDIRECT_LB: \
        (_redir_attr) = &(_cld)->u.ch_redirect_lb.redir_attr; \
	break; \
    case CLT_CH_PROXY: \
        (_redir_attr) = &(_cld)->u.ch_proxy.redir_attr; \
	break; \
    case CLT_CH_PROXY_LB: \
        (_redir_attr) = &(_cld)->u.ch_proxy_lb.redir_attr; \
	break; \
    default: \
        (_redir_attr) = 0; \
	break; \
    } \
}

#define CLD2INTERCEPT_TYPE(_cld, _type) \
{ \
    switch((_cld)->type) { \
    case CLT_CH_REDIRECT: \
    case CLT_CH_REDIRECT_LB: \
        (_type) = INTERCEPT_REDIRECT; \
	break; \
    case CLT_CH_PROXY: \
    case CLT_CH_PROXY_LB: \
        (_type) = INTERCEPT_PROXY; \
	break; \
    default: \
        (_type) = INTERCEPT_NONE; \
	break; \
    } \
}

typedef struct cluster_config {
    int num_cld;
    cluster_descriptor_t **cld; // 'num_cld' denotes size
} cluster_config_t;

/*
 *******************************************************************************
** NS_IP_bind configuration 
 *******************************************************************************
*/

typedef struct ns_ip_bind {
    struct ip_list {
        unsigned int ipaddr[4];
        int8_t ip_type;
    } ip_addr[NKN_MAX_VIPS];
    int n_ips;
} ns_ip_bind_t;



/*
 *******************************************************************************
 * Compression configuration
 *******************************************************************************
 */
typedef struct file_types {
	char *type;
	int type_len;
	int allowed;
}file_types_t;
typedef struct compress_config {

    // enable/disable Object compression for this namespace
    boolean	enable;
    // maximum object-size (bytes) to allow for compression
    uint64_t	max_obj_size;
    // mimimum object-size (bytes) to allow for compression
    uint64_t	min_obj_size;
    // type of compression algorithm
    uint32_t	algorithm;
    file_types_t file_types[MAX_NS_COMPRESS_FILETYPES];

} compress_config_t;

typedef struct unsuccess_response_cache_config {
    boolean status;
    uint32_t code;
    uint32_t age;
} unsuccess_response_cache_cfg_t;

/*
 *******************************************************************************
 * URL Filter configuration
 *******************************************************************************
 */
typedef enum {
	NS_UF_MAP_TYPE_UNDEF = 0,
	NS_UF_MAP_TYPE_URL,
	NS_UF_MAP_TYPE_IWF,
	NS_UF_MAP_TYPE_CALEA
} namespace_uf_map_t;

typedef enum {
	NS_UF_REJECT_UNDEF = 0,
	NS_UF_REJECT_NOACTION,
	NS_UF_REJECT_RESET,
	NS_UF_REJECT_CLOSE,
	NS_UF_REJECT_HTTP_403,
	NS_UF_REJECT_HTTP_404,
	NS_UF_REJECT_HTTP_301,
	NS_UF_REJECT_HTTP_302,
	NS_UF_REJECT_HTTP_200,
	NS_UF_REJECT_HTTP_CUSTOM,
	NS_UF_REJECT_DROP_PKTS,
	NS_UF_URI_SIZE_REJECT_RESET,
	NS_UF_URI_SIZE_REJECT_CLOSE,
	NS_UF_URI_SIZE_REJECT_HTTP_403,
	NS_UF_URI_SIZE_REJECT_HTTP_404,
	NS_UF_URI_SIZE_REJECT_HTTP_301,
	NS_UF_URI_SIZE_REJECT_HTTP_302,
	NS_UF_URI_SIZE_REJECT_HTTP_200
} namespace_uf_reject_t;

typedef struct url_filter_trie {
	AO_t 	   refcnt;
	uint64_t   magicno;
    	nkn_trie_t *trie;
} url_filter_trie_t;

#define UF_TRIE_DATA_MAGIC 0x201404301234abcd

typedef struct url_filter_config {
    namespace_uf_map_t		uf_map_type;
    url_filter_trie_t		*uf_trie;
    int 			uf_is_black_list;
    namespace_uf_reject_t 	uf_reject_action;
    uint32_t                    uf_max_uri_size;
    namespace_uf_reject_t 	uf_uri_size_reject_action;
    union {
    	struct reject_reset {
	} reset;
    	struct reject_close {
	} close;
    	struct reject_http_403 {
	} http_403;
    	struct reject_http_404 {
	} http_404;
    	struct reject_http_301 {
	    const char *redir_host;
	    int redir_host_strlen;
	    const char *redir_url;
	    int redir_url_strlen;
	} http_301;
    	struct reject_http_302 {
	    const char *redir_host;
	    int redir_host_strlen;
	    const char *redir_url;
	    int redir_url_strlen;
	} http_302;
    	struct reject_http_200 {
	    const char *response_body;
	    int response_body_strlen;
	} http_200;
    	struct reject_http_custom {
	    int http_response_code;
	    const char *response_body;
	    int response_body_strlen;
	} http_custom;
    	struct reject_drop_pkts {
	} drop_pkts;
    } uf_reject, uf_uri_size_reject;
} url_filter_config_t;

/*
 *******************************************************************************
 * Cache Pinning configuration
 *******************************************************************************
 */
typedef struct cache_pin_config {

    // enable/disable cache pinning for this namespace
    boolean	enable;
    // Pin all objects
    boolean    cache_auto_pin;
    // maximum cache-size (bytes) to allow for pinned objects
    uint64_t	cache_resv_bytes;
    // per object size (bytes) limit for pinning
    uint32_t	max_obj_bytes;
    // header name in response to indicate if object can be pinned or not
    char	*pin_header;
    // pin header length
    uint32_t	pin_header_len;
    // header name in response to indicate end time when object can be unpinned
    char	*end_header;
    // end header length
    uint32_t	end_header_len;
    // header name in response to indicate start time
    // when object is deemed valid and can be served
    // to clients
    char	*start_header;
    // start header length
    uint32_t	start_header_len;

} cache_pin_config_t;


struct ns_accesslog_config {
    char    *name;
    char    *key;
    uint64_t keyhash;
    int32_t al_resp_header_configured;
    int32_t al_record_cache_history;
    int32_t al_resp_hdr_md5_chksum_configured;
};
/*
 *******************************************************************************
 * Namespace configuration
 *******************************************************************************
 */
#if 1 // Remove after nvsd_mgmt_namespace.c cleanup
#define PROXY_REVERSE	0
#define PROXY_FORWARD	1	// Same as forward proxy
#define PROXY_MID_TIER	PROXY_FORWARD	// Same as forward proxy
#define PROXY_TRANSPARENT 2
#define PROXY_VIRTUAL_DOMAIN 3
#endif

#define NS_INIT_WAIT_INTERVAL 500 // msecs
#define NS_INIT_WAIT_MAX_INTERVALS 6 //

#define NS_REVAL_ALWAYS_TIME_BARRIER 2541441258

typedef enum {
	NSM_UNDEFINED = 0,
	NSM_URI_PREFIX,
	NSM_URI_PREFIX_REGEX,
	NSM_HEADER_VALUE,
	NSM_HEADER_VALUE_REGEX,
	NSM_QUERY_NAME_VALUE,
	NSM_QUERY_NAME_VALUE_REGEX,
	NSM_VIRTUAL_IP_PORT,
	NSM_RTSP_URI_PREFIX,
	NSM_RTSP_URI_PREFIX_REGEX,
	NSM_RTSP_VIRTUAL_IP_PORT
} namespace_select_method_t;

typedef struct namespace_config {
    AO_t init_in_progress; /* namespace initialization status */
    unsigned int tproxy_ipaddr;
    int proxy_mode;
    char *hostname[16];
    int hostname_cnt;
    char *hostname_regex;
    nkn_regex_ctx_t *hostname_regex_ctx;
    unsigned int hostname_regex_tot_cnt;
    char *regex_int_hostname;
    /*to indicate tunnel reason codes which need to be cached*/
    uint64_t tunnel_req_override;
    uint64_t tunnel_res_override;

    /*
     **********************
     * Selection criteria *
     **********************
     */
    namespace_select_method_t select_method;
    union { // discriminated by "select_method"
	struct s_sm_uri_prefix {
	    char *uri_prefix;
	} up;
	struct s_sm_uri_prefix_regex {
	    char *uri_prefix_regex;
	    nkn_regex_ctx_t *uri_prefix_regex_ctx;
    	    unsigned int uri_prefix_regex_tot_cnt;
	} upr;
	struct s_sm_header_value {
	    char *header_name;
	    char *header_value;
	} hv;
	struct s_sm_header_value_regex {
	    char *header_name_value_regex;
	    nkn_regex_ctx_t *header_name_value_regex_ctx;
    	    unsigned int header_name_value_regex_tot_cnt;
	} hvr;
	struct s_sm_query_name_value {
	    char *query_string_nv_name;
	    char *query_string_nv_value;
	} qnv;
	struct s_sm_query_name_value_regex {
	    char *query_string_regex;
	    nkn_regex_ctx_t *query_string_regex_ctx;
    	    unsigned int query_string_regex_tot_cnt;
	} qnvr;
	struct s_sm_virtual_ip_port {
	    uint32_t virtual_host_ip; // Network byte order
	    uint32_t virtual_host_port; // Network byte order
	} vip;
    } sm;

    char *namespace;
    int namespace_strlen;
    char *ns_uid;
    int ns_uid_strlen;
    ssp_config_t *ssp_config;
    http_config_t *http_config;
    namespace_stat_t *stat;
    pub_point_config_t	  *pub_point_config;


    namespace_select_method_t rtsp_select_method;
    union { // discriminated by "select_method"
	struct s_sm_uri_prefix up;
	struct s_sm_uri_prefix_regex  upr;
	struct s_sm_virtual_ip_port  vip;
    } rtsp_sm;
    rtsp_config_t	*rtsp_config;
    cluster_config_t	*cluster_config;
    cache_pin_config_t	*cache_pin_config;
    // Index to get to resource pool
    uint32_t		rp_index;

    ns_ip_bind_t ip_bind_list;

    // Accesslog profile for this namespace
    struct ns_accesslog_config    *acclog_config;
    compress_config_t	*compress_config;
    int md5_checksum_len;  /* Length of content in bytes to be used from response data
                            * for calculating MD5 checksum.
                            */
    unsuccess_response_cache_cfg_t *non2xx_cache_cfg;
    url_filter_config_t *uf_config;
} namespace_config_t;

// Note: Must follow namespace_config_t typedef
#include "nkn_namespace_nodemap.h"

// System initalization
int nkn_init_namespace(int (*is_local_ip)(const char *ip, int ip_strlen));

// Trigger immediate namespace configuration update
void immediate_namespace_update(void);

// TM inteface to signal updates
void nkn_namespace_config_update(void *arg);

// Acquire HTTP namespace token interfaces
//  Return:
//    1) namespace_token_t
//    2) namespace_config_t **ppnsc is optional, set if token is valid and
//	 (ppnsc != NULL)
//
namespace_token_t http_request_to_namespace(mime_header_t *hdr, 
					    const namespace_config_t **ppnsc);
// Acquire RTSP namespace token interfaces
//  Return:
//    1) namespace_token_t
//
namespace_token_t rtsp_request_to_namespace(mime_header_t *hdr);

// Release namespace token acquired via xxxx_request_to_namespace()
void release_namespace_token_t(namespace_token_t token);

// Determine request intercept action

typedef enum intercept_type {
    INTERCEPT_NONE=0,
    INTERCEPT_ERR,
    INTERCEPT_RETRY,
    INTERCEPT_REDIRECT,
    INTERCEPT_PROXY
} intercept_type_t;

typedef struct suffix_map_trie_node {
    int  magicno;
#define SFXMAP_TNODE_MAGIC 0xfedcba98
    intercept_type_t type;
    char pad[24];
} suffix_map_trie_node_t;

// Note: 
//	1) Caller must release ns_token reference via 
//	   release_namespace_token_t() when the return value != INTERCEPT_NONE.
//	2) Caller must release node_ns_token reference via 
//	   release_namespace_token_t() when the return value is either
//	   INTERCEPT_REDIRECT or INTERCEPT_PROXY.
//
intercept_type_t
namespace_request_intercept(int sock_fd, // (I), 
			    mime_header_t *hdr, // (I)
			    uint32_t intf_dest_ip, // (I) Host byte order
                            uint32_t intf_dest_port, // (I) Host byte order
			    char **target_host, // (O)
			    int *target_host_len, // (O)
			    uint16_t *target_port, // (O) Host byte order
			    const cl_redir_attrs_t **rdp, // (O)
			    char **ns_name, // (O)
			    namespace_token_t *ns_token, // (O)
			    namespace_token_t *node_ns_token, // (O)
			    cluster_descriptor_t **pcd); // (O)

// Note: Caller must release token reference via release_namespace_token_t()
const namespace_config_t *namespace_to_config(namespace_token_t token);

char *ns_uri2uri(namespace_token_t ns_token, char *ns_uri, int *result_strlen);
namespace_token_t uol_to_nstoken(nkn_uol_t *uol);
namespace_token_t cachekey_to_nstoken(char *cachekey);

// Decompose namespace URI '/namespace:uuid_host:port/uri'
// into defined components.
//
// ns_uri is NULL terminated.
// Return: 0 => Success
int decompose_ns_uri(const char *ns_uri,
		     const char **namespace, int *namespace_len,
		     const char **uid, int *uid_len,
		     const char **host, int *host_len,
		     const char **port, int *port_len,
		     const char **uri);

int decompose_ns_uri_dir(const char *ns_uri,
			 const char **namespace, int *namespace_len,
			 const char **uid, int *uid_len,
			 const char **host, int *host_len,
			 const char **port, int *port_len);

// Create uol.uri given request URI and namespace token
// Caller must free() returned data
char *nstoken_to_uol_uri(namespace_token_t token, const char *uri,
			 int urilen, const char * domain,
			 int domainlen, size_t *cachelength);

int nstoken_to_uol_uri_stack(namespace_token_t token, const char *uri,
			     int urilen, const char * host, int hostlen,
			     char *stack, int stackbufsz,
			     const namespace_config_t *pnsc,
			     size_t *cachelength);

#define NAMESPACE_WORTHY_CHECK_INGNORE_SIZE 1
int nkn_is_object_cache_worthy(nkn_attr_t *attr, namespace_token_t ns_token,
				int flag);

ingest_policy_t nkn_get_namespace_ingest_policy(namespace_token_t ns_token);

ingest_policy_t nkn_get_proxy_mode_ingest_policy(namespace_token_t ns_token);

int nkn_get_namespace_client_driven_aggr_threshold(namespace_token_t ns_token);

uint32_t nkn_get_namespace_cache_ingest_hotness_threshold(namespace_token_t ns_token);

uint32_t nkn_get_namespace_cache_ingest_size_threshold(
						namespace_token_t ns_token);

/*
 * request_to_origin_server()
 *	Given the caller context and either the HTTP request headers
 *	or the client_req_ip_port and the corresponding namespace_config_t
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
typedef enum req2os_context {
    REQ2OS_UNDEFINED = 0,
    REQ2OS_CACHE_KEY,
    REQ2OS_ORIGIN_SERVER,
    REQ2OS_ORIGIN_SERVER_LOOKUP,
    REQ2OS_SET_ORIGIN_SERVER,
    REQ2OS_TPROXY_ORIGIN_SERVER_LOOKUP,
} req2os_context_t;

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
			int append_port_to_ip);
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
				const namespace_config_t *nsc);

/*
 * Enum for the different parts of the cache prefix string
 */
typedef enum  en_cp_part_type {
	CP_PART_NAMESPACE = 0,
	CP_PART_UUID,
	CP_PART_SERVER,
	CP_PART_PORT
} en_cp_part_type;

/*
 * get_active_namespace_uids()
 *  Return the ns_uid(s) associated with the active namespaces.
 *
 *  Returns: ns_active_uid_list_t which must be free'ed by the caller.
 */
typedef struct ns_active_uid_list {
    int num_entries;
    const char *ns_uid_list[1];
} ns_active_uid_list_t;

ns_active_uid_list_t * get_active_namespace_uids(void);
void free_active_uid_list(ns_active_uid_list_t *list);
char* get_part_from_cache_prefix(const char* cp_cache_prefix,
		en_cp_part_type part_type, uint32_t cache_prefix_len);
/*
 * nkn_add_ns_status_callback() - Register callback for namespace
 *				  online/offline notification
 *
 *	Returns: 0 => Success
 */

typedef int ns_calback_proc_t(namespace_token_t token,
			      const namespace_config_t *nsc, int online);

int nkn_add_ns_status_callback(ns_calback_proc_t *proc);

/*
 * get_active_namespace_token_ctx() - Get the namespace_token_context_t
 *				associated with the active namespaces.
 *
 *	Returns: active number namespaces + namespace_token_context_t
 */
int get_active_namespace_token_ctx(namespace_token_context_t *ctx);

/*
 * get_nth_active_namespace_token() - Get the nth active  namespace_token_t
 *				given namespace_token_context_t.
 */
namespace_token_t
get_nth_active_namespace_token(namespace_token_context_t *ctx, int nth);

void
nkn_get_namespace_cache_limitation(namespace_token_t ns_token,
				   off_t *cache_min_size,
				   int *uri_min_depth,
				   int *cache_age_threshold,
				   uint64_t *cache_ingest_size_threshold,
				   int *cache_ingest_hotness_threshold,
				   int *cache_fill_policy);

uint32_t nkn_get_namespace_auto_pin_config(namespace_token_t ns_token);

int nkn_namespace_use_client_ip(const namespace_config_t *nsc);
int nkn_http_is_transparent_proxy(const namespace_config_t *nsc);

#endif  /* _NKN_NAMESPACE_H */

/*
 * End of nkn_namespace.h
 */
