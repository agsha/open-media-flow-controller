/*
 * Filename :   cli_bgp_cmds.c
 *
 * (C) Copyright 2011 Juniper Networks.
 * All rights reserved.
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include <strings.h>
#include "proc_utils.h"
#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "libnkncli.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

/*----------------------------------------------------------------------------
 *  LOCAL FUNCTION DEFINITIONS
 *  --------------------------------------------------------------------------*/
static int
 cli_log_analyzer_cfg_revmap (void *data, const cli_command *cmd, const bn_binding_array *bindings, const char *name, const tstr_array *name_parts, const char *value, const bn_binding *binding, cli_revmap_reply *ret_reply);

static int
cli_log_analyzer_download_file(const char *password, clish_password_result result, const cli_command *cmd,
			const tstr_array *cmd_line, const tstr_array *params, void *data, tbool *ret_continue);
/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
extern int
cli_std_exec(void *data, const cli_command *cmd, const tstr_array *cmd_line, const tstr_array *params);

int cli_log_analyzer_cmds_init(const lc_dso_info *info,
                      const cli_module_context *context);

int
cli_config_files_cmds_init(
	const lc_dso_info *info,
	const cli_module_context *context);

int cli_clear_filter_rules(void *arg,
        const cli_command *cmd,
        const tstr_array  *cmd_line,
        const tstr_array  *params);

int cli_log_analyzer_fetchurl(void *arg,
        const cli_command *cmd,
        const tstr_array  *cmd_line,
        const tstr_array  *params);

int
cli_log_analyzer_show(void *data, const cli_command *cmd, const tstr_array *cmd_line, const tstr_array *params);

/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
cli_log_analyzer_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	if (context->cmc_mgmt_avail == false) {
	goto bail;
	}

	CLI_CMD_NEW;
	cmd->cc_words_str =         "clear log-analyzer";
	cmd->cc_help_descr =        N_("Keyword directive for log-analyzer related options");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str       = "clear log-analyzer filter-rules";
	cmd->cc_help_descr      = N_("Keyword to specify actions on filter rules configured in the device");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str       = "clear log-analyzer filter-rules *";
	cmd->cc_help_exp	= N_("<device-map>");
	cmd->cc_help_exp_hint	= N_("Name of a device-map where the action on rules are targeted");
	cmd->cc_flags           = ccf_terminal;
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_exec_callback   = cli_clear_filter_rules;
	cmd->cc_help_use_comp	= true;
	cmd->cc_comp_type	= cct_matching_names;
	cmd->cc_comp_pattern =      "/nkn/log_analyzer/config/device_map/*";
	cmd->cc_exec_operation =    cxo_action;
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer";
	cmd->cc_help_descr =        N_("Keyword directive to module for analyzing the DPI log files");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "no log-analyzer";
	cmd->cc_help_descr =        N_("Keyword directive to module for analyzing the DPI log files");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer target";
	cmd->cc_help_descr =        N_("Keyword to specify the target device for filter rules configuration");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "no log-analyzer target";
	cmd->cc_help_descr =        N_("Keyword to specify the target device for filter rules configuration");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer target *";
	cmd->cc_help_exp =          N_("<device-map>");
	cmd->cc_help_exp_hint =     "Name assigned to an already configured device map";
	cmd->cc_flags =             ccf_terminal|ccf_prefix_mode_root;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_help_use_comp =     true;
	cmd->cc_comp_type =         cct_matching_names;
	cmd->cc_comp_pattern =      "/nkn/device_map/config/*";
	cmd->cc_exec_name =         "/nkn/log_analyzer/config/device_map/$1$";
	cmd->cc_exec_value =        "$1$";
	cmd->cc_exec_type =         bt_string;
	cmd->cc_revmap_type =       crt_auto;
	cmd->cc_revmap_order =     cro_log_analyzer;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no log-analyzer target *";
	cmd->cc_help_exp =          N_("<device-map>");
	cmd->cc_help_exp_hint =     "Name assigned to an already configured device map";
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_delete;
	cmd->cc_help_use_comp =     true;
	cmd->cc_comp_type =         cct_matching_names;
	cmd->cc_comp_pattern =      "/nkn/log_analyzer/config/device_map/*";
	cmd->cc_exec_name =         "/nkn/log_analyzer/config/device_map/$1$";
	cmd->cc_exec_value =        "$1$";
	cmd->cc_exec_type =         bt_string;
	cmd->cc_revmap_order =     cro_log_analyzer;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer target * command-mode";
	cmd->cc_help_descr =        N_("Keyword directive on how filter commands be applied to device");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer target * command-mode inline";
	cmd->cc_help_descr =        N_("Policies will be configured on the device by log analyzer");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/log_analyzer/config/device_map/$1$/command_mode/inline";
	cmd->cc_exec_value =        "true";
	cmd->cc_exec_type =         bt_bool;
	cmd->cc_revmap_type =       crt_auto;
	cmd->cc_revmap_order =     cro_log_analyzer;
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer target * command-mode batch-file";
	cmd->cc_help_descr =        N_("Policies will be saved to a file by the log-analyzer to be applied by the ADMIN");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/log_analyzer/config/device_map/$1$/command_mode/inline";
	cmd->cc_exec_value =        "false";
	cmd->cc_exec_type =         bt_bool;
	cmd->cc_revmap_type =       crt_auto;
	cmd->cc_revmap_order =     cro_log_analyzer;
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer config-file";
	cmd->cc_help_descr =        N_("MFC config file containing namespace definitions of well-known domains");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "no log-analyzer config-file";
	cmd->cc_flags =             ccf_unavailable;
	cmd->cc_help_descr =        N_("MFC config file containing namespace definitions of well-known domains");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "no log-analyzer config-file include";
	cmd->cc_help_descr =        N_("Keyword directive to specify inclusion of namespaces for log analysis");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "no log-analyzer config-file include all-ns";
	cmd->cc_help_descr =        N_("Keyword to specify that catch-all T-Proxy namespace be included in log analysis");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/log_analyzer/config/config_file/all-ns";
	cmd->cc_exec_value =        "false";
	cmd->cc_exec_type =         bt_bool;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer config-file url";
	cmd->cc_help_descr =        N_("HTTP or SCP url for fetching config-file from an external server");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer config-file local-file";
	cmd->cc_help_descr =        N_("Keyword to specify that config file be taken from a known location local to the MFC");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/log_analyzer/config/config_file/url";
	cmd->cc_exec_value =        "local";
	cmd->cc_exec_type =         bt_string;
	cmd->cc_revmap_type =       crt_none;
	cmd->cc_revmap_order =     cro_log_analyzer;
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer config-file url *";
	cmd->cc_help_exp =           N_("<url>");
	cmd->cc_help_exp_hint =        N_("Remote URL (http, scp) [http://<remote_hostname>/<path>|scp://<username>:<password>@<remote hostname>/<path>]");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/log_analyzer/config/config_file/url";
	cmd->cc_exec_value =        "$1$";
	cmd->cc_exec_type =         bt_string;
	cmd->cc_exec_callback =		cli_log_analyzer_fetchurl;
	cmd->cc_revmap_type =       crt_none;
	cmd->cc_revmap_order =     cro_log_analyzer;
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer config-file url * include";
	cmd->cc_flags =             ccf_unavailable;
	cmd->cc_help_descr =        N_("Keyword directive to specify inclusion of namespaces for log analysis");
	CLI_CMD_REGISTER;
	
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer config-file local-file include";
	cmd->cc_flags =             ccf_unavailable;
	cmd->cc_help_descr =        N_("Keyword directive to specify inclusion of namespaces for log analysis");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer config-file local-file include all-ns";
	cmd->cc_help_descr =        N_("Keyword to specify that catch-all T-Proxy namespace be included in log analysis");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/log_analyzer/config/config_file/url";
	cmd->cc_exec_value =        "local";
	cmd->cc_exec_type =         bt_string;
	cmd->cc_exec_name2 =         "/nkn/log_analyzer/config/config_file/all-ns";
	cmd->cc_exec_value2 =        "true";
	cmd->cc_exec_type2 =         bt_bool;
	cmd->cc_revmap_callback =    cli_log_analyzer_cfg_revmap;
	cmd->cc_revmap_order =     cro_log_analyzer;
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "log-analyzer config-file url * include all-ns";
	cmd->cc_help_descr =        N_("Keyword to specify that catch-all T-Proxy namespace be included in log analysis");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/log_analyzer/config/config_file/url";
	cmd->cc_exec_value =        "$1$";
	cmd->cc_exec_type =         bt_string;
	cmd->cc_exec_name2 =         "/nkn/log_analyzer/config/config_file/all-ns";
	cmd->cc_exec_value2 =        "true";
	cmd->cc_exec_type2 =         bt_bool;
	cmd->cc_revmap_type =       crt_none;
	cmd->cc_revmap_order =     cro_log_analyzer;
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "show log-analyzer";
	cmd->cc_help_descr =        N_("Module for analyzing the DPI log files");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "show log-analyzer target";
	cmd->cc_help_descr =        N_("Specify the target device for filter rules configuration");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
        cmd->cc_words_str =         "show log-analyzer target *";
        cmd->cc_help_exp =          N_("<device-map-name>");
        cmd->cc_help_exp_hint =     N_("Display the configured options of the log-analyzer for the attached device");
        cmd->cc_flags =             ccf_terminal;
	cmd->cc_help_use_comp =     true;
	cmd->cc_comp_pattern =      "/nkn/log_analyzer/config/device_map/*";
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_capab_required =    ccp_query_basic_curr;
        cmd->cc_exec_callback =     cli_log_analyzer_show;
        CLI_CMD_REGISTER
bail:
    return err;
}

int cli_clear_filter_rules(void *arg,
        const cli_command *cmd,
        const tstr_array  *cmd_line,
        const tstr_array  *params)
{
	int     err = 0;
	const char *device_map_name = NULL;
	node_name_t dname= {0};
	bn_binding *binding = NULL;
	
	UNUSED_ARGUMENT(arg);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	device_map_name = tstr_array_get_str_quick(params, 0);
	snprintf(dname, sizeof(dname),"/nkn/log_analyzer/config/device_map/%s", device_map_name);
	//Check if device-map-name exists
	err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding, dname, NULL);
	bail_error(err);
	if (!binding)
	{
		cli_printf_error(_("You need to bind the device-map %s to the log-analyzer first\n"), device_map_name);
                goto bail;
	}
	//lc_log_basic(LOG_NOTICE, _("the device_map_name is %s"), device_map_name);
	//lc_log_basic(LOG_NOTICE, _("THE NUMBER OF ARGS is %d"), tstr_array_length_quick(params));

	err = mdc_send_action_with_bindings_str_va(
		cli_mcc, NULL, NULL, "/nkn/log_analyzer/actions/clear_filter_rules", 1,
		"device_map_name", bt_string, device_map_name);
	bail_error(err);

 bail:
	bn_binding_free(&binding);
	return err;
}

//Function for fetching the MFC configuration file from a remote location
int cli_log_analyzer_fetchurl(void *data, const cli_command *cmd, const tstr_array  *cmd_line,
		const tstr_array  *params) {
	int err =0;
	const char *remote_url = NULL;
	bn_request *req = NULL;
	uint32 ret_err = 0;

	//Get the url specified by the user
	remote_url = tstr_array_get_str_quick(params, 0);
	bail_null(remote_url);

	if (clish_is_shell()) {
                err = clish_maybe_prompt_for_password_url(remote_url, cli_log_analyzer_download_file, cmd, cmd_line,
								params,data);
                bail_error(err);
        }
        else {
                err = cli_log_analyzer_download_file(NULL, cpr_ok, cmd, cmd_line, params, data, NULL);
                bail_error(err);
        }
        bail_error(err);
bail:
	return err;
}

static int
 cli_log_analyzer_cfg_revmap (void *data, const cli_command *cmd, const bn_binding_array *bindings, const char *name, const tstr_array *name_parts, const char *value, const bn_binding *binding, cli_revmap_reply *ret_reply) {
	int err = 0;
	tstring *all_ns_value = NULL;
	tstring *url_value = NULL;
	tstring *rev_cmd = NULL;
	UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(bindings);
        UNUSED_ARGUMENT(value);
        UNUSED_ARGUMENT(binding);
	
	err = bn_binding_array_get_value_tstr_by_name(bindings,"/nkn/log_analyzer/config/config_file/all-ns",NULL, &all_ns_value);
        bail_error(err);
	err = bn_binding_array_get_value_tstr_by_name(bindings,"/nkn/log_analyzer/config/config_file/url",NULL, &url_value);
        bail_error(err);

	//lc_log_basic(LOG_NOTICE, _("the url is %s"), ts_str(url_value));
	//lc_log_basic(LOG_NOTICE, _("the all-ns value is %s"), ts_str(all_ns_value));
	if (!strcasecmp(ts_str(url_value), "local") && !strcasecmp(ts_str(all_ns_value), "true")) {
		err = ts_new_str(&rev_cmd, "log-analyzer config-file local-file include all-ns");
                bail_error(err);
	}
	else if (strcasecmp(ts_str(url_value), "local") && !strcasecmp(ts_str(all_ns_value), "true")) {
		err = ts_new_sprintf(&rev_cmd, "log-analyzer config-file url \"%s\" include all-ns", ts_str(url_value));
                bail_error(err);
	}
	else if (!strcasecmp(ts_str(url_value), "local")) {
		err = ts_new_str(&rev_cmd, "log-analyzer config-file local-file");
                bail_error(err);
	}
	else if (strcasecmp(ts_str(url_value), "local")) {
		err = ts_new_sprintf(&rev_cmd, "log-analyzer config-file url \"%s\" ", ts_str(url_value));
                bail_error(err);
	}
	if (NULL != rev_cmd) {
                err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
                bail_error(err);
        }

	//Consume the nodes
	err = tstr_array_append_str(ret_reply->crr_nodes, "/nkn/log_analyzer/config/config_file/url");
	bail_error(err);
	err = tstr_array_append_str(ret_reply->crr_nodes, "/nkn/log_analyzer/config/config_file/all-ns");
	bail_error(err);
bail:
	ts_free(&rev_cmd);
	ts_free(&url_value);
	ts_free(&all_ns_value);
	return err;
}

int
cli_log_analyzer_show(void *data, const cli_command *cmd, const tstr_array *cmd_line, const tstr_array *params) {
	const char *device_map_name = NULL;
	node_name_t dname= {0};
	node_name_t mode_node= {0};
	bn_binding *binding = NULL;
	tstring *command_mode_value = NULL;
	int err= 0;

	UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(cmd_line);

	//Get the device map name
        device_map_name = tstr_array_get_str_quick(params, 0);
        bail_null(device_map_name);
	
	snprintf(dname, sizeof(dname),"/nkn/device_map/config/%s", device_map_name);

	//Check if device-map-name exists
	err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding, dname, NULL);
	bail_error(err);

	//If not then throw an error and return
	if (!binding)
	{
		cli_printf_error(_("Invalid device-map name : %s\n"), device_map_name);
                goto bail;
	}
	bn_binding_free(&binding);
	snprintf(dname, sizeof(dname),"/nkn/log_analyzer/config/device_map/%s", device_map_name);
	//Check if device-map-name exists
	err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding, dname, NULL);
	bail_error(err);
	if (!binding)
	{
		cli_printf_error(_("You need to bind the device-map %s to the log-analyzer first\n"), device_map_name);
                goto bail;
	}
	//Get the command-mode value
	snprintf(mode_node, sizeof(mode_node),"/nkn/log_analyzer/config/device_map/%s/command_mode/inline", device_map_name);
	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &command_mode_value, mode_node, NULL);
	bail_error(err);
	//Print the configured output for the specified device-map
	if (!strcasecmp(ts_str(command_mode_value), "true"))
		err = cli_printf("	command-mode	: inline\n");
	else
		err = cli_printf("	command-mode	: batch-file\n");
	//Query and print the confi-file option chosen
	err = cli_printf_query(_("	config-file	: " "#/nkn/log_analyzer/config/config_file/url#\n"));
	bail_error(err);
bail:
	bn_binding_free(&binding);
	ts_free(&command_mode_value);
	return err;
}

static int
cli_log_analyzer_download_file(const char *password, clish_password_result result, const cli_command *cmd,
			const tstr_array *cmd_line, const tstr_array *params, void *data, tbool *ret_continue) {

	int err = 0;
        const char *remote_url = NULL;
	bn_request *req = NULL;
	uint32 ret_err = 0;
	tstring *t_result = NULL;

	UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(cmd_line);
        UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(ret_continue);

        if(result != cpr_ok) {
                goto bail;
        }
	//Get the url specified by the user
	remote_url = tstr_array_get_str_quick(params, 0);
	bail_null(remote_url);
	
	//Create an action to download the file from the given url
	err = bn_action_request_msg_create(&req, "/file/transfer/download");
        bail_error(err);

	//Bind the given url to the action
	err = bn_action_request_msg_append_new_binding(req, 0, "remote_url", bt_string, remote_url, NULL);
        bail_error(err);

	//If password is specified bind it
	if (password) {
                err = bn_action_request_msg_append_new_binding(req, 0, "password", bt_string, password, NULL);
                bail_error(err);
        }
	//Specify the complete path into which to download the file
	err = bn_action_request_msg_append_new_binding(req, 0, "local_path", bt_string, "/config/nkn/dpi-analyzer/mfc_config.txt", NULL);

	//Allow the file to be overwritten
	err = bn_action_request_msg_append_new_binding(req, 0, "allow_overwrite", bt_bool, "true", NULL);
        bail_error(err);

	//Fire the action command
	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &ret_err, NULL);
	bail_error(err);

	bail_error(ret_err);
	
	/*Downlod successful. Call standard CLI processing - setting up nodes etc.. */
	err = cli_std_exec(data, cmd, cmd_line, params);
	bail_error(err);
bail:
	return err;
}
