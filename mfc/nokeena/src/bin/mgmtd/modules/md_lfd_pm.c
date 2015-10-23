/*
 *
 * Filename:    md_lfd_pm.c
 * Date:        2010-09-13
 * Author:      Sunil Mukundan
 *
 * (C) Copyright 2010 Juniper Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 * md_lfd_pm.c: shows how the live-mfp module is added to
 * the initial PM database.
 *
 * This module does not register any nodes of its own. Its sole purpose
 * is to add/change nodes in the initial database.
 *---------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "dso.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "nkn_mgmt_common.h"

/*----------------------------------------------------------------------------
 * PREPROCESSOR MACROS/CONSTANTS
 *---------------------------------------------------------------------------*/
#define NKN_LFD_PATH           "/opt/nkn/sbin/lfd"
#define NKN_LFD_WORKING_DIR    "/coredump/lfd"
#define NKN_LFD_CONFIG_FILE    "/config/nkn/lfd.conf.default"


/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_lfd_pm_ug_rules = NULL;

static const bn_str_value md_lfd_pm_initial_values[] = {
        /*
         * lfd : basic launch configuration.
	 * Load Feedback Daemon
	 * -F config file name
         */
    {"/pm/process/lfd/description", bt_string,
	" Load Feedback Daemon"},
    {"/pm/process/lfd/launch_enabled", bt_bool, "true"},
    {"/pm/process/lfd/auto_launch", bt_bool, "true"},
    {"/pm/process/lfd/working_dir", bt_string, NKN_LFD_WORKING_DIR},
    {"/pm/process/lfd/launch_path", bt_string, NKN_LFD_PATH},
    {"/pm/process/lfd/launch_uid", bt_uint16, "0"},
    {"/pm/process/lfd/launch_gid", bt_uint16, "0"},
    {"/pm/process/lfd/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_LFD},
    {"/pm/process/lfd/initial_launch_timeout", bt_duration_ms, NKN_LAUNCH_TIMEOUT_LFD},
    {"/pm/process/lfd/launch_params/1/param", bt_string, "-F"},
    {"/pm/process/lfd/launch_params/2/param", bt_string, NKN_LFD_CONFIG_FILE},
};

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_lfd_pm_init(const lc_dso_info *info, void *data);


static int md_lfd_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings(
	    commit,
	    db,
	    md_lfd_pm_initial_values,
	    sizeof(md_lfd_pm_initial_values)/ sizeof(bn_str_value));
    bail_error(err);

bail:
    return err;
}

int
md_lfd_pm_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Commit order of 200 is greater than the 0 used by most modules,
     * including md_pm.c.  This is to ensure that we can override some
     * of PM's global configuration, such as liveness grace period.
     *
     * We need modrf_namespace_unrestricted to allow us to set nodes
     * from our initial values function that are not underneath our
     * module root.
     */
    err = mdm_add_module(
                    "lfd_pm",                        // module_name
                    1,                                  // module_version
                    "/pm/nokeena/lfd",            // root_node_name
                    NULL,                               // root_config_name
                    0,                                  // md_mod_flags
                    NULL,                               // side_effects_func
                    NULL,                               // side_effects_arg
                    NULL,                               // check_func
                    NULL,                               // check_arg
                    NULL,                               // apply_func
                    NULL,                               // apply_arg
                    200,                                // commit_order
                    0,                                  // apply_order
                    md_generic_upgrade_downgrade,       // updown_func
                    &md_lfd_pm_ug_rules,                // updown_arg
                    md_lfd_pm_add_initial,             // initial_func
                    NULL,                               // initial_arg
                    NULL,                               // mon_get_func
                    NULL,                               // mon_get_arg
                    NULL,                               // mon_iterate_func
                    NULL,                               // mon_iterate_arg
                    &module);                           // ret_module
    bail_error(err);

bail:
    return err;
}

