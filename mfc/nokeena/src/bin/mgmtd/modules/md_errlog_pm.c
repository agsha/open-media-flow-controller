/*
 *
 * Filename:    md_errlog_pm.c
 * Date:        2008-11-13
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 * md_nkn_errlog_pm.c: shows how the nknerrorlog module is added to
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
#define NKN_ERRORLOG_PATH        "/opt/nkn/sbin/nknerrorlog"
#define NKN_ERRORLOG_WORKING_DIR "/coredump/nknerrorlog"
#define NKN_LOGDIR_PATH          "/var/log/nkn"
#define DEFAULT_SERVER_IP       "127.0.0.1"
#define DEFAULT_SERVER_PORT     "7957"


/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_errlog_pm_ug_rules = NULL;
static const bn_str_value md_elog_pm_initial_values[] = {
    /*
     * nknerrorlog : basic launch configuration.
     */
    {"/pm/process/nknerrorlog/launch_enabled", bt_bool, "false"},
    {"/pm/process/nknerrorlog/auto_launch", bt_bool, "true"},
    {"/pm/process/nknerrorlog/working_dir", bt_string, NKN_ERRORLOG_WORKING_DIR},
    {"/pm/process/nknerrorlog/launch_path", bt_string, NKN_ERRORLOG_PATH},
    {"/pm/process/nknerrorlog/launch_uid", bt_uint16, "0"},
    {"/pm/process/nknerrorlog/launch_gid", bt_uint16, "0"},
    {"/pm/process/nknerrorlog/launch_params/1/param", bt_string, "-g"},
};

/*----------------------------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static int
md_nkn_el_add_initial(
        md_commit *commit,
        mdb_db *db,
        void *arg);

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int md_errlog_pm_init(const lc_dso_info *info, void *data);


/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
md_errlog_pm_init(const lc_dso_info *info, void *data)
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
                    "errlog_pm",                        // module_name
                    3,                                  // module_version
                    "/pm/nokeena/errlog",               // root_node_name
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
                    &md_errlog_pm_ug_rules,                               // updown_arg
                    NULL,       //md_nkn_el_add_initial,              // initial_func
                    NULL,                               // initial_arg
                    NULL,                               // mon_get_func
                    NULL,                               // mon_get_arg
                    NULL,                               // mon_iterate_func
                    NULL,                               // mon_iterate_arg
                    &module);                           // ret_module
    bail_error(err);


    /* Upgrade processing nodes */
    err = md_upgrade_rule_array_new(&md_errlog_pm_ug_rules);
    bail_error(err);
    ra = md_errlog_pm_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_MODIFY;
    rule->mur_name =           "/pm/process/nknerrorlog/launch_enabled";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_DELETE;
    rule->mur_name =            "/pm/process/nknerrorlog";
    MD_ADD_RULE(ra);

bail:
    return err;
}


/*----------------------------------------------------------------------------
 * PRIVATE FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
static int md_nkn_el_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings(
	    commit,
	    db,
	    md_elog_pm_initial_values,
	    sizeof(md_elog_pm_initial_values)/ sizeof(bn_str_value));
    bail_error(err);

bail:
    return err;
}

/*----------------------------------------------------------------------------
 * END OF FILE
 *---------------------------------------------------------------------------*/

