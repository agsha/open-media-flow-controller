/*
 *
 * Filename:    md_nkn_stats.c
 * Date:        2008-11-12
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 * md_nkn_stats.c: shows how the nknaccesslog module is added to
 * the initial PM database.
 *
 *---------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "dso.h"
#include "file_utils.h"
#include "md_utils.h"
#include "md_mod_reg.h"
#include "md_mod_commit.h"
#include "mdb_db.h"
#include "mdb_dbbe.h"
#include "array.h"
#include "tpaths.h"
#include "proc_utils.h"
#include "nkn_cntr_api.h"
#include "mdmod_common.h"
#include "md_nkn_stats.h"
#include "md_graph_utils.h"
#include "md_utils_internal.h"
#include "string.h"
#include "nkn_mgmt_defs.h"
/*----------------------------------------------------------------------------
 * PREPROCESSOR MACROS/CONSTANTS
 *---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/

typedef int (*get_cb)(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

typedef struct md_ns_counter {
    const char *ns_counter;
    const char *get_arg;
    /* added type as of, taking default as uint64 */
    bn_type type;
} md_ns_counter_t ;

typedef struct {
        const char    *node;
        bn_type type;
        const char    *variable;
        get_cb  cb;
} md_nkn_stats_t;

typedef struct {
	const char *variable1;
	const char *variable2;
}md_nkn_arg_list_t;

typedef enum {
    SERVICE_INTF_ALL,
    SERVICE_INTF_HTTP,
    SERVICE_INTF_RTSP
} service_type_t;

typedef struct {
        const char    *node;
        bn_type type;
	md_nkn_arg_list_t args;
        get_cb  cb;
} md_nkn_bkup_stats_t;
typedef struct {
    const char *filename;
    uint32 version;
    uint32 mode;
    const char *action_name;
    mdm_action_std_func function_name;
}md_nkn_stat_graphs_t;

enum {
    HOURLY,
    DAILY,
    WEEKLY,
    MONTHLY,
};

const bn_str_value md_email_initial_values[] = {
    {"/email/notify/events/"
     "\\/stats\\/events\\/nkn_cpu_util_ave\\/rising\\/error",
     bt_name, "/stats/events/nkn_cpu_util_ave/rising/error"},
    {"/email/notify/events/"
     "\\/stats\\/events\\/nkn_cpu_util_ave\\/rising\\/error/enable",
     bt_bool, "true"},

    {"/email/notify/events/"
     "\\/stats\\/events\\/nkn_cpu_util_ave\\/rising\\/clear",
     bt_name, "/stats/events/nkn_cpu_util_ave/rising/clear"},
    {"/email/notify/events/"
     "\\/stats\\/events\\/nkn_cpu_util_ave\\/rising\\/clear/enable",
     bt_bool, "true"},
};


md_commit_forward_action_args md_stats_fwd_args =
{
    GCL_CLIENT_NVSD, true, N_("Request failed; MFD subsystem did not respond"),
    mfat_nonbarrier_async
#ifdef PROD_FEATURE_I18N_SUPPORT
    , GETTEXT_DOMAIN
#endif
};
md_commit_forward_action_args md_ssld_fwd_args =
{
    GCL_CLIENT_SSLD, true, N_("Request failed; SSLD subsystem did not respond"),
    mfat_nonbarrier_async
#ifdef PROD_FEATURE_I18N_SUPPORT
    , GETTEXT_DOMAIN
#endif
};

static int md_nkn_stats_graph_gen_cron(void);

#define md_total_bandwidth_day \
    "/stats/state/chd/tot_bandwidth_day/node/"  \
    "\\/stat\\/nkn\\/nvsd\\/tot_bandwidth_day"

#define md_cache_bandwidth_day \
    "/stats/state/chd/cache_bandwidth_day/node/"  \
    "\\/stat\\/nkn\\/nvsd\\/cache_bandwidth_day"

#define md_disk_bandwidth_day \
    "/stats/state/chd/disk_bandwidth_day/node/"  \
    "\\/stat\\/nkn\\/nvsd\\/disk_bandwidth_day"

#define md_origin_bandwidth_day \
    "/stats/state/chd/origin_bandwidth_day/node/"  \
    "\\/stat\\/nkn\\/nvsd\\/origin_bandwidth_day"

#define md_total_bandwidth_week \
    "/stats/state/chd/tot_bandwidth_week/node/"  \
    "\\/stat\\/nkn\\/nvsd\\/tot_bandwidth_week"

#define md_cache_bandwidth_week \
    "/stats/state/chd/cache_bandwidth_week/node/"  \
    "\\/stat\\/nkn\\/nvsd\\/cache_bandwidth_week"

#define md_disk_bandwidth_week \
    "/stats/state/chd/disk_bandwidth_week/node/"  \
    "\\/stat\\/nkn\\/nvsd\\/disk_bandwidth_week"

#define md_origin_bandwidth_week \
    "/stats/state/chd/origin_bandwidth_week/node/"  \
    "\\/stat\\/nkn\\/nvsd\\/origin_bandwidth_week"

#if 1
#define md_intf_day_rx_chd_base                                             \
    "/stats/state/chd/intf_day/node/"                                      \
    "\\/net\\/interface\\/state\\/%s\\/stats\\/rx\\/bytes"

#define md_intf_day_tx_chd_base                                             \
    "/stats/state/chd/intf_day/node/"                                    \
    "\\/net\\/interface\\/state\\/%s\\/stats\\/tx\\/bytes"

static const char md_intf_day_rx_chd[] = md_intf_day_rx_chd_base "/instance";
static const char md_intf_day_tx_chd[] = md_intf_day_tx_chd_base "/instance";

#define md_intf_month_rx_chd_base                                          \
    "/stats/state/chd/intf_month/node/"                                      \
    "\\/net\\/interface\\/state\\/%s\\/stats\\/rx\\/bytes"

#define md_intf_month_tx_chd_base                                             \
    "/stats/state/chd/intf_month/node/"                                    \
    "\\/net\\/interface\\/state\\/%s\\/stats\\/tx\\/bytes"

static const char md_intf_month_rx_chd[] = md_intf_month_rx_chd_base "/instance";
static const char md_intf_month_tx_chd[] = md_intf_month_tx_chd_base "/instance";

#define md_intf_week_rx_chd_base                                          \
    "/stats/state/sample/intf_week/node/"                                      \
    "\\/net\\/interface\\/state\\/%s\\/stats\\/rx\\/bytes"

#define md_intf_week_tx_chd_base                                             \
    "/stats/state/sample/intf_week/node/"                                    \
    "\\/net\\/interface\\/state\\/%s\\/stats\\/tx\\/bytes"

static const char md_intf_week_rx_chd[] = md_intf_week_rx_chd_base "/instance";
static const char md_intf_week_tx_chd[] = md_intf_week_tx_chd_base "/instance";

#endif
const char *colors_choices[] = {"FF0000", "FFCC00", "FFFF00", "00C000",
                                "00C0FF", "0000FF", "98FB98", "6B8E63"};

lg_color colors_choices_lg[] = {0xff0000, 0xffcc00, 0xffff00, 0x00c000,
                                0x00c0ff, 0x0000ff, 0x98fb98, 0x6b8e63};

const char *green_colors_choices[] = {"006400", "228B22", "3CB371", "00FF00",
                                      "98FB98", "6B8E63"};

lg_color green_colors_lg[] = {0x006400, 0x228b22, 0x3cb371, 0x00ff00,
                              0x98fb98, 0x6b8e63, lg_color_choose};

const char *blue_colors_choices[] = {"00008B", "0000FF", "1E90FF", "48D1CC",
                                     "AFEEEE", "4682B4"};

lg_color blue_colors_lg[] = {0x00008b, 0x0000ff, 0x1e90ff, 0x48d1cc,
                            0xafeeee, 0x4682b4, lg_color_choose};

lg_color single_red         = 0xFF1111;
lg_color single_red_dark    = 0xA40000;
lg_color single_green       = 0x00BB00;
lg_color single_green_dark  = 0x006600;
lg_color single_blue        = 0x0060FF;

md_stats_register_event_type md_stats_register_event_func = NULL;

static int
md_nkn_stats_get_uint64_in_mb(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

const md_nkn_stats_t md_stats_init[] = {
    {sp "/nvsd/total_byte_count", bt_uint64, "glob_client_send_tot_bytes", NULL},
    {sp "/nvsd/total_rtsp_byte_count", bt_uint64, "glob_rtsp_tot_bytes_sent", NULL},
    {sp "/nvsd/total_rtsp_vod_byte_count", bt_uint64, "glob_rtsp_tot_vod_bytes_sent", NULL},
    {sp "/nvsd/disk_byte_count", bt_uint64, "glob_tot_size_from_disk", NULL},
    {sp "/nvsd/cache_byte_count", bt_uint64, "glob_tot_size_from_cache", NULL},
    {sp "/nvsd/origin_byte_count", bt_uint64, "glob_tot_size_from_origin", NULL},
    {sp "/nvsd/origin_received_bytes", bt_uint64, "glob_origin_recv_tot_bytes", NULL},
    {sp "/nvsd/nfs_byte_count", bt_uint64, "glob_tot_size_from_nfs", NULL},
    {sp "/nvsd/num_connections", bt_uint32, "glob_cur_open_http_socket", NULL},
    {sp "/nvsd/num_conx_ipv6", bt_uint32, "glob_cur_open_http_socket_ipv6", NULL},
    {sp "/nvsd/num_http_req", bt_uint64, "glob_tot_http_sockets", NULL},
    {sp "/nvsd/num_http_transaction", bt_uint64, "glob_http_tot_transactions", NULL},
    {sp "/nvsd/tunnel_byte_count", bt_uint64, "glob_tot_size_from_tunnel", NULL},
    {sp "/http/response/200/count", bt_uint64, "glob_http_tot_200_resp", NULL},
    {sp "/http/response/206/count", bt_uint64, "glob_http_tot_206_resp", NULL},
    {sp "/http/response/400/count", bt_uint64, "glob_http_tot_400_resp", NULL},
    {sp "/http/response/404/count", bt_uint64, "glob_http_tot_404_resp", NULL},
    {sp "/http/response/416/count", bt_uint64, "glob_http_tot_416_resp", NULL},
    {sp "/http/response/500/count", bt_uint64, "glob_http_tot_500_resp", NULL},
    {sp "/http/response/501/count", bt_uint64, "glob_http_tot_501_resp", NULL},
    {sp "/http/response/302/count", bt_uint64, "glob_http_tot_302_resp", NULL},
    {sp "/http/response/504/count", bt_uint64, "glob_http_tot_504_resp", NULL},
    {sp "/http/response/304/count", bt_uint64, "glob_http_tot_304_resp", NULL},
    {sp "/http/response/503/count", bt_uint64, "glob_http_tot_503_resp", NULL},
    {sp "/http/response/timeouts", bt_uint64, "glob_socket_tot_timeout", NULL},
    {sp "/http/aborts/cache_req_err", bt_uint64, "glob_cache_request_error", NULL},
    {sp "/http/aborts/error_timeout_withtask", bt_uint64, "glob_err_timeout_with_task", NULL},
    {sp "/http/aborts/http_schd_err_get_data", bt_uint64, "glob_http_schd_err_get_data", NULL},
    {sp "/http/aborts/tot_well_finished", bt_uint64, "glob_http_tot_well_finished", NULL},
    {sp "/http/aborts/mm_stat_err", bt_uint64, "glob_http_cache_miss_cnt", NULL},
    {sp "/http/aborts/task_deadline_missed", bt_uint64, "glob_sched_task_deadline_missed", NULL},
    {sp "/proxy/om_err_get", bt_uint32, "glob_om_err_get", NULL},
    {sp "/proxy/om_err_conn_failed", bt_uint32, "glob_om_err_conn_failed", NULL},
    {sp "/proxy/http_err_parse", bt_uint32, "glob_http_err_parse", NULL},
    {sp "/proxy/origin_fetched_bytes", bt_uint64, "glob_tot_size_from_origin", NULL},
    {sp "/proxy/cur_proxy_rate", bt_uint64, "glob_tot_get_in_this_second", NULL},
    {sp "/ingest/fetch_count", bt_uint64, "glob_om_tot_completed_http_trans", NULL},
    {sp "/ingest/fetch_bytes", bt_uint64, "glob_tot_size_from_disk", NULL},
    {sp "/disk_global/read_count", bt_uint64, "glob_dm2_raw_read_cnt", NULL},
    {sp "/disk_global/write_count", bt_uint64, "glob_dm2_raw_write_cnt", NULL},
    {sp "/disk_global/read_bytes", bt_uint64, "glob_dm2_ext_read_bytes", NULL},
    {sp "/ssp/num_of_seeks", bt_uint64, "glob_ssp_seeks", NULL},
    {sp "/ssp/hash_veri_errs", bt_uint64, "glob_ssp_hash_failures", NULL},
    {sp "/num_of_ports", bt_uint16, "glob_tot_interface", NULL},
    {sp "/scheduler/runnable_q_pushes", bt_uint64, "glob_sched_runnable_q_pushes", NULL},
    {sp "/scheduler/runnable_q_pops", bt_uint64, "glob_sched_runnable_q_pops", NULL},
    {sp "/scheduler/cleanup_q_pushes", bt_uint32, "glob_sched_cleanup_q_pushes", NULL},
    {sp "/scheduler/cleanup_q_pops", bt_uint32, "glob_sched_cleanup_q_pops", NULL},
    {sp "/nvsd/num_rtsp_connections", bt_uint64, "glob_cur_open_rtsp_socket", NULL},
    {sp "/nvsd/num_rtsp_udp_pkt_fwd", bt_uint64, "glob_rtp_tot_udp_packets_fwd", NULL},
    {sp "/http/tunnel/total_connections", bt_uint64, "glob_fp_tot_tunnel", NULL},
    {sp "/http/tunnel/active_connections", bt_uint64, "glob_fp_cur_open_tunnel", NULL},
    {sp "/http/tunnel/total_http09_requests", bt_uint64, "glob_http_09_request", NULL},
    {sp "/http/tunnel/error_count", bt_uint64, "glob_fp_err_make_tunnel", NULL},
    {sp "/http/server/active/ipv4_conx", bt_uint64, "glob_cp_tot_cur_open_sockets", NULL},
    {sp "/http/server/active/ipv6_conx", bt_uint64, "glob_cp_tot_cur_open_sockets_ipv6", NULL},

    {sp "/nvsd/glob_rtcp_tot_udp_packets_fwd", bt_uint64, "glob_rtcp_tot_udp_packets_fwd", NULL},
    {sp "/nvsd/glob_rtcp_tot_udp_sr_packets_sent", bt_uint64, "glob_rtcp_tot_udp_sr_packets_sent", NULL},
    {sp "/nvsd/glob_rtcp_tot_udp_rr_packets_sent", bt_uint64, "glob_rtcp_tot_udp_rr_packets_sent", NULL},
    {sp "/nvsd/glob_rtcp_tot_udp_packets_recv", bt_uint64, "glob_rtcp_tot_udp_packets_recv", NULL},
    {sp "/nvsd/glob_tot_rtcp_sr", bt_uint64, "glob_tot_rtcp_sr", NULL},
    {sp "/nvsd/glob_tot_rtcp_rr", bt_uint64, "glob_tot_rtcp_rr", NULL},
    {sp "/nvsd/tot_num_cl_redirect", bt_uint64, "glob_cll7redir_intercept_success", NULL},
    {sp "/nvsd/cl_redirect_int_err1", bt_uint64, "glob_cll7redir_int_err_1", NULL},
    {sp "/nvsd/cl_redirect_int_err2", bt_uint64, "glob_cll7redir_int_err_2", NULL},
    {sp "/nvsd/cl_redirect_int_errs", bt_uint64, "glob_cll7redir_intercept_errs", NULL},

    {sp "/rtsp/parse_error_count", bt_uint64, "glob_rtsp_tot_rtsp_parse_error", NULL},
    {sp "/rtsp/response/200/count", bt_uint32, "glob_rtsp_tot_status_resp_200", NULL},
    {sp "/rtsp/response/400/count", bt_uint32, "glob_rtsp_tot_status_resp_400", NULL},
    {sp "/rtsp/response/404/count", bt_uint32, "glob_rtsp_tot_status_resp_404", NULL},
    {sp "/rtsp/response/500/count", bt_uint32, "glob_rtsp_tot_status_resp_500", NULL},
    {sp "/rtsp/response/501/count", bt_uint32, "glob_rtsp_tot_status_resp_501", NULL},

    {sp "/rtsp/origin_byte_count", bt_uint64, "glob_rtsp_tot_size_from_origin", NULL},
    {sp "/rtsp/cache_byte_count", bt_uint64, "glob_rtsp_tot_size_from_cache", NULL},
    {sp "/http/cache_hit_count", bt_uint64, "glob_http_cache_hit_cnt", NULL},
    {sp "/https/tot_transactions", bt_uint64, "glob_https_tot_transactions", NULL},
    /*Adding new monitoring nodes for sake of SNMP counter display*/
    {sp "/http/cl_recv_tot_bytes", bt_uint64, "glob_client_recv_tot_bytes", NULL},
    {sp "/http/cl_send_tot_bytes", bt_uint64, "glob_client_send_tot_bytes", NULL},
    {sp "/http/tot_well_finished", bt_uint64, "glob_http_tot_well_finished", NULL},
    {sp "/http/svr_send_tot_req", bt_uint64,"glob_origin_send_tot_request" , NULL},
    {sp "/http/svr_recv_tot_resp", bt_uint64,"glob_origin_recv_tot_response" , NULL},
    {sp "/http/svr_in_bytes", bt_uint64,"glob_origin_recv_tot_bytes", NULL},
    {sp "/http/svr_out_bytes", bt_uint64,"glob_origin_send_tot_bytes", NULL},
    {sp "/rtsp/tot_req", bt_uint64,"glob_rtsp_tot_req_parsed", NULL},
    {sp "/rtsp/tot_resp", bt_uint64,"glob_rtsp_tot_res_parsed", NULL},
    {sp "/rtsp/cl_cache_hit", bt_uint64,"XXX", NULL},
    {sp "/rtsp/cl_cache_miss", bt_uint64,"XXX", NULL},
    {sp "/rtsp/cl_cache_partial_hit", bt_uint64,"XXX", NULL},
    {sp "/rtsp/cl_in_bytes", bt_uint64,"XXX", NULL},
    {sp "/rtsp/cl_out_bytes", bt_uint64,"glob_rtsp_tot_bytes_sent", NULL},
    {sp "/rtsp/cl_tot_conn", bt_uint64,"XXX", NULL},
    {sp "/rtsp/cl_act_conn", bt_uint64,"glob_cur_open_rtsp_socket", NULL},
    {sp "/rtsp/cl_idle_conn", bt_uint64,"XXX", NULL},
    {sp "/rtsp/sr_req", bt_uint64,"XXX", NULL},
    {sp "/rtsp/sr_resp", bt_uint64,"XXX", NULL},
    {sp "/rtsp/sr_in_bytes", bt_uint64,"XXX", NULL},
    {sp "/rtsp/sr_out_bytes", bt_uint64,"glob_rtsp_tot_bytes_sent", NULL},
    {sp "/rtsp/sr_tot_conn", bt_uint64,"XXX", NULL},
    {sp "/rtsp/sr_act_conn", bt_uint64,"glob_cur_open_rtsp_socket", NULL},
    {sp "/rtsp/sr_idle_conn", bt_uint64,"XXX", NULL},
    {sp "/nvsd/tot_cl_send", bt_uint64, "glob_tot_client_send", NULL},
    {sp "/nvsd/tot_cl_send_bm_dm", bt_uint64, "glob_tot_client_send_from_bm_or_dm", NULL},
    {sp "/nvsd/total_dict_size",bt_uint64, "dictionary.memory.maximum_bytes", md_nkn_stats_get_uint64_in_mb},
    {sp "/nvsd/used_dict_size", bt_uint64, "dictionary.memory.used_bytes", md_nkn_stats_get_uint64_in_mb}
};

const md_nkn_bkup_stats_t md_bkup_stats_init[] = {

    {sp "/nvsd/current/total_byte_count", bt_uint64, {"glob_client_send_tot_bytes","glob_client_bkup_send_tot_bytes"}, NULL},
    {sp "/nvsd/current/disk_byte_count", bt_uint64, {"glob_tot_size_from_disk", "glob_tot_bkup_size_from_disk"}, NULL},
    {sp "/nvsd/current/cache_byte_count", bt_uint64, {"glob_tot_size_from_cache", "glob_tot_bkup_size_from_cache"}, NULL},
    {sp "/nvsd/current/origin_byte_count", bt_uint64, {"glob_tot_size_from_origin", "glob_tot_bkup_size_from_origin"}, NULL},
    {sp "/nvsd/current/nfs_byte_count", bt_uint64, {"glob_tot_size_from_nfs", "glob_tot_bkup_size_from_nfs"}, NULL},
    {sp "/nvsd/current/tunnel_byte_count", bt_uint64, {"glob_tot_size_from_tunnel", "glob_tot_bkup_size_from_tunnel"}, NULL},
    {sp "/nvsd/current/num_http_req", bt_uint64, {"glob_tot_http_sockets", "glob_tot_bkup_http_sockets"}, NULL},
    {sp "/nvsd/current/num_http_transaction", bt_uint64, {"glob_http_tot_transactions","glob_http_bkup_tot_transactions"}, NULL},
    {sp "/http/response/current/200/count", bt_uint64, {"glob_http_tot_200_resp", "glob_http_bkup_tot_200_resp"}, NULL},
    {sp "/http/response/current/206/count", bt_uint64, {"glob_http_tot_206_resp", "glob_http_bkup_tot_206_resp"}, NULL},
    {sp "/http/response/current/400/count", bt_uint64, {"glob_http_tot_400_resp", "glob_http_bkup_tot_400_resp"}, NULL},
    {sp "/http/response/current/404/count", bt_uint64, {"glob_http_tot_404_resp", "glob_http_bkup_tot_404_resp"}, NULL},
    {sp "/http/response/current/416/count", bt_uint64, {"glob_http_tot_416_resp", "glob_http_bkup_tot_416_resp"}, NULL},
    {sp "/http/response/current/500/count", bt_uint64, {"glob_http_tot_500_resp", "glob_http_bkup_tot_500_resp"}, NULL},
    {sp "/http/response/current/501/count", bt_uint64, {"glob_http_tot_501_resp", "glob_http_bkup_tot_501_resp"}, NULL},
    {sp "/http/response/current/302/count", bt_uint64, {"glob_http_tot_302_resp", "glob_http_bkup_tot_302_resp"}, NULL},
    {sp "/http/response/current/504/count", bt_uint64, {"glob_http_tot_504_resp", "glob_http_bkup_tot_504_resp"}, NULL},
    {sp "/http/response/current/304/count", bt_uint64, {"glob_http_tot_304_resp", "glob_http_bkup_tot_304_resp"}, NULL},
    {sp "/http/response/current/503/count", bt_uint64, {"glob_http_tot_503_resp", "glob_http_bkup_tot_503_resp"}, NULL},
    {sp "/http/response/current/timeouts", bt_uint64,  {"glob_socket_tot_timeout", "glob_socket_bkup_tot_timeout"}, NULL},
    {sp "/http/aborts/current/cache_req_err", bt_uint64,{"glob_cache_request_error", "glob_cache_bkup_request_error"}, NULL},
    {sp "/http/aborts/current/error_timeout_withtask", bt_uint64, {"glob_err_timeout_with_task", "glob_err_bkup_timeout_with_task"}, NULL},
    {sp "/http/aborts/current/http_schd_err_get_data", bt_uint64, {"glob_http_schd_err_get_data", "glob_http_bkup_schd_err_get_data"}, NULL},
    {sp "/http/aborts/current/tot_well_finished", bt_uint64, {"glob_http_tot_well_finished", "glob_http_bkup_tot_well_finished"}, NULL},
    {sp "/http/aborts/current/mm_stat_err", bt_uint64, {"glob_http_cache_miss_cnt", "glob_http_bkup_cache_miss_cnt"}, NULL},
    {sp "/http/aborts/current/task_deadline_missed", bt_uint64, {"glob_sched_task_deadline_missed", "glob_sched_bkup_task_deadline_missed"}, NULL},
    {sp "/proxy/current/om_err_get", bt_uint32, {"glob_om_err_get", "glob_om_bkup_err_get"}, NULL},
    {sp "/proxy/current/om_err_conn_failed", bt_uint32, {"glob_om_err_conn_failed", "glob_om_bkup_err_conn_failed"}, NULL},
    {sp "/proxy/current/http_err_parse", bt_uint32, {"glob_http_err_parse", "glob_http_bkup_err_parse"}, NULL},
    {sp "/proxy/current/origin_fetched_bytes", bt_uint64, {"glob_tot_size_from_origin", "glob_tot_bkup_size_from_origin"}, NULL},
    {sp "/ingest/current/fetch_count", bt_uint64, {"glob_om_tot_completed_http_trans", "glob_om_bkup_tot_completed_http_trans"}, NULL},
    {sp "/ingest/current/fetch_bytes", bt_uint64, {"glob_tot_size_from_disk", "glob_tot_bkup_size_from_disk"}, NULL},
    {sp "/disk_global/current/read_count", bt_uint64, {"glob_dm2_raw_read_cnt", "glob_dm2_bkup_raw_read_cnt"}, NULL},
    {sp "/disk_global/current/write_count", bt_uint64, {"glob_dm2_raw_write_cnt", "glob_dm2_bkup_raw_write_cnt"}, NULL},
    {sp "/disk_global/current/read_bytes", bt_uint64, {"glob_dm2_ext_read_bytes", "glob_dm2_bkup_ext_read_bytes"}, NULL},
    {sp "/ssp/current/num_of_seeks", bt_uint64, {"glob_ssp_seeks", "glob_ssp_bkup_seeks"}, NULL},
    {sp "/ssp/current/hash_veri_errs", bt_uint64, {"glob_ssp_hash_failures", "glob_ssp_bkup_hash_failures"}, NULL},
    {sp "/scheduler/current/runnable_q_pushes", bt_uint64, {"glob_sched_runnable_q_pushes", "glob_sched_bkup_runnable_q_pushes"}, NULL},
    {sp "/scheduler/current/runnable_q_pops", bt_uint64, {"glob_sched_runnable_q_pops", "glob_sched_bkup_runnable_q_pops"}, NULL},
    {sp "/scheduler/current/cleanup_q_pushes", bt_uint32, {"glob_sched_cleanup_q_pushes", "glob_sched_bkup_cleanup_q_pushes"}, NULL},
    {sp "/scheduler/current/cleanup_q_pops", bt_uint32, {"glob_sched_cleanup_q_pops", "glob_sched_bkup_cleanup_q_pops"}, NULL},
    {sp "/http/tunnel/current/error_count", bt_uint64, {"glob_fp_err_make_tunnel", "glob_fp_bkup_err_make_tunnel"}, NULL},
    {sp "/http/tunnel/current/total_http09_requests", bt_uint64, {"glob_http_09_request", "glob_http_bkup_09_request"}, NULL},
    {sp "/http/tunnel/current/total_connections", bt_uint64, {"glob_fp_tot_tunnel", "glob_fp_bkup_tot_tunnel"}, NULL},

    {sp "/nvsd/current/total_rtsp_byte_count", bt_uint64, {"glob_rtsp_tot_bytes_sent", "glob_rtsp_bkup_tot_bytes_sent"}, NULL},
    {sp "/nvsd/current/total_rtsp_vod_byte_count", bt_uint64, {"glob_rtsp_tot_vod_bytes_sent", "glob_rtsp_bkup_tot_vod_bytes_sent"}, NULL},
    {sp "/nvsd/current/num_rtsp_udp_pkt_fwd", bt_uint64, {"glob_rtp_tot_udp_packets_fwd", "glob_rtp_bkup_tot_udp_packets_fwd"}, NULL},
    {sp "/nvsd/current/glob_rtcp_tot_udp_packets_fwd", bt_uint64, {"glob_rtcp_tot_udp_packets_fwd", "glob_rtcp_bkup_tot_udp_packets_fwd"}, NULL},
    {sp "/nvsd/current/glob_rtcp_tot_udp_sr_packets_sent", bt_uint64, {"glob_rtcp_tot_udp_sr_packets_sent", "glob_rtcp_bkup_tot_udp_sr_packets_sent"}, NULL},
    {sp "/nvsd/current/glob_rtcp_tot_udp_rr_packets_sent", bt_uint64, {"glob_rtcp_tot_udp_rr_packets_sent", "glob_rtcp_bkup_tot_udp_rr_packets_sent"}, NULL},
    {sp "/nvsd/current/glob_rtcp_tot_udp_packets_recv", bt_uint64, {"glob_rtcp_tot_udp_packets_recv", "glob_rtcp_bkup_tot_udp_packets_recv"}, NULL},
    {sp "/nvsd/current/glob_tot_rtcp_sr", bt_uint64, {"glob_tot_rtcp_sr", "glob_tot_bkup_rtcp_sr"}, NULL},
    {sp "/nvsd/current/glob_tot_rtcp_rr", bt_uint64, {"glob_tot_rtcp_rr", "glob_tot_bkup_rtcp_rr"}, NULL},
    {sp "/nvsd/current/tot_num_cl_redirect", bt_uint64, {"glob_cll7redir_intercept_success", "glob_cll7redir_bkup_intercept_success"}, NULL},
    {sp "/nvsd/current/cl_redirect_int_err1", bt_uint64, {"glob_cll7redir_int_err_1", "glob_cll7redir_bkup_int_err_1"}, NULL},
    {sp "/nvsd/current/cl_redirect_int_err2", bt_uint64, {"glob_cll7redir_int_err_2", "glob_cll7redir_bkup_int_err_2"}, NULL},
    {sp "/nvsd/current/cl_redirect_int_errs", bt_uint64, {"glob_cll7redir_intercept_errs", "glob_cll7redir_bkup_int_errs"}, NULL},

    {sp "/rtsp/current/parse_error_count", bt_uint64, {"glob_rtsp_tot_rtsp_parse_error", "glob_rtsp_bkup_tot_rtsp_parse_error"}, NULL},
    {sp "/rtsp/response/current/200/count", bt_uint32, {"glob_rtsp_tot_status_resp_200", "glob_rtsp_bkup_tot_status_resp_200"}, NULL},
    {sp "/rtsp/response/current/400/count", bt_uint32, {"glob_rtsp_tot_status_resp_400", "glob_rtsp_bkup_tot_status_resp_400"}, NULL},
    {sp "/rtsp/response/current/404/count", bt_uint32, {"glob_rtsp_tot_status_resp_404", "glob_rtsp_bkup_tot_status_resp_404"}, NULL},
    {sp "/rtsp/response/current/500/count", bt_uint32, {"glob_rtsp_tot_status_resp_500", "glob_rtsp_bkup_tot_status_resp_500"}, NULL},
    {sp "/rtsp/response/current/501/count", bt_uint32, {"glob_rtsp_tot_status_resp_501", "glob_rtsp_bkup_tot_status_resp_501"}, NULL},
    {sp "/http/current/cache_hit_count", bt_uint64, {"glob_http_cache_hit_cnt", "glob_http_bkup_cache_hit_cnt"}, NULL},
};


/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
md_nkn_stats_init(
        const lc_dso_info *info,
        void *data);

/*----------------------------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static int
md_nkn_stats_add_initial(
        md_commit *commit,
        mdb_db    *db,
        void *arg);



static int
md_nkn_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_nkn_stats_get_uint32(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_nkn_stats_get_diff_uint32(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_nkn_stats_get_diff_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_nvsd_get_reset_uptime(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);
static int
md_get_http_client_response(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_get_http_server_response(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_get_rtsp_client_response(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_get_rtsp_server_response(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);


static int
md_get_http_client_open_conns(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_get_http_server_open_conns(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_get_http_server_conns(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_nkn_stats_get_uint16(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_nkn_dy_stats_get_uint32(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_nkn_dy_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_get_portno(
	md_commit *commit,
	const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags, void *arg);

static int
md_get_diskname(
	md_commit *commit,
	const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags, void *arg);

static int
md_iterate_ports(
	md_commit *commit,
	const mdb_db *db,
        const char *parent_node_name,
        const uint32_array *node_attrib_ids,
        int32 max_num_nodes, int32 start_child_num,
        const char *get_next_child,
        mdm_mon_iterate_names_cb_func iterate_cb,
        void *iterate_cb_arg,
	void *arg);

static int
md_iterate_diskname(
	md_commit *commit,
	const mdb_db *db,
        const char *parent_node_name,
        const uint32_array *node_attrib_ids,
        int32 max_num_nodes, int32 start_child_num,
        const char *get_next_child,
        mdm_mon_iterate_names_cb_func iterate_cb,
        void *iterate_cb_arg,
	void *arg);
static int
md_iterate_namespace(md_commit *commit, const mdb_db *db,
        const char *parent_node_name,
        const uint32_array *node_attrib_ids,
        int32 max_num_nodes, int32 start_child_num,
        const char *get_next_child,
        mdm_mon_iterate_names_cb_func iterate_cb,
        void *iterate_cb_arg, void *arg);

static int
md_graph_bandwidth
	(md_commit *commit,
	const mdb_db *db,
        const char *action_name,
        bn_binding_array *params,
        void *cb_arg);

static int
md_graph_bandwidth_new(md_commit *commit, const mdb_db *db,
                      bn_binding_array *params, tbool *ret_made_graph);
static int
md_graph_bw_day(md_commit *commit, const mdb_db *db,
                   const char *action_name,
                   bn_binding_array *params, void *cb_arg);

static int
md_graph_bw_week(md_commit *commit, const mdb_db *db,
                   const char *action_name,
                   bn_binding_array *params, void *cb_arg);

static int
md_graph_intf_day(md_commit *commit, const mdb_db *db,
                   const char *action_name,
                   bn_binding_array *params, void *cb_arg);

static int
md_graph_intf_week(md_commit *commit, const mdb_db *db,
                   const char *action_name,
                   bn_binding_array *params, void *cb_arg);

static int
md_system_get_cpucores(md_commit *commit, const mdb_db *db,
                    const char *node_name,
                    const bn_attrib_array *node_attribs,
                    bn_binding **ret_binding,
                    uint32 *ret_node_flags, void *arg);

static int
md_nkn_stats_graph_gen(md_commit *commit,
                             const mdb_db *old_db,
                             const mdb_db *new_db,
                             const mdb_db_change_array *change_list,
                             const tstring *node_name,
                             const tstr_array *node_name_parts,
                             mdb_db_change_type change_type,
                             const bn_attrib_array *old_attribs,
                             const bn_attrib_array *new_attribs,
                             void *arg);
static int
md_cntr_upgrade_downgrade(const mdb_db *old_db,
                         mdb_db *inout_new_db,
                         uint32 from_module_version,
                         uint32 to_module_version, void *arg);

static int
md_get_namespace(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg);


static int
md_get_proc_name(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_iterate_proc_name(
        md_commit *commit,
        const mdb_db *db,
        const char *parent_node_name,
        const uint32_array *node_attrib_ids,
        int32 max_num_nodes,
        int32 start_child_num,
        const char *get_next_child,
        mdm_mon_iterate_names_cb_func iterate_cb,
        void *iterate_cb_arg,
        void *arg);

static int
md_get_proc_mem(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int md_backup_file(const char *path,
	const char * dir_path,
        const char * file_name,
	uint32_t files_to_keep);

static int
md_get_total_bw(
	md_commit *commit,
	const mdb_db *db,
	const char *node_name,
	const bn_attrib_array *node_attribs,
	bn_binding **ret_binding,
	uint32 *ret_node_flags,
	void *arg);

/* Aggregated bw for serice interfaces */
static int
md_get_total_bw_service_intf(
	md_commit *commit,
	const mdb_db *db,
	const char *node_name,
	const bn_attrib_array *node_attribs,
	bn_binding **ret_binding,
	uint32 *ret_node_flags,
	void *arg);

static int
md_nkn_ns_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_nkn_wc_stats_get_uint32(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);
static int
md_nkn_wc_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_nkn_namespace_dm2_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_nkn_network_get_uint32(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static tbool
is_deliv_intf(const char *intf_node,
		md_commit *commit,
		const mdb_db *db,
		service_type_t service_type);

static int
md_nkn_stats_add_sample_cfg(md_commit *commit,
		mdb_db *db,
		int start_index,
		int end_index);

static int
md_nkn_stats_add_chd_cfg(md_commit *commit,
		mdb_db *db,
		int start_index,
		int end_index);

static int
md_get_namespace_config_string(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);
static int
md_get_namespace_config_bool(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);
static int
get_node_last_part(const char *full_node, tstring **last_name);


md_upgrade_rule_array *md_cntr_upgrade_rules = NULL;

const md_nkn_stat_graphs_t md_nkn_stat_graphs[] = {
    {"day/bandwidth_day" , 2, DAILY, "/tms/graphs/bw_day", md_graph_bw_day},
    {"week/bandwidth_week" , 2, WEEKLY, "/tms/graphs/bw_week", md_graph_bw_week},
    {"day/intf_day" , 2, DAILY, "/tms/graphs/intf_day", md_graph_intf_day},
    {"week/intf_week" , 2, WEEKLY, "/tms/graphs/intf_week", md_graph_intf_week},
};

/* NOTE md_nkn_stats.h has #define sc "/stats/config/" */

static const bn_str_value md_stats_upgrade_adds_16_to_17[] = {

   {sc "chd/mon_paging",              bt_string,      "mon_paging"},
   {"/stat/nkn/stats/mon_paging/label",bt_string,     "Monitor Paging"},
   {"/stat/nkn/stats/mon_paging/unit",bt_string,      "Number"},
   {sc "chd/mon_paging/enable",       bt_bool,        "true"},
   {sc "chd/mon_paging/source/type",  bt_string,      "sample"},
   {sc "chd/mon_paging/source/id",    bt_string,      "paging"},
   {sc "chd/mon_paging/num_to_keep",  bt_uint32,      "128"},
   {sc "chd/mon_paging/select_type",  bt_string,      "instances"},
   {sc "chd/mon_paging/instances/num_to_use",  bt_uint32,      "3"},
   {sc "chd/mon_paging/instances/num_to_advance",  bt_uint32,  "1"},
   {sc "chd/mon_paging/num_to_cache", bt_int32,       "-1"},
   {sc "chd/mon_paging/sync_interval", bt_uint32,     "10"},
   {sc "chd/mon_paging/time/interval_distance", bt_duration_sec, "0"},
   {sc "chd/mon_paging/time/interval_length", bt_duration_sec, "0"},
   {sc "chd/mon_paging/time/interval_phase", bt_duration_sec, "0"},
   {sc "chd/mon_paging/alarm_partial", bt_bool,       "false"},
   {sc "chd/mon_paging/calc_partial", bt_bool,        "false"},
   {sc "chd/mon_paging/function", bt_string,          "min"},
   {sc "chd/mon_intf_util",              bt_string,      "mon_intf_util"},
   {"/stat/nkn/stats/mon_intf_util/label",bt_string,     "Monitor Net Utilization"},
   {"/stat/nkn/stats/mon_intf_util/unit",bt_string,      "Per Sec"},
   {sc "chd/mon_intf_util/enable",       bt_bool,        "true"},
   {sc "chd/mon_intf_util/source/type",  bt_string,      "chd"},
   {sc "chd/mon_intf_util/source/id",    bt_string,      "intf_util"},
   {sc "chd/mon_intf_util/num_to_keep",  bt_uint32,      "128"},
   {sc "chd/mon_intf_util/select_type",  bt_string,      "instances"},
   {sc "chd/mon_intf_util/instances/num_to_use",  bt_uint32,      "3"},
   {sc "chd/mon_intf_util/instances/num_to_advance",  bt_uint32,  "1"},
   {sc "chd/mon_intf_util/num_to_cache", bt_int32,       "-1"},
   {sc "chd/mon_intf_util/sync_interval", bt_uint32,     "10"},
   {sc "chd/mon_intf_util/time/interval_distance", bt_duration_sec, "0"},
   {sc "chd/mon_intf_util/time/interval_length", bt_duration_sec, "0"},
   {sc "chd/mon_intf_util/time/interval_phase", bt_duration_sec, "0"},
   {sc "chd/mon_intf_util/alarm_partial", bt_bool,       "false"},
   {sc "chd/mon_intf_util/calc_partial", bt_bool,       "false"},
   {sc "chd/mon_intf_util/function", bt_string,          "min"},

};

static const bn_str_value md_stats_upgrade_adds_17_to_18[] = {

   {sc "alarm/aggr_cpu_util",              		bt_string, 	"aggr_cpu_util"},
   {sc "alarm/aggr_cpu_util/enable",              	bt_bool,     	"false"},
   {sc "alarm/aggr_cpu_util/disable_allowed",           bt_bool,      	"true"},
   {sc "alarm/aggr_cpu_util/ignore_first_value",        bt_bool,      	"true"},
   {sc "alarm/aggr_cpu_util/trigger/type",              bt_string,      "chd"},
   {sc "alarm/aggr_cpu_util/trigger/id",                bt_string,      "cpu_util_ave"},
   {sc "alarm/aggr_cpu_util/trigger/node_pattern",      bt_name,      	"/system/cpu/all/busy_pct"},
   {sc "alarm/aggr_cpu_util/rising/enable",             bt_bool,      	"true"},
   {sc "alarm/aggr_cpu_util/falling/enable",            bt_bool,      	"true"},
   {sc "alarm/aggr_cpu_util/rising/error_threshold",    bt_uint32,      "90"},
   {sc "alarm/aggr_cpu_util/rising/clear_threshold",    bt_uint32,      "70"},
   {sc "alarm/aggr_cpu_util/falling/error_threshold",   bt_uint32,      "0"},
   {sc "alarm/aggr_cpu_util/falling/clear_threshold",   bt_uint32,      "0"},
   {sc "alarm/aggr_cpu_util/rising/event_on_clear",     bt_bool,      	"true"},
   {sc "alarm/aggr_cpu_util/falling/event_on_clear",    bt_bool,      	"true"},
   {sc "alarm/aggr_cpu_util/event_name_root",           bt_string,      "aggr_cpu_util"},
   {sc "alarm/aggr_cpu_util/clear_if_missing",          bt_bool,      	"true"},
   {sc "alarm/aggr_cpu_util/long/rate_limit_max",       bt_uint32,      "50"},
   {sc "alarm/aggr_cpu_util/long/rate_limit_win",       bt_duration_sec,"604800"},
   {sc "alarm/aggr_cpu_util/medium/rate_limit_max",     bt_uint32,      "20"},
   {sc "alarm/aggr_cpu_util/medium/rate_limit_win",     bt_duration_sec,"86400"},
   {sc "alarm/aggr_cpu_util/short/rate_limit_max",      bt_uint32,      "5"},
   {sc "alarm/aggr_cpu_util/short/rate_limit_win",      bt_duration_sec,"3600"},

   {sc "sample/mem_util", 				bt_string,	"mem_util"},
   {sc "sample/mem_util/enable",			bt_bool,	"true"},
   {sc "sample/mem_util/node/"
   "\\/stat\\/nkn\\/system\\/memory\\/memutil_pct",	bt_name, 	"/stat/nkn/system/memory/memutil_pct"},
   {sc "sample/mem_util/interval",			bt_duration_sec,"10"},
   {sc "sample/mem_util/num_to_keep",			bt_uint32,	"120"},
   {sc "sample/mem_util/num_to_cache",			bt_int32,	"-1"},
   {sc "sample/mem_util/sample_method",			bt_string,	"direct"},
   {sc "sample/mem_util/delta_constraint",		bt_string,	"none"},
   {sc "sample/mem_util/sync_interval",			bt_uint32,	"10"},
   {sc "sample/mem_util/delta_average",			bt_bool,	"false"},
   {"/stat/nkn/stats/mem_util/label",			bt_string,	"Memory Utilization"},
   {"/stat/nkn/stats/mem_util/unit",			bt_string,	"Percent"},

   {sc "chd/mem_util_ave", 				bt_string,      "mem_util_ave"},
   {"/stat/nkn/stats/mem_util_ave/label",		bt_string,      "Average Memory Utilization"},
   {"/stat/nkn/stats/mem_util_ave/unit",		bt_string,      "Percent"},
   {sc "chd/mem_util_ave/enable",       		bt_bool,        "true"},
   {sc "chd/mem_util_ave/source/type",  		bt_string,      "sample"},
   {sc "chd/mem_util_ave/source/id",    		bt_string,      "mem_util"},
   {sc "chd/mem_util_ave/num_to_keep",  		bt_uint32,      "128"},
   {sc "chd/mem_util_ave/select_type",  		bt_string,      "instances"},
   {sc "chd/mem_util_ave/instances/num_to_use", 	bt_uint32,      "6"},
   {sc "chd/mem_util_ave/instances/num_to_advance",	bt_uint32,  	"1"},
   {sc "chd/mem_util_ave/num_to_cache", 		bt_int32,       "-1"},
   {sc "chd/mem_util_ave/sync_interval", 		bt_uint32,     	"10"},
   {sc "chd/mem_util_ave/time/interval_distance", 	bt_duration_sec,"0"},
   {sc "chd/mem_util_ave/time/interval_length", 	bt_duration_sec,"0"},
   {sc "chd/mem_util_ave/time/interval_phase", 		bt_duration_sec,"0"},
   {sc "chd/mem_util_ave/alarm_partial", 		bt_bool,       	"false"},
   {sc "chd/mem_util_ave/calc_partial", 		bt_bool,       	"false"},
   {sc "chd/mem_util_ave/function", 			bt_string,      "mean"},

   {sc "alarm/nkn_mem_util",                          	bt_string,      "nkn_mem_util"},
   {sc "alarm/nkn_mem_util/enable",                   	bt_bool,        "false"},
   {sc "alarm/nkn_mem_util/disable_allowed",           	bt_bool,        "true"},
   {sc "alarm/nkn_mem_util/ignore_first_value",        	bt_bool,        "true"},
   {sc "alarm/nkn_mem_util/trigger/type",              	bt_string,      "chd"},
   {sc "alarm/nkn_mem_util/trigger/id",                	bt_string,      "mem_util_ave"},
   {sc "alarm/nkn_mem_util/trigger/node_pattern",      	bt_name,        "/stat/nkn/system/memory/memutil_pct"},
   {sc "alarm/nkn_mem_util/rising/enable",             	bt_bool,        "true"},
   {sc "alarm/nkn_mem_util/falling/enable",            	bt_bool,        "true"},
   {sc "alarm/nkn_mem_util/rising/error_threshold",    	bt_uint32,      "90"},
   {sc "alarm/nkn_mem_util/rising/clear_threshold",    	bt_uint32,      "87"},
   {sc "alarm/nkn_mem_util/falling/error_threshold",   	bt_uint32,      "0"},
   {sc "alarm/nkn_mem_util/falling/clear_threshold",   	bt_uint32,      "0"},
   {sc "alarm/nkn_mem_util/rising/event_on_clear",     	bt_bool,        "true"},
   {sc "alarm/nkn_mem_util/falling/event_on_clear",    	bt_bool,        "true"},
   {sc "alarm/nkn_mem_util/event_name_root",           	bt_string,      "nkn_mem_util"},
   {sc "alarm/nkn_mem_util/clear_if_missing",          	bt_bool,        "true"},
   {sc "alarm/nkn_mem_util/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/nkn_mem_util/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/nkn_mem_util/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/nkn_mem_util/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/nkn_mem_util/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/nkn_mem_util/short/rate_limit_win",       bt_duration_sec,"3600"},


};
static const bn_str_value md_stats_upgrade_adds_18_to_19[] = {
   {sc "sample/sas_disk_space",                               bt_string,      "sas_disk_space"},
   {sc "sample/sas_disk_space/enable",                        bt_bool,        "true"},
   {sc "sample/sas_disk_space/node/"
   "\\/nkn\\/monitor\\/sas\\/disk\\/*\\/freespace",     bt_name,        "/nkn/monitor/sas/disk/*/freespace"},
   {sc "sample/sas_disk_space/interval",                      bt_duration_sec,"10"},
   {sc "sample/sas_disk_space/num_to_keep",                   bt_uint32,      "120"},
   {sc "sample/sas_disk_space/num_to_cache",                  bt_int32,       "-1"},
   {sc "sample/sas_disk_space/sample_method",                 bt_string,      "direct"},
   {sc "sample/sas_disk_space/delta_constraint",              bt_string,      "none"},
   {sc "sample/sas_disk_space/sync_interval",                 bt_uint32,      "10"},
   {sc "sample/sas_disk_space/delta_average",                 bt_bool,        "false"},
   {"/stat/nkn/stats/sas_disk_space/label",                   bt_string,      "Free Space on SAS Disks"},
   {"/stat/nkn/stats/sas_disk_space/unit",                    bt_string,      "Bytes"},

   {sc "chd/sas_disk_space",                              bt_string,      "sas_disk_space"},
   {"/stat/nkn/stats/sas_disk_space/label",               bt_string,      "Monitor free space on SAS Disks"},
   {"/stat/nkn/stats/sas_disk_space/unit",                bt_string,      "Bytes"},
   {sc "chd/sas_disk_space/enable",                       bt_bool,        "true"},
   {sc "chd/sas_disk_space/source/type",                  bt_string,      "sample"},
   {sc "chd/sas_disk_space/source/id",                    bt_string,      "sas_disk_space"},
   {sc "chd/sas_disk_space/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/sas_disk_space/select_type",                  bt_string,      "instances"},
   {sc "chd/sas_disk_space/instances/num_to_use",         bt_uint32,      "3"},
   {sc "chd/sas_disk_space/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/sas_disk_space/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/sas_disk_space/sync_interval",                bt_uint32,      "10"},
   {sc "chd/sas_disk_space/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/sas_disk_space/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/sas_disk_space/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/sas_disk_space/alarm_partial",                bt_bool,        "false"},
   {sc "chd/sas_disk_space/calc_partial",                 bt_bool,        "false"},
   {sc "chd/sas_disk_space/function",                     bt_string,      "max"},

   {sc "alarm/sas_disk_space",                            bt_string,      "sas_disk_space"},
   {sc "alarm/sas_disk_space/enable",                     bt_bool,        "false"},
   {sc "alarm/sas_disk_space/disable_allowed",            bt_bool,        "true"},
   {sc "alarm/sas_disk_space/ignore_first_value",         bt_bool,        "true"},
   {sc "alarm/sas_disk_space/trigger/type",               bt_string,      "chd"},
   {sc "alarm/sas_disk_space/trigger/id",                 bt_string,      "sas_disk_space"},
   {sc "alarm/sas_disk_space/trigger/node_pattern",       bt_name,        "/nkn/monitor/sas/disk/*/freespace"},
   {sc "alarm/sas_disk_space/rising/enable",              bt_bool,        "false"},
   {sc "alarm/sas_disk_space/falling/enable",             bt_bool,        "true"},
   {sc "alarm/sas_disk_space/rising/error_threshold",     bt_uint32,      "0"},
   {sc "alarm/sas_disk_space/rising/clear_threshold",     bt_uint32,      "0"},
   {sc "alarm/sas_disk_space/falling/error_threshold",    bt_uint32,      "7"},
   {sc "alarm/sas_disk_space/falling/clear_threshold",    bt_uint32,      "10"},
   {sc "alarm/sas_disk_space/rising/event_on_clear",      bt_bool,        "true"},
   {sc "alarm/sas_disk_space/falling/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/sas_disk_space/event_name_root",            bt_string,      "nkn_disk_space"},
   {sc "alarm/sas_disk_space/clear_if_missing",           bt_bool,        "true"},
   {sc "alarm/sas_disk_space/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/sas_disk_space/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/sas_disk_space/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/sas_disk_space/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/sas_disk_space/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/sas_disk_space/short/rate_limit_win",       bt_duration_sec,"3600"},

   {sc "sample/sata_disk_space",                               bt_string,      "sata_disk_space"},
   {sc "sample/sata_disk_space/enable",                        bt_bool,        "true"},
   {sc "sample/sata_disk_space/node/"
   "\\/nkn\\/monitor\\/sata\\/disk\\/*\\/freespace",     	   bt_name,        "/nkn/monitor/sata/disk/*/freespace"},
   {sc "sample/sata_disk_space/interval",                      bt_duration_sec,"10"},
   {sc "sample/sata_disk_space/num_to_keep",                   bt_uint32,      "120"},
   {sc "sample/sata_disk_space/num_to_cache",                  bt_int32,       "-1"},
   {sc "sample/sata_disk_space/sample_method",                 bt_string,      "direct"},
   {sc "sample/sata_disk_space/delta_constraint",              bt_string,      "none"},
   {sc "sample/sata_disk_space/sync_interval",                 bt_uint32,      "10"},
   {sc "sample/sata_disk_space/delta_average",                 bt_bool,        "false"},
   {"/stat/nkn/stats/sata_disk_space/label",                   bt_string,      "Free Space on SATA Disks"},
   {"/stat/nkn/stats/sata_disk_space/unit",                    bt_string,      "Bytes"},

   {sc "chd/sata_disk_space",                              bt_string,      "sata_disk_space"},
   {"/stat/nkn/stats/sata_disk_space/label",               bt_string,      "Monitor free space on SATA Disks"},
   {"/stat/nkn/stats/sata_disk_space/unit",                bt_string,      "Bytes"},
   {sc "chd/sata_disk_space/enable",                       bt_bool,        "true"},
   {sc "chd/sata_disk_space/source/type",                  bt_string,      "sample"},
   {sc "chd/sata_disk_space/source/id",                    bt_string,      "sata_disk_space"},
   {sc "chd/sata_disk_space/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/sata_disk_space/select_type",                  bt_string,      "instances"},
   {sc "chd/sata_disk_space/instances/num_to_use",         bt_uint32,      "3"},
   {sc "chd/sata_disk_space/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/sata_disk_space/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/sata_disk_space/sync_interval",                bt_uint32,      "10"},
   {sc "chd/sata_disk_space/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/sata_disk_space/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/sata_disk_space/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/sata_disk_space/alarm_partial",                bt_bool,        "false"},
   {sc "chd/sata_disk_space/calc_partial",                 bt_bool,        "false"},
   {sc "chd/sata_disk_space/function",                     bt_string,      "max"},

   {sc "alarm/sata_disk_space",                            bt_string,      "sata_disk_space"},
   {sc "alarm/sata_disk_space/enable",                     bt_bool,        "false"},
   {sc "alarm/sata_disk_space/disable_allowed",            bt_bool,        "true"},
   {sc "alarm/sata_disk_space/ignore_first_value",         bt_bool,        "true"},
   {sc "alarm/sata_disk_space/trigger/type",               bt_string,      "chd"},
   {sc "alarm/sata_disk_space/trigger/id",                 bt_string,      "sata_disk_space"},
   {sc "alarm/sata_disk_space/trigger/node_pattern",       bt_name,        "/nkn/monitor/sata/disk/*/freespace"},
   {sc "alarm/sata_disk_space/rising/enable",              bt_bool,        "false"},
   {sc "alarm/sata_disk_space/falling/enable",             bt_bool,        "true"},
   {sc "alarm/sata_disk_space/rising/error_threshold",     bt_uint32,      "0"},
   {sc "alarm/sata_disk_space/rising/clear_threshold",     bt_uint32,      "0"},
   {sc "alarm/sata_disk_space/falling/error_threshold",    bt_uint32,      "7"},
   {sc "alarm/sata_disk_space/falling/clear_threshold",    bt_uint32,      "10"},
   {sc "alarm/sata_disk_space/rising/event_on_clear",      bt_bool,        "true"},
   {sc "alarm/sata_disk_space/falling/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/sata_disk_space/event_name_root",            bt_string,      "nkn_disk_space"},
   {sc "alarm/sata_disk_space/clear_if_missing",           bt_bool,        "true"},
   {sc "alarm/sata_disk_space/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/sata_disk_space/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/sata_disk_space/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/sata_disk_space/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/sata_disk_space/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/sata_disk_space/short/rate_limit_win",       bt_duration_sec,"3600"},

   {sc "sample/ssd_disk_space",                                bt_string,      "ssd_disk_space"},
   {sc "sample/ssd_disk_space/enable",                         bt_bool,        "true"},
   {sc "sample/ssd_disk_space/node/"
   "\\/nkn\\/monitor\\/ssd\\/disk\\/*\\/freespace",     	   bt_name,        "/nkn/monitor/ssd/disk/*/freespace"},
   {sc "sample/ssd_disk_space/interval",                       bt_duration_sec,"10"},
   {sc "sample/ssd_disk_space/num_to_keep",                    bt_uint32,      "120"},
   {sc "sample/ssd_disk_space/num_to_cache",                   bt_int32,       "-1"},
   {sc "sample/ssd_disk_space/sample_method",                  bt_string,      "direct"},
   {sc "sample/ssd_disk_space/delta_constraint",               bt_string,      "none"},
   {sc "sample/ssd_disk_space/sync_interval",                  bt_uint32,      "10"},
   {sc "sample/ssd_disk_space/delta_average",                  bt_bool,        "false"},
   {"/stat/nkn/stats/ssd_disk_space/label",                    bt_string,      "Free Space on SSD Disks"},
   {"/stat/nkn/stats/ssd_disk_space/unit",                     bt_string,      "Bytes"},

   {sc "chd/ssd_disk_space",                              bt_string,      "ssd_disk_space"},
   {"/stat/nkn/stats/ssd_disk_space/label",               bt_string,      "Monitor free space on SSD Disks"},
   {"/stat/nkn/stats/ssd_disk_space/unit",                bt_string,      "Bytes"},
   {sc "chd/ssd_disk_space/enable",                       bt_bool,        "true"},
   {sc "chd/ssd_disk_space/source/type",                  bt_string,      "sample"},
   {sc "chd/ssd_disk_space/source/id",                    bt_string,      "ssd_disk_space"},
   {sc "chd/ssd_disk_space/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/ssd_disk_space/select_type",                  bt_string,      "instances"},
   {sc "chd/ssd_disk_space/instances/num_to_use",         bt_uint32,      "3"},
   {sc "chd/ssd_disk_space/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/ssd_disk_space/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/ssd_disk_space/sync_interval",                bt_uint32,      "10"},
   {sc "chd/ssd_disk_space/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/ssd_disk_space/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/ssd_disk_space/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/ssd_disk_space/alarm_partial",                bt_bool,        "false"},
   {sc "chd/ssd_disk_space/calc_partial",                 bt_bool,        "false"},
   {sc "chd/ssd_disk_space/function",                     bt_string,      "max"},

   {sc "alarm/ssd_disk_space",                            bt_string,      "ssd_disk_space"},
   {sc "alarm/ssd_disk_space/enable",                     bt_bool,        "false"},
   {sc "alarm/ssd_disk_space/disable_allowed",            bt_bool,        "true"},
   {sc "alarm/ssd_disk_space/ignore_first_value",         bt_bool,        "true"},
   {sc "alarm/ssd_disk_space/trigger/type",               bt_string,      "chd"},
   {sc "alarm/ssd_disk_space/trigger/id",                 bt_string,      "ssd_disk_space"},
   {sc "alarm/ssd_disk_space/trigger/node_pattern",       bt_name,        "/nkn/monitor/ssd/disk/*/freespace"},
   {sc "alarm/ssd_disk_space/rising/enable",              bt_bool,        "false"},
   {sc "alarm/ssd_disk_space/falling/enable",             bt_bool,        "true"},
   {sc "alarm/ssd_disk_space/rising/error_threshold",     bt_uint32,      "0"},
   {sc "alarm/ssd_disk_space/rising/clear_threshold",     bt_uint32,      "0"},
   {sc "alarm/ssd_disk_space/falling/error_threshold",    bt_uint32,      "7"},
   {sc "alarm/ssd_disk_space/falling/clear_threshold",    bt_uint32,      "10"},
   {sc "alarm/ssd_disk_space/rising/event_on_clear",      bt_bool,        "true"},
   {sc "alarm/ssd_disk_space/falling/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/ssd_disk_space/event_name_root",            bt_string,      "nkn_disk_space"},
   {sc "alarm/ssd_disk_space/clear_if_missing",           bt_bool,        "true"},
   {sc "alarm/ssd_disk_space/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/ssd_disk_space/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/ssd_disk_space/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/ssd_disk_space/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/ssd_disk_space/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/ssd_disk_space/short/rate_limit_win",       bt_duration_sec,"3600"},

   {sc "sample/root_disk_space",                               bt_string,      "root_disk_space"},
   {sc "sample/root_disk_space/enable",                        bt_bool,        "true"},
   {sc "sample/root_disk_space/node/"
   "\\/nkn\\/monitor\\/root\\/disk\\/*\\/freespace",     	   bt_name,        "/nkn/monitor/root/disk/*/freespace"},
   {sc "sample/root_disk_space/interval",                      bt_duration_sec,"10"},
   {sc "sample/root_disk_space/num_to_keep",                   bt_uint32,      "120"},
   {sc "sample/root_disk_space/num_to_cache",                  bt_int32,       "-1"},
   {sc "sample/root_disk_space/sample_method",                 bt_string,      "direct"},
   {sc "sample/root_disk_space/delta_constraint",              bt_string,      "none"},
   {sc "sample/root_disk_space/sync_interval",                 bt_uint32,      "10"},
   {sc "sample/root_disk_space/delta_average",                 bt_bool,        "false"},
   {"/stat/nkn/stats/root_disk_space/label",                   bt_string,      "Free Space on Root Disk"},
   {"/stat/nkn/stats/root_disk_space/unit",                    bt_string,      "Bytes"},

   {sc "chd/root_disk_space",                              bt_string,      "root_disk_space"},
   {"/stat/nkn/stats/root_disk_space/label",               bt_string,      "Monitor free space on Root Disk Partitions"},
   {"/stat/nkn/stats/root_disk_space/unit",                bt_string,      "Bytes"},
   {sc "chd/root_disk_space/enable",                       bt_bool,        "true"},
   {sc "chd/root_disk_space/source/type",                  bt_string,      "sample"},
   {sc "chd/root_disk_space/source/id",                    bt_string,      "root_disk_space"},
   {sc "chd/root_disk_space/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/root_disk_space/select_type",                  bt_string,      "instances"},
   {sc "chd/root_disk_space/instances/num_to_use",         bt_uint32,      "3"},
   {sc "chd/root_disk_space/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/root_disk_space/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/root_disk_space/sync_interval",                bt_uint32,      "10"},
   {sc "chd/root_disk_space/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/root_disk_space/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/root_disk_space/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/root_disk_space/alarm_partial",                bt_bool,        "false"},
   {sc "chd/root_disk_space/calc_partial",                 bt_bool,        "false"},
   {sc "chd/root_disk_space/function",                     bt_string,      "max"},

   {sc "alarm/root_disk_space",                            bt_string,      "root_disk_space"},
   {sc "alarm/root_disk_space/enable",                     bt_bool,        "false"},
   {sc "alarm/root_disk_space/disable_allowed",            bt_bool,        "true"},
   {sc "alarm/root_disk_space/ignore_first_value",         bt_bool,        "true"},
   {sc "alarm/root_disk_space/trigger/type",               bt_string,      "chd"},
   {sc "alarm/root_disk_space/trigger/id",                 bt_string,      "root_disk_space"},
   {sc "alarm/root_disk_space/trigger/node_pattern",       bt_name,        "/nkn/monitor/root/disk/*/freespace"},
   {sc "alarm/root_disk_space/rising/enable",              bt_bool,        "false"},
   {sc "alarm/root_disk_space/falling/enable",             bt_bool,        "true"},
   {sc "alarm/root_disk_space/rising/error_threshold",     bt_uint32,      "0"},
   {sc "alarm/root_disk_space/rising/clear_threshold",     bt_uint32,      "0"},
   {sc "alarm/root_disk_space/falling/error_threshold",    bt_uint32,      "7"},
   {sc "alarm/root_disk_space/falling/clear_threshold",    bt_uint32,      "10"},
   {sc "alarm/root_disk_space/rising/event_on_clear",      bt_bool,        "true"},
   {sc "alarm/root_disk_space/falling/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/root_disk_space/event_name_root",            bt_string,      "nkn_disk_space"},
   {sc "alarm/root_disk_space/clear_if_missing",           bt_bool,        "true"},
   {sc "alarm/root_disk_space/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/root_disk_space/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/root_disk_space/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/root_disk_space/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/root_disk_space/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/root_disk_space/short/rate_limit_win",       bt_duration_sec,"3600"},

   {sc "sample/sas_disk_read",                               bt_string,      "sas_disk_read"},
   {sc "sample/sas_disk_read/enable",                         bt_bool,        "true"},
   {sc "sample/sas_disk_read/node/"
   "\\/nkn\\/monitor\\/sas\\/disk\\/*\\/read",     		   bt_name,        "/nkn/monitor/sas/disk/*/read"},
   {sc "sample/sas_disk_read/interval",                       bt_duration_sec,"10"},
   {sc "sample/sas_disk_read/num_to_keep",                    bt_uint32,      "120"},
   {sc "sample/sas_disk_read/num_to_cache",                   bt_int32,       "-1"},
   {sc "sample/sas_disk_read/sample_method",                  bt_string,      "delta"},
   {sc "sample/sas_disk_read/delta_constraint",               bt_string,      "increasing"},
   {sc "sample/sas_disk_read/sync_interval",                  bt_uint32,      "10"},
   {sc "sample/sas_disk_read/delta_average",                  bt_bool,        "false"},
   {"/stat/nkn/stats/sas_disk_read/label",                    bt_string,      "Bytes read from SAS Disks"},
   {"/stat/nkn/stats/sas_disk_read/unit",                     bt_string,      "Bytes"},

   {sc "chd/sas_disk_bw",                              bt_string,      "sas_disk_bw"},
   {"/stat/nkn/stats/sas_disk_bw/label",               bt_string,      "Disk Bandwidth of SAS Disks"},
   {"/stat/nkn/stats/sas_disk_bw/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/sas_disk_bw/enable",                       bt_bool,        "true"},
   {sc "chd/sas_disk_bw/source/type",                  bt_string,      "sample"},
   {sc "chd/sas_disk_bw/source/id",                    bt_string,      "sas_disk_read"},
   {sc "chd/sas_disk_bw/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/sas_disk_bw/select_type",                  bt_string,      "instances"},
   {sc "chd/sas_disk_bw/instances/num_to_use",         bt_uint32,      "1"},
   {sc "chd/sas_disk_bw/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/sas_disk_bw/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/sas_disk_bw/sync_interval",                bt_uint32,      "10"},
   {sc "chd/sas_disk_bw/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/sas_disk_bw/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/sas_disk_bw/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/sas_disk_bw/alarm_partial",                bt_bool,        "false"},
   {sc "chd/sas_disk_bw/calc_partial",                 bt_bool,        "false"},
   {sc "chd/sas_disk_bw/function",                     bt_string,      "nkn_disk_bw"},

  {sc "chd/mon_sas_disk_bw",                              bt_string,      "mon_sas_disk_bw"},
   {"/stat/nkn/stats/mon_sas_disk_bw/label",               bt_string,      "Monitor Disk Bandwidth of SAS Disks"},
   {"/stat/nkn/stats/mon_sas_disk_bw/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/mon_sas_disk_bw/enable",                       bt_bool,        "true"},
   {sc "chd/mon_sas_disk_bw/source/type",                  bt_string,      "chd"},
   {sc "chd/mon_sas_disk_bw/source/id",                    bt_string,      "sas_disk_bw"},
   {sc "chd/mon_sas_disk_bw/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/mon_sas_disk_bw/select_type",                  bt_string,      "instances"},
   {sc "chd/mon_sas_disk_bw/instances/num_to_use",         bt_uint32,      "3"},
   {sc "chd/mon_sas_disk_bw/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/mon_sas_disk_bw/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/mon_sas_disk_bw/sync_interval",                bt_uint32,      "10"},
   {sc "chd/mon_sas_disk_bw/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/mon_sas_disk_bw/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/mon_sas_disk_bw/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/mon_sas_disk_bw/alarm_partial",                bt_bool,        "false"},
   {sc "chd/mon_sas_disk_bw/calc_partial",                 bt_bool,        "false"},
   {sc "chd/mon_sas_disk_bw/function",                     bt_string,      "min"},

   {sc "alarm/sas_disk_bw",                            bt_string,      "sas_disk_bw"},
   {sc "alarm/sas_disk_bw/enable",                     bt_bool,        "false"},
   {sc "alarm/sas_disk_bw/disable_allowed",            bt_bool,        "true"},
   {sc "alarm/sas_disk_bw/ignore_first_value",         bt_bool,        "true"},
   {sc "alarm/sas_disk_bw/trigger/type",               bt_string,      "chd"},
   {sc "alarm/sas_disk_bw/trigger/id",                 bt_string,      "mon_sas_disk_bw"},
   {sc "alarm/sas_disk_bw/trigger/node_pattern",       bt_name,        "/nkn/monitor/sas/disk/*/disk_bw"},
   {sc "alarm/sas_disk_bw/rising/enable",              bt_bool,        "true"},
   {sc "alarm/sas_disk_bw/falling/enable",             bt_bool,        "false"},
   {sc "alarm/sas_disk_bw/rising/error_threshold",     bt_uint32,      "200000000"},
   {sc "alarm/sas_disk_bw/rising/clear_threshold",     bt_uint32,      "100000000"},
   {sc "alarm/sas_disk_bw/falling/error_threshold",    bt_uint32,      "0"},
   {sc "alarm/sas_disk_bw/falling/clear_threshold",    bt_uint32,      "0"},
   {sc "alarm/sas_disk_bw/rising/event_on_clear",      bt_bool,        "true"},
   {sc "alarm/sas_disk_bw/falling/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/sas_disk_bw/event_name_root",            bt_string,      "nkn_disk_bw"},
   {sc "alarm/sas_disk_bw/clear_if_missing",           bt_bool,        "true"},
   {sc "alarm/sas_disk_bw/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/sas_disk_bw/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/sas_disk_bw/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/sas_disk_bw/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/sas_disk_bw/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/sas_disk_bw/short/rate_limit_win",       bt_duration_sec,"3600"},

   {sc "sample/sata_disk_read",                               bt_string,      "sata_disk_read"},
   {sc "sample/sata_disk_read/enable",                         bt_bool,        "true"},
   {sc "sample/sata_disk_read/node/"
   "\\/nkn\\/monitor\\/sata\\/disk\\/*\\/read",     		   bt_name,        "/nkn/monitor/sata/disk/*/read"},
   {sc "sample/sata_disk_read/interval",                       bt_duration_sec,"10"},
   {sc "sample/sata_disk_read/num_to_keep",                    bt_uint32,      "120"},
   {sc "sample/sata_disk_read/num_to_cache",                   bt_int32,       "-1"},
   {sc "sample/sata_disk_read/sample_method",                  bt_string,      "delta"},
   {sc "sample/sata_disk_read/delta_constraint",               bt_string,      "increasing"},
   {sc "sample/sata_disk_read/sync_interval",                  bt_uint32,      "10"},
   {sc "sample/sata_disk_read/delta_average",                  bt_bool,        "false"},
   {"/stat/nkn/stats/sata_disk_read/label",                    bt_string,      "Bytes read from SATA Disks"},
   {"/stat/nkn/stats/sata_disk_read/unit",                     bt_string,      "Bytes"},

  {sc "chd/sata_disk_bw",                              bt_string,      "sata_disk_bw"},
   {"/stat/nkn/stats/sata_disk_bw/label",               bt_string,      "Disk Bandwidth of SATA Disks"},
   {"/stat/nkn/stats/sata_disk_bw/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/sata_disk_bw/enable",                       bt_bool,        "true"},
   {sc "chd/sata_disk_bw/source/type",                  bt_string,      "sample"},
   {sc "chd/sata_disk_bw/source/id",                    bt_string,      "sata_disk_read"},
   {sc "chd/sata_disk_bw/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/sata_disk_bw/select_type",                  bt_string,      "instances"},
   {sc "chd/sata_disk_bw/instances/num_to_use",         bt_uint32,      "1"},
   {sc "chd/sata_disk_bw/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/sata_disk_bw/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/sata_disk_bw/sync_interval",                bt_uint32,      "10"},
   {sc "chd/sata_disk_bw/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/sata_disk_bw/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/sata_disk_bw/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/sata_disk_bw/alarm_partial",                bt_bool,        "false"},
   {sc "chd/sata_disk_bw/calc_partial",                 bt_bool,        "false"},
   {sc "chd/sata_disk_bw/function",                     bt_string,      "nkn_disk_bw"},

  {sc "chd/mon_sata_disk_bw",                              bt_string,      "mon_sata_disk_bw"},
   {"/stat/nkn/stats/mon_sata_disk_bw/label",               bt_string,      "Monitor Disk Bandwidth of SATA Disks"},
   {"/stat/nkn/stats/mon_sata_disk_bw/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/mon_sata_disk_bw/enable",                       bt_bool,        "true"},
   {sc "chd/mon_sata_disk_bw/source/type",                  bt_string,      "chd"},
   {sc "chd/mon_sata_disk_bw/source/id",                    bt_string,      "sata_disk_bw"},
   {sc "chd/mon_sata_disk_bw/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/mon_sata_disk_bw/select_type",                  bt_string,      "instances"},
   {sc "chd/mon_sata_disk_bw/instances/num_to_use",         bt_uint32,      "3"},
   {sc "chd/mon_sata_disk_bw/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/mon_sata_disk_bw/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/mon_sata_disk_bw/sync_interval",                bt_uint32,      "10"},
   {sc "chd/mon_sata_disk_bw/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/mon_sata_disk_bw/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/mon_sata_disk_bw/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/mon_sata_disk_bw/alarm_partial",                bt_bool,        "false"},
   {sc "chd/mon_sata_disk_bw/calc_partial",                 bt_bool,        "false"},
   {sc "chd/mon_sata_disk_bw/function",                     bt_string,      "min"},

   {sc "alarm/sata_disk_bw",                            bt_string,      "sata_disk_bw"},
   {sc "alarm/sata_disk_bw/enable",                     bt_bool,        "false"},
   {sc "alarm/sata_disk_bw/disable_allowed",            bt_bool,        "true"},
   {sc "alarm/sata_disk_bw/ignore_first_value",         bt_bool,        "true"},
   {sc "alarm/sata_disk_bw/trigger/type",               bt_string,      "chd"},
   {sc "alarm/sata_disk_bw/trigger/id",                 bt_string,      "mon_sata_disk_bw"},
   {sc "alarm/sata_disk_bw/trigger/node_pattern",       bt_name,        "/nkn/monitor/sata/disk/*/disk_bw"},
   {sc "alarm/sata_disk_bw/rising/enable",              bt_bool,        "true"},
   {sc "alarm/sata_disk_bw/falling/enable",             bt_bool,        "false"},
   {sc "alarm/sata_disk_bw/rising/error_threshold",     bt_uint32,      "200000000"},
   {sc "alarm/sata_disk_bw/rising/clear_threshold",     bt_uint32,      "100000000"},
   {sc "alarm/sata_disk_bw/falling/error_threshold",    bt_uint32,      "0"},
   {sc "alarm/sata_disk_bw/falling/clear_threshold",    bt_uint32,      "0"},
   {sc "alarm/sata_disk_bw/rising/event_on_clear",      bt_bool,        "true"},
   {sc "alarm/sata_disk_bw/falling/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/sata_disk_bw/event_name_root",            bt_string,      "nkn_disk_bw"},
   {sc "alarm/sata_disk_bw/clear_if_missing",           bt_bool,        "true"},
   {sc "alarm/sata_disk_bw/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/sata_disk_bw/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/sata_disk_bw/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/sata_disk_bw/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/sata_disk_bw/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/sata_disk_bw/short/rate_limit_win",       bt_duration_sec,"3600"},

   {sc "sample/ssd_disk_read",                               bt_string,      "ssd_disk_read"},
   {sc "sample/ssd_disk_read/enable",                         bt_bool,        "true"},
   {sc "sample/ssd_disk_read/node/"
   "\\/nkn\\/monitor\\/ssd\\/disk\\/*\\/read",     		   bt_name,        "/nkn/monitor/ssd/disk/*/read"},
   {sc "sample/ssd_disk_read/interval",                       bt_duration_sec,"10"},
   {sc "sample/ssd_disk_read/num_to_keep",                    bt_uint32,      "120"},
   {sc "sample/ssd_disk_read/num_to_cache",                   bt_int32,       "-1"},
   {sc "sample/ssd_disk_read/sample_method",                  bt_string,      "delta"},
   {sc "sample/ssd_disk_read/delta_constraint",               bt_string,      "increasing"},
   {sc "sample/ssd_disk_read/sync_interval",                  bt_uint32,      "10"},
   {sc "sample/ssd_disk_read/delta_average",                  bt_bool,        "false"},
   {"/stat/nkn/stats/ssd_disk_read/label",                    bt_string,      "Bytes read from SSD Disks"},
   {"/stat/nkn/stats/ssd_disk_read/unit",                     bt_string,      "Bytes"},

  {sc "chd/ssd_disk_bw",                              bt_string,      "ssd_disk_bw"},
   {"/stat/nkn/stats/ssd_disk_bw/label",               bt_string,      "Disk Bandwidth of SSD Disks"},
   {"/stat/nkn/stats/ssd_disk_bw/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/ssd_disk_bw/enable",                       bt_bool,        "true"},
   {sc "chd/ssd_disk_bw/source/type",                  bt_string,      "sample"},
   {sc "chd/ssd_disk_bw/source/id",                    bt_string,      "ssd_disk_read"},
   {sc "chd/ssd_disk_bw/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/ssd_disk_bw/select_type",                  bt_string,      "instances"},
   {sc "chd/ssd_disk_bw/instances/num_to_use",         bt_uint32,      "1"},
   {sc "chd/ssd_disk_bw/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/ssd_disk_bw/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/ssd_disk_bw/sync_interval",                bt_uint32,      "10"},
   {sc "chd/ssd_disk_bw/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/ssd_disk_bw/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/ssd_disk_bw/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/ssd_disk_bw/alarm_partial",                bt_bool,        "false"},
   {sc "chd/ssd_disk_bw/calc_partial",                 bt_bool,        "false"},
   {sc "chd/ssd_disk_bw/function",                     bt_string,      "nkn_disk_bw"},

  {sc "chd/mon_ssd_disk_bw",                              bt_string,      "mon_ssd_disk_bw"},
   {"/stat/nkn/stats/mon_ssd_disk_bw/label",               bt_string,      "Monitor Disk Bandwidth of SSD Disks"},
   {"/stat/nkn/stats/mon_ssd_disk_bw/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/mon_ssd_disk_bw/enable",                       bt_bool,        "true"},
   {sc "chd/mon_ssd_disk_bw/source/type",                  bt_string,      "chd"},
   {sc "chd/mon_ssd_disk_bw/source/id",                    bt_string,      "ssd_disk_bw"},
   {sc "chd/mon_ssd_disk_bw/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/mon_ssd_disk_bw/select_type",                  bt_string,      "instances"},
   {sc "chd/mon_ssd_disk_bw/instances/num_to_use",         bt_uint32,      "3"},
   {sc "chd/mon_ssd_disk_bw/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/mon_ssd_disk_bw/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/mon_ssd_disk_bw/sync_interval",                bt_uint32,      "10"},
   {sc "chd/mon_ssd_disk_bw/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/mon_ssd_disk_bw/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/mon_ssd_disk_bw/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/mon_ssd_disk_bw/alarm_partial",                bt_bool,        "false"},
   {sc "chd/mon_ssd_disk_bw/calc_partial",                 bt_bool,        "false"},
   {sc "chd/mon_ssd_disk_bw/function",                     bt_string,      "min"},

   {sc "alarm/ssd_disk_bw",                            bt_string,      "ssd_disk_bw"},
   {sc "alarm/ssd_disk_bw/enable",                     bt_bool,        "false"},
   {sc "alarm/ssd_disk_bw/disable_allowed",            bt_bool,        "true"},
   {sc "alarm/ssd_disk_bw/ignore_first_value",         bt_bool,        "true"},
   {sc "alarm/ssd_disk_bw/trigger/type",               bt_string,      "chd"},
   {sc "alarm/ssd_disk_bw/trigger/id",                 bt_string,      "mon_ssd_disk_bw"},
   {sc "alarm/ssd_disk_bw/trigger/node_pattern",       bt_name,        "/nkn/monitor/ssd/disk/*/disk_bw"},
   {sc "alarm/ssd_disk_bw/rising/enable",              bt_bool,        "true"},
   {sc "alarm/ssd_disk_bw/falling/enable",             bt_bool,        "false"},
   {sc "alarm/ssd_disk_bw/rising/error_threshold",     bt_uint32,      "200000000"},
   {sc "alarm/ssd_disk_bw/rising/clear_threshold",     bt_uint32,      "100000000"},
   {sc "alarm/ssd_disk_bw/falling/error_threshold",    bt_uint32,      "0"},
   {sc "alarm/ssd_disk_bw/falling/clear_threshold",    bt_uint32,      "0"},
   {sc "alarm/ssd_disk_bw/rising/event_on_clear",      bt_bool,        "true"},
   {sc "alarm/ssd_disk_bw/falling/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/ssd_disk_bw/event_name_root",            bt_string,      "nkn_disk_bw"},
   {sc "alarm/ssd_disk_bw/clear_if_missing",           bt_bool,        "true"},
   {sc "alarm/ssd_disk_bw/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/ssd_disk_bw/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/ssd_disk_bw/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/ssd_disk_bw/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/ssd_disk_bw/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/ssd_disk_bw/short/rate_limit_win",       bt_duration_sec,"3600"},

   {sc "sample/root_disk_read",                               bt_string,      "root_disk_read"},
   {sc "sample/root_disk_read/enable",                         bt_bool,        "true"},
   {sc "sample/root_disk_read/node/"
   "\\/nkn\\/monitor\\/root\\/disk\\/*\\/read",     		   bt_name,        "/nkn/monitor/root/disk/*/read"},
   {sc "sample/root_disk_read/interval",                       bt_duration_sec,"10"},
   {sc "sample/root_disk_read/num_to_keep",                    bt_uint32,      "120"},
   {sc "sample/root_disk_read/num_to_cache",                   bt_int32,       "-1"},
   {sc "sample/root_disk_read/sample_method",                  bt_string,      "delta"},
   {sc "sample/root_disk_read/delta_constraint",               bt_string,      "increasing"},
   {sc "sample/root_disk_read/sync_interval",                  bt_uint32,      "10"},
   {sc "sample/root_disk_read/delta_average",                  bt_bool,        "false"},
   {"/stat/nkn/stats/root_disk_read/label",                    bt_string,      "Bytes read from Root Disk"},
   {"/stat/nkn/stats/root_disk_read/unit",                     bt_string,      "Bytes"},

  {sc "chd/root_disk_bw",                              bt_string,      "root_disk_bw"},
   {"/stat/nkn/stats/root_disk_bw/label",               bt_string,      "Disk Bandwidth of Root Disk Partitions"},
   {"/stat/nkn/stats/root_disk_bw/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/root_disk_bw/enable",                       bt_bool,        "true"},
   {sc "chd/root_disk_bw/source/type",                  bt_string,      "sample"},
   {sc "chd/root_disk_bw/source/id",                    bt_string,      "root_disk_read"},
   {sc "chd/root_disk_bw/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/root_disk_bw/select_type",                  bt_string,      "instances"},
   {sc "chd/root_disk_bw/instances/num_to_use",         bt_uint32,      "1"},
   {sc "chd/root_disk_bw/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/root_disk_bw/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/root_disk_bw/sync_interval",                bt_uint32,      "10"},
   {sc "chd/root_disk_bw/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/root_disk_bw/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/root_disk_bw/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/root_disk_bw/alarm_partial",                bt_bool,        "false"},
   {sc "chd/root_disk_bw/calc_partial",                 bt_bool,        "false"},
   {sc "chd/root_disk_bw/function",                     bt_string,      "nkn_disk_bw"},

  {sc "chd/mon_root_disk_bw",                              bt_string,      "mon_root_disk_bw"},
   {"/stat/nkn/stats/mon_root_disk_bw/label",               bt_string,      "Monitor Disk Bandwidth of Root Disk Partitions"},
   {"/stat/nkn/stats/mon_root_disk_bw/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/mon_root_disk_bw/enable",                       bt_bool,        "true"},
   {sc "chd/mon_root_disk_bw/source/type",                  bt_string,      "chd"},
   {sc "chd/mon_root_disk_bw/source/id",                    bt_string,      "root_disk_bw"},
   {sc "chd/mon_root_disk_bw/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/mon_root_disk_bw/select_type",                  bt_string,      "instances"},
   {sc "chd/mon_root_disk_bw/instances/num_to_use",         bt_uint32,      "3"},
   {sc "chd/mon_root_disk_bw/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/mon_root_disk_bw/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/mon_root_disk_bw/sync_interval",                bt_uint32,      "10"},
   {sc "chd/mon_root_disk_bw/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/mon_root_disk_bw/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/mon_root_disk_bw/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/mon_root_disk_bw/alarm_partial",                bt_bool,        "false"},
   {sc "chd/mon_root_disk_bw/calc_partial",                 bt_bool,        "false"},
   {sc "chd/mon_root_disk_bw/function",                     bt_string,      "min"},

   {sc "alarm/root_disk_bw",                            bt_string,      "root_disk_bw"},
   {sc "alarm/root_disk_bw/enable",                     bt_bool,        "false"},
   {sc "alarm/root_disk_bw/disable_allowed",            bt_bool,        "true"},
   {sc "alarm/root_disk_bw/ignore_first_value",         bt_bool,        "true"},
   {sc "alarm/root_disk_bw/trigger/type",               bt_string,      "chd"},
   {sc "alarm/root_disk_bw/trigger/id",                 bt_string,      "mon_root_disk_bw"},
   {sc "alarm/root_disk_bw/trigger/node_pattern",       bt_name,        "/nkn/monitor/root/disk/*/disk_bw"},
   {sc "alarm/root_disk_bw/rising/enable",              bt_bool,        "true"},
   {sc "alarm/root_disk_bw/falling/enable",             bt_bool,        "false"},
   {sc "alarm/root_disk_bw/rising/error_threshold",     bt_uint32,      "200000000"},
   {sc "alarm/root_disk_bw/rising/clear_threshold",     bt_uint32,      "100000000"},
   {sc "alarm/root_disk_bw/falling/error_threshold",    bt_uint32,      "0"},
   {sc "alarm/root_disk_bw/falling/clear_threshold",    bt_uint32,      "0"},
   {sc "alarm/root_disk_bw/rising/event_on_clear",      bt_bool,        "true"},
   {sc "alarm/root_disk_bw/falling/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/root_disk_bw/event_name_root",            bt_string,      "nkn_disk_bw"},
   {sc "alarm/root_disk_bw/clear_if_missing",           bt_bool,        "true"},
   {sc "alarm/root_disk_bw/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/root_disk_bw/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/root_disk_bw/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/root_disk_bw/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/root_disk_bw/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/root_disk_bw/short/rate_limit_win",       bt_duration_sec,"3600"},

   {sc "sample/sas_disk_io",                                bt_string,      "sas_disk_io"},
   {sc "sample/sas_disk_io/enable",                         bt_bool,        "true"},
   {sc "sample/sas_disk_io/node/"
   "\\/nkn\\/monitor\\/sas\\/disk\\/*\\/read",    	    bt_name,        "/nkn/monitor/sas/disk/*/read"},
   {sc "sample/sas_disk_io/node/"
   "\\/nkn\\/monitor\\/sas\\/disk\\/*\\/write",     	    bt_name,        "/nkn/monitor/sas/disk/*/write"},
   {sc "sample/sas_disk_io/interval",                       bt_duration_sec,"10"},
   {sc "sample/sas_disk_io/num_to_keep",                    bt_uint32,      "120"},
   {sc "sample/sas_disk_io/num_to_cache",                   bt_int32,       "-1"},
   {sc "sample/sas_disk_io/sample_method",                  bt_string,      "delta"},
   {sc "sample/sas_disk_io/delta_constraint",               bt_string,      "increasing"},
   {sc "sample/sas_disk_io/sync_interval",                  bt_uint32,      "10"},
   {sc "sample/sas_disk_io/delta_average",                  bt_bool,        "false"},
   {"/stat/nkn/stats/sas_disk_io/label",                    bt_string,      "Bytes written to SAS Disks"},
   {"/stat/nkn/stats/sas_disk_io/unit",                     bt_string,      "Bytes"},

   {sc "chd/sas_disk_io",                              bt_string,      "sas_disk_io"},
   {"/stat/nkn/stats/sas_disk_io/label",               bt_string,      "Disk I/O of SAS Disks"},
   {"/stat/nkn/stats/sas_disk_io/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/sas_disk_io/enable",                       bt_bool,        "true"},
   {sc "chd/sas_disk_io/source/type",                  bt_string,      "sample"},
   {sc "chd/sas_disk_io/source/id",                    bt_string,      "sas_disk_io"},
   {sc "chd/sas_disk_io/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/sas_disk_io/select_type",                  bt_string,      "instances"},
   {sc "chd/sas_disk_io/instances/num_to_use",         bt_uint32,      "1"},
   {sc "chd/sas_disk_io/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/sas_disk_io/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/sas_disk_io/sync_interval",                bt_uint32,      "10"},
   {sc "chd/sas_disk_io/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/sas_disk_io/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/sas_disk_io/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/sas_disk_io/alarm_partial",                bt_bool,        "false"},
   {sc "chd/sas_disk_io/calc_partial",                 bt_bool,        "false"},
   {sc "chd/sas_disk_io/function",                     bt_string,      "nkn_disk_io"},

  {sc "chd/sas_disk_io_ave",                              bt_string,      "sas_disk_io_ave"},
   {"/stat/nkn/stats/sas_disk_io_ave/label",               bt_string,      "Average Disk I/O of SAS Disks"},
   {"/stat/nkn/stats/sas_disk_io_ave/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/sas_disk_io_ave/enable",                       bt_bool,        "true"},
   {sc "chd/sas_disk_io_ave/source/type",                  bt_string,      "chd"},
   {sc "chd/sas_disk_io_ave/source/id",                    bt_string,      "sas_disk_io"},
   {sc "chd/sas_disk_io_ave/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/sas_disk_io_ave/select_type",                  bt_string,      "instances"},
   {sc "chd/sas_disk_io_ave/instances/num_to_use",         bt_uint32,      "6"},
   {sc "chd/sas_disk_io_ave/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/sas_disk_io_ave/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/sas_disk_io_ave/sync_interval",                bt_uint32,      "10"},
   {sc "chd/sas_disk_io_ave/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/sas_disk_io_ave/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/sas_disk_io_ave/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/sas_disk_io_ave/alarm_partial",                bt_bool,        "false"},
   {sc "chd/sas_disk_io_ave/calc_partial",                 bt_bool,        "false"},
   {sc "chd/sas_disk_io_ave/function",                     bt_string,      "mean"},

   {sc "alarm/sas_disk_io",                            bt_string,      "sas_disk_io"},
   {sc "alarm/sas_disk_io/enable",                     bt_bool,        "false"},
   {sc "alarm/sas_disk_io/disable_allowed",            bt_bool,        "true"},
   {sc "alarm/sas_disk_io/ignore_first_value",         bt_bool,        "true"},
   {sc "alarm/sas_disk_io/trigger/type",               bt_string,      "chd"},
   {sc "alarm/sas_disk_io/trigger/id",                 bt_string,      "sas_disk_io_ave"},
   {sc "alarm/sas_disk_io/trigger/node_pattern",       bt_name,        "/nkn/monitor/sas/disk/*/disk_io"},
   {sc "alarm/sas_disk_io/rising/enable",              bt_bool,        "true"},
   {sc "alarm/sas_disk_io/falling/enable",             bt_bool,        "false"},
   {sc "alarm/sas_disk_io/rising/error_threshold",     bt_uint32,      "5120000"},
   {sc "alarm/sas_disk_io/rising/clear_threshold",     bt_uint32,      "4608000"},
   {sc "alarm/sas_disk_io/falling/error_threshold",    bt_uint32,      "0"},
   {sc "alarm/sas_disk_io/falling/clear_threshold",    bt_uint32,      "0"},
   {sc "alarm/sas_disk_io/rising/event_on_clear",      bt_bool,        "true"},
   {sc "alarm/sas_disk_io/falling/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/sas_disk_io/event_name_root",            bt_string,      "nkn_disk_io"},
   {sc "alarm/sas_disk_io/clear_if_missing",           bt_bool,        "true"},
   {sc "alarm/sas_disk_io/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/sas_disk_io/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/sas_disk_io/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/sas_disk_io/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/sas_disk_io/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/sas_disk_io/short/rate_limit_win",       bt_duration_sec,"3600"},


   {sc "sample/sata_disk_io",                               bt_string,      "sata_disk_io"},
   {sc "sample/sata_disk_io/enable",                         bt_bool,        "true"},
   {sc "sample/sata_disk_io/node/"
   "\\/nkn\\/monitor\\/sata\\/disk\\/*\\/read",     		   bt_name,        "/nkn/monitor/sata/disk/*/read"},
   {sc "sample/sata_disk_io/node/"
   "\\/nkn\\/monitor\\/sata\\/disk\\/*\\/write",     		   bt_name,        "/nkn/monitor/sata/disk/*/write"},
   {sc "sample/sata_disk_io/interval",                       bt_duration_sec,"10"},
   {sc "sample/sata_disk_io/num_to_keep",                    bt_uint32,      "120"},
   {sc "sample/sata_disk_io/num_to_cache",                   bt_int32,       "-1"},
   {sc "sample/sata_disk_io/sample_method",                  bt_string,      "delta"},
   {sc "sample/sata_disk_io/delta_constraint",               bt_string,      "increasing"},
   {sc "sample/sata_disk_io/sync_interval",                  bt_uint32,      "10"},
   {sc "sample/sata_disk_io/delta_average",                  bt_bool,        "false"},
   {"/stat/nkn/stats/sata_disk_io/label",                    bt_string,      "Bytes written to SATA Disks"},
   {"/stat/nkn/stats/sata_disk_io/unit",                     bt_string,      "Bytes"},

   {sc "chd/sata_disk_io",                              bt_string,      "sata_disk_io"},
   {"/stat/nkn/stats/sata_disk_io/label",               bt_string,      "Disk I/O of SATA Disks"},
   {"/stat/nkn/stats/sata_disk_io/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/sata_disk_io/enable",                       bt_bool,        "true"},
   {sc "chd/sata_disk_io/source/type",                  bt_string,      "sample"},
   {sc "chd/sata_disk_io/source/id",                    bt_string,      "sata_disk_io"},
   {sc "chd/sata_disk_io/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/sata_disk_io/select_type",                  bt_string,      "instances"},
   {sc "chd/sata_disk_io/instances/num_to_use",         bt_uint32,      "1"},
   {sc "chd/sata_disk_io/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/sata_disk_io/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/sata_disk_io/sync_interval",                bt_uint32,      "10"},
   {sc "chd/sata_disk_io/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/sata_disk_io/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/sata_disk_io/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/sata_disk_io/alarm_partial",                bt_bool,        "false"},
   {sc "chd/sata_disk_io/calc_partial",                 bt_bool,        "false"},
   {sc "chd/sata_disk_io/function",                     bt_string,      "nkn_disk_io"},

  {sc "chd/sata_disk_io_ave",                              bt_string,      "sata_disk_io_ave"},
   {"/stat/nkn/stats/sata_disk_io_ave/label",               bt_string,      "Average Disk I/O of SATA Disks"},
   {"/stat/nkn/stats/sata_disk_io_ave/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/sata_disk_io_ave/enable",                       bt_bool,        "true"},
   {sc "chd/sata_disk_io_ave/source/type",                  bt_string,      "chd"},
   {sc "chd/sata_disk_io_ave/source/id",                    bt_string,      "sata_disk_io"},
   {sc "chd/sata_disk_io_ave/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/sata_disk_io_ave/select_type",                  bt_string,      "instances"},
   {sc "chd/sata_disk_io_ave/instances/num_to_use",         bt_uint32,      "6"},
   {sc "chd/sata_disk_io_ave/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/sata_disk_io_ave/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/sata_disk_io_ave/sync_interval",                bt_uint32,      "10"},
   {sc "chd/sata_disk_io_ave/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/sata_disk_io_ave/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/sata_disk_io_ave/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/sata_disk_io_ave/alarm_partial",                bt_bool,        "false"},
   {sc "chd/sata_disk_io_ave/calc_partial",                 bt_bool,        "false"},
   {sc "chd/sata_disk_io_ave/function",                     bt_string,      "mean"},

   {sc "alarm/sata_disk_io",                            bt_string,      "sata_disk_io"},
   {sc "alarm/sata_disk_io/enable",                     bt_bool,        "false"},
   {sc "alarm/sata_disk_io/disable_allowed",            bt_bool,        "true"},
   {sc "alarm/sata_disk_io/ignore_first_value",         bt_bool,        "true"},
   {sc "alarm/sata_disk_io/trigger/type",               bt_string,      "chd"},
   {sc "alarm/sata_disk_io/trigger/id",                 bt_string,      "sata_disk_io_ave"},
   {sc "alarm/sata_disk_io/trigger/node_pattern",       bt_name,        "/nkn/monitor/sata/disk/*/disk_io"},
   {sc "alarm/sata_disk_io/rising/enable",              bt_bool,        "true"},
   {sc "alarm/sata_disk_io/falling/enable",             bt_bool,        "false"},
   {sc "alarm/sata_disk_io/rising/error_threshold",     bt_uint32,      "5120000"},
   {sc "alarm/sata_disk_io/rising/clear_threshold",     bt_uint32,      "4608000"},
   {sc "alarm/sata_disk_io/falling/error_threshold",    bt_uint32,      "0"},
   {sc "alarm/sata_disk_io/falling/clear_threshold",    bt_uint32,      "0"},
   {sc "alarm/sata_disk_io/rising/event_on_clear",      bt_bool,        "true"},
   {sc "alarm/sata_disk_io/falling/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/sata_disk_io/event_name_root",            bt_string,      "nkn_disk_io"},
   {sc "alarm/sata_disk_io/clear_if_missing",           bt_bool,        "true"},
   {sc "alarm/sata_disk_io/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/sata_disk_io/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/sata_disk_io/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/sata_disk_io/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/sata_disk_io/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/sata_disk_io/short/rate_limit_win",       bt_duration_sec,"3600"},

   {sc "sample/ssd_disk_io",                               bt_string,      "ssd_disk_io"},
   {sc "sample/ssd_disk_io/enable",                         bt_bool,        "true"},
   {sc "sample/ssd_disk_io/node/"
   "\\/nkn\\/monitor\\/ssd\\/disk\\/*\\/read",     		   bt_name,        "/nkn/monitor/ssd/disk/*/read"},
   {sc "sample/ssd_disk_io/node/"
   "\\/nkn\\/monitor\\/ssd\\/disk\\/*\\/write",     		   bt_name,        "/nkn/monitor/ssd/disk/*/write"},
   {sc "sample/ssd_disk_io/interval",                       bt_duration_sec,"10"},
   {sc "sample/ssd_disk_io/num_to_keep",                    bt_uint32,      "120"},
   {sc "sample/ssd_disk_io/num_to_cache",                   bt_int32,       "-1"},
   {sc "sample/ssd_disk_io/sample_method",                  bt_string,      "delta"},
   {sc "sample/ssd_disk_io/delta_constraint",               bt_string,      "increasing"},
   {sc "sample/ssd_disk_io/sync_interval",                  bt_uint32,      "10"},
   {sc "sample/ssd_disk_io/delta_average",                  bt_bool,        "false"},
   {"/stat/nkn/stats/ssd_disk_io/label",                    bt_string,      "Bytes written to SSD Disks"},
   {"/stat/nkn/stats/ssd_disk_io/unit",                     bt_string,      "Bytes"},

   {sc "chd/ssd_disk_io",                              bt_string,      "ssd_disk_io"},
   {"/stat/nkn/stats/ssd_disk_io/label",               bt_string,      "Disk I/O of SSD Disks"},
   {"/stat/nkn/stats/ssd_disk_io/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/ssd_disk_io/enable",                       bt_bool,        "true"},
   {sc "chd/ssd_disk_io/source/type",                  bt_string,      "sample"},
   {sc "chd/ssd_disk_io/source/id",                    bt_string,      "ssd_disk_io"},
   {sc "chd/ssd_disk_io/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/ssd_disk_io/select_type",                  bt_string,      "instances"},
   {sc "chd/ssd_disk_io/instances/num_to_use",         bt_uint32,      "1"},
   {sc "chd/ssd_disk_io/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/ssd_disk_io/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/ssd_disk_io/sync_interval",                bt_uint32,      "10"},
   {sc "chd/ssd_disk_io/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/ssd_disk_io/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/ssd_disk_io/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/ssd_disk_io/alarm_partial",                bt_bool,        "false"},
   {sc "chd/ssd_disk_io/calc_partial",                 bt_bool,        "false"},
   {sc "chd/ssd_disk_io/function",                     bt_string,      "nkn_disk_io"},

  {sc "chd/ssd_disk_io_ave",                              bt_string,      "ssd_disk_io_ave"},
   {"/stat/nkn/stats/ssd_disk_io_ave/label",               bt_string,      "Average Disk I/O of SSD Disks"},
   {"/stat/nkn/stats/ssd_disk_io_ave/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/ssd_disk_io_ave/enable",                       bt_bool,        "true"},
   {sc "chd/ssd_disk_io_ave/source/type",                  bt_string,      "chd"},
   {sc "chd/ssd_disk_io_ave/source/id",                    bt_string,      "ssd_disk_io"},
   {sc "chd/ssd_disk_io_ave/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/ssd_disk_io_ave/select_type",                  bt_string,      "instances"},
   {sc "chd/ssd_disk_io_ave/instances/num_to_use",         bt_uint32,      "6"},
   {sc "chd/ssd_disk_io_ave/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/ssd_disk_io_ave/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/ssd_disk_io_ave/sync_interval",                bt_uint32,      "10"},
   {sc "chd/ssd_disk_io_ave/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/ssd_disk_io_ave/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/ssd_disk_io_ave/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/ssd_disk_io_ave/alarm_partial",                bt_bool,        "false"},
   {sc "chd/ssd_disk_io_ave/calc_partial",                 bt_bool,        "false"},
   {sc "chd/ssd_disk_io_ave/function",                     bt_string,      "mean"},

   {sc "alarm/ssd_disk_io",                            bt_string,      "ssd_disk_io"},
   {sc "alarm/ssd_disk_io/enable",                     bt_bool,        "false"},
   {sc "alarm/ssd_disk_io/disable_allowed",            bt_bool,        "true"},
   {sc "alarm/ssd_disk_io/ignore_first_value",         bt_bool,        "true"},
   {sc "alarm/ssd_disk_io/trigger/type",               bt_string,      "chd"},
   {sc "alarm/ssd_disk_io/trigger/id",                 bt_string,      "ssd_disk_io_ave"},
   {sc "alarm/ssd_disk_io/trigger/node_pattern",       bt_name,        "/nkn/monitor/ssd/disk/*/disk_io"},
   {sc "alarm/ssd_disk_io/rising/enable",              bt_bool,        "true"},
   {sc "alarm/ssd_disk_io/falling/enable",             bt_bool,        "false"},
   {sc "alarm/ssd_disk_io/rising/error_threshold",     bt_uint32,      "5120000"},
   {sc "alarm/ssd_disk_io/rising/clear_threshold",     bt_uint32,      "4608000"},
   {sc "alarm/ssd_disk_io/falling/error_threshold",    bt_uint32,      "0"},
   {sc "alarm/ssd_disk_io/falling/clear_threshold",    bt_uint32,      "0"},
   {sc "alarm/ssd_disk_io/rising/event_on_clear",      bt_bool,        "true"},
   {sc "alarm/ssd_disk_io/falling/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/ssd_disk_io/event_name_root",            bt_string,      "nkn_disk_io"},
   {sc "alarm/ssd_disk_io/clear_if_missing",           bt_bool,        "true"},
   {sc "alarm/ssd_disk_io/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/ssd_disk_io/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/ssd_disk_io/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/ssd_disk_io/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/ssd_disk_io/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/ssd_disk_io/short/rate_limit_win",       bt_duration_sec,"3600"},


   {sc "sample/root_disk_io",                               bt_string,      "root_disk_io"},
   {sc "sample/root_disk_io/enable",                         bt_bool,        "true"},
   {sc "sample/root_disk_io/node/"
   "\\/nkn\\/monitor\\/root\\/disk\\/*\\/read",     		   bt_name,        "/nkn/monitor/root/disk/*/read"},
   {sc "sample/root_disk_io/node/"
   "\\/nkn\\/monitor\\/root\\/disk\\/*\\/write",     		   bt_name,        "/nkn/monitor/root/disk/*/write"},
   {sc "sample/root_disk_io/interval",                       bt_duration_sec,"10"},
   {sc "sample/root_disk_io/num_to_keep",                    bt_uint32,      "120"},
   {sc "sample/root_disk_io/num_to_cache",                   bt_int32,       "-1"},
   {sc "sample/root_disk_io/sample_method",                  bt_string,      "delta"},
   {sc "sample/root_disk_io/delta_constraint",               bt_string,      "increasing"},
   {sc "sample/root_disk_io/sync_interval",                  bt_uint32,      "10"},
   {sc "sample/root_disk_io/delta_average",                  bt_bool,        "false"},
   {"/stat/nkn/stats/root_disk_io/label",                    bt_string,      "Bytes written to Root Disk"},
   {"/stat/nkn/stats/root_disk_io/unit",                     bt_string,      "Bytes"},

   {sc "chd/root_disk_io",                              bt_string,      "root_disk_io"},
   {"/stat/nkn/stats/root_disk_io/label",               bt_string,      "Disk I/O of Root Disk Partitions"},
   {"/stat/nkn/stats/root_disk_io/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/root_disk_io/enable",                       bt_bool,        "true"},
   {sc "chd/root_disk_io/source/type",                  bt_string,      "sample"},
   {sc "chd/root_disk_io/source/id",                    bt_string,      "root_disk_io"},
   {sc "chd/root_disk_io/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/root_disk_io/select_type",                  bt_string,      "instances"},
   {sc "chd/root_disk_io/instances/num_to_use",         bt_uint32,      "1"},
   {sc "chd/root_disk_io/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/root_disk_io/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/root_disk_io/sync_interval",                bt_uint32,      "10"},
   {sc "chd/root_disk_io/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/root_disk_io/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/root_disk_io/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/root_disk_io/alarm_partial",                bt_bool,        "false"},
   {sc "chd/root_disk_io/calc_partial",                 bt_bool,        "false"},
   {sc "chd/root_disk_io/function",                     bt_string,      "nkn_disk_io"},

  {sc "chd/root_disk_io_ave",                              bt_string,      "root_disk_io_ave"},
   {"/stat/nkn/stats/root_disk_io_ave/label",               bt_string,      "Average Disk I/O of Root Disk Partitions"},
   {"/stat/nkn/stats/root_disk_io_ave/unit",                bt_string,      "Bytes/sec"},
   {sc "chd/root_disk_io_ave/enable",                       bt_bool,        "true"},
   {sc "chd/root_disk_io_ave/source/type",                  bt_string,      "chd"},
   {sc "chd/root_disk_io_ave/source/id",                    bt_string,      "root_disk_io"},
   {sc "chd/root_disk_io_ave/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/root_disk_io_ave/select_type",                  bt_string,      "instances"},
   {sc "chd/root_disk_io_ave/instances/num_to_use",         bt_uint32,      "6"},
   {sc "chd/root_disk_io_ave/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/root_disk_io_ave/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/root_disk_io_ave/sync_interval",                bt_uint32,      "10"},
   {sc "chd/root_disk_io_ave/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/root_disk_io_ave/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/root_disk_io_ave/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/root_disk_io_ave/alarm_partial",                bt_bool,        "false"},
   {sc "chd/root_disk_io_ave/calc_partial",                 bt_bool,        "false"},
   {sc "chd/root_disk_io_ave/function",                     bt_string,      "mean"},

   {sc "alarm/root_disk_io",                            bt_string,      "root_disk_io"},
   {sc "alarm/root_disk_io/enable",                     bt_bool,        "false"},
   {sc "alarm/root_disk_io/disable_allowed",            bt_bool,        "true"},
   {sc "alarm/root_disk_io/ignore_first_value",         bt_bool,        "true"},
   {sc "alarm/root_disk_io/trigger/type",               bt_string,      "chd"},
   {sc "alarm/root_disk_io/trigger/id",                 bt_string,      "root_disk_io_ave"},
   {sc "alarm/root_disk_io/trigger/node_pattern",       bt_name,        "/nkn/monitor/root/disk/*/disk_io"},
   {sc "alarm/root_disk_io/rising/enable",              bt_bool,        "true"},
   {sc "alarm/root_disk_io/falling/enable",             bt_bool,        "false"},
   {sc "alarm/root_disk_io/rising/error_threshold",     bt_uint32,      "5120000"},
   {sc "alarm/root_disk_io/rising/clear_threshold",     bt_uint32,      "4608000"},
   {sc "alarm/root_disk_io/falling/error_threshold",    bt_uint32,      "0"},
   {sc "alarm/root_disk_io/falling/clear_threshold",    bt_uint32,      "0"},
   {sc "alarm/root_disk_io/rising/event_on_clear",      bt_bool,        "true"},
   {sc "alarm/root_disk_io/falling/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/root_disk_io/event_name_root",            bt_string,      "nkn_disk_io"},
   {sc "alarm/root_disk_io/clear_if_missing",           bt_bool,        "true"},
   {sc "alarm/root_disk_io/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/root_disk_io/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/root_disk_io/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/root_disk_io/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/root_disk_io/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/root_disk_io/short/rate_limit_win",       bt_duration_sec,"3600"},

   {sc "sample/appl_cpu_util",                                bt_string,      "appl_cpu_util"},
   {sc "sample/appl_cpu_util/enable",                         bt_bool,        "true"},
   {sc "sample/appl_cpu_util/node/"
   "\\/nkn\\/monitor\\/lfd\\/appmaxutil",                     bt_name,        "/nkn/monitor/lfd/appmaxutil"},
   {sc "sample/appl_cpu_util/interval",                       bt_duration_sec,"10"},
   {sc "sample/appl_cpu_util/num_to_keep",                    bt_uint32,      "120"},
   {sc "sample/appl_cpu_util/num_to_cache",                   bt_int32,       "-1"},
   {sc "sample/appl_cpu_util/sample_method",                  bt_string,      "direct"},
   {sc "sample/appl_cpu_util/delta_constraint",               bt_string,      "none"},
   {sc "sample/appl_cpu_util/sync_interval",                  bt_uint32,      "10"},
   {sc "sample/appl_cpu_util/delta_average",                  bt_bool,        "false"},
   {"/stat/nkn/stats/appl_cpu_util/label",                    bt_string,      "Application CPU Utilization"},
   {"/stat/nkn/stats/appl_cpu_util/unit",                     bt_string,      "Percent"},

   {sc "chd/appl_cpu_util",                              bt_string,      "appl_cpu_util"},
   {"/stat/nkn/stats/appl_cpu_util/label",               bt_string,      "1 min Average of Application CPU Utilization"},
   {"/stat/nkn/stats/appl_cpu_util/unit",                bt_string,      "Percent"},
   {sc "chd/appl_cpu_util/enable",                       bt_bool,        "true"},
   {sc "chd/appl_cpu_util/source/type",                  bt_string,      "sample"},
   {sc "chd/appl_cpu_util/source/id",                    bt_string,      "appl_cpu_util"},
   {sc "chd/appl_cpu_util/num_to_keep",                  bt_uint32,      "128"},
   {sc "chd/appl_cpu_util/select_type",                  bt_string,      "instances"},
   {sc "chd/appl_cpu_util/instances/num_to_use",         bt_uint32,      "6"},
   {sc "chd/appl_cpu_util/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/appl_cpu_util/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/appl_cpu_util/sync_interval",                bt_uint32,      "10"},
   {sc "chd/appl_cpu_util/time/interval_distance",       bt_duration_sec,"0"},
   {sc "chd/appl_cpu_util/time/interval_length",         bt_duration_sec,"0"},
   {sc "chd/appl_cpu_util/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/appl_cpu_util/alarm_partial",                bt_bool,        "false"},
   {sc "chd/appl_cpu_util/calc_partial",                 bt_bool,        "false"},
   {sc "chd/appl_cpu_util/function",                     bt_string,      "mean"},

   {sc "alarm/appl_cpu_util",                           bt_string,      "appl_cpu_util"},
   {sc "alarm/appl_cpu_util/enable",                    bt_bool,        "false"},
   {sc "alarm/appl_cpu_util/disable_allowed",           bt_bool,        "true"},
   {sc "alarm/appl_cpu_util/ignore_first_value",        bt_bool,        "true"},
   {sc "alarm/appl_cpu_util/trigger/type",              bt_string,      "chd"},
   {sc "alarm/appl_cpu_util/trigger/id",                bt_string,      "appl_cpu_util"},
   {sc "alarm/appl_cpu_util/trigger/node_pattern",      bt_name,        "/nkn/monitor/lfd/appmaxutil"},
   {sc "alarm/appl_cpu_util/rising/enable",             bt_bool,        "true"},
   {sc "alarm/appl_cpu_util/falling/enable",            bt_bool,        "false"},
   {sc "alarm/appl_cpu_util/rising/error_threshold",    bt_uint32,      "90"},
   {sc "alarm/appl_cpu_util/rising/clear_threshold",    bt_uint32,      "70"},
   {sc "alarm/appl_cpu_util/falling/error_threshold",   bt_uint32,      "0"},
   {sc "alarm/appl_cpu_util/falling/clear_threshold",   bt_uint32,      "0"},
   {sc "alarm/appl_cpu_util/rising/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/appl_cpu_util/falling/event_on_clear",    bt_bool,        "true"},
   {sc "alarm/appl_cpu_util/event_name_root",           bt_string,      "appl_cpu_util"},
   {sc "alarm/appl_cpu_util/clear_if_missing",          bt_bool,        "true"},
   {sc "alarm/appl_cpu_util/long/rate_limit_max",       bt_uint32,      "50"},
   {sc "alarm/appl_cpu_util/long/rate_limit_win",       bt_duration_sec,"604800"},
   {sc "alarm/appl_cpu_util/medium/rate_limit_max",     bt_uint32,      "20"},
   {sc "alarm/appl_cpu_util/medium/rate_limit_win",     bt_duration_sec,"86400"},
   {sc "alarm/appl_cpu_util/short/rate_limit_max",      bt_uint32,      "5"},
   {sc "alarm/appl_cpu_util/short/rate_limit_win",      bt_duration_sec,"3600"},

};

static const bn_str_value md_stats_upgrade_adds_19_to_20[] = {
   {sc "sample/cache_hit",                               bt_string,      "cache_hit"},
   {sc "sample/cache_hit/enable",                        bt_bool,        "true"},
   {sc "sample/cache_hit/node/"
    "\\/stat\\/nkn\\/nvsd\\/tot_cl_send",           bt_name,        "/stat/nkn/nvsd/tot_cl_send"},
   {sc "sample/cache_hit/node/"
   "\\/stat\\/nkn\\/nvsd\\/tot_cl_send_bm_dm",	 	 bt_name,	 	 "/stat/nkn/nvsd/tot_cl_send_bm_dm"},
   {sc "sample/cache_hit/interval",                      bt_duration_sec,"60"},
   {sc "sample/cache_hit/num_to_keep",                   bt_uint32,      "1440"},
   {sc "sample/cache_hit/num_to_cache",                  bt_int32,       "-1"},
   {sc "sample/cache_hit/sample_method",                 bt_string,      "delta"},
   {sc "sample/cache_hit/delta_constraint",              bt_string,      "increasing"},
   {sc "sample/cache_hit/sync_interval",                 bt_uint32,      "10"},
   {sc "sample/cache_hit/delta_average",                 bt_bool,      	 "false"},
   {"/stat/nkn/stats/cache_hit/label",                   bt_string,      "Data delivered from cache and total data delivered"},
   {"/stat/nkn/stats/cache_hit/unit",                    bt_string,      "Bytes"},

   {sc "chd/cache_hit_ratio",                              bt_string,      "cache_hit_ratio"},
   {"/stat/nkn/stats/cache_hit_ratio/label",               bt_string,      "Cache Hit Ratio"},
   {"/stat/nkn/stats/cache_hit_ratio/unit",                bt_string,      "Percent"},
   {sc "chd/cache_hit_ratio/enable",                       bt_bool,        "true"},
   {sc "chd/cache_hit_ratio/source/type",                  bt_string,      "sample"},
   {sc "chd/cache_hit_ratio/source/id",                    bt_string,      "cache_hit"},
   {sc "chd/cache_hit_ratio/num_to_keep",                  bt_uint32,      "1440"},
   {sc "chd/cache_hit_ratio/select_type",                  bt_string,      "time"},
   {sc "chd/cache_hit_ratio/instances/num_to_use",         bt_uint32,      "1"},
   {sc "chd/cache_hit_ratio/instances/num_to_advance",     bt_uint32,      "1"},
   {sc "chd/cache_hit_ratio/num_to_cache",                 bt_int32,       "-1"},
   {sc "chd/cache_hit_ratio/sync_interval",                bt_uint32,      "10"},
   {sc "chd/cache_hit_ratio/time/interval_distance",       bt_duration_sec,"60"},
   {sc "chd/cache_hit_ratio/time/interval_length",         bt_duration_sec,"86400"},
   {sc "chd/cache_hit_ratio/time/interval_phase",          bt_duration_sec,"0"},
   {sc "chd/cache_hit_ratio/alarm_partial",                bt_bool,        "false"},
   {sc "chd/cache_hit_ratio/calc_partial",                 bt_bool,        "false"},
   {sc "chd/cache_hit_ratio/function",                     bt_string,      "cache_hit_ratio"},

   {sc "alarm/cache_hit_ratio",                            bt_string,      "cache_hit_ratio"},
   {sc "alarm/cache_hit_ratio/enable",                     bt_bool,        "false"},
   {sc "alarm/cache_hit_ratio/disable_allowed",            bt_bool,        "true"},
   {sc "alarm/cache_hit_ratio/ignore_first_value",         bt_bool,        "true"},
   {sc "alarm/cache_hit_ratio/trigger/type",               bt_string,      "chd"},
   {sc "alarm/cache_hit_ratio/trigger/id",                 bt_string,      "cache_hit_ratio"},
   {sc "alarm/cache_hit_ratio/trigger/node_pattern",       bt_name,        "/stat/nkn/nvsd/cache_hit_ratio"},
   {sc "alarm/cache_hit_ratio/rising/enable",              bt_bool,        "false"},
   {sc "alarm/cache_hit_ratio/falling/enable",             bt_bool,        "true"},
   {sc "alarm/cache_hit_ratio/rising/error_threshold",     bt_uint32,      "0"},
   {sc "alarm/cache_hit_ratio/rising/clear_threshold",     bt_uint32,      "0"},
   {sc "alarm/cache_hit_ratio/falling/error_threshold",    bt_uint32,      "5"},
   {sc "alarm/cache_hit_ratio/falling/clear_threshold",    bt_uint32,      "20"},
   {sc "alarm/cache_hit_ratio/rising/event_on_clear",      bt_bool,        "true"},
   {sc "alarm/cache_hit_ratio/falling/event_on_clear",     bt_bool,        "true"},
   {sc "alarm/cache_hit_ratio/event_name_root",            bt_string,      "cache_hit_ratio"},
   {sc "alarm/cache_hit_ratio/clear_if_missing",           bt_bool,        "true"},
   {sc "alarm/cache_hit_ratio/long/rate_limit_max",        bt_uint32,      "50"},
   {sc "alarm/cache_hit_ratio/long/rate_limit_win",        bt_duration_sec,"604800"},
   {sc "alarm/cache_hit_ratio/medium/rate_limit_max",      bt_uint32,      "20"},
   {sc "alarm/cache_hit_ratio/medium/rate_limit_win",      bt_duration_sec,"86400"},
   {sc "alarm/cache_hit_ratio/short/rate_limit_max",       bt_uint32,      "5"},
   {sc "alarm/cache_hit_ratio/short/rate_limit_win",       bt_duration_sec,"3600"},

};

static const bn_str_value md_stats_upgrade_adds_20_to_21[] = {
   {sc "alarm/aggr_cpu_util/enable",              	bt_bool,     	"true"},
   {sc "alarm/appl_cpu_util/enable",                    bt_bool,        "true"},
   {sc "alarm/cache_byte_rate/enable",                    bt_bool,        "true"},
   {sc "alarm/cache_hit_ratio/enable",                     bt_bool,        "true"},
   {sc "alarm/connection_rate/enable",                     bt_bool,        "true"},
   {sc "alarm/http_transaction_rate/enable",                     bt_bool,        "true"},
   {sc "alarm/intf_util/enable",                     bt_bool,        "true"},
   {sc "alarm/nkn_mem_util/enable",                   	bt_bool,        "true"},
   {sc "alarm/origin_byte_rate/enable",                   	bt_bool,        "true"},
   {sc "alarm/root_disk_bw/enable",                     bt_bool,        "true"},
   {sc "alarm/root_disk_io/enable",                     bt_bool,        "true"},
   {sc "alarm/root_disk_space/enable",                     bt_bool,        "true"},
   {sc "alarm/sas_disk_bw/enable",                     bt_bool,        "true"},
   {sc "alarm/sas_disk_io/enable",                     bt_bool,        "true"},
   {sc "alarm/sas_disk_space/enable",                     bt_bool,        "true"},
   {sc "alarm/sata_disk_bw/enable",                     bt_bool,        "true"},
   {sc "alarm/sata_disk_io/enable",                     bt_bool,        "true"},
   {sc "alarm/sata_disk_space/enable",                     bt_bool,        "true"},
   {sc "alarm/ssd_disk_bw/enable",                     bt_bool,        "true"},
   {sc "alarm/ssd_disk_io/enable",                     bt_bool,        "true"},
   {sc "alarm/ssd_disk_space/enable",                     bt_bool,        "true"},

};

static const bn_str_value md_stats_upgrade_adds_22_to_23[] = {
   {sc "sample/filter_acp_rate",                               bt_string,      "filter_acp_rate"},
   {sc "sample/filter_acp_rate/enable",                        bt_bool,        "true"},
   {sc "sample/filter_acp_rate/node/"
   "\\/stat\\/nkn\\/namespace\\/*\\/url_filter_acp_cnt",     bt_name,        "/stat/nkn/namespace/*/url_filter_acp_cnt"},
   {sc "sample/filter_acp_rate/interval",                      bt_duration_sec,"5"},
   {sc "sample/filter_acp_rate/num_to_keep",                   bt_uint32,      "10"},
   {sc "sample/filter_acp_rate/num_to_cache",                  bt_int32,       "-1"},
   {sc "sample/filter_acp_rate/sample_method",                 bt_string,      "delta"},
   {sc "sample/filter_acp_rate/delta_constraint",              bt_string,      "none"},
   {sc "sample/filter_acp_rate/sync_interval",                 bt_uint32,      "5"},
   {sc "sample/filter_acp_rate/delta_average",                 bt_bool,        "false"},
   {"/stat/nkn/stats/filter_acp_rate/label",                   bt_string,      "Filter accept rate"},
   {"/stat/nkn/stats/filter_acp_rate/unit",                    bt_string,      "Accept rate per second"},
   {sc "sample/filter_rej_rate",                               bt_string,      "filter_rej_rate"},
   {sc "sample/filter_rej_rate/enable",                        bt_bool,        "true"},
   {sc "sample/filter_rej_rate/node/"
   "\\/stat\\/nkn\\/namespace\\/*\\/url_filter_rej_cnt",     bt_name,        "/stat/nkn/namespace/*/url_filter_rej_cnt"},
   {sc "sample/filter_rej_rate/interval",                      bt_duration_sec,"5"},
   {sc "sample/filter_rej_rate/num_to_keep",                   bt_uint32,      "10"},
   {sc "sample/filter_rej_rate/num_to_cache",                  bt_int32,       "-1"},
   {sc "sample/filter_rej_rate/sample_method",                 bt_string,      "delta"},
   {sc "sample/filter_rej_rate/delta_constraint",              bt_string,      "none"},
   {sc "sample/filter_rej_rate/sync_interval",                 bt_uint32,      "5"},
   {sc "sample/filter_rej_rate/delta_average",                 bt_bool,        "false"},
   {"/stat/nkn/stats/filter_rej_rate/label",                   bt_string,      "Filter reject rate"},
   {"/stat/nkn/stats/filter_rej_rate/unit",                    bt_string,      "Reject rate per second"},
};

/*
 * following array adds ns.ns1.http.client.conns as http.client.conns and
 * a monitoring node as /stat/nkn/namespace/%s/http.client.conns
 */

#define NS_COUNTERS(x) x,"ns.%s."x
md_ns_counter_t md_ns_counters[] = {
    { NS_COUNTERS("http.client.conns"), bt_uint64  },
    { NS_COUNTERS("http.client.active_conns"), bt_uint64},
    { NS_COUNTERS("http.client.idle_conns"), bt_uint64},
    { NS_COUNTERS("http.client.resp_404"), bt_uint64},
    { NS_COUNTERS("http.client.resp_302"),bt_uint64 },
    { NS_COUNTERS("http.client.resp_304"), bt_uint64},
    { NS_COUNTERS("http.client.resp_206"),bt_uint64 },
    { NS_COUNTERS("http.client.resp_200"),bt_uint64 },
    { NS_COUNTERS("http.server.conns"), bt_uint32 },
    { NS_COUNTERS("http.server.active_conns"), bt_uint32 },
    { NS_COUNTERS("http.server.idle_conns"), bt_uint32 },
    { NS_COUNTERS("rtsp.client.cache_partial"), bt_uint64 },
    { NS_COUNTERS("rtsp.client.conns"), bt_uint32 },
    { NS_COUNTERS("rtsp.client.active_conns"), bt_uint32 },
    { NS_COUNTERS("rtsp.client.idle_conns"), bt_uint32 },
    { NS_COUNTERS("rtsp.server.conns"), bt_uint32 },
    { NS_COUNTERS("rtsp.server.active_conns"), bt_uint32 },
    { NS_COUNTERS("rtsp.server.idle_conns"), bt_uint32 },
    { NS_COUNTERS("rtsp.server.resp_2xx"), bt_uint64 },
    { NS_COUNTERS("rtsp.server.resp_3xx"), bt_uint64 },
    { NS_COUNTERS("rtsp.server.resp_4xx"), bt_uint64 },
    { NS_COUNTERS("rtsp.server.resp_5xx"), bt_uint64 },
    { NULL, NULL, bt_uint64 }
};

/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
md_nkn_stats_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    node_name_t ns_counter_node = {0};
    temp_buf_t get_arg_value = {0};
    char node_name[64];
    char var_name[64];
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;
    char *t_temp = NULL;


    /*
     * Commit order of 200 is greater than the 0 used by most modules,
     * including md_pm.c.  This is to ensure that we can override some
     * of PM's global configuration, such as liveness grace period.
     *
     * We need modrf_namespace_unrestricted to allow us to set nodes
     * from our initial values function that are not underneath our
     * module root.
     */
    err = mdm_add_module(
	    "counters",                         // module_name
	    24,                                  // module_version
	    sp,                                 // root_node_name
	    NULL,                               // root_config_name
	    modrf_namespace_unrestricted,	// md_mod_flags
	    NULL,                               // side_effects_func
	    NULL,                               // side_effects_arg
	    NULL,                               // check_func
	    NULL,                               // check_arg
	    NULL,                               // apply_func
	    NULL,                               // apply_arg
	    200,                                // commit_order
	    0,                                  // apply_order
	    md_cntr_upgrade_downgrade,          // updown_func
	    &md_cntr_upgrade_rules,             // updown_arg
	    md_nkn_stats_add_initial,           // initial_func
	    NULL,                               // initial_arg
	    NULL,                               // mon_get_func
	    NULL,                               // mon_get_arg
	    NULL,                               // mon_iterate_func
	    NULL,                               // mon_iterate_arg
	    &module);                           // ret_module
    bail_error(err);

    err = md_upgrade_rule_array_new(&md_cntr_upgrade_rules);
    bail_error(err);

    ra = md_cntr_upgrade_rules;


#ifdef PROD_FEATURE_I18N_SUPPORT
    err = mdm_module_set_gettext_domain(module, GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type =     MUTT_MODIFY;
    rule->mur_name =             "/stats/config/sample/proc_mem/enable";
    rule->mur_new_value_type =   bt_bool;
    rule->mur_new_value_str =    "false";
    MD_ADD_RULE(ra);

    err = libnkncnt_init();
    /* Lazy initialization on error */


    uint32 i = 0;
    for(i = 0; i < (sizeof(md_stats_init)/sizeof(md_nkn_stats_t)); ++i) {
	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name =   md_stats_init[i].node;
	node->mrn_value_type = md_stats_init[i].type;
	node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
	node->mrn_cap_mask = mcf_cap_node_basic;

	if (md_stats_init[i].cb) {
	    node->mrn_mon_get_func = md_stats_init[i].cb;
	} else {
	    switch(md_stats_init[i].type)
	    {
		case bt_uint16:
		    node->mrn_mon_get_func = md_nkn_stats_get_uint16;
		    break;
		case bt_uint32:
		    node->mrn_mon_get_func = md_nkn_stats_get_uint32;
		    break;
		case bt_uint64:
		    node->mrn_mon_get_func = md_nkn_stats_get_uint64;
		    break;
		default:
		    node->mrn_mon_get_func = md_stats_init[i].cb;
		    break;
	    }
	}
	node->mrn_mon_get_arg  = (void*) md_stats_init[i].variable;
	node->mrn_description = "";
	err = mdm_add_node(module, &node);
	bail_error(err);
    }


    for(i = 0; i < (sizeof(md_bkup_stats_init)/sizeof(md_nkn_bkup_stats_t)); ++i) {
	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name =   md_bkup_stats_init[i].node;
	node->mrn_value_type = md_bkup_stats_init[i].type;
	node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
	node->mrn_cap_mask = mcf_cap_node_basic;


	switch(md_bkup_stats_init[i].type)
	{
	    case bt_uint32:
		node->mrn_mon_get_func = md_nkn_stats_get_diff_uint32;
		break;
	    case bt_uint64:
		node->mrn_mon_get_func = md_nkn_stats_get_diff_uint64;
		break;
	    default:
		node->mrn_mon_get_func = md_bkup_stats_init[i].cb;
		break;
	}
	node->mrn_mon_get_arg  = (void*)(&md_bkup_stats_init[i].args);
	node->mrn_description = "";
	err = mdm_add_node(module, &node);
	bail_error(err);
    }

    /*Adding monitoring nodes for SNMP sake for 2XX, 3XX, 4XX , 5XX counters*/
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/http/client/2xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_http_client_response;
    node->mrn_mon_get_arg =        (void *)"2";
    node->mrn_description =        "Get 2xx http client response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/http/client/3xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_http_client_response;
    node->mrn_mon_get_arg =        (void *)"3";
    node->mrn_description =        "Get 3xx http client response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/http/client/4xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_http_client_response;
    node->mrn_mon_get_arg =        (void *)"4";
    node->mrn_description =        "Get 4xx http client response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/http/client/5xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_http_client_response;
    node->mrn_mon_get_arg =        (void *)"5";
    node->mrn_description =        "Get 5xx http client response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/http/server/3xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_http_server_response;
    node->mrn_mon_get_arg =        (void *)"3";
    node->mrn_description =        "Get 3xx http server response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/http/server/4xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_http_server_response;
    node->mrn_mon_get_arg =        (void *)"4";
    node->mrn_description =        "Get 4xx http server response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/http/server/5xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_http_server_response;
    node->mrn_mon_get_arg =        (void *)"5";
    node->mrn_description =        "Get 5xx http server response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/rtsp/client/2xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_rtsp_client_response;
    node->mrn_mon_get_arg =        (void *)"2";
    node->mrn_description =        "Get 2xx rtsp client response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/rtsp/client/3xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_rtsp_client_response;
    node->mrn_mon_get_arg =        (void *)"3";
    node->mrn_description =        "Get 3xx rtsp client response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/rtsp/client/4xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_rtsp_client_response;
    node->mrn_mon_get_arg =        (void *)"4";
    node->mrn_description =        "Get 4xx rtsp client response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/rtsp/client/5xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_rtsp_client_response;
    node->mrn_mon_get_arg =        (void *)"5";
    node->mrn_description =        "Get 5xx rtsp client response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/rtsp/server/2xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_rtsp_server_response;
    node->mrn_mon_get_arg =        (void *)"2";
    node->mrn_description =        "Get 2xx rtsp server response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/rtsp/server/3xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_rtsp_server_response;
    node->mrn_mon_get_arg =        (void *)"3";
    node->mrn_description =        "Get 3xx rtsp server response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/rtsp/server/4xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_rtsp_server_response;
    node->mrn_mon_get_arg =        (void *)"4";
    node->mrn_description =        "Get 4xx rtsp server response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/rtsp/server/5xx";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_rtsp_server_response;
    node->mrn_mon_get_arg =        (void *)"5";
    node->mrn_description =        "Get 5xx rtsp server response";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/http/svr_conn";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_http_server_conns;
    node->mrn_description =        "Get Http server connection";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/http/svr_open_conn";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_http_server_open_conns;
    node->mrn_description =        "Get Http active server connection";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/http/curr_open_http_socket";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_http_client_open_conns;
    node->mrn_description =        "Get Http active client  connection";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_get_symbol("md_stats", "md_stats_register_event",
	    (void **)&md_stats_register_event_func);
    bail_error_null(err, md_stats_register_event_func);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/nvsd/reset-counter/time";
    node->mrn_value_type =         bt_time;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =	   md_nvsd_get_reset_uptime;
    node->mrn_mon_get_arg =        (void *)"glob_nkn_reset_counter_time";
    node->mrn_description =        "Uptime since the reset-counter was issued";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/stat/nkn/port/*";
    node->mrn_value_type =        bt_string;
    node->mrn_node_reg_flags =    mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_portno;
    node->mrn_mon_iterate_func =  md_iterate_ports;
    node->mrn_description =       "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    snprintf(node_name, 64, "/stat/nkn/port/*/total_sessions");
    node->mrn_name = node_name;
    node->mrn_value_type = bt_uint32;
    node->mrn_mon_get_func = md_nkn_dy_stats_get_uint32;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"net_port_%s_tot_sessions";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/stat/nkn/disk/*";
    node->mrn_value_type =        bt_string;
    node->mrn_node_reg_flags =    mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_diskname;
    node->mrn_mon_iterate_func = md_iterate_diskname;
    node->mrn_description =       "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    snprintf(node_name, 64, "/stat/nkn/disk/*/block_size");
    node->mrn_name = node_name;
    node->mrn_value_type = bt_uint32;
    node->mrn_mon_get_func = md_nkn_dy_stats_get_uint32;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"%s.dm2_block_size";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    snprintf(node_name, 64, "/stat/nkn/disk/*/page_size");
    node->mrn_name = node_name;
    node->mrn_value_type = bt_uint32;
    node->mrn_mon_get_func = md_nkn_dy_stats_get_uint32;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"%s.dm2_page_size";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    snprintf(node_name, 64, "/stat/nkn/disk/*/free_blocks");
    node->mrn_name = node_name;
    node->mrn_value_type = bt_uint32;
    node->mrn_mon_get_func = md_nkn_dy_stats_get_uint32;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"%s.dm2_free_blocks";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    snprintf(node_name, 64, "/stat/nkn/disk/*/free_pages");
    node->mrn_name = node_name;
    node->mrn_value_type = bt_uint32;
    node->mrn_mon_get_func = md_nkn_dy_stats_get_uint32;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"%s.dm2_free_pages";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    snprintf(node_name, 64, "/stat/nkn/disk/*/total_blocks");
    node->mrn_name = node_name;
    node->mrn_value_type = bt_uint32;
    node->mrn_mon_get_func = md_nkn_dy_stats_get_uint32;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"%s.dm2_total_blocks";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    snprintf(node_name, 64, "/stat/nkn/disk/*/total_pages");
    node->mrn_name = node_name;
    node->mrn_value_type = bt_uint32;
    node->mrn_mon_get_func = md_nkn_dy_stats_get_uint32;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"%s.dm2_total_pages";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/tier/sas/ingested_objects";
    node->mrn_value_type = bt_uint64;
    node->mrn_mon_get_func = md_nkn_dy_stats_get_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"dm2.tier.SAS.ingest_objects";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/tier/sata/ingested_objects";
    node->mrn_value_type = bt_uint64;
    node->mrn_mon_get_func = md_nkn_dy_stats_get_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"dm2.tier.SATA.ingest_objects";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/tier/ssd/ingested_objects";
    node->mrn_value_type = bt_uint64;
    node->mrn_mon_get_func = md_nkn_dy_stats_get_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"dm2.tier.SSD.ingest_objects";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/tier/sas/deleted_objects";
    node->mrn_value_type = bt_uint64;
    node->mrn_mon_get_func = md_nkn_dy_stats_get_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"dm2.tier.SAS.delete_objects";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/tier/sata/deleted_objects";
    node->mrn_value_type = bt_uint64;
    node->mrn_mon_get_func = md_nkn_dy_stats_get_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"dm2.tier.SATA.delete_objects";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/stat/nkn/tier/ssd/deleted_objects";
    node->mrn_value_type = bt_uint64;
    node->mrn_mon_get_func = md_nkn_dy_stats_get_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"dm2.tier.SSD.delete_objects";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);


    /*! Namespace get counters */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/stat/nkn/namespace/*";
    node->mrn_value_type =        bt_string;
    node->mrn_node_reg_flags =    mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_namespace;
    node->mrn_mon_iterate_func =  md_iterate_namespace;
    node->mrn_description =       "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    snprintf(node_name, 64, "/stat/nkn/namespace/*/gets");
    node->mrn_name = node_name;
    node->mrn_value_type = bt_uint64;
    node->mrn_mon_get_func = md_nkn_ns_stats_get_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"glob_namespace_%s_gets";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/stat/nkn/namespace/*/status/active";
    node->mrn_value_type =        bt_bool;
    node->mrn_node_reg_flags =    mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_namespace_config_bool;
    node->mrn_mon_get_arg = (void *)"status/active";
    node->mrn_description =       "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/stat/nkn/namespace/*/uid";
    node->mrn_value_type =        bt_string;
    node->mrn_node_reg_flags =    mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_get_namespace_config_string;
    node->mrn_mon_get_arg = (void *)"uid";
    node->mrn_description =       "";
    err = mdm_add_node(module, &node);
    bail_error(err);

#include "md_nvsd_namespace_stats.inc.c"

    for ( i = 0; md_ns_counters[i].ns_counter != NULL; i++) {
	snprintf(ns_counter_node, sizeof(ns_counter_node),
		"/stat/nkn/namespace/*/%s", md_ns_counters[i].ns_counter);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name = ns_counter_node;
	node->mrn_value_type =  md_ns_counters[i].type;
	node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
	node->mrn_cap_mask = mcf_cap_node_basic;
	node->mrn_mon_get_arg = (void*)(md_ns_counters[i].get_arg);
	node->mrn_mon_get_func = md_nkn_wc_stats_get_uint64;
	node->mrn_description = "";
	err = mdm_add_node(module, &node);
	bail_error(err);
    }

    err = mdm_new_node(module, &node);
    bail_error(err);
    snprintf(node_name, 64, "/stat/nkn/nvsd/network/*/thread_counter");
    node->mrn_name = node_name;
    node->mrn_value_type = bt_uint32;
    node->mrn_mon_get_func = md_nkn_network_get_uint32;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_arg = (void *)"monitor_%s_network_thread";
    node->mrn_description = "";
    err = mdm_add_node(module,&node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/stat/nkn/stats/*";
    node->mrn_value_type =        bt_string;
    node->mrn_node_reg_flags =    mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_description =       "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/stat/nkn/stats/*/label";
    node->mrn_value_type =        bt_string;
    node->mrn_node_reg_flags =    mrf_flags_reg_config_literal;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_description =       "";
    node->mrn_initial_value =     " ";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/stat/nkn/stats/*/unit";
    node->mrn_value_type =        bt_string;
    node->mrn_node_reg_flags =    mrf_flags_reg_config_literal;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_description =       " ";
    node->mrn_initial_value =     " ";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Memory Utilization*/

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = 		    "/stat/nkn/system/memory/memutil_pct";
    node->mrn_value_type = 	    bt_uint64;
    node->mrn_node_reg_flags = 	    mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask =     	    mcf_cap_node_basic;
    node->mrn_extmon_provider_name =gcl_client_gmgmthd;
    node->mrn_description = 	    "Current Memory Utilization Percent";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*--------------------------------------------------------------
     * APPEARED IN MODULE VERSION 3
     *  Track Per Process VM stats
     *------------------------------------------------------------*/
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/vmem/*";
    node->mrn_value_type =          bt_string;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_get_proc_name;
    node->mrn_mon_iterate_func =    md_iterate_proc_name;
    node->mrn_description =         "Nodes to hold per process VM stats";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/vmem/*/peak";
    node->mrn_value_type =          bt_uint32;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_get_proc_mem;
    node->mrn_mon_get_arg =         (void *) "VmPeak";
    node->mrn_description =         "VMPeak from /proc/<pid>/status";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/vmem/*/size";
    node->mrn_value_type =          bt_uint32;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_get_proc_mem;
    node->mrn_mon_get_arg =         (void *) "VmSize";
    node->mrn_description =         "VmSize from /proc/<pid>/status";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/vmem/*/lock";
    node->mrn_value_type =          bt_uint32;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_get_proc_mem;
    node->mrn_mon_get_arg =         (void *) "VmLck";
    node->mrn_description =         "VmLck from /proc/<pid>/status";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/vmem/*/data";
    node->mrn_value_type =          bt_uint32;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_get_proc_mem;
    node->mrn_mon_get_arg =         (void *) "VmData";
    node->mrn_description =         "VmData from /proc/<pid>/status";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/vmem/*/resident";
    node->mrn_value_type =          bt_uint32;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_get_proc_mem;
    node->mrn_mon_get_arg =         (void *) "VmRSS";
    node->mrn_description =         "VmRSS from /proc/<pid>/status";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/interface/bw/total";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_get_total_bw;
    node->mrn_mon_get_arg =         (void *) NULL;
    node->mrn_description =         "Total BW stats";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/interface/bw/service_intf_bw";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_get_total_bw_service_intf;
    node->mrn_mon_get_arg =         (void *) "txrx";
    node->mrn_description       =   "Aggregated service-interface stats";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/interface/bw/service_intf_bw_tx";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_get_total_bw_service_intf;
    node->mrn_mon_get_arg =         (void *) "tx";
    node->mrn_description       =   "Aggregated service-interface stats";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/stat/nkn/interface/bw/service_intf_bw_rx";
    node->mrn_value_type =          bt_uint64;
    node->mrn_node_reg_flags =      mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_mon_get_func =        md_get_total_bw_service_intf;
    node->mrn_mon_get_arg =         (void *) "rx";
    node->mrn_description       =   "Aggregated service-interface stats";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*! Need to fix this event name to match with alarm registration */
    str_value_t nkn_events[] = { "connection_rate",
	"cache_byte_rate",
	"origin_byte_rate",
	"disk_byte_rate",
	"http_transaction_rate",
	"aggr_cpu_util",
	"nkn_mem_util",
	"nkn_disk_space",
	"nkn_disk_bw",
	"nkn_disk_io",
	"appl_cpu_util",
	"cache_hit_ratio"
    };

    for ( i = 0 ; i < sizeof(nkn_events)/sizeof(str_value_t) ; i++)
    {
	err = (*md_stats_register_event_func)
	    (module, nkn_events[i], mse_rising_all | mse_falling_all);
	bail_error(err);
    }
    err = (*md_stats_register_event_func)
	(module, "resource_pool_event", mse_rising_all | mse_falling_all);
    bail_error(err);


    /* Action for reset counter, to take backup value*/
    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name =		 "/stat/nkn/actions/reset-counter";
    node->mrn_node_reg_flags =    mrf_flags_reg_action;
    node->mrn_cap_mask =	  mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func = md_commit_forward_action;
    node->mrn_action_arg        = &md_stats_fwd_args;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name =             "/stat/nkn/actions/reset-http-counter";
    node->mrn_node_reg_flags =    mrf_flags_reg_action;
    node->mrn_cap_mask =          mcf_cap_action_restricted;
    node->mrn_action_async_nonbarrier_start_func = md_commit_forward_action;
    node->mrn_action_arg        = &md_stats_fwd_args;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name =             "/stat/nkn/actions/reset-https-counter";
    node->mrn_node_reg_flags =    mrf_flags_reg_action;
    node->mrn_cap_mask =          mcf_cap_action_restricted;
    node->mrn_action_async_nonbarrier_start_func = md_commit_forward_action;
    node->mrn_action_arg        = &md_ssld_fwd_args;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name =             "/stat/nkn/actions/reset-namespace-counter";
    node->mrn_node_reg_flags =    mrf_flags_reg_action;
    node->mrn_cap_mask =          mcf_cap_action_restricted;
    node->mrn_action_async_nonbarrier_start_func = md_commit_forward_action;
    node->mrn_action_arg        = &md_stats_fwd_args;
    node->mrn_actions[0]->mra_param_name = "namespace_name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);

#ifdef PROD_FEATURE_GRAPHING
    /* Graph action */
    err = mdm_new_action(module, &node, 4);
    bail_error(err);
    node->mrn_name =               "/stat/nkn/actions/bandwidth";
    node->mrn_node_reg_flags =     mrf_flags_reg_action;
    node->mrn_cap_mask =           mcf_cap_action_basic;
    node->mrn_action_func =        md_graph_bandwidth;
    node->mrn_action_log_handle =  false;
    node->mrn_actions[0]->mra_param_name = "bgcolor";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "gif_name";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_actions[2]->mra_param_name = "width";
    node->mrn_actions[2]->mra_param_type = bt_string;
    node->mrn_actions[3]->mra_param_name = "height";
    node->mrn_actions[3]->mra_param_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);

    i = 0;
    for(i = 0; i < (sizeof(md_nkn_stat_graphs)/sizeof(md_nkn_stat_graphs_t)); ++i) {
	err = mdm_new_action(module, &node, 8);
	bail_error(err);
	node->mrn_name =               md_nkn_stat_graphs[i].action_name;
	node->mrn_node_reg_flags =     mrf_flags_reg_action;
	node->mrn_cap_mask =           mcf_cap_action_basic;
	node->mrn_action_func =        md_nkn_stat_graphs[i].function_name;
	node->mrn_action_log_handle =  false;
	node->mrn_actions[0]->mra_param_name = "bgcolor";
	node->mrn_actions[0]->mra_param_type = bt_string;
	node->mrn_actions[1]->mra_param_name = "gif_name";
	node->mrn_actions[1]->mra_param_type = bt_string;
	node->mrn_actions[2]->mra_param_name = "filename";
	node->mrn_actions[2]->mra_param_type = bt_string;
	node->mrn_actions[3]->mra_param_name = "width";
	node->mrn_actions[3]->mra_param_type = bt_string;
	node->mrn_actions[4]->mra_param_name = "height";
	node->mrn_actions[4]->mra_param_type = bt_string;
	node->mrn_actions[5]->mra_param_name = "image_width";
	node->mrn_actions[5]->mra_param_type = bt_uint32;
	node->mrn_actions[6]->mra_param_name = "image_height";
	node->mrn_actions[6]->mra_param_type = bt_uint32;
	node->mrn_actions[7]->mra_param_name = "graphver";
	node->mrn_actions[7]->mra_param_type = bt_uint32;
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_value_type = bt_string;
	node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
	node->mrn_cap_mask = mcf_cap_node_basic;

	switch(md_nkn_stat_graphs[i].mode)
	{
	    case HOURLY:
		t_temp =   smprintf("%s/graph/%s", sp,md_nkn_stat_graphs[i].filename);
		node->mrn_name = t_temp;
		node->mrn_check_node_func = md_nkn_stats_graph_gen;
		break;
	    case DAILY:
		t_temp =   smprintf("%s/graph/%s", sp,md_nkn_stat_graphs[i].filename);
		node->mrn_name = t_temp;
		node->mrn_check_node_func = md_nkn_stats_graph_gen;
		break;
	    case WEEKLY:
		node->mrn_check_node_func = md_nkn_stats_graph_gen;
		t_temp =   smprintf("%s/graph/%s", sp,md_nkn_stat_graphs[i].filename);
		node->mrn_name = t_temp;
		break;
	    case MONTHLY:
		node->mrn_check_node_func = md_nkn_stats_graph_gen;
		t_temp =   smprintf("%s/graph/%s", sp,md_nkn_stat_graphs[i].filename);
		node->mrn_name = t_temp;
		break;
	    default:
		break;
	}
	node->mrn_initial_value  = (void*) md_nkn_stat_graphs[i].action_name;
	node->mrn_description = "";
	err = mdm_add_node(module, &node);
	safe_free(t_temp);
	bail_error(err);
    }

#endif /* PROD_FEATURE_GRAPHING */
    /*! To Get number of CPU cores in the system */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/stat/nkn/cpucores";
    node->mrn_value_type =        bt_uint32;
    node->mrn_node_reg_flags =    mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_system_get_cpucores;
    node->mrn_description =       "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = md_upgrade_add_add_rules(ra, md_stats_upgrade_adds_16_to_17,
	    sizeof(md_stats_upgrade_adds_16_to_17) / sizeof(bn_str_value), 16, 17);
    bail_error(err);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =     MUTT_MODIFY;
    rule->mur_name =             "/stats/config/alarm/intf_util/trigger/id";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "mon_intf_util";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =     MUTT_MODIFY;
    rule->mur_name =             "/stats/config/alarm/paging/trigger/type";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "chd";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =     MUTT_MODIFY;
    rule->mur_name =             "/stats/config/alarm/paging/trigger/id";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "mon_paging";
    MD_ADD_RULE(ra);

    err = md_upgrade_add_add_rules(ra, md_stats_upgrade_adds_17_to_18,
	    sizeof(md_stats_upgrade_adds_17_to_18) / sizeof(bn_str_value), 17, 18);
    bail_error(err);

    err = md_upgrade_add_add_rules(ra, md_stats_upgrade_adds_18_to_19,
	    sizeof(md_stats_upgrade_adds_18_to_19) / sizeof(bn_str_value), 18, 19);
    bail_error(err);

    /* Changing the event name from "disk_space" to "nkn_disk_space"
       as "disk_space" is already registered by samara */


    MD_NEW_RULE(ra, 19, 20);
    rule->mur_upgrade_type =     MUTT_MODIFY;
    rule->mur_name =             "/stats/config/alarm/sas_disk_space/event_name_root";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "nkn_disk_space";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 19, 20);
    rule->mur_upgrade_type =     MUTT_MODIFY;
    rule->mur_name =             "/stats/config/alarm/sata_disk_space/event_name_root";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "nkn_disk_space";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 19, 20);
    rule->mur_upgrade_type =     MUTT_MODIFY;
    rule->mur_name =             "/stats/config/alarm/ssd_disk_space/event_name_root";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "nkn_disk_space";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 19, 20);
    rule->mur_upgrade_type =     MUTT_MODIFY;
    rule->mur_name =             "/stats/config/alarm/root_disk_space/event_name_root";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "nkn_disk_space";
    MD_ADD_RULE(ra);

    err = md_upgrade_add_add_rules(ra, md_stats_upgrade_adds_19_to_20,
	    sizeof(md_stats_upgrade_adds_19_to_20) / sizeof(bn_str_value), 19, 20);
    bail_error(err);

    for ( i = 0 ; i < sizeof(md_stats_upgrade_adds_20_to_21) / sizeof(bn_str_value); i++)
    {
	const bn_str_value *val = NULL;
	val = &md_stats_upgrade_adds_20_to_21[i];
	MD_NEW_RULE(ra, 20, 21);
	rule->mur_upgrade_type =     MUTT_MODIFY;
	rule->mur_name =             val->bv_name;
	rule->mur_new_value_type =   val->bv_type;
	rule->mur_new_value_str =    val->bv_value;
	MD_ADD_RULE(ra);
    }

    MD_NEW_RULE(ra, 21, 22);
    rule->mur_upgrade_type =     MUTT_MODIFY;
    rule->mur_name =             "/stats/config/chd/mon_sata_disk_bw/function";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "max";
    MD_ADD_RULE(ra);

    for ( i = 0 ; i < sizeof(md_stats_upgrade_adds_22_to_23) / sizeof(bn_str_value); i++)
    {
	const bn_str_value *val = NULL;
	val = &md_stats_upgrade_adds_22_to_23[i];
	MD_NEW_RULE(ra, 22, 23);
	rule->mur_upgrade_type =     MUTT_MODIFY;
	rule->mur_name =             val->bv_name;
	rule->mur_new_value_type =   val->bv_type;
	rule->mur_new_value_str =    val->bv_value;
	MD_ADD_RULE(ra);
    }

    /*
     * adding following rules again to ensure the values are changed
     * from older releases
     * Changing the event name from "disk_space" to "nkn_disk_space"
     * as "disk_space" is already registered by samara
     */

    MD_NEW_RULE(ra, 23, 24);
    rule->mur_upgrade_type =     MUTT_MODIFY;
    rule->mur_name =             "/stats/config/alarm/sas_disk_space/event_name_root";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "nkn_disk_space";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 23, 24);
    rule->mur_upgrade_type =     MUTT_MODIFY;
    rule->mur_name =             "/stats/config/alarm/sata_disk_space/event_name_root";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "nkn_disk_space";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 23, 24);
    rule->mur_upgrade_type =     MUTT_MODIFY;
    rule->mur_name =             "/stats/config/alarm/ssd_disk_space/event_name_root";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "nkn_disk_space";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 23, 24);
    rule->mur_upgrade_type =     MUTT_MODIFY;
    rule->mur_name =             "/stats/config/alarm/root_disk_space/event_name_root";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "nkn_disk_space";
    MD_ADD_RULE(ra);

    md_nkn_stats_graph_gen_cron();

bail:
    return err;
}

static int
md_nkn_stats_get_uint64_in_mb(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;

    uint64 n = nkncnt_get_uint64((char*) arg);
    n = n /(1024 * 1024);
    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value, 0, n);
    bail_error(err);

bail:
    return err;
}

static int
md_nkn_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
        int err = 0;

        uint64 n = nkncnt_get_uint64((char*) arg);
        err = bn_binding_new_uint64(ret_binding,
                        node_name, ba_value, 0, n);
        bail_error(err);

bail:
        return err;
}

static int
md_nkn_stats_get_diff_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
        int err = 0;
	md_nkn_arg_list_t *argv;

	argv = (md_nkn_arg_list_t *)arg;

        uint64 n1 = nkncnt_get_uint64((char *) argv->variable1);
        uint64 n2 = nkncnt_get_uint64((char *) argv->variable2);
	uint64 n = n1 - n2;
        err = bn_binding_new_uint64(ret_binding,
                        node_name, ba_value, 0, n);
        bail_error(err);

bail:
        return err;
}

static int
md_nkn_stats_get_uint16(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
        int err = 0;

        uint32 n = nkncnt_get_uint16((char*) arg);
        err = bn_binding_new_uint16(ret_binding,
                        node_name, ba_value, 0, n);
        bail_error(err);

bail:
        return err;
}
static int
md_nkn_stats_get_uint32(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
        int err = 0;

        uint32 n = nkncnt_get_uint32((char*) arg);
        err = bn_binding_new_uint32(ret_binding,
                        node_name, ba_value, 0, n);
        bail_error(err);

bail:
        return err;
}
static int
md_nkn_stats_get_diff_uint32(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
        int err = 0;
	md_nkn_arg_list_t *argv;

	argv = (md_nkn_arg_list_t *)arg;

        uint32 n1 = nkncnt_get_uint32((char *) argv->variable1);
        uint32 n2 = nkncnt_get_uint32((char *) argv->variable2);
	uint32 n = n1 - n2;
        err = bn_binding_new_uint32(ret_binding,
                        node_name, ba_value, 0, n);
        bail_error(err);

bail:
        return err;
}

static int
md_nkn_dy_stats_get_uint32(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
        int err = 0;
        const char *var_name = (char *)arg;;
        tstring *number;
        char variable[64] ;

        err = bn_binding_name_to_parts_va(
		node_name, false, 1, 3, &number);
        bail_error_null(err,number);


        snprintf(variable, 64, var_name,ts_str(number));

        uint32 n = nkncnt_get_uint32((char*) variable);
        err = bn_binding_new_uint32(ret_binding,
                        node_name, ba_value, 0, n);
        bail_error(err);

bail:
	ts_free(&number);
        return err;
}
static int
md_nkn_dy_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
        int err = 0;
        const char *var_name = (char *)arg;;
        tstring *number;
        char variable[64] ;

        err = bn_binding_name_to_parts_va(
		node_name, false, 1, 3, &number);
        bail_error_null(err,number);


        snprintf(variable, 64, var_name,ts_str(number));

        uint64_t n = nkncnt_get_uint64((char*) variable);
        err = bn_binding_new_uint64(ret_binding,
                        node_name, ba_value, 0, n);
        bail_error(err);

bail:
	ts_free(&number);
        return err;
}
static int
md_nkn_network_get_uint32(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
	int err = 0;
	tstring *number;
	uint32_t n1 = 0;
	char *counter = NULL;

	err = bn_binding_name_to_parts_va(
			node_name, false, 1, 4, &number);
	bail_error_null(err,number);

	counter = smprintf("monitor_%s_network_thread", ts_str(number));
	bail_null(counter);

	n1 = nkncnt_get_uint64(counter);
	err = bn_binding_new_uint32(ret_binding,
			node_name, ba_value, 0, n1);
	bail_error(err);

bail:
	ts_free(&number);
	safe_free(counter);
	return err;
}

static int
md_nkn_ns_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
        int err = 0;
        const char *var_name = (char *)arg;;
        tstring *number;
        char variable[64] ;
        char variable1[64] ;

        err = bn_binding_name_to_parts_va(
		node_name, false, 1, 3, &number);
        bail_error_null(err,number);


        snprintf(variable, 64, var_name,ts_str(number));
        snprintf(variable1, 64, "glob_namespace_bkup_%s_gets",ts_str(number));

        uint64_t n = nkncnt_get_uint64((char*) variable);
        uint64_t n1 = nkncnt_get_uint64((char*) variable1);
	uint64_t n2 = n - n1;
        err = bn_binding_new_uint64(ret_binding,
                        node_name, ba_value, 0, n2);
        bail_error(err);

bail:
	ts_free(&number);
        return err;
}







/* ------------------------------------------------------------------------- */
static int
md_iterate_ports(md_commit *commit, const mdb_db *db,
                          const char *parent_node_name,
                          const uint32_array *node_attrib_ids,
                          int32 max_num_nodes, int32 start_child_num,
                          const char *get_next_child,
                          mdm_mon_iterate_names_cb_func iterate_cb,
                          void *iterate_cb_arg, void *arg)
{
    int err = 0;
    uint32 num_ports = 0, i = 0;
    const tstr_array *ports = NULL;
    char portnum[8] ;
    uint32 num_ifs = 0;
    const tstr_array *ifs = NULL;
    const char *ifname = NULL;
    uint32 avl_bw = 0;
    char variable_name[64];


    num_ports = nkncnt_get_uint16((char*)"glob_tot_interface");

    /*! Get all the available ports from /net/interface/state/ *
     *  Check whether it is used by NVSD with net_port_*_available_bw
     *  Create binding if available bandwidth is non-zero
     */
    err = md_interface_get_ifnames_cached(md_interface_ifnames_cache_ms, false,
                                    &ifs);
    bail_error_null(err, ifs);

    num_ifs = tstr_array_length_quick(ifs);
    for (i = 0; i < num_ifs; ++i) {
        ifname = tstr_array_get_str_quick(ifs, i);
        bail_null(ifname);
        snprintf(variable_name, 64, "net_port_%s_available_bw",
                                ifname);

        avl_bw = nkncnt_get_uint32((char*)variable_name);
        if (avl_bw != 0 )
        {
            err = (*iterate_cb)(commit, db, ifname, iterate_cb_arg);
            bail_error(err);
        }
    }

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_get_portno(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *portnum = NULL;

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    num_parts = tstr_array_length_quick(parts);
    bail_require(num_parts == 4); /* "/stat/nkn/port/<portnum>"  */
    portnum = tstr_array_get_str_quick(parts, 3);
    bail_null(portnum);

    /* XXX/EMT validate */

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
                             0, portnum);
    bail_error_null(err, *ret_binding);

 bail:
    tstr_array_free(&parts);
    return(err);
}
/*------------------------------------------------------------------------- */
static int
md_iterate_namespace(md_commit *commit, const mdb_db *db,
                          const char *parent_node_name,
                          const uint32_array *node_attrib_ids,
                          int32 max_num_nodes, int32 start_child_num,
                          const char *get_next_child,
                          mdm_mon_iterate_names_cb_func iterate_cb,
                          void *iterate_cb_arg, void *arg)
{
    int err = 0;
    uint32 i = 0, num_bindings = 0;
    bn_binding *binding = NULL;
    bn_binding_array *binding_arrays = NULL;
    char *name = NULL;
    tstr_array *names_array = NULL;
    const char *namespaceid_str = NULL;
    const tstring *namespaceid = NULL;

    err = mdb_iterate_binding(commit, db, "/nkn/nvsd/namespace",
                              0, &binding_arrays);
    bail_error(err);

    err = bn_binding_array_length(binding_arrays, &num_bindings);
    bail_error(err);

    for ( i = 0 ; i < num_bindings ; ++i) {
        err = bn_binding_array_get(binding_arrays, i, &binding);
        bail_error(err);

        safe_free(name);

        err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
                                 &name);

        err = (*iterate_cb)(commit, db, name, iterate_cb_arg);
        bail_error(err);
        safe_free(name);
    }


 bail:
    bn_binding_array_free(&binding_arrays);
    tstr_array_free(&names_array);
    return(err);
}
static int
md_get_namespace(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *namespace = NULL;

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    num_parts = tstr_array_length_quick(parts);
    bail_require(num_parts == 4); /* "/stat/nkn/namespace/<namespace>"  */
    namespace = tstr_array_get_str_quick(parts, 3);
    bail_null(namespace);

    /* XXX/EMT validate */

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
                             0, namespace);
    bail_error_null(err, *ret_binding);

 bail:
    tstr_array_free(&parts);
    return(err);
}

#if 1
/* ------------------------------------------------------------------------- */
static int
md_iterate_diskname(md_commit *commit, const mdb_db *db,
                          const char *parent_node_name,
                          const uint32_array *node_attrib_ids,
                          int32 max_num_nodes, int32 start_child_num,
                          const char *get_next_child,
                          mdm_mon_iterate_names_cb_func iterate_cb,
                          void *iterate_cb_arg, void *arg)
{
    int err = 0;
    uint32 i = 0, num_bindings = 0;
    bn_binding *binding = NULL;
    bn_binding_array *binding_arrays = NULL;
    char *name = NULL;
    tstr_array *names_array = NULL;
    uint32 num_disks = 0;
    const char *diskid_str = NULL;
    const tstring *diskid = NULL;

    err = mdb_iterate_binding(commit, db, "/nkn/nvsd/diskcache/config/disk_id",
                              0, &binding_arrays);
    bail_error(err);

    err = tstr_array_new(&names_array, NULL);
    bail_error(err);

    err = bn_binding_array_length(binding_arrays, &num_bindings);
    bail_error(err);

    for ( i = 0 ; i < num_bindings ; ++i) {
        err = bn_binding_array_get(binding_arrays, i, &binding);
        bail_error(err);

        safe_free(name);

        err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
                                 &name);

        err = tstr_array_append_str(names_array, name);
        bail_error(err);

        //bn_binding_free(&binding);
    }

    num_disks = tstr_array_length_quick(names_array);
    for (i = 0; i < num_disks; ++i) {
        diskid = tstr_array_get_quick(names_array, i);
        diskid_str = ts_str(diskid);
        bail_null(diskid_str);


        // Get diskname */
        err = mdb_get_node_binding_fmt(commit, db, 0 , &binding,
                            "/nkn/nvsd/diskcache/monitor/disk_id/%s/diskname",
                            diskid_str);
        bail_error(err);
        if (binding) {
            err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
                                     &name);
            bail_error(err);

            err = (*iterate_cb)(commit, db, name, iterate_cb_arg);
            bail_error(err);
            safe_free(name);
        }
    }
 bail:
    bn_binding_array_free(&binding_arrays);
    tstr_array_free(&names_array);
    return(err);
}
#endif /* 0 */

/* ------------------------------------------------------------------------- */
static int
md_get_diskname(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *diskname = NULL;

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    num_parts = tstr_array_length_quick(parts);
    bail_require(num_parts == 4); /* "/stat/nkn/disk/<diskname>"  */
    diskname = tstr_array_get_str_quick(parts, 3);
    bail_null(diskname);

    /* XXX/EMT validate */

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
                             0, diskname);
    bail_error_null(err, *ret_binding);

 bail:
    tstr_array_free(&parts);
    return(err);
}

static int
md_nkn_stats_graph_gen(md_commit *commit,
                             const mdb_db *old_db,
                             const mdb_db *new_db,
                             const mdb_db_change_array *change_list,
                             const tstring *node_name,
                             const tstr_array *node_name_parts,
                             mdb_db_change_type change_type,
                             const bn_attrib_array *old_attribs,
                             const bn_attrib_array *new_attribs,
                             void *arg)

{
     int err = 0;
     const char *minute = NULL, *hour = NULL, *day_of_month = NULL;
     const char *month = NULL, *day_of_week = NULL, *user = NULL;
     char *command = NULL;
     char *crontab = NULL;
     tstring *filename_real = NULL;
     char *filename_temp = NULL;
     int fd_temp = -1;
     const char *graph_name = NULL;
     const char *graph_type = NULL;
     tstr_array *parts = NULL;
     uint32 num_parts = 0;
     const char *user_cron_file = "user_cron";
     tstring *action_ts = NULL;

     const bn_attrib *old_value = NULL;
     const bn_attrib *new_value = NULL;
    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;

     if (old_attribs != NULL) {
         old_value = bn_attrib_array_get_quick(old_attribs, ba_value);
         if (old_value != NULL ) {
             goto bail;
         }
     }

     if (new_attribs != NULL ) {
         new_value = bn_attrib_array_get_quick(new_attribs, ba_value);
         if (new_value == NULL ) {
             goto bail;
         }
     }

     err = bn_attrib_get_tstr(new_value, NULL, bt_string, NULL, &action_ts);
     bail_error(err);

     // if old value is NULL , means new node is created
     err = bn_binding_name_to_parts(ts_str(node_name), &parts, NULL);
     bail_error_null(err, parts);

     num_parts = tstr_array_length_quick(parts);
     bail_require(num_parts == 5); /* "/stat/nkn/graph/hourly/"  */
     graph_name = tstr_array_get_str_quick(parts, 4);
     bail_null(graph_name);
     graph_type = tstr_array_get_str_quick(parts, 3);
     bail_null(graph_type);

     if ( !strcmp(graph_type, "hourly" ) ) {
         minute = "*/10"; /* Check every 10 minutes */
         hour = "*";
         day_of_month = "*";
         month = "*";
         day_of_week = "*";
     }
     else if ( !strcmp(graph_type, "daily" )) {
         minute = "0"; /* Check every 60 minutes */
         hour = "*";
         day_of_month = "*";
         month = "*";
         day_of_week = "*";
     }
     else if ( !strcmp(graph_type, "monthly" )) {
         minute = "0"; /* Check every 24 hours */
         hour = "0";
         day_of_month = "*";
         month = "*";
         day_of_week = "*";
     }
     else if ( !strcmp(graph_type, "weekly" )) {
         minute = "0"; /* Check every 24 hours */
         hour = "0";
         day_of_month = "*";
         month = "*";
         day_of_week = "*";
     }

     user = "admin"; /* Run as admin */
// /opt/tms/bin/mdreq action  "/tms/graphs/bw_hourly"  filename string monthly_image.png graphver uint32 2

     command = smprintf("%s %s %s %s %s %s/%s.png %s %s %s",
                         "/opt/tms/bin/mdreq", "action",
                         ts_str(action_ts), "filename", "string", graph_type,
                         graph_name, "graphver", "uint32" , "2" );
     bail_null(command);

    /*
     * Tell it not to try to send mail with the output of the script.
     */
    crontab = smprintf("MAILTO=\"\"\n"
                    "%s %s %s %s %s %s\n",
                       minute, hour, day_of_month, month, day_of_week,
                       command);
    bail_null(crontab);

    /* ------------------------------------------------------------------------
     * Get the temporary output file set up
     */
    err = lf_path_from_va_str(&filename_real, true, 2,
                              md_gen_output_path, user_cron_file);
    bail_error(err);

    err = lf_temp_file_get_fd(ts_str(filename_real),
                              &filename_temp, &fd_temp);
    bail_error(err);

    /*-----------------------------------------------------------------
     * Write the file
     */
    err = lf_write_bytes(fd_temp, crontab, strlen(crontab), NULL);
    bail_error(err);

    close(fd_temp);

    /*-----------------------------------------------------------------
     * Move the file into place
     */
    err = lf_temp_file_activate(ts_str(filename_real), filename_temp,
                                md_gen_conf_file_owner,
                                md_gen_conf_file_group,
                                md_gen_conf_file_mode, true);
    bail_error(err);
    /* Launch crontab -u user file
     */

    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);
#define PROG_CRONTAB_PATH   "/usr/bin/crontab"
    err = ts_new_str(&(lp->lp_path), PROG_CRONTAB_PATH);
    bail_error(err);

    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 4,
            PROG_CRONTAB_PATH, "-u", user, ts_str(filename_real));
    bail_error(err);

    lp->lp_wait = false;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_DEBUG;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

    lr = malloc(sizeof(*lr));
    bail_null(lr);
    memset(lr, 0, sizeof(*lr));

    err = lc_launch(lp, lr);
    bail_error(err);

bail:
     tstr_array_free(&parts);
     ts_free(&action_ts);
     ts_free(&filename_real);
     lc_launch_params_free(&lp);
     free(lr);
     safe_free(command);
     safe_free(crontab);
     safe_free(filename_temp);
     return err;
}
static int
md_nkn_stats_graph_gen_cron(void)
{
     int err = 0;
     const char *minute = NULL, *hour = NULL, *day_of_month = NULL;
     const char *month = NULL, *day_of_week = NULL, *user = NULL;
     char *command = NULL;
     char *crontab = NULL;
     char *crontab_report = NULL;
     const char *cur_report = NULL;
     const char *latest_report = NULL;
     const char *bk_report = NULL;
     tstring *filename_real = NULL;
     char *filename_temp = NULL;
     int fd_temp = -1;
     const char *graph_name = NULL;
     const char *user_cron_file = "user_cron";
     uint32  i = 0;
    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;

    /* ------------------------------------------------------------------------
     * Get the temporary output file set up
     */
    err = lf_path_from_va_str(&filename_real, true, 2,
                              md_gen_output_path, user_cron_file);
    bail_error(err);

    err = lf_temp_file_get_fd(ts_str(filename_real),
                              &filename_temp, &fd_temp);
    bail_error(err);

    /* -----------------------------------------------------------------------*/

     for ( i = 0 ; i < sizeof(md_nkn_stat_graphs)/sizeof(md_nkn_stat_graphs_t) ; i++ ) {
         switch(md_nkn_stat_graphs[i].mode) {
             case HOURLY :
                 minute = "*/10"; /* Check every 10 minutes */
                 hour = "*";
                 day_of_month = "*";
                 month = "*";
                 day_of_week = "*";
                 break;
             case DAILY :
                 minute = "0"; /* Check every 60 minutes */
                 hour = "*";
                 day_of_month = "*";
                 month = "*";
                 day_of_week = "*";
                 cur_report = "/tmp/intf_hourly_report";
                 bk_report = "/tmp/intf_last_hour_stats";
                 break;
             case WEEKLY :
                 minute = "0"; /* Check every 24 hours */
                 hour = "0";
                 day_of_month = "*";
                 month = "*";
                 day_of_week = "*";
                 cur_report = "/tmp/intf_daily_report";
                 bk_report = "/tmp/intf_last_day_stats";
                 break;
             case MONTHLY :
                 minute = "0"; /* Check every 24 hours */
                 hour = "0";
                 day_of_month = "*";
                 month = "*";
                 day_of_week = "*";
                 cur_report = "/tmp/intf_daily_report";
                 bk_report = "/tmp/intf_last_day_stats";
                 break;
             default :
                 break;
         }


     user = "admin"; /* Run as admin */
// /opt/tms/bin/mdreq action  "/tms/graphs/bw_hourly"  filename string monthly_image.png graphver uint32 2

     command = smprintf("%s %s %s %s %s %s.png %s %s %s",
                         "/opt/tms/bin/mdreq", "action",
                         md_nkn_stat_graphs[i].action_name, "filename",
                         "string", md_nkn_stat_graphs[i].filename,
                         "graphver", "uint32" , "2" );
     bail_null(command);

    /*
     * Tell it not to try to send mail with the output of the script.
     */
    crontab = smprintf("MAILTO=\"\"\n"
                    "%s %s %s %s %s %s\n",
                       minute, hour, day_of_month, month, day_of_week,
                       command);
    bail_null(crontab);

    /*-----------------------------------------------------------------
     * Write the file
     */
    err = lf_write_bytes(fd_temp, crontab, strlen(crontab), NULL);
    bail_error(err);
    safe_free(command);	// Free this as it is used again in this loop
    safe_free(crontab);	// Free this as it is used again in this loop

    }
    // TODO
    minute = "0,15,30,45"; /* Check every 24 hours */
    hour = "*";
    day_of_month = "*";
    month = "*";
    day_of_week = "*";
    cur_report = "/tmp/intf_day_report";
    bk_report = "/tmp/intf_last_day_stats";
    latest_report = "/tmp/intf_daily_report";
    crontab_report = smprintf("MAILTO=\"\"\n"
                    "%s %s %s %s %s %s %s %s %s %s\n",
                       minute, hour, day_of_month, month, day_of_week,
                       "/opt/nkn/bin/intfstats" , cur_report , bk_report,
                       latest_report, "day");
    bail_null(crontab_report);
    err = lf_write_bytes(fd_temp, crontab_report, strlen(crontab_report), NULL);
    bail_error(err);
    safe_free(crontab_report);

    minute = "0,15,30,45"; /* Check every 15 minutes */
    hour = "*";
    day_of_month = "*";
    month = "*";
    day_of_week = "*";
    cur_report = "/tmp/intf_hour_report";
    bk_report = "/tmp/intf_last_hour_stats";
    latest_report = "/tmp/intf_hourly_report";
    crontab_report = smprintf("MAILTO=\"\"\n"
                    "%s %s %s %s %s %s %s %s %s %s\n",
                       minute, hour, day_of_month, month, day_of_week,
                       "/opt/nkn/bin/intfstats" , cur_report , bk_report,
                       latest_report, "hour");
    bail_null(crontab_report);
    err = lf_write_bytes(fd_temp, crontab_report, strlen(crontab_report), NULL);
    bail_error(err);
    safe_free(crontab_report);


    close(fd_temp);

    /*-----------------------------------------------------------------
     * Move the file into place
     */
    err = lf_temp_file_activate(ts_str(filename_real), filename_temp,
                                md_gen_conf_file_owner,
                                md_gen_conf_file_group,
                                md_gen_conf_file_mode, true);
    bail_error(err);
    /* Launch crontab -u user file
     */

    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);
#define PROG_CRONTAB_PATH   "/usr/bin/crontab"
    err = ts_new_str(&(lp->lp_path), PROG_CRONTAB_PATH);
    bail_error(err);

    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 4,
            PROG_CRONTAB_PATH, "-u", user, ts_str(filename_real));
    bail_error(err);

    lp->lp_wait = false;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_DEBUG;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

    lr = malloc(sizeof(*lr));
    bail_null(lr);
    memset(lr, 0, sizeof(*lr));

    err = lc_launch(lp, lr);
    bail_error(err);
    lc_launch_params_free(&lp);
    free (lr);


bail:
    safe_free(command);
    safe_free(crontab);
    safe_free(crontab_report);
    safe_free(filename_temp);
    ts_free(&filename_real);
    return err;
}
/* ------------------------------------------------------------------------- */
static int
md_nkn_stats_add_initial(
        md_commit *commit,
        mdb_db    *db,
        void *arg)
{
        int err = 0;
        uint32 i = 0;
        uint32 j = 0;
        node_name_t node_pattern = {0}, sample_node = {0};
        char *node_esc = NULL;
        bn_binding *binding = NULL;
        tstr_array *src_nodes = NULL;
        const char *src = NULL;
        uint32 num_src = 0 ;
        uint32 src_cnt = 0 ;

/* Removing nkn_cpu_util_ave alarm
        err = mdb_create_node_bindings(commit, db,
            md_nkn_cpu_util_average_initial_values,
            sizeof(md_nkn_cpu_util_average_initial_values)/sizeof(bn_str_value));
        bail_error(err);

        err = mdb_create_node_bindings
        (commit, db, md_email_initial_values,
         sizeof(md_email_initial_values) / sizeof(bn_str_value));
        bail_error(err);
*/
        err = mdb_create_node_bindings(commit, db,
                md_nkn_proc_mem_inital_values,
                sizeof(md_nkn_proc_mem_inital_values)/ sizeof(bn_str_value));
        bail_error(err);


       for ( i = 0 ; i < sizeof(nkn_sample_cfg_entries)/sizeof(struct sample_config) ; i++)
       {
           if ( nkn_sample_cfg_entries[i].name != NULL )
           {
               snprintf( node_pattern, sizeof(node_pattern),"/stats/config/sample/%s",nkn_sample_cfg_entries[i].name );
               err = mdb_set_node_str
    	    	    ( commit, db, bsso_create, 0, bt_string, nkn_sample_cfg_entries[i].name, "%s", node_pattern );
               bail_error(err);
               err = mdb_set_node_str
    	    	    ( commit, db, bsso_create, 0, bt_string, nkn_sample_cfg_entries[i].label, "/stat/nkn/stats/%s/label", nkn_sample_cfg_entries[i].name );
               bail_error(err);
               err = mdb_set_node_str
    	    	    ( commit, db, bsso_create, 0, bt_string, nkn_sample_cfg_entries[i].unit, "/stat/nkn/stats/%s/unit", nkn_sample_cfg_entries[i].name );
               bail_error(err);
               if( nkn_sample_cfg_entries[i].enable != NULL )
               {
                   err = mdb_set_node_str
			( commit, db, bsso_modify, 0, bt_bool, nkn_sample_cfg_entries[i].enable, "%s/enable", node_pattern );
                   bail_error(err);
               }
               /*! support multiple patterns */
               if ( nkn_sample_cfg_entries[i].node != NULL )
               {
                   err = ts_tokenize_str( nkn_sample_cfg_entries[i].node, ',', '\0', '\0', 0, &src_nodes );
                   bail_error_null( err, src_nodes );
                   num_src = tstr_array_length_quick(src_nodes);
                   for ( j = 0 ; j < num_src ; j++)
                   {
                       src = tstr_array_get_str_quick( src_nodes, j );
                       bail_null(src);
                       err = bn_binding_name_escape_str( src, &node_esc );
                       bail_error_null( err, node_esc );

                       err = mdb_set_node_str
		    	( commit, db, bsso_create, 0, bt_name, src, "%s/node/%s", node_pattern, node_esc );
                       safe_free(node_esc);
                   }
                   tstr_array_free(&src_nodes);
               }
               snprintf( sample_node, sizeof(sample_node), "%s/interval", node_pattern);
               err = bn_binding_new_duration_sec( &binding, sample_node, ba_value, 0, nkn_sample_cfg_entries[i].interval );
               bail_error(err);
               err = mdb_set_node_binding( commit, db, bsso_modify, 0, binding );
               bail_error(err);
               bn_binding_free(&binding);

               snprintf( sample_node, sizeof(sample_node), "%s/num_to_keep", node_pattern);
               err = bn_binding_new_uint32( &binding, sample_node, ba_value, 0, nkn_sample_cfg_entries[i].num_to_keep );
               bail_error(err);
               err = mdb_set_node_binding( commit, db, bsso_modify, 0, binding );
               bail_error(err);
               bn_binding_free(&binding);

               if( nkn_sample_cfg_entries[i].sample_method != NULL )
               {
                   err = mdb_set_node_str
			( commit, db, bsso_modify, 0, bt_string, nkn_sample_cfg_entries[i].sample_method, "%s/sample_method", node_pattern );
                   bail_error(err);
               }
               if( nkn_sample_cfg_entries[i].delta_constraint != NULL )
               {
                   err = mdb_set_node_str
			( commit, db, bsso_modify, 0, bt_string, nkn_sample_cfg_entries[i].delta_constraint, "%s/delta_constraint", node_pattern );
                   bail_error(err);
               }

               snprintf( sample_node, sizeof(sample_node), "%s/sync_interval", node_pattern );
               err = bn_binding_new_uint32( &binding, sample_node, ba_value, 0, nkn_sample_cfg_entries[i].sync_interval);
               bail_error(err);
               err = mdb_set_node_binding( commit,db, bsso_modify, 0, binding );
               bail_error(err);
               bn_binding_free(&binding);

	       /* Setting num_to_cache for samples to comply with Samara FIR */	
               snprintf( sample_node, sizeof(sample_node), "%s/num_to_cache", node_pattern );
               err = mdb_set_node_str
                     (commit, db, bsso_modify, 0, bt_int32, "-1", "%s", sample_node );
               bail_error(err);

	       /* New node 'delta_average' in samara infrastructure */
	       if( nkn_sample_cfg_entries[i].delta_avg != NULL )
               {
                   err = mdb_set_node_str
                        ( commit, db, bsso_modify, 0, bt_bool, nkn_sample_cfg_entries[i].delta_avg, "%s/delta_average", node_pattern );
                   bail_error(err);
               }


           }

       }


       /*! CHD Initialization */
       for ( i = 0 ; i < sizeof( nkn_chd_cfg_entries )/sizeof( struct chd_config ) ; i++ ) {
           if ( nkn_chd_cfg_entries[i].name != NULL )
           {
               snprintf( node_pattern, sizeof(node_pattern), "/stats/config/chd/%s", nkn_chd_cfg_entries[i].name );
               err = mdb_set_node_str
                   ( commit, db, bsso_create, 0, bt_string, nkn_chd_cfg_entries[i].name, "%s", node_pattern);
               bail_error(err);
               err = mdb_set_node_str
    	    	    ( commit, db, bsso_create, 0, bt_string, nkn_chd_cfg_entries[i].label, "/stat/nkn/stats/%s/label", nkn_chd_cfg_entries[i].name );
               bail_error(err);
               err = mdb_set_node_str
    	    	    ( commit, db, bsso_create, 0, bt_string, nkn_chd_cfg_entries[i].unit, "/stat/nkn/stats/%s/unit", nkn_chd_cfg_entries[i].name );
               bail_error(err);

               if ( nkn_chd_cfg_entries[i].enable != NULL )
               {
                   err = mdb_set_node_str
			( commit, db, bsso_modify, 0, bt_bool, nkn_chd_cfg_entries[i].enable, "%s/enable", node_pattern );
                   bail_error(err);
               }
               if ( nkn_chd_cfg_entries[i].source_type != NULL )
               {
                   err = mdb_set_node_str
			( commit, db, bsso_modify, 0, bt_string, nkn_chd_cfg_entries[i].source_type, "%s/source/type", node_pattern );
                   bail_error(err);
               }
               if ( nkn_chd_cfg_entries[i].source_id != NULL )
               {
                   err = mdb_set_node_str
			( commit, db, bsso_modify, 0, bt_string, nkn_chd_cfg_entries[i].source_id, "%s/source/id", node_pattern );
                   bail_error(err);
               }
               /*! Read all source node configuraions */
               if ( nkn_chd_cfg_entries[i].source_node != NULL )
               {
                   src_cnt = 0;
                   while ( nkn_chd_cfg_entries[i].source_node[src_cnt] != NULL)
                   {
                       src = nkn_chd_cfg_entries[i].source_node[src_cnt];
                       bail_null(src);
                       err = bn_binding_name_escape_str(src, &node_esc);
                       bail_error_null(err,node_esc);

                       err = mdb_set_node_str
		    	( commit, db, bsso_create, 0, bt_name, src, "%s/source/node/%s", node_pattern, node_esc );
                       safe_free(node_esc);
                       src_cnt++;
                   }
                   tstr_array_free(&src_nodes);
               }
               snprintf( sample_node, sizeof(sample_node), "%s/num_to_keep", node_pattern );
               err = bn_binding_new_uint32( &binding, sample_node, ba_value, 0, nkn_chd_cfg_entries[i].num_to_keep );
               bail_error(err);
               err = mdb_set_node_binding( commit, db, bsso_modify, 0, binding );
               bail_error(err);
               bn_binding_free(&binding);

               if ( nkn_chd_cfg_entries[i].select_type != NULL )
               {
                   err = mdb_set_node_str
			( commit, db, bsso_modify, 0, bt_string, nkn_chd_cfg_entries[i].select_type, "%s/select_type", node_pattern );
                   bail_error(err);
               }

               snprintf( sample_node, sizeof(sample_node), "%s/instances/num_to_use", node_pattern );
               err = bn_binding_new_uint32( &binding, sample_node, ba_value, 0, nkn_chd_cfg_entries[i].inst_num_to_use );
               bail_error(err);
               err = mdb_set_node_binding( commit, db, bsso_modify, 0, binding );
               bail_error(err);
               bn_binding_free(&binding);

               snprintf( sample_node, sizeof(sample_node), "%s/instances/num_to_advance", node_pattern );
               err = bn_binding_new_uint32( &binding, sample_node, ba_value, 0, nkn_chd_cfg_entries[i].inst_num_to_advance );
               bail_error(err);
               err = mdb_set_node_binding(commit,db,bsso_modify,0,binding);
               bail_error(err);
               bn_binding_free(&binding);

	       /* Setting num_to_cache for chds to comply with Samara FIR */	
               snprintf( sample_node, sizeof(sample_node), "%s/num_to_cache", node_pattern );
               err = mdb_set_node_str
                     ( commit, db, bsso_modify, 0, bt_int32, "-1", "%s", sample_node );
               bail_error(err);

               snprintf( sample_node, sizeof(sample_node), "%s/time/interval_length", node_pattern );
               err = bn_binding_new_duration_sec( &binding, sample_node, ba_value, 0, nkn_chd_cfg_entries[i].time_interval_length );
               bail_error(err);
               err = mdb_set_node_binding( commit, db, bsso_modify, 0, binding );
               bail_error(err);
               bn_binding_free(&binding);

               snprintf( sample_node, sizeof(sample_node), "%s/time/interval_distance", node_pattern );
               err = bn_binding_new_duration_sec( &binding, sample_node, ba_value, 0, nkn_chd_cfg_entries[i].time_interval_distance );
               bail_error(err);
               err = mdb_set_node_binding(commit, db, bsso_modify, 0, binding );
               bail_error(err);
               bn_binding_free(&binding);

               snprintf( sample_node, sizeof(sample_node), "%s/time/interval_phase", node_pattern );
               err = bn_binding_new_duration_sec( &binding, sample_node, ba_value, 0, nkn_chd_cfg_entries[i].time_interval_phase );
               bail_error(err);
               err = mdb_set_node_binding( commit, db, bsso_modify, 0, binding );
               bail_error(err);
               bn_binding_free(&binding);

               if ( nkn_chd_cfg_entries[i].function != NULL )
               {
                   err = mdb_set_node_str
			( commit, db, bsso_modify, 0, bt_string, nkn_chd_cfg_entries[i].function, "%s/function", node_pattern );
                   bail_error(err);
               }

               snprintf( sample_node, sizeof(sample_node), "%s/sync_interval", node_pattern);
               err = bn_binding_new_uint32( &binding, sample_node, ba_value, 0, nkn_chd_cfg_entries[i].sync_interval );
               bail_error(err);
               err = mdb_set_node_binding( commit, db, bsso_modify, 0, binding );
               bail_error(err);
               bn_binding_free(&binding);

               if ( nkn_chd_cfg_entries[i].calc_partial != NULL )
               {
                   err = mdb_set_node_str
			( commit, db, bsso_modify, 0, bt_bool, nkn_chd_cfg_entries[i].calc_partial, "%s/calc_partial", node_pattern );
                   bail_error(err);
               }
               if ( nkn_chd_cfg_entries[i].alarm_partial != NULL )
               {
                   err = mdb_set_node_str
			( commit, db, bsso_modify, 0, bt_bool, nkn_chd_cfg_entries[i].alarm_partial, "%s/alarm_partial", node_pattern );
                   bail_error(err);
               }
           }
       }

       /*! Disable Individual CPU util alerts */
       err = mdb_set_node_str
                (commit,db,bsso_modify,0,bt_bool,"false", "%s/enable", "/stats/config/alarm/cpu_util_indiv");
       bail_error(err);

      /* Alarm should be triggered only when the value stays above the threshold for sometime.
	 The values are set here for these 3 alarms as they are defined by Tall Maple */
       err = mdb_set_node_str(commit, db, bsso_modify, 0, bt_string, "mon_intf_util",
				 "/stats/config/alarm/intf_util/trigger/id");
       bail_error(err);

       err = mdb_set_node_str(commit, db, bsso_modify, 0, bt_string, "chd",
				 "/stats/config/alarm/paging/trigger/type");
       bail_error(err);

       err = mdb_set_node_str(commit, db, bsso_modify, 0, bt_string, "mon_paging",
				 "/stats/config/alarm/paging/trigger/id");
       bail_error(err);

       for ( i = 0 ; i < sizeof(alarm_cfg_entries)/sizeof(struct alarm_config) ; i++)
       {
           if (alarm_cfg_entries[i].name != NULL)
           {
               snprintf(node_pattern, sizeof(node_pattern), "/stats/config/alarm/%s", alarm_cfg_entries[i].name);
               err = mdb_set_node_str
                   (commit,db,bsso_create,0,bt_string,alarm_cfg_entries[i].name,"%s",node_pattern);
               bail_error(err);

               if (alarm_cfg_entries[i].enable != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,bt_bool,alarm_cfg_entries[i].enable,"%s/enable",node_pattern);
                   bail_error(err);
               }

               if (alarm_cfg_entries[i].disable_allowed != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,bt_bool,alarm_cfg_entries[i].disable_allowed,"%s/disable_allowed",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].ignore_first_value != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,bt_bool,alarm_cfg_entries[i].ignore_first_value,"%s/ignore_first_value",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].trigger_type != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,bt_string,alarm_cfg_entries[i].trigger_type,"%s/trigger/type",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].trigger_id != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,bt_string,alarm_cfg_entries[i].trigger_id,"%s/trigger/id",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].trigger_node_pattern != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,bt_name,alarm_cfg_entries[i].trigger_node_pattern,"%s/trigger/node_pattern",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].rising_enable != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,bt_bool,alarm_cfg_entries[i].rising_enable,"%s/rising/enable",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].falling_enable != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,bt_bool,alarm_cfg_entries[i].falling_enable,"%s/falling/enable",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].rising_err_threshold != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,alarm_cfg_entries[i].data_type,alarm_cfg_entries[i].rising_err_threshold,"%s/rising/error_threshold",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].falling_err_threshold != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,alarm_cfg_entries[i].data_type,alarm_cfg_entries[i].falling_err_threshold,"%s/falling/error_threshold",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].rising_clr_threshold != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,alarm_cfg_entries[i].data_type,alarm_cfg_entries[i].rising_clr_threshold,"%s/rising/clear_threshold",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].falling_clr_threshold != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,alarm_cfg_entries[i].data_type,alarm_cfg_entries[i].falling_clr_threshold,"%s/falling/clear_threshold",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].rising_event_on_clr != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,bt_bool,alarm_cfg_entries[i].rising_event_on_clr,"%s/rising/event_on_clear",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].falling_event_on_clr != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,bt_bool,alarm_cfg_entries[i].falling_event_on_clr,"%s/falling/event_on_clear",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].event_name_root != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,bt_string,alarm_cfg_entries[i].event_name_root,"%s/event_name_root",node_pattern);
                   bail_error(err);
               }
               if (alarm_cfg_entries[i].clear_if_missing != NULL)
               {
                   err = mdb_set_node_str
			(commit,db,bsso_modify,0,bt_bool,alarm_cfg_entries[i].clear_if_missing,"%s/clear_if_missing",node_pattern);
                   bail_error(err);
               }

	       err = mdb_set_node_str( commit, db, bsso_modify, 0, bt_duration_sec,
					   alarm_cfg_entries[i].rate_limit_win_short, "%s/short/rate_limit_win", node_pattern);
               bail_error(err);

	       err = mdb_set_node_str( commit, db, bsso_modify, 0, bt_uint32,
					   alarm_cfg_entries[i].rate_limit_max_short, "%s/short/rate_limit_max", node_pattern);
               bail_error(err);

               err = mdb_set_node_str( commit, db, bsso_modify, 0, bt_duration_sec,
                                           alarm_cfg_entries[i].rate_limit_win_medium, "%s/medium/rate_limit_win", node_pattern);
               bail_error(err);

               err = mdb_set_node_str( commit, db, bsso_modify, 0, bt_uint32,
                                           alarm_cfg_entries[i].rate_limit_max_medium, "%s/medium/rate_limit_max", node_pattern);
               bail_error(err);

               err = mdb_set_node_str( commit, db, bsso_modify, 0, bt_duration_sec,
                                           alarm_cfg_entries[i].rate_limit_win_long, "%s/long/rate_limit_win", node_pattern);
               bail_error(err);

               err = mdb_set_node_str( commit, db, bsso_modify, 0, bt_uint32,
                                           alarm_cfg_entries[i].rate_limit_max_long, "%s/long/rate_limit_max", node_pattern);
               bail_error(err);

           }

       }

 bail:
        safe_free(node_esc);
        return err;
}
#ifdef PROD_FEATURE_GRAPHING

#define md_mem_bandwidth_chd_base "/stats/state/chd/cache_byte_rate/node/\\/stat\\/nkn\\/nvsd\\/cache_byte_rate"

#define md_disk_bandwidth_chd_base "/stats/state/chd/disk_byte_rate/node/\\/stat\\/nkn\\/nvsd\\/disk_byte_rate"

#define md_origin_bandwidth_chd_base "/stats/state/chd/origin_byte_rate/node/\\/stat\\/nkn\\/nvsd\\/origin_byte_rate"

static const char *md_mem_bandwidth_chd = md_mem_bandwidth_chd_base "/instance";

static const char *md_disk_bandwidth_chd = md_disk_bandwidth_chd_base "/instance";

static const char *md_origin_bandwidth_chd = md_origin_bandwidth_chd_base "/instance";


static int
md_graph_bandwidth(md_commit *commit, const mdb_db *db,
                    const char *action_name,
                    bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    const bn_binding *binding = NULL;
    tbool made_graph = false;

    err = md_graph_bandwidth_new(commit, db, params, &made_graph);
    bail_error(err);

    if (!made_graph) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Failed to create Bandwidth graph, no data available"));
        bail_error(err);
    }
 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */

static int
md_graph_bandwidth_new(md_commit *commit, const mdb_db *db,
                      bn_binding_array *params, tbool *ret_made_graph)
{
    int err = 0;
    tbool made_graph = false;
    lg_graph *graph = NULL;
    lg_series_graph *sg = NULL;
    lg_axis *xax = NULL, *yax = NULL;
    uint32 num_points = 0, max_num_points = 0;
    gdImagePtr image = NULL;
    lg_data_series *cache_bw = NULL, *disk_bw = NULL, *origin_bw = NULL;
    lg_data_series_group *dsg = NULL;
    lg_data_series_array *dsa = NULL;
    lg_data_series_group_flags_bf graph_flags = 0;
    tstr_array *legend_labels = NULL;
    tstr_array *series_roots = NULL;
    char *series_root = NULL;
    lg_data_series *ds = NULL;
    uint32 i =0 ;
    lt_time_sec time_min = -1, time_max = -1;


    err = tstr_array_new(&legend_labels, NULL);
    bail_error(err);

    err = tstr_array_new(&series_roots, NULL);
    bail_error(err);

    err = tstr_array_append_str(legend_labels, "Cache Bandwidth");
    bail_error(err);

    err = tstr_array_append_str(series_roots, md_mem_bandwidth_chd_base);
    bail_error(err);

    err = tstr_array_append_str(legend_labels, "Disk Bandwidth");
    bail_error(err);

    err = tstr_array_append_str(series_roots, md_disk_bandwidth_chd_base);
    bail_error(err);

    err = tstr_array_append_str(legend_labels, "Origin Bandwidth");
    bail_error(err);

    err = tstr_array_append_str(series_roots, md_origin_bandwidth_chd_base);
    bail_error(err);

    err = lg_graph_new(lgt_line, &graph);
    bail_error(err);
    graph->lgg_flags |= lgf_color_recycling_nocomplain ; //| lgf_dot_data_points;

    time_max = lt_curr_time();
    /* we need last 1 hr of data */
    time_min = time_max - 3600;

    err = lg_graph_fetch_stats_data_mult
        (graph, true, commit, db, series_roots, legend_labels, -1, -1,
	 time_min, time_max,
         1, 1024 * 1024, 0, &dsa, NULL, NULL);
    bail_error(err);

    for (i = 0; i < 3 ; ++i) {
        ds = lg_data_series_array_get_quick(dsa, i);
        bail_null(ds);
        max_num_points = max(max_num_points,
                             bn_attrib_array_length_quick(ds->lds_y_values));
    }

    if (max_num_points < 2) {
        /* This is not enough data to generate a graph */
        goto bail;
    }
    /* ........................................................................
     * Finish assembling the graph structure, and draw the graph.
     */
    sg = graph->lgg_series_graph;

    err = lg_data_series_group_new
        (graph_flags, true, &dsg);
    bail_error(err);

    err = bn_attrib_new_datetime_sec(&(dsg->ldsg_snap_distance),
                                     ba_value, 0, 15);
    bail_error(err);

    lg_data_series_array_free(&(dsg->ldsg_data));
    dsg->ldsg_data = dsa;

    err = lg_data_series_group_array_append_takeover(sg->lsg_series_groups,
                                                     &dsg);
    bail_error(err);

    xax = sg->lsg_x_axis;
    err = md_graph_format_x_axis(graph, xax);
    bail_error(err);

    yax = sg->lsg_y_axis;

    err = bn_attrib_new_uint64(&(yax->lga_auto_opts->laao_range_low),
                               ba_value, 0, 0);
    bail_error(err);


    err = lg_axis_set_auto(graph, yax, _("MB/Sec"));
    bail_error(err);

    err = md_graph_standard_params(graph);
    bail_error(err);

    err = md_graph_render(graph, params, 0, &image);
    bail_error(err);

    err = md_graph_save(image, params, commit, -1, NULL, 0);
    bail_error(err);

    made_graph = true;

 bail:
    tstr_array_free(&legend_labels);
    tstr_array_free(&series_roots);
    safe_free(series_root);

    if (yax && (yax->lga_auto_opts))
    {
        bn_attrib_free(&(yax->lga_auto_opts->laao_range_low));
    }

    lg_graph_free(&graph);
    if (image) {
        gdImageDestroy(image);
    }
    if (ret_made_graph) {
        *ret_made_graph = made_graph;
    }
    return(err);
}
/* ------------------------------------------------------------------------- */

static int
md_graph_bw_day_new(md_commit *commit, const mdb_db *db,
                       bn_binding_array *params, const tstr_array *names,
                       tbool *ret_made_graph)
{
    int err = 0;
    tstr_array *stats_nodes = NULL;
    int num_stats_nodes = 0, num_stats_node_parts = 0, i = 0, k = 0;
    const char *stats_node = NULL;
    tstr_array *legend_labels = NULL;
    tstr_array *stats_node_parts = NULL;
    tstr_array *series_roots = NULL;
    char *series_root = NULL;
    uint32 max_num_points = 0;
    lg_graph *graph = NULL;
    lg_series_graph *sg = NULL;
    lg_data_series_group *dsg = NULL;
    lg_data_series_array *dsa = NULL;
    lg_data_series *ds = NULL;
    lg_axis *xax = NULL, *yax = NULL;
    gdImagePtr image = NULL;
    const bn_attrib *attr = NULL;
    tstr_array *graph_markup = NULL;
    char *max_pct_str = NULL;
    lg_data_series_group_flags_bf graph_flags = 0;
    uint32 y_axis_range = 0;
    tstring *path = NULL;
    tstring *filename = NULL ;
    tstring *dir_path = NULL;
    lt_time_sec time_min = -1, time_max = -1;

    err = tstr_array_new(&legend_labels, NULL);
    bail_error(err);

    err = tstr_array_new(&series_roots, NULL);
    bail_error(err);

    err = tstr_array_append_str(legend_labels, "Total Bandwidth");
    bail_error(err);

    err = tstr_array_append_str(series_roots, md_total_bandwidth_day);
    bail_error(err);

    err = tstr_array_append_str(legend_labels, "Cache Bandwidth");
    bail_error(err);

    err = tstr_array_append_str(series_roots, md_cache_bandwidth_day);
    bail_error(err);

    err = tstr_array_append_str(legend_labels, "Disk Bandwidth");
    bail_error(err);

    err = tstr_array_append_str(series_roots, md_disk_bandwidth_day);
    bail_error(err);

    err = tstr_array_append_str(legend_labels, "Origin Bandwidth");
    bail_error(err);

    err = tstr_array_append_str(series_roots, md_origin_bandwidth_day);
    bail_error(err);

    /* ........................................................................
     * Fetch all of the data to be graphed, and check if there is enough.
     */
    err = lg_graph_new(lgt_line, &graph);
    bail_error(err);
    graph->lgg_flags |= lgf_color_recycling_nocomplain ; //| lgf_dot_data_points;

    time_max = lt_curr_time();
    /* we need last 24 hrs data */
    time_min = time_max - (3600 * 24);

    err = lg_graph_fetch_stats_data_mult
        (graph, true, commit, db, series_roots, legend_labels, -1, -1,
	 time_min, time_max, 1, 1024 * 1024, 0, &dsa, NULL, NULL);
    bail_error(err);

    for (i = 0; i < 4 ; ++i) {
        ds = lg_data_series_array_get_quick(dsa, i);
        bail_null(ds);
        max_num_points = max(max_num_points,
                             bn_attrib_array_length_quick(ds->lds_y_values));
    }
#if 1
    if (max_num_points < 2) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Not enough data to create Daily Bandwidth graph"));
        complain_error(err);
        goto bail;
    }
#endif

    /* ........................................................................
     * Add the data to the graph
     */
    sg = graph->lgg_series_graph;

    err = lg_data_series_group_new(graph_flags, true, &dsg);
    bail_error(err);

    /* XXX/EMT: 15 is a hack!  See bug 11823 */
    err = bn_attrib_new_datetime_sec(&(dsg->ldsg_snap_distance),
                                     ba_value, 0, 15);
    bail_error(err);

    lg_data_series_array_free(&(dsg->ldsg_data));
    dsg->ldsg_data = dsa;

    err = lg_data_series_group_array_append_takeover(sg->lsg_series_groups,
                                                     &dsg);
    bail_error(err);

    /* ........................................................................
     * Finish assembling the graph structure, and draw the graph.
     */

    xax = sg->lsg_x_axis;
    err = md_graph_format_x_axis(graph, xax);
//    err = md_graph_format_x_axis(graph, xax, 1, 3600);
    bail_error(err);

    yax = sg->lsg_y_axis;

    err = bn_attrib_new_uint64(&(yax->lga_auto_opts->laao_range_low),
                               ba_value, 0, 0);
    bail_error(err);

    err = lg_axis_set_auto(graph, yax, _("MBytes/Sec"));
    bail_error(err);

    err = md_graph_standard_params(graph);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(params, "filename", NULL,
                                                  &filename);
    bail_error(err);

    if (filename == NULL ) {
        err = ts_new_str(&filename, "graph.png");
        bail_error(err);
    }
    err = lf_path_from_dir_file(web_graphs_path, ts_str(filename), &path);
    bail_error(err);

    err = lf_path_parent(ts_str(path), &dir_path);
    bail_error_null(err, dir_path);

    lf_ensure_dir_owner(ts_str(dir_path), 0755, graph_file_uid,
                                          graph_file_gid);
    //err = lf_ensure_dir_of_file(ts_str(path), graph_file_mode);
    bail_error(err);


    /*! TODO Check if the file is available . If so move the file with
        suffix to indicate timeline . Decide on the number of files to
        keep and overwrite the old one when max number of files to keep
        reaches its limit
     */

    err = md_backup_file(ts_str(path), ts_str(dir_path), "bandwidth_day", 10);
    bail_error(err);


    err = md_graph_render(graph, params, 0, &image);
    bail_error(err);

    /* ........................................................................
     * Finally, save the graph to disk.
     */
    err = md_graph_save(image, params, commit, -1, NULL, 0);
    bail_error(err);

 bail:
    /* ........................................................................
     * If we didn't successfully make the graph, we have to report it.
     */
    if (err) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Error creating Hourly Bandwidth graph"));
        complain_error(err);
    }
    ts_free(&filename);
    ts_free(&path);
    ts_free(&dir_path);

    if (yax && (yax->lga_auto_opts))
    {
        bn_attrib_free(&(yax->lga_auto_opts->laao_range_low));
    }

    tstr_array_free(&stats_nodes);
    tstr_array_free(&stats_node_parts);
    tstr_array_free(&legend_labels);
    tstr_array_free(&series_roots);
    tstr_array_free(&graph_markup);
    safe_free(series_root);
    safe_free(max_pct_str);
    ts_free(&filename);
    ts_free(&path);
    ts_free(&dir_path);
    lg_graph_free(&graph);
    if (image) {
        gdImageDestroy(image);
    }
    if (ret_made_graph) {
        *ret_made_graph = true;
    }
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_graph_bw_day(md_commit *commit, const mdb_db *db,
                   const char *action_name,
                   bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    tbool made_graph = false;

    err = md_graph_bw_day_new(commit, db, params, NULL , &made_graph);
    bail_error(err);

    if (!made_graph) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Failed to create Net graph, no data available"));
        bail_error(err);
    }

 bail:
    return(err);
}


static int
md_graph_bw_week_new(md_commit *commit, const mdb_db *db,
                       bn_binding_array *params, const tstr_array *names,
                       tbool *ret_made_graph)
{
    int err = 0;
    tstr_array *stats_nodes = NULL;
    int num_stats_nodes = 0, num_stats_node_parts = 0, i = 0, k = 0;
    const char *stats_node = NULL;
    tstr_array *legend_labels = NULL;
    tstr_array *stats_node_parts = NULL;
    tstr_array *series_roots = NULL;
    char *series_root = NULL;
    uint32 max_num_points = 0;
    lg_graph *graph = NULL;
    lg_series_graph *sg = NULL;
    lg_data_series_group *dsg = NULL;
    lg_data_series_array *dsa = NULL;
    lg_data_series *ds = NULL;
    lg_axis *xax = NULL, *yax = NULL;
    gdImagePtr image = NULL;
    const bn_attrib *attr = NULL;
    tstr_array *graph_markup = NULL;
    char *max_pct_str = NULL;
    lg_data_series_group_flags_bf graph_flags = 0;
    uint32 y_axis_range = 0;
    tstring *path = NULL;
    tstring *filename = NULL ;
    tstring *dir_path = NULL;
    lt_time_sec time_min = -1, time_max = -1;

    err = tstr_array_new(&legend_labels, NULL);
    bail_error(err);
    err = tstr_array_new(&series_roots, NULL);
    bail_error(err);

    err = tstr_array_append_str(legend_labels, "Total Bandwidth");
    bail_error(err);

    err = tstr_array_append_str(series_roots, md_total_bandwidth_week);
    bail_error(err);

    err = tstr_array_append_str(legend_labels, "Cache Bandwidth");
    bail_error(err);

    err = tstr_array_append_str(series_roots, md_cache_bandwidth_week);
    bail_error(err);

    err = tstr_array_append_str(legend_labels, "Disk Bandwidth");
    bail_error(err);

    err = tstr_array_append_str(series_roots, md_disk_bandwidth_week);
    bail_error(err);

    err = tstr_array_append_str(legend_labels, "Origin Bandwidth");
    bail_error(err);

    err = tstr_array_append_str(series_roots, md_origin_bandwidth_week);
    bail_error(err);

   /* ........................................................................
    * Fetch all of the data to be graphed, and check if there is enough.
    */
    err = lg_graph_new(lgt_line, &graph);
    bail_error(err);
    graph->lgg_flags |= lgf_color_recycling_nocomplain ; //| lgf_dot_data_points;

    time_max = lt_curr_time();
    /* we need last 7 days of data */
    time_min = time_max - (3600 * 24 * 7);

    err = lg_graph_fetch_stats_data_mult
        (graph, true, commit, db, series_roots, legend_labels, -1, -1,
	 time_min, time_max, 1, 1024 * 1024 , 0, &dsa, NULL, NULL);
    bail_error(err);

    for (i = 0; i < 4 ; ++i) {
        ds = lg_data_series_array_get_quick(dsa, i);
        bail_null(ds);
        max_num_points = max(max_num_points,
                             bn_attrib_array_length_quick(ds->lds_y_values));
    }
#if 1
    if (max_num_points < 2) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Not enough data to create Daily Bandwidth graph"));
        complain_error(err);
        goto bail;
    }
#endif

    /* ........................................................................
     * Add the data to the graph
     */
    sg = graph->lgg_series_graph;

    err = lg_data_series_group_new(graph_flags, true, &dsg);
    bail_error(err);

    /* XXX/EMT: 15 is a hack!  See bug 11823 */
    err = bn_attrib_new_datetime_sec(&(dsg->ldsg_snap_distance),
                                     ba_value, 0, 15);
    bail_error(err);

    lg_data_series_array_free(&(dsg->ldsg_data));
    dsg->ldsg_data = dsa;

    err = lg_data_series_group_array_append_takeover(sg->lsg_series_groups,
                                                     &dsg);
    bail_error(err);

    /* ........................................................................
     * Finish assembling the graph structure, and draw the graph.
     */

    xax = sg->lsg_x_axis;
    err = md_graph_format_x_axis(graph, xax);
    bail_error(err);

    yax = sg->lsg_y_axis;

    err = bn_attrib_new_uint64(&(yax->lga_auto_opts->laao_range_low),
                               ba_value, 0, 0);
    bail_error(err);

    err = lg_axis_set_auto(graph, yax, _("MBytes/Sec"));
    bail_error(err);

    err = md_graph_standard_params(graph);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(params, "filename", NULL,
                                                  &filename);
    bail_error(err);

    if (filename == NULL ) {
        err = ts_new_str(&filename, "graph.png");
        bail_error(err);
    }
    err = lf_path_from_dir_file(web_graphs_path, ts_str(filename), &path);
    bail_error(err);

    err = lf_path_parent(ts_str(path), &dir_path);
    bail_error_null(err, dir_path);

    err = lf_ensure_dir_owner(ts_str(dir_path), 0755, graph_file_uid,
                                          graph_file_gid);

    //err = lf_ensure_dir_of_file(ts_str(path), graph_file_mode);
    bail_error(err);

    err = md_backup_file(ts_str(path), ts_str(dir_path), "bandwidth_week", 10);
    bail_error(err);

    err = md_graph_render(graph, params, 0, &image);
    bail_error(err);

    /* ........................................................................
     * Finally, save the graph to disk.
     */
    err = md_graph_save(image, params, commit, -1, NULL, 0);
    bail_error(err);

 bail:
    /* ........................................................................
     * If we didn't successfully make the graph, we have to report it.
     */
    if (err) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Error creating Daily Bandwidth Graph"));
        complain_error(err);
    }
    ts_free(&filename);
    ts_free(&path);
    ts_free(&dir_path);

    if (yax && (yax->lga_auto_opts))
    {
        bn_attrib_free(&(yax->lga_auto_opts->laao_range_low));
    }

    tstr_array_free(&stats_nodes);
    tstr_array_free(&stats_node_parts);
    tstr_array_free(&legend_labels);
    tstr_array_free(&series_roots);
    tstr_array_free(&graph_markup);
    safe_free(series_root);
    safe_free(max_pct_str);
    ts_free(&filename);
    ts_free(&path);
    ts_free(&dir_path);
    lg_graph_free(&graph);
    if (image) {
        gdImageDestroy(image);
    }
    if (ret_made_graph) {
        *ret_made_graph = true;
    }
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_graph_bw_week(md_commit *commit, const mdb_db *db,
                   const char *action_name,
                   bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    tbool made_graph = false;

    err = md_graph_bw_week_new(commit, db, params, NULL, &made_graph);
    bail_error(err);

    if (!made_graph) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Failed to create Net graph, no data available"));
        bail_error(err);
    }

 bail:
    return(err);
}

/*----------------------------------------------------------------------------*/

static int
md_graph_intf_day_new(md_commit *commit, const mdb_db *db,
                       bn_binding_array *params, const tstr_array *names,
                       tbool *ret_made_graph)
{
    int err = 0;
    tbool made_graph = false;
    lg_graph *graph = NULL;
    lg_series_graph *sg = NULL;
    lg_axis *xax = NULL, *yax = NULL;
    int num_points = 0, max_num_points = 0;
    gdImagePtr image = NULL;
    tstring *path = NULL;
    tstring *filename = NULL ;
    tstring *dir_path = NULL;
    lt_time_sec time_min = -1, time_max = -1;

    err = lg_graph_new(lgt_line, &graph);
    bail_error(err);

    graph->lgg_flags |= lgf_color_recycling_nocomplain ;//| lgf_dot_data_points;

    time_max = lt_curr_time();
    /* we need last 24 hrs of data */
    time_min = time_max - (3600 * 24 );

    /* XXX/EMT: 30.0 is a hack!  See bug 11823 */
    err = md_graph_get_stats_data
        (graph, blue_colors_lg, commit, db, names, md_intf_day_rx_chd_base,
         _("%s RX"), NULL, time_min, time_max, 1, 1024 * 1024,
         0, 0.0, &max_num_points);
    bail_error(err);

    /* XXX/EMT: 30.0 is a hack!  See bug 11823 */
    err = md_graph_get_stats_data
        (graph, green_colors_lg, commit, db, names, md_intf_day_tx_chd_base,
         _("%s TX"), NULL, time_min, time_max, 1, 1024 * 1024,
         0, 0.0, &max_num_points);
    bail_error(err);

    if (max_num_points < 2) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Not enough data to create Day Bandwidth graph"));
        complain_error(err);
        /* This is not enough data to generate a graph */
        goto bail;
    }

    sg = graph->lgg_series_graph;
    xax = sg->lsg_x_axis;

    err = md_graph_format_x_axis(graph, xax);
    bail_error(err);

    yax = sg->lsg_y_axis;
    err = bn_attrib_new_uint64(&(yax->lga_auto_opts->laao_range_low),
                               ba_value, 0, 0);
    bail_error(err);

    err = lg_axis_set_auto(graph, yax, _("MBytes"));
    bail_error(err);

    err = md_graph_standard_params(graph);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(params, "filename", NULL,
                                                  &filename);
    bail_error(err);

    if (filename == NULL ) {
        err = ts_new_str(&filename, "graph.png");
        bail_error(err);
    }
    err = lf_path_from_dir_file(web_graphs_path, ts_str(filename), &path);
    bail_error(err);

    err = lf_path_parent(ts_str(path), &dir_path);
    bail_error_null(err, dir_path);

    lf_ensure_dir_owner(ts_str(dir_path), 0755, graph_file_uid,
                                          graph_file_gid);
    bail_error(err);

    err = md_backup_file(ts_str(path), ts_str(dir_path), "intf_day", 10);
    bail_error(err);

    err = md_graph_render(graph, params, 0, &image);
    bail_error(err);

    /* ........................................................................
     * Finally, save the graph to disk.
     */
    err = md_graph_save(image, params, commit, -1, NULL, 0);
    bail_error(err);


    made_graph = true;

 bail:
    if (err) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Error creating Day Interface Stats graph"));
        complain_error(err);
    }
    ts_free(&filename);
    ts_free(&path);
    ts_free(&dir_path);

    if (yax && (yax->lga_auto_opts))
    {
        bn_attrib_free(&(yax->lga_auto_opts->laao_range_low));
    }
    ts_free(&filename);
    ts_free(&path);
    ts_free(&dir_path);

    lg_graph_free(&graph);

    if (image) {
        gdImageDestroy(image);
    }

    if (ret_made_graph) {
        *ret_made_graph = made_graph;
    }
    return(err);
}

/*----------------------------------------------------------------------------*/

static int
md_graph_intf_day(md_commit *commit, const mdb_db *db,
                   const char *action_name,
                   bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    tbool made_graph = false;
    bn_binding_array *intf_binding_array = NULL;
    tstr_array *names = NULL;

    err = mdb_iterate_binding(commit, db, "/net/interface/state",
                                      0, &intf_binding_array);
    bail_error(err);

    err = bn_binding_array_get_last_name_parts(intf_binding_array, &names);
    bail_error(err);

    /*
     * For some reason, these get to us in reverse alphabetical order.
     */
    err = tstr_array_sort(names);
    bail_error(err);

    /* Don't report on the loopback interface */
    err = tstr_array_delete_str(names, "lo");
    bail_error(err);

    err = md_graph_intf_day_new(commit, db, params, names, &made_graph);
    bail_error(err);

    if (!made_graph) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Failed to create Net graph, no data available"));
        bail_error(err);
    }

 bail:
    bn_binding_array_free(&intf_binding_array);
    tstr_array_free(&names);
    return(err);
}

/*!----------------------------------------------------------------------------*/
static int
md_graph_intf_week_new(md_commit *commit, const mdb_db *db,
                       bn_binding_array *params, const tstr_array *names,
                       tbool *ret_made_graph)
{
    int err = 0;
    tbool made_graph = false;
    lg_graph *graph = NULL;
    lg_series_graph *sg = NULL;
    lg_axis *xax = NULL, *yax = NULL;
    int num_points = 0, max_num_points = 0;
    gdImagePtr image = NULL;
    tstring *path = NULL;
    tstring *filename = NULL ;
    tstring *dir_path = NULL;
    lt_time_sec time_min = -1, time_max = -1;

    err = lg_graph_new(lgt_line, &graph);
    bail_error(err);

    graph->lgg_flags |= lgf_color_recycling_nocomplain;

    time_max = lt_curr_time();
    /* we need last 7 days of data */
    time_min = time_max - (3600 * 24 * 7);


    /* XXX/EMT: 30.0 is a hack!  See bug 11823 */
    err = md_graph_get_stats_data
        (graph, blue_colors_lg, commit, db, names, md_intf_week_rx_chd_base,
         _("%s RX"), NULL, time_min, time_max, 1, 1024 * 1024,
         0, 0.0, &max_num_points);
    bail_error(err);

    /* XXX/EMT: 30.0 is a hack!  See bug 11823 */
    err = md_graph_get_stats_data
        (graph, green_colors_lg, commit, db, names, md_intf_week_tx_chd_base,
         _("%s TX"), NULL, -1, time_min, time_max, 1024 * 1024,
         0, 0.0, &max_num_points);
    bail_error(err);

    if (max_num_points < 2) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Not enough data to create week Bandwidth graph"));
        complain_error(err);
        /* This is not enough data to generate a graph */
        goto bail;
    }

    sg = graph->lgg_series_graph;
    xax = sg->lsg_x_axis;

    err = md_graph_format_x_axis(graph, xax);
    bail_error(err);

    yax = sg->lsg_y_axis;
    err = bn_attrib_new_uint64(&(yax->lga_auto_opts->laao_range_low),
                               ba_value, 0, 0);
    bail_error(err);

    err = lg_axis_set_auto(graph, yax, _("MBytes"));
    bail_error(err);

    err = md_graph_standard_params(graph);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(params, "filename", NULL,
                                                  &filename);
    bail_error(err);

    if (filename == NULL ) {
        err = ts_new_str(&filename, "graph.png");
        bail_error(err);
    }
    err = lf_path_from_dir_file(web_graphs_path, ts_str(filename), &path);
    bail_error(err);

    err = lf_path_parent(ts_str(path), &dir_path);
    bail_error_null(err, dir_path);

    lf_ensure_dir_owner(ts_str(dir_path), 0755, graph_file_uid,
                                          graph_file_gid);
    bail_error(err);

    err = md_backup_file(ts_str(path), ts_str(dir_path), "intf_week", 10);
    bail_error(err);

    err = md_graph_render(graph, params, 0, &image);
    bail_error(err);

    /* ........................................................................
     * Finally, save the graph to disk.
     */
    err = md_graph_save(image, params, commit, -1, NULL, 0);
    bail_error(err);


    made_graph = true;

 bail:
    if (err) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Error creating Week Interface Stats graph"));
        complain_error(err);
    }
    ts_free(&filename);
    ts_free(&path);
    ts_free(&dir_path);

    if (yax && (yax->lga_auto_opts))
    {
        bn_attrib_free(&(yax->lga_auto_opts->laao_range_low));
    }
    ts_free(&filename);
    ts_free(&path);
    ts_free(&dir_path);

    lg_graph_free(&graph);

    if (image) {
        gdImageDestroy(image);
    }

    if (ret_made_graph) {
        *ret_made_graph = made_graph;
    }
    return(err);
}


static int
md_graph_intf_week(md_commit *commit, const mdb_db *db,
                   const char *action_name,
                   bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    tbool made_graph = false;
    bn_binding_array *intf_binding_array = NULL;
    tstr_array *names = NULL;

    err = mdb_iterate_binding(commit, db, "/net/interface/state",
                                      0, &intf_binding_array);
    bail_error(err);

    err = bn_binding_array_get_last_name_parts(intf_binding_array, &names);
    bail_error(err);

    /*
     * For some reason, these get to us in reverse alphabetical order.
     */
    err = tstr_array_sort(names);
    bail_error(err);

    /* Don't report on the loopback interface */
    err = tstr_array_delete_str(names, "lo");
    bail_error(err);

    err = md_graph_intf_week_new(commit, db, params, names, &made_graph);
    bail_error(err);

    if (!made_graph) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Failed to create Net graph, no data available"));
        bail_error(err);
    }

 bail:
    bn_binding_array_free(&intf_binding_array);
    tstr_array_free(&names);
    return(err);
}

/* ------------------------------------------------------------------------- */

#endif /* PROD_FEATURE_GRAPHING */
static int
md_system_get_cpucores(md_commit *commit, const mdb_db *db,
                    const char *node_name,
                    const bn_attrib_array *node_attribs,
                    bn_binding **ret_binding,
                    uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    tstr_array *lines = NULL;
    tstr_array *fields = NULL;
    const char *line = NULL;
    const char *field = NULL;
    uint32 num_lines = 0 , i = 0 ;
    uint32 num_fields = 0 , j = 0 ;
    uint32 cpus[32] ;
    uint32 num_cpus = 0;

    for ( i = 0 ; i < 32 ; i++ ) {
        cpus[i] = 0;
    }

    err = lf_read_file_lines("/proc/cpuinfo", &lines);
    bail_error(err);

    num_lines = tstr_array_length_quick(lines);
    for ( i = 0 ; i < num_lines; ++i) {
        line = tstr_array_get_str_quick(lines, i);
        bail_null(line);
        if ( (strstr(line, "physical") != NULL) && (strstr(line, "id") != NULL ) )
        {
            err = ts_tokenize_str(line,':','\0','\0',0,&fields);
            bail_error_null(err,fields);
            num_fields = tstr_array_length_quick(fields);
            if ( num_fields >= 2 )
            {
                field = tstr_array_get_str_quick(fields,1);
                bail_null(field);
                if (atoi(field) < 32 ) {
                    cpus[atoi(field)] = 1;
                }
                else {
                    lc_log_basic(LOG_NOTICE, "Number of CPUs are calculated wrongly");
                    goto bail;
                }
            }
            tstr_array_free(&fields);
       }
    }
    for ( i = 0 ; i < 32 ; i++ ) {
        if (cpus[i] == 1)
            num_cpus++;
    }

    err = bn_binding_new_uint32(ret_binding,
                        node_name, ba_value, 0, num_cpus);
    bail_error(err);

 bail:
    tstr_array_free(&fields);
    tstr_array_free(&lines);
    return(err);
}

static int
md_cntr_upgrade_downgrade(const mdb_db *old_db,
                          mdb_db *inout_new_db,
                          uint32 from_module_version,
                          uint32 to_module_version, void *arg)
{
    int err = 0;
    md_upgrade_rule_array **rules = arg;
    char *bname = NULL;
    char *node_pattern = NULL;
    uint32 i = 0;

    bail_null(rules);
    bail_null(*rules);

    err = md_upgrade_db_transform
        (old_db, inout_new_db, from_module_version, to_module_version, *rules);
    bail_error(err);



    /*
     * Upgrades we can't express as rules :
     * - 1 to 2 : add cpu_util_average alarm
     */

    if (from_module_version == 1 && to_module_version == 2 ) {
       err = mdb_create_node_bindings(NULL, inout_new_db,
                 md_nkn_cpu_util_average_initial_values,
                 sizeof(md_nkn_cpu_util_average_initial_values)/sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_email_initial_values,
            sizeof(md_email_initial_values) / sizeof(bn_str_value));
       bail_error(err);

       /*! Disable Individual CPU util alerts */
       err = mdb_set_node_str
            (NULL, inout_new_db, bsso_modify, 0, bt_bool,
             "false", "%s/enable", "/stats/config/alarm/cpu_util_indiv");
       bail_error(err);

    }

    if (from_module_version == 2 && to_module_version == 3) {
       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_proc_mem_inital_values,
            sizeof(md_nkn_proc_mem_inital_values) / sizeof(bn_str_value));
       bail_error(err);
    }

    if (from_module_version == 3 && to_module_version == 4) {
       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_cache_bw_day_inital_values,
            sizeof(md_nkn_cache_bw_day_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_cache_bw_week_inital_values,
            sizeof(md_nkn_cache_bw_week_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_disk_bw_day_inital_values,
            sizeof(md_nkn_disk_bw_day_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_disk_bw_week_inital_values,
            sizeof(md_nkn_disk_bw_week_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_origin_bw_day_inital_values,
            sizeof(md_nkn_origin_bw_day_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_origin_bw_week_inital_values,
            sizeof(md_nkn_origin_bw_week_inital_values ) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_total_bw_day_inital_values,
            sizeof(md_nkn_total_bw_day_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_total_bw_week_inital_values,
            sizeof (md_nkn_total_bw_week_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_intf_day_inital_values,
            sizeof(md_nkn_intf_day_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_intf_week_inital_values,
            sizeof(md_nkn_intf_week_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_intf_month_inital_values,
            sizeof(md_nkn_intf_month_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_bw_month_avg_inital_values,
            sizeof(md_nkn_bw_month_avg_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_bw_month_peak_inital_values,
            sizeof(md_nkn_bw_month_peak_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_bw_week_avg_inital_values,
            sizeof(md_nkn_bw_week_avg_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_bw_week_peak_inital_values,
            sizeof(md_nkn_bw_week_peak_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_connection_month_avg_inital_values,
            sizeof(md_nkn_connection_month_avg_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_connection_month_peak_inital_values,
            sizeof(md_nkn_connection_month_peak_inital_values ) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_connection_week_avg_inital_values,
            sizeof(md_nkn_connection_week_avg_inital_values) / sizeof(bn_str_value));
       bail_error(err);

       err = mdb_create_node_bindings
           (NULL, inout_new_db, md_nkn_connection_week_peak_inital_values,
            sizeof(md_nkn_connection_week_peak_inital_values) / sizeof(bn_str_value));
       bail_error(err);
    }

    if (from_module_version == 4 && to_module_version == 5) {
       for ( i = 0 ; i < sizeof(alarm_cfg_entries)/sizeof(struct alarm_config) ; i++)
       {
           if (alarm_cfg_entries[i].name != NULL)
           {
               node_pattern = smprintf("/stats/config/alarm/%s",alarm_cfg_entries[i].name);
               if (alarm_cfg_entries[i].enable != NULL)
               {
                   err = mdb_set_node_str
			(NULL,inout_new_db,bsso_modify,0,bt_bool,alarm_cfg_entries[i].enable,"%s/enable",node_pattern);
	       }
	   }
       }
    }
    else if (from_module_version == 5 && to_module_version == 6) {
	    md_nkn_stats_add_sample_cfg(NULL, inout_new_db, 0 ,0);
	    md_nkn_stats_add_chd_cfg(NULL, inout_new_db , 0 ,0);

    }
    else if (from_module_version == 6 && to_module_version == 7) {
	    md_nkn_stats_add_sample_cfg(NULL, inout_new_db, 1 ,1);
    }
    /*Remove the old nodes for perdiskbytes since the sample/node value has changed*/
    else if (from_module_version == 7 && to_module_version == 8) {
	const char *binding = NULL;
	binding = smprintf("/stats/config/sample/%s", "perdiskbytes");
        err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
	                           0, bt_string, "", "%s", binding);
           bail_error(err);
	    md_nkn_stats_add_sample_cfg(NULL, inout_new_db, 15, 15);
    } else if (from_module_version == 8 && to_module_version == 9) {
	   /*
	    * Since the  sample related to  BW has been commented out , the sample index for
	    * perdiskbytes has changed from 15 to 13.Hence the nodes are not getting created.
	    * Changing the node index for the function md_nkn_stats_add_sample_cfg
	    * If we are going to add new samples , please add it at the end of the sample array.
	    */

	    md_nkn_stats_add_sample_cfg(NULL, inout_new_db, 13, 13);
    } else if (to_module_version == 11) {
	/* change sampling node to use %age instead of absolute numbersi delete following nodes */
	///* /stats/config/sample/resource_pool/node/\/nkn\/nvsd\/resource_mgr\/monitor\/counter\/*\/used\/* */
	node_name_t node_name = {0}, node_value = {0};

	snprintf(node_name, sizeof(node_name), "/stats/config/sample/resource_pool/node/"
		"\\/nkn\\/nvsd\\/resource_mgr\\/monitor\\/state\\/*\\/used\\/p_age\\/*");
	snprintf(node_value, sizeof(node_value), "/nkn/nvsd/resource_mgr/monitor/state/*/used/p_age/*");
	err = mdb_set_node_str(NULL, inout_new_db, bsso_create, 0, bt_name, node_value,
		"%s", node_name);
	bail_error(err);

	snprintf(node_name, sizeof(node_name), "/stats/config/sample/resource_pool/node/"
		"\\/nkn\\/nvsd\\/resource_mgr\\/monitor\\/counter\\/*\\/used\\/*");
	err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_none, NULL,
		"%s", node_name);
	bail_error(err);
    } else if (to_module_version == 12) {
	/* Change the sample method of num_of_connections and http_transaction_count from "direct" to "delta" */

	err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_string, "delta",
		"%s", "/stats/config/sample/num_of_connections/sample_method");
	bail_error(err);

	err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0, bt_string, "delta",
		"%s", "/stats/config/sample/http_transaction_count/sample_method");
	bail_error(err);
	
   } else if(to_module_version == 13) {
	
	node_name_t change_alarms[6] = {"total_byte_rate", "connection_rate", "cache_byte_rate",
					"origin_byte_rate", "disk_byte_rate", "http_transaction_rate"};

	node_name_t new_trig_ids[6] = {"mon_total_byte_rate", "mon_connection_rate", "mon_cache_byte_rate",
				       "mon_origin_byte_rate", "mon_disk_byte_rate", "mon_http_transaction_rate"};

	node_name_t del_alarms[3] = {"avg_cache_byte_rate", "avg_disk_byte_rate", "avg_origin_byte_rate"};

	/*Alarms should be triggered only if the value stays above the threshold for specific period.
	So, adding new monitoring chds for a few existing chds */
        err = md_nkn_stats_add_chd_cfg(NULL, inout_new_db , 34 ,39);
	bail_error(err);

	/*Changing the alarm configuration to watch the new monitoring chds */

	for ( i = 0 ; i <=5 ; i++ ) {
            err = mdb_set_node_str
                 (NULL, inout_new_db, bsso_modify, 0, bt_string, new_trig_ids[i], "/stats/config/alarm/%s/trigger/id", change_alarms[i]);
            bail_error(err);
	}	

	/* Removing the avg_cache_byte_rate, avg_origin_byte_rate and avg_disk_byte_rate alarms */
	for ( i = 0 ; i < 3 ; i++ ) {
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_none, NULL, "/stats/config/alarm/%s", del_alarms[i]);
	    bail_error(err);
	}	
   } else if(to_module_version == 14) {

	/* Removing the nkn_cpu_util_ave notification */	

	node_name_t del_events[] = { "/email/notify/events/\\/stats\\/events\\/nkn_cpu_util_ave\\/rising\\/error",
				     "/email/notify/events/\\/stats\\/events\\/nkn_cpu_util_ave\\/rising\\/clear"};

	node_name_t mod_chds[] = {"mon_total_byte_rate", "mon_connection_rate", "mon_cache_byte_rate",
				   "mon_origin_byte_rate", "mon_disk_byte_rate", "mon_http_transaction_rate"};

	err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_none, NULL, "/stats/config/alarm/nkn_cpu_util_ave");
	bail_error(err);

	for ( i = 0 ; i < sizeof(del_events)/sizeof(node_name_t) ; i++ ) {
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_none, NULL, "%s", del_events[i]);
	    bail_error(err);
	}

	/* Setting num_to_cache for new chds to comply with Samara FIR */	
	for ( i = 0 ; i < sizeof(mod_chds)/sizeof(node_name_t) ; i++ ) {
            err = mdb_set_node_str
                 (NULL, inout_new_db, bsso_modify, 0, bt_int32, "-1", "/stats/config/chd/%s/num_to_cache", mod_chds[i]);
            bail_error(err);
	}	
   } else if(to_module_version == 15) {

	/*Removing perdiskbyte_rate and peroriginbyte_rate alarms */
	node_name_t del_alarms[] = {"perdiskbyte_rate", "peroriginbyte_rate"};

	for ( i = 0 ; i < sizeof(del_alarms)/sizeof(node_name_t) ; i++ ) {
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_none, NULL, "/stats/config/alarm/%s", del_alarms[i]);
	    bail_error(err);
	}	

   } else if(to_module_version == 16) {

	/* Removing unwanted samples, chds and alarms */
	node_name_t del_samples[] = { "total_cache_byte_count", "total_disk_byte_count", "total_origin_byte_count",
				      "perportbytes", "peroriginbytes", "perdiskbytes" };	
	node_name_t del_chds[] = { "avg_cache_byte_rate", "avg_origin_byte_rate", "avg_disk_byte_rate",
				   "perportbyte_rate", "peroriginbyte_rate", "perdiskbyte_rate" };
	node_name_t del_alarms[] = { "perportbyte_rate", "total_byte_rate" };

	for ( i = 0 ; i < sizeof(del_samples)/sizeof(node_name_t) ; i++ ) {
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_none, NULL, "/stats/config/sample/%s", del_samples[i]);
	    bail_error(err);
	}
	for ( i = 0 ; i < sizeof(del_chds)/sizeof(node_name_t) ; i++ ) {
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_none, NULL, "/stats/config/chd/%s", del_chds[i]);
	    bail_error(err);
	}
	for ( i = 0 ; i < sizeof(del_alarms)/sizeof(node_name_t) ; i++ ) {
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 0, bt_none, NULL, "/stats/config/alarm/%s", del_alarms[i]);
	    bail_error(err);
	}
   }

 bail:
    return(err);
}



static int
md_get_proc_name(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *procname = NULL;

    //lc_log_basic(LOG_NOTICE, "node_name : %s", node_name);

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    num_parts = tstr_array_length_quick(parts);
    bail_require(num_parts == 4);

    procname = tstr_array_get_str_quick(parts, 3);
    bail_null(procname);

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string, 0, procname);
    bail_error_null(err, *ret_binding);

    //err = bn_binding_dump(LOG_NOTICE, "procname", *ret_binding);
    //bail_error(err);

bail:
    tstr_array_free(&parts);
    return err;
}

/*
 * Monitor only the following processes
 *  - nvsd
 *  - nknlogd
 *  - mgmtd
 *  - statsd
 *  - nkndashboard
 *  - nknoomgr
 *  - nknvpemgr
 *  - httpd
 *  - nkncache_evictd
 */
static int
md_iterate_proc_name(
        md_commit *commit,
        const mdb_db *db,
        const char *parent_node_name,
        const uint32_array *node_attrib_ids,
        int32 max_num_nodes,
        int32 start_child_num,
        const char *get_next_child,
        mdm_mon_iterate_names_cb_func iterate_cb,
        void *iterate_cb_arg,
        void *arg)
{
    int err = 0;
    uint32 i = 0;
    uint32 num_bindings = 0;
    char *name = NULL;
    tstr_array *names_array = NULL;
    const char *proc_names[] = {"nvsd", "nknlogd", "mgmtd", "statsd", "nkndashboard", "nknoomgr", "nknvpemgr", "httpd", "nkncache_evictd"};

    err = tstr_array_new(&names_array, NULL);
    bail_error(err);

    for(i = 0; i < sizeof(proc_names)/sizeof(const char*); ++i) {
        err = tstr_array_append_str(names_array, proc_names[i]);
        bail_error(err);

        err = (*iterate_cb)(commit, db, proc_names[i], iterate_cb_arg);
        bail_error(err);
    }


bail:
    tstr_array_free(&names_array);
    return err;
}



/*
 * Launch our helper script and grab the value
 */
static int
md_get_proc_mem(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    const char *vm_name = (const char *) arg;
    tstring *procname = NULL;
    char *pm_node = NULL;
    tbool found = false;
    tstring *pid = NULL;
    tstring *ret_output = NULL;
    int32 status = 0;
    uint32 value = 0;


    bail_require(vm_name);

    /* Get the PID for this process */
    err = bn_binding_name_to_parts_va(node_name, false, 1, 3, &procname);
    bail_error_null(err, procname);

    /* Invoke the script passing 0 for pid, vmname and proc name */
    err = lc_launch_quick_status(&status, &ret_output, false, 4,
    			"/opt/nkn/bin/mem.sh", "0",
			vm_name, ts_str(procname));
    bail_error(err);

    /* Get the value from the output */
    if (ret_output) {
        value = atoi(ts_str(ret_output));
    }

    err = bn_binding_new_uint32(ret_binding, node_name, ba_value, 0, value);
    bail_error(err);

bail:
    safe_free(pm_node);
    ts_free(&procname);
    ts_free(&ret_output);
    ts_free(&pid);
    return err;
}

static int md_backup_file(const char *path,
		const char * dir_path,
                const char * file_name,
		uint32_t files_to_keep)
{
    int err = 0;
    tstring *file = NULL;
    const char *date;
    const struct tm *tm;
    time_t now;
    char tmp[64];
    char *destname = NULL;
    tbool exists = false;
    lf_cleanup_context *ctxt = NULL;

    err = lf_path_last(path, &file);
    bail_error_null(err, file);

    if ( (now = time(NULL)) == (time_t)-1 || (tm = localtime(&now)) == NULL) {
	goto bail ;
    };
    strftime(tmp, sizeof(tmp), "%y%m%d_%H%M%S", tm);

    err = ts_insert_str(file, ts_length(file) - 4 , tmp);
    bail_error(err);

    destname = smprintf("%s/%s", dir_path, ts_str(file));
    bail_error_null(err, destname);

    err = lf_test_exists(path, &exists);
    bail_error(err);

    err = lf_cleanup_get_context(dir_path, &ctxt);
    bail_error_null(err, ctxt);

    err = lf_cleanup_leave_n_files
	(ctxt, file_name, files_to_keep, NULL );
    bail_error(err);

    if (exists) {
	lc_log_basic(LOG_DEBUG, " File Exist : %s", path);
	lc_log_basic(LOG_DEBUG, "filename : %s dirname : %s file : %s", ts_str(file), dir_path, ts_str(file));
	lf_rename_file(path , destname);
    }
    else {
	lc_log_basic(LOG_DEBUG, "File Not Exist : %s", path);
    }
bail:
    ts_free(&file);
    safe_free(destname);
    return(err);
}

static int md_nvsd_get_reset_uptime(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    lt_time_sec old_time = 0, new_time = 0, uptime = 0;

    old_time = nkncnt_get_int32((char *)arg);
    new_time = lt_curr_time();
    uptime = new_time - old_time;
    err = bn_binding_new_time_sec(ret_binding,
	    node_name, ba_value, 0,uptime);
    bail_error(err);
bail:
    return err;
}

static int
md_get_total_bw(md_commit *commit,
 const mdb_db *db,
 const char *node_name,
 const bn_attrib_array *node_attribs,
 bn_binding **ret_binding,
 uint32 *ret_node_flags,
 void *arg)
{
    int err = 0;
    tstring *cval = NULL;
    tbool found = false;
    uint64 bw = 0;
    bn_binding *binding = NULL;
    uint64 bw_mbps = 0;
    const char *bname ="/stats/state/chd/intf_util/node/\\/net\\/interface\\/global\\/state\\/stats\\/rxtx\\/bytes_per_sec/last/value";

    err = mdb_get_node_value_tstr(commit, db, bname, 0, &found, &cval);
    bail_error(err);

    if(cval) {
        sscanf(ts_str(cval),"%lu",&bw);
        bw_mbps = bw * 8;
    }
    err = bn_binding_new_uint64(ret_binding, node_name, ba_value, 0, bw_mbps);
    bail_error(err);

bail:
    ts_free(&cval);
    return err;
}

/* Monitoring node handler function for aggregated bw for service interfaces including management interfaces */
static int
md_get_total_bw_service_intf(md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    tstr_array *rx_tx_tstr_bindings = NULL;
    uint16 i = 0;
    const char *binding_name = NULL;
    uint64 aggr_bw = 0;
    uint64 rx_bytes = 0;
    uint64 tx_bytes = 0;
    char *rx_node = NULL;
    char *tx_node = NULL;
    char *link_node = NULL;
    char *speed = NULL;
    tstring *t_speed = NULL;
    tstring *t_speed_str = NULL;
    tbool found = false;
    tbool link_up = false;
    tstring *t_intf_name = NULL;
    service_type_t serv_type = SERVICE_INTF_ALL;

    /* get if its for tx,rx or both*/
    const char *req_type = (char *)arg;

    if(strcmp(req_type, "total_http") == 0) {
	req_type = "total";
	serv_type = SERVICE_INTF_HTTP;

    }

    err = mdb_get_matching_tstr_array( commit,
	    db,
	    "/net/interface/state/*",
	    0,
	    &rx_tx_tstr_bindings);
    bail_error_null(err, rx_tx_tstr_bindings);

    for( i = 0; i < tstr_array_length_quick(rx_tx_tstr_bindings) ; i++)
    {

	const char* node_last_name = NULL;
	binding_name = tstr_array_get_str_quick(
		rx_tx_tstr_bindings, i);

	err = get_node_last_part(binding_name, &t_intf_name);
	bail_error_null(err, t_intf_name);

	node_last_name = ts_str_maybe(t_intf_name);
	link_node = smprintf("%s/flags/link_up", binding_name);
	bail_null(link_node);

	err = mdb_get_node_value_tbool(commit, db, link_node, 0,
		&found, &link_up);
	safe_free(link_node);
	ts_free(&t_intf_name);

	if(!link_up)
	    continue;

	if(node_last_name && is_deliv_intf(node_last_name, commit, db, serv_type)) {
	    if(!strcmp(req_type, "txrx")) {

		rx_node = smprintf("%s/stats/rx/bytes", binding_name);
		tx_node = smprintf("%s/stats/tx/bytes", binding_name);

		err = mdb_get_node_value_uint64(commit, db,
			rx_node, 0,
			NULL, &rx_bytes);
		bail_error(err);

		err = mdb_get_node_value_uint64(commit, db,
			tx_node, 0,
			NULL, &tx_bytes);
		bail_error(err);

		aggr_bw += (rx_bytes + tx_bytes);

	    }
	    else if(!strcmp(req_type, "rx")) {

		rx_node = smprintf("%s/stats/rx/bytes", binding_name);

		err = mdb_get_node_value_uint64(commit, db,
			rx_node, 0,
			NULL, &rx_bytes);
		bail_error(err);

		aggr_bw += (rx_bytes);

	    }
	    else if(!strcmp(req_type, "tx")) {

		tx_node = smprintf("%s/stats/tx/bytes", binding_name);

		err = mdb_get_node_value_uint64(commit, db,
			tx_node, 0,
			NULL, &tx_bytes);
		bail_error(err);

		aggr_bw += (tx_bytes);

	    }
	    safe_free(rx_node);
	    safe_free(tx_node);
	    safe_free(speed);
	    ts_free(&t_speed);
	    ts_free(&t_speed_str);
	}
	ts_free(&t_intf_name);
    }// end of for

    if(strcmp(req_type, "total")) {
	//Covert bytes to BITS here
	aggr_bw = aggr_bw * 8;
    } else {
	//For Total Service BW convert Mb/s into b/s
	aggr_bw = aggr_bw * 1000;
    }

    err = bn_binding_new_uint64(ret_binding, node_name, ba_value, 0, aggr_bw);
    bail_error(err);
bail:
    safe_free(rx_node);
    safe_free(tx_node);
    safe_free(link_node);
    safe_free(speed);
    ts_free(&t_intf_name);
    ts_free(&t_speed);
    ts_free(&t_speed_str);
    tstr_array_free(&rx_tx_tstr_bindings);
    return	err;
}

int
get_node_last_part(const char *full_node, tstring **last_name)
{
    int err = 0;
    tstr_array *name_parts = NULL;
    uint32 num_parts = 0;
    const char *last_name_part = NULL;

    bail_null(full_node);

    err = ts_tokenize_str(full_node, '/', '\0', '\0', 0, &name_parts);
    bail_error(err);

    num_parts = tstr_array_length_quick(name_parts);
    last_name_part = tstr_array_get_str_quick (name_parts, num_parts - 1);

    err = ts_new_str(last_name, last_name_part);
    bail_error(err);
bail:
    tstr_array_free(&name_parts);
    return err;

}

static tbool
is_deliv_intf(const char *intf_node,
		md_commit *commit,
		const mdb_db *db,
		service_type_t service_type)
{
    int err = 0;
    tstr_array *http_listen_intf = NULL;
    tstr_array *rtsp_listen_intf = NULL;
    tbool found = false;
    tstring *t_intf_name = NULL;
    const char *interface_name = NULL;
    uint32 i = 0;

    /* Dont consider loopback and sit0 interfaces*/
    if(!strcmp(intf_node, "lo") || !strcmp(intf_node, "sit0")) {
	goto bail;
    }

    /* look for http delivery interfaces */
    if(service_type == SERVICE_INTF_HTTP || service_type == SERVICE_INTF_ALL) {
	err = mdb_get_matching_tstr_array( commit,
		db,
		"/nkn/nvsd/http/config/interface/*",
		0,
		&http_listen_intf);
	bail_error_null(err, http_listen_intf);

	/* if the array is empty then all interface are to be used as service interfaces */
	if(!tstr_array_length_quick(http_listen_intf)) {
	    found = true;
	    goto bail;
	}

	for(i = 0; i < tstr_array_length_quick(http_listen_intf) ; i++ ) {
	    const char *last_name_part = NULL;

	    interface_name = tstr_array_get_str_quick(http_listen_intf, i);
	    err = get_node_last_part(interface_name, &t_intf_name);
	    bail_error_null(err, t_intf_name);
	    last_name_part = ts_str_maybe(t_intf_name);
	    if(last_name_part && !strcmp(last_name_part,intf_node)) {
		found = true;
		goto bail;

	    }
	    ts_free(&t_intf_name);
	}
    }
    /* check if interface is part of rtsp delivery*/
    if(service_type == SERVICE_INTF_RTSP || service_type == SERVICE_INTF_ALL ) {
	err = mdb_get_matching_tstr_array( commit,
		db,
		"/nkn/nvsd/rtstream/config/interface/*",
		0,
		&rtsp_listen_intf);
	bail_error_null(err, rtsp_listen_intf);

	if(!tstr_array_length_quick(rtsp_listen_intf)) {
	    found = true;
	    goto bail;
	}

	for(i = 0; i < tstr_array_length_quick(rtsp_listen_intf) ; i++ ) {
	    const char *last_name_part = NULL;
	    interface_name = tstr_array_get_str_quick(rtsp_listen_intf, i);
	    err = get_node_last_part(interface_name, &t_intf_name);
	    bail_error_null(err, t_intf_name);
	    last_name_part = ts_str_maybe(t_intf_name);
	    if(last_name_part && !strcmp(last_name_part,intf_node)) {
		found = true;
		goto bail;
	    }
	    ts_free(&t_intf_name);
	}
    }

bail:
    ts_free(&t_intf_name);
    tstr_array_free(&http_listen_intf);
    tstr_array_free(&rtsp_listen_intf);
    return found;
}

/*This function shouldn't be used for adding samples anymore.  The array sample_cfg_entries is deprecated */
static int
md_nkn_stats_add_sample_cfg(md_commit *commit,
		mdb_db *db,
		int start_index,
		int end_index)
{
    int err = 0;
    char *node_pattern = NULL;
    char *sample_node = NULL;
    tstr_array *src_nodes = NULL;
    bn_binding *binding = NULL;
    int i = 0;
    uint32 j = 0;
    const char *src = NULL;
    char *node_esc = NULL;

    uint32 num_src = 0 ;
    for ( i = start_index ; i <= end_index  ; i++)
    {
	if (sample_cfg_entries[i].name != NULL)
	{
	    node_pattern = smprintf("/stats/config/sample/%s",sample_cfg_entries[i].name);
	    err = mdb_set_node_str
		(commit,db,bsso_create,0,bt_string,sample_cfg_entries[i].name,"%s",node_pattern);
	    bail_error(err);
	    err = mdb_set_node_str
		(commit,db,bsso_create,0,bt_string,sample_cfg_entries[i].label,"/stat/nkn/stats/%s/label",sample_cfg_entries[i].name);
	    bail_error(err);
	    err = mdb_set_node_str
		(commit,db,bsso_create,0,bt_string,sample_cfg_entries[i].unit,"/stat/nkn/stats/%s/unit",sample_cfg_entries[i].name);
	    bail_error(err);
	    if(sample_cfg_entries[i].enable != NULL)
	    {
		err = mdb_set_node_str
		    (commit,db,bsso_modify,0,bt_bool,sample_cfg_entries[i].enable,"%s/enable",node_pattern);
		bail_error(err);
	    }
	    /*! support multiple patterns */
	    if (sample_cfg_entries[i].node != NULL)
	    {
		err = ts_tokenize_str(sample_cfg_entries[i].node,',','\0','\0',0,&src_nodes);
		bail_error_null(err,src_nodes);
		num_src = tstr_array_length_quick(src_nodes);
		for ( j = 0 ; j < num_src ; j++)
		{
		    src = tstr_array_get_str_quick(src_nodes,j);
		    bail_null(src);
		    err = bn_binding_name_escape_str(src,&node_esc);
		    bail_error_null(err,node_esc);

		    err = mdb_set_node_str
			(commit,db,bsso_create,0,bt_name,src,"%s/node/%s",node_pattern,node_esc);
		    safe_free(node_esc);
		}
		tstr_array_free(&src_nodes);
	    }
	    sample_node = smprintf("%s/interval",node_pattern);
	    err = bn_binding_new_duration_sec(&binding,sample_node,ba_value,0,sample_cfg_entries[i].interval);
	    bail_error(err);
	    err = mdb_set_node_binding(commit,db,bsso_modify,0,binding);
	    bail_error(err);
	    bn_binding_free(&binding);
	    safe_free(sample_node);

	    sample_node = smprintf("%s/num_to_keep",node_pattern);
	    err = bn_binding_new_uint32(&binding,sample_node,ba_value,0,sample_cfg_entries[i].num_to_keep);
	    bail_error(err);
	    err = mdb_set_node_binding(commit,db,bsso_modify,0,binding);
	    bail_error(err);
	    bn_binding_free(&binding);
	    safe_free(sample_node);

	    if(sample_cfg_entries[i].sample_method != NULL)
	    {
		err = mdb_set_node_str
		    (commit,db,bsso_modify,0,bt_string,sample_cfg_entries[i].sample_method,"%s/sample_method",node_pattern);
		bail_error(err);
	    }
	    if(sample_cfg_entries[i].delta_constraint != NULL)
	    {
		err = mdb_set_node_str
		    (commit,db,bsso_modify,0,bt_string,sample_cfg_entries[i].delta_constraint,"%s/delta_constraint",node_pattern);
		bail_error(err);
	    }

	    sample_node = smprintf("%s/sync_interval",node_pattern);
	    err = bn_binding_new_uint32(&binding,sample_node,ba_value,0,sample_cfg_entries[i].sync_interval);
	    bail_error(err);
	    err = mdb_set_node_binding(commit,db,bsso_modify,0,binding);
	    bail_error(err);
	    bn_binding_free(&binding);
	    safe_free(sample_node);

	    /* Setting num_to_cache for samples to comply with Samara FIR */	
	    sample_node = smprintf("%s/num_to_cache", node_pattern);
	    err = mdb_set_node_str
		(commit, db, bsso_modify, 0, bt_int32, "-1", "%s", sample_node);
	    bail_error(err);
	    safe_free(sample_node);


	    safe_free(node_pattern);
	}

    }
bail:
    safe_free(node_pattern);
    safe_free(src_nodes);
    safe_free(sample_node);
    bn_binding_free(&binding);
    return err;
}

/*This function shouldn't be used for adding chds anymore. The array chd_cfg_entries is deprecated */
static int
md_nkn_stats_add_chd_cfg(md_commit *commit,
			mdb_db *db,
			int start_index,
			int end_index)
{
    int err = 0;
    char *node_pattern = NULL;
    char *sample_node = NULL;
    tstr_array *src_nodes = NULL;
    bn_binding *binding = NULL;
    int i = 0, j = 0;
    const char *src = NULL;
    uint32 src_cnt = 0 ;
    char *node_esc = NULL;

    /*! CHD Initialization */
    for ( i = start_index ; i <= end_index ; i++ ) {
	if (chd_cfg_entries[i].name != NULL)
	{
	    node_pattern = smprintf("/stats/config/chd/%s",chd_cfg_entries[i].name);
	    err = mdb_set_node_str
		(commit,db,bsso_create,0,bt_string,chd_cfg_entries[i].name,"%s",node_pattern);
	    bail_error(err);
	    err = mdb_set_node_str
		(commit,db,bsso_create,0,bt_string,chd_cfg_entries[i].label,"/stat/nkn/stats/%s/label",chd_cfg_entries[i].name);
	    bail_error(err);
	    err = mdb_set_node_str
		(commit,db,bsso_create,0,bt_string,chd_cfg_entries[i].unit,"/stat/nkn/stats/%s/unit",chd_cfg_entries[i].name);
	    bail_error(err);

	    if (chd_cfg_entries[i].enable != NULL)
	    {
		err = mdb_set_node_str
		    (commit,db,bsso_modify,0,bt_bool,chd_cfg_entries[i].enable,"%s/enable",node_pattern);
		bail_error(err);
	    }
	    if (chd_cfg_entries[i].source_type != NULL)
	    {
		err = mdb_set_node_str
		    (commit,db,bsso_modify,0,bt_string,chd_cfg_entries[i].source_type,"%s/source/type",node_pattern);
		bail_error(err);
	    }
	    if (chd_cfg_entries[i].source_id != NULL)
	    {
		err = mdb_set_node_str
		    (commit,db,bsso_modify,0,bt_string,chd_cfg_entries[i].source_id,"%s/source/id",node_pattern);
		bail_error(err);
	    }
	    /*! Read all source node configuraions */
	    if (chd_cfg_entries[i].source_node != NULL)
	    {
		src_cnt = 0;
		while (chd_cfg_entries[i].source_node[src_cnt] != NULL)
		{
		    src = chd_cfg_entries[i].source_node[src_cnt];
		    bail_null(src);
		    err = bn_binding_name_escape_str(src,&node_esc);
		    bail_error_null(err,node_esc);

		    err = mdb_set_node_str
			(commit,db,bsso_create,0,bt_name,src,"%s/source/node/%s",node_pattern,node_esc);
		    safe_free(node_esc);
		    src_cnt++;
		}
		tstr_array_free(&src_nodes);
	    }
	    sample_node = smprintf("%s/num_to_keep",node_pattern);
	    err = bn_binding_new_uint32(&binding,sample_node,ba_value,0,chd_cfg_entries[i].num_to_keep);
	    bail_error(err);
	    err = mdb_set_node_binding(commit,db,bsso_modify,0,binding);
	    bail_error(err);
	    bn_binding_free(&binding);
	    safe_free(sample_node);

	    if (chd_cfg_entries[i].select_type != NULL)
	    {
		err = mdb_set_node_str
		    (commit,db,bsso_modify,0,bt_string,chd_cfg_entries[i].select_type,"%s/select_type",node_pattern);
		bail_error(err);
	    }

	    sample_node = smprintf("%s/instances/num_to_use",node_pattern);
	    err = bn_binding_new_uint32(&binding,sample_node,ba_value,0,chd_cfg_entries[i].inst_num_to_use);
	    bail_error(err);
	    err = mdb_set_node_binding(commit,db,bsso_modify,0,binding);
	    bail_error(err);
	    bn_binding_free(&binding);
	    safe_free(sample_node);

	    sample_node = smprintf("%s/instances/num_to_advance",node_pattern);
	    err = bn_binding_new_uint32(&binding,sample_node,ba_value,0,chd_cfg_entries[i].inst_num_to_advance);
	    bail_error(err);
	    err = mdb_set_node_binding(commit,db,bsso_modify,0,binding);
	    bail_error(err);
	    bn_binding_free(&binding);
	    safe_free(sample_node);

	    /* Setting num_to_cache for chds to comply with Samara FIR */	
	    sample_node = smprintf("%s/num_to_cache", node_pattern);
	    err = mdb_set_node_str
		(commit, db, bsso_modify, 0, bt_int32, "-1", "%s", sample_node);
	    bail_error(err);
	    safe_free(sample_node);

	    sample_node = smprintf("%s/time/interval_length",node_pattern);
	    err = bn_binding_new_duration_sec(&binding,sample_node,ba_value,0,chd_cfg_entries[i].time_interval_length);
	    bail_error(err);
	    err = mdb_set_node_binding(commit,db,bsso_modify,0,binding);
	    bail_error(err);
	    bn_binding_free(&binding);
	    safe_free(sample_node);

	    sample_node = smprintf("%s/time/interval_distance",node_pattern);
	    err = bn_binding_new_duration_sec(&binding,sample_node,ba_value,0,chd_cfg_entries[i].time_interval_distance);
	    bail_error(err);
	    err = mdb_set_node_binding(commit,db,bsso_modify,0,binding);
	    bail_error(err);
	    bn_binding_free(&binding);
	    safe_free(sample_node);

	    sample_node = smprintf("%s/time/interval_phase",node_pattern);
	    err = bn_binding_new_duration_sec(&binding,sample_node,ba_value,0,chd_cfg_entries[i].time_interval_phase);
	    bail_error(err);
	    err = mdb_set_node_binding(commit,db,bsso_modify,0,binding);
	    bail_error(err);
	    bn_binding_free(&binding);
	    safe_free(sample_node);

	    if (chd_cfg_entries[i].function != NULL)
	    {
		err = mdb_set_node_str
		    (commit,db,bsso_modify,0,bt_string,chd_cfg_entries[i].function,"%s/function",node_pattern);
		bail_error(err);
	    }

	    sample_node = smprintf("%s/sync_interval",node_pattern);
	    err = bn_binding_new_uint32(&binding,sample_node,ba_value,0,chd_cfg_entries[i].sync_interval);
	    bail_error(err);
	    err = mdb_set_node_binding(commit,db,bsso_modify,0,binding);
	    bail_error(err);
	    bn_binding_free(&binding);
	    safe_free(sample_node);

	    if (chd_cfg_entries[i].calc_partial != NULL)
	    {
		err = mdb_set_node_str
		    (commit,db,bsso_modify,0,bt_bool,chd_cfg_entries[i].calc_partial,"%s/calc_partial",node_pattern);
		bail_error(err);
	    }
	    if (chd_cfg_entries[i].alarm_partial != NULL)
	    {
		err = mdb_set_node_str
		    (commit,db,bsso_modify,0,bt_bool,chd_cfg_entries[i].alarm_partial,"%s/alarm_partial",node_pattern);
		bail_error(err);
	    }
	    safe_free(node_pattern);
	}
    }
bail:
    safe_free(node_pattern);
    tstr_array_free(&src_nodes);
    safe_free(sample_node);
    bn_binding_free(&binding);
    return err;
}


static int
md_nkn_wc_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    tstring *t_str = NULL;
    char *counter_name = NULL;
    uint64_t n = 0;
    const char *var_name = (const char *) arg;

    bail_null(arg);

    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 3, &t_str);
    bail_error_null(err, t_str);

    counter_name = smprintf(var_name, ts_str(t_str));
    bail_null(counter_name);

    // read the counter value
    n = nkncnt_get_uint64((char *) counter_name);
    // create a binding to hold the counter value.
    err = bn_binding_new_uint64(ret_binding, node_name, ba_value, 0, n);
    bail_error(err);

bail:
    ts_free(&t_str);
    safe_free(counter_name);
    return err;
}

static int
md_nkn_namespace_dm2_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    tstring *t_str = NULL;
    tstring *t_ns_uid = NULL;
    uint64_t n = 0;
    node_name_t uid_nd = {0};
    str_value_t counter_name = {0};

    bail_null(arg);
    const char *var_name = (const char *) arg;

    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 3, &t_str);
    bail_error_null(err, t_str);

    snprintf(uid_nd, sizeof(uid_nd), "/nkn/nvsd/namespace/%s/uid",
	    ts_str(t_str));

    err = mdb_get_node_value_tstr(NULL, db,
	    uid_nd,
	    0,
	    NULL,
	    &t_ns_uid);
    bail_error(err);

    // read the counter value
    if(ts_length(t_ns_uid) > 0) {
	//Ignore the "/" in namespace uid
	snprintf(counter_name, sizeof(counter_name), var_name, (ts_str(t_ns_uid)) + 1);

	n = nkncnt_get_uint64(counter_name);
    }
    // create a binding to hold the counter value.
    err = bn_binding_new_uint64(ret_binding, node_name, ba_value, 0, n);
    bail_error(err);

bail:
    ts_free(&t_str);
    ts_free(&t_ns_uid);
    return err;
}

static int
md_get_http_client_response(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    char *resp = ((char *)arg);
    uint64 val = 0;

    switch(resp[0]) {
	case '2':
	    {
		uint64 r_200 = nkncnt_get_uint64("glob_http_tot_200_resp");
		uint64 r_206 = nkncnt_get_uint64("glob_http_tot_206_resp");
		val = r_200 + r_206;
	    }
	    break;
	case '3':
	    {
		uint64 r_302 =  nkncnt_get_uint64("glob_http_tot_302_resp");
		uint64 r_304 = nkncnt_get_uint64("glob_http_tot_304_resp");
		val = r_302 + r_304;
	    }
	    break;
	case '4':
	    {
		uint64 r_400 = nkncnt_get_uint64("glob_http_tot_400_resp");
		uint64 r_403 = nkncnt_get_uint64("glob_http_tot_403_resp");
		uint64 r_404 = nkncnt_get_uint64("glob_http_tot_404_resp");
		uint64 r_405 = nkncnt_get_uint64("glob_http_tot_405_resp");
		uint64 r_413 = nkncnt_get_uint64("glob_http_tot_413_resp");
		uint64 r_414 = nkncnt_get_uint64("glob_http_tot_414_resp");
		uint64 r_416 = nkncnt_get_uint64("glob_http_tot_416_resp");
		val  = r_400 + r_403 + r_404 + r_405
		    + r_413 + r_414 + r_416;
	    }
	    break;
	case  '5':
	    {
		uint64 r_500 = nkncnt_get_uint64("glob_http_tot_500_resp");
		uint64 r_501 = nkncnt_get_uint64("glob_http_tot_501_resp");
		uint64 r_502 = nkncnt_get_uint64("glob_http_tot_502_resp");
		uint64 r_503 = nkncnt_get_uint64("glob_http_tot_503_resp");
		uint64 r_504 = nkncnt_get_uint64("glob_http_tot_504_resp");
		uint64 r_505 = nkncnt_get_uint64("glob_http_tot_505_resp");
		/* XXX - check for overflow */
		val = r_500 + r_501 + r_502 + r_503
		    + r_504 + r_505;
	    }
	    break;
	default:
	    break;
    }

    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value, 0, val);
    bail_error(err);
bail:
    return err;

}
/*----------------------------------------------------------------------------
 * END OF FILE
 *---------------------------------------------------------------------------*/
static int
md_get_http_server_response(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    char *resp = ((char *)arg);
    uint64 val = 0;

    switch(resp[0]) {
	case '3':
	    {
		uint64 r_304 = nkncnt_get_uint64("glob_om_http_tot_304_resp");
		val =  r_304;
	    }
	    break;
	case '4':
	    {
		uint64 r_400 = nkncnt_get_uint64("glob_om_http_tot_400_resp");
		uint64 r_404 = nkncnt_get_uint64("glob_om_http_tot_404_resp");
		uint64 r_414 = nkncnt_get_uint64("glob_om_http_tot_414_resp");
		uint64 r_416 = nkncnt_get_uint64("glob_om_http_tot_416_resp");
		val  = r_400 + r_404 + r_414 + r_416;
	    }
	    break;
	case  '5':
	    {
		uint64 r_500 = nkncnt_get_uint64("glob_om_http_tot_500_resp");
		uint64 r_501 = nkncnt_get_uint64("glob_om_http_tot_501_resp");
		/* XXX - check for overflow */
		val = r_500 + r_501 ;
	    }
	    break;
	default:
	    break;
    }

    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value, 0, val);
    bail_error(err);
bail:
    return err;

}

static int
md_get_rtsp_client_response(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    char *resp = ((char *)arg);
    uint64 val = 0;

    switch(resp[0]) {
	case '2':
	    {
		uint64 r_200 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_200");
		uint64 r_201 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_201");
		uint64 r_250 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_250");
		val = r_200 + r_201 + r_250;
	    }
	    break;
	case '3':
	    {
		uint64 r_300 =  nkncnt_get_uint64("glob_rtsp_tot_status_resp_300");
		uint64 r_301 =  nkncnt_get_uint64("glob_rtsp_tot_status_resp_301");
		uint64 r_302 =  nkncnt_get_uint64("glob_rtsp_tot_status_resp_302");
		uint64 r_303 =  nkncnt_get_uint64("glob_rtsp_tot_status_resp_303");
		uint64 r_304 =  nkncnt_get_uint64("glob_rtsp_tot_status_resp_304");
		uint64 r_305 =  nkncnt_get_uint64("glob_rtsp_tot_status_resp_305");
		val = r_300 + r_301 + r_302 + r_303 + r_304 + r_305;
	    }
	    break;
	case '4':
	    {
		uint64 r_400 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_400");
		uint64 r_401 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_401");
		uint64 r_402 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_402");
		uint64 r_403 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_403");
		uint64 r_404 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_404");
		uint64 r_405 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_405");
		uint64 r_406 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_406");
		uint64 r_407 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_407");
		uint64 r_408 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_408");
		uint64 r_410 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_410");
		uint64 r_411 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_411");
		uint64 r_412 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_412");
		uint64 r_413 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_413");
		uint64 r_414 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_414");
		uint64 r_415 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_415");
		uint64 r_451 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_451");
		uint64 r_452 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_452");
		uint64 r_453 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_453");
		uint64 r_454 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_454");
		uint64 r_455 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_455");
		uint64 r_456 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_456");
		uint64 r_457 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_457");
		uint64 r_458 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_458");
		uint64 r_459 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_459");
		uint64 r_460 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_460");
		uint64 r_461 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_461");
		uint64 r_462 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_462");

		val = r_400 + r_401 + r_402 + r_403 + r_404
		    + r_405 + r_406 + r_407 + r_408 + r_410
		    + r_411 + r_412 + r_413 + r_414 + r_415
		    + r_451 + r_452 + r_453 + r_454 + r_455
		    + r_456 + r_457 + r_458 + r_459 + r_460
		    + r_461 + r_462;


	    }
	    break;
	case  '5':
	    {
		uint64 r_500 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_500");
		uint64 r_501 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_501");
		uint64 r_502 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_502");
		uint64 r_503 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_503");
		uint64 r_504 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_504");
		uint64 r_551 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_551");
		/* XXX - check for overflow */
		val = r_500 + r_501 + r_502 + r_503
		    + r_504 + r_551;
	    }
	    break;
	default:
	    break;
    }

    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value, 0, val);
    bail_error(err);
bail:
    return err;

}

static int
md_get_rtsp_server_response(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    char *resp = ((char *)arg);
    uint64 val = 0;

    switch(resp[0]) {
	case '2':
	    {
		uint64 r_200 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_200");
		uint64 r_201 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_201");
		uint64 r_250 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_250");
		val = r_200 + r_201 + r_250;
	    }
	    break;
	case '3':
	    {
		uint64 r_300 =  nkncnt_get_uint64("glob_rtsp_tot_status_resp_300");
		uint64 r_301 =  nkncnt_get_uint64("glob_rtsp_tot_status_resp_301");
		uint64 r_302 =  nkncnt_get_uint64("glob_rtsp_tot_status_resp_302");
		uint64 r_303 =  nkncnt_get_uint64("glob_rtsp_tot_status_resp_303");
		uint64 r_304 =  nkncnt_get_uint64("glob_rtsp_tot_status_resp_304");
		uint64 r_305 =  nkncnt_get_uint64("glob_rtsp_tot_status_resp_305");
		val = r_300 + r_301 + r_302 + r_303 + r_304 + r_305;
	    }
	    break;
	case '4':
	    {
		uint64 r_400 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_400");
		uint64 r_401 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_401");
		uint64 r_402 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_402");
		uint64 r_403 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_403");
		uint64 r_404 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_404");
		uint64 r_405 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_405");
		uint64 r_406 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_406");
		uint64 r_407 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_407");
		uint64 r_408 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_408");
		uint64 r_410 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_410");
		uint64 r_411 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_411");
		uint64 r_412 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_412");
		uint64 r_413 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_413");
		uint64 r_414 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_414");
		uint64 r_415 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_415");
		uint64 r_451 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_451");
		uint64 r_452 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_452");
		uint64 r_453 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_453");
		uint64 r_454 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_454");
		uint64 r_455 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_455");
		uint64 r_456 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_456");
		uint64 r_457 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_457");
		uint64 r_458 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_458");
		uint64 r_459 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_459");
		uint64 r_460 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_460");
		uint64 r_461 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_461");
		uint64 r_462 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_462");

		val = r_400 + r_401 + r_402 + r_403 + r_404
		    + r_405 + r_406 + r_407 + r_408 + r_410
		    + r_411 + r_412 + r_413 + r_414 + r_415
		    + r_451 + r_452 + r_453 + r_454 + r_455
		    + r_456 + r_457 + r_458 + r_459 + r_460
		    + r_461 + r_462;


	    }
	    break;
	case  '5':
	    {
		uint64 r_500 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_500");
		uint64 r_501 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_501");
		uint64 r_502 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_502");
		uint64 r_503 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_503");
		uint64 r_504 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_504");
		uint64 r_551 = nkncnt_get_uint64("glob_rtsp_tot_status_resp_551");
		/* XXX - check for overflow */
		val = r_500 + r_501 + r_502 + r_503
		    + r_504 + r_551;
	    }
	    break;
	default:
	    break;
    }

    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value, 0, val);
    bail_error(err);
bail:
    return err;

}

static int
md_get_http_server_conns(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    uint64 ipv4 =  nkncnt_get_uint64("glob_cp_tot_closed_sockets");
    uint64 ipv6 = nkncnt_get_uint64("glob_cp_tot_closed_sockets_ipv6");

    uint64 n = ipv4 + ipv6;
    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value, 0, n);
    bail_error(err);

bail:
    return err;
}

static int
md_get_http_server_open_conns(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    uint64 ipv4 =  nkncnt_get_uint64("glob_cp_tot_cur_open_sockets");
    uint64 ipv6 = nkncnt_get_uint64("glob_cp_tot_cur_open_sockets_ipv6");

    uint64 n = ipv4 + ipv6;
    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value, 0, n);
    bail_error(err);

bail:
    return err;
}

static int
md_get_http_client_open_conns(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    uint64 ipv4 =  nkncnt_get_uint64("glob_cur_open_http_socket");
    uint64 ipv6 = nkncnt_get_uint64("glob_cur_open_http_socket_ipv6");

    uint64 n = ipv4 + ipv6;
    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value, 0, n);
    bail_error(err);

bail:
    return err;
}

static int
md_get_namespace_config_string(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    tstring *ns_name = NULL, *str_value = NULL;
    const char *src_name= ( char *) arg;
    node_name_t node = {0};

    bail_null(src_name);

    err = bn_binding_name_to_parts_va(
	    node_name, false, 1, 3, &ns_name);
    bail_error_null(err,ns_name);

    snprintf(node, sizeof(node), "/nkn/nvsd/namespace/%s/%s",
	    ts_str(ns_name), src_name);

    err = mdb_get_node_value_tstr(commit, db, node, 0,
	    NULL, &str_value);
    bail_error(err);

    err = bn_binding_new_str(ret_binding,
	    node_name, ba_value, bt_string,0, ts_str(str_value));
    bail_error(err);
bail:
    ts_free(&ns_name);
    ts_free(&str_value);

    return err;
}

static int
md_get_namespace_config_bool(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    tstring *ns_name = NULL, *str_value = NULL;
    const char *src_name= ( char *) arg;
    node_name_t node = {0};

    bail_null(src_name);

    err = bn_binding_name_to_parts_va(
	    node_name, false, 1, 3, &ns_name);
    bail_error_null(err,ns_name);

    snprintf(node, sizeof(node), "/nkn/nvsd/namespace/%s/%s",
	    ts_str(ns_name), src_name);

    err = mdb_get_node_value_tstr(commit, db, node, 0,
	    NULL, &str_value);
    bail_error(err);

    err = bn_binding_new_str(ret_binding,
	    node_name, ba_value, bt_bool,0, ts_str(str_value));
    bail_error(err);

bail:
    ts_free(&ns_name);
    ts_free(&str_value);

    return err;
}

static int
md_nkn_wc_stats_get_uint32(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    tstring *t_str = NULL;
    char *counter_name = NULL;
    uint64_t n = 0;
    const char *var_name = (const char *) arg;

    bail_null(arg);

    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 3, &t_str);
    bail_error_null(err, t_str);

    counter_name = smprintf(var_name, ts_str(t_str));
    bail_null(counter_name);

    // read the counter value
    n = nkncnt_get_uint32((char *) counter_name);
    // create a binding to hold the counter value.
    err = bn_binding_new_uint32(ret_binding, node_name, ba_value, 0, n);
    bail_error(err);

bail:
    ts_free(&t_str);
    safe_free(counter_name);
    return err;
}

