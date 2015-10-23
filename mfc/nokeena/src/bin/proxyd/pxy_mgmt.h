/*
 * Filename :   pxy_mgmt.h
 * Date:        2011-05-02
 * Author:      Bijoy M Chandrasekharan
 *
 * (C) Copyright 2011, Nokeena Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef  __PXY_MGMT__H
#define  __PXY_MGMT__H

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

#include "pxy_defs.h"


const char        * program_name = "proxyd";
lew_context       * mgmt_lew = NULL;
md_client_context * mgmt_mcc = NULL;
tbool               mgmt_exiting = false;
static pthread_mutex_t upload_mgmt_log_lock;

tstring           * server_port_list = NULL;
tstring           * server_interface_list = NULL;
int                 update_from_db_done = 0;


static const char l4proxy_prefix[]   = "/nkn/l4proxy/config";
static const char l4proxy_use_client_ip_binding[]  = "/nkn/l4proxy/config/use_client_ip";
static const char l4proxy_add_cacheable_ip_binding[]  = "/nkn/l4proxy/config/add_cacheable_ip_list";
static const char l4proxy_add_block_ip_binding[]  = "/nkn/l4proxy/config/add_block_ip_list";
static const char l4proxy_del_ip_binding[]  = "/nkn/l4proxy/config/del_ip_list";

/* Local Function Prototypes */

/* Local Variables */
static const char http_config_prefix[]                    = "/nkn/nvsd/http/config";
static const char http_config_server_port_prefix[]        = "/nkn/nvsd/http/config/server_port";
static const char http_config_server_interface_prefix[]   = "/nkn/nvsd/http/config/interface";

///////////////////////////////////////////////////////////////////////////////
extern int  use_client_ip;
extern int  exit_req;

extern int  pxy_tunnel_all;       /* domain-filter tunnel-list all(1), (0) for 'no' of this command*/
extern int  pxy_cache_fail_reject;/* domain-filter cache-fail [tunnel(0) | reject(1)] */

extern void add_block_ip_list(char * p);
extern void add_cacheable_ip_list(char * p);
extern void del_ip_list(char * p);
///////////////////////////////////////////////////////////////////////////////

int pxy_mgmt_event_registeration_init(void);
int pxy_mgmt_handle_event_request(gcl_session *session,  
                              bn_request  **inout_request,
                              void        *arg);
int pxy_mgmt_handle_session_close(int fd, fdic_callback_type cbt, 
                              void *vsess, void *gsec_arg);
int pxy_config_handle_set_request( 
        bn_binding_array *old_bindings, bn_binding_array *new_bindings);
int pxy_module_cfg_handle_change(
        bn_binding_array *old_bindings, bn_binding_array *new_bindings);

int pxy_l4proxy_query_init (void);
int pxy_l4proxy_cfg_handle_change(
        const bn_binding_array *arr,
        uint32 idx,
        bn_binding *binding,
         void *data);
int pxy_http_cfg_query(void);

int pxy_http_module_cfg_handle_change (bn_binding_array *old_bindings,
				bn_binding_array *new_bindings);
int pxy_http_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
        		bn_binding *binding, void *data);
int pxy_http_cfg_server_port(const bn_binding_array *arr, uint32 idx, 
			bn_binding *binding, void *data);
int pxy_http_cfg_server_interface(const bn_binding_array *arr, uint32 idx, 
			bn_binding *binding, void *data);

int    pxy_deinit(void);
int    pxy_mgmt_process(int init_value);
void * pxy_mgmt_func(void * arg);
void   pxy_mgmt_init(void);
int    pxy_mgmt_initiate_exit(void);
int    log_basic(const char *fmt, ...)__attribute__ ((format (printf, 1, 2)));
int    log_debug(const char *fmt, ...)__attribute__ ((format (printf, 1, 2)));;

#endif /* __PXY_MGMT__H */
