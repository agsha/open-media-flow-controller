/*
 *
 * Filename:  md_nkn_bonding.c
 * Date:      2009-05-19
 * Author:    Dhruva Narasimhan
 *
 * (C) Copyright 2008-2009 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */


#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "mdm_events.h"
#include "tpaths.h"

static md_upgrade_rule_array *md_nkn_bonding_ug_rules= NULL;

int md_nkn_bonding_init(const lc_dso_info *info, void *data);


static int
md_nkn_bonding_commit_check(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list,
        void *arg);

int md_nkn_bonding_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    err = mdm_add_module("nkn_bonding", 1, "/nkn/bonding", "/net/bonding/config",
	    modrf_all_changes,
	    NULL, NULL,
	    md_nkn_bonding_commit_check, NULL,
	    NULL, NULL,
	    0, 0,
	    md_generic_upgrade_downgrade,
	    &md_nkn_bonding_ug_rules,
	    NULL, NULL,
	    NULL, NULL, NULL, NULL, &module);
    bail_error(err);

bail:
    return err;
}


static int
md_nkn_bonding_commit_check(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list,
        void *arg)
{
    int err = 0;
    const mdb_db_change *change = NULL;
    int i = 0, num_changes = 0;
    tstring *t_bond_mode = NULL;
    const bn_attrib *new_val = NULL;

    num_changes = mdb_db_change_array_length_quick(change_list);
    for( i = 0; i < num_changes; ++i )
    {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if (((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "net", "bonding", "config", "*", "mode"))
		    && ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type))))
	{
	    lc_log_basic(LOG_NOTICE, "bonding mode changed");
	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val)
	    {
		err = bn_attrib_get_tstr(new_val, NULL, bt_string, NULL, &t_bond_mode);
		bail_error(err);

		/* Make sure unsupported bond modes are not allowed!
		 */

		if ( t_bond_mode )
		{
		    lc_log_basic(LOG_NOTICE, "bonding mode changed: %s", ts_str(t_bond_mode));

		    if ( ts_equal_str(t_bond_mode, "backup", false) ||
			    ts_equal_str(t_bond_mode, "balance-xor", false) ||
			    ts_equal_str(t_bond_mode, "broadcast", false) ||
			    ts_equal_str(t_bond_mode, "link-agg", false) ||
			    ts_equal_str(t_bond_mode, "balance-tlb", false) ||
			    ts_equal_str(t_bond_mode, "balance-alb", false) )
		    {
			err = md_commit_set_return_status_msg_fmt(commit, -1, _("Unsupported bond mode"));
			bail_error(err);

			goto bail;
		    }
		}
	    }
	}
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);
bail:
    return err;
}
