/*
 * Filename:    cli_nkn_mfdlog_cmds.c
 * Date:        2009/01/09
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008-2009 Nokeena Networks Inc.
 * All rights reserved.
 */

#include "common.h"
#include "climod_common.h"
#include "nkn_defs.h"



int 
cli_nkn_mfdlog_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


int
cli_mfdlog_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


static cli_help_callback cli_interface_help_log = NULL;
static cli_completion_callback cli_interface_completion_log = NULL;

int (*cli_interface_validate)(const char *ifname, tbool *ret_valid) = NULL;

cli_expansion_option cli_mfdlog_port_opts[] = {
    {"7957", N_("Default Port number"), (void *) 7957},
    {NULL, NULL, NULL}
};

int 
cli_nkn_mfdlog_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;
    void *callback = NULL;

#ifdef PROD_FEATURE_I18N_SUPPORT
        /* This is pretty much redundant and can be skipped in your
         * modules, unless you want internationalization support.
         */
        err = cli_set_gettext_domain(GETTEXT_DOMAIN);
        bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }


    CLI_CMD_NEW;
    cmd->cc_words_str =             "mfdlog";
    cmd->cc_help_descr =            N_("Configure Access log and "
                                        "Error log port and interface");
    cmd->cc_flags =                 ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "mfdlog interface";
    cmd->cc_help_descr =            N_("Configure interface");
    CLI_CMD_REGISTER;


    /* Must get the help and completion callbacks from
     * cli_interface_cmd
     */

    err = lc_dso_module_get_symbol(info->ldi_context, 
            "cli_interface_cmds", "cli_interface_help", 
            &callback);
    bail_error_null(err, callback);

    cli_interface_help_log = (cli_help_callback)(callback);

    err = lc_dso_module_get_symbol(info->ldi_context, 
            "cli_interface_cmds", "cli_interface_completion", 
            &callback);
    bail_error_null(err, callback);
    cli_interface_completion_log = (cli_completion_callback)(callback);
    
    err = lc_dso_module_get_symbol(info->ldi_context, 
            "cli_interface_cmds", "cli_interface_validate", 
            &callback);
    bail_error_null(err, callback);
    cli_interface_validate = (callback);



    CLI_CMD_NEW;
    cmd->cc_words_str =             "mfdlog interface *";
    cmd->cc_help_exp =              N_("<ifname>");
    cmd->cc_help_exp_hint =         N_("interface name, for example eth0, eth1, etc.");
    cmd->cc_help_callback =         cli_interface_help_log;
    cmd->cc_comp_callback =         cli_interface_completion_log;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "mfdlog interface * tcp";
    cmd->cc_help_descr =            N_("Configure TCP options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "mfdlog interface * tcp port";
    cmd->cc_help_descr =            N_("Configure TCP port number");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "mfdlog interface * tcp port *";
    cmd->cc_help_exp =              N_("<port number>");
    cmd->cc_help_exp_hint =         N_("TCP port number");
    cmd->cc_help_options =          cli_mfdlog_port_opts;
    cmd->cc_help_num_options =      sizeof(cli_mfdlog_port_opts)/sizeof(cli_expansion_option);
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_callback =         cli_mfdlog_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "mfdlog field-length";
    cmd->cc_help_descr =            N_("Configure field lengths");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "mfdlog field-length *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("Field length, bytes");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/mfdlog/config/field-length";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint16;
    cmd->cc_revmap_order =          cro_mgmt;
    cmd->cc_revmap_type =           crt_auto;
    CLI_CMD_REGISTER;


bail:
    return err;
}

int
cli_mfdlog_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    tbool valid = false;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    err = cli_interface_validate(
            tstr_array_get_str_quick(params, 0),
            &valid);

    if (valid == false) {
        goto bail;
    }

    err = mdc_set_binding(cli_mcc, 
            NULL,
            NULL,
            bsso_modify,
            bt_string,
            tstr_array_get_str_quick(params, 0),
            "/nkn/mfdlog/config/interface");
    bail_error(err);



    err = mdc_set_binding(cli_mcc, 
            NULL,
            NULL,
            bsso_modify,
            bt_uint16,
            tstr_array_get_str_quick(params, 1),
            "/nkn/mfdlog/config/port");
    bail_error(err);

bail:
    return err;
}





















