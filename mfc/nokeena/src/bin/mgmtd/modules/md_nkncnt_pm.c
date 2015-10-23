/*
 *
 * Filename:    md_nkncnt_pm.c
 * Date:        2011-04-29
 * Author:      Manikandan Vengatachalam
 *
 * (C) Copyright 2011 Juniper Networks, Inc.
 * All rights reserved
 *
 */

/*----------------------------------------------------------------------------
 * md_nkncnt_pm.c: shows how the nkncnt binary is added to
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
#include "file_utils.h"
#include "nkn_mgmt_common.h"

/*----------------------------------------------------------------------------
 * PREPROCESSOR MACROS/CONSTANTS
 *---------------------------------------------------------------------------*/
#define NKN_NKNCNT_PATH          "/opt/nkn/bin/nkncnt"
#define NKN_NKNCNT_WORKING_DIR   "/coredump/nkncnt"
#define NKNCNT_FILENAME_BASE	 "/var/log/nkncnt/nkn_counters"
#define NKNCNT_DUMP_FREQUENCY	 "20" /* Bug 8870*/
#define NKNCNT_DEFAULT_CONFIG	"/var/opt/tms/output/nkncnt.conf"
/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_nkncnt_pm_ug_rules = NULL;
static const bn_str_value md_nkncnt_pm_initial_values[] = {
    /*
     * nkncnt : basic launch configuration.
     */
    {"/pm/process/nkncnt/launch_enabled", bt_bool, "true"},
    {"/pm/process/nkncnt/auto_launch", bt_bool, "true"},
    {"/pm/process/nkncnt/delete_trespassers", bt_bool, "false"},
    {"/pm/process/nkncnt/working_dir", bt_string, NKN_NKNCNT_WORKING_DIR},
    {"/pm/process/nkncnt/launch_path", bt_string, NKN_NKNCNT_PATH},
    {"/pm/process/nkncnt/launch_uid", bt_uint16, "0"},
    {"/pm/process/nkncnt/launch_gid", bt_uint16, "0"},

    {"/pm/process/nkncnt/term_signals/force/1", bt_string, "SIGKILL"},
    {"/pm/process/nkncnt/term_signals/normal/1", bt_string, "SIGTERM"},

    {"/pm/process/nkncnt/launch_params/1/param", bt_string, "-t"},
    {"/pm/process/nkncnt/launch_params/2/param", bt_string, NKNCNT_DUMP_FREQUENCY},
    {"/pm/process/nkncnt/launch_params/3/param", bt_string, "-f"},
    {"/pm/process/nkncnt/launch_params/4/param", bt_string, NKNCNT_FILENAME_BASE},

    {"/pm/process/nkncnt/launch_params/5", bt_uint8, "5"},
    {"/pm/process/nkncnt/launch_params/5/param", bt_string, "-C"},
    {"/pm/process/nkncnt/launch_params/6", bt_uint8, "6"},
    {"/pm/process/nkncnt/launch_params/6/param", bt_string, NKNCNT_DEFAULT_CONFIG},

    {"/pm/process/nkncnt/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_NKNCNT},
    {"/pm/process/nkncnt/initial_launch_timeout", bt_duration_ms, NKN_LAUNCH_TIMEOUT_NKNCNT},
    /* ---- nkncnt Liveness term signals ----- */
    {"/pm/process/nkncnt/term_signals/liveness/1", bt_string, "SIGKILL"},
    {"/pm/process/nkncnt/term_signals/liveness/2", bt_string, "SIGQUIT"},
    {"/pm/process/nkncnt/term_signals/liveness/3", bt_string, "SIGTERM"},
};

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_nkncnt_pm_init(const lc_dso_info *info, void *data);
static int md_nkncnt_pm_commit_apply(
	md_commit *commit,
	const mdb_db *old_db,
	const mdb_db *new_db,
	mdb_db_change_array *change_list,
	void *arg);


static int md_nkn_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings(
	    commit,
	    db,
	    md_nkncnt_pm_initial_values,
	    sizeof(md_nkncnt_pm_initial_values)/ sizeof(bn_str_value));
    bail_error(err);

bail:
    return err;
}

int
md_nkncnt_pm_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;
    tbool file_found = false;

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
                    "nkncnt",
                    6,
                    "/pm/nokeena/nkncnt",
                    NULL,
                    modrf_namespace_unrestricted,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    md_nkncnt_pm_commit_apply,
                    NULL,
                    200,
                    0,
                    md_generic_upgrade_downgrade,
                    &md_nkncnt_pm_ug_rules,
                    md_nkn_pm_add_initial,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    &module);

    bail_error(err);



    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/pm/nokeena/nkncnt/config/upload/url";
    node->mrn_value_type =	bt_string;
    node->mrn_initial_value =	"";
    node->mrn_node_reg_flags =	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =	mcf_cap_node_privileged;
    node->mrn_description =	"URL to upload a counter/stat file to.";
    node->mrn_audit_descr =	N_("Stat upload path");
    err = mdm_add_node(module, &node);
    bail_error(err);


    /* Upgrade processing nodes */
    err = md_upgrade_rule_array_new(&md_nkncnt_pm_ug_rules);
    bail_error(err);
    ra = md_nkncnt_pm_ug_rules;

    /*bug 8364 */
    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_DELETE;
    rule->mur_name =           "/pm/process/nkncnt/launch_params/5/param";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_DELETE;
    rule->mur_name =           "/pm/process/nkncnt/launch_params/6/param";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_DELETE;
    rule->mur_name =           "/pm/process/nkncnt/launch_params/7/param";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_DELETE;
    rule->mur_name =           "/pm/process/nkncnt/launch_params/8/param";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nkncnt/launch_params/2/param";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "30";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nkncnt/launch_params/2/param";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "20";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/nkncnt/auto_launch";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    MD_ADD_RULE(ra);

     MD_NEW_RULE(ra, 3,4);
     rule->mur_upgrade_type =    MUTT_ADD;
     rule->mur_name =            "/pm/process/nkncnt/launch_params/5/param";
     rule->mur_new_value_type =  bt_string;
     rule->mur_new_value_str =   "-U";
     MD_ADD_RULE(ra);

     MD_NEW_RULE(ra, 3, 4);
     rule->mur_upgrade_type =    MUTT_ADD;
     rule->mur_name =            "/pm/process/nkncnt/launch_params/6/param";
     rule->mur_new_value_type =  bt_string;
     rule->mur_new_value_str =   "";
     MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3,4);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/nkncnt/launch_params/5";
    rule->mur_new_value_type =  bt_uint8;
    rule->mur_new_value_str =   "5";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3,4);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/nkncnt/launch_params/6";
    rule->mur_new_value_type =  bt_uint8;
    rule->mur_new_value_str =   "6";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =	MUTT_ADD;
    rule->mur_name =		"/pm/nokeena/nkncnt/config/upload/url";
    rule->mur_new_value_type =	bt_string;
    rule->mur_copy_from_node = "/pm/process/nkncnt/launch_params/6/param";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =	MUTT_MODIFY;
    rule->mur_name =		"/pm/process/nkncnt/launch_params/5/param";
    rule->mur_new_value_type =	bt_string;
    rule->mur_new_value_str =	"-C";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =	MUTT_MODIFY;
    rule->mur_name =		"/pm/process/nkncnt/launch_params/6/param";
    rule->mur_new_value_type =	bt_string;
    rule->mur_new_value_str =	NKNCNT_DEFAULT_CONFIG;
    MD_ADD_RULE(ra);

    /* Create the default config file if it doesnt exist. */
    err = lf_test_exists(NKNCNT_DEFAULT_CONFIG, &file_found);
    complain_error(err);

    if (!file_found) {
	err = lf_touch_file(NKNCNT_DEFAULT_CONFIG, 00666);
	complain_error(err);
    }


bail:
    return err;
}

static int md_nkncnt_pm_commit_apply(
	md_commit *commit,
	const mdb_db *old_db,
	const mdb_db *new_db,
	mdb_db_change_array *change_list,
	void *arg)
{
    int err = 0;
    int i, num_changes = 0;
    const mdb_db_change *change = NULL;
    tbool found = false;
    tstring *t_url = NULL;
    const bn_attrib *new_val = NULL;



    num_changes = mdb_db_change_array_length_quick(change_list);
    for (i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if (bn_binding_name_pattern_match(ts_str(change->mdc_name),
		    "/pm/nokeena/nkncnt/config/upload/url")) {

	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		err = bn_attrib_get_tstr(new_val, NULL, bt_string, NULL, &t_url);
		bail_error(err);

		err = lf_write_file_full(NKNCNT_DEFAULT_CONFIG,
			0666, lwo_overwrite, ts_str(t_url), ts_length(t_url));
		bail_error(err);

		ts_free(&t_url);
	    }
	}
    }
bail:
    ts_free(&t_url);
    return err;
}
