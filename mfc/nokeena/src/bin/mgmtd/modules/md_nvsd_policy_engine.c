/*
 *
 * Filename:  md_nvsd_policy_engine.c
 * Date:      2010/08/27
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "nkn_mgmt_defs.h"
#include "nkn_common_config.h"
#include "nvsd_mgmt_pe.h"
#include "nkn_cntr_api.h"


/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int md_nvsd_policy_engine_init(const lc_dso_info *info, void *data);

const bn_str_value md_nvsd_pe_initial_values[] = {

};

int
md_nvsd_pe_is_found_in_lst(md_commit *commit, const mdb_db *old_db,
					const char *item_name,const char *lst_nd, tbool *bound);
/*----------------------------------------------------------------------------
 * LOCAL FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/

static md_upgrade_rule_array *md_nvsd_pe_ug_rules = NULL;

static int
md_nvsd_pe_commit_check(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg);

static int
md_nvsd_pe_add_initial(md_commit *commit, mdb_db *db, void *arg);

static int md_pe_ns_check (md_commit *commit,const  mdb_db *db,
		const char *action_name, bn_binding_array *params, void *cb_arg);

static int
md_nkn_pe_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

static int
md_get_pe_err_count(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg);

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
int
md_nvsd_policy_engine_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd-policy_engine", 2,
	    "/nkn/nvsd/policy_engine", "/nkn/nvsd/policy_engine/config",
	    0,
	    NULL, NULL,
	    md_nvsd_pe_commit_check,
	    NULL,
	    NULL,
	    NULL,
	    0,
	    0,
	    md_generic_upgrade_downgrade,
	    &md_nvsd_pe_ug_rules,
	    md_nvsd_pe_add_initial,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/config/policy/script/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "An entry for each script based policy";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/config/policy/script/*/commited";
    node->mrn_value_type =         bt_bool;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/config/server/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = NKN_MAX_PE_SRVR_STR;
    node->mrn_limit_str_no_charlist = NKN_PE_ALLOWED_CHARS;
    node->mrn_limit_wc_count_max = NKN_MAX_PE_SRVR;
    node->mrn_bad_value_msg =       "Error creating Node";
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/nkn/nvsd/policy_engine/config/server/*/namespace/*";
    node->mrn_value_type =         bt_string;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/config/server/*/proto";
    node->mrn_value_type =         bt_string;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/config/server/*/uri";
    node->mrn_value_type =         bt_string;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/config/server/*/enabled";
    node->mrn_value_type =         bt_bool;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/config/server/*/endpoint/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = NKN_MAX_PE_HOST_STR;
    node->mrn_limit_str_no_charlist = NKN_PE_ALLOWED_CHARS;
    node->mrn_limit_wc_count_max = NKN_MAX_PE_SRVR_HST;
    node->mrn_bad_value_msg =       "Error creating Node";
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/config/server/*/endpoint/*/port";
    node->mrn_value_type =         bt_uint16;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_initial_value =      "0";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /* Action Nodes */
    err = mdm_new_action(module, &node, 3);
    bail_error(err);
    node->mrn_name               = "/nkn/nvsd/policy_engine/actions/check_policy_namespace_dependancy";
    node->mrn_node_reg_flags     = mrf_flags_reg_action;
    node->mrn_cap_mask           = mcf_cap_action_basic;
    node->mrn_action_func = md_pe_ns_check;
    node->mrn_actions[0]->mra_param_name =  "policy";
    node->mrn_actions[0]->mra_param_type =  bt_string;
    node->mrn_actions[1]->mra_param_name =  "ns_active";
    node->mrn_actions[1]->mra_param_type =  bt_bool;
    node->mrn_actions[2]->mra_param_name =  "silent";
    node->mrn_actions[2]->mra_param_type =  bt_bool;
    node->mrn_description        = "action node to check for policy dependancy" ;
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Monitoring Nodes */

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/stats/tot_eval_http_recv_req";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_nkn_pe_stats_get_uint64;
    node->mrn_mon_get_arg =        (void *)"glob_pe_eval_http_recv_req";
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/stats/tot_eval_http_send_resp";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_nkn_pe_stats_get_uint64;
    node->mrn_mon_get_arg =        (void *)"glob_pe_eval_http_send_resp";
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/stats/tot_eval_cl_http_recv_req";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_nkn_pe_stats_get_uint64;
    node->mrn_mon_get_arg =        (void *)"glob_pe_eval_cl_http_recv_req";
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/stats/tot_eval_om_recv_resp";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_nkn_pe_stats_get_uint64;
    node->mrn_mon_get_arg =        (void *)"glob_pe_eval_om_recv_resp";
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/stats/tot_eval_om_send_req";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_nkn_pe_stats_get_uint64;
    node->mrn_mon_get_arg =        (void *)"glob_pe_eval_om_send_req";
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/stats/tot_eval_err_http_recv_req";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_pe_err_count;
    node->mrn_mon_get_arg =        (void *)"1";
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/stats/tot_eval_err_http_send_resp";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_pe_err_count;
    node->mrn_mon_get_arg =        (void *)"2";
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/stats/tot_eval_err_cl_http_recv_req";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_pe_err_count;
    node->mrn_mon_get_arg =        (void *)"3";
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/stats/tot_eval_err_om_recv_resp";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_pe_err_count;
    node->mrn_mon_get_arg =        (void *)"4";
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/policy_engine/stats/tot_eval_err_om_send_req";
    node->mrn_value_type =         bt_uint64;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_mon_get_func =       md_get_pe_err_count;
    node->mrn_mon_get_arg =        (void *)"5";
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Upgrade processing nodes */

    err = md_upgrade_rule_array_new(&md_nvsd_pe_ug_rules);
    bail_error(err);
    ra = md_nvsd_pe_ug_rules;

    /*! Added in module version 2
     */
    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/policy_engine/config/server/*";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/policy_engine/config/server/*/namespace/*";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/policy_engine/config/server/*/proto";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/policy_engine/config/server/*/uri";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/policy_engine/config/server/*/enabled";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/policy_engine/config/server/*/endpoint/*/port";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/policy_engine/config/server/*/endpoint/*";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

bail:
    return(err);
}

/*----------------------------------------------------------------------------
 * LOCAL FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
static int
md_nvsd_pe_commit_check(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    int i, num_changes = 0;
    const mdb_db_change *change = NULL;
    const char *srvr_name = NULL;

    /*
     * No checking to be done.
     */
    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);
	if ( (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			"nkn", "nvsd", "policy_engine", "config", "server", "*")))
	{
	    srvr_name = tstr_array_get_str_quick(change->mdc_name_parts, 5);;
	    bail_null(srvr_name);
	    if(mdct_delete == change->mdc_change_type) {
		tbool bound = false;

		err = md_nvsd_pe_is_found_in_lst(commit, old_db,
			srvr_name, "/nkn/nvsd/policy_engine/config/server/%s/namespace/*",&bound);
		bail_error(err);

		if(bound) {
		    err = md_commit_set_return_status_msg_fmt(
			    commit, 1,
			    "Policy server has namesapce attached to it.Please unbind\n");
		    bail_error(err);
		    goto bail;
		}
	    }
	}
    }
    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    return(err);
}


int
md_nvsd_pe_is_found_in_lst(md_commit *commit, const mdb_db *old_db,
					const char *item_name,const char *lst_nd, tbool *bound)
{
    int err = 0;
    node_name_t item_nds = {0};
    tstr_array *item_lst = NULL;

    snprintf(item_nds, sizeof(item_nds), lst_nd,
	    item_name);

    err = mdb_get_matching_tstr_array(commit,
	    old_db,
	    item_nds,
	    0,
	    &item_lst);
    bail_error(err);

    if(item_lst && tstr_array_length_quick(item_lst) ) {

	*bound = true;
    }

bail:
    tstr_array_free(&item_lst);
    return err;
}


static int
md_nvsd_pe_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    bn_binding *binding = NULL;

    err = mdb_create_node_bindings
	(commit, db, md_nvsd_pe_initial_values,
	 sizeof(md_nvsd_pe_initial_values) / sizeof(bn_str_value));
    bail_error(err);

bail:
    bn_binding_free(&binding);
    return(err);
}

static int
md_pe_ns_check (md_commit *commit,const  mdb_db *db,
		 const char *action_name, bn_binding_array *params,void *cb_arg)
{
    int err = 0;
    tstr_array *namespace_policy_nodes = NULL;
    const char *binding_name = NULL;
    tstring *t_ns_policy = NULL;
    tstring *t_policy = NULL;
    tstring *t_is_ns_active = NULL;
    tbool t_active = false;
    tbool t_silent = false;
    char *node_ns_active = NULL;
    char *print_msg = NULL;
    bn_binding *binding = NULL;
    tstr_array *name_parts = NULL;
    uint32 i = 0;

    bail_null(commit);
    bail_null(db);

    err = mdb_get_matching_tstr_array( commit,
	    db,
	    "/nkn/nvsd/namespace/*/policy/file",
	    0,
	    &namespace_policy_nodes);
    bail_error_null(err, namespace_policy_nodes);

    /* Get the Policy name */
    err = bn_binding_array_get_value_tstr_by_name
	(params, "policy", NULL, &t_policy);
    bail_error_null(err, t_policy);

    /* Get the Check Type */
    err = bn_binding_array_get_value_tbool_by_name
	(params, "ns_active", NULL, &t_active);
    bail_error(err);

    /* silent or not */
    err = bn_binding_array_get_value_tbool_by_name
	(params, "silent", NULL, &t_silent);
    bail_error(err);

    for( i = 0; i < tstr_array_length_quick(namespace_policy_nodes) ; i++)
    {

	binding_name = tstr_array_get_str_quick(
		namespace_policy_nodes, i);
	bail_null(binding_name);

	err = mdb_get_node_value_tstr(commit, db,
		binding_name,
		0, NULL, &t_ns_policy);
	bail_error_null(err, t_ns_policy);

	/* Check to see if policy has a namespace associated with it */
	if(!strcmp(ts_str(t_ns_policy), ts_str(t_policy))) {

	    err =   ts_tokenize_str(binding_name, '/', '\\', '"', ttf_ignore_leading_separator, &name_parts);
	    bail_error_null(err, name_parts);

	    /*Check to return true if namespace is active or not */
	    /* used for conditions such as commit or revert where only an active namespace is of importance*/
	    if(t_active) {
		node_ns_active = smprintf("/nkn/nvsd/namespace/%s/status/active", tstr_array_get_str_quick(name_parts, 3));
		bail_null(node_ns_active);
		err = mdb_get_node_value_tstr(commit, db, node_ns_active, 0, NULL, &t_is_ns_active);
		bail_error_null(err, t_is_ns_active);
		safe_free(node_ns_active);

		if(!strncmp("true", ts_str(t_is_ns_active), 4)) {

		    print_msg = smprintf("Policy is associated with namespace %s.Please inactivate the namespace.",
			    tstr_array_get_str_quick(name_parts, 3));
		} else {
		    err = md_commit_set_return_status(commit, 0);
		    bail_error(err);
		    goto bail;

		}
	    } else {
		/* used for conditions such as delete,where bound namespace is of importance */
		print_msg = smprintf("Policy is associated with namespace %s.Please unbind the policy",
			tstr_array_get_str_quick(name_parts, 3));
	    }
	    err = bn_binding_new_str(&binding, "associated_ns", ba_value, bt_string, 0, tstr_array_get_str_quick(name_parts, 3));
	    bail_error_null(err, binding);

	    err = md_commit_binding_append(commit, 0, binding);
	    bail_error(err);

	    /*
	     *if its silent dont compain of any error,
	     *the caller will know the namespace associated with policy through the binding passed.
	     */
	    if(t_silent) {
		err = md_commit_set_return_status(commit, 0);
		bail_error(err);
		goto bail;

	    }
	    else
	    {	err = md_commit_set_return_status_msg(commit, 1,print_msg);
		bail_error(err);
		lc_log_basic(LOG_NOTICE,"Policy is assoicated with namespace");

		safe_free(print_msg);
		goto bail;
	    }
	}
	ts_free(&t_ns_policy);
	safe_free(print_msg);
	safe_free(node_ns_active);
	bn_binding_free(&binding);
	ts_free(&t_is_ns_active);

    }
    err = md_commit_set_return_status(commit, 0);
    bail_error(err);
    lc_log_basic(LOG_NOTICE,"Policy is not assoicated with namespace");
bail:
    ts_free(&t_ns_policy);
    ts_free(&t_policy);
    safe_free(node_ns_active);
    safe_free(print_msg);
    bn_binding_free(&binding);
    ts_free(&t_is_ns_active);
    tstr_array_free(&namespace_policy_nodes);
    return err;

}

static int
md_nkn_pe_stats_get_uint64(
        md_commit *commit,
        const mdb_db *db,
        const char *node_name,
        const bn_attrib_array *node_attribs,
        bn_binding **ret_binding,
        uint32 *ret_node_flags,
        void *arg)
{
    int err = 0;
    uint64 n;

    bail_null(arg);
    n = nkncnt_get_uint64((char*) arg);
    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value, 0, n);
    bail_error(err);

bail:
    return err;
}


static int
md_get_pe_err_count(
	md_commit *commit,
	const mdb_db *db,
	const char *node_name,
	const bn_attrib_array *node_attribs,
	bn_binding **ret_binding,
	uint32 *ret_node_flags,
	void *arg)
{
    int err = 0;
    char *resp;
    uint64 val = 0;

    bail_null(arg);
    resp = ((char *)arg);


    switch(resp[0]) {
	// "/nkn/nvsd/policy_engine/stats/tot_eval_err_http_recv_req";
	case '1':
	    {
		uint64 hrq_01 = nkncnt_get_uint64("glob_pe_err_http_req_get_no_token");
		uint64 hrq_02 = nkncnt_get_uint64("glob_pe_err_http_req_get_no_host");
		uint64 hrq_03 = nkncnt_get_uint64("glob_pe_err_http_req_get_no_uri");
		uint64 hrq_04 = nkncnt_get_uint64("glob_pe_err_http_req_get_no_method");
		uint64 hrq_05 = nkncnt_get_uint64("glob_pe_err_http_req_get_unknown_token");
		uint64 hrq_06 = nkncnt_get_uint64("glob_pe_err_http_req_set_no_action");
		uint64 hrq_07 = nkncnt_get_uint64("glob_pe_err_http_req_set_redirect");
		uint64 hrq_08 = nkncnt_get_uint64("glob_pe_err_http_req_set_no_cache_object");
		uint64 hrq_09 = nkncnt_get_uint64("glob_pe_err_http_req_set_cache_object");
		uint64 hrq_10 = nkncnt_get_uint64("glob_pe_err_http_req_set_origin_server");
		uint64 hrq_11 = nkncnt_get_uint64("glob_pe_err_http_req_set_url_rewrite");
		uint64 hrq_12 = nkncnt_get_uint64("glob_pe_err_http_req_set_ins_hdr_param");
		uint64 hrq_13 = nkncnt_get_uint64("glob_pe_err_http_req_set_ins_hdr");
		uint64 hrq_14 = nkncnt_get_uint64("glob_pe_err_http_req_set_rmv_hdr_param");
		uint64 hrq_15 = nkncnt_get_uint64("glob_pe_err_http_req_set_rmv_hdr");
		uint64 hrq_16 = nkncnt_get_uint64("glob_pe_err_http_req_set_mod_hdr_param");
		uint64 hrq_17 = nkncnt_get_uint64("glob_pe_err_http_req_set_mod_hdr");
		uint64 hrq_18 = nkncnt_get_uint64("glob_pe_err_http_req_set_unknown_action");
		uint64 hrq_19 = nkncnt_get_uint64("glob_pe_err_http_req_set_cache_index_append");
		uint64 hrq_20 = nkncnt_get_uint64("glob_pe_err_http_req_set_transmit_rate");
		val = hrq_01 + hrq_02 + hrq_03 + hrq_04 + hrq_05 + hrq_06
		    + hrq_07 + hrq_08 + hrq_09 + hrq_10 + hrq_11 + hrq_12
		    + hrq_13 + hrq_14 + hrq_15 + hrq_16 + hrq_17 + hrq_18
		    + hrq_19 + hrq_20;
	    }
	    break;

	    // "/nkn/nvsd/policy_engine/stats/tot_eval_err_http_send_resp";
	case '2':
	    {
		uint64 hrsp_01 = nkncnt_get_uint64("glob_pe_err_http_rsp_get_no_token");
		uint64 hrsp_02 = nkncnt_get_uint64("glob_pe_err_http_rsp_get_no_host");
		uint64 hrsp_03 = nkncnt_get_uint64("glob_pe_err_http_rsp_get_no_uri");
		uint64 hrsp_04 = nkncnt_get_uint64("glob_pe_err_http_rsp_get_no_method");
		uint64 hrsp_05 = nkncnt_get_uint64("glob_pe_err_http_rsp_get_unknown_token");
		uint64 hrsp_06 = nkncnt_get_uint64("glob_pe_err_http_rsp_set_no_action");
		uint64 hrsp_07 = nkncnt_get_uint64("glob_pe_err_http_rsp_set_ins_hdr_param");
		uint64 hrsp_08 = nkncnt_get_uint64("glob_pe_err_http_rsp_set_ins_hdr");
		uint64 hrsp_09 = nkncnt_get_uint64("glob_pe_err_http_rsp_set_rmv_hdr_param");
		uint64 hrsp_10 = nkncnt_get_uint64("glob_pe_err_http_rsp_set_rmv_hdr");
		uint64 hrsp_11 = nkncnt_get_uint64("glob_pe_err_http_rsp_set_mod_hdr_param");
		uint64 hrsp_12 = nkncnt_get_uint64("glob_pe_err_http_rsp_set_mod_hdr");
		uint64 hrsp_13 = nkncnt_get_uint64("glob_pe_err_http_rsp_set_set_ip_tos");
		uint64 hrsp_14 = nkncnt_get_uint64("glob_pe_err_http_rsp_set_unknown_action");
		val = hrsp_01 + hrsp_02 + hrsp_03 + hrsp_04 + hrsp_05
		    + hrsp_06 + hrsp_07 + hrsp_08 + hrsp_09 + hrsp_10
		    + hrsp_11 + hrsp_12 + hrsp_13 + hrsp_14;
	    }
	    break;

	    // "/nkn/nvsd/policy_engine/stats/tot_eval_err_cl_http_recv_req";
	case '3':
	    {
		uint64 hreq_01 = nkncnt_get_uint64("glob_pe_err_cl_http_req_get_no_token");
		uint64 hreq_02 = nkncnt_get_uint64("glob_pe_err_cl_http_req_get_no_host");
		uint64 hreq_03 = nkncnt_get_uint64("glob_pe_err_cl_http_req_get_no_uri");
		uint64 hreq_04 = nkncnt_get_uint64("glob_pe_err_cl_http_req_get_no_method");
		uint64 hreq_05 = nkncnt_get_uint64("glob_pe_err_cl_http_req_get_unknown_token");
		uint64 hreq_06 = nkncnt_get_uint64("glob_pe_err_cl_http_req_set_no_action");
		uint64 hreq_07 = nkncnt_get_uint64("glob_pe_err_cl_http_req_set_cluster_redriect");
		uint64 hreq_08 = nkncnt_get_uint64("glob_pe_err_cl_http_req_set_cluster_proxy");
		uint64 hreq_09 = nkncnt_get_uint64("glob_pe_err_cl_http_req_set_unknown_action");
		val = hreq_01 + hreq_02 + hreq_03 + hreq_04 + hreq_05
		    + hreq_06 + hreq_07 + hreq_08 + hreq_09;
	    }
	    break;

	    // "/nkn/nvsd/policy_engine/stats/tot_eval_err_om_recv_resp";
	case '4':
	    {
		uint64 orsp_01 = nkncnt_get_uint64("glob_pe_err_om_rsp_get_no_token");
		uint64 orsp_02 = nkncnt_get_uint64("glob_pe_err_om_rsp_get_no_host");
		uint64 orsp_03 = nkncnt_get_uint64("glob_pe_err_om_rsp_get_no_uri");
		uint64 orsp_04 = nkncnt_get_uint64("glob_pe_err_om_rsp_get_no_method");
		uint64 orsp_05 = nkncnt_get_uint64("glob_pe_err_om_rsp_get_unknown_token");
		uint64 orsp_06 = nkncnt_get_uint64("glob_pe_err_om_rsp_set_no_action");
		uint64 orsp_07 = nkncnt_get_uint64("glob_pe_err_om_rsp_set_redirect");
		uint64 orsp_08 = nkncnt_get_uint64("glob_pe_err_om_rsp_set_ins_hdr_param");
		uint64 orsp_09 = nkncnt_get_uint64("glob_pe_err_om_rsp_set_ins_hdr");
		uint64 orsp_10 = nkncnt_get_uint64("glob_pe_err_om_rsp_set_rmv_hdr_param");
		uint64 orsp_11 = nkncnt_get_uint64("glob_pe_err_om_rsp_set_rmv_hdr");
		uint64 orsp_12 = nkncnt_get_uint64("glob_pe_err_om_rsp_set_mod_hdr_param");
		uint64 orsp_13 = nkncnt_get_uint64("glob_pe_err_om_rsp_set_mod_hdr");
		uint64 orsp_14 = nkncnt_get_uint64("glob_pe_err_om_rsp_set_unknown_action");
		val = orsp_01 + orsp_02 + orsp_03 + orsp_04 + orsp_05
		    + orsp_06 + orsp_07 + orsp_08 + orsp_09 + orsp_10
		    + orsp_11 + orsp_12 + orsp_13 + orsp_14;
	    }
	    break;

	    // "/nkn/nvsd/policy_engine/stats/tot_eval_err_om_send_req";
	case  '5':
	    {
		uint64 oreq_01 = nkncnt_get_uint64("glob_pe_err_om_req_get_no_token");
		uint64 oreq_02 = nkncnt_get_uint64("glob_pe_err_om_req_get_no_host");
		uint64 oreq_03 = nkncnt_get_uint64("glob_pe_err_om_req_get_no_uri");
		uint64 oreq_04 = nkncnt_get_uint64("glob_pe_err_om_req_get_no_method");
		uint64 oreq_05 = nkncnt_get_uint64("glob_pe_err_om_req_get_unknown_token");
		uint64 oreq_06 = nkncnt_get_uint64("glob_pe_err_om_req_set_no_action");
		uint64 oreq_07 = nkncnt_get_uint64("glob_pe_err_om_req_set_ins_hdr_param");
		uint64 oreq_08 = nkncnt_get_uint64("glob_pe_err_om_req_set_ins_hdr");
		uint64 oreq_09 = nkncnt_get_uint64("glob_pe_err_om_req_set_rmv_hdr_param");
		uint64 oreq_10 = nkncnt_get_uint64("glob_pe_err_om_req_set_rmv_hdr");
		uint64 oreq_11 = nkncnt_get_uint64("glob_pe_err_om_req_set_mod_hdr_param");
		uint64 oreq_12 = nkncnt_get_uint64("glob_pe_err_om_req_set_mod_hdr");
		uint64 oreq_13 = nkncnt_get_uint64("glob_pe_err_om_req_set_set_ip_tos");
		uint64 oreq_14 = nkncnt_get_uint64("glob_pe_err_om_req_set_unknown_action");
		val = oreq_01 + oreq_02 + oreq_03 + oreq_04 + oreq_05
		    + oreq_06 + oreq_07 + oreq_08 + oreq_09 + oreq_10
		    + oreq_11 + oreq_12 + oreq_13 + oreq_14;
	    }
	    break;
	default:
	    break;
    }

    err = bn_binding_new_uint64(ret_binding, node_name, ba_value, 0, val);
    bail_error(err);
bail:
    return err;

}


/* ---------------------------------END OF FILE---------------------------------------- */

