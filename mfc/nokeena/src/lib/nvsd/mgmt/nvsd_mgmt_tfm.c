/*
 *
 * Filename:  nkn_mgmt_tfm.c
 * Date:      2009/05/19 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-09 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

/* Samara Headers */
#include "md_client.h"
#include "license.h"

/* NVSD headers */
#include "nkn_cfg_params.h"
#include "nkn_defs.h"
#include "nvsd_mgmt_lib.h"

/* Local Function Prototypes */
int nvsd_tfm_cfg_query(const bn_binding_array * bindings);

int nvsd_tfm_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

static int
nvsd_tfm_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part, const
	tstring * bndgs_value, void *data);

/* Extern Variables */
extern md_client_context *nvsd_mcc;
extern int tfm_partial_file_time_limit;
extern uint64_t glob_tfm_max_partial_files;
extern uint64_t glob_tfm_max_file_size;

const char tfm_config_prefix[] = "/nkn/nvsd/tfm/config";

/* Local Variables */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_tfm_cfg_query()
 *	purpose : query for tfm config parameters
 */
int
nvsd_tfm_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG, "nvsd tfm module mgmtd query initializing ..");
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    tfm_config_prefix,
	    NULL,
	    nvsd_tfm_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;
}	/* end of nvsd_tfm_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_tfm_module_cfg_handle_change()
 *	purpose : handler for config changes for tfm module
 */
int
nvsd_tfm_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    UNUSED_ARGUMENT(old_bindings);

    /*
     * Call the callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    tfm_config_prefix,
	    NULL,
	    nvsd_tfm_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;
}	/* end of nvsd_tfm_module_cfg_handle_change() 
								 */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_tfm_cfg_handle_change()
 *	purpose : handler for config changes for tfm module
 */
static int
nvsd_tfm_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part, const
	tstring * bndgs_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tbool *rechecked_licenses_p = data;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

bail:
    return err;

}	/* end of nvsd_tfm_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/* End of nkn_mgmt_tfm.c */
