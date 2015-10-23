/*
 * Filename: 	nvsd_mgmt_errlog.c
 * Date:	25/FEB/2010
 * Author:	Manikandan Vengatachalam
 *
 * (C) Copyright 2011 Juniper Networks Inc.
 * All rights reserved.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "md_client.h"
#include "license.h"
#include "ttime.h"
#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_util.h"
#include "nvsd_mgmt_lib.h"

extern uint32_t errorlog_ip;
extern uint16_t errorlog_port;

extern md_client_context *nvsd_mcc;
extern unsigned int jnpr_log_level;
static const char *node_errorlog_level = "/nkn/errorlog/config/level";
static const char *node_remote_ip = "/nkn/errorlog/config/remote/ip";
static const char *node_remote_port = "/nkn/errorlog/config/remote/port";

const char errlog_prefix[] = "/nkn/errorlog/config";
int nvsd_errlog_cfg_query(const bn_binding_array * bindings);

int nvsd_errlog_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

static int
nvsd_errlog_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data);

int
nvsd_errlog_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    errlog_prefix,
	    NULL,
	    nvsd_errlog_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;
}

int
nvsd_errlog_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    UNUSED_ARGUMENT(old_bindings);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    errlog_prefix,
	    NULL,
	    nvsd_errlog_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;
}

static int
nvsd_errlog_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;

    tstr_array *name_parts = NULL;
    tstring * mod = NULL;
    char strmod[256];
    char * p;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    if (bn_binding_name_pattern_match(ts_str(name), "/nkn/errorlog/config/**")) {
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
    } else {
	/*
	 * This is not the server_map node hence leave 
	 */
	goto bail;
    }

    /*
     * get remote IP address 
     */
    if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 3, 2, "remote", "ip")) {
	char *t_ip = NULL;
	in_addr_t inaddr;

	err = bn_binding_get_str(binding, ba_value, bt_ipv4addr, NULL, &t_ip);
	bail_error(err);

	inaddr = inet_addr(t_ip);
	lc_log_basic(jnpr_log_level, "Read %s as : \"%s\"",
		node_remote_ip, t_ip);

	errorlog_ip = inaddr;
	safe_free(t_ip);
    }

    /*
     * Get Remote port 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 3, 2, "remote", "port")) {
	uint16_t port = 0;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &port);
	bail_error(err);

	lc_log_basic(jnpr_log_level, "Read %s as : \"%d\"",
		node_remote_port, port);

	errorlog_port = port;
    }
    /*
     * Get errorlog level 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 1, "level")) {
	uint32 log_level = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &log_level);
	bail_error(err);

	lc_log_basic(jnpr_log_level, "Read %s as : \"%d\"",
		node_errorlog_level, log_level);

	console_log_level = log_level;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 1, "mod")) {
	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&mod);

	bail_error(err);
	snprintf(strmod, sizeof(strmod), "%s", ts_str(mod));
	p=&strmod[0];
	if( (strlen(p)>=3) && (p[0]=='0') && (p[1]=='x') ) {
	    console_log_mod = convert_hexstr_2_int64(p+2);
	} else {
	    console_log_mod = atoi(p);
	}
	lc_log_basic(LOG_NOTICE, "Read .../errorlog/mod as : %s", ts_str(mod));
    }

bail:
    tstr_array_free(&name_parts);
    ts_free(&mod);

    return err;
}
