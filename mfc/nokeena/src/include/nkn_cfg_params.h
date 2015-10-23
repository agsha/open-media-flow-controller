#ifndef NKN_CFG_PARAM_H
#define NKN_CFG_PARAM_H

#define MAX_TIMESTAMP_LEN	50
#define MAX_PORTLIST            64

extern unsigned short u16_g_port_mapper_min;
extern unsigned short u16_g_port_mapper_max;
extern long cm_max_cache_size;		// physmem (PM) for RAMcache
extern unsigned int bm_page_ratio;
extern long nkn_max_dict_mem_MB;	// PM left over after RAMcache + system
extern const char * fmgr_queuefile ;
extern long bm_cfg_cr_max_queue_time;
extern long bm_cfg_cr_max_req;
extern unsigned int max_unresp_connections;
extern int http_cfg_fo_use_dns;
extern int http_cfg_origin_conn_timeout;
extern int bm_cfg_discard;
extern int bm_cfg_dynscale;
extern int bm_cfg_valgrind;
extern int fmgr_cache_no_cache_obj ;
extern int fmgr_queue_maxsize ;
extern int http_idle_timeout ;
extern const char * include_orig_key;
extern int cp_idle_timeout ;
extern int nkn_max_client ;
extern int max_http_req_size ;
extern int max_get_rate_limit ;
extern int nkn_http_serverport[MAX_PORTLIST] ;
extern int nkn_rtsp_vod_serverport[MAX_PORTLIST] ;
extern int nkn_rtsp_live_serverport[MAX_PORTLIST] ;
extern int sess_afr_faststart ;
extern long sess_assured_flow_rate ;
extern long sess_bandwidth_limit ;
extern int net_use_nkn_stack ;
extern int l4proxy_enabled ;
extern int adnsd_enabled ;
extern int NM_tot_threads ;
extern int nm_lb_policy;
extern int om_cache_no_cache_obj ;
extern const char * om_oomgr_queuefile ;
extern int om_oomgr_queue_retries ;
extern int om_oomgr_queue_retry_delay ;
extern long om_max_inline_ingest_objsize ;
extern unsigned int om_connection_limit;
extern int glob_log_cache_ingest_failure;
//extern int ssp_smoothflow ;
//extern int ssp_hashcheck;
//extern int dbg_log_level;
//extern int dbg_log_mod;
extern int am_cache_promotion_enabled;
extern int am_cache_ingest_hits_threshold;
extern unsigned int am_max_origin_entries;
extern unsigned int am_origin_tbl_timeout;
extern unsigned int am_cache_ingest_policy;
extern int am_cache_ingest_last_eviction_time_diff;
extern int am_cache_evict_aging_time;
extern int am_cache_promotion_hotness_increment;
extern int am_cache_promotion_hotness_threshold;
extern int am_cache_promotion_hit_increment;
extern int am_cache_promotion_hit_threshold;
extern int mm_prov_hi_threshold;
extern int mm_prov_lo_threshold;
extern int mm_evict_thread_time_sec;
extern int rtsp_enable;
extern int vfs_dm2_enable;
extern int fuse_enable;
extern int rtsched_enable;
extern int nfs_numthreads;
extern const char * gmgr_queuefile ;
extern int gmgr_queue_maxsize ;
extern int forwardproxy_enable;
extern int cp_enable;
extern int ip_spoofing_enable;
extern int video_ankeena_fmt_enable;
extern int rtsp_tproxy_enable;
extern int tunnel_only_enable;
extern int rtsp_relay_only_enable;
extern int adns_enabled;
extern int bond_if_cfg_enabled;
extern int close_socket_by_RST_enable;
extern int om_req_collison_delay_msecs;
extern int om_req_behind_delay_msecs;
extern int om_req_ahead_delay_msecs;
extern int om_tproxy_buf_pages_per_task;
extern int glob_adns_cache_timeout;
extern int bind_socket_with_interface;
extern unsigned long max_virtual_memory;
extern const char * cmm_queuefile;
extern int cmm_queue_retries;
extern int cmm_queue_retry_delay;
extern const char * node_status_queuefile;
extern int node_status_queue_maxsize;
extern int debug_fd_trace;
extern int nm_hdl_send_and_receive;
extern unsigned nkn_am_memory_limit;
extern int pe_url_category_lookup;
extern int pe_ucflt_failover_bypass_enable;

////////////////////////////////////////////////////////////////////////////////
// CL7 Proxy namespace configurable options
////////////////////////////////////////////////////////////////////////////////

// Origin Fetch
extern const char *cl7pxyns_origin_fetch_cache_directive_no_cache;
extern int cl7pxyns_origin_fetch_cache_age;
extern const char *cl7pxyns_origin_fetch_custom_cache_control;
extern int cl7pxyns_origin_fetch_partial_content;
extern int cl7pxyns_origin_fetch_date_header_modify;
extern int cl7pxyns_origin_fetch_flush_intermediate_caches;
extern int cl7pxyns_origin_fetch_content_store_cache_age_threshold;
extern int cl7pxyns_origin_fetch_content_store_uri_depth_threshold;
extern int cl7pxyns_origin_fetch_offline_fetch_size;
extern int cl7pxyns_origin_fetch_offline_fetch_smooth_flow;
extern int cl7pxyns_origin_fetch_convert_head;
extern long cl7pxyns_origin_fetch_store_cache_min_size;
extern int cl7pxyns_origin_fetch_is_set_cookie_cacheable;
extern int cl7pxyns_origin_fetch_is_host_header_inheritable;

extern int cl7pxyns_origin_fetch_content_type_count;
extern const char *cl7pxyns_origin_fetch_cache_age_map_0_content_type;
extern int cl7pxyns_origin_fetch_cache_age_map_0_cache_age;

extern const char *cl7pxyns_origin_fetch_cache_age_map_1_content_type;
extern int cl7pxyns_origin_fetch_cache_age_map_1_cache_age;

extern const char *cl7pxyns_origin_fetch_cache_age_map_2_content_type;
extern int cl7pxyns_origin_fetch_cache_age_map_2_cache_age;

extern const char *cl7pxyns_origin_fetch_cache_age_map_3_content_type;
extern int cl7pxyns_origin_fetch_cache_age_map_3_cache_age;

extern const char *cl7pxyns_origin_fetch_cache_age_map_4_content_type;
extern int cl7pxyns_origin_fetch_cache_age_map_4_cache_age;

extern long cl7pxyns_origin_fetch_cache_barrier_revalidate_reval_time;
extern int cl7pxyns_origin_fetch_cache_barrier_revalidate_reval_type;
extern int cl7pxyns_origin_fetch_disable_cache_on_query;
extern int cl7pxyns_origin_fetch_strip_query;
extern int cl7pxyns_origin_fetch_use_date_header;
extern int cl7pxyns_origin_fetch_disable_cache_on_cookie;
extern int cl7pxyns_origin_fetch_client_req_max_age;
extern int cl7pxyns_origin_fetch_client_req_serve_from_origin;
extern int cl7pxyns_origin_fetch_cache_fill;
extern int cl7pxyns_origin_fetch_ingest_policy;
extern int cl7pxyns_origin_fetch_ingest_only_in_ram;
extern int cl7pxyns_origin_fetch_eod_on_origin_close;
extern const char *cl7pxyns_origin_fetch_object_start_header;
extern int cl7pxyns_origin_fetch_object_cache_pin_cache_pin_ena;
extern int cl7pxyns_origin_fetch_object_cache_pin_cache_auto_pin;
extern long cl7pxyns_origin_fetch_object_cache_pin_cache_resv_bytes;
extern long cl7pxyns_origin_fetch_object_cache_pin_max_obj_bytes;
extern const char *cl7pxyns_origin_fetch_object_cache_pin_pin_header;
extern const char *cl7pxyns_origin_fetch_object_cache_pin_end_header;
extern int cl7pxyns_origin_fetch_redirect_302;
extern int cl7pxyns_origin_fetch_tunnel_all;
extern int cl7pxyns_origin_fetch_object_correlation_etag_ignore;
extern int cl7pxyns_origin_fetch_object_correlation_validatorsall_ignore;
extern int cl7pxyns_origin_fetch_client_driven_agg_threshold;
extern int cl7pxyns_origin_fetch_cache_obj_size_min;
extern int cl7pxyns_origin_fetch_cache_obj_size_max;
extern int cl7pxyns_origin_fetch_cache_reval_cache_revalidate;
extern int cl7pxyns_origin_fetch_cache_reval_method;
extern int cl7pxyns_origin_fetch_cache_reval_validate_header;
extern const char *cl7pxyns_origin_fetch_cache_reval_header_name;
extern int cl7pxyns_origin_fetch_cache_ingest_hotness_threshold;
extern int cl7pxyns_origin_fetch_cache_ingest_size_threshold;
extern int cl7pxyns_origin_fetch_cacheable_no_revalidation;
extern int cl7pxyns_origin_fetch_client_req_connect_handle;

// Origin Request
extern const char *cl7pxyns_origin_request_add_headers_name_0;
extern const char *cl7pxyns_origin_request_add_headers_value_0;

extern const char *cl7pxyns_origin_request_add_headers_name_1;
extern const char *cl7pxyns_origin_request_add_headers_value_1;

extern const char *cl7pxyns_origin_request_add_headers_name_2;
extern const char *cl7pxyns_origin_request_add_headers_value_2;

extern const char *cl7pxyns_origin_request_add_headers_name_3;
extern const char *cl7pxyns_origin_request_add_headers_value_3;

extern int cl7pxyns_origin_request_x_forward_header;
extern int cl7pxyns_origin_request_orig_conn_values_conn_timeout;
extern int cl7pxyns_origin_request_orig_conn_values_conn_retry_delay;
extern int cl7pxyns_origin_request_orig_conn_values_read_timeout;
extern int cl7pxyns_origin_request_orig_conn_values_read_retry_delay;

extern int glob_dm2_num_preread_disk_threads;
extern int glob_dm2_throttle_writes;
extern int glob_dm2_small_write_enable;
extern int glob_dm2_small_write_min_size;

extern unsigned long glob_am_bytes_based_hotness_threshold;
extern int glob_am_bytes_based_hotness;

/* PUSH Ingest config variables */
extern int glob_am_push_ingest_enabled;
extern int glob_mm_push_ingest_buffer_ratio;
extern int glob_mm_push_entry_info_check_tmout;
extern int glob_mm_push_ingest_parallel_ingest_restrict;

////////////////////////////////////////////////////////////////////////////////
// End CL7 Proxy namespace configurable options
////////////////////////////////////////////////////////////////////////////////

// URL filter options
extern int uf_enable;
extern int uf_use_cli;
extern int uf_reject_mode;
extern const char *uf_bin_filename;


extern void http_cfg_add_response_hdr_callback (char * s);
extern void http_no_cfg_add_response_hdr_callback (char * s);
extern int http_get_nth_add_response_hdr(int nth_element,
					const char **name, int *namelen,
					const char **value, int *valuelen);

extern void http_cfg_allowed_content_types_callback (char * s);
extern void http_no_cfg_allowed_content_types_callback (char * s);
extern const char * get_http_content_type(const char * uri, int len);
extern int allow_content_type(const char *uri, int urilen);

extern void http_cfg_content_type_callback (char * s);
extern void http_no_cfg_content_type_callback (char * s);

extern int http_suppress_response_hdr(const char *name, int namelen);
extern void http_cfg_suppress_response_hdr_callback (char * s);
extern void http_no_cfg_suppress_response_hdr_callback (char * s);

//extern void rtsp_cfg_pub_point_callback(pub_point_node_data_t *pstPublishPoint);
//extern void rtsp_no_cfg_pub_point_callback(pub_point_node_data_t *pstPublishPoint);

extern void rtsp_cfg_so_callback(char *s);
extern void rtsp_no_cfg_so_callback(char *ext);


extern void nkn_http_portlist_callback(char * http_port_list_str);
extern void nkn_http_interfacelist_callback(char * http_interface_list_str);
extern void nkn_rtsp_interfacelist_callback(char * rtsp_interface_list_str);
extern void nkn_rtsp_vod_portlist_callback(char * rtsp_port_list_str);
extern void nkn_rtsp_live_portlist_callback(char * rtsp_port_list_str);
extern void nkn_http_allow_req_method_callback(char * http_allow_req_method_list_str);
extern void nkn_http_delete_req_method_callback(char * http_allow_req_method);
extern void nkn_http_set_all_req_method_callback(int allow_all_method);

#endif
