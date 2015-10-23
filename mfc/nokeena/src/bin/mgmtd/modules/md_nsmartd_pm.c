/*
 *
 * Filename:    md_nsmartd_pm.c
 * Date:        06 Feb 2012
 * Author:      Srihari MS
 *
 * (C) Copyright 2011 Juniper Networks, Inc.
 * All rights reserved
 *
 */

/*----------------------------------------------------------------------------
 * md_nsmartd_pm.c: shows how the nsmartd module is added to
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
#define NSMARTD_PATH          "/opt/nkn/sbin/nsmartd"
#define NSMARTD_WORKING_DIR   "/coredump/nsmartd"

/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_nsmartd_pm_ug_rules = NULL;

static const bn_str_value md_nsmartd_pm_initial_values[] = {
    /*
     * nsmartd : basic launch configuration.
     */
    {"/pm/process/nsmartd/launch_enabled", bt_bool, "true"},
    {"/pm/process/nsmartd/auto_launch", bt_bool, "true"},
    {"/pm/process/nsmartd/delete_trespassers", bt_bool, "false"},
    {"/pm/process/nsmartd/working_dir", bt_string, NSMARTD_WORKING_DIR},
    {"/pm/process/nsmartd/launch_path", bt_string, NSMARTD_PATH},
    {"/pm/process/nsmartd/launch_uid", bt_uint16, "0"},
    {"/pm/process/nsmartd/launch_gid", bt_uint16, "0"},
    {"/pm/process/nsmartd/fd_limit", bt_uint32, "1024"},
    /* 100 MB = 100 * 1024 * 1024 ; */
    {"/pm/process/nsmartd/memory_size_limit", bt_int64, "104857600" },
    {"/pm/process/nsmartd/initial_launch_order", bt_int32, NKN_NSMARTD_LAUNCH_ORDER},
    {"/pm/process/nsmartd/initial_launch_timeout", bt_duration_ms, NKN_NSMARTD_LAUNCH_TIMEOUT},
};

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_nsmartd_pm_init(const lc_dso_info *info, void *data);


static int md_nkn_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings(
	    commit,
	    db,
	    md_nsmartd_pm_initial_values,
	    sizeof(md_nsmartd_pm_initial_values)/ sizeof(bn_str_value));
    bail_error(err);

bail:
    return err;
}

int
md_nsmartd_pm_init(const lc_dso_info *info, void *data)
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
                    "nsmartd",
                    2,
                    "/pm/nokeena/nsmartd",
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
                    &md_nsmartd_pm_ug_rules,
                    md_nkn_pm_add_initial,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    &module);
    bail_error(err);
    /**********************************************************************
     *              UPGRADE RULES
     *********************************************************************/
    err = md_upgrade_rule_array_new(&md_nsmartd_pm_ug_rules);
    bail_error(err);

    ra = md_nsmartd_pm_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =        MUTT_DELETE;
    rule->mur_name =                "/pm/process/nsmartd/launch_params/1/param";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =        MUTT_MODIFY;
    rule->mur_name =                "/pm/process/nsmartd/auto_launch";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "true";
    MD_ADD_RULE(ra);

bail:
    return err;
}

