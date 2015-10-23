/*
 *
 * Filename:  md_l4proxy.c
 * Date:      2008/11/14
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
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
#include "nkn_mgmt_defs.h"



/* ------------------------------------------------------------------------- */

int md_l4proxy_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_l4proxy_ug_rules = NULL;

int
md_addr_validate(md_commit *commit,
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
md_l4proxy_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("l4proxy", 1,
                         "/nkn/l4proxy", "/nkn/l4proxy/config",
			 0 ,
                         NULL, NULL,
                         NULL, NULL,
                         NULL, NULL,
                         0, 0,
                         md_generic_upgrade_downgrade,
			 &md_l4proxy_ug_rules,
                         NULL, NULL,	/* inital comit function and args */
                         NULL, NULL,	/* get_monitoring function and args (ignored ) */
			 NULL, NULL,	/* mon. iterate function function and args */
			 &module);
    bail_error(err);

    /* controll enable/disable of l4proxy */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/l4proxy/config/enable";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false"; /* has to be true by default, setting it to false for testing. */
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "l4proxy staus";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* control enable/disable of tunnel-list */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/l4proxy/config/tunnel-list/enable";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "l4proxy staus";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* control enable/disable of l4proxy cache-fail
     * 0 = tunnel;
     * 1 = reject;
     * anything else reserved for future use
     */

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/l4proxy/config/cache-fail/action";
    /* NOTE - using u32 here for two values here. I believe
     * there could be more values for this in future. so bool will not scale
     */
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "0";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "l4proxy staus";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* XXX- following nodes have been added for future relse. */
    /* control  enable/disable of l4proxy white list */
    err = mdm_new_node(module, &node);
    bail_error(err);
    /* wlist is whitelist */
    node->mrn_name              = "/nkn/l4proxy/config/wlist/enable";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "l4proxy while list staus";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* threshold value for white list addresses */
    err = mdm_new_node(module, &node);
    bail_error(err);
    /* wlist is whitelist */
    node->mrn_name              = "/nkn/l4proxy/config/wlist/threshold";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "4096";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "l4proxy while list staus";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* ip addresses maintained by l4 proxy module (in whilelist) */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/l4proxy/config/wlist/ip/*";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_check_node_func 	= md_addr_validate;
    /* argument to distuinguish with blacklist */
    node->mrn_check_node_arg	= (void *)"1";
    node->mrn_description       = "l4proxy while list staus";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* control  enable/disable of l4proxy black list */
    err = mdm_new_node(module, &node);
    bail_error(err);
    /* blist is blacklist */
    node->mrn_name              = "/nkn/l4proxy/config/blist/enable";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "l4proxy black list staus";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* ip addresses maintained by l4 proxy module (in blacklist) */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/l4proxy/config/blist/ip/*";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_check_node_func 	= md_addr_validate;
    /* argument to distuinguish with whitelist */
    node->mrn_check_node_arg	= (void *)"0";
    node->mrn_description       = "l4proxy black list status";
    err = mdm_add_node(module, &node);
    bail_error(err);


bail:
    return(err);
}


int
md_addr_validate(md_commit *commit,
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
    const bn_attrib *ip_address = NULL;
    node_name_t ip_node_name = {0};
    tstring *t_ip_address = NULL, *ip_addr_other_list = NULL;
    //uint32 ip_addr = 0;
    tbool found = false;
    /* arg == 1, then whitelist. if arg == 0; then blacklist */
    const char *cb_arg = arg;
    uint32 white_list;

    lc_log_debug(LOG_NOTICE, "checking value %d", change_type);
    if (change_type == mdct_modify || change_type == mdct_add) {
	/* checking in case there is a modification (this is not be happening)
	 * or in case there is a addition of new ip address */
	ip_address = bn_attrib_array_get_quick(new_attribs, ba_value);
	bail_null(ip_address);
	if (ip_address) {
	    /* t_ip_address is allocated memory */
	    err = bn_attrib_get_tstr(ip_address, NULL, bt_string, NULL, &t_ip_address);
	    bail_error_null(err, t_ip_address);

	    /* check if ip address is valid or not */
	    //err = lc_str_to_ipv4addr(ts_str(t_ip_address), &ip_addr);
	    // Porting to FIR - use new API to check for IPv4
	    if (!lia_str_is_ipv4addr_quick(ts_str(t_ip_address))) {
		err = md_commit_set_return_status_msg(commit, EINVAL, "Invalid IP address");
		bail_error(err);
		goto bail;
	    }
	    if (cb_arg == NULL) {
		err = md_commit_set_return_status_msg(commit, EINVAL, "Internal error occured");
		bail_error(err);
		goto bail;
	    }
	    white_list = atoi(cb_arg);
	    /* check if this is already exists in other list */
	    if (white_list == 0) {
		/* this is blacklist callback, so look into white-list */
		snprintf(ip_node_name, sizeof(ip_node_name), "/nkn/l4proxy/config/wlist/ip/%s", ts_str(t_ip_address));
	    } else {
		/* this is whitelist callback, so look into black-list */
		snprintf(ip_node_name, sizeof(ip_node_name), "/nkn/l4proxy/config/blist/ip/%s", ts_str(t_ip_address));
	    }
	    lc_log_debug(LOG_NOTICE, "node- %s, white_list=> %d", ip_node_name, white_list);
            err = mdb_get_node_value_tstr(commit, new_db, ip_node_name, 0, &found, &ip_addr_other_list);
            bail_error(err);

	    /* we don't need this, so free this */
	    ts_free(&ip_addr_other_list);

	    if (found == true) {
		/* node exists in other list */
		err = md_commit_set_return_status_msg_fmt(commit, EINVAL ,"IP Address exists in %s",
			white_list ? "black-list" : "white-list" );
		bail_error(err);
		goto bail;
	    }
	} else {
	    /* this must not have hapened */
	    err = md_commit_set_return_status_msg(commit, EINVAL ,"Invalid IP address");
	    bail_error(err);
	    goto bail;
	}
    } else {
	/* no need to check for delete or other operations */
	err = 0;
	goto bail;
    }

bail:
    ts_free(&t_ip_address);
    return err;
}


