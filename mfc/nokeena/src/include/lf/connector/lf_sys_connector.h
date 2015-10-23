#ifndef _LF_SYS_CONNECTOR_H
#define _LF_SYS_CONNECTOR_H

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

// lf defs
#include "lf_limits.h"
#include "lf_common_defs.h"
#include "lf_stats.h"
#include "lf_err.h"
#include "lf_ref_token.h"

#define MAX_CPU 32
#define LF_SYS_TCP_STAT_BUF_SIZE (4096)
#define LF_SYS_DISK_LABEL_LEN (11)

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* keeps emacs happy */
}
#endif

typedef struct tag_lf_sys_disk_stats_out {
    char name[11];
    double util;
} lf_sys_disk_stats_out_t;

typedef struct tag_lf_sys_glob_metrics_out {
    uint32_t size;
    double cpu_use[MAX_CPU];
    uint32_t cpu_max;
    uint32_t num_tcp_conn;
    uint32_t tcp_conn_rate;
    uint32_t tcp_conn_rate_max;
    uint32_t tcp_active_conn_max;
    uint32_t http_tps_max;
    uint64_t blk_io_rate;
    uint64_t blk_io_rate_max;
    uint64_t if_bw;
    uint64_t if_bw_max;
    uint32_t num_disks;
    lf_sys_disk_stats_out_t disk_stats[LF_SYS_MAX_DISKS]; 
} lf_sys_glob_metrics_out_t;

typedef struct tag_lf_sys_glob_metrics_ctx {
    uint32_t size;
    double cpu_use[MAX_CPU];
    uint32_t cpu_max;
    uint32_t num_tcp_conn;
    uint32_t tcp_conn_rate;
    uint32_t tcp_conn_rate_max;
    uint32_t tcp_active_conn_max;
    uint32_t http_tps_max;
    uint64_t blk_io_rate;
    uint64_t blk_io_rate_max;
    uint64_t if_bw;
    uint64_t if_bw_max;
    uint32_t num_disks;
    lf_sys_disk_stats_out_t disk_stats[LF_SYS_MAX_DISKS]; 

    pthread_mutex_t lock;
    uint32_t prev_num_tcp_conn;
    double *prev_cpu_times;
    uint64_t prev_if_bytes;
    uint64_t prev_disk_ios;
    ma_t if_bw_ma;
    ma_t blk_io_rate_ma;
    char *tcp_stat_buf;
    cp_trie *disk_list;
    uint64_t prev_up;
} lf_sys_glob_metrics_ctx_t;

typedef struct tag_lf_sys_disk_stats_ctx {
    uint64_t prev_disk_util;
} lf_sys_disk_stats_ctx_t;

typedef struct tag_lf_sys_connector {
    uint32_t num_apps; 		/* special case:1 for system section */
    uint32_t sampling_interval;
    uint32_t window_len;
    uint32_t num_tokens;

    void *app_specific_metrics[LF_MAX_APPS];
    lf_ref_token_t rt;
    struct iovec *out[LF_MAX_APPS];

    lf_connector_update_all_apps_fnp update_all_app_metrics;
    lf_connector_get_snapshot_fnp get_snapshot;
    lf_connector_release_snapshot_fnp release_snapshot;
} lf_sys_connector_t;

int32_t lf_sys_connector_create(
	const lf_connector_input_params_t *input,
	void **out);
int32_t lf_sys_connector_cleanup(lf_sys_connector_t *sc);

#ifdef __cplusplus
}
#endif

#endif //_LF_SYS_CONNECTOR_H
