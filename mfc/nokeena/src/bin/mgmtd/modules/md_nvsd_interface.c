/*
 *
 * Filename:  md_nvsd_interface_interface.c
 * Date:      2009/03/19
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-09 Nokeena Networks, Inc.
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
#include "tpaths.h"
#include "nkn_mgmt_defs.h"


/* ------------------------------------------------------------------------- */

int md_nvsd_interface_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_nvsd_interface_ug_rules = NULL;

static int md_nvsd_interface_commit_apply(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

static int
md_nvsd_interface_commit_check(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg);
static int
md_interface_identify(md_commit *commit, const mdb_db *db,
                    const char *action_name,
                    bn_binding_array *params, void *cb_arg);

/* ------------------------------------------------------------------------- */
int
md_nvsd_interface_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd-interface", 10,
	    "/nkn/nvsd/interface", "/net/interface/config",
	    modrf_all_changes,
	    NULL, NULL,
	    md_nvsd_interface_commit_check, NULL,
	    md_nvsd_interface_commit_apply, NULL,
	    0, 0,
	    NULL,
	    &md_nvsd_interface_ug_rules,
	    NULL, NULL,
	    NULL, NULL, NULL, NULL,
	    &module);
    bail_error(err);

    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/interface/actions/blink-interface";
    node->mrn_node_reg_flags =     mrf_flags_reg_action;
    node->mrn_cap_mask =           mcf_cap_action_basic;
    node->mrn_action_func =        md_interface_identify;
    node->mrn_actions[0]->mra_param_name = "interface";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "time";
    node->mrn_actions[1]->mra_param_type = bt_uint32;
    err = mdm_add_node(module, &node);
    bail_error(err);



bail:
    return(err);
}

static int
md_nvsd_interface_commit_check(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    int i, num_changes = 0;
    const mdb_db_change *change = NULL;
    node_name_t nvsd_ip_nd = {0};
    node_name_t ip_nd = {0};
    const char *c_ifname = NULL;
    tbool ret_found = false;
    tstring *ip_val = NULL;
    tstr_array *nvsd_ip_map = NULL;
    const bn_attrib *old_val = NULL;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

	/* Check if it is IP change */
	if ((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 9,
			"net", "interface", "config", "*", "addr", "ipv4", "static", "1", "ip"))
		&& ( (mdct_delete == change->mdc_change_type) ||
		    (mdct_modify == change->mdc_change_type)))
	{
	    c_ifname = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    bail_null(c_ifname);

	    old_val = bn_attrib_array_get_quick(change->mdc_old_attribs,
		    ba_value);
	    bail_error(err);

	    if(!old_val)
		continue;

	    err = bn_attrib_get_tstr(old_val, NULL, bt_ipv4addr, NULL,
		    &ip_val);
	    bail_error_null(err, ip_val);

	    snprintf(nvsd_ip_nd, sizeof(nvsd_ip_nd),
		    "/nkn/nvsd/namespace/*/vip/%s", ts_str(ip_val));

	    err = mdb_get_matching_tstr_array(commit, new_db,
		    nvsd_ip_nd,
		    0, &nvsd_ip_map);
	    bail_error_null(err, nvsd_ip_map);

	    if(tstr_array_length_quick(nvsd_ip_map)) {
		err = md_commit_set_return_status_msg_fmt(
			commit, 1,
			"IP address is bound to a"
			" namespace.Please unbind it.");
		bail_error(err);

	    }

	    ts_free(&ip_val);
	    tstr_array_free(&nvsd_ip_map);
	}
    }

bail:
    ts_free(&ip_val);
    tstr_array_free(&nvsd_ip_map);
    return err;
}

/* ------------------------------------------------------------------------- */
static int
md_nvsd_interface_commit_apply(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    int i, num_changes = 0;
    const mdb_db_change *change = NULL;
    const char *c_ifname = NULL;


    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

	/* Check if it is IP change */
	if ((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 4, "net", "interface", "config", "*"))
		&& (mdct_add == change->mdc_change_type))
	{
	    err = md_commit_set_return_status_msg(commit, 0,
		    "Information:\n\t If media delivery service has to use this configuration change then\n\tplease restart mod-delivery using the 'service restart' command");
	}
	else if ((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 9, "net", "interface", "config", "*", "addr", "ipv6", "static", "*", "ip")))
	{
	    err = md_commit_set_return_status_msg(commit, 0,
		    "Information:\n If media delivery service has to use this configuration change then\n please restart mod-delivery using the 'service restart' command");
	}
	else if (((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 7, "net", "interface", "config", "*", "addr", "ipv4", "dhcp")) ||
		    (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 9, "net", "interface", "config", "*", "addr", "ipv4", "static", "*", "ip")))
		&& (mdct_modify == change->mdc_change_type))
	{
	    err = md_commit_set_return_status_msg(commit, 0,
		    "Information:\n If media delivery service has to use this configuration change then\n please restart mod-delivery using the 'service restart' command");
	}
	else if (((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 7, "net", "interface", "config", "*", "type", "ethernet", "duplex"))
		    && (mdct_modify == change->mdc_change_type)))
	{
	    uint32 idx = 0;
	    int32 status = 0;
	    c_ifname = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    bail_null(c_ifname);
	    err = lc_launch_quick_status(&status,
		    NULL, false, 2, "/opt/nkn/bin/set_interface_txqueuelen.sh", (c_ifname));
	    bail_error(err);
	}
	else if (((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 7, "net", "interface", "config", "*", "type", "ethernet", "speed"))
		    && (mdct_modify == change->mdc_change_type)))
	{
	    uint32 idx = 0;
	    int32 status = 0;
	    c_ifname = tstr_array_get_str_quick(change->mdc_name_parts, 3);
	    bail_null(c_ifname);
	    err = lc_launch_quick_status(&status,
		    NULL, false, 2, "/opt/nkn/bin/set_interface_txqueuelen.sh", (c_ifname));
	    bail_error(err);
	}
	else if (((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 5, "net", "interface", "config", "*", "enable"))
		    && (mdct_modify == change->mdc_change_type)))
	{
	    err = md_commit_set_return_status_msg(commit, 0,
		    "Information:\n\t If media delivery service has to use this configuration change then\n\tplease restart mod-delivery using the 'service restart' command");

	}
    }
bail:
    return(err);
}


static int
md_interface_identify(md_commit *commit, const mdb_db *db,
                    const char *action_name,
                    bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    tstring *t_interface = NULL;
    tstring *t_devsource = NULL;
    uint32 time_interval = 0;
    const bn_binding *binding = NULL;
    char *node_name = NULL;
    tbool found = false;
    int32 ret_status = 0;
    char *iden_time = NULL;

    bail_null(params);

    err = bn_binding_array_get_value_tstr_by_name(params, "interface", NULL,
	    &t_interface);
    bail_error(err);
    if(t_interface && ts_num_chars(t_interface) > 0) {
	lc_log_basic(LOG_DEBUG, "Interface is %s \n",
		ts_str(t_interface));
	node_name = smprintf( "/net/interface/state/%s/devsource", ts_str(t_interface));

	err = mdb_get_node_value_tstr(NULL, db, node_name, 0, &found,
		&t_devsource);
	if( t_devsource != NULL) {
	    if(ts_equal_str(t_devsource, "physical", false)) {
		lc_log_basic(LOG_DEBUG, "Devsource is %s \n",
			ts_str(t_devsource));
		err = bn_binding_array_get_binding_by_name(params, "time",
			&binding);
		bail_error(err);
		if(binding) {
		    err = bn_binding_get_uint32(binding, ba_value, NULL,
			    &time_interval);
		    bail_error(err);
		    lc_log_basic(LOG_DEBUG, "Blink interval is %u \n",
			    time_interval);
		}
		iden_time = smprintf("%u", time_interval);
		err = lc_launch_quick_status(&ret_status, NULL, true, 4, "/sbin/ethtool", "-p", ts_str(t_interface), iden_time);
		if(ret_status) {
		    err = md_commit_set_return_status_msg(commit, 0,
			    "error invoking the ethtool\n");
		    goto bail;

		}
		bail_error(err);


	    } else {
		err = md_commit_set_return_status_msg(commit, 0,
			"The interface is not a physical interface\n");
		bail_error(err);
		goto bail;
	    }
	} else {
	    err = md_commit_set_return_status_msg(commit, 0,
		    "The interface is not a physical interface\n");
	    bail_error(err);

	    goto bail;
	}

    }
bail:
    safe_free(iden_time);
    safe_free(node_name);
    ts_free(&t_interface);
    ts_free(&t_devsource);
    return err;
}
