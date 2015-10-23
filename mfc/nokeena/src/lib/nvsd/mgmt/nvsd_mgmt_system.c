/*
 *
 * Filename:  nkn_mgmt_system.c
 * Date:      2008/11/23
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

/* Standard Headers */
#include <sys/time.h>
#include <sys/resource.h>

/* Samara Headers */
#include "md_client.h"
#include "license.h"

/* NVSD headers */
#include "nkn_defs.h"
#include "nkn_cfg_params.h"
#include "nkn_util.h"
#include "nvsd_mgmt_lib.h"

/* Local Function Prototypes */
int nvsd_system_cfg_query(const bn_binding_array * bindings);

int nvsd_system_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

int nvsd_system_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data);

int nvsd_debug_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data);

/* Extern Variables */
extern md_client_context *nvsd_mcc;
extern int nkn_assert_enable;
extern int console_log_level;
extern uint64_t console_log_mod;
extern int user_space_l3_pxy;
extern int kernel_space_l3_pxy;
extern int service_init_flags;
int glob_sched_task_total_cnt;
int glob_rtsched_num_core_threads;
int glob_sched_num_core_threads;

extern uint32_t g_nkn_num_stat_threads;
extern uint32_t g_nkn_num_get_threads;
extern uint32_t g_nkn_num_put_threads;
extern uint32_t g_nkn_num_val_threads;
extern uint32_t g_nkn_num_upd_threads;

extern int debug_run_command(const char *line);

extern int
nvsd_mgmt_main_tgl_pxy(int pxy_enable);

/* Local Variables */
const char system_config_prefix[] = "/nkn/nvsd/system/config";
static const char system_debug_level_binding[] =
"/nkn/nvsd/system/config/debug/level";
static const char system_debug_mod_binding[] =
"/nkn/nvsd/system/config/debug/mod";
static const char system_debug_command_binding[] =
"/nkn/nvsd/system/config/debug/command";
char debug_config_prefix[] = "/nkn/nvsd/debug/config";
static const char system_rtsched_thread_config_prefix[] =
"/nkn/nvsd/system/config/rtsched/threads";
static const char system_sched_thread_config_prefix[] =
"/nkn/nvsd/system/config/sched/threads";
static const char system_sched_task_config_prefix[] =
"/nkn/nvsd/system/config/sched/tasks";
static const char system_coredump_config_prefix[] =
"/nkn/nvsd/system/config/coredump";
static const char system_bypass_config_prefix[] =
"/nkn/nvsd/system/config/bypass";
static const char system_inits_config_preread[] =
"/nkn/nvsd/system/config/init/preread";

#ifdef USE_MFD_LICENSE
// MFD license state.  True when an active MFD license is installed.
static tbool nvsd_mfd_licensed = false;

/*
 * This binding is set automatically by the licensing system when
 * licenses pertinent to MFD are installed.
 * The binding will have a boolean saying whether or not that particular
 * license is installed.
 */
static const char *nkn_mfd_lic_binding =
"/nkn/mfd/licensing/config/mfd_licensed";
#endif /*USE_MFD_LICENSE*/

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_system_cfg_query()
 *	purpose : query for system config parameters
 */
int
nvsd_system_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;
    bn_binding_array *bindings_tmp = NULL;
    system_cfg_chng_ctxt_t system_cfg_chng_ctxt;
    tstring *t_no_of_threads = NULL;
    tstring *t_no_of_sched_threads = NULL;
    uint32_t num_cpus = 0;

    tstring *t_no_of_sched_tasks = NULL;

    system_cfg_chng_ctxt.is_db_change = 0;

    lc_log_basic(LOG_DEBUG, "nvsd system module mgmtd query initializing ..");

    err =
	mgmt_lib_mdc_foreach_binding_prequeried(bindings, system_config_prefix,
		NULL,
		nvsd_system_cfg_handle_change,
		&system_cfg_chng_ctxt);
    bail_error(err);

#ifdef USE_MFD_LICENSE

    /*
     * License Checking 
     */
    err =
	mgmt_lib_mdc_foreach_binding_prequeried(bindings,
		"/nkn/mfd/licensing/config",
		NULL,
		nvsd_system_cfg_handle_change,
		&rechecked_licenses);
    bail_error(err);

    /*
     * License Checking 
     */
    err =
	mgmt_lib_mdc_foreach_binding_prequeried(bindings, "/license/key", NULL,
		nvsd_system_cfg_handle_change,
		&rechecked_licenses);
    bail_error(err);
#endif /*USE_MFD_LICENSE*/
    err =
	mgmt_lib_mdc_foreach_binding_prequeried(bindings, debug_config_prefix,
		NULL,
		nvsd_debug_cfg_handle_change,
		&rechecked_licenses);
    bail_error(err);

    // TODO get it from the binding
    err = mdc_get_binding_tstr(nvsd_mcc, NULL, NULL, NULL, &t_no_of_threads,
	    system_rtsched_thread_config_prefix, NULL);
    bail_error(err);

    // TODO Get this from the binding
    /*
     * Get the number of cores first 
     */
    err = mdc_get_binding_children(nvsd_mcc,
	    NULL, NULL, true, &bindings_tmp, false,
	    false, "/system/cpu/indiv");
    bail_error(err);

    if (bindings_tmp) {
	err = bn_binding_array_length(bindings_tmp, &num_cpus);
	bail_error(err);
	bn_binding_array_free(&bindings_tmp);
    }

    if (t_no_of_threads == NULL) {

	/*
	 * Setting the default value for the number of threads as set in the
	 * mgmtd module 
	 */
	err = 1;
	if (num_cpus >= 8)
	    glob_rtsched_num_core_threads = 4;
	else
	    glob_rtsched_num_core_threads = 2;
	bail_error(err);
    }
    lc_log_basic(LOG_DEBUG, "No Of realtime threads:%d\n",
	    atoi(ts_str(t_no_of_threads)));
    glob_rtsched_num_core_threads = atoi(ts_str(t_no_of_threads));
    if (!glob_rtsched_num_core_threads) {
	if (num_cpus >= 8)
	    glob_rtsched_num_core_threads = 4;
	else
	    glob_rtsched_num_core_threads = 2;
    }
    // TODO- Get this from the binding
    err =
	mdc_get_binding_tstr(nvsd_mcc, NULL, NULL, NULL, &t_no_of_sched_threads,
		system_sched_thread_config_prefix, NULL);
    bail_error(err);

    if (t_no_of_sched_threads == NULL) {
	/*
	 * Setting the default value for the number of threads as set in the
	 * mgmtd module 
	 */
	err = 1;
	if (num_cpus > 24) {
	    g_nkn_num_stat_threads = g_nkn_num_stat_threads / 2;
	    g_nkn_num_get_threads = g_nkn_num_get_threads / 3 - 1;
	    g_nkn_num_put_threads = g_nkn_num_put_threads / 3 + 1;
	    g_nkn_num_val_threads = g_nkn_num_val_threads / 3 + 1;
	    g_nkn_num_upd_threads = g_nkn_num_upd_threads / 3 + 1;
	    glob_sched_num_core_threads = 4;
	} else if (num_cpus >= 8)
	    glob_sched_num_core_threads = 2;
	else
	    glob_sched_num_core_threads = 1;
	bail_error(err);
    }
    lc_log_basic(LOG_DEBUG, "No Of scheduler threads:%d\n",
	    atoi(ts_str(t_no_of_sched_threads)));
    glob_sched_num_core_threads = atoi(ts_str(t_no_of_sched_threads));
    if (!glob_sched_num_core_threads) {
	if (num_cpus > 24) {
	    g_nkn_num_stat_threads = g_nkn_num_stat_threads / 2;
	    g_nkn_num_get_threads = g_nkn_num_get_threads / 3 - 1;
	    g_nkn_num_put_threads = g_nkn_num_put_threads / 3 + 1;
	    g_nkn_num_val_threads = g_nkn_num_val_threads / 3 + 1;
	    g_nkn_num_upd_threads = g_nkn_num_upd_threads / 3 + 1;
	    glob_sched_num_core_threads = 4;
	} else if (num_cpus >= 8)
	    glob_sched_num_core_threads = 2;
	else
	    glob_sched_num_core_threads = 1;
    }
    // TODO-Get this from the binding
    err = mdc_get_binding_tstr(nvsd_mcc, NULL, NULL, NULL, &t_no_of_sched_tasks,
	    system_sched_task_config_prefix, NULL);
    bail_error(err);

    if (t_no_of_sched_tasks == NULL) {
	/*
	 * Setting the default value for the number of tasks as set in the
	 * mgmtd module 
	 */
	err = 1;
	glob_sched_task_total_cnt = 256 * 1000;
	bail_error(err);
    }
    lc_log_basic(LOG_DEBUG, "No Of scheduler tasks:%d\n",
	    atoi(ts_str(t_no_of_sched_tasks)));
    glob_sched_task_total_cnt = (atoi(ts_str(t_no_of_sched_tasks)) * 1000);

bail:
    ts_free(&t_no_of_threads);
    ts_free(&t_no_of_sched_threads);
    ts_free(&t_no_of_sched_tasks);
    return err;
}	/* end of nvsd_system_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_system_module_cfg_handle_change()
 *	purpose : handler for config changes for system module
 */
int
nvsd_system_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;
    system_cfg_chng_ctxt_t system_cfg_chng_ctxt;

    system_cfg_chng_ctxt.is_db_change = 1;

    UNUSED_ARGUMENT(old_bindings);

    /*
     * Call the callbacks 
     */
    err =
	mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
		system_config_prefix, NULL,
		nvsd_system_cfg_handle_change,
		&system_cfg_chng_ctxt);
    bail_error(err);

    /*
     * Call the callbacks 
     */
    err =
	mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
		debug_config_prefix, NULL,
		nvsd_debug_cfg_handle_change,
		&rechecked_licenses);
    bail_error(err);

bail:
    return err;
}	/* end of * nvsd_system_module_cfg_handle_change() */

static int
nvsd_system_cfg_kernel_space_tunnel(int mode)
{
    FILE *FP = NULL;
    int rv = -1;

    FP = fopen("/proc/tproxy/tp_forward", "w");
    if (FP) {
	rv = fprintf(FP, "%d", mode);
	fclose(FP);
	return 0;
    }
    return -1;
}

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_system_cfg_handle_change()
 *	purpose : handler for config changes for system module
 */
int
nvsd_system_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    uint16 server_port = 0;
    system_cfg_chng_ctxt_t *system_cfg_chng_ctxt = data;

    (void) server_port;
    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(system_cfg_chng_ctxt);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*-------- Debug level ------------*/
    if (ts_equal_str(name, system_debug_level_binding, false)) {
	uint32 level = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &level);

	bail_error(err);
	console_log_level = level;
	lc_log_basic(LOG_DEBUG,
		"Read .../system/config/debug/level as : \"%d\"", level);
    } else if (ts_equal_str(name, system_bypass_config_prefix, false)) {

	uint32_t en = 0;
	tbool ip_fwd_ena = false;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &en);
	bail_error(err);

	switch (en) {
	    case 0:
		user_space_l3_pxy = kernel_space_l3_pxy = 0;
		break;
	    case 1:
		user_space_l3_pxy = 1;
		kernel_space_l3_pxy = 0;
		break;
	    case 2:
		user_space_l3_pxy = 0;
		kernel_space_l3_pxy = 1;
		break;
	}

	/*
	 * get the value for Ip-FWD MDC 
	 */
	// TODO-TR-Query only during initial launch
	err = mdc_get_binding_tbool(nvsd_mcc, NULL, NULL, NULL,
		&ip_fwd_ena,
		"/nkn/nvsd/network/config/ip-forward",
		NULL);
	bail_error(err);

	/*
	 * User space L3 proxy Restor IP table rules Disable IP fwd 
	 */

	/*
	 * Don't enable user space l3 proxy for a db change event enable it
	 * only for cfg_query i.e startup 
	 */

	if (!(system_cfg_chng_ctxt->is_db_change) && user_space_l3_pxy
		&& ip_fwd_ena) {
	    err = nvsd_mgmt_main_tgl_pxy(2);
	    bail_error(err);
	} else if (!(system_cfg_chng_ctxt->is_db_change) && user_space_l3_pxy) {
	    err = nvsd_mgmt_main_tgl_pxy(0);
	    bail_error(err);
	}
	if (kernel_space_l3_pxy) {
	    err = nvsd_system_cfg_kernel_space_tunnel(1);
	    bail_error(err);
	} else {
	    nvsd_system_cfg_kernel_space_tunnel(0);
	}

    }

    /*-------- debug mod ------------*/
    else if (ts_equal_str(name, system_debug_mod_binding, false)) {
	tstring *mod = NULL;
	char strmod[256];
	char *p;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &mod);

	bail_error(err);
	snprintf(strmod, sizeof (strmod), "%s", ts_str(mod));
	p = &strmod[0];
	if ((strlen(p) >= 3) && (p[0] == '0') && (p[1] == 'x')) {
	    //console_log_mod = convert_hexstr_2_int64(p + 2);
	} else {
	    //console_log_mod = atoi(p);
	}
	lc_log_basic(LOG_DEBUG, "Read .../system/config/debug/mod as : %s",
		ts_str(mod));
	ts_free(&mod);
    } else if (ts_equal_str(name, system_debug_command_binding, false)) {
	tstring *mod = NULL;
	char strmod[256];

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &mod);

	bail_error(err);
	snprintf(strmod, sizeof (strmod), "%s", ts_str(mod));
	debug_run_command(strmod);
	lc_log_basic(LOG_DEBUG, "Read .../system/config/debug/command as : %s",
		strmod);
	ts_free(&mod);
    }
    /*
     * Get the value of flag that controls coredump for nvsd 
     */
    /*
     * Node: /nkn/nvsd/system/config/coredump 
     */
    else if (ts_equal_str(name, system_coredump_config_prefix, false)) {
	uint32 nvsd_coredump;
	struct rlimit rlim;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &nvsd_coredump);
	bail_error(err);

	lc_log_basic(LOG_DEBUG, "Read .../system/config/coredump as : %d",
		nvsd_coredump);

	/*
	 * Call the ulimit API to set the value for coredump 
	 */
	if (1 == nvsd_coredump) {	// Coredump enabled
	    /*
	     * Set coredump size to unlimited 
	     */
	    rlim.rlim_cur = RLIM_INFINITY;
	    rlim.rlim_max = RLIM_INFINITY;
	} else {
	    /*
	     * Set coredump size to 0 
	     */
	    rlim.rlim_cur = 0;
	    rlim.rlim_max = 0;
	}
	/*
	 * Set the coredump size using ulimit api 
	 */
	setrlimit(RLIMIT_CORE, &rlim);
    } else if (ts_equal_str(name, system_inits_config_preread, false)) {
	tbool init_preread = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &init_preread);
	bail_error(err);

	if (init_preread) {
	    service_init_flags |= PREREAD_INIT;
	} else {
	    service_init_flags &= ~PREREAD_INIT;
	}

    }
#ifdef USE_MFD_LICENSE
    else if (ts_equal_str(name, nkn_mfd_lic_binding, false)) {
	// lc_log_basic(LOG_NOTICE, "Matched %s", nkn_mfd_lic_binding);
	err = bn_binding_get_tbool(binding, ba_value, NULL, &nvsd_mfd_licensed);
	if (err)
	    lc_log_basic(LOG_ERR,
		    "bn_binding_get_tbool(...,&nvsd_mfs_licensed) ret %d",
		    err);
	if (nvsd_mfd_licensed)
	    lc_log_basic(LOG_NOTICE, "MFD licensed TRUE");
	else
	    lc_log_basic(LOG_NOTICE, "MFD licensed FALSE");

	bail_error(err);
    }
#endif /*USE_MFD_LICENSE*/
bail:
    return err;
}	/* end of nvsd_system_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_debug_cfg_handle_change()
 *	purpose : handler for config changes for debug module
 */
int
nvsd_debug_cfg_handle_change(const bn_binding_array * arr,
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

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*-------- assert flag ------------*/
    if (ts_equal_str(name, "/nkn/nvsd/debug/config/assert/enabled", false)) {
	tbool t_assert_enabled = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_assert_enabled);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../debug/config/assert/enabled as : \"%s\"",
		t_assert_enabled ? "true" : "false");
	nkn_assert_enable = t_assert_enabled;
    }
bail:
    return err;
}	/* end of nvsd_debug_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/* End of nkn_mgmt_system.c */
