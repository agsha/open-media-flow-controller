/*
 *
 * Filename:  nkn_mgmt_fmgr.c
 * Date:      2008/12/05 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

/* Samara Headers */
#include "md_client.h"
#include "license.h"

/* NVSD headers */
#include "nkn_defs.h"
#include "nkn_cfg_params.h"
#include "nvsd_mgmt_lib.h"

/* Local Function Prototypes */
int nvsd_fmgr_cfg_query(const bn_binding_array * bindings);

int nvsd_fmgr_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

static int
nvsd_fmgr_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part, const
	tstring * bndgs_value, void *data);

/* Extern Variables */
extern md_client_context *nvsd_mcc;

/* Local Variables */
const char fmgr_config_prefix[] = "/nkn/nvsd/fmgr/config";
static const char fmgr_input_queue_filename_binding[] =
"/nkn/nvsd/fmgr/config/input_queue_filename";
static const char fmgr_input_queue_maxsize_binding[] =
"/nkn/nvsd/fmgr/config/input_queue_maxsize";
static const char fmgr_cache_no_cache_obj_binding[] =
"/nkn/nvsd/fmgr/config/cache_no_cache_obj";

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_fmgr_cfg_query()
 *	purpose : query for fmgr config parameters
 */
int
nvsd_fmgr_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG, "nvsd fmgr module mgmtd query initializing ..");

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    fmgr_config_prefix,
	    NULL,
	    nvsd_fmgr_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;

}	/* end of nvsd_fmgr_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_fmgr_module_cfg_handle_change()
 *	purpose : handler for config changes for fmgr module
 */
int
nvsd_fmgr_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    UNUSED_ARGUMENT(old_bindings);

    /*
     * Call the callbacks 
     */

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    fmgr_config_prefix,
	    NULL,
	    nvsd_fmgr_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;

}	/* end of * nvsd_fmgr_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_fmgr_cfg_handle_change()
 *	purpose : handler for config changes for fmgr module
 */
static int
nvsd_fmgr_cfg_handle_change(const bn_binding_array * arr,
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

    /*-------- INPUT QUEUE FILENAME ------------*/
    if (ts_equal_str(name, fmgr_input_queue_filename_binding, false)) {

	tstring *t_input_queue_filename = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_input_queue_filename);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../fmgr/config/input_queue_filename as : \"%s\"",
		ts_str(t_input_queue_filename));

	/*
	 * Assign it the global 
	 */
	fmgr_queuefile =
	    nkn_strdup_type(ts_str(t_input_queue_filename), mod_mgmt_charbuf);
	ts_free(&t_input_queue_filename);
    }

    /*-------- INPUT QUEUE MAXSIZE ------------*/
    if (ts_equal_str(name, fmgr_input_queue_maxsize_binding, false)) {

	uint32 t_input_queue_maxsize = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_input_queue_maxsize);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../fmgr/config/input_queue_maxsize as : \"%d\"",
		t_input_queue_maxsize);
	/*
	 * Set the global variable for the module 
	 */
	fmgr_queue_maxsize = t_input_queue_maxsize;
    }

    /*-------- CACHE NO CACHE OBJECT ------------*/
    if (ts_equal_str(name, fmgr_cache_no_cache_obj_binding, false)) {

	tbool t_cache_no_cache_obj;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_cache_no_cache_obj);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../fmgr/config/cache_no_cache_obj as : \"%s\"",
		t_cache_no_cache_obj ? "TRUE" : "FALSE");

	/*
	 * Assign it the global 
	 */
	if (t_cache_no_cache_obj)
	    fmgr_cache_no_cache_obj = 1;
	else
	    fmgr_cache_no_cache_obj = 0;
    }

bail:
    return err;

}	/* end of nvsd_fmgr_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/* End of nkn_mgmt_fmgr.c */
