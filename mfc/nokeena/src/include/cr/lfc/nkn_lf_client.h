/**
 * @file   nkn_lf_client.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Mon Apr 16 17:27:36 2012
 * 
 * @brief  definitions for the lf client
 * 
 * 
 */
#ifndef _LF_CLIENT_
#define _LF_CLIENT_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>

#include "mfp/event_timer/mfp_event_timer.h"
#include "nkn_vpe_ism_read_api.h"
#include "nkn_sync_http_client.h"
#include "cr_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif 

struct tag_obj_lf_client;

typedef enum tag_lf_cache_status {
    LF_UNREACHABLE,
    CACHE_DOWN,
    CACHE_UP
}lf_cache_status_t;

typedef struct tag_lf_stats_t {
    lf_cache_status_t status;
    double cpu_load;
} lf_stats_t;

typedef enum {
    LF_CLIENT_IDLE,
    LF_CLIENT_RUNNING,
    LF_CLIENT_PROGRESS,
    LF_CLIENT_STOP
} lf_client_state_t;

typedef struct tag_lf_src_params {
    char fqdn[256];
    uint16_t port;
} lf_src_params_t;

typedef int32_t (*lf_client_update_cb_fnp)(void *state,
			   const lf_stats_t *const stats,
			   uint32_t stats_size);

typedef int32_t (*lf_client_delete_fnp)(struct tag_obj_lf_client *);

typedef int32_t (*lf_client_set_state_fnp)
(struct tag_obj_lf_client *, lf_client_state_t state);

typedef lf_client_state_t (*lf_client_get_state_fnp)
(const struct tag_obj_lf_client *const);

typedef int32_t (*lf_client_get_update_fnp)(
		   struct tag_obj_lf_client*); 

typedef void (*lf_client_set_update_interval_fnp)
(struct tag_obj_lf_client *, uint32_t update_interval);

typedef void (*lf_client_set_uri_fnp)
(struct tag_obj_lf_client *, const char *const uri);

typedef struct tag_obj_lf_client {
    lf_client_state_t state;
    pthread_mutex_t lock;
    uint32_t update_interval; //milliseconds
    lf_client_update_cb_fnp update_cb;
    void *update_cb_data;
    lf_client_delete_fnp delete;
    lf_client_set_state_fnp set_state;
    lf_client_get_state_fnp get_state;
    lf_client_get_update_fnp get_update;
    lf_client_set_update_interval_fnp set_update_interval;
    lf_client_set_uri_fnp set_uri;
    const event_timer_t *evt;
    lf_src_params_t lf_src;
} obj_lf_client_t;

int32_t lf_client_create(
		 uint32_t interval, lf_client_update_cb_fnp cb,
		 void *cb_data, const char *lf_src_fqdn,
		 uint16_t lf_listen_port, const event_timer_t *evt,
		 obj_lf_client_t **out);

#ifdef __cplusplus
}
#endif

#endif //_LF_CLIENT_
