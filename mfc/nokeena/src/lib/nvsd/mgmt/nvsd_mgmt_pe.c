/*
 *
 * Filename:  nvsd_mgmt_pe.c
 * Date:      2011/06/20
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2009-10 Juniper  Networks, Inc.
 * All rights reserved.
 *
 *
 */

/* Standard Headers */
#include <stdio.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "net/if.h"
#include "interface.h"
#include "sys/ioctl.h"

/* Samara Headers */
#include "md_client.h"
#include "license.h"

#include "nkn_namespace.h"

#include "nvsd_mgmt_namespace.h"
#include "nkn_defs.h"
#include "nvsd_mgmt.h"
#include "nkn_assert.h"
#include "nkn_debug.h"
#include "nvsd_mgmt_pe.h"
#include "nvsd_mgmt_lib.h"

static int nvsd_pe_srvr_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data);

static int nvsd_pe_srvr_delete_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * b_value,
	void *data);

static policy_srvr_t *get_pe_srvr_element(const char *cp_pe_srvr, int add);

static int
nvsd_mgmt_pe_srvr_add_ep(policy_srvr_t * pst_pe_srvr,
	const char *ep_name, uint16_t port);

static int
nvsd_mgmt_pe_srvr_remove_ep(policy_srvr_t * pst_pe_srvr, const char *ep_name);

int
nvsd_pe_srvr_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

policy_srvr_t lst_policy_srvr[NKN_MAX_PE_SRVR];

const char pe_srvr_prefix[] = "/nkn/nvsd/policy_engine/config/server";
static tbool dynamic_change = false;

/* Extern Variables */
extern md_client_context *nvsd_mcc;

policy_srvr_t *nvsd_mgmt_get_pe_srvr(const char *cp_name);

int
nvsd_pe_srvr_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    memset(lst_policy_srvr, 0, sizeof (lst_policy_srvr));

    lc_log_basic(LOG_DEBUG, "PE SRVR cfg query initializing ..");

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    pe_srvr_prefix,
	    NULL,
	    nvsd_pe_srvr_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;
}

int
nvsd_pe_srvr_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    /*
     * Reset the flag 
     */
    dynamic_change = false;

    /*
     * Lock first 
     */
    nvsd_mgmt_namespace_lock();

    /*
     * Call the new bindings callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    pe_srvr_prefix,
	    NULL,
	    nvsd_pe_srvr_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    /*
     * Call the old bindings callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    pe_srvr_prefix,
	    NULL,
	    nvsd_pe_srvr_delete_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    /*
     * Unlock when leaving 
     */
    nvsd_mgmt_namespace_unlock();

    /*
     * If relevant node has changed then indicate to the module 
     */
    if (dynamic_change)
	nkn_namespace_config_update(NULL);

    return err;

bail:
    /*
     * Unlock when leaving 
     */
    nvsd_mgmt_namespace_unlock();

    return err;
}	/* end of * nvsd_pe_srvr_module_cfg_handle_change() */

static int
nvsd_pe_srvr_delete_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *srvr_name = NULL;
    tstr_array *name_parts = NULL;
    policy_srvr_t *pst_pe_srvr = NULL;

    tbool *rechecked_licenses_p = data;

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
    if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/policy_engine/config/server/**")) {
	/*
	 * get the cert name 
	 */
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);

	srvr_name = tstr_array_get_str_quick(name_parts, 5);

	lc_log_basic(LOG_DEBUG,
		"Read ... as /nkn/nvsd/policy_engine/config/server/: \"%s\"",
		srvr_name);

	pst_pe_srvr = get_pe_srvr_element(srvr_name, 0);
	if (!pst_pe_srvr)
	    goto bail;
    } else {
	/*
	 * This is not a cert node.. hence bail 
	 */
	goto bail;
    }

    if (bn_binding_name_parts_pattern_match_va(name_parts, 5, 1, "*")) {
	safe_free(pst_pe_srvr->name);
	safe_free(pst_pe_srvr->uri);
	memset(pst_pe_srvr, 0, sizeof (policy_srvr_t));
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 3, "*", "endpoint", "*")) {
	const char *ep_name = NULL;

	ep_name = tstr_array_get_str_quick(name_parts, 7);
	bail_error(err);

	err = nvsd_mgmt_pe_srvr_remove_ep(pst_pe_srvr, ep_name);

	if (err)
	    lc_log_basic(LOG_NOTICE, "Error adding endpoint to server");
    }

bail:
    tstr_array_free(&name_parts);
    return err;
}

static int
nvsd_pe_srvr_cfg_handle_change(const bn_binding_array * arr,
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
    policy_srvr_t *pst_pe_srvr = NULL;
    const char *pe_srvr_name = NULL;
    tbool *rechecked_licenses_p = data;

    char *t_uri = NULL;
    char *t_proto = NULL;

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
    if (bn_binding_name_pattern_match(ts_str(name),
		"/nkn/nvsd/policy_engine/config/server/**"))
    {
	/*-------- Get the server------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	pe_srvr_name = tstr_array_get_str_quick(name_parts, 5);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../nkn/nvsd/policy_engine/config/server as : \"%s\"",
		pe_srvr_name);

	/*
	 * Get the resource_mgr entry in the global array 
	 */
	pst_pe_srvr = get_pe_srvr_element(pe_srvr_name, 1);
	if (!pst_pe_srvr)
	    goto bail;

	if (pst_pe_srvr->name == NULL) {
	    pst_pe_srvr->name = nkn_strdup_type(pe_srvr_name, mod_mgmt_charbuf);
	}

    } else {
	/*
	 * This is not the policy engine srvr node hence leave 
	 */
	goto bail;
    }

    if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "enabled")) {
	tbool t_enabled;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enabled);
	bail_error(err);
	pst_pe_srvr->enable = t_enabled;
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "uri")) {

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &t_uri);
	bail_error(err);

	if (pst_pe_srvr->uri)
	    safe_free(pst_pe_srvr->uri);

	if (t_uri)
	    pst_pe_srvr->uri = nkn_strdup_type(t_uri, mod_mgmt_charbuf);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "proto")) {

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &t_proto);
	bail_error(err);

	if (t_proto && !strcmp(t_proto, "ICAP")) {
	    pst_pe_srvr->proto = ICAP;
	}
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 4, "*", "endpoint", "*", "port")) {
	uint16_t port = 0;
	const char *ep_name = NULL;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &port);
	bail_error(err);
	ep_name = tstr_array_get_str_quick(name_parts, 7);
	bail_error(err);

	err = nvsd_mgmt_pe_srvr_add_ep(pst_pe_srvr, ep_name, port);

	if (err)
	    lc_log_basic(LOG_NOTICE, "Error adding endpoint to server");
    }

bail:
    safe_free(t_uri);
    safe_free(t_proto);
    tstr_array_free(&name_parts);
    return err;
}

/* Function to add an endpoint to the policy server
 * If there was no enpoint previously defined create one and
 * fill domain,port
 * else find the endpoint by matching against
 * domain and fill the port
 */
static int
nvsd_mgmt_pe_srvr_add_ep(policy_srvr_t * pst_pe_srvr,
	const char *ep_name, uint16_t port)
{
    int i = 0;
    int free_entry_index = -1;

    policy_ep_t *ep = NULL;

    for (i = 0; i < NKN_MAX_PE_SRVR_HST; i++) {
	ep = &(pst_pe_srvr->endpoint[i]);
	if (ep->domain == NULL) {
	    if (free_entry_index == -1)
		free_entry_index = i;
	    continue;
	} else if (!strcmp(ep->domain, ep_name)) {
	    // ep.domain = nkn_strdup_type(ep_name, mod_mgmt_charbuf);
	    ep->port = port;
	    return 0;
	}

    }

    if (free_entry_index != -1) {
	ep = &(pst_pe_srvr->endpoint[free_entry_index]);
	ep->domain = nkn_strdup_type(ep_name, mod_mgmt_charbuf);
	ep->port = port;
	return 0;
    } else
	return -1;

}

static int
nvsd_mgmt_pe_srvr_remove_ep(policy_srvr_t * pst_pe_srvr, const char *ep_name)
{
    int i = 0;

    policy_ep_t *ep = NULL;

    for (i = 0; i < NKN_MAX_PE_SRVR_HST; i++) {
	ep = &(pst_pe_srvr->endpoint[i]);
	if (ep->domain && !strcmp(ep->domain, ep_name)) {
	    safe_free(ep->domain);
	    memset(ep, 0, sizeof (policy_ep_t));
	    return 0;
	}

    }
    return -1;
}

policy_srvr_t *
nvsd_mgmt_get_pe_srvr(const char *cp_name)
{
    int i;

    /*
     * Sanity Check 
     */
    if (cp_name == NULL)
	return (NULL);

    /*
     * Now find the matching entry 
     */
    for (i = 0; i < NKN_MAX_PE_SRVR; i++) {
	if ((lst_policy_srvr[i].name != NULL) &&
		(strcmp(cp_name, lst_policy_srvr[i].name) == 0)) {
	    /*
	     * Return this entry 
	     */
	    return (&(lst_policy_srvr[i]));
	}
    }

    /*
     * No Match 
     */
    return (NULL);
}	/* end of nvsd_mgmt_get_pe_srvr() */

static policy_srvr_t *
get_pe_srvr_element(const char *cp_pe_srvr, int add)
{
    int i = 0;
    int free_entry_index = -1;

    for (i = 0; i < NKN_MAX_PE_SRVR; i++) {
	if (lst_policy_srvr[i].name == NULL) {
	    /*
	     * Save the first free entry 
	     */
	    if (free_entry_index == -1)
		free_entry_index = i;

	    /*
	     * Empty entry.. hence continue 
	     */
	    continue;
	} else if (strcmp(cp_pe_srvr, lst_policy_srvr[i].name) == 0) {
	    /*
	     * Found match hence return this entry 
	     */
	    return (&(lst_policy_srvr[i]));
	}
    }

    /*
     * Now that we have gone through the list and no match return the
     * free_entry_index 
     */
    if (add && free_entry_index != -1)
	return (&(lst_policy_srvr[free_entry_index]));

    /*
     * No match and no free entry 
     */
    return (NULL);
}	/* end of get_pe_srvr_element () */
