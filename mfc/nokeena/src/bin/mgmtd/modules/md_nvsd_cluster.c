/*
 *
 * Filename:  md_nvsd_cluster.c
 * Date:	  24 August 2010
 * Author:	  Manikandan.V
 *
 * (C) Copyright 2010 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include <arpa/inet.h>
#include "nkn_mgmt_defs.h"

/* ------------------------------------------------------------------------- */

int nkn_log_level = LOG_DEBUG;
const char *proxy_nodes[] = {
    "/nkn/nvsd/cluster/config/%s/proxy-local",
    "/nkn/nvsd/cluster/config/%s/proxy-base-path",
    "/nkn/nvsd/cluster/config/%s/proxy-addr-port",
    "/nkn/nvsd/cluster/config/%s/proxy-cluster-hash",
    NULL,
};
const char *redirect_nodes[] = {
    "/nkn/nvsd/cluster/config/%s/redirect-local",
    "/nkn/nvsd/cluster/config/%s/redirect-base-path",
    "/nkn/nvsd/cluster/config/%s/redirect-addr-port",
    "/nkn/nvsd/cluster/config/%s/cluster-hash",
    NULL,
};
int md_nvsd_cluster_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_nvsd_cluster_ug_rules = NULL;

static int md_nvsd_cluster_commit_check(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

static int md_nvsd_cluster_commit_side_effects(
	md_commit *commit,
	const mdb_db *old_db,
	mdb_db *inout_new_db,
	mdb_db_change_array *change_list,
	void *arg);

int md_cluster_is_configured(md_commit *commit, const mdb_db* db, const char *cl_name, tbool *cl_configd);
int md_cluster_is_ns_attached(md_commit *commit, const mdb_db* db, const char *cl_name, tbool *ns_attached);
int
md_cluster_copy_nodes(md_commit *commit, const mdb_db *old_db, mdb_db *new_db,
	const char *cl_name, const char *src_node[], const char *dest_node[]);

static int
md_nvsd_cluster_validate_name(const char *name,
			tstring **ret_msg);

int
md_nvsd_suffix_validate(md_commit *commit,
                                const mdb_db *old_db,
                                const mdb_db *new_db,
                                const mdb_db_change_array *change_list,
                                const tstring *node_name,
                                const tstr_array *node_name_parts,
                                mdb_db_change_type change_type,
                                const bn_attrib_array *old_attribs,
                                const bn_attrib_array *new_attribs,
                                void *arg);
int
md_nvsd_addr_port_validate(md_commit *commit,
                                const mdb_db *old_db,
                                const mdb_db *new_db,
                                const mdb_db_change_array *change_list,
                                const tstring *node_name,
                                const tstr_array *node_name_parts,
                                mdb_db_change_type change_type,
                                const bn_attrib_array *old_attribs,
                                const bn_attrib_array *new_attribs,
                                void *arg);
/* ------------------------------------------------------------------------- */
int
md_nvsd_cluster_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /* NOTE: for changing any default values, verify the impact on _side_effects() */
    err = mdm_add_module("cluster", 4,
                         "/nkn/nvsd/cluster", "/nkn/nvsd/cluster/config",
			 0,
                         md_nvsd_cluster_commit_side_effects, NULL,
                         md_nvsd_cluster_commit_check, NULL,
                         NULL, NULL,
                         0, 0,
                         md_generic_upgrade_downgrade,
		             &md_nvsd_cluster_ug_rules,
                         NULL, NULL,
                         NULL, NULL, NULL, NULL,
			 &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_str_no_charlist = "/\\*:|`\"?";
    node->mrn_description =        "Cluster Name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/server-map";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Server-Map name associated with the cluster";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/namespace";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Namespace name associated with the cluster";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/cluster-type";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value =      "1"; /* default cluster type is redirect */
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Cluster Type";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/cluster-hash";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value =      "2";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Cluster Hash";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/request-routing-method";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value =      "1";	/* {redirect,proxy}-only */
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Cluster Request Routing Method";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/redirect-addr-port";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "255.255.255.255:65535";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Cluster redirect address with port";
    node->mrn_check_node_func 	=  md_nvsd_addr_port_validate;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/redirect-local";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Cluster Redirect Local Option";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/redirect-base-path";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Cluster redirect base path";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/load-threshold";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  80;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_bad_value_msg =	   "error: Allowable input range for load-threshold is 0-100";
    node->mrn_description =        "Cluster Load balance Threshold";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/replicate-mode";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value =      "1";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Cluster Replicate Mode";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* if(value == "1") then {base-url }, if(value == "2") then { complete-url } */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/proxy-cluster-hash";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value =      "2";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Cluster Hash";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/proxy-addr-port";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "255.255.255.255:65535";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Cluster proxy address with port";
    node->mrn_check_node_func 	=  md_nvsd_addr_port_validate;
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* true = allow, false = deny */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/proxy-local";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Cluster proxy Local Option";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/proxy-base-path";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Cluster proxy Local Option";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/reset";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal|mrf_change_no_notify;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Cluster reset option ";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/configured";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal|mrf_change_no_notify;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Cluster configured option";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/suffix/*";
    node->mrn_value_type =	   bt_string;
    node->mrn_limit_wc_count_max = 1024;
    node->mrn_limit_str_max_chars = 16;	/* length of suffix */
    node->mrn_limit_str_charlist = CLIST_LOWER_LETTERS CLIST_NUMBERS;
    node->mrn_bad_value_msg =	   "Invalid suffix name";
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =	   mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =	   mcf_cap_node_restricted;
    node->mrn_description =        "Cluster suffix options";
    node->mrn_check_node_func =    md_nvsd_suffix_validate;
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* 1 is for proxy , 2 is for redirect */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/cluster/config/*/suffix/*/action";
    node->mrn_value_type =	   bt_uint32;
    node->mrn_initial_value =      "0";
    node->mrn_node_reg_flags =	   mrf_flags_reg_config_literal;
    node->mrn_cap_mask =	   mcf_cap_node_restricted;
    node->mrn_description =        "Cluster suffix action";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /* upgrade rules */

    err = md_upgrade_rule_array_new(&md_nvsd_cluster_ug_rules);
    bail_error(err);
    ra = md_nvsd_cluster_ug_rules ;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/cluster/config/*/proxy-cluster-hash";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "2";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/cluster/config/*/proxy-local";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str = "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/cluster/config/*/proxy-addr-port";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = "255.255.255.255:65535";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/cluster/config/*/proxy-base-path";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/cluster/config/*/reset";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str = "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/cluster/config/*/configured";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str = "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_nvsd_cluster_commit_check(md_commit *commit,
                     const mdb_db *old_db,
                     const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    int i = 0,num_changes = 0;
    const mdb_db_change *change = NULL;
    tstring *ret_msg = NULL;
    const char *cluster_map_name = NULL;
    tstr_array *t_cluster_maps = NULL;
    tstring *t_cluster_name = NULL;
    tstr_array *t_name_parts= NULL;

    num_changes = mdb_db_change_array_length_quick(change_list);
    lc_log_debug(nkn_log_level, "num_changes - %d", num_changes);
    for(i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 5, "nkn", "nvsd", "cluster", "config", "*"))	{
	    cluster_map_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    if (!cluster_map_name) goto bail;

	    err = md_nvsd_cluster_validate_name (cluster_map_name, &ret_msg);
	    if (err) {
		err = 0;
		err = md_commit_set_return_status_msg_fmt(
			commit, 1,
			_("error: %s\n"), ts_str(ret_msg));
		ts_free(&ret_msg);
		bail_error(err);
		goto bail;
	    }

	    if(mdct_delete == change->mdc_change_type){
		/* Get the clusters associated with the namespace */
		err = mdb_get_matching_tstr_array(commit,
			old_db,
			"/nkn/nvsd/namespace/*/cluster/*/cl-name",
			0,
			&t_cluster_maps);
		bail_error(err);

		if (t_cluster_maps != NULL) {
		    /* At least 1 or more cluster(s) found.
		     * Check if this cluster is associated with at least
		     * one namespace. If so, reject the commit
		     */
		    for(uint32 j = 0; j < tstr_array_length_quick(t_cluster_maps); j++) {

			lc_log_basic(LOG_DEBUG, "cluster_delete: binding: '%s'", tstr_array_get_str_quick(t_cluster_maps, j));

			/* Get the value of the node and compare the
			 * value with the cluster name.
			 */
			err = mdb_get_node_value_tstr(NULL,
				old_db,
				tstr_array_get_str_quick(t_cluster_maps, j),
				0, NULL,
				&t_cluster_name);
			bail_error(err);

			if (t_cluster_name && ts_equal_str(t_cluster_name, cluster_map_name, false)) {
			    /* Name matched... REJECT the commit */
			    /* Get the namespace name */
			    err = ts_tokenize_str(
				    tstr_array_get_str_quick(t_cluster_maps, j),
				    '/', '\0', '\0', 0, &t_name_parts);
			    bail_error(err);

			    err = md_commit_set_return_status_msg_fmt(
				    commit, 1,
				    _("Cluster \"%s\" is associated with namespace "
					"\"%s\".\nSorry cannot delete this entry"),
				    cluster_map_name,
				    tstr_array_get_str_quick(t_name_parts, 4));
			    bail_error(err);

			    tstr_array_free(&t_name_parts);
			    goto bail;
			}
			ts_free(&t_cluster_name);
			t_cluster_name = NULL;
		    }
		    tstr_array_free(&t_cluster_maps);
		}
	    }
	} else if (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 6, "nkn", "nvsd", "cluster", "config", "*", "server-map"))
	{
	    cluster_map_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    if (!cluster_map_name) goto bail;

	    err = md_nvsd_cluster_validate_name (cluster_map_name, &ret_msg);
	    if (err) {
		err = 0;
		err = md_commit_set_return_status_msg_fmt(
			commit, 1,
			_("error: %s\n"), ts_str(ret_msg));
		ts_free(&ret_msg);
		bail_error(err);
		goto bail;
	    }

	    if(mdct_modify == change->mdc_change_type){
		/* Get the clusters associated with the namespace */
		err = mdb_get_matching_tstr_array(commit,
			old_db,
			"/nkn/nvsd/namespace/*/cluster/*/cl-name",
			0,
			&t_cluster_maps);
		bail_error(err);

		if (t_cluster_maps != NULL) {
		    /* At least 1 or more cluster(s) found.
		     * Check if this cluster is associated with at least
		     * one namespace. If so, reject the commit
		     */
		    for(uint32 j = 0; j < tstr_array_length_quick(t_cluster_maps); j++) {

			lc_log_basic(LOG_DEBUG, "Modify server-map association from cluster: binding: '%s'", tstr_array_get_str_quick(t_cluster_maps, j));

			/* Get the value of the node and compare the
			 * value with the cluster name.
			 */
			err = mdb_get_node_value_tstr(NULL,
				old_db,
				tstr_array_get_str_quick(t_cluster_maps, j),
				0, NULL,
				&t_cluster_name);
			bail_error(err);

			if (t_cluster_name && ts_equal_str(t_cluster_name, cluster_map_name, false)) {
			    /* Name matched... REJECT the commit */
			    /* Get the cluster name */
			    err = ts_tokenize_str(
				    tstr_array_get_str_quick(t_cluster_maps, j),
				    '/', '\0', '\0', 0, &t_name_parts);
			    bail_error(err);

			    err = md_commit_set_return_status_msg_fmt(
				    commit, 1,
				    _("Cluster \"%s\" is associated with namespace "
					"\"%s\".\nSorry cannot modify the server-map association with the cluster"),
				    cluster_map_name,
				    tstr_array_get_str_quick(t_name_parts, 4));
			    bail_error(err);

			    tstr_array_free(&t_name_parts);
			    goto bail;
			}
			ts_free(&t_cluster_name);
			t_cluster_name = NULL;
		    }
		    tstr_array_free(&t_cluster_maps);
		}
	    }
	} else if (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 6, "nkn" ,"nvsd", "cluster" ,"config" ,"*", "request-routing-method")
		&& (mdct_modify == change->mdc_change_type)) {
	    /* setting of request-routing-method to load-balance is not allowed if cl-type is not set */
	    uint32 rr_method = 0;
	    tbool cl_configured = false, ns_added = false;

	    cluster_map_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    if (!cluster_map_name) goto bail;

	    /* get new value */
	    err = mdb_get_node_value_uint32(commit, new_db, ts_str(change->mdc_name),
		    0, NULL, &rr_method);
	    bail_error(err);

	    err = md_cluster_is_ns_attached(commit, new_db, cluster_map_name, &ns_added);
	    bail_error(err);
	    err = md_cluster_is_configured(commit, new_db, cluster_map_name, &cl_configured);
	    bail_error(err);

	    if (rr_method == 2) {
		if (ns_added == false) {
		    /* disallowed, set req-routing-method first */
		    err = md_commit_set_return_status_msg_fmt(commit, EINVAL,
			    "Cluster is not added to the namespace");
		    bail_error(err);
		    goto bail;
		}
		if (cl_configured == false) {
		    /* disallowed, set req-routing-method first */
		    err = md_commit_set_return_status_msg_fmt(commit, EINVAL,
			    "Request routing method is not set");
		    bail_error(err);
		    goto bail;
		}
	    }
	} else if ((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 6, "nkn", "nvsd", "cluster", "config", "*", "replicate-mode")
		    || bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "cluster", "config", "*", "load-threshold"))
		&& ((mdct_modify == change->mdc_change_type))) {

	    node_name_t cl_node = {0};
	    tbool cl_reset = false, cl_configured = false, ns_added = false;

	    lc_log_debug(nkn_log_level, "got %s, chg_type(%d)", ts_str(change->mdc_name), change->mdc_change_type);
	    cluster_map_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    if (!cluster_map_name) goto bail;

	    snprintf(cl_node, sizeof(cl_node), "/nkn/nvsd/cluster/config/%s/reset", cluster_map_name);
	    err = mdb_get_node_value_tbool(commit, new_db, cl_node, 0, NULL, &cl_reset);
	    bail_error(err);

	    if (cl_reset == true) {
		/* resetting the cluster to defaults, so allow the changes */
		continue;
	    }

	    err = md_cluster_is_ns_attached(commit, new_db, cluster_map_name, &ns_added);
	    bail_error(err);
	    err = md_cluster_is_configured(commit, new_db, cluster_map_name, &cl_configured);
	    bail_error(err);
	    /* disallow change if these nodes are modified which out setting req-routing-method */
	    if (ns_added == false) {
		/* disallowed, set req-routing-method first */
		err = md_commit_set_return_status_msg_fmt(commit, EINVAL,
			"Cluster is not added to the namespace");
		bail_error(err);
		goto bail;
	    }
	    if (cl_configured == false) {
		/* disallowed, set req-routing-method first */
		err = md_commit_set_return_status_msg_fmt(commit, EINVAL,
			"Request routing method is not set");
		bail_error(err);
		goto bail;
	    }
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "cluster", "config", "*", "configured")) {
	    tbool ns_attached = false, cl_configured = false;

	    cluster_map_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    if (!cluster_map_name) goto bail;

	    err = md_cluster_is_configured(commit, new_db, cluster_map_name, &cl_configured);
	    bail_error(err);

	    err = md_cluster_is_ns_attached(commit, new_db, cluster_map_name, &ns_attached);
	    bail_error(err);

	    if ((ns_attached == false) && (cl_configured == true)) {
		err = md_commit_set_return_status_msg_fmt(commit, EINVAL,
			"Cluster is not added to the namespace");
		bail_error(err);
		goto bail;
	    }
	}
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    tstr_array_free(&t_cluster_maps);
    tstr_array_free(&t_name_parts);
    ts_free(&ret_msg);
    ts_free(&t_cluster_name);
    return(err);
}


static int
md_nvsd_cluster_validate_name(const char *name,
			tstring **ret_msg)
{
    int err = 0;
    int i = 0;
    const char *p = "/\\*:|`\"?.- ";
    int j = 0;
    int k = 0;
    int l = strlen(p);

    if (strlen(name) == 0) {
	if (ret_msg) {
	    err = ts_new_sprintf(ret_msg,
		    "Bad cluster name.");
	    err = lc_err_not_found;
	    goto bail;
	}
    }

    if (strcmp(name, "list") == 0) {
	if (ret_msg) {
	    err = ts_new_sprintf(ret_msg,
		    "Bad cluster name. Use of reserved keyword 'list' is not allowed");
	}
	err = lc_err_not_found;
	goto bail;

    }

    if (name[0] == '_') {
	if (ret_msg) {
	    err = ts_new_sprintf(ret_msg,
		    "Bad cluster name. "
		    "Name cannot start with a leading underscore ('_')");
	}
	err = lc_err_not_found;
	goto bail;
    }
    k = strlen(name);

    for (i = 0; i < k; i++) {
	for (j = 0; j < l; j++) {
	    if (p[j] == name[i]) {
		if (ret_msg)
		    err = ts_new_sprintf(ret_msg,
			    "Bad cluster name. "
			    "Name cannot contain the characters '%s'", p);
		err = lc_err_not_found;
		goto bail;
	    }
	}
    }

bail:
    return err;
}

int
md_nvsd_addr_port_validate(md_commit *commit,
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
    int status=0;
    const bn_attrib *new_value = NULL;
    tstring *t_addr_port = NULL;
    const char *t_addr = NULL;
    const char *t_port = NULL;
    const char *c_addr = NULL;
    int len = 0;
    tstr_array *tokens = NULL;
    uint32 i = 0, ip =0, port=0;
    struct sockaddr_in sa;

    if (change_type != mdct_delete) {
	/* Get the value */
	new_value = bn_attrib_array_get_quick(new_attribs, ba_value);

	if (new_value) {
	    err = bn_attrib_get_tstr(new_value, NULL, bt_string, NULL, &t_addr_port);
	    bail_error(err);

	    if ( ts_length(t_addr_port) < 1 )
		goto bail;

	    err = ts_tokenize_str(ts_str(t_addr_port),
		    ':',
		    '\0',
		    '\0',
		    0,
		    &tokens);
	    bail_error(err);

	    len = tstr_array_length_quick(tokens);

	    if ( len != 2 ) {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("error : Malformed <address>:<port>"));
		bail_error(err);
		goto bail;

	    }
	    t_addr = tstr_array_get_str_quick(tokens, 0);
	    t_port = tstr_array_get_str_quick(tokens, 1);

	    port = atoi(t_port);

	    if (port > 65535){
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			"Port number shouldn't be greater than 65535. \n");
		bail_error(err);
	    }
	    status=inet_pton(AF_INET,t_addr, &(sa.sin_addr));
	    if(status != 1) {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			"IP address is invalid. \n");
		bail_error(err);
	    }
	}
    }
bail:
    ts_free(&t_addr_port);
    tstr_array_free(&tokens);
    return err;

}

static int
md_nvsd_cluster_commit_side_effects( md_commit *commit,
	const mdb_db *old_db,
	mdb_db *inout_new_db,
	mdb_db_change_array *change_list,
	void *arg)
{
    int err = 0, num_changes = 0, i = 0;
    uint32 num_bindings = 0, j = 0;
    const char *cl_name = NULL;
    mdb_db_change *change = NULL;
    bn_binding *binding = NULL, *suff_bn = NULL;
    bn_binding_array *ba = NULL;
    node_name_t cl_suffix_prefix = {0};

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "cluster", "config", "*", "reset")) {
	    tbool enabled = false;
	    /* reset all needed values for cluster */
	    cl_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    bail_null(cl_name);
	    err = mdb_get_node_value_tbool(commit, inout_new_db, ts_str(change->mdc_name), 0, NULL, &enabled);
	    bail_error(err);
	    if (enabled == false) {
		/* don't reset anything if reset is false */
		continue;
	    }
	    lc_log_debug(nkn_log_level, "cluster_name - %s(%d)", cl_name, enabled);
	    /* the value sent here is ignored, (subop =reset)it is sent so that
	       some of the APIs doesn'g fail on this */
	    err = mdb_set_node_str(commit, inout_new_db, bsso_reset, 0,
		    bt_uint32, "0",
		    "/nkn/nvsd/cluster/config/%s/cluster-type", cl_name);
	    bail_error(err);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_reset, 0,
		    bt_uint32, "2",
		    "/nkn/nvsd/cluster/config/%s/cluster-hash", cl_name);
	    bail_error(err);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_reset, 0,
		    bt_uint32, "0",
		    "/nkn/nvsd/cluster/config/%s/request-routing-method", cl_name);
	    bail_error(err);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_reset, 0,
		    bt_string, "255.255.255.255:65535",
		    "/nkn/nvsd/cluster/config/%s/redirect-addr-port", cl_name);
	    bail_error(err);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_reset, 0,
		    bt_bool, "false",
		    "/nkn/nvsd/cluster/config/%s/redirect-local", cl_name);
	    bail_error(err);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_reset, 0,
		    bt_string, "",
		    "/nkn/nvsd/cluster/config/%s/redirect-base-path", cl_name);
	    bail_error(err);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_reset, 0,
		    bt_uint32, "1",
		    "/nkn/nvsd/cluster/config/%s/replicate-mode", cl_name);
	    bail_error(err);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_reset, 0,
		    bt_uint32, "80",
		    "/nkn/nvsd/cluster/config/%s/load-threshold", cl_name);
	    bail_error(err);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_reset, 0,
		    bt_uint32, "2",
		    "/nkn/nvsd/cluster/config/%s/proxy-cluster-hash", cl_name);
	    bail_error(err);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_reset, 0,
		    bt_string, "255.255.255.255:65535",
		    "/nkn/nvsd/cluster/config/%s/proxy-addr-port", cl_name);
	    bail_error(err);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_reset, 0,
		    bt_bool, "false",
		    "/nkn/nvsd/cluster/config/%s/proxy-local", cl_name);
	    bail_error(err);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_reset, 0,
		    bt_string, "",
		    "/nkn/nvsd/cluster/config/%s/proxy-base-path", cl_name);
	    bail_error(err);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_reset, 0,
		    bt_bool, "false",
		    "/nkn/nvsd/cluster/config/%s/configured", cl_name);
	    bail_error(err);

	    snprintf(cl_suffix_prefix, sizeof(cl_suffix_prefix),
		    "/nkn/nvsd/cluster/config/%s/suffix", cl_name);

	    err = mdb_iterate_binding(NULL, inout_new_db, cl_suffix_prefix, 0, &ba);
	    bail_error(err);

	    err = bn_binding_array_length(ba, &num_bindings);
	    bail_error(err);

	    bn_binding_array_dump("CLUSTER", ba, nkn_log_level);
	    lc_log_debug(nkn_log_level, "num-binding =>%d", num_bindings);
	    for (j = 0; j < num_bindings; ++j) {
		err = bn_binding_array_get(ba, j, &suff_bn);
		bail_error(err);

		/* remove all the bindings from cluster */
		err = mdb_set_node_binding(NULL, inout_new_db, bsso_delete, 0, suff_bn);
		bail_error(err);
	    }
	    bn_binding_array_free(&ba);
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "cluster", "config", "*", "cluster-type")
		&& (mdct_modify == change->mdc_change_type)) {
	    uint32 old_cl_type = 0, new_cl_type = 0;
	    tbool old_configd = false, new_configd = false;

	    cl_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    bail_null(cl_name);

	    err = mdb_get_node_binding(commit, old_db, ts_str(change->mdc_name), 0, &binding);
	    bail_error(err);
	    if (binding == NULL) {
		/* this may happen for case of start-up , not a failure */
		continue;
	    }
	    err = bn_binding_get_uint32(binding, ba_value, NULL, &old_cl_type);
	    bail_error(err);
	    bn_binding_free(&binding);

	    err = mdb_get_node_binding(commit, inout_new_db, ts_str(change->mdc_name), 0, &binding);
	    bail_error_null(err, binding);
	    err = bn_binding_get_uint32(binding, ba_value, NULL, &new_cl_type);
	    bail_error(err);
	    bn_binding_free(&binding);

	    /* from old db */
	    err = md_cluster_is_configured(commit, old_db, cl_name, &old_configd);
	    bail_error(err);

	    /* from new db */
	    err = md_cluster_is_configured(commit, inout_new_db, cl_name, &new_configd);
	    bail_error(err);
	    if (old_cl_type == 0 || new_cl_type == 0) {
		/* this may be reset of cluster type */
		continue;
	    } else {
		if ((old_configd == false) && (new_configd == true)) {
		    /* this is to avoid if user enters values for the first time */
		    continue;
		}
		/* there may be a change of cl-type from redirecct <=> proxy */
		if (old_cl_type == 1 && new_cl_type == 2) {
		    err = md_cluster_copy_nodes(commit, old_db, inout_new_db,
			    cl_name, redirect_nodes, proxy_nodes);
		    bail_error(err);
		} else if (old_cl_type == 2 && new_cl_type == 1) {
		    err = md_cluster_copy_nodes(commit, old_db, inout_new_db,
			    cl_name, proxy_nodes, redirect_nodes);
		    bail_error(err);
		} else {
		    /* XXX - what to do */
		    continue;
		}
	    }
	}
    }

bail:
    bn_binding_free(&binding);
    return err;
}

int
md_cluster_copy_nodes(md_commit *commit, const mdb_db *old_db, mdb_db *new_db,
	const char *cl_name, const char *src_node[], const char *dest_node[])
{
    int i = 0, err = 0;
    node_name_t src = {0}, dest = {0};
    tbool value_same = false;
    bn_binding *old_binding = NULL, *new_binding = NULL;

    for (i = 0; (src_node[i] != NULL) && (dest_node[i] != NULL) ; i++) {
	snprintf(src, sizeof(src), src_node[i], cl_name);
	snprintf(dest, sizeof(dest), dest_node[i], cl_name);

	bn_binding_free(&old_binding);
	err = mdb_get_node_binding(commit, old_db, dest, 0, &old_binding);
	bail_error(err);

	bn_binding_free(&new_binding);
	err = mdb_get_node_binding(commit, new_db, dest, 0, &new_binding);
	bail_error(err);

	if ((old_binding != NULL) && (new_binding != NULL)) {
	    bn_binding_dump(nkn_log_level, NULL, old_binding);
	    bn_binding_dump(nkn_log_level, NULL, new_binding);

	    err = bn_binding_compare_attribs(old_binding, new_binding, ba_value, &value_same);
	    bail_error(err);
	    if (value_same == false) {
		lc_log_debug(nkn_log_level, "not copying the value");
		continue;
	    }
	}

	err = mdb_copy_node_binding(commit, old_db, src, new_db, dest);
	bail_error(err);
    }

bail:
    bn_binding_free(&new_binding);
    bn_binding_free(&old_binding);
    return err;
}

int
md_cluster_is_configured(md_commit *commit, const mdb_db* db, const char *cl_name, tbool *cl_configd)
{
    int err = 0;
    node_name_t cl_node = {0};
    tbool cl_configured = false;

    snprintf(cl_node, sizeof(cl_node), "/nkn/nvsd/cluster/config/%s/configured", cl_name);
    err = mdb_get_node_value_tbool(commit, db, cl_node, 0, NULL, &cl_configured);
    bail_error(err);

    *cl_configd = cl_configured;
bail:
    return err;
}

int
md_cluster_is_ns_attached(md_commit *commit, const mdb_db* db, const char *cl_name, tbool *ns_attached)
{
    int err = 0;
    node_name_t cl_ns_name = {0};
    tstring *t_cluster_name = NULL;

    snprintf(cl_ns_name, sizeof(cl_ns_name), "/nkn/nvsd/cluster/config/%s/namespace", cl_name);

    err = mdb_get_node_value_tstr(commit, db, cl_ns_name,
	    0, NULL, &t_cluster_name);
    bail_error(err);

    if ((t_cluster_name == NULL) || (0 == strcmp(ts_str(t_cluster_name), ""))) {
	*ns_attached = false;
    } else {
	*ns_attached = true;
    }

bail:
    ts_free(&t_cluster_name);
    return err;
}

int
md_nvsd_suffix_validate(md_commit *commit,
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

bail:
    return err;
}
