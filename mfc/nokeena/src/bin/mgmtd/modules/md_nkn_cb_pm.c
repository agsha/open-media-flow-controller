/*
 *
 * Filename:    md_nkn_cb_pm.c
 *
 * (C) Copyright 2012 Nokeena Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 * md_nkn_cb_pm.c: shows how the nkn_cb module is added to
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
#define NKN_CB_PATH 		"/opt/nkn/sbin/cb"
#define NKN_CB_WORKING_DIR 	"/coredump/cb"
#define NKN_CB_LOGLEVEL   	"6"
#define NKN_CB_LISTENPORT   	"19090"

/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_cb_pm_ug_rules = NULL;

static const bn_str_value md_nkn_cb_pm_initial_values[] = {
    /*
     * nkn-cb : basic launch configuration.
     */
    {"/pm/process/cb/launch_enabled", bt_bool, "true"},
    {"/pm/process/cb/auto_launch", bt_bool, "false"},
    {"/pm/process/cb/working_dir", bt_string, NKN_CB_WORKING_DIR},
    {"/pm/process/cb/launch_path", bt_string, NKN_CB_PATH},
    {"/pm/process/cb/launch_uid", bt_uint16, "0"},
    {"/pm/process/cb/launch_gid", bt_uint16, "0"},
    {"/pm/process/cb/launch_params/1/param", bt_string, "-e"},
    {"/pm/process/cb/launch_params/2/param", bt_string, "-l"},
    {"/pm/process/cb/launch_params/3/param", bt_string, NKN_CB_LOGLEVEL},
    {"/pm/process/cb/launch_params/4/param", bt_string, "-p"},
    {"/pm/process/cb/launch_params/5/param", bt_string, NKN_CB_LISTENPORT},
    {"/pm/process/cb/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_CB},
    {"/pm/process/cb/initial_launch_timeout", bt_duration_ms, NKN_LAUNCH_TIMEOUT_CB},

    // TODO: Revisit this once we figure out how to check liveness of cb
};

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_nkn_cb_pm_init(const lc_dso_info *info, void *data);


static int md_nkn_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings(
	    commit,
	    db,
	    md_nkn_cb_pm_initial_values,
	    sizeof(md_nkn_cb_pm_initial_values)/ sizeof(bn_str_value));
    bail_error(err);

bail:
    return err;
}

int
md_nkn_cb_pm_init(const lc_dso_info *info, void *data)
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
                    "cb",
                    1,
                    "/pm/nokeena/cb",
                    NULL,
                    modrf_namespace_unrestricted,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    200,
                    0,
                    md_generic_upgrade_downgrade,
                    &md_cb_pm_ug_rules,
                    md_nkn_pm_add_initial,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    &module);
    bail_error(err);

    /* Upgrade processing nodes */
    err = md_upgrade_rule_array_new(&md_cb_pm_ug_rules);
    bail_error(err);
    ra = md_cb_pm_ug_rules;

bail:
    return err;
}
