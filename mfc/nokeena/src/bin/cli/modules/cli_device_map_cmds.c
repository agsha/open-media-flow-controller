/*
 * Filename :   cli_device_map_cmds.c
 *
 * (C) Copyright 2014 Juniper Networks.
 * All rights reserved.
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "proc_utils.h"
#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "libnkncli.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

/*----------------------------------------------------------------------------
 *  LOCAL FUNCTION DEFINITIONS
 * --------------------------------------------------------------------------*/
static int
 cli_device_map_cfg_revmap (void *data, const cli_command *cmd, const bn_binding_array *bindings, const char *name, const tstr_array *name_parts, const char *value, const bn_binding *binding, cli_revmap_reply *ret_reply);

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/

int cli_device_map_cmds_init(const lc_dso_info *info,
                      const cli_module_context *context);

int
cli_config_files_cmds_init(
	const lc_dso_info *info,
	const cli_module_context *context);

int
cli_device_map_show(void *data, const cli_command *cmd, const tstr_array *cmd_line, const tstr_array *params);

/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
cli_device_map_cmds_init(
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
	cmd->cc_words_str =         "device-map";
	cmd->cc_help_descr =        N_("Keyword to begin definition of a device that sends traffic for caching");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no device-map";
	cmd->cc_help_descr =        N_("Disable the device that sends traffic for caching");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show device-map";
	cmd->cc_help_descr =        N_("Display the configured setting for a particular device");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
        cmd->cc_words_str =         "show device-map *";
        cmd->cc_help_exp =          N_("<device-map-name>");
        cmd->cc_help_exp_hint =     N_("Display the configured options for the chosen device");
        cmd->cc_flags =             ccf_terminal;
	cmd->cc_help_use_comp =     true;
	cmd->cc_comp_pattern =      "/nkn/device_map/config/*";
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_capab_required =    ccp_query_basic_curr;
        cmd->cc_exec_callback =     cli_device_map_show;
        CLI_CMD_REGISTER
	
	CLI_CMD_NEW;
	cmd->cc_words_str =             "device-map *";
	cmd->cc_help_exp =              N_("<name>");
	cmd->cc_help_exp_hint =         N_("Up to 32-characters alphanumeric string to identify the device mappings");
	cmd->cc_help_use_comp =         true;
	cmd->cc_comp_type =         	cct_matching_names;
	cmd->cc_comp_pattern =      	"/nkn/device_map/config/*";
	cmd->cc_exec_name =             "/nkn/device_map/config/$1$";
	cmd->cc_flags =                 ccf_terminal | ccf_prefix_mode_root;
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_operation =        cxo_set;
	cmd->cc_exec_value =            "$1$";
	cmd->cc_exec_type =         	bt_string;
	cmd->cc_revmap_type =           crt_none;
	cmd->cc_revmap_order =     	cro_device_map;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =             "no device-map *";
	cmd->cc_help_exp =              N_("<name>");
	cmd->cc_help_exp_hint =         N_("Up to 32-characters alphanumeric string to identify the device mappings");
	cmd->cc_comp_type =         	cct_matching_names;
	cmd->cc_comp_pattern =      	"/nkn/device_map/config/*";
	cmd->cc_help_use_comp =         true;
	cmd->cc_exec_name =             "/nkn/device_map/config/$1$";
	cmd->cc_flags =                 ccf_terminal;
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_operation =        cxo_delete;
	cmd->cc_exec_value =            "$1$";
	cmd->cc_exec_type =         	bt_string;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "device-map * fqdn";
	cmd->cc_help_descr =        "IPadress or fully qualified domain name of the device to which filter rules will be applied";
	CLI_CMD_REGISTER; 
    
	CLI_CMD_NEW;
	cmd->cc_words_str =             "device-map * fqdn *";
	cmd->cc_help_exp =              N_("<fqdn>");
	cmd->cc_help_exp_hint =         N_("Ipaddress or fully qualified domain name");
	cmd->cc_exec_name =             "/nkn/device_map/config/$1$/device_info/fqdn";
	cmd->cc_flags =                 ccf_terminal;
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_operation =        cxo_set;
	cmd->cc_exec_value =            "$2$";
	cmd->cc_exec_type =         	bt_string;
	cmd->cc_revmap_type =           crt_auto;
	cmd->cc_revmap_order =     	cro_device_map;
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "device-map * username";
	cmd->cc_help_descr =        "Username to be used when filter configurations are done to the device";
	CLI_CMD_REGISTER; 
   
	CLI_CMD_NEW;
	cmd->cc_words_str =             "device-map * username *";
	cmd->cc_help_exp =              N_("<username>");
	cmd->cc_help_exp_hint =         N_("Specify username here");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "device-map * username * password";
	cmd->cc_help_descr =        "Password associated with the user name for the device where filter rules are configured";
	CLI_CMD_REGISTER; 
    
	CLI_CMD_NEW;
	cmd->cc_words_str =             "device-map * username * password *";
	cmd->cc_help_exp =              N_("<password>");
	cmd->cc_help_exp_hint =         N_("Specify password here");
	cmd->cc_exec_name =             "/nkn/device_map/config/$1$/device_info/username";
	cmd->cc_exec_value =            "$2$";
	cmd->cc_exec_type =             bt_string;
	cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_operation =        cxo_set;
	cmd->cc_exec_name2 =            "/nkn/device_map/config/$1$/device_info/password";
	cmd->cc_exec_value2 =           "$3$";
	cmd->cc_exec_type2 =         	bt_string;
	cmd->cc_revmap_callback =       cli_device_map_cfg_revmap;
	cmd->cc_revmap_names =      	"/nkn/device_map/config/*";
	cmd->cc_revmap_order =     	cro_device_map;
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "device-map * filter-configuration";
	cmd->cc_help_descr =        "Filter policy related configuration for this device";
	CLI_CMD_REGISTER; 
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "device-map * filter-configuration frequency";
	cmd->cc_help_descr =        "The frequency in which the policy will be sent to the device";
	CLI_CMD_REGISTER; 
	
	CLI_CMD_NEW;
	cmd->cc_words_str =             "device-map * filter-configuration frequency *";
	cmd->cc_help_exp =              N_("<minutes>");
	cmd->cc_help_exp_hint =         N_("Specify the frequency in minutes");
	cmd->cc_exec_name =             "/nkn/device_map/config/$1$/filter_config/frequency";
	cmd->cc_exec_value =            "$2$";
	cmd->cc_exec_type =             bt_uint32;
	cmd->cc_revmap_type =           crt_auto;
	cmd->cc_flags =                 ccf_terminal;
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_operation =        cxo_set;
	cmd->cc_revmap_order =     	cro_device_map;
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str =         "device-map * filter-configuration max-rules";
	cmd->cc_help_descr =        "Max number of unique policy rules that can be collected before it is fired to the router";
	CLI_CMD_REGISTER; 
	
	CLI_CMD_NEW;
	cmd->cc_words_str =             "device-map * filter-configuration max-rules *";
	cmd->cc_help_exp =              N_("<max_rules>");
	cmd->cc_help_exp_hint =         N_("Max number of filter rules that can be accumulated before it is fired to the device");
	cmd->cc_exec_name =             "/nkn/device_map/config/$1$/filter_config/max_rules";
	cmd->cc_exec_value =            "$2$";
	cmd->cc_exec_type =             bt_uint32;
	cmd->cc_revmap_type =           crt_auto;
	cmd->cc_flags =                 ccf_terminal;
	cmd->cc_capab_required =        ccp_set_rstr_curr;
	cmd->cc_exec_operation =        cxo_set;
	cmd->cc_revmap_order =     	cro_device_map;
	CLI_CMD_REGISTER;
bail:
    return err;
}

static int
 cli_device_map_cfg_revmap (void *data, const cli_command *cmd, const bn_binding_array *bindings, const char *name, const tstr_array *name_parts, const char *value, const bn_binding *binding, cli_revmap_reply *ret_reply) {
	int err = 0;
	tstring *username = NULL;
	tstring *password = NULL;
	tstring *rev_cmd = NULL;
	UNUSED_ARGUMENT(data);
        UNUSED_ARGUMENT(cmd);
        UNUSED_ARGUMENT(bindings);
        UNUSED_ARGUMENT(value);
        UNUSED_ARGUMENT(binding);

	bail_null(value);
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &username, "%s/device_info/username", name);
        bail_error(err);
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
                NULL, &password, "%s/device_info/password", name);
        bail_error(err);
	if (strlen(value) > 1) {
		err = ts_new_sprintf(&rev_cmd, "# device-map %s username \"%s\" password \"%s\"",
                                value, ts_str(username), ts_str(password));
                bail_error(err);
	}
	if (NULL != rev_cmd) {
                err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
                bail_error(err);
        }
	//Consume the nodes
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
	bail_error(err);
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/device_info/username", name);
	bail_error(err);
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/device_info/password", name);
	bail_error(err);
bail:
	ts_free(&rev_cmd);
	ts_free(&username);
	ts_free(&password);
	return err;
}

int
cli_device_map_show(void *data, const cli_command *cmd, const tstr_array *cmd_line, const tstr_array *params) {
	const char *device_map_name = NULL;
	node_name_t dname= {0};
	bn_binding *binding = NULL;
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
	
	//Print the configured output for the specified device-map
	err = cli_printf_query(_("	fqdn		: " "#/nkn/device_map/config/%s/device_info/fqdn#\n"), device_map_name);
	err = cli_printf_query(_("	username	: " "#/nkn/device_map/config/%s/device_info/username#\n"), device_map_name);
	err = cli_printf_query(_("	freqency	: " "#/nkn/device_map/config/%s/filter_config/frequency#\n"), device_map_name);
	err = cli_printf_query(_("	max-rules	: " "#/nkn/device_map/config/%s/filter_config/max_rules#\n"), device_map_name);
	bail_error(err);
bail:
	bn_binding_free(&binding);
	return err;
}
