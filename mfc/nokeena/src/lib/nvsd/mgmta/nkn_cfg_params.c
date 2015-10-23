#include <stdlib.h>
#include <stdlib.h>
#include <pthread.h>
#include "nkn_mgmt_agent.h"
#include "nkn_mgmt_smi.h"
#include "nkn_cfg_params.h"
#include "nkn_http.h"
#include "nkn_defs.h"
#include "om_port_mapper.h"
#include "nvsd_mgmt_namespace.h"



NknCfgParamValue nknCfgParams[] = 
{
{ { "vfs.dm2.enable",  NKN_INT_TYPE }, & vfs_dm2_enable },
{ { "rtsp.enable",  NKN_INT_TYPE }, & rtsp_enable },
{ { "rtsp.share_library",  NKN_FUNC_TYPE }, & rtsp_cfg_so_callback },
{ { "fuse.enable",  NKN_INT_TYPE }, & fuse_enable },
{ { "nfs.numthreads",  NKN_INT_TYPE }, & nfs_numthreads },
{ { "forwardproxy.enable",  NKN_INT_TYPE }, & forwardproxy_enable },
{ { "ip_spoofing.enable",  NKN_INT_TYPE }, & ip_spoofing_enable },
{ { "rtsp_relay_only.enable",  NKN_INT_TYPE }, & rtsp_relay_only_enable },
{ { "om.req_collison_delay_msecs", NKN_INT_TYPE }, & om_req_collison_delay_msecs },
{ { "om.req_behind_delay_msecs",  NKN_INT_TYPE }, & om_req_behind_delay_msecs },
{ { "om.req_ahead_delay_msecs",  NKN_INT_TYPE }, & om_req_ahead_delay_msecs },
{ { "om.tproxy_buf_pages_per_task", NKN_INT_TYPE }, &om_tproxy_buf_pages_per_task},
{ { "ip_spoofing.enable",  NKN_INT_TYPE }, & ip_spoofing_enable },
{ { "video_ankeena_fmt.enable",  NKN_INT_TYPE }, & video_ankeena_fmt_enable },
{ { "rtsp_tproxy.enable",  NKN_INT_TYPE }, & rtsp_tproxy_enable },
{ { "rtsched.enable",  NKN_INT_TYPE }, & rtsched_enable},
{ { "adns.enable",  NKN_INT_TYPE }, & adns_enabled},
{ { "adns.cache-timeout",  NKN_INT_TYPE }, &glob_adns_cache_timeout},
{ { "adns.adnsd-enabled",  NKN_INT_TYPE }, &adnsd_enabled},
{ { "bind_socket_with_interface.enable",  NKN_INT_TYPE }, &bind_socket_with_interface},
{ { "max_virtual_memory", NKN_LONG_TYPE }, &max_virtual_memory},
{ { "bm.cr_req_queue_timeout", NKN_LONG_TYPE }, &bm_cfg_cr_max_queue_time},
{ { "bm.cr_req_queue_maxreq", NKN_LONG_TYPE }, &bm_cfg_cr_max_req},
{ { "http_cfg_fo_use_dns",  NKN_INT_TYPE }, & http_cfg_fo_use_dns},
{ { "http_cfg_origin_conn_timeout",  NKN_INT_TYPE }, & http_cfg_origin_conn_timeout},
{ { "bm.discardenable", NKN_INT_TYPE }, &bm_cfg_discard},
{ { "bm.dynscaleenable", NKN_INT_TYPE }, &bm_cfg_dynscale},
{ { "bm.valgrind", NKN_INT_TYPE }, &bm_cfg_valgrind},
{ { "ingest.failure.log.enable", NKN_INT_TYPE }, &glob_log_cache_ingest_failure},
{ { "debug_fd_trace", NKN_INT_TYPE }, &debug_fd_trace},
{ { "pmmaper_disable", NKN_INT_TYPE }, &om_pmap_config.pmapper_disable},
{ { "nm_handle_send_and_receive", NKN_INT_TYPE }, &nm_hdl_send_and_receive},
{ { "pe_url_category_lookup.enable", NKN_INT_TYPE }, &pe_url_category_lookup},
{ { "pe_url_cat_failover_bypass.enable", NKN_INT_TYPE }, &pe_ucflt_failover_bypass_enable},


////////////////////////////////////////////////////////////////////////////////
// CL7 Proxy namespace configurable options
////////////////////////////////////////////////////////////////////////////////
//    Origin Fetch
{ { "cl7pxyns.origin_fetch.cache_directive_no_cache", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_fetch_cache_directive_no_cache},

{ { "cl7pxyns.origin_fetch.default_cache_age", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_age},

{ { "cl7pxyns.origin_fetch.custom_cache_control", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_fetch_custom_cache_control},

{ { "cl7pxyns.origin_fetch.partial_content", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_partial_content},

{ { "cl7pxyns.origin_fetch.date_header_modify", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_date_header_modify},

{ { "cl7pxyns.origin_fetch.flush_intermediate_caches", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_flush_intermediate_caches},

{ { "cl7pxyns.origin_fetch.content_store_cache_age_threshold", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_content_store_cache_age_threshold},

{ { "cl7pxyns.origin_fetch.content_store_uri_depth_threshold", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_content_store_uri_depth_threshold},

{ { "cl7pxyns.origin_fetch.offline_fetch_size", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_offline_fetch_size},

{ { "cl7pxyns.origin_fetch.offline_fetch_smooth_flow", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_offline_fetch_smooth_flow},

{ { "cl7pxyns.origin_fetch.convert_head", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_convert_head},

{ { "cl7pxyns.origin_fetch.store_cache_min_size", NKN_LONG_TYPE }, 
    &cl7pxyns_origin_fetch_store_cache_min_size},

{ { "cl7pxyns.origin_fetch.is_set_cookie_cacheable", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_is_set_cookie_cacheable},

{ { "cl7pxyns.origin_fetch.is_host_header_inheritable", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_is_host_header_inheritable},

{ { "cl7pxyns.origin_fetch.content_type_count", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_content_type_count},

{ { "cl7pxyns.origin_fetch.cache_age_map_0.content_type", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_fetch_cache_age_map_0_content_type},
{ { "cl7pxyns.origin_fetch.cache_age_map_0.cache_age", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_age_map_0_cache_age},

{ { "cl7pxyns.origin_fetch.cache_age_map_1.content_type", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_fetch_cache_age_map_1_content_type},
{ { "cl7pxyns.origin_fetch.cache_age_map_1.cache_age", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_age_map_1_cache_age},

{ { "cl7pxyns.origin_fetch.cache_age_map_2.content_type", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_fetch_cache_age_map_2_content_type},
{ { "cl7pxyns.origin_fetch.cache_age_map_2.cache_age", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_age_map_2_cache_age},

{ { "cl7pxyns.origin_fetch.cache_age_map_3.content_type", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_fetch_cache_age_map_3_content_type},
{ { "cl7pxyns.origin_fetch.cache_age_map_3.cache_age", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_age_map_3_cache_age},

{ { "cl7pxyns.origin_fetch.cache_age_map_4.content_type", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_fetch_cache_age_map_4_content_type},
{ { "cl7pxyns.origin_fetch.cache_age_map_4.cache_age", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_age_map_4_cache_age},

{ { "cl7pxyns.origin_fetch.cache_barrier_revalidate.reval_time",NKN_LONG_TYPE}, 
    &cl7pxyns_origin_fetch_cache_barrier_revalidate_reval_time},

{ { "cl7pxyns.origin_fetch.cache_barrier_revalidate.reval_type",NKN_INT_TYPE}, 
    &cl7pxyns_origin_fetch_cache_barrier_revalidate_reval_type},

{ { "cl7pxyns.origin_fetch.disable_cache_on_query", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_disable_cache_on_query},

{ { "cl7pxyns.origin_fetch.strip_query", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_strip_query},

{ { "cl7pxyns.origin_fetch.use_date_header", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_use_date_header},

{ { "cl7pxyns.origin_fetch.disable_cache_on_cookie", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_disable_cache_on_cookie},

{ { "cl7pxyns.origin_fetch.client_req_max_age", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_client_req_max_age},

{ { "cl7pxyns.origin_fetch.client_req_serve_from_origin", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_client_req_serve_from_origin},

{ { "cl7pxyns.origin_fetch.cache_fill", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_fill},

{ { "cl7pxyns.origin_fetch.ingest_policy", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_ingest_policy},

{ { "cl7pxyns.origin_fetch.ingest_only_in_ram", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_ingest_only_in_ram},

{ { "cl7pxyns.origin_fetch.eod_on_origin_close", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_eod_on_origin_close},

{ { "cl7pxyns.origin_fetch.object.start_header", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_fetch_object_start_header},

{ { "cl7pxyns.origin_fetch.object.cache_pin.cache_pin_ena", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_object_cache_pin_cache_pin_ena},

{ { "cl7pxyns.origin_fetch.object.cache_pin.cache_auto_pin", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_object_cache_pin_cache_auto_pin},

{ { "cl7pxyns.origin_fetch.object.cache_pin.cache_resv_bytes", NKN_LONG_TYPE }, 
    &cl7pxyns_origin_fetch_object_cache_pin_cache_resv_bytes},

{ { "cl7pxyns.origin_fetch.object.cache_pin.max_obj_bytes", NKN_LONG_TYPE }, 
    &cl7pxyns_origin_fetch_object_cache_pin_max_obj_bytes},

{ { "cl7pxyns.origin_fetch.object.cache_pin.pin_header", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_fetch_object_cache_pin_pin_header},

{ { "cl7pxyns.origin_fetch.object.cache_pin.end_header", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_fetch_object_cache_pin_end_header},

{ { "cl7pxyns.origin_fetch.redirect_302", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_redirect_302},

{ { "cl7pxyns.origin_fetch.tunnel_all", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_tunnel_all},

{ { "cl7pxyns.origin_fetch.object_correlation_etag_ignore", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_object_correlation_etag_ignore},

{ { "cl7pxyns.origin_fetch.object_correlation_validatorsall_ignore", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_object_correlation_validatorsall_ignore},

{ { "cl7pxyns.origin_fetch.client_driven_agg_threshold", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_client_driven_agg_threshold},

{ { "cl7pxyns.origin_fetch.cache_obj_size_min", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_obj_size_min},

{ { "cl7pxyns.origin_fetch.cache_obj_size_max", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_obj_size_max},

{ { "cl7pxyns.origin_fetch.cache_reval.cache_revalidate", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_reval_cache_revalidate},

{ { "cl7pxyns.origin_fetch.cache_reval.method", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_reval_method},

{ { "cl7pxyns.origin_fetch.cache_reval.validate_header", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_reval_validate_header},

{ { "cl7pxyns.origin_fetch.cache_reval.header_name", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_fetch_cache_reval_header_name},

{ { "cl7pxyns.origin_fetch.cache_ingest.hotness_threshold", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_ingest_hotness_threshold},

{ { "cl7pxyns.origin_fetch.cache_ingest.size_threshold", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cache_ingest_size_threshold},

{ { "cl7pxyns.origin_fetch.cacheable_no_revalidation", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_cacheable_no_revalidation},

{ { "cl7pxyns.origin_fetch.client_req_connect_handle", NKN_INT_TYPE }, 
    &cl7pxyns_origin_fetch_client_req_connect_handle},

//    Origin Request

{ { "cl7pxyns.origin_request.add_headers.name_0", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_request_add_headers_name_0},
{ { "cl7pxyns.origin_request.add_headers.value_0", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_request_add_headers_value_0},

{ { "cl7pxyns.origin_request.add_headers.name_1", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_request_add_headers_name_1},
{ { "cl7pxyns.origin_request.add_headers.value_1", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_request_add_headers_value_1},

{ { "cl7pxyns.origin_request.add_headers.name_2", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_request_add_headers_name_2},
{ { "cl7pxyns.origin_request.add_headers.value_2", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_request_add_headers_value_2},

{ { "cl7pxyns.origin_request.add_headers.name_3", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_request_add_headers_name_3},
{ { "cl7pxyns.origin_request.add_headers.value_3", NKN_STR_PTR_TYPE }, 
    &cl7pxyns_origin_request_add_headers_value_3},

{ { "cl7pxyns.origin_request.x_forward_header", NKN_INT_TYPE }, 
    &cl7pxyns_origin_request_x_forward_header},

{ { "cl7pxyns.origin_request.orig_conn_values.conn_timeout", NKN_INT_TYPE }, 
    &cl7pxyns_origin_request_orig_conn_values_conn_timeout},

{ { "cl7pxyns.origin_request.orig_conn_values.conn_retry_delay", NKN_INT_TYPE}, 
    &cl7pxyns_origin_request_orig_conn_values_conn_retry_delay},

{ { "cl7pxyns.origin_request.orig_conn_values.read_timeout", NKN_INT_TYPE}, 
    &cl7pxyns_origin_request_orig_conn_values_read_timeout},

{ { "cl7pxyns.origin_request.orig_conn_values.read_retry_delay", NKN_INT_TYPE}, 
    &cl7pxyns_origin_request_orig_conn_values_read_retry_delay},

////////////////////////////////////////////////////////////////////////////////
// End CL7 Proxy namespace configurable options
////////////////////////////////////////////////////////////////////////////////

{ { "http.include_orig_key", NKN_STR_PTR_TYPE }, &include_orig_key},

// DM2 Config Values
{ { "dm2.num_preread_disk_threads", NKN_INT_TYPE},
    &glob_dm2_num_preread_disk_threads},
{ { "dm2.throttle_writes", NKN_INT_TYPE }, &glob_dm2_throttle_writes},
{ { "dm2.small_write_enable", NKN_INT_TYPE }, &glob_dm2_small_write_enable},
{ { "dm2.small_write_min_size", NKN_INT_TYPE }, &glob_dm2_small_write_min_size},

// AM config values
{ { "am.byte_serve_hotness_enable", NKN_INT_TYPE },
    &glob_am_bytes_based_hotness},
{ { "am.byte_serve_hotness_threshold", NKN_LONG_TYPE },
    &glob_am_bytes_based_hotness_threshold},

// Push Ingest config values
{ { "mm.push_ingest_enabled", NKN_INT_TYPE },
    &glob_am_push_ingest_enabled},
{ { "mm.push_ingest_buffer_ratio", NKN_INT_TYPE },
    &glob_mm_push_ingest_buffer_ratio},
{ { "mm.push_ingest_buffer_hold_timeout", NKN_INT_TYPE },
    &glob_mm_push_entry_info_check_tmout},
{ { "mm.push_ingest_no_parallel_first_put", NKN_INT_TYPE },
    &glob_mm_push_ingest_parallel_ingest_restrict},

// ** Note: Add new entries after this line **

{ { NULL, NKN_INT_TYPE }, NULL },};

uint16_t u16_g_port_mapper_min = D_TP_PORT_RANGE_MIN_DEF;
uint16_t u16_g_port_mapper_max = D_TP_PORT_RANGE_MAX_DEF; // max - min = 32768 ports (default)

long cm_max_cache_size  = 1024;
unsigned int bm_page_ratio = 0;
long nkn_max_dict_mem_MB;
const char * fmgr_queuefile = "/tmp/FMGR.queue";
int fmgr_cache_no_cache_obj  = 0;
long bm_cfg_cr_max_queue_time = 0;
long bm_cfg_cr_max_req = 0;
unsigned int max_unresp_connections = 0;
int http_cfg_fo_use_dns = 0;
int http_cfg_origin_conn_timeout = 10;
int bm_cfg_dynscale = 1;
int bm_cfg_discard = 1;
int bm_cfg_valgrind = 0;
long bm_cfg_buf_lruscale = 3;
int fmgr_queue_maxsize  = 1024;
int http_idle_timeout  = 60;
const char * include_orig_key = "abcde";
int cp_idle_timeout  = 300;
int nkn_max_client  = 5000;
int max_http_req_size  = 16384;
int max_get_rate_limit  = 500000;
int nkn_http_serverport[MAX_PORTLIST];
int nkn_rtsp_vod_serverport[MAX_PORTLIST];
int nkn_rtsp_live_serverport[MAX_PORTLIST];
int sess_afr_faststart  = 0;
long sess_assured_flow_rate  = 0;
long sess_bandwidth_limit  = 0;
int net_use_nkn_stack = 0;
int l4proxy_enabled = 0;
int NM_tot_threads = 2;
int nm_lb_policy = 0;
int om_cache_no_cache_obj  = 0;
const char * om_oomgr_queuefile = "/tmp/OOMGR.queue";
int om_oomgr_queue_retries  = 2;
int om_oomgr_queue_retry_delay  = 1;
long om_max_inline_ingest_objsize = 10485760;
unsigned int om_connection_limit = 0;
//int ssp_smoothflow  = 0;
//int ssp_hashcheck = 1;
//int dbg_log_level  = 2;
//int dbg_log_mod  = 0xFFFFFFFF;
int am_cache_promotion_enabled = 1;
int am_cache_ingest_hits_threshold = 1;
int am_cache_ingest_last_eviction_time_diff = 60;
int am_cache_evict_aging_time = 3600;
int am_cache_promotion_hotness_increment = 1;
int am_cache_promotion_hotness_threshold = 1;
int am_cache_promotion_hit_increment = 1;
int am_cache_promotion_hit_threshold = 1;
int rtsp_enable = 0;
int vfs_dm2_enable = 1;
int fuse_enable = 0;
int rtsched_enable = 1;
int nfs_numthreads = 100;
const char * gmgr_queuefile = "/tmp/GMGR.queue";
int gmgr_queue_maxsize  = 1024;
int forwardproxy_enable = 1;
int cp_enable = 1;
int ip_spoofing_enable = 0;
int video_ankeena_fmt_enable = 1;
int rtsp_tproxy_enable = 0;
int tunnel_only_enable = 0;
int rtsp_relay_only_enable = 0;
int adns_enabled = 1;
int bond_if_cfg_enabled = 0;
int bind_socket_with_interface = 1;
unsigned long max_virtual_memory = 0;
int close_socket_by_RST_enable = 0;
int om_req_collison_delay_msecs = 100;
int om_req_behind_delay_msecs = 10;
int om_req_ahead_delay_msecs = 100;
int om_tproxy_buf_pages_per_task = 2;
int glob_log_cache_ingest_failure = 0;


int glob_adns_cache_timeout = 2047;

const char * cmm_queuefile = "/tmp/CMM.queue";
int cmm_queue_retries = 2;
int cmm_queue_retry_delay = 1;

const char * node_status_queuefile = "/tmp/NODESTATUS.queue";
int node_status_queue_maxsize  = 1024;

////////////////////////////////////////////////////////////////////////////////
// CL7 Proxy namespace configurable options
////////////////////////////////////////////////////////////////////////////////

// Origin Fetch
const char *cl7pxyns_origin_fetch_cache_directive_no_cache = "tunnel"; // CL7 default
int cl7pxyns_origin_fetch_cache_age = 0;
const char *cl7pxyns_origin_fetch_custom_cache_control = 0;
int cl7pxyns_origin_fetch_partial_content = 0;
int cl7pxyns_origin_fetch_date_header_modify = 0;
int cl7pxyns_origin_fetch_flush_intermediate_caches = 0;
int cl7pxyns_origin_fetch_content_store_cache_age_threshold = 0;
int cl7pxyns_origin_fetch_content_store_uri_depth_threshold = 1; // CL7 default
int cl7pxyns_origin_fetch_offline_fetch_size = 0;
int cl7pxyns_origin_fetch_offline_fetch_smooth_flow = 0;
int cl7pxyns_origin_fetch_convert_head = 0;
long cl7pxyns_origin_fetch_store_cache_min_size = LONG_MAX; // CL7 default
int cl7pxyns_origin_fetch_is_set_cookie_cacheable = 0;
int cl7pxyns_origin_fetch_is_host_header_inheritable = 1; // CL7 default

int cl7pxyns_origin_fetch_content_type_count = 0;
const char *cl7pxyns_origin_fetch_cache_age_map_0_content_type = 0;
int cl7pxyns_origin_fetch_cache_age_map_0_cache_age = 0;

const char *cl7pxyns_origin_fetch_cache_age_map_1_content_type = 0;
int cl7pxyns_origin_fetch_cache_age_map_1_cache_age = 0;

const char *cl7pxyns_origin_fetch_cache_age_map_2_content_type = 0;
int cl7pxyns_origin_fetch_cache_age_map_2_cache_age = 0;

const char *cl7pxyns_origin_fetch_cache_age_map_3_content_type = 0;
int cl7pxyns_origin_fetch_cache_age_map_3_cache_age = 0;

const char *cl7pxyns_origin_fetch_cache_age_map_4_content_type = 0;
int cl7pxyns_origin_fetch_cache_age_map_4_cache_age = 0;

long cl7pxyns_origin_fetch_cache_barrier_revalidate_reval_time = 0;
int cl7pxyns_origin_fetch_cache_barrier_revalidate_reval_type = 0;
int cl7pxyns_origin_fetch_disable_cache_on_query = 1; // CL7 default
int cl7pxyns_origin_fetch_strip_query = 0;
int cl7pxyns_origin_fetch_use_date_header = 0;
int cl7pxyns_origin_fetch_disable_cache_on_cookie = 0;
int cl7pxyns_origin_fetch_client_req_max_age = 0;
int cl7pxyns_origin_fetch_client_req_serve_from_origin = 1; // CL7 default
int cl7pxyns_origin_fetch_cache_fill = 0;
int cl7pxyns_origin_fetch_ingest_policy = 0;
int cl7pxyns_origin_fetch_ingest_only_in_ram = 1; // CL7 default
int cl7pxyns_origin_fetch_eod_on_origin_close = 0;
const char *cl7pxyns_origin_fetch_object_start_header = 0;
int cl7pxyns_origin_fetch_object_cache_pin_cache_pin_ena = 0;
int cl7pxyns_origin_fetch_object_cache_pin_cache_auto_pin = 0;
long cl7pxyns_origin_fetch_object_cache_pin_cache_resv_bytes = 0;
long cl7pxyns_origin_fetch_object_cache_pin_max_obj_bytes = 0;
const char *cl7pxyns_origin_fetch_object_cache_pin_pin_header = 0;
const char *cl7pxyns_origin_fetch_object_cache_pin_end_header = 0;
int cl7pxyns_origin_fetch_redirect_302 = 0;
int cl7pxyns_origin_fetch_tunnel_all = 0;
int cl7pxyns_origin_fetch_object_correlation_etag_ignore = 0;
int cl7pxyns_origin_fetch_object_correlation_validatorsall_ignore = 0;
int cl7pxyns_origin_fetch_client_driven_agg_threshold = 0;
int cl7pxyns_origin_fetch_cache_obj_size_min = 0;
int cl7pxyns_origin_fetch_cache_obj_size_max = 0;

int cl7pxyns_origin_fetch_cache_reval_cache_revalidate = 0; // CL7 default
int cl7pxyns_origin_fetch_cache_reval_method = REVAL_METHOD_GET; // CL7 default
int cl7pxyns_origin_fetch_cache_reval_validate_header = 0; // CL7 default
const char *cl7pxyns_origin_fetch_cache_reval_header_name = ""; // CL7 default

int cl7pxyns_origin_fetch_cache_ingest_hotness_threshold = 0;
int cl7pxyns_origin_fetch_cache_ingest_size_threshold = 0;
int cl7pxyns_origin_fetch_cacheable_no_revalidation = 0;
int cl7pxyns_origin_fetch_client_req_connect_handle = 0;

// Origin Request
const char *cl7pxyns_origin_request_add_headers_name_0 = 0;
const char *cl7pxyns_origin_request_add_headers_value_0 = 0;

const char *cl7pxyns_origin_request_add_headers_name_1 = 0;
const char *cl7pxyns_origin_request_add_headers_value_1 = 0;

const char *cl7pxyns_origin_request_add_headers_name_2 = 0;
const char *cl7pxyns_origin_request_add_headers_value_2 = 0;

const char *cl7pxyns_origin_request_add_headers_name_3 = 0;
const char *cl7pxyns_origin_request_add_headers_value_3 = 0;

int cl7pxyns_origin_request_x_forward_header = 0;
int cl7pxyns_origin_request_orig_conn_values_conn_timeout = 0;
int cl7pxyns_origin_request_orig_conn_values_conn_retry_delay = 0;
int cl7pxyns_origin_request_orig_conn_values_read_timeout = 0;
int cl7pxyns_origin_request_orig_conn_values_read_retry_delay = 0;
////////////////////////////////////////////////////////////////////////////////
// End CL7 Proxy namespace configurable options
////////////////////////////////////////////////////////////////////////////////

/*
 * This function is called before the configuration file is parsed.
 */
void cfg_params_init(void);
void cfg_params_init(void)
{
	int i;
	http_allowed_methods.method_cnt = 0;
	http_allowed_methods.all_methods = 0;
	NKN_MUTEX_INIT(&http_allowed_methods.cfg_mutex, NULL, "http-method-mutex");
	for ( i = 0; i < MAX_HTTP_METHODS_ALLOWED; i++) {
		http_allowed_methods.method[i].allowed = 0;
		http_allowed_methods.method[i].method_len = 0;
		http_allowed_methods.method[i].name = NULL;
	}
}


/*
 * This function is called after the configuration file has been parsed.
 * the purpose of this function is to make sure the configuration is right.
 */
void nkn_check_cfg(void);
void nkn_check_cfg(void)
{
}

