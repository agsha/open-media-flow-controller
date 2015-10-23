/*
 *
 * Filename:  nvsd_mgmt_main.c
 * Date:      2008/11/23 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

/* Standard Headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <glib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/prctl.h>

/* Samara Headers */
#include "md_client.h"
#include "mdc_wrapper.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"
#include "backtrace.h"
#include "proc_utils.h"
#include "mdc_backlog.h"

/* nvsd Headers */
#include "nkn_defs.h"
#include "nvsd_mgmt.h"
#include "nkn_stat.h"
#include "nkn_common_config.h"
#include "nkn_am_api.h"
#include "nkn_cfg_params.h"
#include "nkn_regex.h"
#include "nvsd_mgmt_api.h"
#include "nvsd_mgmt_lib.h"
#include "cache_mgr.h"
#include "nkn_mgmt_defs.h"
#include "nkn_mgmt_log.h"
#include "libnknmgmt.h"
#include "interface.h"

/* An array of structure to hold the nodes to be queried
   and the corresponding call backs to be called
 */

/* NOTE ORDERING /REGISTERING OF CALLBACKS IS VERY IMPORTANT HERE */
static int
nvsd_mgmt_get_nodes(mgmt_query_data_t * mgmt_query,
	bn_binding_array ** bindings);

static void
report_segfault(int signo, siginfo_t * sinf, void * arg);

extern const char debug_config_prefix[];
extern const char resource_mgr_config_prefix[];

extern const char pe_srvr_prefix[];
extern const char errlog_prefix[];
extern const char streamlog_prefix[];
extern const char network_config_prefix[];
extern const char http_config_server_port_prefix[];
extern const char http_config_allow_req_method_prefix[];
extern const char http_config_server_interface_prefix[];
extern const char http_config_prefix[];

extern const char http_config_content_type_prefix[];
extern const char http_content_suppress_resp_header_prefix[];
extern const char http_content_add_resp_header_prefix[];
extern const char http_content_set_resp_header_prefix[];
extern const char http_content_allowed_type_prefix[];

extern const char rtstream_config_prefix[];
extern const char rtstream_media_encode_prefix[];
extern const char rtstream_vod_server_port_prefix[];
extern const char rtstream_live_server_port_prefix[];
extern const char rtsp_config_server_interface_prefix[];

extern const char buffermgr_config_prefix[];
extern const char am_config_prefix[];
extern const char mm_config_prefix[];

extern const char virtual_player_config_prefix[];
extern const char server_map_config_prefix[];
extern const char cluster_config_prefix[];
extern const char namespace_config_prefix[];
extern const char uri_config_prefix[];
extern const char origin_fetch_config_prefix[];
extern const char origin_fetch_content_type_config_prefix[];
extern const char acclog_prefix[];
extern const char l4proxy_config_prefix[];
extern const char url_filter_map_config_prefix[];

int mgmt_query_data_sanity(mgmt_query_data_t * mgmt_query, int *ret_code);

int nvsd_system_tp_set_nvsd_state(int state);

mgmt_query_data_t mgmt_query_data[] = {
    {"/nkn/nvsd/system/config", NULL},
    {"/nkn/mfd/licensing/config", NULL},
    {"/license/key", NULL},
    {resource_mgr_config_prefix, NULL},
    {pe_srvr_prefix, NULL},
    {errlog_prefix, NULL},
    {streamlog_prefix, NULL},
    {network_config_prefix, NULL},
    {http_config_prefix, 0},
    {rtstream_config_prefix, 0},
    {buffermgr_config_prefix, 0},
    {debug_config_prefix, NULL},
    {am_config_prefix, NULL},
    {mm_config_prefix, NULL},
    {virtual_player_config_prefix, 0},
    {server_map_config_prefix, NULL},
    {cluster_config_prefix, NULL},
    {namespace_config_prefix, NULL},
    {uri_config_prefix, NULL},
    {origin_fetch_config_prefix, NULL},
    {acclog_prefix, NULL},
    {l4proxy_config_prefix, NULL},
    {url_filter_map_config_prefix, NULL},
};

#define MAX_MGMT_QUERY sizeof(mgmt_query_data)/sizeof(mgmt_query_data_t)

/* Samara Variable */
lew_context *nvsd_lwc = NULL;
md_client_context *nvsd_mcc = NULL;
mdc_bl_context *nvsd_blc = NULL;
tbool nvsd_exiting = false;
static lew_event *nvsd_mgmt_retry_event = NULL;
int service_init_flags = 0;

int
setup_signal_handler(void);

extern int
nvsd_handle_smap_status_change(const char *t_server_map,
	const char *t_bin_filename,
	const char *status);

int nvsd_mgmt_recv_action_request(gcl_session * sess,
	bn_request ** inout_request, void *arg);
int nvsd_mgmt_recv_query_request(gcl_session * sess,
	bn_request ** inout_request, void *arg);
int nvsd_mgmt_recv_event_request(gcl_session * sess,
	bn_request ** inout_request, void *arg);
int nvsd_mgmt_req_start(md_client_context * mcc,
	bn_request * request,
	tbool request_complete,
	bn_response * response, int32 idx, void *arg);
int nvsd_mgmt_req_complete(md_client_context * mcc,
	bn_request * request,
	tbool request_complete,
	bn_response * response, int32 idx, void *arg);
int
nvsd_mgmt_main_tgl_pxy(int pxy_enable);

int
nvsd_mgmt_retry_handler(int fd,
	short event_type, void *data, lew_context * ctxt);

pthread_mutex_t nvsd_gcl_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t nvsd_gcl_cond_var = PTHREAD_COND_INITIALIZER;

extern unsigned int jnpr_log_level;

extern int
nsvd_mgmt_ns_is_obj_cached(const char *cp_namespace,
	const char *cp_uid,
	const char *incoming_uri, char **ret_cache_name);

/* Local Variables */
static const char pxe_tgl_sh[] = "/opt/nkn/bin/pxy_toggle.sh";
static GThread *mgmt_thread;

/* global variables*/
NKNCNT_DEF(tot_bkup_bytes_sent, uint64_t, "", "Total bytes sent backup")
NKNCNT_DEF(tot_bkup_size_from_disk, uint64_t, "",
	"Total bytes sent from disk backup")
NKNCNT_DEF(tot_bkup_size_from_cache, uint64_t, "",
	"Total bytes sent from cache backup")
NKNCNT_DEF(tot_bkup_size_from_origin, uint64_t, "",
	"Total bytes sent from origin backup")
NKNCNT_DEF(tot_bkup_size_from_nfs, uint64_t, "",
	"Total bytes sent from NFS backup")
NKNCNT_DEF(tot_bkup_size_from_tunnel, uint64_t, "",
	"Total bytes sent through tunnel backup")
NKNCNT_DEF(http_bkup_tot_200_resp, uint32_t, "", "Total 200 response backup")
NKNCNT_DEF(http_bkup_tot_206_resp, uint32_t, "", "Total 206 response backup")
NKNCNT_DEF(http_bkup_tot_302_resp, uint32_t, "", "Total 302 response backup")
NKNCNT_DEF(http_bkup_tot_304_resp, uint32_t, "", "Total 304 response backup")
NKNCNT_DEF(http_bkup_tot_400_resp, uint32_t, "", "Total 400 response backup")
NKNCNT_DEF(http_bkup_tot_404_resp, uint32_t, "", "Total 404 response backup")
NKNCNT_DEF(http_bkup_tot_416_resp, uint32_t, "", "Total 416 response backup")
NKNCNT_DEF(http_bkup_tot_500_resp, uint32_t, "", "Total 500 response backup")
NKNCNT_DEF(http_bkup_tot_501_resp, uint32_t, "", "Total 501 response backup")
NKNCNT_DEF(http_bkup_tot_503_resp, uint32_t, "", "Total 503 response backup")
NKNCNT_DEF(http_bkup_tot_504_resp, uint32_t, "", "Total 504 response backup")
NKNCNT_DEF(http_bkup_tot_well_finished, uint32_t, "",
	"Total http well finished bkup")
NKNCNT_DEF(http_bkup_tot_transactions, uint64_t, "", "total http transactions")
NKNCNT_DEF(cache_bkup_request_error, uint64_t, "",
	"Total cache request error bkup")
NKNCNT_DEF(err_bkup_timeout_with_task, uint32_t, "", "Total timeout error bkup")
NKNCNT_DEF(socket_bkup_tot_timeout, uint32_t, "", "Total socket timeout bkup")
NKNCNT_DEF(http_bkup_schd_err_get_data, uint32_t, "",
	"Total http schedule error bkup")
NKNCNT_DEF(http_bkup_cache_miss_cnt, uint64_t, "", "Total Http cache miss bkup")
NKNCNT_DEF(http_bkup_cache_hit_cnt, uint64_t, "", "Total Http cache hit bkup")
NKNCNT_DEF(sched_bkup_task_deadline_missed, uint32_t, "",
	"Total schedule task deadline missed bkup")
NKNCNT_DEF(om_bkup_err_get, uint32_t, "", "Total OM error bkup")
NKNCNT_DEF(http_bkup_err_parse, uint32_t, "", "Total http parse error bkup")
NKNCNT_DEF(om_bkup_err_conn_failed, uint32_t, "",
	"Total OM connection failed error bkup")
NKNCNT_DEF(tot_bkup_get_in_this_second, uint64_t, "", "Total gets  bkup")
NKNCNT_DEF(om_bkup_tot_completed_http_trans, uint64_t, "",
	"Total OM completed http transaction bkup")
NKNCNT_DEF(dm2_bkup_raw_read_cnt, uint64_t, "", "Total DM2 raw read count bkup")
NKNCNT_DEF(dm2_bkup_raw_write_cnt, uint64_t, "",
	"Total DM2 raw write count bkup")
NKNCNT_DEF(dm2_bkup_ext_read_bytes, uint64_t, "",
	"Total DM2 external read bytes bkup")
NKNCNT_DEF(ssp_bkup_seeks, uint64_t, "", "Total Seeks bkup")
NKNCNT_DEF(ssp_bkup_hash_failures, uint64_t, "", "Total hash failures bkup")
NKNCNT_DEF(sched_bkup_runnable_q_pushes, uint64_t, "",
	"Total schedule runnable q pushes bkup")
NKNCNT_DEF(sched_bkup_runnable_q_pops, uint64_t, "",
	"Total schedule runnable q pops bkup")
NKNCNT_DEF(fp_bkup_err_make_tunnel, uint64_t, "",
	"Total Tunnel fp make error bkup")
NKNCNT_DEF(tot_bkup_http_sockets, uint64_t, "",
	"Total HTTP connection accepted")
NKNCNT_DEF(tot_bkup_http_sockets_ipv6, uint64_t, "",
	"Total HTTP connection accepted for ipv6")
NKNCNT_DEF(nkn_reset_counter_time, int32_t, "",
	"Time at which Reset counter was issued ")
NKNCNT_DEF(rtsp_bkup_tot_bytes_sent, uint64_t, "",
	"RTSP bytes served from origin")
NKNCNT_DEF(rtp_bkup_tot_udp_packets_fwd, uint64_t, "",
	"Total RTP udp packet forwarded")
NKNCNT_DEF(rtcp_bkup_tot_udp_packets_fwd, uint64_t, "",
	"Total RTCP udp packet forwarded")
NKNCNT_DEF(rtcp_bkup_tot_udp_sr_packets_sent, uint64_t, "",
	"Total RTCP udp packet sent")
NKNCNT_DEF(rtcp_bkup_tot_udp_rr_packets_sent, uint64_t, "",
	"Total RTCP udp packet sent")
NKNCNT_DEF(rtsp_bkup_tot_status_resp_200, uint32_t, "",
	"Total RTSP 200 response backup")
NKNCNT_DEF(rtsp_bkup_tot_status_resp_400, uint32_t, "",
	"Total RTSP 400 response backup")
NKNCNT_DEF(rtsp_bkup_tot_status_resp_404, uint32_t, "",
	"Total RTSP 404 response backup")
NKNCNT_DEF(rtsp_bkup_tot_status_resp_500, uint32_t, "",
	"Total RTSP 500 response backup")
NKNCNT_DEF(rtsp_bkup_tot_status_resp_501, uint32_t, "",
	"Total RTSP 501 response backup")
NKNCNT_DEF(rtcp_bkup_tot_udp_packets_recv, uint64_t, "",
	"Total RTCP udp packet received")
NKNCNT_DEF(tot_bkup_rtcp_sr, uint64_t, "", "Total RTCP SR packet received")
NKNCNT_DEF(tot_bkup_rtcp_rr, uint64_t, "", "Total RTCP RR packet received")
NKNCNT_DEF(rtsp_bkup_tot_rtsp_parse_error, uint64_t, "",
	"Total RTSP parser error")
NKNCNT_DEF(rtsp_bkup_tot_vod_bytes_sent, uint64_t, "",
	"RTSP bytes served from vod")
NKNCNT_DEF(fp_bkup_tot_tunnel, uint64_t, "", "Total HTTP tunnel connections")
NKNCNT_DEF(cll7redir_bkup_intercept_success, uint64_t, "",
	"Total number of Redirects")
NKNCNT_DEF(cll7redir_bkup_int_err_1, uint64_t, "",
	"number of Redirect errors 1")
NKNCNT_DEF(cll7redir_bkup_int_err_2, uint64_t, "",
	"number of Redirect errors 2")
NKNCNT_DEF(cll7redir_bkup_int_errs, uint64_t, "", "number of Redirect errors")

NKNCNT_EXTERN(http_tot_200_resp, uint64_t)
NKNCNT_EXTERN(http_tot_206_resp, uint64_t)
NKNCNT_EXTERN(http_tot_302_resp, uint64_t)
NKNCNT_EXTERN(http_tot_304_resp, uint64_t)
NKNCNT_EXTERN(http_tot_400_resp, uint64_t)
NKNCNT_EXTERN(http_tot_404_resp, uint64_t)
NKNCNT_EXTERN(http_tot_416_resp, uint64_t)
NKNCNT_EXTERN(http_tot_500_resp, uint64_t)
NKNCNT_EXTERN(http_tot_501_resp, uint64_t)
NKNCNT_EXTERN(http_tot_503_resp, uint64_t)
NKNCNT_EXTERN(http_tot_504_resp, uint64_t)
NKNCNT_EXTERN(http_tot_well_finished, uint64_t)
NKNCNT_EXTERN(http_tot_transactions, uint64_t)
NKNCNT_EXTERN(tot_bytes_sent, uint64_t)
NKNCNT_EXTERN(tot_size_from_disk, uint64_t)
NKNCNT_EXTERN(tot_size_from_cache, uint64_t)
NKNCNT_EXTERN(tot_size_from_origin, uint64_t)
NKNCNT_EXTERN(tot_size_from_nfs, uint64_t)
NKNCNT_EXTERN(tot_size_from_tunnel, uint64_t)
NKNCNT_EXTERN(cache_request_error, uint64_t)
NKNCNT_EXTERN(err_timeout_with_task, uint64_t)
NKNCNT_EXTERN(socket_tot_timeout, uint64_t)
NKNCNT_EXTERN(http_schd_err_get_data, uint64_t)
NKNCNT_EXTERN(http_cache_miss_cnt, uint64_t)
NKNCNT_EXTERN(http_cache_hit_cnt, uint64_t)
NKNCNT_EXTERN(om_err_get, uint32_t)
NKNCNT_EXTERN(http_err_parse, uint32_t)
NKNCNT_EXTERN(om_err_conn_failed, uint32_t)
NKNCNT_EXTERN(tot_get_in_this_second, uint64_t)
NKNCNT_EXTERN(om_tot_completed_http_trans, uint64_t)
NKNCNT_EXTERN(dm2_raw_read_cnt, uint64_t)
NKNCNT_EXTERN(dm2_raw_write_cnt, uint64_t)
NKNCNT_EXTERN(dm2_ext_read_bytes, uint64_t)
NKNCNT_EXTERN(ssp_seeks, uint64_t)
NKNCNT_EXTERN(ssp_hash_failures, uint64_t)
NKNCNT_EXTERN(sched_cleanup_q_pushes, uint32_t)
NKNCNT_EXTERN(sched_cleanup_q_pops, uint32_t)
NKNCNT_EXTERN(fp_err_make_tunnel, uint64_t)
NKNCNT_EXTERN(tot_http_sockets, uint64_t)
NKNCNT_EXTERN(tot_http_sockets_ipv6, uint64_t)
NKNCNT_EXTERN(rtsp_tot_bytes_sent, uint64_t)
NKNCNT_EXTERN(rtp_tot_udp_packets_fwd, uint64_t)
NKNCNT_EXTERN(rtcp_tot_udp_packets_fwd, uint64_t)
NKNCNT_EXTERN(rtcp_tot_udp_sr_packets_sent, uint64_t)
NKNCNT_EXTERN(rtcp_tot_udp_rr_packets_sent, uint64_t)
NKNCNT_EXTERN(rtsp_tot_status_resp_200, uint32_t)
NKNCNT_EXTERN(rtsp_tot_status_resp_400, uint32_t)
NKNCNT_EXTERN(rtsp_tot_status_resp_404, uint32_t)
NKNCNT_EXTERN(rtsp_tot_status_resp_500, uint32_t)
NKNCNT_EXTERN(rtsp_tot_status_resp_501, uint32_t)
NKNCNT_EXTERN(rtcp_tot_udp_packets_recv, uint64_t)
NKNCNT_EXTERN(tot_rtcp_sr, uint64_t)
NKNCNT_EXTERN(tot_rtcp_rr, uint64_t)
NKNCNT_EXTERN(rtsp_tot_rtsp_parse_error, uint64_t)
NKNCNT_EXTERN(rtsp_tot_vod_bytes_sent, uint64_t)
NKNCNT_EXTERN(fp_tot_tunnel, uint64_t)
NKNCNT_EXTERN(cll7redir_intercept_success, uint64_t)
NKNCNT_EXTERN(cll7redir_int_err_1, uint64_t)
NKNCNT_EXTERN(cll7redir_int_err_2, uint64_t)
NKNCNT_EXTERN(cll7redir_intercept_errs, uint64_t)

int user_space_l3_pxy = 0;
int kernel_space_l3_pxy = 0;

int geodb_installed = 0;

int nvsd_mgmt_thread_init(void);
int nvsd_mdc_init(void);
int nvsd_mgmt_initiate_exit(void);
int nvsd_rsrc_update_event(const bn_binding_array * arr,
	uint32 idx, bn_binding * binding, void *data);

/* Added for reset counter without mod-delivery restart*/
int nvsd_mgmt_reset_counter_backup_data(void);
int nvsd_set_time_for_reset_counter(void);
static int nvsd_mgmt_handle_session_close(int fd,
	fdic_callback_type cbt,
	void *vsess, void *gsec_arg);

static int nvsd_mgmt_handle_event_request(gcl_session * sess,
	bn_request ** inout_request,
	void *arg);

static int nvsd_config_handle_set_request(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);
static int nvsd_mgmt_handle_query_request(gcl_session * sess,
	bn_request **
	inout_query_request, void *arg);
static int nvsd_mgmt_handle_action_request(gcl_session * sess,
	bn_request **
	inout_action_request,
	void *arg);
static int nvsd_mgmt_handle_show_rank_action(gcl_session * sess,
	const bn_binding_array *
	bindings,
	const bn_request *
	action_request);

static int
nvsd_mgmt_handle_is_obj_cached(gcl_session * sess,
	const bn_binding_array * bindings,
	const bn_request * action_request);

static int nvsd_mgmt_handle_show_tier_action(gcl_session * sess,
	const bn_binding_array *
	bindings,
	const bn_request *
	action_request);
static int nvsd_mgmt_handle_server_map_refresh_action(gcl_session * sess, const
	bn_binding_array *
	bindings,
	const bn_request *
	action_request);
static int nvsd_mgmt_handle_buffermgr_object_list_action(gcl_session *
	sess, const
	bn_binding_array *
	bindings,
	const bn_request *
	action_request);

/* List of signals server can handle */
static const int nvsd_signals_handled[] = {
    SIGINT, SIGHUP, SIGPIPE, SIGTERM, SIGUSR1
};

#define nvsd_num_signals_handled (int)(sizeof(nvsd_signals_handled) / sizeof(int))

/* Libevent handles for server */
static lew_event *nvsd_event_signals[nvsd_num_signals_handled];

/* Extern Funtions */
extern int nvsd_http_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);
extern int nvsd_buffermgr_module_cfg_handle_change(bn_binding_array *
	old_bindings,
	bn_binding_array *
	new_bindings);

extern int nvsd_fmgr_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);
extern int nvsd_tfm_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);
extern int nvsd_network_module_cfg_handle_change(bn_binding_array *
	old_bindings,
	bn_binding_array *
	new_bindings);
extern int nvsd_system_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array *
	new_bindings);
extern int nvsd_am_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);
extern int nvsd_mm_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);
extern int nvsd_namespace_module_cfg_handle_change(bn_binding_array *
	old_bindings,
	bn_binding_array *
	new_bindings);
extern int nvsd_virtual_player_module_cfg_handle_change(bn_binding_array *
	old_bindings,
	bn_binding_array *
	new_bindings);
extern int nvsd_server_map_module_cfg_handle_change(bn_binding_array *
	old_bindings,
	bn_binding_array *
	new_bindings);
extern int nvsd_cluster_module_cfg_handle_change(bn_binding_array *
	old_bindings,
	bn_binding_array *
	new_bindings);
extern int nvsd_l4proxy_module_cfg_handle_change(bn_binding_array *
	old_bindings,
	bn_binding_array *
	new_bindings);

extern int nvsd_rtstream_module_cfg_handle_change(bn_binding_array *
	old_bindings,
	bn_binding_array *
	new_bindings);
extern int nvsd_pub_point_module_cfg_handle_change(bn_binding_array *
	old_bindings,
	bn_binding_array *
	new_bindings);

extern int nvsd_resource_mgr_module_cfg_handle_change(bn_binding_array *
	old_bindings,
	bn_binding_array *
	new_bindings);
extern int nvsd_errlog_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array *
	new_bindings);
extern int nvsd_acclog_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array *
	new_bindings);
extern int
nvsd_pe_srvr_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);
int ssld_config_query(void);
int ssld_config_handle_change(const bn_binding_array * arr,
	uint32 idx, bn_binding * binding, void *data);

extern char *get_uids_in_disk(void);

extern char *get_namespace_precedence_list(void);

extern int read_binary_servermap_to_buf(const char *t_server_map,
	const char *t_bin_filename);
extern int nvsd_rp_adjust_bw(void);

extern int update_interface_events(void *);

extern void dns_get_cache_list(char *dns_list, int32_t max_entries);
extern int32_t dns_delete_cache_all(void);
extern int32_t dns_delete_cache_fqdn(char *domain);
extern int32_t dns_get_cache_fqdn(char *dn, char *domain);

/* Extern Variables */
extern tbool glob_cmd_rate_limit;
extern uint32_t ssl_listen_port;

/*
 *	funtion : nvsd_handle_signal ()
 *	purpose : signal handler for mgmt 
 */
static int
nvsd_handle_signal(int signum, short ev_type, void *data, lew_context * ctxt)
{
    lc_log_basic(LOG_DEBUG,
	    "nvsd_handle_signal () - entering signal handler for signal %d",
	    signum);
    (void) ev_type;
    (void) data;
    (void) ctxt;

    switch (signum) {
	case SIGHUP:
	case SIGINT:
	case SIGTERM:
	    nvsd_mgmt_initiate_exit();
	case SIGPIPE:
	    /*
	     * Currently ignored 
	     */
	    break;

	case SIGUSR1:
	    /*
	     * Currently ignored 
	     */
	    break;

	default:
	    break;
    }

    catcher(signum);
    lc_log_basic(LOG_DEBUG,
	    "nvsd_handle_signal () - after calling catcher () for signal %d",
	    signum);

    return 0;
}	/* end of nvsd_handle_signal() */

volatile int nvsd_mgmt_thrd_initd = 0;

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_thread_hdl ()
 *	purpose : thread handler for the management module
 */
static int
nvsd_mgmt_thread_hdl(void)
{
    int err = 0;

    (void) err;
    /*
     * Set the thread name 
     */
    prctl(PR_SET_NAME, "nvsd-mgmt", 0, 0, 0);

    /*
     * Call the main loop of samara 
     */
    lc_log_debug(LOG_DEBUG, _("nvsd_mgmt_thread_hdl () - before lew_dispatch"));

    pthread_mutex_lock(&nvsd_gcl_mutex);

    err = nvsd_mgmt_init();
    bail_error(err);

    nvsd_mgmt_thrd_initd = 1;

    /*
     * Call the interface init before we wait on poll 
     */
    /*
     * TODO:handle error 
     */
    err = interface_init();
    complain_error(err);

    pthread_cond_signal(&nvsd_gcl_cond_var);
    pthread_mutex_unlock(&nvsd_gcl_mutex);

    err = lew_dispatch(nvsd_lwc, NULL, NULL);
    bail_error(err);

    /*
     * De-init to be called 
     */
    err = nvsd_mgmt_deinit();
    bail_error(err);

bail:
    if (!nvsd_mgmt_thrd_initd) {
	pthread_cond_signal(&nvsd_gcl_cond_var);
	pthread_mutex_unlock(&nvsd_gcl_mutex);
	lc_log_debug(LOG_NOTICE, ("error: initing nvsd mgmt thread"));
	exit(1);
    }
    return err;

}	/* end of nvsd_mgmt_thread_hdl() */

int
nvsd_mgmt_main_tgl_pxy(int pxy_enable)
{

    lc_launch_params *lp = NULL;
    int err = 0;

    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, lp);

    err = ts_new_str(&(lp->lp_path), pxe_tgl_sh);
    bail_error(err);

    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 1, pxe_tgl_sh);
    bail_error(err);

    if (pxy_enable == 0) {
	err = tstr_array_append_str(lp->lp_argv, "0");
    } else if (pxy_enable == 1) {
	err = tstr_array_append_str(lp->lp_argv, "1");
    } else if (pxy_enable == 2) {
	err = tstr_array_append_str(lp->lp_argv, "2");
    }

    bail_error(err);

    lp->lp_wait = false;

    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_INFO;

    err = lc_launch(lp, NULL);
    bail_error(err);

bail:
    lc_launch_params_free(&lp);
    return err;
}

int
nvsd_system_tp_set_nvsd_state(int state)
{
    FILE *FP = NULL;
    int rv = -1;

    FP = fopen("/proc/tproxy/nvsd_state", "w");
    if (FP) {
	rv = fprintf(FP, "%d", state);
	fclose(FP);
	return 0;
    }
    return -1;
}

static void
report_segfault(int signo, siginfo_t * sinf, void *arg)
{
    static int already_segfault = 0;
    UNUSED_ARGUMENT(signo);
    UNUSED_ARGUMENT(sinf);
    UNUSED_ARGUMENT(arg);

    if (already_segfault) {
	/* not doing anything now as we have already been called and
	 * we are not doing anything else
	 */
	return;
    }
    already_segfault++;

    /*
     * User space L3 pxy For any of the below signals, Enable Ip-fwd and
     * remove rules 
     */
    if (signo == SIGSEGV ||
	    signo == SIGABRT || signo == SIGQUIT || signo == SIGFPE) {
	/*
	 * Call the script to save the iptable entry to a file, remove the
	 * entries and enable ipfwd 
	 */
	if (user_space_l3_pxy)
	    nvsd_mgmt_main_tgl_pxy(1);

	if (kernel_space_l3_pxy) {
	    // Notify TPROXY module
	    nvsd_system_tp_set_nvsd_state(0 /* 0 - NVSD is down */ );
	}

    }

    lbt_backtrace_log(LOG_NOTICE, "mod-delivery ");
    signal(signo, SIG_DFL);
    raise(signo);
}

int
setup_signal_handler(void)
{
    struct sigaction act;

    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = report_segfault;
    sigfillset(&act.sa_mask);
    if (sigaction(SIGSEGV, &act, NULL))
	return -1;

    if (sigaction(SIGABRT, &act, NULL))
	return -1;

    if (sigaction(SIGQUIT, &act, NULL))
	return -1;

    return 0;
}

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_thread_init ()
 *	purpose : thread handler for the management module
 */
int
nvsd_mgmt_thread_init(void)
{
    GError *t_err = NULL;

    /*
     * Set up signal handler for SEGFAULT 
     */
    setup_signal_handler();

    pthread_mutex_lock(&nvsd_gcl_mutex);
    /*
     * As a test create a thread here 
     */
    mgmt_thread = g_thread_create_full((GThreadFunc) nvsd_mgmt_thread_hdl,
	    NULL, (128 * 1024), TRUE,
	    FALSE, G_THREAD_PRIORITY_NORMAL, &t_err);
    if (NULL == mgmt_thread) {
	lc_log_basic(LOG_ERR, "Mgmt Thread Creation Failed !!!!");
	g_error_free(t_err);
	pthread_mutex_unlock(&nvsd_gcl_mutex);
	return 1;
    }

    pthread_cond_wait(&nvsd_gcl_cond_var, &nvsd_gcl_mutex);

    if (nvsd_mgmt_thrd_initd == 1) {
	lc_log_basic(LOG_NOTICE, "Management Initialized");
    }

    pthread_mutex_unlock(&nvsd_gcl_mutex);

    return 0;
}	/* end of nvsd_mgmt_thread_init() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_init ()
 *	purpose : initializes the mgmt interface to mgmtd
 *		this logic is taken from their template
 */
int
nvsd_mgmt_init(void)
{
    int i;
    int err = 0;
    struct stat stat_info;
    bn_binding_array *ret_bindings = NULL;

    /*
     * The LOCAL4 facility is coordinated with an additional facility
     * we add in our graft point in src/include/logging.inc.h
     */

    /*
     * TEMPORARY FIX -- Feb 1st 2009 - Ram 
     */
    /*
     * Check if the temp file exists 
     */
    err = stat("/tmp/.reinit-disk", &stat_info);
    if (0 == err) {
	/*
	 * File exists hence invoke disk reinit script 
	 */
	system("/opt/nkn/bin/reinit_config_disks.sh");
	unlink("/tmp/.reinit-disk");
	lc_log_basic(LOG_NOTICE,
		I_("Re-initializing the disks : COMPLETED !!!!!"));
    }

    /*
     * END OF TEMPORARY FIX 
     */

    err = lew_init(&nvsd_lwc);
    bail_error(err);

    /*
     * Register to hear about all the signals we care about.
     * These are automatically persistent, which is fine.
     */
    for (i = 0; i < (int) nvsd_num_signals_handled; ++i) {
	err = lew_event_reg_signal
	    (nvsd_lwc, &(nvsd_event_signals[i]), nvsd_signals_handled[i],
	     nvsd_handle_signal, NULL);
	bail_error(err);
    }

    /*
     * Connect/Create GCL sessions, intialize mgmtd interface 
     */
    err = nvsd_mdc_init();
    bail_error(err);

    // TODO
    /*
     * Get all the nodes of our interest here 
     */
    err = nvsd_mgmt_get_nodes(mgmt_query_data, &ret_bindings);
    bail_error(err);
    /*
     * Get and parse config for the various modules 
     */
    // TODO
    /*
     * Parse all the nodes and fill nvsd's state 
     */

    mgmt_log_init(nvsd_mcc, "/nkn/debug/log/ps/nvsd/level");

    nvsd_system_cfg_query(ret_bindings);
    nvsd_resource_mgr_cfg_query(ret_bindings);
    nvsd_pe_srvr_cfg_query(ret_bindings);
    nvsd_errlog_cfg_query(ret_bindings);
    nvsd_strlog_cfg_query(ret_bindings);
    nvsd_network_cfg_query(ret_bindings);
    nvsd_http_cfg_query(ret_bindings);
    nvsd_rtstream_cfg_query(ret_bindings);
    nvsd_buffermgr_cfg_query(ret_bindings);
    nvsd_fmgr_cfg_query(ret_bindings);
    nvsd_tfm_cfg_query(ret_bindings);
    nvsd_am_cfg_query(ret_bindings);
    nvsd_mm_cfg_query(ret_bindings);
    nvsd_virtual_player_cfg_query(ret_bindings);
    nvsd_pub_point_cfg_query();	// TOOD not used
    nvsd_server_map_cfg_query(ret_bindings);
    nvsd_cluster_cfg_query(ret_bindings);    // Has to be loaded before namespace 
    nvsd_url_filter_cfg_query(ret_bindings); // Has to be loaded before namespace
    nvsd_namespace_cfg_query(ret_bindings);
    nvsd_acclog_cfg_query(ret_bindings);
    nvsd_l4proxy_cfg_query(ret_bindings);

    nvsd_namespace_uf_cfg_query(ret_bindings);
    /*
     * The time node maintains the time at which nvsd restart and/or reset
     * counter cli was applied.Hence calling the below function to
     * update the time in the time node
     */
    err = nvsd_set_time_for_reset_counter();
    bail_error(err);

bail:
    bn_binding_array_free(&ret_bindings);
    return (err);
}	/* end of nvsd_mgmt_init() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_deinit ()
 *	purpose : do the cleanup activity for exiting
 */
int
nvsd_mgmt_deinit(void)
{
    int err = 0;

    err = mdc_wrapper_deinit(&nvsd_mcc);
    bail_error(err);

    err = lew_deinit(&nvsd_lwc);
    bail_error(err);

bail:
    lc_log_basic(LOG_NOTICE, _("nvsd daemon exiting with code %d"), err);
    lc_log_close();
    return (err);

}	/* end of nvsd_mgmt_deinit() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mdc_init ()
 *	purpose : initializes the mgmt interface to mgmtd
 *		this logic is taken from their template
 */
int
nvsd_mdc_init(void)
{
    int err = 0;
    mdc_wrapper_params *mwp = NULL;
    fdic_handle *fdich = NULL;

    err = mdc_wrapper_params_get_defaults(&mwp);
    bail_error(err);

    mwp->mwp_lew_context = nvsd_lwc;
    mwp->mwp_gcl_client = gcl_client_nvsd;

    mwp->mwp_request_start_func = nvsd_mgmt_req_start;
    mwp->mwp_request_complete_func = nvsd_mgmt_req_complete;
    mwp->mwp_session_close_func = nvsd_mgmt_handle_session_close;
    mwp->mwp_event_request_func = nvsd_mgmt_recv_event_request;
    mwp->mwp_action_request_func = nvsd_mgmt_recv_action_request;
    mwp->mwp_query_request_func = nvsd_mgmt_recv_query_request;

    err = mdc_wrapper_init(mwp, &nvsd_mcc);
    bail_error(err);

    err = lc_random_seed_nonblocking_once();
    bail_error(err);

    err = mdc_wrapper_get_fdic_handle(nvsd_mcc, &fdich);
    bail_error_null(err, fdich);

    err = mdc_bl_init(fdich, &nvsd_blc);
    bail_error(err);

    /*
     * NOTE: GCL Request/Response is not synchronus 
     */
    /*
     * We can end up getting a new gcl request, while waiting for a gcl
     * response, It is expected to do sanity check, to avoid wierd states 
     */

    /*
     * Register to receive config changes.  We care about:
     *   1. Any of our own configuration nodes.
     */
#define LICENSE
#ifdef LICENSE
    // 2: Change to the MFD license (When active status changes)
#endif /*LICENSE*/
    err = mdc_send_action_with_bindings_str_va
	(nvsd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add",
#ifndef LICENSE
	 7,
#else
	 8,
#endif
	 "event_name", bt_name, mdc_event_dbchange, "match/1/pattern",
	 bt_string, "/nkn/nvsd/**", "match/2/pattern", bt_string,
	 "/nkn/l4proxy/**", "match/3/pattern", bt_string,
	 "/nkn/debug/log/all/level", "match/4/pattern", bt_string,
	 "/nkn/debug/log/ps/nvsd/level", "match/5/pattern", bt_string,
	 "/nkn/errorlog/**", "match/6/pattern", bt_string,
	 "/nkn/accesslog/config/profile/*", "match/7/pattern", bt_string,
	 "/nkn/accesslog/config/profile/*/format"
#ifdef LICENSE
	 , "match/8/pattern", bt_string, "/nkn/mfd/licensing/**"
#endif	  /*LICENSE*/
	);
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va
	(nvsd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	 "event_name", bt_name, "/net/interface/events/link_up");

    bail_error(err);

    err = mdc_send_action_with_bindings_str_va
	(nvsd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	 "event_name", bt_name, "/net/interface/events/link_down");
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va
	(nvsd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	 "event_name", bt_name, "/net/interface/events/state_change");
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va
	(nvsd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	 "event_name", bt_name, "/nkn/nvsd/resource_mgr/state/update");
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va
	(nvsd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	 "event_name", bt_name, "/nkn/geodb/events/installation");
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va
	(nvsd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	 "event_name", bt_name,
	 "/nkn/nvsd/server-map/notify/map-status-change");
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va
	(nvsd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	 "event_name", bt_name, "/nkn/nvsd/http/notify/delivery_intf_add");
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va
	(nvsd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	 "event_name", bt_name, "/nkn/nvsd/http/notify/delivery_intf_del");
    bail_error(err);

    /*
     * Registering for restart event as nvsd needs to know when ssld restarts 
     */
    err = mdc_send_action_with_bindings_str_va
	(nvsd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	 "event_name", bt_name, "/pm/events/proc/restart");
    bail_error(err);

    err = mdc_send_action_with_bindings_str_va
	(nvsd_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 1,
	"event_name", bt_name, "/nkn/nvsd/url-filter/event/map-status-change");
    bail_error(err);

bail:
    mdc_wrapper_params_free(&mwp);
    if (err) {
	lc_log_debug(LOG_ERR, _("Unable to connect to the management daemon"));
    }

    return (err);
}	/* end of nvsd_mdc_init () */

int
nvsd_mgmt_retry_handler(int fd,
	short event_type, void *data, lew_context * ctxt)
{
    int err = 0;

    UNUSED_ARGUMENT(fd);
    UNUSED_ARGUMENT(event_type);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(ctxt);

    err = nvsd_mdc_init();
    bail_error(err);

bail:
    return err;
}

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_handle_session_close ()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_mgmt_handle_session_close(int fd,
	fdic_callback_type cbt,
	void *vsess, void *gsec_arg)
{
    int err = 0;
    gcl_session *gsec_session = vsess;

    (void) gsec_session;
    UNUSED_ARGUMENT(fd);
    UNUSED_ARGUMENT(cbt);
    UNUSED_ARGUMENT(vsess);
    UNUSED_ARGUMENT(gsec_arg);
    /*
     * If we're exiting, this is expected and we shouldn't do anything.
     * Otherwise, it's an error, so we should complain and reestablish
     * the connection.
     */
    if (nvsd_exiting) {
	goto bail;
    }

    lc_log_basic(LOG_ERR, _("Lost connection to mgmtd; reconnecting"));

    /*
     * trigger an timer event which will re-try 
     */

    /*
     * If we haven't connected to mgmtd even once bail out 
     */
    if (!nvsd_mgmt_thrd_initd)
	goto bail;

    /*
     * Reconnect to mgmtd 
     */
    err = lew_event_reg_timer(nvsd_lwc, &nvsd_mgmt_retry_event,
	    nvsd_mgmt_retry_handler, NULL, 5000);
    bail_error(err);

bail:
    return (err);
}	/* end of nvsd_mgmt_handle_session_close() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_handle_action_request ()
 *	purpose : picked up as is from samara's demo
 */
static int
nvsd_mgmt_handle_action_request(gcl_session * sess,
	bn_request ** inout_action_request, void *arg)
{
    int err = 0;
    bn_msg_type req_type = bbmt_none;
    bn_binding_array *bindings = NULL;
    tstring *action_name = NULL;
    tstring *t_name = NULL;
    bn_response *resp = NULL;
    char *buff = NULL;
    tstr_array *tok_array = NULL;
    tstring *t_regex = NULL;
    tstring *namespace_name = NULL;
    const char *err_msg = NULL;
    char errorbuf[1024];
    char *lst_nsuids = NULL;
    char *lst_ns_precedence = NULL;
    tstring *t_ns = NULL;
    tstring *t_type = NULL;
    bn_response *dns_cache_resp = NULL;
    char *lst_dnscache = NULL;
    tstr_array *dnscache_tok_array = NULL;

    UNUSED_ARGUMENT(arg);

    bail_null(inout_action_request);
    bail_null(*inout_action_request);

    err = bn_request_get
	(*inout_action_request, &req_type, NULL, true, &bindings, NULL, NULL);
    bail_error(err);
    bail_require(req_type == bbmt_action_request);

    err = bn_action_request_msg_get_action_name(*inout_action_request,
	    &action_name);
    bail_error(err);

    lc_log_basic(LOG_INFO, "Received action %s", ts_str(action_name));

    /*
     * Check to see if server initialization is completed 
     */
    if (!nkn_system_inited) {
	lc_log_debug(LOG_WARNING,
		I_
		("Server initialization in progress, cannot process  action %s"),
		ts_str(action_name));
	err =
	    bn_response_msg_create_send(sess, bbmt_action_response,
		    bn_request_get_msg_id
		    (*inout_action_request), 1,
		    _
		    ("error : server initialization in progress, please try after sometime"),
		    0);
	goto bail;
    }
    /*
     * BUG FIX:7322 Removing disk cache related action handling from
     * nvsd-mgmt thread. The disk actions are handled in disk-mgmt thread of
     * nvsd 
     */

    /*
     * Check to see if the action is allowed for the time frame 
     */
    /*
     * "/nkn/nvsd/namespace/actions/rate_limit" action could be used as
     * generic action for rate limiting commands 
     */
    /*
     * Any command that wants to rate limit can trigger this action before it 
     * does the real stuff 
     */

    /*
     * Dummy handling of rate-limit function.This will be removed
     * once it is confirmed this feature is not needed.
     */
    if (ts_equal_str(action_name, "/nkn/nvsd/actions/cmd/rate_limit", false)) {
	err = bn_response_msg_create_send
	    (sess, bbmt_action_response,
	     bn_request_get_msg_id(*inout_action_request), 0, _(""), 0);
	bail_error(err);
	goto bail;
    }

    /*
     * Start processing based on the action 
     */

    /*
     * DISPLAY AM RANKS action handler 
     */
    if (ts_equal_str(action_name, "/nkn/nvsd/am/actions/rank", false)) {
	err = nvsd_mgmt_handle_show_rank_action(sess, bindings,
		*inout_action_request);
	bail_error(err);
    }

    /*
     * DISPLAY AM TIER action handler 
     */
    else if (ts_equal_str(action_name, "/nkn/nvsd/am/actions/tier", false)) {
	err = nvsd_mgmt_handle_show_tier_action(sess, bindings,
		*inout_action_request);
	bail_error(err);
    }
    /*
     * DISPLAY Buffer manager object request list action handler 
     */
    else if (ts_equal_str
	    (action_name, "/nkn/nvsd/buffermgr/actions/object-list", false)) {
	err =
	    nvsd_mgmt_handle_buffermgr_object_list_action(sess, bindings,
		    *inout_action_request);
	bail_error(err);
    }

    /*
     * FASTSTART action handler 
     */
    else if (lc_is_prefix
	    ("/nkn/nvsd/virtual_player/actions/fast_start/",
	     ts_str(action_name), false)) {
	tstring *t_value = NULL;

	err = bn_response_msg_create_send
	    (sess, bbmt_action_response,
	     bn_request_get_msg_id(*inout_action_request), 0, _(""), 0);

	/*
	 * Get the virtual player name field 
	 */
	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "name", NULL, &t_name);
	bail_error(err);

	/*
	 * Get the value field 
	 */
	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "value", NULL, &t_value);
	bail_error(err);

	if (ts_equal_str (action_name,
		    "/nkn/nvsd/virtual_player/actions/fast_start/uri_query",
		    false)) {
	    /*
	     * Get the nodes set now 
	     */
	    nvsd_mgmt_virtual_player_action_hdlr(NVSD_VP_FS_URI_QUERY,
		    ts_str(t_name),
		    ts_str(t_value));
	} else if (ts_equal_str (action_name,
		    "/nkn/nvsd/virtual_player/actions/fast_start/time",
		    false)) {
	    /*
	     * Get the nodes set now 
	     */
	    nvsd_mgmt_virtual_player_action_hdlr(NVSD_VP_FS_TIME,
		    ts_str(t_name),
		    ts_str(t_value));
	} else if (ts_equal_str (action_name,
		    "/nkn/nvsd/virtual_player/actions/fast_start/size",
		    false)) {
	    /*
	     * Get the nodes set now 
	     */
	    nvsd_mgmt_virtual_player_action_hdlr(NVSD_VP_FS_SIZE,
		    ts_str(t_name),
		    ts_str(t_value));
	}
	ts_free(&t_value);
    }

    /*
     * GET LOST UIDS action handler 
     */
    else if (ts_equal_str
	    (action_name, "/nkn/nvsd/namespace/actions/get_lost_uids",
	     false)) {

	int aindex = 0;
	int len = 0;

	lst_nsuids = get_uids_in_disk();
	if (lst_nsuids) {

	    err = bn_action_response_msg_create(&resp,
		    bn_request_get_msg_id
		    (*inout_action_request), 0,
		    NULL);
	    bail_error(err);

	    err = ts_tokenize_str(lst_nsuids, '\n', 0, 0, 0, &tok_array);
	    bail_error(err);

	    len = tstr_array_length_quick(tok_array);
	    for (aindex = 0; aindex < len; ++aindex) {
		buff = smprintf("%d", (aindex + 1));
		bail_null(buff);

		err = bn_response_msg_append_new_binding(resp,
			buff,
			bt_string, 0,
			ts_str
			(tstr_array_get_quick
			 (tok_array, aindex)),
			0);
		bail_error(err);
		safe_free(buff);
	    }

	    tstr_array_free(&tok_array);
	    err = bn_response_msg_set_return_code_msg(resp, 0, NULL);
	    bail_error(err);

	    err = bn_response_msg_send(sess, resp);
	    bail_error(err);

	} else {
	    err = bn_response_msg_create_send
		(sess, bbmt_action_response,
		 bn_request_get_msg_id(*inout_action_request), 1, NULL, 0);
	    bail_error(err);
	}
    }

    /*
     * GET NAMESPACE PRECEDENCE LIST action handler 
     */
    else if (ts_equal_str (action_name,
	     "/nkn/nvsd/namespace/actions/get_namespace_precedence_list",
	     false)) {

	int aindex = 0;
	int len = 0;

	lst_ns_precedence = get_namespace_precedence_list();
	if (lst_ns_precedence && strlen(lst_ns_precedence) > 0) {

	    err = bn_action_response_msg_create(&resp,
		    bn_request_get_msg_id
		    (*inout_action_request), 0,
		    NULL);
	    bail_error(err);

	    err = ts_tokenize_str(lst_ns_precedence, '\n', 0, 0, 0, &tok_array);
	    bail_error(err);

	    len = tstr_array_length_quick(tok_array);
	    for (aindex = 0; aindex < len; ++aindex) {
		if (ts_length(tstr_array_get_quick(tok_array, aindex)) == 0)
		    break;
		buff = smprintf("%d", (aindex + 1));
		bail_null(buff);

		err = bn_response_msg_append_new_binding(resp,
			buff,
			bt_string, 0,
			ts_str
			(tstr_array_get_quick
			 (tok_array, aindex)),
			0);
		bail_error(err);
		safe_free(buff);
	    }

	    tstr_array_free(&tok_array);
	    err = bn_response_msg_set_return_code_msg(resp, 0, NULL);
	    bail_error(err);

	    err = bn_response_msg_send(sess, resp);
	    bail_error(err);

	} else {
	    err = bn_response_msg_create_send
		(sess, bbmt_action_response,
		 bn_request_get_msg_id(*inout_action_request), 1, NULL, 0);
	    bail_error(err);
	}
    }

    /*
     * GET RAMCACHE LIST action handler 
     */
    else if (ts_equal_str(action_name,
		"/nkn/nvsd/namespace/actions/get_ramcache_list", false)) {
	tstring *t_namespace = NULL;
	tstring *t_uid = NULL;
	tstring *t_filename = NULL;
	uint32 nobjects = 0;
	bn_binding *binding = NULL;

	/*
	 * Get the namespace field 
	 */
	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "namespace", NULL, &t_namespace);
	bail_error(err);

	/*
	 * Get the uid field 
	 */
	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "uid", NULL, &t_uid);
	bail_error(err);

	/*
	 * Get the filename field 
	 */
	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "filename", NULL, &t_filename);
	bail_error(err);

	/*
	 * Call the action logic that get the ramcache object list 
	 */
	nobjects =
	    nsvd_mgmt_ns_get_ramcache_list(ts_str(t_namespace), ts_str(t_uid),
		    ts_str(t_filename));

	err = bn_action_response_msg_create(&resp,
		bn_request_get_msg_id
		(*inout_action_request), 0, NULL);
	bail_error(err);

	err = bn_binding_new_uint32(&binding,
		    "/nkn/nvsd/namespace/num_ram_objects",
		    ba_value, bt_uint32, nobjects);
	bail_error(err);

	err = bn_action_response_msg_append_binding_takeover(resp, &binding, 0);
	bail_error(err);

	err = bn_response_msg_send(sess, resp);
	bail_error(err);

	ts_free(&t_namespace);
	ts_free(&t_filename);
	ts_free(&t_uid);
    } else if (ts_equal_str
	    (action_name, "/nkn/nvsd/namespace/actions/is_obj_cached", false)) {
	err = nvsd_mgmt_handle_is_obj_cached(sess, bindings,
		*inout_action_request);
	bail_error(err);

    }

    /*
     * VALIDATE DOMAIN REGEX action handler 
     */
    else if (ts_equal_str (action_name,
		"/nkn/nvsd/uri/actions/validate_domain_regex",
		false)) {
	err = bn_binding_array_get_value_tstr_by_name(bindings, "regex", NULL,
		&t_regex);
	bail_error(err);

	if (nkn_valid_regex(ts_str(t_regex), errorbuf, sizeof (errorbuf))) {
	    err = 0;			// valid regex
	    err_msg = 0;
	} else {
	    err = 1;
	    errorbuf[sizeof (errorbuf) - 1] = '\0';
	    err_msg = errorbuf;
	}
	err = bn_response_msg_create_send(sess, bbmt_action_response,
		bn_request_get_msg_id
		(*inout_action_request), err, err_msg,
		0);
	bail_error(err);
    }

    /*
     * Handle UPDATED SERVER-MAP 
     */
    else if (ts_equal_str(action_name,
		"/nkn/nvsd/server-map/actions/updated-map", false)) {

	err = nvsd_mgmt_handle_server_map_refresh_action(sess, bindings,
		*inout_action_request);
	bail_error(err);

    }
    /*
     * Handle RESET-COUNTER action 
     */
    else if (ts_equal_str (action_name,
		"/stat/nkn/actions/reset-counter", false)) {

	err = bn_response_msg_create_send(sess,
		bbmt_action_response,
		bn_request_get_msg_id
		(*inout_action_request), err,
		"Request to reset counter sent", 0);
	bail_error(err);
	err = nvsd_mgmt_reset_counter_backup_data();
	bail_error(err);
    } else if (ts_equal_str (action_name,
		"/stat/nkn/actions/reset-http-counter", false)) {

	err = bn_response_msg_create_send(sess,
		bbmt_action_response,
		bn_request_get_msg_id
		(*inout_action_request), err,
		"Request to reset http counter sent",
		0);
	bail_error(err);
    } else if (ts_equal_str (action_name,
		"/stat/nkn/actions/reset-namespace-counter", false)) {
	char resp_str[256] = { 0 };
	err = bn_binding_array_get_value_tstr_by_name(bindings,
		"namespace_name", NULL, &namespace_name);
	if (!namespace_name) {
	    // Reset counters for all namespaces
	    snprintf(resp_str, sizeof (resp_str),
		    "Request to reset all namespace counter sent");
	} else {
	    // Reset one namespace counters
	    snprintf(resp_str, sizeof (resp_str),
		    "Request to reset %s namespace counter sent",
		    ts_str(namespace_name));
	}

	err = bn_response_msg_create_send(sess,
		bbmt_action_response,
		bn_request_get_msg_id
		(*inout_action_request), err,
		resp_str, 0);
	bail_error(err);
    }
    /*
     * GET DNS Cache action handler 
     */
    else if (ts_equal_str
	    (action_name, "/nkn/nvsd/network/actions/get_dns_cache", false)) {

	int aindex = 0;
	int len = 0;
	char *bn_name = NULL;

	lst_dnscache =
	    (char *) nkn_calloc_type((MAX_DNS_INFO * 50), sizeof (char),
		    mod_mgmt_charbuf);
	if (!lst_dnscache)
	    goto bail;

	dns_get_cache_list(lst_dnscache, 50);

	if (lst_dnscache) {

	    err = bn_action_response_msg_create(&dns_cache_resp,
		    bn_request_get_msg_id
		    (*inout_action_request), 0,
		    NULL);
	    bail_error(err);

	    err = ts_tokenize_str(lst_dnscache, '\n', 0, 0, 0,
			&dnscache_tok_array);
	    bail_error(err);

	    len = tstr_array_length_quick(dnscache_tok_array);

	    for (aindex = 0; aindex < len; ++aindex) {
		bn_name = smprintf("dns_cache_%d", (aindex + 1));
		bail_null(bn_name);

		err = bn_response_msg_append_new_binding(dns_cache_resp,
			bn_name, bt_string, 0,
			ts_str(tstr_array_get_quick(dnscache_tok_array, aindex)),
			0);
		bail_error(err);
		safe_free(bn_name);
	    }

	    tstr_array_free(&dnscache_tok_array);

	    err = bn_response_msg_set_return_code_msg(dns_cache_resp, 0, NULL);
	    bail_error(err);

	    err = bn_response_msg_send(sess, dns_cache_resp);
	    bail_error(err);

	} else {
	    err = bn_response_msg_create_send
		(sess, bbmt_action_response,
		 bn_request_get_msg_id(*inout_action_request), 1, NULL, 0);
	    bail_error(err);
	}
    }
    /*
     * DELETE DNS Cache - ALL entries action handler 
     */
    else if (ts_equal_str
	    (action_name, "/nkn/nvsd/network/actions/del_dns_cache_all",
	     false)) {

	int del_cnt = 0;

	del_cnt = dns_delete_cache_all();

	if (del_cnt) {
	    err = bn_response_msg_create_send(sess,
		    bbmt_action_response,
		    bn_request_get_msg_id
		    (*inout_action_request), 0,
		    "\t Cache Entries deleted", 0);
	    bail_error(err);
	} else {
	    err = bn_response_msg_create_send
		(sess, bbmt_action_response,
		 bn_request_get_msg_id(*inout_action_request), 1,
		 "\t No Cache Entries Found", 0);
	    bail_error(err);
	}
    } /* DELETE DNS Cache Entry action handler */
    else if (ts_equal_str
	    (action_name, "/nkn/nvsd/network/actions/del_dns_cache_entry",
	     false)) {

	int del_status = 0;

	tstring *t_domainname = NULL;
	char *c_domain = NULL;

	/*
	 * Get the domain field 
	 */
	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "domain", NULL, &t_domainname);
	bail_error(err);

	c_domain = smprintf("%s", ts_str(t_domainname));

	del_status = dns_delete_cache_fqdn(c_domain);

	if (!del_status) {
	    err = bn_response_msg_create_send(sess,
		    bbmt_action_response,
		    bn_request_get_msg_id
		    (*inout_action_request), 0,
		    "\t Requested Cache Entry is deleted",
		    0);
	    bail_error(err);
	} else {
	    err = bn_response_msg_create_send
		(sess, bbmt_action_response,
		 bn_request_get_msg_id(*inout_action_request), 1,
		 "\t Requested Cache Entry is NOT deleted", 0);
	    bail_error(err);
	}
	safe_free(c_domain);
    } else if (ts_equal_str (action_name,
		"/nkn/nvsd/network/actions/get_dns_cache_fqdn", false)) {

	    int fqdn_entry_absent = 0;
	    int len = 0, aindex = 0;
	    tstring *t_domainname = NULL;
	    char c_domain[MAX_DNS_INFO] = { 0, };
	    char *bn_name = NULL;

	    lst_dnscache =
		(char *) nkn_calloc_type((MAX_DNS_INFO), sizeof (char),
			mod_mgmt_charbuf);

	    if (!lst_dnscache)
		goto bail;

	    /*
	     * Get the domain field 
	     */
	    err = bn_binding_array_get_value_tstr_by_name(bindings, "domain", 
		    NULL, &t_domainname);
	    bail_error(err);

	    if (!t_domainname)
		goto bail;

	    snprintf(c_domain, MAX_DNS_INFO, "%s", ts_str(t_domainname));

	    lc_log_basic(LOG_DEBUG,
		    I_("Requesting Cache Entry from DNS cache '%s'"),
		    c_domain);
	    fqdn_entry_absent = dns_get_cache_fqdn(c_domain, lst_dnscache);
	    lc_log_basic(LOG_DEBUG, I_("Got DNS details from DNS cache '%s'"),
		    c_domain);

	    if (fqdn_entry_absent) {
		err = bn_response_msg_create_send(sess, bbmt_action_response,
			bn_request_get_msg_id
			(*inout_action_request), 0, NULL,
			0);
		bail_error(err);
		lc_log_basic(LOG_DEBUG,
			I_
			("Requested Cache Entry '%s' is not present in DNS cache"),
			c_domain);
	    } else {
		err =
		    ts_tokenize_str(lst_dnscache, '\n', 0, 0, 0,
			    &dnscache_tok_array);
		bail_error(err);

		len = tstr_array_length_quick(dnscache_tok_array);

		err = bn_action_response_msg_create(&dns_cache_resp,
			bn_request_get_msg_id
			(*inout_action_request), 0,
			NULL);
		bail_error(err);

		lc_log_basic(LOG_DEBUG, I_("Entry '%s' is present in DNS cache"),
			c_domain);
		for (aindex = 0; aindex < len; ++aindex) {
		    bn_name = smprintf("dns_cache_%d", aindex);
		    bail_null(bn_name);

		    err = bn_response_msg_append_new_binding(dns_cache_resp,
			    bn_name,
			    bt_string, 0,
			    ts_str (tstr_array_get_quick (dnscache_tok_array,
			      aindex)), 0);
		    bail_error(err);
		    safe_free(bn_name);
		}
		tstr_array_free(&dnscache_tok_array);

		err = bn_response_msg_set_return_code_msg(dns_cache_resp, 0, NULL);
		bail_error(err);

		err = bn_response_msg_send(sess, dns_cache_resp);
		bail_error(err);
	    }
	} else {
	    lc_log_basic(LOG_DEBUG, I_("Got unexpected action %s"),
		    ts_str(action_name));
	    err = bn_response_msg_create_send
		(sess, bbmt_action_response,
		 bn_request_get_msg_id(*inout_action_request), 1,
		 _("error : action not yet supported"), 0);
	    goto bail;
	}

bail:
    safe_free(buff);
    safe_free(lst_nsuids);
    safe_free(lst_ns_precedence);
    ts_free(&action_name);
    ts_free(&t_name);
    ts_free(&t_ns);
    ts_free(&t_type);
    ts_free(&t_regex);
    tstr_array_free(&tok_array);
    bn_binding_array_free(&bindings);
    bn_response_msg_free(&resp);
    safe_free(lst_dnscache);
    bn_response_msg_free(&dns_cache_resp);
    return (err);
}	/* end of nvsd_mgmt_handle_action_request() */

int
nvsd_mgmt_recv_action_request(gcl_session * sess,
	bn_request ** inout_request, void *arg)
{
    int err = 0;
    tbool handled = false;

    UNUSED_ARGUMENT(arg);

    err = mdc_bl_backlog_or_handle_message_takeover
	(nvsd_blc, sess, inout_request, nvsd_mgmt_handle_action_request,
	 NULL, &handled);
    bail_error(err);

bail:
    return err;
}

int
nvsd_mgmt_recv_query_request(gcl_session * sess,
	bn_request ** inout_request, void *arg)
{
    int err = 0;
    tbool handled = false;

    UNUSED_ARGUMENT(arg);

    err = mdc_bl_backlog_or_handle_message_takeover
	(nvsd_blc, sess, inout_request, nvsd_mgmt_handle_query_request,
	 NULL, &handled);
    bail_error(err);

bail:
    return err;
}

int
nvsd_mgmt_recv_event_request(gcl_session * sess,
	bn_request ** inout_request, void *arg)
{
    int err = 0;
    tbool handled = false;

    UNUSED_ARGUMENT(arg);

    err = mdc_bl_backlog_or_handle_message_takeover
	(nvsd_blc, sess, inout_request, nvsd_mgmt_handle_event_request,
	 NULL, &handled);
    bail_error(err);

bail:
    return err;
}

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_handle_event_request ()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_mgmt_handle_event_request(gcl_session * sess,
	bn_request ** inout_request, void *arg)
{
    if_info_state_change_t interface_change_info;
    int err = 0;
    bn_msg_type msg_type = bbmt_none;
    bn_binding_array *old_bindings = NULL, *new_bindings = NULL;
    tstring *event_name = NULL;
    bn_binding_array *bindings = NULL;
    uint32 num_callbacks = 0, i = 0;
    void *data = NULL;
    tbool rechecked_licenses = false;

    (void) num_callbacks;
    (void) i;
    (void) data;
    UNUSED_ARGUMENT(sess);
    UNUSED_ARGUMENT(arg);

    bail_null(inout_request);
    bail_null(*inout_request);

    err = bn_request_get(*inout_request, &msg_type, NULL, false, &bindings,
	    NULL, NULL);
    bail_error(err);

    bn_request_msg_dump(NULL, *inout_request, LOG_DEBUG,
	    "Events recieved by NVSD");

    bail_require(msg_type == bbmt_event_request);

    err = bn_event_request_msg_get_event_name(*inout_request, &event_name);
    bail_error_null(err, event_name);

    lc_log_basic(jnpr_log_level, "Received event: %s", ts_str(event_name));

    memset(&interface_change_info, 0, sizeof (if_info_state_change_t));

    if (ts_equal_str(event_name, mdc_event_dbchange, false)) {
	err = mdc_event_config_change_notif_extract(bindings, &old_bindings,
		&new_bindings, NULL);
	bail_error(err);
	err = nvsd_config_handle_set_request(old_bindings, new_bindings);
	bail_error(err);

	//nvsd_mgmt_namespace_lock();
    //update_namespace_filter_map("0");
	//nvsd_mgmt_namespace_unlock();
    //nkn_namespace_config_update(NULL);
    } else if (ts_equal_str
	    (event_name, "/nkn/nvsd/resource_mgr/state/update", false)) {
	/*
	 * update the resource pool value 
	 */
	lc_log_debug(jnpr_log_level, "receive update event ");
	err = bn_binding_array_foreach(bindings, nvsd_rsrc_update_event, NULL);
	bail_error(err);
    } else if (ts_equal_str(event_name, "/net/interface/events/link_up", false)) {
	tstring *t_intfname = NULL;

	/*
	 * Call the callback that handles link up event 
	 */
	err = bn_binding_array_foreach(bindings,
		nvsd_interface_up_cfg_handle_change,
		&rechecked_licenses);

	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "ifname", NULL, &t_intfname);
	bail_error(err);

	strncpy(interface_change_info.intfname, ts_str(t_intfname),
		ts_length(t_intfname));

	get_other_interface_details(&interface_change_info);
	interface_change_info.chng_type = INTF_LINK_UP;
	update_interface_events(&interface_change_info);

	ts_free(&t_intfname);
    } else if (ts_equal_str(event_name, "/net/interface/events/link_down"
		, false)) {
	tstring *t_intfname = NULL;

	/*
	 * Call the callback that handles link down event 
	 */
	err = bn_binding_array_foreach(bindings,
		nvsd_interface_up_cfg_handle_change,
		&rechecked_licenses);

	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "ifname", NULL, &t_intfname);
	bail_error(err);

	strncpy(interface_change_info.intfname, ts_str(t_intfname),
		ts_length(t_intfname));

	get_other_interface_details(&interface_change_info);
	interface_change_info.chng_type = INTF_LINK_DOWN;
	update_interface_events(&interface_change_info);
	ts_free(&t_intfname);
    } else if (ts_equal_str
	    (event_name, "/net/interface/events/state_change", false)) {

	tstring *t_intfname = NULL;

	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "ifname", NULL, &t_intfname);
	bail_error(err);

	strncpy(interface_change_info.intfname, ts_str(t_intfname),
		ts_length(t_intfname));

	get_other_interface_details(&interface_change_info);
	interface_change_info.chng_type = INTF_LINK_CHNG;
	update_interface_events(&interface_change_info);
	ts_free(&t_intfname);
    } else if (ts_equal_str
	    (event_name, "/nkn/nvsd/http/notify/delivery_intf_add", false)) {

	tstring *t_intfname = NULL;

	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "interface", NULL, &t_intfname);
	bail_error(err);

	strncpy(interface_change_info.intfname, ts_str(t_intfname),
		ts_length(t_intfname));

	get_other_interface_details(&interface_change_info);
	interface_change_info.chng_type = DELIVERY_ADD;
	update_interface_events(&interface_change_info);
	ts_free(&t_intfname);
    } else if (ts_equal_str
	    (event_name, "/nkn/nvsd/http/notify/delivery_intf_del", false)) {
	tstring *t_intfname = NULL;

	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "interface", NULL, &t_intfname);
	bail_error(err);

	strncpy(interface_change_info.intfname, ts_str(t_intfname),
		ts_length(t_intfname));

	get_other_interface_details(&interface_change_info);
	interface_change_info.chng_type = DELIVERY_DEL;
	update_interface_events(&interface_change_info);
	ts_free(&t_intfname);
    } else if (ts_equal_str(event_name, "/nkn/geodb/events/installation", false)) {
	/*
	 * Set the global variable to notify on the geodb installation 
	 */
	lc_log_debug(jnpr_log_level, "receive geodb installation event ");

	geodb_installed = 1;
    } else if (ts_equal_str(event_name, "/pm/events/proc/restart", false)) {
	tstring *t_name = NULL;
	tstring *t_value = NULL;

	err = bn_binding_array_get_first_matching_value_tstr
	    (bindings, "/pm/monitor/process/*", 0, NULL, &t_name, &t_value);
	bail_error_null(err, t_value);

	if (strcmp(ts_str(t_value), "ssld") == 0) {
	    ssld_config_query();
	}
	ts_free(&t_value);
	ts_free(&t_name);
    } else if (ts_equal_str
	    (event_name, "/nkn/nvsd/server-map/notify/map-status-change",
	     false)) {
	tstring *smap_name;
	tstring *file_name;
	tstring *status;

	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "server_map_name", NULL, &smap_name);
	bail_error(err);

	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "status", NULL, &status);
	bail_error(err);

	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "file_name", NULL, &file_name);
	bail_error(err);

	lc_log_debug(LOG_INFO, "Recieved map status change event for %s",
		ts_str(smap_name));

	err =
	    nvsd_handle_smap_status_change(ts_str(smap_name), ts_str(file_name),
		    ts_str(status));
	bail_error(err);
    } else if(ts_equal_str(event_name, "/nkn/nvsd/url-filter/event/map-status-change", false)) {
	bn_binding_array_dump("URL-MAP-EVENT", bindings, LOG_NOTICE );
	err = nvsd_url_filter_update_map(bindings);
	bail_error(err);
	nkn_namespace_config_update (NULL);

    } else {
	lc_log_debug(LOG_WARNING, I_("Received unexpected event %s"),
		ts_str(event_name));
    }

bail:
    bn_binding_array_free(&bindings);
    bn_binding_array_free(&old_bindings);
    bn_binding_array_free(&new_bindings);
    ts_free(&event_name);
    return (err);
}	/* end of nvsd_mgmt_handle_event_request() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_handle_query_request ()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_mgmt_handle_query_request(gcl_session * sess,
	bn_request ** inout_query_request, void *arg)
{
    int err = 0;

    UNUSED_ARGUMENT(arg);

    bail_null(inout_query_request);
    bail_null(*inout_query_request);

    err = mdc_dispatch_mon_request(sess, *inout_query_request,
	    nvsd_mon_handle_get,
	    NULL, nvsd_mon_handle_iterate, NULL);
    bail_error(err);

bail:
    return (err);
}	/* end of nvsd_mgmt_handle_query_request() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_config_handle_set_request ()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_config_handle_set_request(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;

    /*
     * Call the handler for each module 
     */
    err = nvsd_http_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_rtstream_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_buffermgr_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_fmgr_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_tfm_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_network_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_system_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_am_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_mm_module_cfg_handle_change(old_bindings, new_bindings);
    /*
     * For any sumb modules which is associted with namespace, Have the CFG
     * handle change being called before namespace_cfg_handle_change This
     * will make a difference when we talk from JUNOS as the single commit
     * will have all the changes 
     */
    err = nvsd_url_filter_map_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_pe_srvr_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_acclog_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_namespace_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_virtual_player_module_cfg_handle_change(old_bindings,
	    new_bindings);
    err = nvsd_server_map_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_cluster_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_resource_mgr_module_cfg_handle_change(old_bindings, 
	    new_bindings);
    err = nvsd_errlog_module_cfg_handle_change(old_bindings, new_bindings);
    err = nvsd_l4proxy_module_cfg_handle_change(old_bindings, new_bindings);
    err = mgmt_log_cfg_handle_change(old_bindings, new_bindings,
	    "/nkn/debug/log/ps/nvsd/level");
    return err;
}	/* end of nvsd_config_handle_set_request() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_initiate_exit ()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_mgmt_initiate_exit()
{
    int err = 0;
    uint32 num_recs = 0, i = 0;

    (void) num_recs;
    (void) i;
    /*
     * Some occurrences are only OK if they happen while we're
     * exiting, such as getting NULL back for a mgmt request.
     */
    nvsd_exiting = true;

    /*
     * Now make ourselves fall out of the main loop by removing all
     * active events.
     */

    /*
     * Need to do this here to remove our mgmt-related events.
     */
    err = mdc_wrapper_disconnect(nvsd_mcc, false);
    bail_error(err);

    /*
     * Calling lew_delete_all_events() is not guaranteed
     * to solve this for us, since the GCL/libevent shim does not use
     * the libevent wrapper.  lew_escape_from_dispatch() will
     * definitely do it, though.
     */
    err = lew_escape_from_dispatch(nvsd_lwc, true);
    bail_error(err);

bail:
    return (err);
}	/* end of nvsd_mgmt_initiate_exit() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_update_node_uint32()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_mgmt_update_node_uint32(const char *cpNode, uint32 new_value)
{
    int err = 0;
    uint32 code = 0;
    char *value_str;

    value_str = smprintf("%d", new_value);
    err = mdc_modify_binding
	(nvsd_mcc, &code, NULL, bt_uint32, value_str, cpNode);

    /*
     * If we sent the message successfully but an error code was
     * returned from mgmtd, treat it as an error here.
     */
    err = code;
    goto bail;

bail:
    safe_free(value_str);
    return (err);
}	/* end of nvsd_mgmt_update_node_uint32() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_update_node_bool()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_mgmt_update_node_bool(const char *cpNode, tbool new_value)
{
    int err = 0;
    uint32 code = 0;
    char *value_str;

    value_str = smprintf("%s", new_value ? "true" : "false");
    err = mdc_modify_binding(nvsd_mcc, &code, NULL, bt_bool, value_str, cpNode);

    /*
     * If we sent the message successfully but an error code was
     * returned from mgmtd, treat it as an error here.
     */
    err = code;
    goto bail;
bail:
    safe_free(value_str);
    return (err);
}	/* end of nvsd_mgmt_update_node_bool() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_update_node_str()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_mgmt_update_node_str(const char *cpNode,
	const char *new_value, bn_type binding_type)
{
    int err = 0;
    uint32 code = 0;

    err = mdc_modify_binding
	(nvsd_mcc, &code, NULL, binding_type, new_value, cpNode);

    /*
     * If we sent the message successfully but an error code was
     * returned from mgmtd, treat it as an error here.
     */
    err = code;
    goto bail;
bail:
    return (err);

}	/* end of nvsd_mgmt_update_node_str() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_handle_is_obj_cached ()
 *	purpose : Determine if object is cached or not
 */
static int
nvsd_mgmt_handle_is_obj_cached(gcl_session * sess,
	const bn_binding_array * bindings,
	const bn_request * action_request)
{
    int err = 0;
    bn_response *resp = NULL;
    char *show_data = NULL;
    tstring *t_namespace = NULL;
    tstring *t_uid = NULL;
    tstring *t_uri = NULL;

    /*
     * get the namespace field 
     */
    err = bn_binding_array_get_value_tstr_by_name
	(bindings, "namespace", NULL, &t_namespace);
    bail_error(err);

    /*
     * get the uid field 
     */
    err = bn_binding_array_get_value_tstr_by_name
	(bindings, "uid", NULL, &t_uid);
    bail_error(err);

    /*
     * get the uri field 
     */
    err = bn_binding_array_get_value_tstr_by_name
	(bindings, "uri", NULL, &t_uri);
    bail_error(err);

    if (!t_namespace || !t_uid || !t_uri) {
	/*
	 * send negative response 
	 */

	goto bail;
    }

    err =
	nsvd_mgmt_ns_is_obj_cached(ts_str(t_namespace), ts_str(t_uid),
		ts_str(t_uri), &show_data);
    bail_error(err);

    err = bn_action_response_msg_create
	(&resp, bn_request_get_msg_id(action_request), 0, NULL);
    bail_error(err);

    err = bn_response_msg_set_return_code_msg(resp, 0, show_data);
    bail_error(err);

    err = bn_response_msg_send(sess, resp);
    bail_error(err);
bail:
    ts_free(&t_namespace);
    ts_free(&t_uid);
    ts_free(&t_uri);
    bn_response_msg_free(&resp);
    safe_free(show_data);
    return err;
}

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_handle_show_rank_action ()
 *	purpose : picked up as is from samara's demo
 */
static int
nvsd_mgmt_handle_show_rank_action(gcl_session * sess,
	const bn_binding_array * bindings,
	const bn_request * action_request)
{
    int err = 0;
    char t_filename[25];
    bn_response *resp = NULL;
    char *show_data = NULL;
    int t_rank;
    int fd;
    struct stat st;
    int file_size;
    FILE *fpTemp = NULL;

    (void) t_rank;
    UNUSED_ARGUMENT(bindings);

    strcpy(t_filename, "/tmp/.am_rank_XXXXXX");
    fd = mkstemp(t_filename);
    if (fd == -1) {
	err = bn_response_msg_create_send
	    (sess, bbmt_action_response, bn_request_get_msg_id(action_request),
	     1, _("Failed to get the statistics"), 0);
	bail_error(err);
	goto bail;
    }
    close(fd);
    fpTemp = fopen(t_filename, "w");
    if (NULL == fpTemp) {
	err = bn_response_msg_create_send
	    (sess, bbmt_action_response, bn_request_get_msg_id(action_request),
	     1, _("Failed to get the statistics"), 0);
	bail_error(err);
	goto bail;
    }

    fprintf(fpTemp, "Analytics Details By Rank\n");
    fprintf(fpTemp, "-------------------------\n");

    AM_print_entries(fpTemp);
    fprintf(fpTemp, "\n------- End of List ------------------\n");
    fflush(fpTemp);

    fclose(fpTemp);

    /*
     * Now read the data into buffer 
     */
    stat(t_filename, &st);
    file_size = st.st_size;

    /* + 1 for the end '\0' */
    show_data = nkn_calloc_type(st.st_size + 1, sizeof (char), mod_mgmt_charbuf);
    if (NULL == show_data) {
	err = 1;
	bail_error(err);
    }

    fd = open(t_filename, O_RDONLY);
    if (fd == -1) {
	err = 1;
	bail_error(err);
    }

    read(fd, show_data, file_size);
    close(fd);

    unlink(t_filename);

    err = bn_action_response_msg_create
	(&resp, bn_request_get_msg_id(action_request), 0, NULL);
    complain_error(err);

    err = bn_response_msg_set_return_code_msg(resp, 0, show_data);
    bail_error(err);

    err = bn_response_msg_send(sess, resp);
    complain_error(err);

bail:
    free(show_data);
    bn_response_msg_free(&resp);
    return (err);
}	/* end of nvsd_mgmt_handle_show_rank_action () */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_handle_show_tier_action ()
 *	purpose : picked up as is from samara's demo
 */
static int
nvsd_mgmt_handle_show_tier_action(gcl_session * sess,
	const bn_binding_array * bindings,
	const bn_request * action_request)
{
    int err = 0;
    char t_filename[25];
    bn_response *resp = NULL;
    char *show_data = NULL;
    int fd;
    struct stat st;
    int file_size;
    FILE *fpTemp = NULL;

    UNUSED_ARGUMENT(bindings);

    /* 
       err = bn_binding_array_get_value_tstr_by_name
       (bindings, "output", NULL, &t_filename);
       bail_error(err);
     */
    strcpy(t_filename, "/tmp/.am_tier_XXXXXX");
    fd = mkstemp(t_filename);
    if (fd == -1) {
	err = bn_response_msg_create_send
	    (sess, bbmt_action_response, bn_request_get_msg_id(action_request),
	     1, _("Failed to get the statistics"), 0);
	bail_error(err);
	goto bail;
    }
    close(fd);
    fpTemp = fopen(t_filename, "w");
    if (NULL == fpTemp) {
	err = bn_response_msg_create_send
	    (sess, bbmt_action_response, bn_request_get_msg_id(action_request),
	     1, _("Failed to get the statistics"), 0);
	bail_error(err);
	goto bail;
    }

    fprintf(fpTemp, "Analytics Details By Tier\n");
    fprintf(fpTemp, "-------------------------\n");

    /*
     * Call the AM utility to print the list 
     */
    AM_print_entries(fpTemp);

    fprintf(fpTemp, "------- End of List ------------------\n");
    fflush(fpTemp);

    fclose(fpTemp);

    /*
     * Now read the data into buffer 
     */
    stat(t_filename, &st);
    file_size = st.st_size;

    /* The +1 is for the terminating '\0' */
    show_data = nkn_calloc_type(st.st_size + 1, sizeof (char), mod_mgmt_charbuf);
    if (NULL == show_data) {
	err = 1;
	bail_error(err);
    }

    fd = open(t_filename, O_RDONLY);
    if (fd == -1) {
	err = 1;
	bail_error(err);
    }

    read(fd, show_data, file_size);
    close(fd);

    unlink(t_filename);
    err = bn_action_response_msg_create
	(&resp, bn_request_get_msg_id(action_request), 0, NULL);
    complain_error(err);

    err = bn_response_msg_set_return_code_msg(resp, 0, show_data);
    bail_error(err);

    err = bn_response_msg_send(sess, resp);
    complain_error(err);

bail:
    safe_free(show_data);
    bn_response_msg_free(&resp);
    return (err);
}	/* end of nvsd_mgmt_handle_show_tier_action * () */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_handle_server_map_refresh_action ()
 *	purpose : Force update a server-map
 */
static int
nvsd_mgmt_handle_server_map_refresh_action(gcl_session * sess,
	const bn_binding_array * bindings,
	const bn_request * action_request)
{
    int err = 0;
    bn_response *resp = NULL;
    const bn_binding *binding = NULL;
    char *smap_name = NULL;
    char *smap_bin_file_name = NULL;

    err = bn_binding_array_get_binding_by_name(bindings, "name", &binding);
    bail_error_null(err, binding);

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &smap_name);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name(bindings, "file-name", &binding);
    bail_error_null(err, binding);

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
	    &smap_bin_file_name);
    bail_error(err);

    err = bn_response_msg_create_send(sess, bbmt_action_response,
	    bn_request_get_msg_id(action_request),
	    0, _("Reading server map"), 0);
    bail_error(err);

    /*
     * call callout fn to read and set the binary xml for the server map 
     */

    err = read_binary_servermap_to_buf(smap_name, smap_bin_file_name);
    bail_error(err);

    if (err)
	lc_log_basic(LOG_NOTICE, "Failed to read server map");

bail:
    bn_response_msg_free(&resp);
    safe_free(smap_name);
    safe_free(smap_bin_file_name);
    return err;
}	/* nvsd_mgmt_handle_server_map_refresh_action () */

int
nvsd_mgmt_reset_counter_backup_data(void)
{
    int err = 0;

    lc_log_basic(LOG_DEBUG, _("Inside the action code for reset counter\n"));
    glob_http_bkup_tot_200_resp = glob_http_tot_200_resp;
    glob_http_bkup_tot_206_resp = glob_http_tot_206_resp;
    glob_http_bkup_tot_302_resp = glob_http_tot_302_resp;
    glob_http_bkup_tot_304_resp = glob_http_tot_304_resp;
    glob_http_bkup_tot_400_resp = glob_http_tot_400_resp;
    glob_http_bkup_tot_404_resp = glob_http_tot_404_resp;
    glob_http_bkup_tot_416_resp = glob_http_tot_416_resp;
    glob_http_bkup_tot_500_resp = glob_http_tot_500_resp;
    glob_http_bkup_tot_501_resp = glob_http_tot_501_resp;
    glob_http_bkup_tot_503_resp = glob_http_tot_503_resp;
    glob_http_bkup_tot_504_resp = glob_http_tot_504_resp;
    glob_http_bkup_tot_well_finished = glob_http_tot_well_finished;
    glob_http_bkup_tot_transactions = glob_http_tot_transactions;
    glob_tot_bkup_bytes_sent = glob_tot_bytes_sent;
    glob_tot_bkup_size_from_disk = glob_tot_size_from_disk;
    glob_tot_bkup_size_from_cache = glob_tot_size_from_cache;
    glob_tot_bkup_size_from_origin = glob_tot_size_from_origin;
    glob_tot_bkup_size_from_nfs = glob_tot_size_from_nfs;
    glob_tot_bkup_size_from_tunnel = glob_tot_size_from_tunnel;
    glob_cache_bkup_request_error = glob_cache_request_error;
    glob_err_bkup_timeout_with_task = glob_err_timeout_with_task;
    glob_socket_bkup_tot_timeout = glob_socket_tot_timeout;
    glob_http_bkup_schd_err_get_data = glob_http_schd_err_get_data;
    glob_http_bkup_cache_miss_cnt = glob_http_cache_miss_cnt;
    glob_sched_bkup_task_deadline_missed = glob_sched_task_deadline_missed;
    glob_om_bkup_err_get = glob_om_err_get;
    glob_http_bkup_err_parse = glob_http_err_parse;
    glob_om_bkup_err_conn_failed = glob_om_err_conn_failed;
    glob_tot_bkup_get_in_this_second = glob_tot_get_in_this_second;
    glob_om_bkup_tot_completed_http_trans = glob_om_tot_completed_http_trans;
    glob_dm2_bkup_raw_read_cnt = glob_dm2_raw_read_cnt;
    glob_dm2_bkup_raw_write_cnt = glob_dm2_raw_write_cnt;
    glob_dm2_bkup_ext_read_bytes = glob_dm2_ext_read_bytes;
    glob_ssp_bkup_seeks = glob_ssp_seeks;
    glob_ssp_bkup_hash_failures = glob_ssp_hash_failures;
    glob_sched_bkup_runnable_q_pushes = glob_sched_runnable_q_pushes;
    glob_sched_bkup_runnable_q_pops = glob_sched_runnable_q_pops;
    glob_fp_bkup_err_make_tunnel = glob_fp_err_make_tunnel;
    glob_tot_bkup_http_sockets = glob_tot_http_sockets;
    glob_tot_bkup_http_sockets_ipv6 = glob_tot_http_sockets_ipv6;
    glob_rtsp_bkup_tot_bytes_sent = glob_rtsp_tot_bytes_sent;
    glob_rtp_bkup_tot_udp_packets_fwd = glob_rtp_tot_udp_packets_fwd;
    glob_rtcp_bkup_tot_udp_packets_fwd = glob_rtcp_tot_udp_packets_fwd;
    glob_rtcp_bkup_tot_udp_packets_fwd = glob_rtcp_tot_udp_packets_fwd;
    glob_rtcp_bkup_tot_udp_sr_packets_sent = glob_rtcp_tot_udp_sr_packets_sent;
    glob_rtcp_bkup_tot_udp_rr_packets_sent = glob_rtcp_tot_udp_rr_packets_sent;
    glob_rtsp_bkup_tot_status_resp_200 = glob_rtsp_tot_status_resp_200;
    glob_rtsp_bkup_tot_status_resp_400 = glob_rtsp_tot_status_resp_400;
    glob_rtsp_bkup_tot_status_resp_404 = glob_rtsp_tot_status_resp_404;
    glob_rtsp_bkup_tot_status_resp_500 = glob_rtsp_tot_status_resp_500;
    glob_rtsp_bkup_tot_status_resp_501 = glob_rtsp_tot_status_resp_501;
    glob_rtcp_bkup_tot_udp_packets_recv = glob_rtcp_tot_udp_packets_recv;
    glob_tot_bkup_rtcp_sr = glob_tot_rtcp_sr;
    glob_tot_bkup_rtcp_rr = glob_tot_rtcp_rr;
    glob_rtsp_bkup_tot_rtsp_parse_error = glob_rtsp_tot_rtsp_parse_error;
    glob_rtsp_bkup_tot_vod_bytes_sent = glob_rtsp_tot_vod_bytes_sent;
    glob_fp_bkup_tot_tunnel = glob_fp_tot_tunnel;
    glob_cll7redir_bkup_intercept_success = glob_cll7redir_intercept_success;
    glob_cll7redir_bkup_int_err_1 = glob_cll7redir_int_err_1;
    glob_cll7redir_bkup_int_err_2 = glob_cll7redir_int_err_2;
    glob_cll7redir_bkup_int_errs = glob_cll7redir_intercept_errs;
    glob_http_bkup_cache_hit_cnt = glob_http_cache_hit_cnt;

    err = nvsd_ns_reset_counter_values();
    err = nvsd_set_time_for_reset_counter();
    bail_error(err);

bail:
    return err;
}

int
nvsd_set_time_for_reset_counter()
{
    int err = 0;

    glob_nkn_reset_counter_time = lt_curr_time();

    lc_log_basic(LOG_DEBUG, _("Time of reset:%d\n"),
	    (int) glob_nkn_reset_counter_time);

    bail_error(err);
bail:
    return err;
}

/*
 *	funtion : nvsd_mgmt_handle_buffermgr_object_list_action ()
 *	purpose : picked up as is from samara's demo
 */
static int
nvsd_mgmt_handle_buffermgr_object_list_action(gcl_session * sess,
	const bn_binding_array * bindings,
	const bn_request * action_request)
{
    int err = 0;
    char t_filename[64];
    bn_response *resp = NULL;
    char *show_data = NULL;

    FILE *fpTemp = NULL;

    UNUSED_ARGUMENT(bindings);

    strcpy(t_filename, "/var/log/nkn/buffermgr_object_list");
    fpTemp = fopen(t_filename, "w");
    if (NULL == fpTemp) {
	err = bn_response_msg_create_send
	    (sess, bbmt_action_response, bn_request_get_msg_id(action_request),
	     1, _("Failed to open ftemp"), 0);
	bail_error(err);
	goto bail;
    }

    fprintf(fpTemp, "Buffer manager Object Request List\n");
    fprintf(fpTemp, "-------------------------\n");

    /*
     * Function call to Buffer manager to get the request list 
     */

    nkn_bm_req_entries(fpTemp);
    fprintf(fpTemp, "\n------- End of List ------------------\n");
    fflush(fpTemp);

    fclose(fpTemp);
    err = bn_action_response_msg_create
	(&resp, bn_request_get_msg_id(action_request), 0, NULL);
    complain_error(err);

    err = bn_response_msg_set_return_code_msg(resp, 0, "");
    bail_error(err);

    err = bn_response_msg_send(sess, resp);
    complain_error(err);

bail:
    free(show_data);
    bn_response_msg_free(&resp);
    return (err);
}	/* end of * nvsd_mgmt_handle_buffermgr_object_list_action () */

int
ssld_config_query(void)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;

    err = mdc_get_binding_children(nvsd_mcc,
	    NULL, NULL, true, &bindings, false, true,
	    "/nkn/ssld/delivery/config");
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings, ssld_config_handle_change,
	    &rechecked_licenses);
    bail_error(err);

bail:
    bn_binding_array_free(&bindings);
    return (err);
}

int
ssld_config_handle_change(const bn_binding_array * arr,
	uint32 idx, bn_binding * binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tbool *rechecked_licenses_p = data;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/ssld/delivery/config/server_port/*")) {
	uint16 t_port;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_port);
	bail_error(err);
	ssl_listen_port = (uint32_t) t_port;
    }
bail:
    return err;
}

int
get_other_interface_details(if_info_state_change_t * interface_change_info)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    node_name_t pattern = { 0 };
    node_name_t ipv4_addr_node_name = {0},
		ipv6_addr_node_name = {0},
		ipv6_enable_node_name = { 0},
		dev_src_node_name = { 0},
		link_up_node_name = { 0},
		oper_up_node_name = { 0},
		ipv6_mask_len_node_name = { 0},
		ipv4_mask_len_node_name = {0};

    tbool link_up = false, oper_up = false, ipv6_enable = false;
    tstring *ipv4addr = NULL, *ipv6addr = NULL, *devsource =
	NULL, *ipv6mask_len = NULL, *ipv4mask_len = NULL;

    snprintf(pattern, sizeof (pattern), "/net/interface/state/%s",
	    interface_change_info->intfname);
    err =
	mdc_get_binding_children(nvsd_mcc, NULL, NULL, true, &bindings, true,
		true, pattern);
    bail_error(err);

    if (!bindings)
	goto bail;

    snprintf(ipv6_enable_node_name, sizeof (ipv6_enable_node_name),
	    "/net/interface/state/%s/addr/settings/ipv6/enable",
	    interface_change_info->intfname);
    err =
	bn_binding_array_get_value_tbool_by_name(bindings,
		ipv6_enable_node_name, NULL,
		&ipv6_enable);
    bail_error(err);
    if (ipv6_enable)
	interface_change_info->ipv6_enable = 1;
    else
	interface_change_info->ipv6_enable = 0;

    snprintf(dev_src_node_name, sizeof (dev_src_node_name),
	    "/net/interface/state/%s/devsource",
	    interface_change_info->intfname);
    err =
	bn_binding_array_get_value_tstr_by_name(bindings, dev_src_node_name,
		NULL, &devsource);
    bail_error(err);

    if (devsource)
	strncpy(interface_change_info->intfsource, ts_str(devsource),
		ts_length(devsource));

    snprintf(ipv6_addr_node_name, sizeof (ipv6_addr_node_name),
	    "/net/interface/state/%s/addr/ipv6/1/ip",
	    interface_change_info->intfname);
    err =
	bn_binding_array_get_value_tstr_by_name(bindings, ipv6_addr_node_name,
		NULL, &ipv6addr);
    bail_error(err);
    if (ipv6addr)
	strncpy(interface_change_info->ipv6addr, ts_str(ipv6addr),
		ts_length(ipv6addr));

    snprintf(ipv6_mask_len_node_name, sizeof (ipv6_mask_len_node_name),
	    "/net/interface/state/%s/addr/ipv6/1/mask_len",
	    interface_change_info->intfname);
    err =
	bn_binding_array_get_value_tstr_by_name(bindings,
		ipv6_mask_len_node_name, NULL,
		&ipv6mask_len);
    bail_error(err);
    if (ipv6mask_len)
	interface_change_info->ipv6_mask_len = atoi(ts_str(ipv6mask_len));

    snprintf(oper_up_node_name, sizeof (oper_up_node_name),
	    "/net/interface/state/%s/flags/oper_up",
	    interface_change_info->intfname);
    err =
	bn_binding_array_get_value_tbool_by_name(bindings, oper_up_node_name,
		NULL, &oper_up);
    bail_error(err);
    if (oper_up)
	interface_change_info->oper_up = 1;
    else
	interface_change_info->oper_up = 0;

    snprintf(ipv4_addr_node_name, sizeof (ipv4_addr_node_name),
	    "/net/interface/state/%s/addr/ipv4/1/ip",
	    interface_change_info->intfname);
    err =
	bn_binding_array_get_value_tstr_by_name(bindings, ipv4_addr_node_name,
		NULL, &ipv4addr);
    bail_error(err);
    if (ipv4addr)
	strncpy(interface_change_info->ipv4addr, ts_str(ipv4addr),
		ts_length(ipv4addr));

    snprintf(ipv4_mask_len_node_name, sizeof (ipv4_mask_len_node_name),
	    "/net/interface/state/%s/addr/ipv4/1/mask_len",
	    interface_change_info->intfname);
    err =
	bn_binding_array_get_value_tstr_by_name(bindings,
		ipv4_mask_len_node_name, NULL,
		&ipv4mask_len);
    bail_error(err);
    if (ipv4mask_len)
	interface_change_info->ipv4_mask_len = atoi(ts_str(ipv4mask_len));

    snprintf(link_up_node_name, sizeof (link_up_node_name),
	    "/net/interface/state/%s/flags/link_up",
	    interface_change_info->intfname);
    err =
	bn_binding_array_get_value_tbool_by_name(bindings, link_up_node_name,
		NULL, &link_up);
    bail_error(err);
    if (link_up)
	interface_change_info->link_up = 1;
    else
	interface_change_info->link_up = 0;

bail:
    ts_free(&ipv6addr);
    ts_free(&ipv6mask_len);
    ts_free(&ipv4addr);
    ts_free(&ipv4mask_len);
    ts_free(&devsource);
    bn_binding_array_free(&bindings);
    return err;
}

int
nvsd_mgmt_req_complete(md_client_context * mcc,
	bn_request * request,
	tbool request_complete,
	bn_response * response, int32 idx, void *arg)
{
    int err = 0;

    UNUSED_ARGUMENT(mcc);
    UNUSED_ARGUMENT(request);
    UNUSED_ARGUMENT(request_complete);
    UNUSED_ARGUMENT(response);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(arg);

    err = mdc_bl_set_ready(nvsd_blc, true);
    bail_error(err);

bail:
    return err;
}

int
nvsd_mgmt_req_start(md_client_context * mcc,
	bn_request * request,
	tbool request_complete,
	bn_response * response, int32 idx, void *arg)
{
    int err = 0;

    UNUSED_ARGUMENT(mcc);
    UNUSED_ARGUMENT(request);
    UNUSED_ARGUMENT(request_complete);
    UNUSED_ARGUMENT(response);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(arg);

    err = mdc_bl_set_ready(nvsd_blc, false);
    bail_error(err);

bail:
    return err;
}

int
mgmt_query_data_sanity(mgmt_query_data_t * mgmt_query, int *ret_code)
{
    int err = 0;
    unsigned int i = 0, j = 0;
    const char *node = NULL;
    int sanity_failed = 0;

    bail_null(mgmt_query);

    if (ret_code) {
	*ret_code = sanity_failed;
    }

    for (i = 0; i < MAX_MGMT_QUERY; i++) {
	node = mgmt_query[i].query_nd;
	for (j = i + 1; j < MAX_MGMT_QUERY; j++) {
	    if (lc_is_prefix(node, mgmt_query[j].query_nd, false)) {
		lc_log_basic(LOG_NOTICE, "sanity failed: %s is overlapping"
			" with %s", node, mgmt_query[j].query_nd);
		sanity_failed = 1;
	    } else if (lc_is_prefix(mgmt_query[j].query_nd, node, false)) {
		lc_log_basic(LOG_NOTICE, "sanity failed: %s is overlapping"
			" with %s", mgmt_query[j].query_nd, node);
		sanity_failed = 1;
	    } else {
		/*
		 * good to go 
		 */
	    }
	}
    }

bail:
    if (ret_code) {
	/*
	 * non zero ret-code is sanity failed 
	 */
	*ret_code = sanity_failed;
    }

    return err;
}

static int
nvsd_mgmt_get_nodes(mgmt_query_data_t * mgmt_query,
	bn_binding_array ** bindings)
{
    int err = 0;
    bn_request *req = NULL;
    uint32_t i = 0;
    bn_query_subop node_subop = bqso_iterate;
    int node_flags = bqnf_sel_iterate_subtree;
    uint32_t ret_code = 0;
    tstring *ret_msg = NULL;

    err = mgmt_query_data_sanity(mgmt_query, NULL);
    bail_error(err);

    err = bn_request_msg_create(&req, bbmt_query_request);
    bail_error(err);

    for (i = 0; i < MAX_MGMT_QUERY; i++) {
	err = bn_query_request_msg_append_str(req,
		node_subop,
		node_flags,
		mgmt_query[i].query_nd, NULL);
	bail_error(err);
    }

    err = mdc_send_mgmt_msg(nvsd_mcc, req, true, bindings,
	    &ret_code, &ret_msg);
    bail_error(err);

bail:
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}

int
mgmt_lib_mdc_foreach_binding_prequeried(const bn_binding_array * bindings,
	const char *binding_spec,
	const char *db_name,
	mdc_foreach_binding_func callback,
	void *callback_data)
{
    int err = 0;
    node_name_t binding_spec_pattern = { 0 };
    uint32 ret_num_matches = 0;

    snprintf(binding_spec_pattern, sizeof (binding_spec_pattern), "%s/**",
	    binding_spec);
    err =
	mdc_foreach_binding_prequeried(bindings, binding_spec_pattern, db_name,
		callback, callback_data,
		&ret_num_matches);
    bail_error(err);

bail:
    return err;
}

/* End of nvsd_mgmt_main.c */
/* ------------------------------------------------------------------------- */
