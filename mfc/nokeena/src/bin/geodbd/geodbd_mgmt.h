/*
 * Filename :   geodbd_mgmt.h
 * Date:        04 July 2011
 * Author:      Muthukumar
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef  __GEODBD_MGMT__H
#define  __GEODBD_MGMT__H

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

const char        * program_name = "geodbd";
lew_context       * geodbd_lew = NULL;
md_client_context * geodbd_mcc = NULL;
tbool               geodbd_exiting = false;
static pthread_mutex_t upload_mgmt_geodbd_lock;
int                 update_from_db_done = 0;


static const char geodbd_prefix[]   = "/nkn/geodbd/config";

///////////////////////////////////////////////////////////////////////////////
extern int  exit_req;
///////////////////////////////////////////////////////////////////////////////

int geodbd_mgmt_event_registration_init(void);
int geodbd_mgmt_handle_event_request(gcl_session *session,  
                              bn_request  **inout_request,
                              void        *arg);
int geodbd_mgmt_handle_session_close(int fd, fdic_callback_type cbt, 
                              void *vsess, void *gsec_arg);
int geodbd_config_handle_set_request( 
        bn_binding_array *old_bindings, bn_binding_array *new_bindings);
int geodbd_module_cfg_handle_change(
        bn_binding_array *old_bindings, bn_binding_array *new_bindings);
int
geodbd_mgmt_handle_action_request(gcl_session *sess,
	bn_request **inout_action_request, void *arg);


int geodbd_query_init (void);
int geodbd_cfg_handle_change(
        const bn_binding_array *arr,
        uint32 idx,
        bn_binding *binding,
         void *data);

int    geodbd_deinit(void);
int    geodbd_mgmt_process(int init_value);
void * geodbd_mgmt_func(void * arg);
void   geodbd_mgmt_init(void);
int    geodbd_mgmt_initiate_exit(void);

extern int
log_basic(const char *__restrict fmt, ...)
    __attribute__ ((__format__ (printf, 1, 2)));

extern int
log_debug(const char *__restrict fmt, ...)
    __attribute__ ((__format__ (printf, 1, 2)));

int geodbd_install(const char *path, tstring **ret_output);
#endif /* __GEODBD_MGMT__H */
