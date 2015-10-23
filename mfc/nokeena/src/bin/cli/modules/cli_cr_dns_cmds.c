/*
 * Filename:    cli_cr_dns_cmds.c
 * Date:        2012/4/16
 * Author:      Manikandan V
 *
 * (C) Copyright 2012,  Juniper Networks Inc.
 *  All rights reserved.
 */

#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "cli_module.h"

int 
cli_cr_dns_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

static int
cli_cr_service_cmds(const lc_dso_info *info,
                    const cli_module_context *context);

int
cli_cr_dns_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;
    cli_command *cmd = NULL;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns";
    cmd->cc_help_descr =        N_("Configure cr-dns params");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns listen";
    cmd->cc_help_descr =        N_("Configure cr-dns listen port");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns listen *";
    cmd->cc_help_exp =        N_("<Port Number>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns/config/listen/port";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns tcp";
    cmd->cc_help_descr =        N_("Configure tcp params for cr-dns");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns tcp timeout";
    cmd->cc_help_descr =        N_("Configure tcp timeout for cr-dns");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns tcp timeout *";
    cmd->cc_help_exp =          N_("<Timeout value>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns/config/tcp/timeout";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns tx-threads";
    cmd->cc_help_descr =        N_("Configure no.of transmit threads for cr-dns");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns tx-threads *";
    cmd->cc_help_exp =          N_("<No.of tx threads>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns/config/tx/threads";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns rx-threads";
    cmd->cc_help_descr =        N_("Configure no.of receiving threads for cr-dns");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns rx-threads *";
    cmd->cc_help_exp =          N_("<No.of rx threads>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns/config/rx/threads";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns debug-assert";
    cmd->cc_help_descr =        N_("Configure debug options for cr-dns");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns debug-assert enable";
    cmd->cc_help_descr =        N_("Enable the debug assert for cr-dns");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns/config/debug_assert";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns debug-assert disable";
    cmd->cc_help_descr =        N_("Disable the debug assert for cr-dns");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/cr_dns/config/debug_assert";
    CLI_CMD_REGISTER;

    err = cli_cr_service_cmds(info, context);
    bail_error(err);

bail:

	return err;
}

static int
cli_cr_service_cmds(const lc_dso_info *info,
                    const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

/* To be un-commented once a seperate production environment is ready
    CLI_CMD_NEW;
    cmd->cc_words_str	   = "service";
    cmd->cc_help_descr     = N_("service management");
    CLI_CMD_REGISTER;
*/

    CLI_CMD_NEW;
    cmd->cc_words_str	   = "service restart";
    cmd->cc_help_descr     = N_("restart a service");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	   = "service stop";
    cmd->cc_help_descr     = N_("terminate a service");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-dnsd";
    cmd->cc_help_descr     = N_("Stop and Start the cr-dnsd service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/restart_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "dnsd";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service stop mod-dnsd" ;
    cmd->cc_help_descr     = N_("Stop the cr-dnsd service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/terminate_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "dnsd";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service restart mod-stored";
    cmd->cc_help_descr     = N_("Stop and Start the cr-stored service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/restart_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "dns_stored";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "service stop mod-stored";
    cmd->cc_help_descr     = N_("Stop the cr-stored service");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_rstr_curr;
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name     = "/pm/actions/terminate_process";
    cmd->cc_exec_name      = "process_name";
    cmd->cc_exec_type      = bt_string;
    cmd->cc_exec_value     = "dns_stored";
    CLI_CMD_REGISTER;


bail:
    return err;
}
