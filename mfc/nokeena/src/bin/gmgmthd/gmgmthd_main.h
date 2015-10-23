/*
 *
 * Filename:  gmgmthd_main.h
 * Date:      2010/03/25
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-2010 Ankeena Networks, Inc.  
 * All rights reserved.
 *
 */

#ifndef __GENERIC_MGMT_HDLR_MAIN_H_
#define __GENERIC_MGMT_HDLR_MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <signal.h>
#include "signal_utils.h"
#include "libevent_wrapper.h"

/* ------------------------------------------------------------------------- */
/** Types
 */
/* done to avoid frequent node requests,
will be useful,when are there more configs
*/

/*prefetch timer context definition*/
typedef struct prefetch_timer_data_st {
        int                     index;          /* Index holder : Never reset or modify */
        char                    *name;
        lew_event               *poll_event;
        lew_event_handler       callback;
        void                    *context_data; /* Points to iteself */
        lt_dur_ms               poll_frequency;
	int			flags;
	int			retry_count;
	int			retry_timeout;
} prefetch_timer_data_t;

#define MAX_PREFETCH_JOBS               32
#define PREFETCH_MAX_RETRY_CNT		7

//#define PREFETCH_FLAG_NVSD_INIT         0x0001  - unused as of now
#define PREFETCH_FLAG_NVSD_PREREAD	0x0002
/* ------------------------------------------------------------------------- */
/** Globals
 */
extern	md_client_context *gmgmthd_mcc;
extern	md_client_context *gmgmthd_timer_mcc;
extern	lew_context *gmgmthd_lwc;
extern	lew_context *gmgmthd_timer_lwc;
extern	tbool gmgmthd_exiting;
extern	tbool mfp_exiting;
extern	md_client_context *mfp_mcc;
extern	md_client_context *mfp_timer_mcc;
extern	lew_context *mfp_lwc;
extern	lew_context *mfp_timer_lwc;

/* Global structure to hold the config of watchdog */
extern prefetch_timer_data_t prefetch_timers[MAX_PREFETCH_JOBS];
extern tbool is_pacifica;

/* ------------------------------------------------------------------------- */
/** Prototypes
 */

extern int gmgmthd_initiate_exit(void);

extern int gmgmthd_mgmt_init(void);
extern int gmgmthd_mgmt_shutdown(void);
extern int gmgmthd_mgmt_deinit(void);

extern int gmgmthd_mgmt_timer_thread_init(void);

extern int gmgmthd_config_update_pkt_lim(void);
extern int gmgmthd_config_query(void);
extern int gmgmthd_config_handle_change(const bn_binding_array *arr, uint32 idx,
                                   bn_binding *binding, void *data);
extern int gmgmthd_config_handle_delete(const bn_binding_array *arr, uint32 idx,
                                   bn_binding *binding, void *data);

extern int gmgmthd_mon_handle_get(const char *bname, const tstr_array *bname_parts,
                             void *data, bn_binding_array *resp_bindings);
extern int gmgmthd_mon_handle_iterate(const char *bname,
                                 const tstr_array *bname_parts,
                                 void *data, tstr_array *resp_names);
extern tbool CMM_liveness_check(void);

extern int mfp_init(void);
extern int mfp_mgmt_thread_init(void);
extern int mfp_deinit(void);
extern int gmgmthd_disk_query(void);
extern int lfd_connect(void);
#ifdef __cplusplus
}
#endif

#endif /* __GENERIC_MGMT_HDLR_MAIN_H_ */
