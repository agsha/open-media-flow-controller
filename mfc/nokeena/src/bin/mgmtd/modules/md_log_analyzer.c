/*
 *
 * Filename:  md_log_analyzer.c
 *
 * (C) Copyright 2011 Juniper Networks
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
#include "nkn_defs.h"
#include "file_utils.h"
#include "tpaths.h"
#include <arpa/inet.h>

#define REMOVE_PBR_PATH  /opt/dpi-analyzer/scripts/clear_filter_rules

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int md_log_analyzer_init(const lc_dso_info *info, void *data);
int md_check_if_node_exists(md_commit *commit, const mdb_db *old_db,
                const mdb_db *new_db, const char *check_name,
                const char *node_root, int *exists);
/*----------------------------------------------------------------------------
 * LOCAL FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_log_analyzer_ug_rules = NULL;
static int md_log_analyzer_clear_filter_rules (md_commit *commit, mdb_db *db,
                const char *action_name, bn_binding_array *params,
                void *cb_arg);
static int
md_log_analyzer_commit_side_effects (md_commit *commit,
                            const mdb_db *old_db, mdb_db *new_db,
                            mdb_db_change_array *change_list, void *arg);

static int
md_log_analyzer_commit_apply(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg);

static int
md_log_analyzer_commit_check( md_commit *commit,
        const mdb_db *old_db, const mdb_db *new_db,
        mdb_db_change_array *change_list, void *arg);

static int
device_map_validation(md_commit *commit, const mdb_db *new_db, const char *device_map_name);
/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/

int
md_log_analyzer_init (const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating log-analyzer module
     */
    err = mdm_add_module("log_analyzer", /*module_name*/
                        1,      /*module_version*/
                        "/nkn/log_analyzer",     /*root_node_name*/
                        "/nkn/log_analyzer/config",      /*root_config_name*/
                        0,      /*md_mod_flags*/
	                md_log_analyzer_commit_side_effects,     /*side_effects_func*/
                        NULL,   /*side_effects_arg*/
                        md_log_analyzer_commit_check,   /*check_func*/
                        NULL,   /*check_arg*/
                        md_log_analyzer_commit_apply,    /*apply_func*/
                        NULL,   /*apply_arg*/
                        0,      /*commit_order*/
                        0,      /*apply_order*/
                        md_generic_upgrade_downgrade,   /*updown_func*/
                        &md_log_analyzer_ug_rules,   /*updown_arg*/
                        NULL,   /*initial_func*/
                        NULL,   /*initial_arg*/
                        NULL,   /*mon_get_func*/
                        NULL,   /*mon_get_arg*/
                        NULL,   /*mon_iterate_func*/
                        NULL,   /*mon_iterate_arg*/
                        &module);       /*ret_module*/
	bail_error(err);

	err = mdm_new_action(module, &node, 1);
	bail_error(err);
	node->mrn_name                  = "/nkn/log_analyzer/actions/clear_filter_rules";
	node->mrn_node_reg_flags        = mrf_flags_reg_action;
	node->mrn_cap_mask              = mcf_cap_action_privileged;
	node->mrn_action_async_start_func  = md_log_analyzer_clear_filter_rules;
	node->mrn_description           = "action node for clearing filter rules";
	node->mrn_actions[0]->mra_param_name = "device_map_name";
	node->mrn_actions[0]->mra_param_type = bt_string;
	node->mrn_actions[0]->mra_param_required = true;
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name =            "/nkn/log_analyzer/config/device_map/*";
	node->mrn_value_type =      bt_string;
	node->mrn_node_reg_flags =  mrf_flags_reg_config_wildcard;
	node->mrn_cap_mask =        mcf_cap_node_restricted;
	node->mrn_description =     "Device map associated with this log-analyzer";
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name              =       "/nkn/log_analyzer/config/device_map/*/command_mode/inline";
	node->mrn_value_type        =       bt_bool;
	node->mrn_initial_value	    =       "true";
	node->mrn_node_reg_flags    =       mrf_flags_reg_config_literal;
	node->mrn_cap_mask          =       mcf_cap_node_basic;
	node->mrn_description       =       "Keyword directive on how filter commands be applied to device";
	err = mdm_add_node(module, &node);
	bail_error(err);

	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name =            "/nkn/log_analyzer/config/config_file/url";
	node->mrn_value_type =      bt_string;
	node->mrn_node_reg_flags =  mrf_flags_reg_config_literal;
	node->mrn_initial_value	    =  "local";
	node->mrn_cap_mask =        mcf_cap_node_basic;
	node->mrn_description =     "HTTP or SCP url for fetching config-file from an external server";
	err = mdm_add_node(module, &node);
	bail_error(err);
	
	err = mdm_new_node(module, &node);
	bail_error(err);
	node->mrn_name			=	"/nkn/log_analyzer/config/config_file/all-ns";
	node->mrn_value_type		=	bt_bool;
	node->mrn_initial_value		=       "false";
	node->mrn_node_reg_flags	=	mrf_flags_reg_config_literal;
	node->mrn_cap_mask		=        mcf_cap_node_restricted;
	node->mrn_description		=	"Keyword to specify that catch-all T-Proxy namespace be included in log analysis";
	err = mdm_add_node(module, &node);
	bail_error(err);
	
	err = md_upgrade_rule_array_new(&md_log_analyzer_ug_rules);
	bail_error(err);
	ra = md_log_analyzer_ug_rules;

bail:
	return(err);
}

int
md_check_if_node_exists(md_commit *commit, const mdb_db *old_db,
		const mdb_db *new_db, const char *check_name,
		const char *node_root, int *exists)
{
    int err = 0;
    tbool found = false;
    tstring *t_name = NULL;
    node_name_t node_name = {0};

    *exists = 0;

    if (check_name == NULL ) {
	md_commit_set_return_status_msg_fmt(commit, 1, "Invalid input ");
	goto bail;
    }

    if (0 == strcmp(check_name, "")) {
	goto bail;
    }
    snprintf(node_name, sizeof(node_name), "%s/%s", node_root, check_name);

    ts_free(&t_name);
    err = mdb_get_node_value_tstr(commit, new_db, node_name, 0,
	    &found , &t_name);
    bail_error(err);

    if (found) {
	*exists = 1;
    }

bail:
    return err;
}

/*----------------------------------------------------------------------------
 * LOCAL FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
static int md_log_analyzer_clear_filter_rules(md_commit *commit, mdb_db *db,
                const char *action_name, bn_binding_array *params,
                void *cb_arg)
{
	int err = 0, exists = 0, vaildation_failed = 0;
	char *name = NULL;
	const bn_binding *binding = NULL;
	lc_launch_params *lp = NULL;
	lc_launch_result *lr = NULL;

	err = bn_binding_array_get_binding_by_name(params, "device_map_name", &binding);
	bail_error_null(err, binding);

	err = bn_binding_get_str(binding, ba_value, bt_string, 0, &name);
	bail_error_null(err, name);

	//Check if the device-map name specified exists
	err = md_check_if_node_exists(commit, db, db, name,
	"/nkn/device_map/config", &exists);
	bail_error(err);
	if (exists == 0 ) {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
		    "The device-map %s doesn't exist", name);
		bail_error(err);
		goto bail;
	}
	//Validate if the device-map attached has all the required fields configured
	vaildation_failed = device_map_validation(commit, db, name);
	if (vaildation_failed)
		goto bail;
	err = lc_launch_params_get_defaults(&lp);
	bail_error(err);

	err = ts_new_str(&(lp->lp_path), "/opt/dpi-analyzer/scripts/clear_filter_rules");
	bail_error(err);
	err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 2,"/opt/dpi-analyzer/scripts/clear_filter_rules", name);
	bail_error(err);;

	lp->lp_wait = false;
	lp->lp_env_opts = eo_preserve_all;
	lp->lp_log_level = LOG_NOTICE;

	lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
	lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

	lr = malloc(sizeof(lc_launch_result));
	bail_null(lr);

	memset(lr, 0, sizeof(lc_launch_result));

	//Launch the shell script which will in turn call the python script that clears the filter rules from the router
	err = lc_launch(lp,lr);
	bail_error(err);
	err = md_commit_set_return_status_msg_fmt(commit, 1, "Please look into /var/log/messages to check if this command has succeeded or not. After checking the status please restart the log-analyzer tool for the changes to take effect");

bail:
	safe_free(name);
	return(err);
}

static int
md_log_analyzer_commit_apply (md_commit *commit,
                      const mdb_db *old_db, const mdb_db *new_db,
                      mdb_db_change_array *change_list, void *arg)
{
	int     err = 0;
        tstring *t_newvalue = NULL;
        tstring *t_oldvalue = NULL;
	uint32  num_changes = 0, i = 0;
	mdb_db_change *change = NULL;

	num_changes = mdb_db_change_array_length_quick(change_list);
	for(i = 0; i < num_changes; i++) {
		change = mdb_db_change_array_get_quick(change_list, i);
		bail_null(change);
		if (mdct_delete == change->mdc_change_type) {
			err = md_commit_set_return_status_msg_fmt(commit, 0, "IMPOTANT NOTE:Please restart the log-analyzer tool for the change to take effect");
			bail_error(err);
		}
		else {
			if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
			    0, 7, "nkn", "log_analyzer", "config", "device_map", "*", "command_mode", "inline")) ||
				(bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
                            0, 5, "nkn", "log_analyzer", "config", "config_file", "url")))
			{
				err = mdb_get_node_value_tstr(commit, new_db, ts_str(change->mdc_name), 0, NULL, &t_newvalue);
				err = mdb_get_node_value_tstr(commit, old_db, ts_str(change->mdc_name), 0, NULL, &t_oldvalue);
				if (t_oldvalue && strcmp(ts_str(t_newvalue), ts_str(t_oldvalue))) {
					err = md_commit_set_return_status_msg_fmt(commit, 0, "IMPOTANT NOTE:Please restart the log-analyzer tool for the change to take effect");
					bail_error(err);
				}
			}
		}
	}
bail:
	ts_free(&t_newvalue);
	ts_free(&t_oldvalue);
    return(err);
}

static int
md_log_analyzer_commit_side_effects (md_commit *commit,
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

/* Local function that validates if the specified device-map has all the mandatory fields configured
 */
static int
device_map_validation(md_commit *commit,const mdb_db *new_db, const char *device_map_name)
{
	int err = 0, vaildation_failed = 0;
	node_name_t fqdn_node_name = {0};
	node_name_t username_node_name = {0};
	node_name_t password_node_name = {0};
	tstring *t_fqdn = NULL;
	tstring *t_username = NULL;
	tstring *t_password = NULL;
	
	//Check if the fqdn is specifed for the device-map
	snprintf(fqdn_node_name, sizeof(fqdn_node_name), "/nkn/device_map/config/%s/device_info/fqdn", device_map_name);
	err = mdb_get_node_value_tstr(commit, new_db, fqdn_node_name, 0, NULL, &t_fqdn);
	bail_error(err);
	if ((t_fqdn == NULL) || (0 == strcmp(ts_str(t_fqdn), ""))) {
		err = md_commit_set_return_status_msg_fmt(commit,1, "The fqdn for the device_map \"%s\" has to be specified", device_map_name);
		bail_error(err);
		vaildation_failed =1;
		goto bail;
	}
	//Check if the username is specifed for the device-map
	snprintf(username_node_name, sizeof(username_node_name), "/nkn/device_map/config/%s/device_info/username", device_map_name);
	err = mdb_get_node_value_tstr(commit, new_db, username_node_name, 0, NULL, &t_username);
	bail_error(err);
	if ((t_username == NULL) || (0 == strcmp(ts_str(t_username), ""))) {
		err = md_commit_set_return_status_msg_fmt(commit,1, "The username for this device_map has to be specified");
		bail_error(err);
		vaildation_failed =1;
		goto bail;
	}
	//Check if the password is specifed for the device-map
	snprintf(password_node_name, sizeof(password_node_name), "/nkn/device_map/config/%s/device_info/password", device_map_name);
	err = mdb_get_node_value_tstr(commit, new_db, password_node_name, 0, NULL, &t_password);
	bail_error(err);
	if ((t_password == NULL) || (0 == strcmp(ts_str(t_password), ""))) {
		err = md_commit_set_return_status_msg_fmt(commit,1, "The password for this device_map has to be specified");
		bail_error(err);
		vaildation_failed =1;
		goto bail;
	}
bail:
	ts_free(&t_fqdn);
	ts_free(&t_username);
	ts_free(&t_password);
	return vaildation_failed;
}

static int
md_log_analyzer_commit_check( md_commit *commit,
        const mdb_db *old_db, const mdb_db *new_db,
        mdb_db_change_array *change_list, void *arg)
{
	int32 num_changes = 0;
	mdb_db_change *change = NULL;
	int i = 0, exists= 0;
	int err = 0;
    	const char *device_map_name = NULL;

	num_changes = mdb_db_change_array_length_quick(change_list);
	for(i = 0; i < num_changes; i++) {
		change = mdb_db_change_array_get_quick(change_list, i);
		bail_null(change);
		if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
			    0, 5, "nkn", "log_analyzer", "config", "device_map", "*")) {
			//Get the device-map name
			device_map_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
			//Check if the device-map name specified exists
			err = md_check_if_node_exists(commit, old_db, new_db, device_map_name,
                        "/nkn/device_map/config", &exists);
	                bail_error(err);
			if (exists == 0 ) {
				err = md_commit_set_return_status_msg_fmt(commit, 1,
				    "The device-map %s doesn't exist", device_map_name);
				bail_error(err);
				goto bail;
                	}
			//Validate if the device-map attached has all the required fields configured
			device_map_validation(commit, new_db, device_map_name);
		}
	}
bail:
	return err;
}
