
/*
 *
 * Filename:  nkn_mgmt_buffermgr.c
 * Date:      2008/11/23
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
#include "nkn_stat.h"

/* Local Function Prototypes */
int nvsd_buffermgr_cfg_query(const bn_binding_array * bindings);

int nvsd_buffermgr_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

int nvsd_buffermgr_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);

static int calculate_mem_sizes(void
	);

/* Global Variables */
uint32 glob_ram_cachesize;
uint32 g_ram_cachesize;
uint32 g_max_ram_cachesize_calculated;
uint32 g_ram_dictsize_MB;
uint32 g_cal_ram_dictsize_MB;

NKNCNT_DEF(max_ram_cachesize_calculated, uint32_t, "",
	"max ram cache calculated")

const char buffermgr_config_prefix[] = "/nkn/nvsd/buffermgr/config";

/* Local Variables */
static const char buffermgr_max_dictsize_binding[] =
"/nkn/nvsd/buffermgr/config/max_dictsize";
static const char buffermgr_max_cachesize_binding[] =
"/nkn/nvsd/buffermgr/config/max_cachesize";
static const char buffermgr_pageratio_binding[] =
"/nkn/nvsd/buffermgr/config/pageratio";
static const char buffermgr_attr_sync_interval_binding[] =
"/nkn/nvsd/buffermgr/config/attr_sync_interval";
static const char buffermgr_revalidate_window_binding[] =
"/nkn/nvsd/buffermgr/config/revalidate_window";
static const char buffermgr_revalidate_minsize_binding[] =
"/nkn/nvsd/buffermgr/config/revalidate_minsize";
static const char threadpool_stack_size_binding[] =
"/nkn/nvsd/buffermgr/config/threadpool_stack_size";
static const char buffermgr_lru_scale_binding[] =
"/nkn/nvsd/buffermgr/config/lru-scale";
static const char buffermgr_attributesize_binding[] =
"/nkn/nvsd/buffermgr/config/attributesize";
static const char buffermgr_attributecount_binding[] =
"/nkn/nvsd/buffermgr/config/attributecount";
static const char buffermgr_unsuccessful_cache_binding[] =
"/nkn/nvsd/buffermgr/config/unsuccessful_response/cachesize";

/* Extern Variables */
extern md_client_context *nvsd_mcc;
extern int64_t glob_bm_cfg_attr_sync_interval;
extern int nkn_buf_reval_time;
extern int nkn_buf_reval_size;
extern uint64_t glob_mm_def_stack_sz;
extern long bm_cfg_buf_lruscale;
extern int bm_min_attrsize;
extern uint32_t bm_min_attrcount;
extern uint32_t bm_negcache_percent;

/*
 *	funtion : nvsd_buffermgr_cfg_query()
 *	purpose : query for buffermgr config parameters
 */
int
nvsd_buffermgr_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG,
	    "nvsd buffermgr module mgmtd query initializing ..");

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    buffermgr_config_prefix,
	    NULL,
	    nvsd_buffermgr_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    /*
     * Now that all the ramcache nodes are read, run the memory calculation
     * routine 
     */
    calculate_mem_sizes();

bail:
    return err;

}	/* end of nvsd_buffermgr_cfg_query() */

/*
 *	funtion : nvsd_buffermgr_module_cfg_handle_change()
 *	purpose : handler for config changes for buffermgr module
 */
int
nvsd_buffermgr_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    UNUSED_ARGUMENT(old_bindings);

    /*
     * Call the callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    buffermgr_config_prefix,
	    NULL,
	    nvsd_buffermgr_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;

}	/* end of nvsd_buffermgr_module_cfg_handle_change() */

/*
 *	funtion : nvsd_buffermgr_cfg_handle_change()
 *	purpose : handler for config changes for buffermgr module
 *
 * Memory required for non-RAMcache is 750MB.  The biggest users are the kernel,
 * MM, and DM2.  Of this, we assign at least 400MB to kernel + MM.
 */
int
nvsd_buffermgr_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstr_array *name_parts = NULL;
    uint16 serv_port = 0;
    tbool bool_value = false;
    tbool *rechecked_licenses_p = data;

    (void) bool_value;
    (void) serv_port;
    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*-------- MAX CACHESIZE ------------*/
    if (ts_equal_str(name, buffermgr_max_cachesize_binding, false)) {

	err = bn_binding_get_uint32(binding, ba_value, NULL, &g_ram_cachesize);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../buffermgr/config/max_cachesize as : \"%d\"",
		g_ram_cachesize);
    } else if (ts_equal_str(name, buffermgr_pageratio_binding, false)) {

	err = bn_binding_get_uint32(binding, ba_value, NULL, &bm_page_ratio);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../buffermgr/config/pageratio as : \"%d\"",
		bm_page_ratio);
    }

    /*-------- MAX DICTIONARY SIZE ------------*/
    else if (ts_equal_str(name, buffermgr_max_dictsize_binding, false)) {

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &g_ram_dictsize_MB);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../buffermgr/config/max_dictsize as : \"%d\"",
		g_ram_dictsize_MB);
    } else if (ts_equal_str(name, buffermgr_attr_sync_interval_binding, false)) {
	uint32 t_attr_sync_interval = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_attr_sync_interval);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../buffermgr/config/attr_sync_interval as : \"%d\"",
		t_attr_sync_interval);

	glob_bm_cfg_attr_sync_interval = t_attr_sync_interval;
    } else if (ts_equal_str(name, buffermgr_revalidate_window_binding, false)) {
	uint32 t_revalidate_window = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_revalidate_window);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../buffermgr/config/revalidate_window as : \"%d\"",
		t_revalidate_window);

	nkn_buf_reval_time = t_revalidate_window;
    } else if (ts_equal_str(name, buffermgr_revalidate_minsize_binding, false)) {
	uint32 t_revalidate_minsize = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_revalidate_minsize);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../buffermgr/config/revalidate_minsize as : \"%d\"",
		t_revalidate_minsize);

	nkn_buf_reval_size = t_revalidate_minsize;
    }

    /*-------- THREAD-POOL STACK SIZE ------------*/
    else if (ts_equal_str(name, threadpool_stack_size_binding, false)) {
	uint64 t_threadpool_stack_size;

	err = bn_binding_get_uint64(binding,
		ba_value, NULL, &t_threadpool_stack_size);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../buffermgr/config/threadpool_stack_size as : \"%ld\"",
		t_threadpool_stack_size);

	glob_mm_def_stack_sz = t_threadpool_stack_size;
    }

    /*-------- LRU scale factor ------------*/
    else if (ts_equal_str(name, buffermgr_lru_scale_binding, false)) {
	uint16 t_lru_scale;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_lru_scale);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../buffermgr/config/lru-scale as : \"%d\"",
		t_lru_scale);

	bm_cfg_buf_lruscale = t_lru_scale;

    } else if (ts_equal_str(name, buffermgr_attributesize_binding, false)) {
	uint16 t_attributesize;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_attributesize);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../buffermgr/config/attributesize as : \"%d\"",
		t_attributesize);

	bm_min_attrsize = t_attributesize;
    } else if (ts_equal_str(name, buffermgr_attributecount_binding, false)) {
	uint32 t_attributecount;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_attributecount);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../buffermgr/config/attributecount as : \"%d\"",
		t_attributecount);

	bm_min_attrcount = t_attributecount;
    } else if (ts_equal_str(name, buffermgr_unsuccessful_cache_binding, false)) {
	uint32 t_unsuccessful_cache;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_unsuccessful_cache);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../buffermgr/config/unsuccessful_response/cachesize as : \"%d\"",
		t_unsuccessful_cache);

	bm_negcache_percent = t_unsuccessful_cache;
    }

bail:
    tstr_array_free(&name_parts);

    return err;
}	/* end of nvsd_buffermgr_cfg_handle_change() */

/*
 *	funtion : calculate_mem_sizes()
 *	purpose : calculate the different memory sizes based on
 *		configured values
 *
 * Memory required for non_RAMcache is 750MB.  The biggest users are the kernel,
 * MM, and DM2.  Of this, we assigned at least 400MB to kernel + MM.
 */
static int
calculate_mem_sizes(void)
{
#define MIN_SYSTEM_MEM_MB 800
    int err = 0;
    tstring *t_str = NULL;
    uint32 t_total_physical_mem_KB;
    uint32 t_mem_for_others_MB;	/* total memory available for all others *
				 * dm2 + nvsd + system */
    uint32 t_total_physical_mem_MB;	/* total memory available */
    uint32 t_min_system_mem_MB;	/* memory available for system */
    uint32 t_min_non_dict_mem_MB;	/* memory for nvsd excluding dict */

    /*
     * BELOW LOGIC IS TO SET A REASONABLE VALUE FOR CACHE SIZE 
     */
    /*
     * Get the value of this system/memory stat node 
     */
    err = mdc_get_binding_tstr(nvsd_mcc, NULL, NULL, NULL, &t_str,
	    "/system/memory/physical/total", NULL);
    bail_error(err);

    /*
     * Get the value out of TM 
     */
    t_total_physical_mem_KB = atoi(ts_str(t_str));
    t_total_physical_mem_MB = t_total_physical_mem_KB / 1024;


    /*
     * Logic used here is the following --->> If max_cachesize is set to 0,
     * then RAM cachesize is decided by the following : Total RAM <= 4GB : -
     * 30% for system - 256MB for nvsd non-dict - 256MB for dict Total RAM
     * between 4GB and 8GB: - 40% for system - 500MB for non-dict - 750MB for 
     * dict memory Total RAM between 8GB and 16GB: - 40% for system - 750MB
     * for non-dict - 1GB for dict memory Total RAM between 16GB and 36GB: -
     * 30% for system - 1.2GB for non-dict - 3GB for dict memory Total RAM >
     * 36GB - 12GB for system - 0.5% for non-dict - 10% for dict memory 
     */
    if (t_total_physical_mem_MB <= 4 * 1024) {	// if <= 4GB
	t_min_system_mem_MB = (t_total_physical_mem_MB * 3) / 10;
	t_min_non_dict_mem_MB = 256;	// 1/4 GB
	nkn_max_dict_mem_MB = 256;	// 1/4 MB
    } else if (t_total_physical_mem_MB <= 8 * 1024) {	// if <= 8GB
	t_min_system_mem_MB = (t_total_physical_mem_MB * 4) / 10;
	t_min_non_dict_mem_MB = 512;	// 1/2 GB
	nkn_max_dict_mem_MB = 768;	// 3/4 GB
    } else if (t_total_physical_mem_MB <= 16 * 1024) {	// if <= 16GB
	t_min_system_mem_MB = (t_total_physical_mem_MB * 4) / 10;
	t_min_non_dict_mem_MB = 768;	// 3/4 GB
	nkn_max_dict_mem_MB = 1024;	// 1.0 GB
    } else if (t_total_physical_mem_MB <= 36 * 1024) {	// if <= 36GB
	t_min_system_mem_MB = (t_total_physical_mem_MB * 3) / 10;
	t_min_non_dict_mem_MB = 1.2 * 1024;	// 1.2 GB
	nkn_max_dict_mem_MB = 3 * 1024;	// 3.0 GB
    } else {					// if > 36GB
	/*
	 * We don't want the Linux kernel to continue taking more memory just 
	 * because we have more physical memory. 
	 */
	t_min_system_mem_MB = 12 * 1024;
	/*
	 * AM consumes 800MB when physical memory > 16GB 
	 */
	t_min_non_dict_mem_MB = t_total_physical_mem_MB / 25;	// 4%
	/*
	 * Giving 7.2GB on a 72GB system seems reasonable.  We might be able
	 * to give 8.5% on the low end. 
	 */
	nkn_max_dict_mem_MB = t_total_physical_mem_MB / 10;	// 10%
    }
    /*
     * Total memory except the ram-cache 
     */
    t_mem_for_others_MB = t_min_system_mem_MB + t_min_non_dict_mem_MB +
	nkn_max_dict_mem_MB;

    /*
     * With little memory, need at least 1GB memory in system 
     */
    if (t_mem_for_others_MB > t_total_physical_mem_MB)
	assert(0);

    g_max_ram_cachesize_calculated =
	t_total_physical_mem_MB - t_mem_for_others_MB;

    /*
     * Set the calculated size for default setting 
     */
    if (0 == g_ram_cachesize) {
	/*
	 * Set g_ram_cachesize to what is calculated 
	 */
	g_ram_cachesize = g_max_ram_cachesize_calculated;
    }
    /*
     * Check if configured g_ram_cachesize has left at least
     * MIN_SYSTEM_MEM_MB for others 
     */
    else if (g_ram_cachesize > (t_total_physical_mem_MB - MIN_SYSTEM_MEM_MB)) {
	/*
	 * Give MIN_SYSTEM_MEM_MB for others 
	 */
	g_ram_cachesize = t_total_physical_mem_MB - MIN_SYSTEM_MEM_MB;
    }
    /*
     * Adding a nkn counter to be accessed in gmgmthd to get this value 
     */
    glob_max_ram_cachesize_calculated = g_max_ram_cachesize_calculated;

    /*
     * Set the global variable for the module 
     */
    /*
     * convert from MBytes to Bytes. 
     */
    cm_max_cache_size = (long) g_ram_cachesize *(long) 1024 *(long) 1024;
    glob_ram_cachesize = g_ram_cachesize;

    if (g_ram_dictsize_MB)
	nkn_max_dict_mem_MB = g_ram_dictsize_MB;

    g_cal_ram_dictsize_MB = nkn_max_dict_mem_MB;

bail:
    ts_free(&t_str);
    return err;
}	/* end of calculate_mem_sizes() */
/* End of nkn_mgmt_buffermgr.c */
