
/*
 *
 * Filename:  nvsd_mgmt_am.c
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
#include "nkn_cfg_params.h"
#include "nkn_defs.h"
#include "nkn_diskmgr2_api.h"
#include "nkn_am_api.h"
#include "nvsd_mgmt_lib.h"

/** Global variables **/
uint32 g_cal_AM_mem_limit;

/* Local Function Prototypes */
int nvsd_am_cfg_query(const bn_binding_array * bindings);

int nvsd_am_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

static int
nvsd_am_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);

/* Extern Variables */
extern md_client_context *nvsd_mcc;

/* Local Variables */
const char am_config_prefix[] = "/nkn/nvsd/am/config";
static const char am_cache_promotion_enabled_binding[] =
"/nkn/nvsd/am/config/cache_promotion_enabled";
static const char am_cache_promotion_size_threshold_binding[] =
"/nkn/nvsd/am/config/cache_promotion/size_threshold";
static const char am_cache_ingest_hits_threshold_binding[] =
"/nkn/nvsd/am/config/cache_ingest_hits_threshold";
static const char am_cache_ingest_size_threshold_binding[] =
"/nkn/nvsd/am/config/cache_ingest_size_threshold";
static const char am_cache_ingest_last_eviction_time_diff_binding[] =
"/nkn/nvsd/am/config/cache_ingest_last_eviction_time_diff";
static const char am_cache_evict_aging_time_binding[] =
"/nkn/nvsd/am/config/cache_evict_aging_time";
static const char am_cache_hotness_increment[] =
"/nkn/nvsd/am/config/cache_promotion/hotness_increment";
static const char am_cache_hotness_threshold[] =
"/nkn/nvsd/am/config/cache_promotion/hotness_threshold";
static const char am_cache_hit_increment[] =
"/nkn/nvsd/am/config/cache_promotion/hit_increment";
static const char am_cache_hit_threshold[] =
"/nkn/nvsd/am/config/cache_promotion/hit_threshold";
static const char am_cache_ingest_queue_depth_binding[] =
"/nkn/nvsd/am/config/cache_ingest_queue_depth";
static const char am_cache_ingest_object_timeout_binding[] =
"/nkn/nvsd/am/config/cache_ingest_object_timeout";
static const char am_cache_ingest_policy_binding[] =
"/nkn/nvsd/am/config/cache_ingest_policy";
static const char am_memory_limit[] = "/nkn/nvsd/am/config/memory_limit";
static const char am_cache_ingest_ignore_hotness_check[] =
"/nkn/nvsd/am/config/cache_ingest_ignore_hotness_check";

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_am_cfg_query()
 *	purpose : query for am config parameters
 */
int
nvsd_am_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG, "nvsd am module mgmtd query initializing ..");

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    am_config_prefix,
	    NULL,
	    nvsd_am_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);
bail:
    return err;

}								/* end of nvsd_am_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_am_module_cfg_handle_change()
 *	purpose : handler for config changes for am module
 */
int
nvsd_am_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    UNUSED_ARGUMENT(old_bindings);

    /*
     * Call the callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    am_config_prefix,
	    NULL,
	    nvsd_am_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;

}	/* end of nvsd_am_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_am_cfg_handle_change()
 *	purpose : handler for config changes for am module
 */
static int
nvsd_am_cfg_handle_change(const bn_binding_array * arr,
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
    tstring *t_str = NULL;
    uint32 t_total_physical_mem_KB;
    uint32 t_total_physical_mem_MB;	/* total memory available */

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*-------- CACHE PROMOTION ENABLED ------------*/
    if (ts_equal_str(name, am_cache_promotion_enabled_binding, false)) {

	tbool t_cache_promotion_enabled;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_cache_promotion_enabled);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_promotion_enabled as : \"%s\"",
		t_cache_promotion_enabled ? "TRUE" : "FALSE");

	/*
	 * Assign it the global 
	 */
	if (t_cache_promotion_enabled)
	    am_cache_promotion_enabled = 1;
	else
	    am_cache_promotion_enabled = 0;
    }

    /*-------- CACHE PROMOTION SIZE THRESHOLD ---------*/
    if (ts_equal_str(name, am_cache_promotion_size_threshold_binding, false)) {
	uint32 t_cache_promotion_size_threshold;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL,
		&t_cache_promotion_size_threshold);
	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_promotion/size_threshold as : \"%d\"",
		t_cache_promotion_size_threshold);
	g_am_promote_size_threshold = t_cache_promotion_size_threshold;
    }

    /*-------- CACHE INGEST HITS THRESHOLD ------------*/
    if (ts_equal_str(name, am_cache_ingest_hits_threshold_binding, false)) {

	uint32 t_cache_ingest_hits_threshold = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL,
		&t_cache_ingest_hits_threshold);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_ingest_hits_threshold as : \"%d\"",
		t_cache_ingest_hits_threshold);

	/*
	 * Assign it the global 
	 */
	am_cache_ingest_hits_threshold = t_cache_ingest_hits_threshold;
    }

    /*-------- CACHE INGEST SIZE THRESHOLD ------------*/
    if (ts_equal_str(name, am_cache_ingest_size_threshold_binding, false)) {

	uint32 t_cache_ingest_size_threshold = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL,
		&t_cache_ingest_size_threshold);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_ingest_size_threshold as : \"%d\"",
		t_cache_ingest_size_threshold);

	/*
	 * There are 2 variables to be set If size greater than 0 then set
	 * nkn_am_ingest_small_obj_enable to 1 and
	 * nkn_am_ingest_small_obj_size to the size else set
	 * nkn_am_ingest_small_obj_enable to 0 
	 */
	if (t_cache_ingest_size_threshold)
	    nkn_am_ingest_small_obj_enable = 1;
	else
	    nkn_am_ingest_small_obj_enable = 0;
	nkn_am_ingest_small_obj_size = t_cache_ingest_size_threshold;
    }

    /*-------- CACHE INGEST LAST EVICTION TIME DIFF ------------*/
    if (ts_equal_str
	    (name, am_cache_ingest_last_eviction_time_diff_binding, false)) {

	int32 t_cache_ingest_last_eviction_time_diff = 0;

	err = bn_binding_get_duration_sec(binding,
		ba_value, NULL,
		&t_cache_ingest_last_eviction_time_diff);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_ingest_last_eviction_time_diff as : \"%d\"",
		t_cache_ingest_last_eviction_time_diff);

	/*
	 * Set the global variable for the module 
	 */
	am_cache_ingest_last_eviction_time_diff =
	    t_cache_ingest_last_eviction_time_diff;
    }

    /*-------- CACHE EVICT AGING TIME ------------*/
    if (ts_equal_str(name, am_cache_evict_aging_time_binding, false)) {

	int32 t_cache_evict_aging_time = 0;

	err = bn_binding_get_duration_sec(binding,
		ba_value, NULL,
		&t_cache_evict_aging_time);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_evict_aging_time as : \"%d\"",
		t_cache_evict_aging_time);

	/*
	 * Set the global variable for the module 
	 */
	am_cache_evict_aging_time = t_cache_evict_aging_time;
    }

    /*-------- CACHE PROMOTION HOTNESS INCREMENT ------------*/
    if (ts_equal_str(name, am_cache_hotness_increment, false)) {
	uint32 t_cache_hotness_increment = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_cache_hotness_increment);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_promotion/hotness_increment as : \"%d\"",
		t_cache_hotness_increment);

	/*
	 * Assign it the global 
	 */
	am_cache_promotion_hotness_increment = t_cache_hotness_increment;
    }

    /*-------- CACHE PROMOTION HOTNESS THRESHOLD ------------*/
    if (ts_equal_str(name, am_cache_hotness_threshold, false)) {
	uint32 t_cache_hotness_threshold = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_cache_hotness_threshold);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_promotion/hotness_threshold as : \"%d\"",
		t_cache_hotness_threshold);

	/*
	 * Assign it the global 
	 */
	am_cache_promotion_hotness_threshold = t_cache_hotness_threshold;
	/*
	 * Call the DM2 function to update tier specific thresholds 
	 */
	DM2_mgmt_update_am_thresholds();
    }

    /*-------- CACHE PROMOTION HIT INCREMENT ------------*/
    if (ts_equal_str(name, am_cache_hit_increment, false)) {
	uint32 t_cache_hit_increment = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_cache_hit_increment);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_promotion/hit_increment as : \"%d\"",
		t_cache_hit_increment);

	/*
	 * Assign it the global 
	 */
	am_cache_promotion_hit_increment = t_cache_hit_increment;
    }

    /*-------- CACHE PROMOTION HIT THRESHOLD ------------*/
    if (ts_equal_str(name, am_cache_hit_threshold, false)) {
	uint32 t_cache_hit_threshold = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_cache_hit_threshold);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_promotion/hit_threshold as : \"%d\"",
		t_cache_hit_threshold);

	/*
	 * Assign it the global 
	 */
	am_cache_promotion_hit_threshold = t_cache_hit_threshold;
    }

    /*-------- CACHE INGEST QUEUE DEPTH ------------*/
    if (ts_equal_str(name, am_cache_ingest_queue_depth_binding, false)) {
	uint32 t_cache_ingest_queue_depth = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL,
		&t_cache_ingest_queue_depth);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_ingest_queue_depth as : \"%d\"",
		t_cache_ingest_queue_depth);

	/*
	 * Assign it the global 
	 */
	nkn_am_max_origin_entries = t_cache_ingest_queue_depth;
    }

    /*-------- CACHE INGEST QUEUE DEPTH ------------*/
    if (ts_equal_str(name, am_cache_ingest_object_timeout_binding, false)) {
	uint32 t_cache_ingest_object_timeout = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL,
		&t_cache_ingest_object_timeout);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_ingest_object_timeout as : \"%d\"",
		t_cache_ingest_object_timeout);

	/*
	 * Assign it the global 
	 */
	nkn_am_origin_tbl_timeout = t_cache_ingest_object_timeout;
    }

    /*-------- CACHE INGEST POLICY ------------*/
    if (ts_equal_str(name, am_cache_ingest_policy_binding, false)) {
	uint32 t_cache_ingest_policy = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_cache_ingest_policy);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_ingest_policy as : \"%d\"",
		t_cache_ingest_policy);

	/*
	 * Assign it the global 
	 */
	am_cache_ingest_policy = t_cache_ingest_policy;
    }

    /*--------Set the AM memory limit-------------*/
    if (ts_equal_str(name, am_memory_limit, false)) {
	uint32 t_am_memory = 0;

	if (!t_am_memory) {
	    /*
	     * Get the value of this system/memory stat node 
	     */
	    err = mdc_get_binding_tstr(nvsd_mcc, NULL, NULL,
		    NULL, &t_str,
		    "/system/memory/physical/total", NULL);
	    bail_error(err);

	    /*
	     * Get the value out of TM 
	     */
	    t_total_physical_mem_KB = atoi(ts_str(t_str));
	    t_total_physical_mem_MB = t_total_physical_mem_KB / 1024;

	    if (t_total_physical_mem_MB <= 4096) {
		/*
		 * less than 4GB 
		 */
		t_am_memory = 100;
	    } else if (t_total_physical_mem_MB <= 8192) {
		/*
		 * less than 8GB 
		 */
		t_am_memory = 400;
	    } else if (t_total_physical_mem_MB <= 16384) {
		/*
		 * less than 16GB 
		 */
		t_am_memory = 600;
	    } else {
		/*
		 * greater than 16GB 
		 */
		t_am_memory = 800;
	    }
	}

	nkn_am_memory_limit = t_am_memory * 1024 * 1024;	/* Convert it to
								 * Bytes */
	g_cal_AM_mem_limit = t_am_memory;
    }

    /*--------Set the AM memory limit-------------*/
    if (ts_equal_str(name, am_cache_ingest_ignore_hotness_check, false)) {
	uint32 t_am_hotness_check = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_am_hotness_check);
	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../am/config/cache_ingest_ignore_hotness_check as : \"%d\"",
		t_am_hotness_check);
	glob_am_ingest_ignore_hotness_check = t_am_hotness_check;
    }
bail:
    ts_free(&t_str);
    return err;

}	/* end of nvsd_am_cfg_handle_change() */


/* End of nkn_mgmt_am.c */
