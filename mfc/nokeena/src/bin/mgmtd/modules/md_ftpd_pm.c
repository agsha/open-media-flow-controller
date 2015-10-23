/*
 *
 * Filename:    md_ftpd_pm.c
 * Date:        2008-11-17
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 * md_ftpd.c: shows how the ftpd module is added to
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
#define NKN_FTPD_PATH           "/opt/nkn/bin/pure-ftpd"
#define NKN_FTPD_WORKING_DIR    "/coredump/pure-ftpd"
#define NKN_FTPD_CONFIG_FILE    "/config/nkn/accesslog.cfg"


/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_ftpd_pm_ug_rules = NULL;

static const bn_str_value md_alog_pm_initial_values[] = {
    /*
     * ftpd : basic launch configuration.
     */
    // -A -c50 -B -C8 -d -D -fftp -H -I15 -lunix -L7500:8 -m4 -s -U133:022 -u500 -r -i
    // -Oclf:/var/log/pureftpd.log -Ostats:/var/log/pureftpd-stats.log -K
    // -j -k99 -Z
    {"/pm/process/ftpd/launch_enabled", bt_bool, "true"},
    {"/pm/process/ftpd/auto_launch", bt_bool, "false"},
    {"/pm/process/ftpd/working_dir", bt_string, NKN_FTPD_WORKING_DIR},
    {"/pm/process/ftpd/launch_path", bt_string, NKN_FTPD_PATH},
    {"/pm/process/ftpd/launch_uid", bt_uint16, "0"},
    {"/pm/process/ftpd/launch_gid", bt_uint16, "0"},


    {"/pm/process/ftpd/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_FTPD},
    {"/pm/process/ftpd/initial_launch_timeout", bt_duration_ms, NKN_LAUNCH_TIMEOUT_FTPD},

    {"/pm/process/ftpd/launch_params/1/param", bt_string, "-A"},
    {"/pm/process/ftpd/launch_params/2/param", bt_string, "-c50"},
    {"/pm/process/ftpd/launch_params/3/param", bt_string, "-C8"},
    {"/pm/process/ftpd/launch_params/4/param", bt_string, "-d"},
    {"/pm/process/ftpd/launch_params/5/param", bt_string, "-E"},
    {"/pm/process/ftpd/launch_params/6/param", bt_string, "-fftp"},
    {"/pm/process/ftpd/launch_params/7/param", bt_string, "-H"},
    {"/pm/process/ftpd/launch_params/8/param", bt_string, "-I15"},
    {"/pm/process/ftpd/launch_params/9/param", bt_string, "-lunix"},
    {"/pm/process/ftpd/launch_params/10/param", bt_string, "-L7500:8"},
    {"/pm/process/ftpd/launch_params/11/param", bt_string, "-m4"},
    {"/pm/process/ftpd/launch_params/12/param", bt_string, "-s"},
    {"/pm/process/ftpd/launch_params/13/param", bt_string, "-U133:022"},
    {"/pm/process/ftpd/launch_params/14/param", bt_string, "-u1000"},
    {"/pm/process/ftpd/launch_params/15/param", bt_string, "-i"},
    {"/pm/process/ftpd/launch_params/16/param", bt_string, "-Oclf:/var/log/pureftpd.log"},
    {"/pm/process/ftpd/launch_params/17/param", bt_string, "-Ostats:/var/log/pureftpd-stats.log"},
    {"/pm/process/ftpd/launch_params/18/param", bt_string, "-K"},
    {"/pm/process/ftpd/launch_params/19/param", bt_string, "-k85"},
    {"/pm/process/ftpd/launch_params/20/param", bt_string, "-Z"},
    {"/pm/process/ftpd/launch_params/21/param", bt_string, "-x"},
    {"/pm/process/ftpd/launch_params/22/param", bt_string, "-X"},
    {"/pm/process/ftpd/launch_params/23/param", bt_string, "-T12800:"},


};

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_ftpd_pm_init(const lc_dso_info *info, void *data);


static int md_ftpd_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
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
md_ftpd_pm_init(const lc_dso_info *info, void *data)
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
	    "ftpd_pm",                        // module_name
	    4,                                  // module_version
	    "/pm/nokeena/ftpd",            // root_node_name
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
	    md_generic_upgrade_downgrade,       // updown_func
	    &md_ftpd_pm_ug_rules,                // updown_arg
	    md_ftpd_pm_add_initial,             // initial_func
	    NULL,                               // initial_arg
	    NULL,                               // mon_get_func
	    NULL,                               // mon_get_arg
	    NULL,                               // mon_iterate_func
	    NULL,                               // mon_iterate_arg
	    &module);                           // ret_module
    bail_error(err);

    /**********************************************************************
     *              UPGRADE RULES
     *********************************************************************/
    err = md_upgrade_rule_array_new(&md_ftpd_pm_ug_rules);
    bail_error(err);

    ra = md_ftpd_pm_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =        MUTT_MODIFY;
    rule->mur_name =                "/pm/process/ftpd/initial_launch_order";
    rule->mur_new_value_type =      bt_int32;
    rule->mur_new_value_str =       NKN_LAUNCH_ORDER_FTPD;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =        MUTT_MODIFY;
    rule->mur_name =                "/pm/process/ftpd/initial_launch_timeout";
    rule->mur_new_value_type =      bt_duration_ms;
    rule->mur_new_value_str =       NKN_LAUNCH_TIMEOUT_FTPD;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =        MUTT_MODIFY;
    rule->mur_name =                "/pm/process/ftpd/launch_params/19/param";
    rule->mur_new_value_type =	    bt_string;
    rule->mur_new_value_str =	    "-k85";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =        MUTT_MODIFY;
    rule->mur_name =                "/pm/process/ftpd/launch_params/23/param";
    rule->mur_new_value_type =	    bt_string;
    rule->mur_new_value_str =	    "-T12800:";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =        MUTT_MODIFY;
    rule->mur_name =                "/pm/process/ftpd/auto_launch";
    rule->mur_new_value_type =      bt_bool;
    rule->mur_new_value_str =       "false";
    MD_ADD_RULE(ra);

bail:
    return err;
}

