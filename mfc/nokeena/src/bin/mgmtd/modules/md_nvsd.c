/*
 *
 * Filename:  md_nvsd.c
 * Date:      2008/11/14
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */


#include <ctype.h>
#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "file_utils.h"

#include "nkn_mgmt_defs.h"

extern  unsigned int jnpr_log_level;
const char *proc_drop_cache = "/proc/sys/vm/drop_caches";
const mode_t jnpr_proc_file_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

md_commit_forward_action_args md_nvsd_fwd_args =
{
    GCL_CLIENT_NVSD, true, N_("Request failed; Unable to reach MFD subsystem"),
    mfat_blocking
#ifdef PROD_FEATURE_I18N_SUPPORT
    , GETTEXT_DOMAIN
#endif
};


int md_pm_add_param(md_commit *commit, mdb_db *inout_new_db,
	const char *pname, const char *param);
int md_pm_remove_param(md_commit *commit, mdb_db *inout_new_db,
	const char *pname, const char *param);

int md_pm_update_param(md_commit *commit, mdb_db *inout_new_db,
	const char *pname, const char *param, int add, int *param_found);

int md_pm_enable_launch_auto(md_commit *commit, mdb_db *inout_new_db,
	const char *pname);
int md_pm_disable_launch_auto(md_commit *commit, mdb_db *inout_new_db,
	const char *pname);


static int
md_nvsd_side_effects(
	md_commit *commit,
	const mdb_db *old_db,
	mdb_db *inout_new_db,
	mdb_db_change_array *change_list,
	void *arg);

/* ------------------------------------------------------------------------- */

int md_nvsd_init(const lc_dso_info *info, void *data);

static int
md_nvsd_write_proc(md_commit *commit, const mdb_db *db,
		    const char *action_name,
		    bn_binding_array *params, void *cb_arg);

static md_upgrade_rule_array *md_nvsd_ug_rules = NULL;

static int md_nvsd_commit_check(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

int md_nvsd_debug_mod_check(md_commit *commit,
                    const mdb_db *old_db,
                    const mdb_db *new_db,
                    const mdb_db_change_array *change_list,
                    const tstring *node_name,
                    const tstr_array *node_name_parts,
                    mdb_db_change_type change_type,
                    const bn_attrib_array *old_attribs,
                    const bn_attrib_array *new_attribs,
                    void *arg);
static int
md_nvsd_get_engine_id( md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);
/* ------------------------------------------------------------------------- */
static int
md_nvsd_upgrade_downgrade(const mdb_db *old_db,
			    mdb_db *inout_new_db,
			    uint32 from_version,
			    uint32 to_version,
			    void *arg);
int
md_nvsd_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd", 15,
                         "/nkn/nvsd", NULL,
			 0,
                         md_nvsd_side_effects, NULL,
                         md_nvsd_commit_check, NULL,
                         NULL, NULL,
                         0, 0,
                         md_nvsd_upgrade_downgrade,
			 &md_nvsd_ug_rules,
                         NULL, NULL,
                         NULL, NULL, NULL, NULL,
			 &module);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/system/config/debug/level
     *  - Configure debug level
     *  - Default value is 1 (ERROR)
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/debug/level";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int = 1;
    node->mrn_limit_num_min_int = 1;
    node->mrn_limit_num_max_int = 7;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "debug message level";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /*
     * Setup config node - /nkn/nvsd/system/config/debug/mod
     *  - Configure debug mod
     *  - Default value is all modules
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/debug/mod";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "0x00FFFFFFFFFFFFFF";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "debug message mod";
    node->mrn_check_node_func   = md_nvsd_debug_mod_check;
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/system/config/debug/command
     *  - Configure debug command
     *  - Default value is "".
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/debug/command";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "debug message command";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup node for enabling/disabling soft assertion
     * Node : /nkn/nvsd/debug/config/assert/enabled
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/debug/config/assert/enabled";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "debug assert flag";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Adding node to control the number of rtsched threads.
     * Node: /nkn/nvsd/system/config/rtsched/threads
     */

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/rtsched/threads";
    node->mrn_value_type	= bt_uint32;
    node->mrn_initial_value	= "0";
    node->mrn_limit_num_min_int = 0;
    node->mrn_limit_num_max_int = 8;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Number of rtsched threads";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/sched/threads";
    node->mrn_value_type	= bt_uint32;
    node->mrn_initial_value	= "0";
    node->mrn_limit_num_min_int = 0;
    node->mrn_limit_num_max_int = 8;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Number of scheduler threads";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/coredump";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "2";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Enable/Disable nvsd process's coredump";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/mod_dmi";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Start/Stop mod-dmi service";
    err = mdm_add_node(module, &node);
    bail_error(err);
   
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/mod_rapi";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Start/Stop rest-api service";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/mod_ccf_dist";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Start/Stop ccf-distribution service";
    err = mdm_add_node(module, &node);
    bail_error(err);
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/mod_ssl";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Start/Stop mod-ssl service";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/mod_ftp";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Start/Stop mod-ftp service";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/mod_crawler";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Start/Stop mod-crawler service";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/bypass";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "0";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/sched/tasks";
    node->mrn_value_type	= bt_uint32;
    node->mrn_initial_value	= "256";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Number of scheduler tasks. Default: 256K";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/system/config/init/preread";
    node->mrn_value_type	= bt_bool;
    node->mrn_initial_value	= "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/nvsd/system/monitor/engine_id";
    node->mrn_value_type	= bt_string;
    node->mrn_node_reg_flags	= mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask		= mcf_cap_node_basic;
    node->mrn_mon_get_func	= md_nvsd_get_engine_id;
    node->mrn_description	= "provides engine-id for SNMP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name		= "/nkn/nvsd/write_proc";
    node->mrn_node_reg_flags	= mrf_flags_reg_action;
    node->mrn_cap_mask		= mcf_cap_action_basic;
    node->mrn_action_func	= md_nvsd_write_proc;
    node->mrn_action_log_handle	= false;
    node->mrn_actions[0]->mra_param_name    = "proc-name";
    node->mrn_actions[0]->mra_param_type    = bt_string;
    node->mrn_actions[1]->mra_param_name    = "proc-value";
    node->mrn_actions[1]->mra_param_type    = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = md_upgrade_rule_array_new(&md_nvsd_ug_rules);
    bail_error(err);
    ra = md_nvsd_ug_rules;


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/state/alive";
    node->mrn_value_type        = bt_bool;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal_nonbarrier;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_watchdog;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/watch_dog/config/restart";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* action node to do rate limiting on cmds */
    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/actions/cmd/rate_limit";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_nvsd_fwd_args;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Upgrade processing nodes */

    err = md_upgrade_rule_array_new(&md_nvsd_ug_rules);
    bail_error(err);
    ra = md_nvsd_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/state/alive";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str = "true";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/system/config/rtsched/threads";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "2";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/system/config/sched/threads";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "1";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =          "/nkn/nvsd/watch_dog/config/restart";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =          "/nkn/nvsd/system/config/coredump";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type	= MUTT_ADD;
    rule->mur_name		= "/nkn/nvsd/system/config/sched/tasks";
    rule->mur_new_value_type	= bt_uint32;
    rule->mur_new_value_str	= "256";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =          "/nkn/nvsd/system/config/bypass";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =          "/nkn/nvsd/system/config/init/preread";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =          "/nkn/nvsd/system/config/mod_dmi";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type =   MUTT_MODIFY;
    rule->mur_name =          "/nkn/nvsd/system/config/init/preread";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =          "/nkn/nvsd/system/config/mod_rapi";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =          "/nkn/nvsd/system/config/mod_ccf_dist";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =          "/nkn/nvsd/system/config/mod_ssl";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =          "/nkn/nvsd/system/config/mod_ftp";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =          "/nkn/nvsd/system/config/mod_crawler";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type =   MUTT_MODIFY;
    rule->mur_name =          "/nkn/nvsd/system/config/mod_rapi";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =    "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type =   MUTT_MODIFY;
    rule->mur_name =          "/nkn/nvsd/system/config/mod_ccf_dist";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =    "false";
    MD_ADD_RULE(ra);


 bail:
    return(err);
}

int md_nvsd_debug_mod_check(md_commit *commit,
                    const mdb_db *old_db,
                    const mdb_db *new_db,
                    const mdb_db_change_array *change_list,
                    const tstring *node_name,
                    const tstr_array *node_name_parts,
                    mdb_db_change_type change_type,
                    const bn_attrib_array *old_attribs,
                    const bn_attrib_array *new_attribs,
                    void *arg)
{
    int err = 0;
    const bn_attrib *new_value = NULL;
    tstring *new_value_ts = NULL;
    uint32 len = 0, i = 0;
    const char *p = NULL;

    new_value = bn_attrib_array_get_quick(new_attribs, ba_value);
    if(new_value == NULL) {
        goto bail;
    }

    err = bn_attrib_get_tstr(new_value, NULL, bt_string, NULL, &new_value_ts);
    bail_error(err);

    len = ts_length(new_value_ts);
    if ( (len > 18) ||
        !(ts_has_prefix_str(new_value_ts, "0x", true) )) {
        err = md_commit_set_return_status_msg_fmt(commit, 1, _("Invalid mode. Enter hexadecimal number"));
        bail_error(err);
    }

    p = ts_str(new_value_ts);
    for(i = 2; i < len; ++i) {

        if ( !isxdigit(p[i]) ) {
            err = md_commit_set_return_status_msg_fmt(commit, 1, _("Invalid mode. Enter hexadecimal number"));
            bail_error(err);
        }
    }

bail:
    ts_free(&new_value_ts);
    return err;
}


/* ------------------------------------------------------------------------- */
static int
md_nvsd_commit_check(md_commit *commit,
                     const mdb_db *old_db,
                     const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0, num_changes=0;
    const mdb_db_change *change = NULL;
    uint32 t_tasks = 0;
    uint32 bypass_val = 0;
    tstr_array *tproxy_interfaces = NULL;
    uint32 count = 0;

    num_changes = mdb_db_change_array_length_quick(change_list);

    for (int i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick(change_list, i );
	bail_null(change);

	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, 
			"nkn", "nvsd", "system", "config" ,"sched", "tasks")) && (mdct_modify == change->mdc_change_type)) {

	    err = mdb_get_node_value_uint32(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &t_tasks);
	    bail_error(err);

	    if( !((t_tasks == 256) || (t_tasks == 512) || (t_tasks == 1024))){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Valid inputs: 256 or 512 or 1024\n"));
		bail_error(err);
		goto bail;
	    }
	} else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
			0, 5, "nkn", "nvsd", "system", "config", "bypass"))) {

            err = mdb_get_node_value_uint32(commit, new_db, ts_str(change->mdc_name), 0, NULL, &bypass_val);
            bail_error(err);

	    if (2 == bypass_val) /* Val 2 is for 'optimized' */{
		err = mdb_get_matching_tstr_array( NULL,
			(const mdb_db *)new_db,
			"/nkn/nvsd/http/config/t-proxy/*",
			0,
			&tproxy_interfaces);
		bail_error(err);
		bail_null(tproxy_interfaces);

		count = tstr_array_length_quick(tproxy_interfaces);

		if (count == 0) {
		    err = md_commit_set_return_status_msg(commit, 1,
			    "Enable 'delivery protocol http transparent' on the delivery interface to "
			    "enable 'optimized' option.");
		    bail_error(err);
		    goto bail;
		}
	    }
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
			0, 5, "nkn", "nvsd", "system", "config", "mod_rapi")
		|| bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
		    0, 5, "nkn", "nvsd", "system","config", "mod_dmi")) {

	    tbool mod_rapi_status = false, mod_dmi_status = false;

	    err = mdb_get_node_value_tbool(commit, new_db,
		    "/nkn/nvsd/system/config/mod_rapi", 0, NULL, &mod_rapi_status);
	    bail_error(err);

	    err = mdb_get_node_value_tbool(commit, new_db,
		    "/nkn/nvsd/system/config/mod_dmi", 0, NULL, &mod_dmi_status);
	    bail_error(err);

	    if (mod_rapi_status && mod_dmi_status) {
		/* both can't be enabled */
		err = md_commit_set_return_status_msg(commit, 1,
			"Both DMI and Rest-API service can not be enabled");
		bail_error(err);
		goto bail;
	    }
	}
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

 bail:
    tstr_array_free(&tproxy_interfaces);
    return(err);
}

int
bn_write_proc_fs(const bn_binding *proc_name, const bn_binding *proc_value);
int
bn_write_proc_fs(const bn_binding *proc_name, const bn_binding *proc_value)
{
    int err = 0;
    char *proc_file = NULL;
    tstring *proc_buf = NULL;
    tbool exist = false;
    mode_t proc_fs_mode = JNPR_PROC_FILE_MODE;

    if ((NULL == proc_name) || (NULL == proc_value)) {
	err = 1;
	goto bail;
    }
    err = bn_binding_get_str(proc_name, ba_value, bt_string, NULL, &proc_file);
    bail_error(err);

    if (NULL == proc_file) {
	err = 1;
	goto bail;
    }
    /* we have got the name of proc file, now check if it exists */
    err = lf_test_is_regular(proc_file, &exist);

    if (err || !exist) {
	lc_log_basic(LOG_ERR, "Could not read proc file '%s'", proc_file);
	err = lc_err_file_not_found;
	goto bail;
    }
    err = bn_binding_get_tstr(proc_value, ba_value, bt_string, NULL, &proc_buf);
    bail_error(err);

    if ((NULL == proc_buf) || (0 == ts_length(proc_buf))) {
	err = 1;
	goto bail;
    }
    err = lf_write_file(proc_file,
			proc_fs_mode,
			(const char *)ts_str(proc_buf),
			true,
			ts_length(proc_buf));
    bail_error(err);

bail:
    ts_free(&proc_buf);
    safe_free(proc_file);
    return err;
}


static int
md_nvsd_write_proc(md_commit *commit, const mdb_db *db,
		    const char *action_name,
		    bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    const bn_binding *bn_proc_name = NULL, *bn_proc_value = NULL;

    bn_binding_array_dump(NULL, params, LOG_NOTICE);

    err = bn_binding_array_get_binding_by_name(params, "proc-name", &bn_proc_name);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name(params, "proc-value", &bn_proc_value);
    bail_error(err);

    err = bn_write_proc_fs(bn_proc_name, bn_proc_value);
    bail_error(err);

bail:
    return err;
}

static int
md_nvsd_get_engine_id( md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    tstring *t_hostid = NULL, *t_engineid = NULL;

    err = mdb_get_node_value_tstr(commit, db, "/system/hostid", 0, NULL, &t_hostid);
    bail_error_null(err, t_hostid);

    /*
       if len(hostid) < 27
	   use hostid as it is.
       else {it should be UUID}
	   UUID format is 49434D53-0200-9060-2500-609025000042
	   1. remove all "-"
	   2. trim it ot 27 characters
    */
    if (ts_length(t_hostid) > 27) {
	err = ts_remove_char(t_hostid, '-');
	bail_error(err);
	/* check again, */
	if (ts_length(t_hostid) > 27) {
	    err = ts_trim_num_chars(t_hostid, 0, ts_length(t_hostid) -27);
	    bail_error(err);
	}
	lc_log_debug(jnpr_log_level, "Engine-ID:%s, %d",
		ts_str(t_hostid), ts_length(t_hostid));
    }
    t_engineid = t_hostid;

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
                             0, ts_str(t_engineid));
    bail_error_null(err, *ret_binding);

bail:
    ts_free(&t_hostid);
    return err ;
}

static int
md_nvsd_side_effects(
	md_commit *commit,
	const mdb_db *old_db,
	mdb_db *inout_new_db,
	mdb_db_change_array *change_list,
	void *arg)
{
    int err = 0, num_changes=0;
    const mdb_db_change *change = NULL;

    /* mod-rest-api is enabled, set "-r" to params list */
    num_changes = mdb_db_change_array_length_quick(change_list);

    for (int i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i );
	bail_null(change);

	if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
			0, 5, "nkn", "nvsd", "system", "config", "mod_rapi")) {
	    const bn_attrib *new_attrib = NULL;
	    tbool mod_rapi_status = false;

	    new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);

	    err = bn_attrib_get_tbool(new_attrib, NULL, NULL,
		    &mod_rapi_status);
	    bail_error(err);

	    if (mod_rapi_status) {
		err = md_pm_add_param(commit, inout_new_db, "agentd", "-r");
		bail_error(err);

		err = md_pm_enable_launch_auto(commit, inout_new_db, "agentd");
		bail_error(err);
	    } else {
		err = md_pm_remove_param(commit, inout_new_db, "agentd", "-r");
		bail_error(err);

		err = md_pm_disable_launch_auto(commit, inout_new_db, "agentd");
		bail_error(err);
	    }
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
			0, 5, "nkn", "nvsd", "system", "config", "mod_ccf_dist")) {
	    const bn_attrib *new_attrib = NULL;
	    tbool mod_jcf_status = false;

	    new_attrib = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);

	    err = bn_attrib_get_tbool(new_attrib, NULL, NULL,
		    &mod_jcf_status);
	    bail_error(err);
	    if (mod_jcf_status) {
		err = md_pm_enable_launch_auto(commit, inout_new_db, "jcfg-server");
		bail_error(err);
	    } else {
		err = md_pm_disable_launch_auto(commit, inout_new_db, "jcfg-server");
		bail_error(err);
	    }
	}
    }

bail:
    return err;
}

int
md_pm_add_param(md_commit *commit, mdb_db *inout_new_db, const char *pname,
	const char *param)
{
    int err = 0, found = 0;

    err =  md_pm_update_param(commit, inout_new_db, pname, param, 1, &found);
    bail_error(err);

bail:
    return err;
}
/*
 * if add == 1, add the param,
 * else delete the param
 *
 * return found =1 , if any node was found
 */
int
md_pm_update_param(md_commit *commit, mdb_db *inout_new_db, const char *pname,
	const char *param, int add, int *param_found)
{
    int err = 0 , num_bindings = 0, i = 0, found = 0;
    const char * last_param_index = NULL;
    int next_param_index = 0, param_index = 0;
    const tstring *name = NULL;
    char *param_value = NULL;
    node_name_t node_name = {0};
    bn_binding_array *param_array = NULL;
    bn_binding *binding = NULL;
    tstr_array *name_parts = NULL;

    snprintf(node_name, sizeof(node_name),
	    "/pm/process/%s/launch_params", pname);

    err = mdb_iterate_binding(commit, inout_new_db, node_name, mdqf_sel_iterate_subtree,
	    &param_array);
    bail_error(err);

    //bn_binding_array_dump("PARAMS", param_array, LOG_NOTICE);

    num_bindings = bn_binding_array_length_quick(param_array);

    for (i = 0; i < num_bindings; i++) {
        binding = bn_binding_array_get_quick(param_array, i);
	if (binding == NULL) {
	    continue;
	}
        err = bn_binding_get_name(binding, &name);
        bail_error(err);

	snprintf(node_name, sizeof(node_name),
		"/pm/process/%s/launch_params/*/param", pname);
	if(bn_binding_name_pattern_match(ts_str_maybe(name), node_name)) {
	    //bn_binding_dump(LOG_NOTICE, "ADD-PARAM", binding);

	    safe_free(param_value);
	    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &param_value);
	    bail_error_null(err, param_value);
	    if (0 == strcmp(param_value,param) ) {
		/* no need to add again */
		found = 1;
		break;
	    } else {
		/* it is a different param */
	    }
	} else {
	    continue;
	}
    }

    if (param_found) {
	*param_found = found;
    }

    if (found) {
	/* found the param in bindings, don't add again */
	if (add) {
	    /* do nothing */
	} else {
	    /* delete the param */
	    bail_null(binding);
	    err = bn_binding_get_name_parts(binding, &name_parts);
	    bail_error(err);

	    last_param_index = tstr_array_get_str_quick(name_parts, 4);
	    param_index = atoi(last_param_index);
	    snprintf(node_name, sizeof(node_name),
		    "/pm/process/%s/launch_params/%d", pname, param_index);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_delete, 0, bt_string,
		    param, node_name);
	    bail_error(err);
	}
    } else {
	if (add) {
	    /* param is not found, add it */
	    if (binding != NULL) {
		err = bn_binding_get_name_parts(binding, &name_parts);
		bail_error(err);

		last_param_index = tstr_array_get_str_quick(name_parts, 4);
		next_param_index = atoi(last_param_index) + 1;
	    } else {
		/* enabling it and no other params are avialable  */
		last_param_index = "";
		next_param_index = 1;
	    }

	    //lc_log_basic(LOG_NOTICE, "index: %s, next: %d", last_param_index, next_param_index);
	    snprintf(node_name, sizeof(node_name),
		    "/pm/process/%s/launch_params/%d/param", pname, next_param_index);
	    err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0, bt_string,
		    param, node_name);
	    bail_error(err);
	} else {
	    /* nothing to do, as param is already deleted */
	}
    }

bail:
    tstr_array_free(&name_parts);
    safe_free(param_value);
    return err;
}

int
md_pm_remove_param(md_commit *commit, mdb_db *inout_new_db, const char *pname,
	const char *param)
{
    int err = 0, found = 1;

    while (found) {
	err =  md_pm_update_param(commit, inout_new_db, pname, param, 0, &found);
	bail_error(err);
    }

bail:
    return err;
}

int
md_pm_enable_launch_auto(md_commit *commit, mdb_db *inout_new_db, const char *pname)
{
    int err = 0;
    node_name_t node_name = {0};

    snprintf(node_name, sizeof(node_name), "/pm/process/%s/auto_launch", pname);

    err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
	    0, bt_bool, "true", node_name);
    bail_error(err);

bail:
    return err;
}
int
md_pm_disable_launch_auto(md_commit *commit, mdb_db *inout_new_db, const char *pname)
{
    int err = 0;
    node_name_t node_name = {0};

    snprintf(node_name, sizeof(node_name), "/pm/process/%s/auto_launch", pname);

    err = mdb_set_node_str(commit, inout_new_db, bsso_modify,
	    0, bt_bool, "false", node_name);
    bail_error(err);

bail:
    return err;
}

static int
md_nvsd_upgrade_downgrade(const mdb_db *old_db,
			    mdb_db *inout_new_db,
			    uint32 from_version,
			    uint32 to_version,
			    void *arg)
{
    int err = 0;
    tstring *ts_mod_dmi = NULL;

    /* Handle generic upgrade rules first */
    err = md_generic_upgrade_downgrade(old_db, inout_new_db,
				       from_version, to_version, arg);
    bail_error(err);

    if (to_version == 14) {
	err = mdb_get_node_value_tstr(NULL, inout_new_db,
		"/nkn/nvsd/system/config/mod_dmi", 0, NULL, &ts_mod_dmi);
	bail_error(err);
	err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		0, bt_bool, "true", "/nkn/nvsd/system/config/mod_dmi");
	bail_error(err);
	if (ts_mod_dmi && ts_equal_str(ts_mod_dmi, "1", false)) {
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		    0, bt_bool, "true", "/nkn/nvsd/system/config/mod_dmi");
	    bail_error(err);
	} else {
	    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
		    0, bt_bool, "false", "/nkn/nvsd/system/config/mod_dmi");
	    bail_error(err);
	}
    }

bail:
    ts_free(&ts_mod_dmi);
    return err;
}
