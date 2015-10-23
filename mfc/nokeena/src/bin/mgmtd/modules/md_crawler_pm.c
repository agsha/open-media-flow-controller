/*
 *
 * Filename:    md_crawler_pm.c
 * Date:        2011-08-26
 * Author:      Thilak Raj S
 *
 * (C) Copyright 2008-2010 Juniper Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 * md_crawler_pm.c: shows how the crawler module is added to
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
#define NKN_CRAWLER_PATH             "/opt/nkn/bin/cse"
#define NKN_CRAWLER_WORKING_DIR      "/coredump/crawler"


/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_crawler_pm_ug_rules = NULL;

static const bn_str_value md_crawler_pm_initial_values[] = {
    /*
     * crawler : basic launch configuration.
     */
    {"/pm/process/crawler/description", bt_string,
    				"Juniper CRAWLER"},
    {"/pm/process/crawler/launch_path", bt_string,
    				NKN_CRAWLER_PATH},
    {"/pm/process/crawler/working_dir", bt_string,
    				NKN_CRAWLER_WORKING_DIR},


    {"/pm/process/crawler/launch_enabled", bt_bool, "true"},
    {"/pm/process/crawler/auto_launch", bt_bool, "true"},
    {"/pm/process/crawler/launch_uid", bt_uint16, "0"},
    {"/pm/process/crawler/launch_gid", bt_uint16, "0"},
    {"/pm/process/crawler/term_signals/force/1", bt_string, "SIGKILL"},
    {"/pm/process/crawler/term_signals/normal/1", bt_string, "SIGTERM"},

    /*------------------ Launch Order ---------------*/
    {"/pm/process/crawler/initial_launch_order", bt_int32,
    				NKN_LAUNCH_ORDER_CRAWLER},
    {"/pm/process/crawler/initial_launch_timeout", bt_duration_ms,
    				NKN_LAUNCH_TIMEOUT_CRAWLER},

    /*--------------- Params -------------------*/
};

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_crawler_pm_init(const lc_dso_info *info, void *data);


static int md_nkn_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
        int err = 0;

        err = mdb_create_node_bindings(
                        commit,
                        db,
                        md_crawler_pm_initial_values,
                        sizeof(md_crawler_pm_initial_values)/ sizeof(bn_str_value));
        bail_error(err);

bail:
        return err;
}

int
md_crawler_pm_init(const lc_dso_info *info, void *data)
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
                    "crawler-pm",
                    1,
                    "/pm/nokeena/crawler",
                    NULL,
                    0,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    200,
                    0,
                    md_generic_upgrade_downgrade,
                    &md_crawler_pm_ug_rules,
                    md_nkn_pm_add_initial,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    &module);
    bail_error(err);

    /* Upgrade processing nodes */
    err = md_upgrade_rule_array_new(&md_crawler_pm_ug_rules);
    bail_error(err);
    ra = md_crawler_pm_ug_rules;

bail:
    return err;
}

