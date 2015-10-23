/*
 * Filename :   adns_mgmt.h
 * Date:        23 May 2011
 * Author:      Kiran Desai
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef  __ADNS_MGMT__H
#define  __ADNS_MGMT__H

#include "md_client.h"
#include "mdc_wrapper.h"
#include "bnode.h"
#include "signal_utils.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>

const char        * program_name = "adnsd";
lew_context       * mgmt_lew = NULL;
md_client_context * mgmt_mcc = NULL;
tbool               mgmt_exiting = false;
static pthread_mutex_t upload_mgmt_adnsd_lock;
int                 update_from_db_done = 0;


static const char adnsd_prefix[]   = "/nkn/adnsd/config";

///////////////////////////////////////////////////////////////////////////////
extern int  exit_req;
///////////////////////////////////////////////////////////////////////////////

int adnsd_mgmt_event_registration_init(void);
int adnsd_mgmt_handle_event_request(gcl_session *session,  
                              bn_request  **inout_request,
                              void        *arg);
int adnsd_mgmt_handle_session_close(int fd, fdic_callback_type cbt, 
                              void *vsess, void *gsec_arg);
int adnsd_config_handle_set_request( 
        bn_binding_array *old_bindings, bn_binding_array *new_bindings);
int adnsd_module_cfg_handle_change(
        bn_binding_array *old_bindings, bn_binding_array *new_bindings);

int adnsd_query_init (void);
int adnsd_cfg_handle_change(
        const bn_binding_array *arr,
        uint32 idx,
        bn_binding *binding,
         void *data);

int    adnsd_deinit(void);
int    adnsd_mgmt_process(int init_value);
void * adnsd_mgmt_func(void * arg);
void   adnsd_mgmt_init(void);
int    adnsd_mgmt_initiate_exit(void);

extern int
log_basic(const char *__restrict fmt, ...)
    __attribute__ ((__format__ (printf, 1, 2)));

extern int
log_debug(const char *__restrict fmt, ...)
    __attribute__ ((__format__ (printf, 1, 2)));


#endif /* __ADNSD_MGMT__H */
