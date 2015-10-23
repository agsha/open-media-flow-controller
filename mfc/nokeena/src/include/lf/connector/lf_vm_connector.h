#ifndef _LF_VM_CONNECTOR_H
#define _LF_VM_CONNECTOR_H

#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

// libvirt includes
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

#include "lf_limits.h"
#include "lf_stats.h"
#include "lf_common_defs.h"
#include "lf_ref_token.h"

#ifdef __cplusplus
extern "C" {
#endif 
#if 0	/* keeps emacs happy */
}
#endif

#define MAX_HYPERVISOR_TYPES 5
#define MAX_VMA LF_MAX_APPS

typedef enum tag_hyperv_type {
    QEMU_KVM,
    VMWARE
} HYPERV_TYPE;

struct tag_lf_vm_connector;
struct tag_lf_vma_metrics_calc;
struct tag_lf_vm_conn_info;
typedef int32_t (*lf_vm_metrics_copy_fnp)	\
    (const struct tag_lf_vma_metrics_calc *const , struct tag_lf_vma_metrics_calc *);
typedef int32_t (*lf_vm_metrics_update_fnp)		\
    (uint64_t sample_duration, const struct tag_lf_vm_conn_info * ,
     struct tag_lf_vma_metrics_calc *const );

typedef struct tag_lf_vma_metrics {
    uint32_t size;
    char name[64];
    uint32_t cpu_max;
    double cpu_use;
    uint64_t if_bw_max;
    uint64_t if_bw_use;
} lf_vma_metrics_t;
typedef lf_vma_metrics_t lf_vma_metrics_out_t;

typedef struct tag_lf_vma_metrics_calc {
    uint32_t size;
    char name[64];
    uint32_t cpu_max;
    double cpu_use;
    uint64_t if_bw_max;
    uint64_t if_bw_use;

    pthread_mutex_t lock;
    uint64_t prev_cpu_time;
    uint64_t prev_bw_use;
    uint64_t prev_blk_dev_bw_use;
    ma_t *cpu_use_ma;
    ma_t *if_bw_use_ma;

    lf_vm_metrics_copy_fnp copy_metrics;
    lf_vm_metrics_update_fnp update_metrics;

} lf_vma_metrics_calc_t;
  
struct tag_lf_vm_connector {
    uint32_t num_vma;
    struct timeval last_sample_time;
    struct timeval curr_sample_time;
    uint32_t num_tokens;

    virConnectPtr vcon[MAX_VMA];
    virDomainPtr vdom[MAX_VMA];
    virDomainInfo vdom_info[MAX_VMA];
    TAILQ_HEAD(tt1, tag_str_list_elm) if_list_head[MAX_VMA];
    TAILQ_HEAD(tt2, tag_str_list_elm) blk_dev_list_head[MAX_VMA];

    lf_vma_metrics_calc_t vmm[MAX_VMA];
    lf_ref_token_t rt;
    lf_connector_get_app_count_fnp get_app_count;
    lf_connector_update_all_apps_fnp update_all_app_metrics;
    lf_connector_get_snapshot_fnp get_snapshot;
    lf_connector_release_snapshot_fnp release_snapshot;
    struct iovec *out[MAX_VMA];
}; 
typedef struct tag_lf_vm_connector lf_vm_connector_t;

struct tag_lf_vm_conn_info {
    virConnectPtr *vcon;
    virDomainPtr *vdom;
    virDomainInfo *vdom_info;
    struct tt1 *if_list_head;
    struct tt2 *blk_dev_list_head;
    uint64_t sample_duration;
};
typedef struct tag_lf_vm_conn_info lf_vm_conn_info_t;

int32_t lf_vm_connector_create(const lf_connector_input_params_t *const vmai,
			       void **out);
int32_t lf_vm_connector_cleanup(lf_vm_connector_t *ctx);

#ifdef __cplusplus
}
#endif

#endif //_LF_VM_CONNECTOR_H
