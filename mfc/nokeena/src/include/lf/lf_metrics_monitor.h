#ifndef _LF_METRICS_MONITOR_H_
#define _LF_METRICS_MONITOR_H_

#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

// nkn defs
#include "mfp/event_timer/mfp_event_timer.h"

//lf defs
#include "lf_vm_connector.h"
#include "lf_na_connector.h"
#include "lf_sys_connector.h"
#include "lf_common_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 /* keeps emacs happy */
}
#endif

/**
 * hypev - hypervisor
 * vma - VM Applications
 * na - native applications
 * object model
 * connector
 * 
 */
#define LF_SECTION_AVAIL (1)
#define LF_SECTION_UNAVAIL (0)

typedef void (*lf_metrics_monitor_thread_fnp)(void *);

typedef struct tag_lf_metrics_t {
    uint32_t state;
    uint32_t sampling_interval;
    uint32_t num_tokens;
    uint32_t token_no;
    uint32_t window_len;
    lf_ev_timer_ctx_t evt;
    pthread_mutex_t lock;
    char tbuf[80];

    lf_metrics_monitor_thread_fnp monitor_thread;
    lf_connector_input_params_t ci[LF_MAX_SECTIONS];

    uint8_t section_map[LF_MAX_SECTIONS];
    lf_vm_connector_t *vmc;
    lf_na_connector_t *nac;
    lf_sys_connector_t *sc;
} lf_metrics_ctx_t;

int32_t lf_metrics_start_monitor(lf_metrics_ctx_t *lfm);
int32_t lf_metrics_create_ctx(lf_app_list_t *na_list,
			      lf_app_list_t *vma_list,
			      uint32_t sampling_interval, 
			      uint32_t window_len,
			      uint32_t ext_window_len,
			      uint32_t max_tokens, 
			      lf_metrics_ctx_t **out);
int32_t lf_metrics_get_snapshot_ref(lf_metrics_ctx_t *lfm,
				    void **out, 
				    void **ref_id);
int32_t lf_metrics_get_filter_snapshot_ref(lf_metrics_ctx_t *lfm,
					   struct str_list_t **filter,
					   void **out,
					   void **ref_id);
int32_t lf_metrics_release_snapshot_ref(lf_metrics_ctx_t *lfm,
					void **ref_id);
char *lf_get_snapshot_time_safe(lf_metrics_ctx_t *lfm);

static inline void
lf_release_lock(lf_metrics_ctx_t *lfm)
{
    pthread_mutex_unlock(&lfm->lock);
}


#ifdef __cplusplus
}
#endif

#endif //_LF_METRICS_MONITOR_
