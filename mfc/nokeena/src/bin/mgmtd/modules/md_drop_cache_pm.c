/*
 *
 * Filename:    md_drop_cache_pm.c
 * Date:        2011-04-13
 * Author:      Ramanand Narayanan
 *
 * (C) Copyright 2011 Juniper Networks, Inc.
 * All Rights Reserved
 *
 */

/*----------------------------------------------------------------------------
 * md_drop_cache_pm.c: shows how the drop_cache module is added to
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
#include "file_utils.h"

/*----------------------------------------------------------------------------
 * PREPROCESSOR MACROS/CONSTANTS
 *---------------------------------------------------------------------------*/
#define NKN_DROP_CACHE_PATH          "/opt/nkn/bin/drop_cache.sh"
#define NKN_DROP_CACHE_WORKING_DIR   "/coredump/drop_cache"


/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_drop_cache_pm_ug_rules = NULL;

static const bn_str_value md_drop_cache_pm_initial_values[] = {
    /*
     * nkndrop_cache : basic launch configuration.
     */
    {"/pm/process/nkndrop_cache/launch_enabled", bt_bool, "true"},
    {"/pm/process/nkndrop_cache/auto_launch", bt_bool, "true"},
    {"/pm/process/nkndrop_cache/delete_trespassers", bt_bool, "false"},
    {"/pm/process/nkndrop_cache/working_dir", bt_string, NKN_DROP_CACHE_WORKING_DIR},
    {"/pm/process/nkndrop_cache/launch_path", bt_string, NKN_DROP_CACHE_PATH},
    {"/pm/process/nkndrop_cache/launch_uid", bt_uint16, "0"},
    {"/pm/process/nkndrop_cache/launch_gid", bt_uint16, "0"},

    {"/pm/process/nkndrop_cache/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_DROP_CACHE},
    {"/pm/process/nkndrop_cache/initial_launch_timeout", bt_duration_ms, NKN_LAUNCH_TIMEOUT_DROP_CACHE},
};

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_drop_cache_pm_init(const lc_dso_info *info, void *data);

static int
md_drop_cache_apply(md_commit *commit,
	const mdb_db		    *old_db,
	const mdb_db		    *new_db,
	mdb_db_change_array	    *change_list,
	void			    *arg);

static int
md_nkn_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings(
	    commit,
	    db,
	    md_drop_cache_pm_initial_values,
	    sizeof(md_drop_cache_pm_initial_values)/ sizeof(bn_str_value));
    bail_error(err);

bail:
    return err;
}

int
md_drop_cache_pm_init(const lc_dso_info *info, void *data)
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
	    "drop_cache",
	    2,
	    "/pm/nokeena/drop_cache",
	    NULL,
	    modrf_namespace_unrestricted,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    md_drop_cache_apply,
	    NULL,
	    200,
	    0,
	    md_generic_upgrade_downgrade,
	    &md_drop_cache_pm_ug_rules,
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
    node->mrn_name		= "/pm/nokeena/drop_cache/config/threshold";
    node->mrn_value_type	= bt_uint8;
    node->mrn_initial_value	= "5";
    node->mrn_limit_num_min	= "0";
    node->mrn_limit_num_max	= "100";
    node->mrn_bad_value_msg	= N_("Value should be between 0 to 100");
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_description	= "the drop-cache threshold";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /**********************************************************************
     *              UPGRADE RULES
     *********************************************************************/
    err = md_upgrade_rule_array_new(&md_drop_cache_pm_ug_rules);
    bail_error(err);

    ra = md_drop_cache_pm_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type	= MUTT_ADD;
    rule->mur_name		= "/pm/nokeena/drop_cache/config/threshold";
    rule->mur_new_value_type	= bt_uint8;
    rule->mur_new_value_str	= "5";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

bail:
    return err;
}


static int
md_drop_cache_apply(md_commit *commit,
	const mdb_db		    *old_db,
	const mdb_db		    *new_db,
	mdb_db_change_array	    *change_list,
	void			    *arg)
{
    int err = 0;
    const bn_attrib *new_val;
    char out_string[256];
    uint8 threshold;
    int i, num_changes = 0;
    const mdb_db_change *change = NULL;

    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; i++) {

	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if ((bn_binding_name_pattern_match(ts_str(change->mdc_name),
			"/pm/nokeena/drop_cache/config/threshold"))) {

	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		err = bn_attrib_get_uint8(new_val, NULL, NULL, &threshold);
		bail_error(err);

		/* write to file */
		snprintf(out_string, sizeof(out_string),
			"Threshold: %d\n", threshold);
		err = lf_write_file("/config/nkn/drop_cache_threshold", 0666,
				    out_string, true, strlen(out_string));
		bail_error(err);
	    }
	}
    }

bail:
    return err;
}

