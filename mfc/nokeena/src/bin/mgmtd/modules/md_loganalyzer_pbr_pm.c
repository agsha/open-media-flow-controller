/*
 *
 * Filename:    md_loganalyzer_pbr_pm.c
 * Date:        2014-03-18
 * Author:      Ramanand Narayanan
 *
 * (C) Copyright 2014 Juniper Networks, Inc.
 * All rights reserved
 *
 */

/*----------------------------------------------------------------------------
 * md_loganalyzer_pbr_pm.c: This is mainly to add the log analyzer built by
 * Siva to be controlled by PM. It requires a set of environment variables 
 * to be set before it is invoked.
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
#define NKN_LOGANALYZER_PBR_PATH          "/opt/dpi-analyzer/scripts/log_analyzer_server"

/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_loganalyzer_pbr_pm_ug_rules = NULL;

static const bn_str_value md_loganalyzer_pbr_pm_initial_values[] = {
        /*
         * log-analyzer : basic launch configuration.
         */
        {"/pm/process/log-analyzer/description", bt_string,
				"HTTP Log Analyzer with optional PBR"},
        {"/pm/process/log-analyzer/launch_enabled", bt_bool, "true"},
        {"/pm/process/log-analyzer/auto_launch", bt_bool, "false"},
        {"/pm/process/log-analyzer/delete_trespassers", bt_bool, "false"},
        {"/pm/process/log-analyzer/launch_path", bt_string,
					NKN_LOGANALYZER_PBR_PATH},
        {"/pm/process/log-analyzer/launch_uid", bt_uint16, "0"},
        {"/pm/process/log-analyzer/launch_gid", bt_uint16, "0"},

        {"/pm/process/log-analyzer/initial_launch_order", bt_int32,
					NKN_LAUNCH_ORDER_LOGANALYZER_PBR},
};

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_loganalyzer_pbr_pm_init(const lc_dso_info *info, void *data);


static int
md_nkn_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
        int err = 0;

        err = mdb_create_node_bindings(
                        commit,
                        db,
                        md_loganalyzer_pbr_pm_initial_values,
                        sizeof(md_loganalyzer_pbr_pm_initial_values)/ sizeof(bn_str_value));
        bail_error(err);

bail:
        return err;
}

int
md_loganalyzer_pbr_pm_init(const lc_dso_info *info, void *data)
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
                    "log-analyzer",
                    2,
                    "/pm/nokeena/log-analyzer", NULL,
                    modrf_namespace_unrestricted,
                    NULL, NULL,
                    NULL, NULL,
                    NULL, NULL,
                    200, 0,
                    md_generic_upgrade_downgrade,
		    &md_loganalyzer_pbr_pm_ug_rules,
                    md_nkn_pm_add_initial,
                    NULL, NULL,
                    NULL, NULL,
                    NULL,
                    &module);
    bail_error(err);


    /* ------------------------------------------------------------------------
     * Upgrade registration
     */

    err = md_upgrade_rule_array_new(&md_loganalyzer_pbr_pm_ug_rules);
    bail_error(err);
    ra = md_loganalyzer_pbr_pm_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =	  MUTT_DELETE;
    rule->mur_name =		  "/pm/process/log-analyzer/launch_params/1";
    MD_ADD_RULE(ra);
    
    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =	  MUTT_DELETE;
    rule->mur_name =		  "/pm/process/log-analyzer/launch_params/2";
    MD_ADD_RULE(ra);


bail:
    return err;

} /* end of md_loganalyzer_pbr_pm_init() */

/* End of md_loganalyzer_pbr_pm.c */
