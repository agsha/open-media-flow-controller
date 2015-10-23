/*
 *
 * Filename:    md_acclog.c
 * Date:        2008-11-12
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 * md_acclog.c: shows how the nknaccesslog module is added to
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
/*----------------------------------------------------------------------------
 * PREPROCESSOR MACROS/CONSTANTS
 *---------------------------------------------------------------------------*/
#define NKN_ACCESSLOG_PATH        "/opt/nkn/sbin/nknaccesslog"
#define NKN_ACCESSLOG_WORKING_DIR "/coredump/nknaccesslog"
#define NKN_ACCESSLOG_CONFIG_FILE "/config/nkn/accesslog.cfg"



/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_acclog_pm_ug_rules = NULL;
static const bn_str_value md_alog_pm_initial_values[] = {
        /*
         * nknaccesslog : basic launch configuration.
         */
        {"/pm/process/nknaccesslog/launch_enabled", bt_bool, "false"},
        {"/pm/process/nknaccesslog/auto_launch", bt_bool, "true"},
        {"/pm/process/nknaccesslog/working_dir", bt_string, NKN_ACCESSLOG_WORKING_DIR},
        {"/pm/process/nknaccesslog/launch_path", bt_string, NKN_ACCESSLOG_PATH},
        {"/pm/process/nknaccesslog/launch_uid", bt_uint16, "0"},
        {"/pm/process/nknaccesslog/launch_gid", bt_uint16, "0"},
        {"/pm/process/nknaccesslog/kill_timeout", bt_duration_ms, "3000"}
};

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_acclog_pm_init(const lc_dso_info *info, void *data);


static int md_nkn_al_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
        int err = 0;

        err = mdb_create_node_bindings(
                        commit,
                        db,
                        md_alog_pm_initial_values,
                        sizeof(md_alog_pm_initial_values)/ sizeof(bn_str_value));
        bail_error(err);

bail:
        return err;
}

int
md_acclog_pm_init(const lc_dso_info *info, void *data)
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
                    "acclog_pm",                        // module_name
                    3,                                  // module_version
                    "/pm/nokeena/accesslog",            // root_node_name
                    NULL,                               // root_config_name
                    modrf_namespace_unrestricted,       // md_mod_flags
                    NULL,                               // side_effects_func
                    NULL,                               // side_effects_arg
                    NULL,                               // check_func
                    NULL,                               // check_arg
                    NULL,                               // apply_func
                    NULL,                               // apply_arg
                    200,                                // commit_order
                    0,                                  // apply_order
                    md_generic_upgrade_downgrade,                               // updown_func
                    &md_acclog_pm_ug_rules,                               // updown_arg
                    NULL, //md_nkn_al_add_initial,              // initial_func
                    NULL,                               // initial_arg
                    NULL,                               // mon_get_func
                    NULL,                               // mon_get_arg
                    NULL,                               // mon_iterate_func
                    NULL,                               // mon_iterate_arg
                    &module);                           // ret_module
    bail_error(err);

    /* Upgrade processing nodes */
    err = md_upgrade_rule_array_new(&md_acclog_pm_ug_rules);
    bail_error(err);
    ra = md_acclog_pm_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_MODIFY;
    rule->mur_name =           "/pm/process/nknaccesslog/launch_enabled";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =   MUTT_DELETE;
    rule->mur_name =           "/pm/process/nknaccesslog";
    MD_ADD_RULE(ra);



bail:
    return err;
}

