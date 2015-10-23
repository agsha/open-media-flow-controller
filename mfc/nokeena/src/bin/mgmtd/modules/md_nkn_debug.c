/*
 *
 * Filename:    md_nkn_debug.c
 * Date:        2008-11-12
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved/
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
#include "file_utils.h"
#include "nkn_mgmt_defs.h"
#include "nkn_mgmt_log.h"
#define NKNCNT_PATH     "/opt/nkn/bin/nkncnt"

extern unsigned int jnpr_log_level;

/* ------------------------------------------------------------------------- */

int md_nkn_debug_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_nkn_debug_ug_rules = NULL;

static int md_nkn_debug_commit_apply(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

static int
md_nkn_debug_handle_dump_generate(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg);

static int
md_nkn_debug_handle_systemcall(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg);
static int
md_nkn_debug_handle_dump_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed);

static int
md_nkn_debug_handle_amrank_dump_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed);

static int
md_nkn_debug_handle_amrank_dump_generate(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg);

static int
md_nkn_debug_handle_generate_disk_metadata(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg);

static int
md_nkn_debug_handle_generate_disk_metadata_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed);

static int
md_snapshot_files_get(md_commit *commit, const mdb_db *db,
                     const char *node_name,
                     const bn_attrib_array *node_attribs,
                     bn_binding **ret_binding, uint32 *ret_node_flags,
                     void *arg);
static int
md_snapshot_files_iterate(md_commit *commit, const mdb_db *db,
                         const char *parent_node_name,
                         const uint32_array *node_attrib_ids,
                         int32 max_num_nodes, int32 start_child_num,
                         const char *get_next_child,
                         mdm_mon_iterate_names_cb_func iterate_cb,
                         void *iterate_cb_arg, void *arg);
static int
md_nkn_debug_handle_collect_counters(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg);
static int
md_nkn_backtrace_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed);
static int
md_nkn_handle_config_files(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg);

static int
md_nkn_handle_config_files_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed);

static int
md_nkn_handle_backtrace(md_commit *commit,
	const mdb_db *db,
	const char *action_name,
	bn_binding_array *params,
	void *cb_arg);

const char container_dump_path[] = "/opt/nkn/bin/container_dump.py";
const char amrank_dump_path[] = "/opt/nkn/bin/amrank_dump.sh";
const char prog_python_path[] = "/usr/bin/python";
const char disk_metadata_path[] ="/opt/nkn/bin/disk_metadata.sh";
const char config_file_hdlr_path[] ="/opt/nkn/bin/config_file_hdlr.py";

static const bn_str_value md_nkn_passwd_initial_values[] = {
    {"/auth/passwd/user/__mfcd", bt_string, "__mfcd"},
    {"/auth/passwd/user/__mfcd/enable", bt_bool, "true"},
    {"/auth/passwd/user/__mfcd/password", bt_string,
	"$1$XZ3xQtuz$dCSRRI4iP.KWb8qSVfbQa."},
    {"/auth/passwd/user/__mfcd/uid", bt_uint32, "0"},
    {"/auth/passwd/user/__mfcd/gid", bt_uint32, "0"},
    {"/auth/passwd/user/__mfcd/gecos", bt_string, "DMI MFCD"},
    {"/auth/passwd/user/__mfcd/home_dir", bt_string, "/home/mfcd"},
    {"/auth/passwd/user/__mfcd/shell", bt_string, CLI_SHELL_PATH},
    {"/auth/passwd/user/__mfcd/newly_created", bt_bool, "false"},

};

static int
md_nkn_debug_upgrade_downgrade(const mdb_db *old_db,
                          mdb_db *inout_new_db,
                          uint32 from_module_version,
                          uint32 to_module_version, void *arg);

/* struct used to pass credentials for disk meta data upload*/
typedef struct metadata_cb_arg {
	lc_launch_result* lr;
	tstring* remote_url;
	tstring* password;
	char* dump_file;
} metadata_cb_arg_t;

const bn_str_value md_nkn_debug_initial[] = {
    { "/nkn/debug/log/ps/nvsd", bt_string, "nvsd" },
    { "/nkn/debug/log/ps/nvsd/level", bt_uint8, "7" },
    { "/nkn/debug/log/ps/gmgmthd", bt_string, "gmgmthd" },
    { "/nkn/debug/log/ps/gmgmthd/level", bt_uint8, "7" },
    { "/nkn/debug/log/ps/mgmtd", bt_string, "mgmtd" },
    { "/nkn/debug/log/ps/mgmtd/level", bt_uint8, "7" },
    { "/nkn/debug/log/ps/ssld", bt_string, "ssld" },
    { "/nkn/debug/log/ps/ssld/level", bt_uint8, "7" },
    { "/nkn/debug/log/ps/nknlogd", bt_string, "nknlogd" },
    { "/nkn/debug/log/ps/nknlogd/level", bt_uint8, "7" },
    { "/nkn/debug/log/ps/l4proxyd", bt_string, "l4proxyd" },
    { "/nkn/debug/log/ps/l4proxyd/level", bt_uint8, "7" },
    { "/nkn/debug/log/ps/web", bt_string, "web" },
    { "/nkn/debug/log/ps/web/level", bt_uint8, "7" },
    { "/nkn/debug/log/ps/ingester", bt_string, "ingester" },
    { "/nkn/debug/log/ps/ingester/level", bt_uint8, "7" },
    { "/nkn/debug/log/ps/snmp", bt_string, "snmp" },
    { "/nkn/debug/log/ps/snmp/level", bt_uint8, "7" },
    { "/nkn/debug/log/ps/statsd", bt_string, "statsd" },
    { "/nkn/debug/log/ps/statsd/level", bt_uint8, "7" },
    { "/nkn/debug/log/ps/agentd", bt_string, "agentd" },
    { "/nkn/debug/log/ps/agentd/level", bt_uint8, "7" },
};

static int
md_nkn_debug_upgrade_downgrade(const mdb_db *old_db,
                          mdb_db *inout_new_db,
                          uint32 from_module_version,
                          uint32 to_module_version, void *arg)
{
    int err = 0;

    lc_log_basic(LOG_INFO, "called from %d to %d",
	    from_module_version,to_module_version );

    err = md_generic_upgrade_downgrade(old_db, inout_new_db, from_module_version,
	    to_module_version, arg);
    bail_error(err);

    if (to_module_version == 4) {
	err = mdb_create_node_bindings(NULL, inout_new_db, md_nkn_debug_initial,
		sizeof(md_nkn_debug_initial)/sizeof(bn_str_value));
	bail_error(err);
    }
    else if (to_module_version == 5) {

        err = mdb_create_node_bindings(NULL, inout_new_db,
		md_nkn_passwd_initial_values,
		sizeof(md_nkn_passwd_initial_values)/sizeof(bn_str_value));
        bail_error(err);
    }

bail:
    return err;
}

static int
md_nkn_debug_add_initial(
		md_commit *commit,
		mdb_db    *db,
		void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings(commit, db, md_nkn_debug_initial,
	    sizeof(md_nkn_debug_initial)/sizeof(bn_str_value));
    bail_error(err);

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
int md_nkn_debug_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    err = mdm_add_module("nkn-debug", 6,
	    "/nkn/debug", NULL,
	    modrf_all_changes,
	    NULL, NULL,
	    NULL, NULL,
	    md_nkn_debug_commit_apply, NULL,
	    0, 0,
	    md_nkn_debug_upgrade_downgrade,
	    &md_nkn_debug_ug_rules,
	    md_nkn_debug_add_initial, NULL,
	    NULL, NULL,
	    NULL, NULL,
	    &module);
    bail_error(err);


    /*
     * Monitoring node for the archieved logs
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/debug/snapshot/corefiles/*";
    node->mrn_value_type	= bt_string;
    node->mrn_node_reg_flags	= mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_mon_get_func	= md_snapshot_files_get;
    node->mrn_mon_iterate_func	= md_snapshot_files_iterate;
    node->mrn_description	= "List of snapshot files available";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name =                "/nkn/debug/generate/dump";
    node->mrn_node_reg_flags =      mrf_flags_reg_action;
    node->mrn_cap_mask =            mcf_cap_action_restricted;
    node->mrn_action_async_start_func = md_nkn_debug_handle_dump_generate;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name =                "/nkn/debug/generate/amrankdump";
    node->mrn_node_reg_flags =      mrf_flags_reg_action;
    node->mrn_cap_mask =            mcf_cap_action_restricted;
    node->mrn_action_async_start_func = md_nkn_debug_handle_amrank_dump_generate;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name               = "/nkn/debug/generate/systemcall";
    node->mrn_node_reg_flags     = mrf_flags_reg_action;
    node->mrn_cap_mask           = mcf_cap_action_privileged;
    node->mrn_action_func        = md_nkn_debug_handle_systemcall;
    node->mrn_description        = "System call handler";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 4);
    bail_error(err);
    node->mrn_name               = "/nkn/debug/generate/collect-counters";
    node->mrn_node_reg_flags     = mrf_flags_reg_action;
    node->mrn_cap_mask           = mcf_cap_action_privileged;
    node->mrn_action_func        = md_nkn_debug_handle_collect_counters;
    node->mrn_actions[0]->mra_param_name = "frequency";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "duration";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_actions[2]->mra_param_name = "filename";
    node->mrn_actions[2]->mra_param_type = bt_string;
    node->mrn_actions[3]->mra_param_name = "pattern";
    node->mrn_actions[3]->mra_param_type = bt_string;
    node->mrn_description        = "Collect counter details";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name =                "/nkn/debug/generate/disk_metadata";
    node->mrn_node_reg_flags =      mrf_flags_reg_action;
    node->mrn_cap_mask =            mcf_cap_action_restricted;
    node->mrn_action_async_start_func =  md_nkn_debug_handle_generate_disk_metadata;
    node->mrn_actions[0]->mra_param_name = "remote_url";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "password";
    node->mrn_actions[1]->mra_param_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name =                "/nkn/debug/action/upload_bt";
    node->mrn_node_reg_flags =      mrf_flags_reg_action;
    node->mrn_cap_mask =            mcf_cap_action_restricted;
    node->mrn_action_async_start_func =  md_nkn_handle_backtrace;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/debug/log/all/level";
    node->mrn_value_type	= bt_uint8;
    node->mrn_initial_value_int =  7;
    node->mrn_limit_num_min_int =  0;	/* LOG_FATAL */
    node->mrn_limit_num_max_int =  7;	/* LOG_DEBUG */
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_description	= "logging level for all processes";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/debug/log/ps/*";
    node->mrn_value_type	= bt_string;
    node->mrn_node_reg_flags	= mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_description	= "list of all mgmt  processes";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name		= "/nkn/debug/log/ps/*/level";
    node->mrn_value_type	= bt_uint8;
    node->mrn_initial_value_int =  7;
    node->mrn_limit_num_min_int =  0;	/* LOG_FATAL */
    node->mrn_limit_num_max_int =  7;	/* LOG_DEBUG */
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask		= mcf_cap_node_restricted;
    node->mrn_description	= "logging level for per process";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name =                "/nkn/debug/generate/config_files";
    node->mrn_node_reg_flags =      mrf_flags_reg_action;
    node->mrn_cap_mask =            mcf_cap_action_restricted;
    node->mrn_action_async_start_func = md_nkn_handle_config_files;
    node->mrn_actions[0]->mra_param_name = "cmd";
    node->mrn_actions[0]->mra_param_type = bt_uint16;
    node->mrn_actions[1]->mra_param_name = "file_name";
    node->mrn_actions[1]->mra_param_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = md_upgrade_rule_array_new(&md_nkn_debug_ug_rules);
    bail_error(err);

    ra = md_nkn_debug_ug_rules;

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/debug/log/all/level";
    rule->mur_new_value_type =  bt_uint8;
    rule->mur_new_value_str  =  "7";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/debug/log/ps/*/level";
    rule->mur_new_value_type =  bt_uint8;
    rule->mur_new_value_str  =  "7";
    MD_ADD_RULE(ra);

bail:
    return err;
}


/* ------------------------------------------------------------------------- */
static int
md_nkn_debug_commit_apply(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0, num_changes = 0, i = 0;
    uint8 all_log_level = -1, mgmt_log_level = -1;
    tbool all_found = false, mgmt_found = false;
    mdb_db_change *change = NULL;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "debug", "log", "all", "level"))) {
	    err = mdb_get_node_value_uint8(commit, new_db,
		    ts_str(change->mdc_name), 0 , &all_found, &all_log_level);
	    bail_error(err);

	} else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "debug", "log", "ps", "mgmtd", "level"))) {
	    err = mdb_get_node_value_uint8(commit, new_db,
		    ts_str(change->mdc_name), 0 , &mgmt_found, &mgmt_log_level);
	    bail_error(err);
	}
    }

    if (all_found) {
	set_log_level(all_log_level);
    }

    if (mgmt_found) {
	set_log_level(mgmt_log_level);
    }

 bail:
    return(err);
}

static int
md_nkn_handle_config_files(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg)

{
    int err = 0;
    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;
    char *cmd = NULL;
    char *f_name = NULL;
    const bn_binding *binding = NULL;

    lr = malloc(sizeof(*lr));
    bail_null(lr);
    memset(lr, 0, sizeof(*lr));

    err = bn_binding_array_get_binding_by_name(params, "cmd", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_uint16, NULL, &cmd);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name(params, "file_name", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &f_name);

    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, lp);

    err = ts_new_str(&(lp->lp_path), prog_python_path);
    bail_error(err);

    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 4, prog_python_path, config_file_hdlr_path, cmd, f_name);
    bail_error(err);

    lp->lp_wait = false;

    lp->lp_env_opts = eo_preserve_all;

    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

    err = lc_launch(lp, lr);
    bail_error(err);

    err = md_commit_set_completion_proc_async(commit, true);

    err = md_commit_add_completion_proc_state_action(commit,
            lr->lr_child_pid, lr->lr_exit_status,
            lr->lr_child_pid == -1,
            "md_nkn_handle_config_files",
            md_nkn_handle_config_files_finalize, lr);
    lr = NULL;
    bail_error(err);


bail:
    lc_launch_params_free(&lp);
    if(lr) {
        lc_launch_result_free_contents(lr);
        safe_free(lr);
    }
    safe_free(cmd);
    safe_free(f_name);
    return err;
}


static int
md_nkn_handle_config_files_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed)
{
    int err = 0;
    lc_launch_result *lr = finalize_arg;
    const tstring *output = NULL;
    int32 offset1 = 0, offset2 = 0;
    bn_binding *binding = NULL;

    bail_null(commit);
    bail_null(lr);

    if(completed) {
        lr->lr_child_pid = -1;
        lr->lr_exit_status = wait_status;
    }

    err = lc_launch_complete_capture(lr);
    bail_error(err);

    err = lc_check_exit_status_full(lr->lr_exit_status, NULL, LOG_INFO,
            LOG_WARNING, LOG_ERR,
            config_file_hdlr_path);
    bail_error(err);

    output = lr->lr_captured_output;
    bail_null(output);

    err = bn_binding_new_str(&binding, "Result", ba_value, bt_string, 0, ts_str(output));
    bail_error(err);
    err = md_commit_binding_append(commit, 0, binding);
    bail_error(err);

bail:
    if (lr) {
        lc_launch_result_free_contents(lr);
        safe_free(lr);
    }
    bn_binding_free(&binding);
    return err;
}

static int
md_nkn_debug_handle_dump_generate(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg)
{
    int err = 0;
    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;

    lr = malloc(sizeof(*lr));
    bail_null(lr);
    memset(lr, 0, sizeof(*lr));

    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, lp);

    err = ts_new_str(&(lp->lp_path), prog_python_path);
    bail_error(err);

    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 2, prog_python_path, container_dump_path);
    bail_error(err);

    lp->lp_wait = false;

    lp->lp_env_opts = eo_preserve_all;

    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

    err = lc_launch(lp, lr);
    bail_error(err);

    err = md_commit_set_completion_proc_async(commit, true);

    err = md_commit_add_completion_proc_state_action(commit,
            lr->lr_child_pid, lr->lr_exit_status,
            lr->lr_child_pid == -1,
            "md_nkn_debug_handle_dump_generate",
            md_nkn_debug_handle_dump_finalize, lr);
    lr = NULL;
    bail_error(err);


bail:
    lc_launch_params_free(&lp);
    if(lr) {
        lc_launch_result_free_contents(lr);
        safe_free(lr);
    }
    return err;
}


static int
md_nkn_debug_handle_dump_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed)
{
    int err = 0;
    lc_launch_result *lr = finalize_arg;
    const tstring *output = NULL;
    tstring *container = NULL;
    int32 offset1 = 0, offset2 = 0;
    bn_binding *binding = NULL;

    bail_null(commit);
    bail_null(lr);

    if(completed) {
        lr->lr_child_pid = -1;
        lr->lr_exit_status = wait_status;
    }

    err = lc_launch_complete_capture(lr);
    bail_error(err);

    err = lc_check_exit_status_full(lr->lr_exit_status, NULL, LOG_INFO,
            LOG_WARNING, LOG_ERR,
            container_dump_path);
    bail_error(err);

    output = lr->lr_captured_output;
    bail_null(output);

    err = ts_find_str(output, 0, "CONTAINER=", &offset1);
    bail_error(err);
    bail_require(offset1 >= 0);
    offset1 += strlen("CONTAINER=");

    err = ts_find_char(output, offset1, '\n', &offset2);
    bail_error(err);

    err = ts_substr(output, offset1, offset2 - offset1, &container);
    bail_error_null(err, container);

    err = bn_binding_new_str(&binding, "dump_path", ba_value, bt_string, 0, ts_str(container));
    bail_error(err);

    err = md_commit_binding_append(commit, 0, binding);
    bail_error(err);

bail:
    if (lr) {
        lc_launch_result_free_contents(lr);
        safe_free(lr);
    }
    ts_free(&container);
    bn_binding_free(&binding);
    return err;
}

static int
md_nkn_debug_handle_amrank_dump_generate(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg)
{
    int err = 0;
    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;

    lr = malloc(sizeof(*lr));
    bail_null(lr);
    memset(lr, 0, sizeof(*lr));

    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, lp);

    err = ts_new_str(&(lp->lp_path), prog_sh_path);
    bail_error(err);

    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 2, prog_sh_path, amrank_dump_path);
    bail_error(err);

    lp->lp_wait = false;

    lp->lp_env_opts = eo_preserve_all;

    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

    err = lc_launch(lp, lr);
    bail_error(err);

    err = md_commit_set_completion_proc_async(commit, true);

    err = md_commit_add_completion_proc_state_action(commit,
            lr->lr_child_pid, lr->lr_exit_status,
            lr->lr_child_pid == -1,
            "md_nkn_debug_handle_amrank_dump_generate",
            md_nkn_debug_handle_amrank_dump_finalize, lr);
    lr = NULL;
    bail_error(err);


bail:
    lc_launch_params_free(&lp);
    if(lr) {
        lc_launch_result_free_contents(lr);
        safe_free(lr);
    }
    return err;
}

static int
md_nkn_debug_handle_generate_disk_metadata(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg)
{
    int err = 0;
    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;
    lt_time_sec curr_time;
    lt_date ret_date;
    lt_time_sec ret_daytime_sec;
    char time_buf[64];
    char* metadata_file;
    metadata_cb_arg_t* metadata_cb_arg;
    tstring *t_remote_url = NULL;
    tstring *t_password = NULL;

    lr = malloc(sizeof(*lr));
    bail_null(lr);
    memset(lr, 0, sizeof(*lr));

    metadata_cb_arg = malloc(sizeof(*metadata_cb_arg));
    memset(metadata_cb_arg, 0,sizeof(*metadata_cb_arg));

    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, lp);
    err = ts_new_str(&(lp->lp_path), prog_sh_path);
    bail_error(err);

    /* get the time in current time zone and convert as string*/
    curr_time  = lt_curr_time();

    lt_time_to_date_daytime_sec (curr_time, &ret_date, &ret_daytime_sec);

    lt_daytime_sec_to_buf (ret_daytime_sec, 64, time_buf);

    metadata_file = smprintf("/var/log/nkn/dm2_meta_data.%s",time_buf);
    bail_null(metadata_file);

    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 3, prog_sh_path, disk_metadata_path, metadata_file);
    bail_error(err);

    lp->lp_wait = false;

    lp->lp_env_opts = eo_preserve_all;

    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

    err = bn_binding_array_get_value_tstr_by_name
	(params, "remote_url", NULL, &t_remote_url);
    bail_error_null(err, t_remote_url);

    err = bn_binding_array_get_value_tstr_by_name
	(params, "password", NULL, &t_password);
    bail_error_null(err, t_password);

    err = lc_launch(lp, lr);
    bail_error(err);
    metadata_cb_arg->lr = lr;
    metadata_cb_arg->remote_url = t_remote_url;
    metadata_cb_arg->password = t_password;
    metadata_cb_arg->dump_file = metadata_file;

    err = md_commit_set_completion_proc_async(commit, true);

    err = md_commit_add_completion_proc_state_action(commit,
	    lr->lr_child_pid, lr->lr_exit_status,
	    lr->lr_child_pid == -1,
            "md_nkn_debug_handle_generate_disk_metadata",
	    md_nkn_debug_handle_generate_disk_metadata_finalize,
	    metadata_cb_arg);
    bail_error(err);

bail:
    lc_launch_params_free(&lp);
    return err;
}

static int
md_nkn_debug_handle_generate_disk_metadata_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed)
{
    int err = 0;
    metadata_cb_arg_t* metadata_cb_arg = NULL;
    lc_launch_result *lr = NULL;
    const tstring *output = NULL;
    tstring *dump_name = NULL;
    tstring *dump_dir = NULL;
    tstring *dump_file = NULL;
    bn_request *req = NULL;

    bail_null(commit);
    bail_null(finalize_arg);
    metadata_cb_arg = (metadata_cb_arg_t*)finalize_arg;

    bail_null(metadata_cb_arg->remote_url);
    bail_null(metadata_cb_arg->dump_file);
    /* not checking for password here as it can be null */

    lr = (lc_launch_result*)metadata_cb_arg->lr;
    bail_null(lr);

    if(completed) {
	lr->lr_child_pid = -1;
	lr->lr_exit_status = wait_status;
    }

    err = lc_launch_complete_capture(lr);
    bail_error(err);

    err = lc_check_exit_status_full(lr->lr_exit_status, NULL, LOG_INFO,
	    LOG_WARNING, LOG_ERR,
	    disk_metadata_path);
    bail_error(err);

    output = lr->lr_captured_output;
    bail_null(output);

    if(strncmp(ts_str(output), "Cache is empty", 14) == 0) {
	lc_log_basic(LOG_NOTICE, "Cache is empty,Will not upload meta-data.");
	goto bail;
    }

    err = ts_new_sprintf(&dump_file, "%s", metadata_cb_arg->dump_file);
    bail_error_null(err, dump_file);

    err = lf_path_last(ts_str(dump_file), &dump_name);
    bail_error_null(err, dump_name);

    err = lf_path_parent(ts_str(dump_file), &dump_dir);
    bail_error_null(err, dump_dir);

    err = bn_action_request_msg_create(&req,
	    "/file/transfer/upload");
    bail_error_null(err, req);

    if (metadata_cb_arg->password) {
	err = ts_trim_whitespace (dump_file);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding(req,
		0,
		"local_path",
		bt_string,
		ts_str(dump_file),
		NULL);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding(req,
		0,
		"remote_url",
		bt_string,
		ts_str(metadata_cb_arg->remote_url),
		NULL);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding(req,
		0,
		"password",
		bt_string,
		ts_str(metadata_cb_arg->password),
		NULL);
	bail_error(err);


	err = md_commit_handle_action_from_module(
		commit, req, NULL, NULL, NULL, NULL, NULL);
	bail_error(err);

    }
    else {
	err = ts_trim_whitespace (dump_name);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding(req,
		0,
		"local_dir",
		bt_string,
		ts_str(dump_dir),
		NULL);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding(req,
		0,
		"local_filename",
		bt_string,
		ts_str(dump_name),
		NULL);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding(req,
		0,
		"remote_url",
		bt_string,
		ts_str(metadata_cb_arg->remote_url),
		NULL);
	bail_error(err);


	err = md_commit_handle_action_from_module(
		commit, req, NULL, NULL, NULL, NULL, NULL);
	bail_error(err);

    }
    lc_log_basic(LOG_NOTICE, "Started uploading meta-data to %s", ts_str(metadata_cb_arg->remote_url));

bail:
    if (lr) {
	lc_launch_result_free_contents(lr);
	safe_free(lr);
    }
    if(metadata_cb_arg) {
	ts_free(&metadata_cb_arg->remote_url);
	ts_free(&metadata_cb_arg->password);
	safe_free(metadata_cb_arg->dump_file);
	safe_free(metadata_cb_arg);
    }
    ts_free(&dump_name);
    ts_free(&dump_dir);
    ts_free(&dump_file);
    return err;
}

static int
md_nkn_debug_handle_amrank_dump_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed)
{
    int err = 0;
    lc_launch_result *lr = finalize_arg;
    const tstring *output = NULL;
    tstring *container = NULL;
    int32 offset1 = 0, offset2 = 0;
    bn_binding *binding = NULL;

    bail_null(commit);
    bail_null(lr);

    if(completed) {
	lr->lr_child_pid = -1;
	lr->lr_exit_status = wait_status;
    }

    err = lc_launch_complete_capture(lr);
    bail_error(err);

    err = lc_check_exit_status_full(lr->lr_exit_status, NULL, LOG_INFO,
	    LOG_WARNING, LOG_ERR,
	    amrank_dump_path);
    bail_error(err);

    output = lr->lr_captured_output;
    bail_null(output);

    err = ts_find_str(output, 0, "AMDUMP=", &offset1);
    bail_error(err);
    bail_require(offset1 >= 0);
    offset1 += strlen("AMDUMP=");

    err = ts_find_char(output, offset1, '\n', &offset2);
    bail_error(err);

    err = ts_substr(output, offset1, offset2 - offset1, &container);
    bail_error_null(err, container);

    err = bn_binding_new_str(&binding, "dump_path", ba_value, bt_string, 0, ts_str(container));
    bail_error(err);

    err = md_commit_binding_append(commit, 0, binding);
    bail_error(err);

bail:
    if (lr) {
	lc_launch_result_free_contents(lr);
	safe_free(lr);
    }
    ts_free(&container);
    bn_binding_free(&binding);
    return err;
}


/*
 * Action to handle system calls from mgmtd
 * Primarily used for calls from CMC to MFD that
 * call clish_ ... () which will not run from
 * RGP.
 */
static int
md_nkn_debug_handle_systemcall(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg)
{
    uint32_t i;
    int err = 0;
    char temp_str [5];
    uint32_t param_count = 0;
    tstring *prog_path = NULL;
    tstring *arg_value = NULL;
    tstring *stdout_output = NULL;
    lc_launch_params *lp = NULL;
    lc_launch_result lr;


    /* Initialize launch params */
    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, lp);

    /* Get the number of parameters */
    param_count = bn_binding_array_length_quick(params);

    /* Get the command */
    err = bn_binding_array_get_value_tstr_by_name(params, "command",  NULL, &prog_path);
    bail_error(err);

    err = ts_dup(prog_path, &(lp->lp_path));
    bail_error(err);

    err = tstr_array_new(&(lp->lp_argv), 0);
    bail_error(err);

    err = tstr_array_insert_str(lp->lp_argv, 0, ts_str(prog_path));
    bail_error(err);

    /* Get the arguments */
    for (i = 1; i < param_count; i++)
    {
    	sprintf (temp_str, "%d", i);

        err = bn_binding_array_get_value_tstr_by_name(params, temp_str,  NULL, &arg_value);
        bail_error(err);

        err = tstr_array_insert_str(lp->lp_argv, i, ts_str(arg_value));
        bail_error(err);

	ts_free(&arg_value);
    }

    lp->lp_wait = true;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;

    err = lc_launch(lp, &lr);
    bail_error(err);

    stdout_output = lr.lr_captured_output;

    err = md_commit_set_return_status_msg
                (commit, 0, ts_str(stdout_output));
    bail_error(err);

bail:
    ts_free(&stdout_output);
    ts_free(&prog_path);
    lc_launch_params_free(&lp);
    return err;
} /* end of md_nkn_debug_handle_systemcall() */


/* ------------------------------------------------------------------------- *
 * Description : This is called from the iterator. This function creates a
 * 		node based on the filename  in the parameter
 * ------------------------------------------------------------------------- */
static int
md_snapshot_files_get(md_commit *commit, const mdb_db *db,
                     const char *node_name,
                     const bn_attrib_array *node_attribs,
                     bn_binding **ret_binding, uint32 *ret_node_flags,
                     void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *filename = NULL;

    /* XXX/EMT: should validate filename */

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error(err);

    /* Verify this is "/nkn/snapshot/corefiles/ *" */
    num_parts = tstr_array_length_quick(parts);
    bail_require(num_parts == 5);

    filename = tstr_array_get_str_quick(parts, 4);
    bail_null(filename);

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string, 0,
                             filename);
    bail_error(err);

 bail:
    tstr_array_free(&parts);
    return(err);
}


/* ------------------------------------------------------------------------- *
 * Description : Iterate through the snapshot directory and get  the list of
 *		snapshots that are there
 *
 * ------------------------------------------------------------------------- */
static int
md_snapshot_files_iterate(md_commit *commit, const mdb_db *db,
                         const char *parent_node_name,
                         const uint32_array *node_attrib_ids,
                         int32 max_num_nodes, int32 start_child_num,
                         const char *get_next_child,
                         mdm_mon_iterate_names_cb_func iterate_cb,
                         void *iterate_cb_arg, void *arg)
{
    int err = 0;
    tstr_array *names = NULL;
    const char *name = NULL;
    uint32 num_names = 0, i = 0;

    /*
     * Get the list of files in the snapshot directory
     */

    err = lf_dir_list_names("/coredump/snapshots", &names);
    bail_error_null(err, names);

    /* Get the  total number of files */
    num_names = tstr_array_length_quick(names);

    /* Now iterate through the files and call the function to create the node */
    for (i = 0; i < num_names; ++i) {
        /* Get the file name first */
        name = tstr_array_get_str_quick(names, i);
        bail_null(name);

	/* Make sure it ends with .gz  */
        if (lc_is_suffix(".gz", name, false)) {
	    /* Now call the function that creates the node */
            err = (*iterate_cb)(commit, db, name, iterate_cb_arg);
            bail_error(err);
        }
    }

 bail:
    tstr_array_free(&names);
    return(err);
} /* end of md_snapshot_files_iterate () */


/*
 * BUG 7146: Moving the collect counter background data collection from
 * CLI to mgmthd. This is to ensure that the data collection happens even
 * when the CLI session exits.
 * The function call md_nkn_debug_handle_collect_counters will launch the
 * /opt/nkn/bin/collect_nkncnt_data.sh which in turn invokes the /opt/nkn/bin/nkncnt
 * as a background process.
 */
static int
md_nkn_debug_handle_collect_counters(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg)
{
    int err = 0;
    uint32_t i;
    uint32_t ret_err = 0;
    bn_request *req = NULL;
    tstring *ret_msg = NULL;
    tstring *stdout_output = NULL;
    lc_launch_params *lp = NULL;
    lc_launch_result lr;
    char *frequency = NULL;
    char *duration = NULL;
    char *pattern = NULL;
    char *filename = NULL;
    const bn_binding *binding = NULL;
    /* Initialize launch params */
    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, &lp);
    err = ts_new_str(&(lp->lp_path), NKNCNT_PATH);
    bail_error(err);
    err = tstr_array_new(&(lp->lp_argv), 0);
    bail_error(err);

    err = tstr_array_insert_str(lp->lp_argv, 0,  NKNCNT_PATH);
    bail_error(err);

    /* Frequency value*/
    err = bn_binding_array_get_binding_by_name(params, "frequency", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &frequency);
    err = tstr_array_insert_str(lp->lp_argv, 1, "-t");
    bail_error (err);
    if(frequency) {
	err = tstr_array_insert_str(lp->lp_argv, 2, frequency);
	bail_error(err);
    }else {//Defualt value is set to 0.
	err = tstr_array_insert_str(lp->lp_argv, 2, "0");
	bail_error(err);
    }
    /* Duration value */
    err = bn_binding_array_get_binding_by_name(params, "duration", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &duration);
    err = tstr_array_insert_str(lp->lp_argv, 3, "-l");
    bail_error (err);
    if(duration) {
	err = tstr_array_insert_str(lp->lp_argv, 4, duration);
    }else {//Default value is set to 0.
	err = tstr_array_insert_str(lp->lp_argv, 4, "0");
    }
    bail_error(err);
    /* Search pattern */
    err = bn_binding_array_get_binding_by_name(params, "pattern", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &pattern);
    err = tstr_array_insert_str(lp->lp_argv, 5, "-s");
    bail_error (err);
    //The default pattern "-" is changed to" _" in the CLI command itself.
    //Since the pattern string is already set, NULL check is skipped.
    err = tstr_array_insert_str(lp->lp_argv, 6, pattern);
    bail_error(err);
    /* Filename */
    err = bn_binding_array_get_binding_by_name(params, "filename", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &filename);
    err = tstr_array_insert_str(lp->lp_argv, 7, "-S");
    bail_error (err);
    //The filename will never be NULL , hence skipping the NULL check.
    err = tstr_array_insert_str(lp->lp_argv, 8, filename);
    bail_error(err);

    lp->lp_wait = false;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_INFO;
    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_devnull;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_devnull;
    err = lc_launch(lp, &lr);
    bail_error(err);
bail:
    safe_free(frequency);
    safe_free(pattern);
    safe_free(duration);
    safe_free(filename);
    ts_free(&stdout_output);
    lc_launch_params_free(&lp);
    return err;
}

static int
md_nkn_handle_backtrace(md_commit *commit,
	const mdb_db *db,
	const char *action_name,
	bn_binding_array *params,
	void *cb_arg)
{
    int err = 0;
    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;
    uint32 file_count = 0;
    bn_binding *binding = NULL;

    /* check if any file exist or not (don't count "."/".."/".anything else") */
    err = lf_dir_count_names_matching("/var/nkn/backtraces",
	    lf_dir_matching_nodots_names_func,
	    NULL,
	    &file_count);
    bail_error(err);

    lc_log_basic(LOG_DEBUG, "file_ count = %d", file_count);
    if (file_count == 0) {
	err = 0;
	/* send the absolute path back to the caller */
	err = bn_binding_new_str(&binding, "dump_path", ba_value, bt_string, 0, "");
	bail_error(err);

	err = md_commit_binding_append(commit, 0, binding);
	bail_error(err);

	/* tell CLI to return error */
	err = md_commit_set_return_status_msg(commit, 1, "No backtraces found");
	bail_error(err);
	goto bail;
    }

    lr = malloc(sizeof(*lr));
    bail_null(lr);
    memset(lr, 0, sizeof(*lr));

    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, lp);

    err = ts_new_str(&(lp->lp_path), "/bin/tar");
    bail_error(err);

    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 6, "/bin/tar",
	    "-C", "/var/nkn",
	    "-zcf", "/tmp/backtraces.tgz",
	    "backtraces"
	    );

    bail_error(err);

    /* setting wait to false, so that process doesn't block on child process */
    lp->lp_wait = false;

    lp->lp_env_opts = eo_preserve_all;

    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_devnull;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_devnull;

    /* lc_launch() will fork and exec, return immedietly */
    err = lc_launch(lp, lr);
    bail_error(err);

    err = md_commit_set_completion_proc_async(commit, true);
    bail_error(err);

    err = md_commit_add_completion_proc_state_action(commit,
	    lr->lr_child_pid,
	    lr->lr_exit_status,
	    lr->lr_child_pid == -1,
            "md_nkn_handle_backtrace",
	    md_nkn_backtrace_finalize,
	    (void *)"/tmp/backtraces.tgz");

    bail_error(err);


bail:
    if (lp) {
	lc_launch_params_free(&lp);
    }
    if(lr) {
	/* in case of error, lr wouldn't have been freed */
	lc_launch_result_free_contents(lr);
	safe_free(lr);
    }
    bn_binding_free(&binding);
    return err;
}

/*
 * finalize_arg	- contains the full path of file generated
 * NOTE: this callback will be called as soon as childd forked by lc_launch
 * exits.
 */
static int
md_nkn_backtrace_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed)
{
    const char *filename = (const char *)finalize_arg;
    bn_binding *binding = NULL;
    int err = 0;

    bail_null(commit);
    bail_null(filename);

    /* send the absolute path back to the caller */
    err = bn_binding_new_str(&binding, "dump_path", ba_value, bt_string, 0, filename);
    bail_error(err);

    err = md_commit_binding_append(commit, 0, binding);
    bail_error(err);

bail:
    bn_binding_free(&binding);
    return err;

}
/* End of md_nkn_debug.c */
