/*
 * Filename: 	nvsd_mgmt_strlog.c
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
#include "nvsd_mgmt_lib.h"

extern uint32_t streamlog_ip;
extern uint16_t streamlog_port;

extern md_client_context *nvsd_mcc;
extern unsigned int jnpr_log_level;

static const char *node_remote_ip = "/nkn/streamlog/config/remote/ip";
static const char *node_remote_port = "/nkn/streamlog/config/remote/port";

/*static const char *node_remote_interface =
  "/nkn/streamlog/config/remote/interface";*/

const char *streamlog_prefix = "/nkn/streamlog/config";
int nvsd_strlog_cfg_query(const bn_binding_array * bindings);

int nvsd_strlog_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

static int
nvsd_strlog_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data);

int
nvsd_strlog_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    streamlog_prefix,
	    NULL,
	    nvsd_strlog_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;
}

int
nvsd_strlog_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    UNUSED_ARGUMENT(old_bindings);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    streamlog_prefix,
	    NULL,
	    nvsd_strlog_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;
}

static int
nvsd_strlog_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tbool *rechecked_licenses_p = data;
    int fd = 0;

    tstr_array *name_parts = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match(ts_str(name), "/nkn/streamlog/config/**")) {

	/*-------- Get the Namespace ------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error_null(err, name_parts);
    } else {
	/*
	 * This is not the namespace node hence leave 
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

	lc_log_basic(jnpr_log_level, "Read %s as : \"%s\"",
		node_remote_ip, t_ip);

	inaddr = inet_addr(t_ip);

	streamlog_ip = inaddr;
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

	streamlog_port = port;
    }
bail:
    if (fd > 0)
	close(fd);
    return err;
}
