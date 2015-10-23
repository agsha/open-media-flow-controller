
/*
 * Filename: 	nvsd_mgmt_acclog.c
 * Date:	11/4/2010
 * Author:	Dhruva Narasimhan
 *
 * (C) Copyright 2010 Juniper Networks Inc.
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
#include <alloca.h>

#include "md_client.h"
#include "license.h"
#include "ttime.h"
#include "nkn_defs.h"
#include "nkn_lockstat.h"
#include "nvsd_mgmt_lib.h"

extern uint32_t accesslog_ip;
extern uint16_t accesslog_port;
extern uint32_t accesslog_remote_if_ip;

extern md_client_context *nvsd_mcc;
extern unsigned int jnpr_log_level;
nkn_mutex_t profile_lock;
static tstr_array *profiles = NULL;

const char acclog_prefix[] = "/nkn/accesslog/config/profile";
static const char *node_remote_ip = "/nkn/accesslog/config/remote/ip";
static const char *node_remote_port = "/nkn/accesslog/config/remote/port";

extern int al_remove_from_table(const char *name);
extern void nvsd_update_namespace_accesslog_profile_format(const char
	*profile_name, const char
	*profile_format,
	void *ns_node_data);

int nvsd_acclog_cfg_query(const bn_binding_array * bindings);


int nvsd_acclog_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

static int
nvsd_acclog_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);

static int
nvsd_acclog_cfg_handle_delete_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);

const char *nvsd_mgmt_acclog_profile_get(uint32_t idx);
uint32_t nvsd_mgmt_acclog_profile_length(void);

int
nvsd_acclog_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    NKN_MUTEX_INIT(&profile_lock, NULL, "acclog_profile");

    err = tstr_array_new(&profiles, NULL);
    bail_error_null(err, profiles);

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    acclog_prefix,
	    NULL,
	    nvsd_acclog_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

bail:
    return err;
}

int
nvsd_acclog_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    UNUSED_ARGUMENT(old_bindings);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    "",
	    NULL,
	    nvsd_acclog_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    "",
	    NULL,
	    nvsd_acclog_cfg_handle_delete_change,
	    &rechecked_licenses);
    bail_error(err);

bail:
    return err;
}

static int
nvsd_acclog_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tbool *rechecked_licenses_p = data;
    int fd = 0;
    struct ifreq ifr;
    tstr_array *name_parts = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match(ts_str(name), "/nkn/accesslog/config/**")) {

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

	inaddr = inet_addr(t_ip);

	lc_log_basic(jnpr_log_level, "Read %s as : \"%s\"",
		node_remote_ip, t_ip);

	accesslog_ip = inaddr;
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

	accesslog_port = port;
    }

    /*
     * Get remote interface and resolve to IP to bind to 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 1, "interface")) {
	char *t_if = NULL;

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &t_if);
	bail_error(err);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd <= 0)
	    goto bail;

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, t_if, IFNAMSIZ - 1);
	if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
	    goto bail;
	}

	accesslog_remote_if_ip =
	    ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr;

	close(fd);
	fd = 0;
	safe_free(t_if);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 1, "*")) {
	const char *pname = tstr_array_get_str_quick(name_parts, 4);

	printf("New Log profile: %s\n", pname);

	if (pname != NULL) {
	    NKN_MUTEX_LOCK(&profile_lock);
	    err = tstr_array_append_sprintf(profiles, "uds-acclog-%s", pname);
	    NKN_MUTEX_UNLOCK(&profile_lock);
	    bail_error(err);
	}
    } else if (bn_binding_name_parts_pattern_match_va
		(name_parts, 4, 2, "*", "format")) {
	    const char *profile_name = NULL;
	    char *format_name = NULL;

	    profile_name = tstr_array_get_str_quick(name_parts, 4);
	    bail_null(profile_name);

	    err = bn_binding_get_str(binding, ba_value,
		    bt_string, NULL, &format_name);
	    bail_error(err);

	    if (format_name != NULL)
		nvsd_update_namespace_accesslog_profile_format(profile_name,
			format_name, NULL);

	    safe_free(format_name);
	}

bail:
    if (fd > 0)
	close(fd);
    tstr_array_free(&name_parts);
    return err;
}

static int
nvsd_acclog_cfg_handle_delete_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    tbool *rechecked_licenses_p = data;
    tstr_array *name_parts = NULL;
    const tstring *name = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    if (bn_binding_name_pattern_match(ts_str(name), "/nkn/accesslog/config/**")) {

	/*-------- Get the Namespace ------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error_null(err, name_parts);
    } else {
	/*
	 * This is not the namespace node hence leave 
	 */
	goto bail;
    }

    if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 1, "*")) {
	char *p = alloca(256);
	const char *pname = tstr_array_get_str_quick(name_parts, 4);

	snprintf(p, 256, "uds-acclog-%s", pname);

	NKN_MUTEX_LOCK(&profile_lock);
	err = tstr_array_delete_str_once(profiles, p);
	NKN_MUTEX_UNLOCK(&profile_lock);
	bail_error(err);

	/*
	 * Prepare to remove from hash table also 
	 */

	al_remove_from_table(p);
    }

bail:
    tstr_array_free(&name_parts);
    return 0;
}

const char *
nvsd_mgmt_acclog_profile_get(uint32_t idx)
{
    const char *p = NULL;

    NKN_MUTEX_LOCK(&profile_lock);
    p = tstr_array_get_str_quick(profiles, idx);
    // tstr_array_dump(profiles);
    NKN_MUTEX_UNLOCK(&profile_lock);
    return p;
}

uint32_t
nvsd_mgmt_acclog_profile_length(void)
{
    int i = 0;

    NKN_MUTEX_LOCK(&profile_lock);
    i = tstr_array_length_quick(profiles);
    NKN_MUTEX_UNLOCK(&profile_lock);

    return i;
}
