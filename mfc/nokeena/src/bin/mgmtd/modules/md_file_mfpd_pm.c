/*
 *
 * Filename:    md_file_mfpd_pm.c
 * Date:        2010-09-13
 * Author:      Varsha Rajagopalan
    {"/pm/process/file_mfpd/launch_params/4/param", bt_string, "-h"},
 *
 * (C) Copyright 2010 Juniper Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 * md_file_mfpd_pm.c: shows how the file-mfp module is added to
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
#define NKN_FILE_MFPD_PATH           "/opt/nkn/sbin/file_mfpd"
#define NKN_FILE_MFPD_WORKING_DIR    "/coredump/file_mfpd"
#define NKN_FILE_MFPD_PUBLISH_DIR    "/nkn/vpe/file"
#define NKN_FILE_MFPD_SCHEMA_DIR     "/config/nkn/mfp_pmf_schema.xsd" //Yet to be given
#define NKN_FILE_MFPD_CONFIG_FILE    "/config/nkn/mfp.filed.conf.default"


/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_file_mfpd_pm_ug_rules = NULL;

static const bn_str_value md_file_mfpd_pm_initial_values[] = {
        /*
         * file_mfpd : basic launch configuration.
	 *Media Flow Publisher File Listener Daemon
	 *-D - run as a daemon
	 *-w - directory to watch for publish events
	 *-x - Path to the XML schema file
	 *-h - print help
         */
    {"/pm/process/file_mfpd/description", bt_string,
	" Media Flow Publisher File Listener Daemon"},
    {"/pm/process/file_mfpd/launch_enabled", bt_bool, "true"},
    {"/pm/process/file_mfpd/auto_launch", bt_bool, "false"},
    {"/pm/process/file_mfpd/working_dir", bt_string, NKN_FILE_MFPD_WORKING_DIR},
    {"/pm/process/file_mfpd/launch_path", bt_string, NKN_FILE_MFPD_PATH},
    {"/pm/process/file_mfpd/launch_uid", bt_uint16, "0"},
    {"/pm/process/file_mfpd/launch_gid", bt_uint16, "0"},
    {"/pm/process/file_mfpd/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_FILE_MFPD},
    {"/pm/process/file_mfpd/initial_launch_timeout", bt_duration_ms, NKN_LAUNCH_TIMEOUT_FILE_MFPD},
    {"/pm/process/file_mfpd/launch_params/1/param", bt_string, "-w"},
    {"/pm/process/file_mfpd/launch_params/2/param", bt_string, NKN_FILE_MFPD_PUBLISH_DIR},
    {"/pm/process/file_mfpd/launch_params/3", bt_uint8, "3"},
    {"/pm/process/file_mfpd/launch_params/3/param", bt_string, "-F"},
    {"/pm/process/file_mfpd/launch_params/4", bt_uint8, "4"},
    {"/pm/process/file_mfpd/launch_params/4/param", bt_string, NKN_FILE_MFPD_CONFIG_FILE},
};

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_file_mfpd_pm_init(const lc_dso_info *info, void *data);


static int md_file_mfpd_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    err = mdb_create_node_bindings(
	    commit,
	    db,
	    md_file_mfpd_pm_initial_values,
	    sizeof(md_file_mfpd_pm_initial_values)/ sizeof(bn_str_value));
    bail_error(err);

bail:
    return err;
}

int
md_file_mfpd_pm_init(const lc_dso_info *info, void *data)
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
                    "file_mfpd_pm",                        // module_name
                    3,                                  // module_version
                    "/pm/nokeena/file_mfpd",            // root_node_name
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
                    &md_file_mfpd_pm_ug_rules,                // updown_arg
                    md_file_mfpd_pm_add_initial,             // initial_func
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
    err = md_upgrade_rule_array_new(&md_file_mfpd_pm_ug_rules);
    bail_error(err);
    ra = md_file_mfpd_pm_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/file_mfpd/launch_params/3/param";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "-F";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/file_mfpd/launch_params/4/param";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   NKN_FILE_MFPD_CONFIG_FILE;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/file_mfpd/launch_params/3";
    rule->mur_new_value_type =  bt_uint8;
    rule->mur_new_value_str =   "3";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/file_mfpd/launch_params/4";
    rule->mur_new_value_type =  bt_uint8;
    rule->mur_new_value_str =   "4";
    MD_ADD_RULE(ra);

bail:
    return err;
}

