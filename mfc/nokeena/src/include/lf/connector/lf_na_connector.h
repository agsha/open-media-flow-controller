#ifndef _LF_NA_CONNECTOR_H
#define _LF_NA_CONNECTOR_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

// nkn defs
#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nkncnt_client.h"
#include "interface.h"

// lf defs
#include "lf_limits.h"
#include "lf_common_defs.h"
#include "lf_stats.h"
#include "lf_err.h"

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* keeps emacs happy */
}
#endif

#define LF_NA_HTTP_MAX_RP 65
#define LF_NA_HTTP_MAX_VIP (MAX_NKN_ALIAS)
#define LF_NA_HTTP_SYSUP_TOLERANCE 30
#define LF_NA_HTTP_SYSUP_TIME ((90+LF_NA_HTTP_SYSUP_TOLERANCE)*1000)

enum lf_na_filter_id_t {
    LF_NA_RP_FILTER_ID = 0,
    LF_NA_NS_FILTER_ID,
    LF_NA_FILTER_ID_MAX
};

enum lf_na_http_mod_t {
    LF_NA_HTTP_MOD_MM = 0,
    LF_NA_HTTP_MOD_NW,
    LF_NA_HTTP_MOD_SCHED,
    LF_NA_HTTP_MOD_MAX
};

typedef enum lf_na_http_disk_tier {
    LF_NA_HTTP_DISK_TIER_SSD,
    LF_NA_HTTP_DISK_TIER_SAS,
    LF_NA_HTTP_DISK_TIER_SATA,
    LF_NA_HTTP_DISK_TIER_OTH,
    LF_NA_HTTP_DISK_TIER_MAX
} lf_na_http_disk_tier_t;

typedef struct tag_lf_na_http_rp_metrics {
    uint32_t avail;
    uint64_t bw_use;
    uint64_t bw_max;
    
    uint32_t active_conn;
    uint32_t active_conn_max;

    char name[64];
} lf_na_http_rp_metrics_t;

typedef struct tag_lf_http_vip_metrics {
    char name[64];
    uint32_t ip;
    uint64_t tot_sessions;
} lf_na_http_vip_metrics_t;

typedef struct tag_lf_na_http_glob_metrics {
    uint32_t size;
    uint32_t num_rp;
    uint32_t num_vip;
    uint32_t tps;
    uint32_t tps_max;
    double lf;
    int64_t ext_lf;
    int8_t preread_state[LF_NA_HTTP_DISK_TIER_MAX];
    double disk_lf[LF_NA_HTTP_DISK_TIER_MAX];
    double ram_lf;
} lf_na_http_glob_metrics_t;

typedef struct tag_lf_na_http_metrics_out {
    lf_na_http_glob_metrics_t gm;
    lf_na_http_rp_metrics_t rpm[LF_NA_HTTP_MAX_RP];
    lf_na_http_vip_metrics_t vipm[LF_NA_HTTP_MAX_VIP];
} lf_na_http_metrics_out_t;

typedef struct tag_lf_na_filter_out {
    char *counter_name_offset;
    struct cp_vec_list_t *filter_out[2];
}lf_na_filter_out_t;

typedef struct tag_lf_na_http_metrics_ctx {
    lf_na_http_glob_metrics_t gm;
    lf_na_http_rp_metrics_t rpm[LF_NA_HTTP_MAX_RP];
    lf_na_http_vip_metrics_t vipm[LF_NA_HTTP_MAX_VIP];
    
    char *rp_name[LF_NA_HTTP_MAX_RP];
    char *vip_name[LF_NA_HTTP_MAX_VIP];

    int32_t pid;
    uint64_t time_elapsed;
    uint64_t sampling_interval;
    pthread_mutex_t lock;
    cp_trie *rp_cache;
    uint32_t rp_cache_rev;
    struct pid_stat_list_t *sched_pid_list;
    struct pid_stat_list_t *mm_pid_list;
    struct pid_stat_list_t *nw_pid_list;
    ma_t http_conn_rate;
    uint32_t prev_tps;
    uint64_t cpu_clk_freq;
    int8_t is_ready;
    ma_t bw_use[LF_NA_HTTP_MAX_RP];
    ma_t ext_lf;
    uint32_t disk_get_q_count[LF_NA_HTTP_DISK_TIER_MAX];
    uint32_t disk_get_q_sac[LF_NA_HTTP_DISK_TIER_MAX];
} lf_na_http_metrics_ctx_t;

struct tag_lf_na_connector;

typedef struct tag_lf_na_connector {
    uint32_t num_apps;
    uint32_t sampling_interval;
    uint32_t window_len;
    uint32_t ext_window_len;
    uint32_t num_tokens;

    struct timeval last_sample_time;
    struct timeval curr_sample_time;
    
    nkncnt_client_ctx_t nc[LF_MAX_APPS];
    void *app_specific_metrics[LF_MAX_APPS];
    struct iovec *out[LF_MAX_APPS];
    pthread_mutex_t token_ref_cnt_lock;
    int32_t token_ref_cnt[LF_MAX_CLIENT_TOKENS];
    int32_t curr_token;
    lf_connector_get_app_count_fnp get_app_count;
    lf_connector_update_all_apps_fnp update_all_app_metrics;
    lf_connector_get_snapshot_fnp get_snapshot;
    lf_connector_get_filter_snapshot_fnp get_filter_snapshot;
    lf_connector_release_snapshot_fnp release_snapshot;
} lf_na_connector_t;

int32_t lf_na_connector_create(const lf_connector_input_params_t *const inp,
			       void **out);
int32_t lf_na_connector_cleanup(lf_na_connector_t *nac);
int32_t lf_cleanup_cp_vec_list(struct cp_vec_list_t *head);
#ifdef __cplusplus
}
#endif

#endif //_LF_NA_CONNECTOR_H
