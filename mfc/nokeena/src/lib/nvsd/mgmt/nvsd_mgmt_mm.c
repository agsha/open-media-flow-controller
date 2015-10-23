/*
 *
 * Filename:  nvsd_mgmt_mm.c
 * Date:      2008/12/19 
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
#include "nkn_cfg_params.h"
#include "nkn_defs.h"
#include "nvsd_mgmt_lib.h"

/* Local Function Prototypes */
int nvsd_mm_cfg_query(const bn_binding_array * bindings);

int nvsd_mm_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

static int
nvsd_mm_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);

/* Extern Variables */
extern md_client_context *nvsd_mcc;

const char mm_config_prefix[] = "/nkn/nvsd/mm/config";

/* Local Variables */
static const char mm_provider_threshold_high_binding[] =
"/nkn/nvsd/mm/config/provider/threshold/high";
static const char mm_provider_threshold_low_binding[] =
"/nkn/nvsd/mm/config/provider/threshold/low";
static const char mm_evict_thread_time_binding[] =
"/nkn/nvsd/mm/config/evict_thread_time";

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mm_cfg_query()
 *	purpose : query for mm config parameters
 */
int
nvsd_mm_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG, "nvsd mm module mgmtd query initializing ..");

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    mm_config_prefix,
	    NULL,
	    nvsd_mm_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;
}	/* end of nvsd_mm_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mm_module_cfg_handle_change()
 *	purpose : handler for config changes for mm module
 */
int
nvsd_mm_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    UNUSED_ARGUMENT(old_bindings);

    /*
     * Call the callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    mm_config_prefix,
	    NULL,
	    nvsd_mm_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;
}	/* end of nvsd_mm_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mm_cfg_handle_change()
 *	purpose : handler for config changes for mm module
 */
static int
nvsd_mm_cfg_handle_change(const bn_binding_array * arr,
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

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*-------- PROVIDER THRESHOLD HIGH ------------*/
    if (ts_equal_str(name, mm_provider_threshold_high_binding, false)) {

	uint32 t_provider_threshold_high = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_provider_threshold_high);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../mm/config/provider_threshold_high as : \"%d\"",
		t_provider_threshold_high);

	/*
	 * Assign it the global 
	 */
	mm_prov_hi_threshold = t_provider_threshold_high;
    }

    /*-------- PROVIDER THRESHOLD LOW ------------*/
    if (ts_equal_str(name, mm_provider_threshold_low_binding, false)) {

	uint32 t_provider_threshold_low = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_provider_threshold_low);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../mm/config/provider_threshold_low as : \"%d\"",
		t_provider_threshold_low);

	/*
	 * Assign it the global 
	 */
	mm_prov_lo_threshold = t_provider_threshold_low;
    }

    /*-------- EVICT THREAD TIME ------------*/
    if (ts_equal_str(name, mm_evict_thread_time_binding, false)) {

	uint32 t_evict_thread_time = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_evict_thread_time);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../mm/config/evict_thread_time as : \"%d\"",
		t_evict_thread_time);

    }

bail:
    return err;
}	/* end of nvsd_mm_cfg_handle_change() */

/* ------------------------------------------------------------------------- */
/* End of nkn_mgmt_mm.c */
