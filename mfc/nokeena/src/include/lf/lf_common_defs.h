#ifndef _LF_COMMON_DEFS_H_
#define _LF_COMMON_DEFS_H_

#include <stdio.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "lf_limits.h"
#include "mfp/event_timer/mfp_event_timer.h"

// cprop defs
#include "cprops/collection.h"
#include "cprops/trie.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 /* keeps emacs happy */
}
#endif

#define LF_SYSTEM_SECTION 0
#define LF_VM_SECTION 1
#define LF_NA_SECTION 2

#define LF_NA_HTTP 0
#define LF_VMA_WMS 0
#define LF_SYS_GLOB 0

#define NUM_CPU_COLS (8)

typedef uint32_t (*lf_connector_get_app_count_fnp)	\
    (const void *);
typedef int32_t (*lf_connector_update_all_apps_fnp)	\
    (uint64_t sample_duration, const void *);
typedef int32_t (*lf_connector_get_snapshot_fnp)
    (void *const connector_ctx, 
     void **out, void **ref_id);
typedef int32_t (*lf_connector_get_filter_snapshot_fnp)
    (void *const connector_ctx,
     void **const filter, void **out, void **ref_id);
typedef int32_t (*lf_connector_release_snapshot_fnp)
    (void *connector_ctx, void *ref_id);

typedef struct tag_lf_ev_timer_ctx {
    event_timer_t *ev;
    pthread_t ev_thread;
}lf_ev_timer_ctx_t;

typedef struct tag_lf_app_list {
    uint32_t num_apps;
    const char *name[LF_MAX_APPS];
}lf_app_list_t;

typedef struct tag_lf_connector_input {
    lf_app_list_t *app_list;
    uint32_t sampling_interval;
    uint32_t window_len;
    uint32_t ext_window_len;
    uint32_t num_tokens;
    void *connector_specific_params;
}lf_connector_input_params_t;

typedef struct tag_pid_stat {
    int32_t pid;
    uint64_t curr_cpu[4];
    uint64_t prev_cpu[4];
    uint64_t tot_cpu;
    uint8_t first_flag;
} lf_pid_stat_t;

typedef struct tag_str_list_elm {
    char *name;
    TAILQ_ENTRY(tag_str_list_elm) entries;
} str_list_elm_t;
TAILQ_HEAD(str_list_t, tag_str_list_elm);

typedef struct tag_i32_list_elm {
    int32_t num;
    TAILQ_ENTRY(tag_i32_list_elm) entries;
} i32_list_elm_t;
TAILQ_HEAD(i32_list_t, tag_i32_list_elm);

typedef struct tag_pid_stat_list_elm {
    lf_pid_stat_t st;
    TAILQ_ENTRY(tag_pid_stat_list_elm) entries;
} pid_stat_list_elm_t;
TAILQ_HEAD(pid_stat_list_t, tag_pid_stat_list_elm);

typedef struct tag_cp_vec_list_elm {
    cp_vector *v;
    TAILQ_ENTRY(tag_cp_vec_list_elm) entries;
} cp_vec_list_elm_t;
TAILQ_HEAD(cp_vec_list_t, tag_cp_vec_list_elm);	

typedef int32_t (*lf_app_metrics_create_fnp)
    (void *inp, void **lf_app_metric_ctx);
typedef int32_t (*lf_app_metrics_update_fnp)
    (void *app_conn, void *lf_app_metric_ctx);
typedef void (*lf_app_metrics_cleanup_fnp)
    (void *lf_app_metrics_ctx);
typedef int32_t (*lf_app_metrics_hold_fnp)
    (void *lf_app_metrics_ctx);
typedef int32_t (*lf_app_metrics_release_fnp)
    (void *lf_app_metrics_ctx);
typedef int32_t (*lf_app_metrics_get_out_size_fnp)
    (void *lf_app_metrics_ctx, uint32_t *out_size);
typedef int32_t (*lf_app_metrics_copy_fnp)
    (const void *lf_app_metrics_ctx_in, 
     void *const lf_app_metrics_ctx_out);
typedef struct tag_lf_app_metrics_intf {
    lf_app_metrics_create_fnp create;
    lf_app_metrics_update_fnp update;
    lf_app_metrics_get_out_size_fnp get_out_size;
    lf_app_metrics_copy_fnp copy;
    lf_app_metrics_hold_fnp hold;
    lf_app_metrics_release_fnp release;
    lf_app_metrics_cleanup_fnp cleanup;
} lf_app_metrics_intf_t;

typedef struct tag_lf_sys_glob_max {
    uint32_t tcp_conn_rate_max;
    uint32_t tcp_active_conn_max;
    uint32_t http_tps_max;
    uint32_t blk_io_rate_max;
    uint64_t if_bw_max;
} lf_sys_glob_max_t;

lf_app_metrics_intf_t *
lf_app_metrics_intf_query(lf_app_metrics_intf_t **app_intf,
			  uint32_t section_id, uint32_t app_id);
int32_t
lf_app_metrics_intf_register(lf_app_metrics_intf_t **app_intf,
			     uint32_t section_id,
			     uint32_t app_id,
			     lf_app_metrics_intf_t *intf);
#ifdef __cplusplus
}
#endif
#endif //_LF_COMMON_DEFS_H_
