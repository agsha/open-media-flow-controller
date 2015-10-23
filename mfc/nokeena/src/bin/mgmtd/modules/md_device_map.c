/*
 *
 * Filename:  md_device_map.c
 *
 * (C) Copyright 2014 Juniper Networks
 * All rights reserved.
 * Author: Sivakumar Gurumurthy
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
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"
#include "file_utils.h"
#include "tpaths.h"
#include <arpa/inet.h>
#include "jmd_common.h"

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int md_device_map_init(const lc_dso_info *info, void *data);
int md_check_if_node_attached(md_commit *commit, const mdb_db *old_db,
                const mdb_db *new_db,  const mdb_db_change *change,
                const char *check_name, const char *node_pattern, int *error);
/*----------------------------------------------------------------------------
 * LOCAL FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_device_map_ug_rules = NULL;
static int
md_device_map_commit_side_effects (md_commit *commit,
                            const mdb_db *old_db, mdb_db *new_db,
                            mdb_db_change_array *change_list, void *arg);

static int
md_device_map_commit_apply(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg);

static int
md_device_map_commit_check( md_commit *commit,
        const mdb_db *old_db, const mdb_db *new_db,
        mdb_db_change_array *change_list, void *arg);
/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/

int
md_device_map_init (const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating device_map module
     */
    err = mdm_add_module("device_map", /*module_name*/
                        1,      /*module_version*/
                        "/nkn/device_map",     /*root_node_name*/
                        "/nkn/device_map/config",      /*root_config_name*/
                        0,      /*md_mod_flags*/
	                md_device_map_commit_side_effects,     /*side_effects_func*/
                        NULL,   /*side_effects_arg*/
                        md_device_map_commit_check,   /*check_func*/
                        NULL,   /*check_arg*/
                        md_device_map_commit_apply,    /*apply_func*/
                        NULL,   /*apply_arg*/
                        0,      /*commit_order*/
                        0,      /*apply_order*/
                        md_generic_upgrade_downgrade,   /*updown_func*/
                        &md_device_map_ug_rules,   /*updown_arg*/
                        NULL,   /*initial_func*/
                        NULL,   /*initial_arg*/
                        NULL,   /*mon_get_func*/
                        NULL,   /*mon_get_arg*/
                        NULL,   /*mon_iterate_func*/
                        NULL,   /*mon_iterate_arg*/
                        &module);       /*ret_module*/
    bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name =               "/nkn/device_map/config/*";
	node->mrn_value_type =         bt_string;
	node->mrn_limit_wc_count_max = NKN_MAX_DEVICE_MAPS;
	node->mrn_limit_str_max_chars = MAX_DEVICE_MAP_NAMELEN;
	node->mrn_limit_str_charlist =  CLIST_ALPHANUM "_";
	node->mrn_bad_value_msg =      "Invalid name/maximum device-maps configured";
	node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
	node->mrn_cap_mask =           mcf_cap_node_basic;
	node->mrn_description =        "Device-Map";
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name              =       "/nkn/device_map/config/*/device_info/fqdn";
	node->mrn_value_type        =       bt_string;
	node->mrn_limit_str_max_chars =     NKN_MAX_FQDN_LENGTH;
	node->mrn_node_reg_flags    =       mrf_flags_reg_config_literal;
	node->mrn_initial_value		=   "";
	node->mrn_cap_mask          =       mcf_cap_node_basic;
	node->mrn_description       =       "Ipaddress or fqdn of this device";
	node->mrn_check_node_func   =       md_rfc952_domain_validate;
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name              =       "/nkn/device_map/config/*/device_info/username";
	node->mrn_value_type        =       bt_string;
	node->mrn_limit_str_max_chars =     32;
	node->mrn_node_reg_flags    =       mrf_flags_reg_config_literal;
	node->mrn_initial_value		=   "";
	node->mrn_cap_mask          =       mcf_cap_node_basic;
	node->mrn_description       =       "Username of this device";
	err = mdm_add_node(module, &node);
	bail_error(err);
	
	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name              =       "/nkn/device_map/config/*/device_info/password";
	node->mrn_value_type        =       bt_string;
	node->mrn_node_reg_flags    =       mrf_flags_reg_config_literal | mrf_flags_reg_config_literal_secure;
	node->mrn_initial_value		=   "";
	node->mrn_cap_mask          =       mcf_cap_node_restricted;
	node->mrn_description       =       "Password of this device";
	err = mdm_add_node(module, &node);
	bail_error(err);
	
	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name              =       "/nkn/device_map/config/*/filter_config/frequency";
	node->mrn_value_type        =       bt_uint32;
	node->mrn_node_reg_flags    =       mrf_flags_reg_config_literal;
	node->mrn_initial_value		=   "120";
	node->mrn_limit_num_min_int =       1;
    	node->mrn_limit_num_max_int     =   20160; //2 weeks specified in minutes
	node->mrn_cap_mask          =       mcf_cap_node_restricted;
	node->mrn_description       =       "Frequency in minutes in which to fire the policy to the router";
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name              =       "/nkn/device_map/config/*/filter_config/max_rules";
	node->mrn_value_type        =       bt_uint32;
	node->mrn_node_reg_flags    =       mrf_flags_reg_config_literal;
	node->mrn_initial_value		=   "10000";
	node->mrn_limit_num_min_int =       1;
    	node->mrn_limit_num_max_int     =   25000;
	node->mrn_cap_mask          =       mcf_cap_node_restricted;
	node->mrn_description       =       "Max number of unique policy rules that can be collected before it is fired to the router";
	err = mdm_add_node(module, &node);
	bail_error(err);
	
	err = md_upgrade_rule_array_new(&md_device_map_ug_rules);
        bail_error(err);
        ra = md_device_map_ug_rules;
bail:
    return(err);
}

int
md_check_if_node_attached(md_commit *commit, const mdb_db *old_db,
		const mdb_db *new_db,  const mdb_db_change *change,
		const char *check_name, const char *node_pattern, int *error)
{
    int err = 0, num = 0, i = 0;
    tstr_array *smap_nodes = NULL;
    tstring  * t_smap_name = NULL;
    const char *smap_node = node_pattern, *node = NULL;

    if (check_name == NULL || (change == NULL)) {
	md_commit_set_return_status_msg_fmt(commit, 1, "Invalid input ");
	*error = 1;
	goto bail;
    }

    err = mdb_get_matching_tstr_array(commit, new_db, smap_node,
	    0, &smap_nodes);
    bail_error(err);

    if (smap_nodes == NULL) {
	*error = 0;
	goto bail;
    }

    num = tstr_array_length_quick(smap_nodes);

    *error = 0;
    for (i = 0; i < num; i++) {
	node = tstr_array_get_str_quick(smap_nodes, i);

	ts_free(&t_smap_name);
	err = mdb_get_node_value_tstr(commit, new_db, node, 0,
		NULL, &t_smap_name);
	bail_error(err);

	if (0 == strcmp(ts_str_maybe_empty(t_smap_name), "")) {
	    /* ignore if NULL */
	    continue;
	}
	if (0 == strcmp(ts_str_maybe_empty(t_smap_name), check_name)) {
	    /* there an instance of this, are checking ourselves */
	    if (0 == strcmp(node,ts_str(change->mdc_name))) {
		continue;
	    }
	    *error = 1;
	    goto bail;
	}
    }

bail:
    tstr_array_free(&smap_nodes);
    ts_free(&t_smap_name);
    return err;
}

/*----------------------------------------------------------------------------
 * LOCAL FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
static int md_device_map_commit_apply (md_commit *commit,
                      const mdb_db *old_db, const mdb_db *new_db,
                      mdb_db_change_array *change_list, void *arg)
{
	int     err = 0;
	uint32  num_changes = 0, i = 0;
	mdb_db_change *change = NULL;
	const char *device_map_name = NULL;
        tstring *t_value = NULL;
	uint32 t_frequency = 0;
	tbool node_found;
	int attached = 0;

	num_changes = mdb_db_change_array_length_quick(change_list);
	for(i = 0; i < num_changes; i++) {
		change = mdb_db_change_array_get_quick(change_list, i);
		bail_null(change);
		//Get the device-map name
		if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 4, "nkn", "device_map", "config", "**") && (mdct_delete != change->mdc_change_type)) {
			device_map_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);
			//Check if it is attached to the log-analyzer
			err = md_check_if_node_attached(commit, old_db, new_db, change, device_map_name, "/nkn/log_analyzer/config/device_map/*", &attached);
			bail_error(err);
		}
		if (attached && (mdct_delete != change->mdc_change_type)) {
		//Throw the message to restart log-analyzer for change to take effect
			if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
			    0, 6, "nkn", "device_map", "config", "*", "device_info", "fqdn") ||
			bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
			    0, 6, "nkn", "device_map", "config", "*", "device_info", "username") ||
			bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
			    0, 6, "nkn", "device_map", "config", "*", "device_info", "password"))) {
				lc_log_basic(LOG_NOTICE, "The MDC_NAME IS %s",ts_str(change->mdc_name));
			   	//Check the specified node 
				err = mdb_get_node_value_tstr(commit, new_db, ts_str(change->mdc_name), 0, &node_found, &t_value);
				bail_error(err);
				//If the value is not equal to "" then throw the error
				if (t_value != NULL && strcmp(ts_str(t_value), "")) {
					err = md_commit_set_return_status_msg_fmt(commit, 0,
					    "IMPOTANT NOTE: This device-map is attached to the log-analyzer. Please restart log-analyzer tool for the change to take effect");
					bail_error(err);
				}
			}
			//Get the value of the frequency node
			if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
			    0, 6, "nkn", "device_map", "config", "*", "filter_config", "frequency") && (mdct_delete != change->mdc_change_type)) {
				err = mdb_get_node_value_uint32(commit, new_db, ts_str(change->mdc_name), 0, NULL, &t_frequency);
				bail_error(err);
				//lc_log_basic(LOG_NOTICE, "The t_frequency IS %d", t_frequency);
				if (t_frequency != 120) {
				    err = md_commit_set_return_status_msg_fmt(commit, 0,
					    "IMPOTANT NOTE: This device-map is attached to the log-analyzer. Please restart log-analyzer tool for the change to take effect");
				    bail_error(err);
				}
			}
		}
	}
bail:
	ts_free(&t_value);
	return(err);
}

static int
md_device_map_commit_side_effects (md_commit *commit,
                            const mdb_db *old_db, mdb_db *new_db,
                            mdb_db_change_array *change_list, void *arg)
{
    tbool initial = false;
    int     err = 0;
    uint32  num_changes = 0, i = 0;
    mdb_db_change *change = NULL;
    const char *local_as = NULL, *prefix_str = NULL, *mask_str = NULL;
    node_name_t network_nd = {0};

    err = md_commit_get_commit_initial(commit, &initial);
    bail_error(err);

    /* if mgmtd just restarted */
    if (initial) {
        return 0;
    }

bail:
    return(err);
}

static int
md_device_map_commit_check( md_commit *commit,
        const mdb_db *old_db, const mdb_db *new_db,
        mdb_db_change_array *change_list, void *arg)
{
	const char *device_map_name = NULL;
	int err = 0;
	int32 num_changes = 0;
        mdb_db_change *change = NULL;
        int i = 0, attached = 0;
	num_changes = mdb_db_change_array_length_quick(change_list);
        for(i = 0; i < num_changes; i++) {
                change = mdb_db_change_array_get_quick(change_list, i);
		bail_null(change);
		//Should not be able to delete the device-map if it is bound to the log-analyzer
		if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
                            0, 4, "nkn", "device_map", "config", "*") &&
				(mdct_delete == change->mdc_change_type)) {
			//Get the device-map name
			device_map_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);
			//Check if it is attached to the log-analyzer
			err = md_check_if_node_attached(commit, old_db, new_db, change, device_map_name, "/nkn/log_analyzer/config/device_map/*", &attached);
			bail_error(err);
			//Throw an error if it is attached
			if (attached) {
			    err = md_commit_set_return_status_msg_fmt(commit, 1,
				    "device-map %s is attached to the log-analyzer. Please unbind it and then delete", device_map_name);
			    bail_error(err);
			    goto bail;
			}
		}
	}
bail:
    return err;
}
