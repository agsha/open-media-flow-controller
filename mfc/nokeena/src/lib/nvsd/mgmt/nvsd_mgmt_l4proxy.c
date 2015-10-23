/*
 *
 * Filename:  nvsd_mgmt_l4proxy.c
 * Date:      2011/05/20 
 * Author:    Ramalingam Chandran.
 *
 * (C) Copyright 2010 Juniper Networks, Inc.  
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

/* Samara Headers */
#include "md_client.h"
#include "license.h"

/* nvsd Headers */
#include "nkn_defs.h"
#include "nvsd_mgmt.h"
#include "nkn_cfg_params.h"
#include "nvsd_mgmt_lib.h"

/* Local Function Prototypes */
int nvsd_l4proxy_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

static int
nvsd_l4proxy_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);

/* Extern Variables */
extern md_client_context *nvsd_mcc;

const char l4proxy_config_prefix[] = "/nkn/l4proxy/config";

/* Local Variables */

/* Extern Functions */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_l4proxy_cfg_query()
 *	purpose : query for l4proxy config parameters
 */
int
nvsd_l4proxy_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG, "nvsd l4proxy  module mgmtd query initializing ..");

    /*
     * Bind to get CLUSTER 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    l4proxy_config_prefix,
	    NULL,
	    nvsd_l4proxy_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

bail:
    return err;
}	/* end of nvsd_l4proxy_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_l4proxy_module_cfg_handle_change()
 *	purpose : handler for config changes for l4proxy module
 */
int
nvsd_l4proxy_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    UNUSED_ARGUMENT(old_bindings);

    /*
     * Call the new bindings callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    l4proxy_config_prefix,
	    NULL,
	    nvsd_l4proxy_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);
bail:
    return err;
}	/* end of * nvsd_l4proxy_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_l4proxy_cfg_handle_change()
 *	purpose : handler for config changes for l4proxy module
 */
static int
nvsd_l4proxy_cfg_handle_change(const bn_binding_array * arr,
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
    if (bn_binding_name_pattern_match(ts_str(name), "/nkn/l4proxy/config/**")) {
	lc_log_basic(LOG_DEBUG, "Read .../nkn/l4proxy/config");
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error_null(err, name_parts);

    } else {
	/*
	 * This is not the l4proxy node hence leave 
	 */
	goto bail;
    }

    /*
     * Get the l4proxy status 
     */
    if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 1, "enable")) {

	tbool t_enabled = false;

	err = bn_binding_get_tbool(binding, ba_value, 0, &t_enabled);
	bail_error(err);

	lc_log_basic(LOG_DEBUG, "Read .../nkn/l4proxy/config/enable : %ds",
		t_enabled);

	l4proxy_enabled = t_enabled;
    }
bail:
    tstr_array_free(&name_parts);
    return err;
}	/* end of nvsd_l4proxy_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/* End of nvsd_mgmt_l4proxy.c */
