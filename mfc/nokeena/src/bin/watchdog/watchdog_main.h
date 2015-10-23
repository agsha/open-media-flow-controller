/*
 *
 * Filename:  watchdog_main.h
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
/* Watch-dog struct to store watch-dog settings */
typedef struct watchdog_config_st {
	tbool restart_nvsd;
	tbool is_httpd_enabled;
	tbool is_httpd_listen_enabled;
	tbool is_listen_intf_link_up;
	tbool has_mgmt_intf;		//Start probing only when we http list intf set
	tbool is_gdb_ena;		//GDB cmd str
	tbool is_nvsd_rst;
	char *httpd_listen_port;
	char *mgmt_ip;
	char *mgmt_intf;
	char *domain;
	uint32 num_network_threads;
	uint32 hung_count;
	uint32 num_lic_sock;
	uint32 coredump_enabled;
	lt_time_sec grace_period;
	int16 nvsd_listen_port;
	char *probe_uri;
	char *probe_o_host;		//MFC Probes origin host
	char *probe_o_port;		//MFC Probes origin port
} watchdog_config_t;

/* ------------------------------------------------------------------------- */
/** Globals
 */
extern	md_client_context *watchdog_mcc;
extern	lew_context *watchdog_lwc;
extern	tbool watchdog_exiting;

/* Global structure to hold the config of watchdog */
extern watchdog_config_t   watchdog_config;
/* ------------------------------------------------------------------------- */

// preread states
#define NO_PREREAD		(0) // no preread -- present only in exclude disk config
#define PREREAD_START		(1) // monitor preread -- present only in include disk config
#define PREREAD_END		(2) // preread done -- present only in include disk config
/** Prototypes
 */

extern int watchdog_initiate_exit(void);

extern int watchdog_mgmt_init(void);
extern int watchdog_mgmt_shutdown(void);
extern int watchdog_mgmt_deinit(void);

extern int watchdog_config_update_pkt_lim(void);
extern int watchdog_config_query(void);
extern int watchdog_config_handle_change(const bn_binding_array *arr, uint32 idx,
                                   bn_binding *binding, void *data);
extern int watchdog_config_handle_delete(const bn_binding_array *arr, uint32 idx,
                                   bn_binding *binding, void *data);

extern int watchdog_mon_handle_get(const char *bname, const tstr_array *bname_parts,
                             void *data, bn_binding_array *resp_bindings);
extern int watchdog_mon_handle_iterate(const char *bname,
                                 const tstr_array *bname_parts,
                                 void *data, tstr_array *resp_names);

#ifdef __cplusplus
}
#endif

#endif /* __GENERIC_MGMT_HDLR_MAIN_H_ */
