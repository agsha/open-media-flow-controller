/*
 * Filename :   cli_nkn_if_cmds.c
 * Date     :   2009/05/21
 * Author   :   Dhruva Narasimhan
 *
 * (C) Copyright 2008-2009 Nokeena Networks, Inc.
 * All rights reserved.
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "tpaths.h"
#include "nkn_defs.h"

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int 
cli_nkn_if_cmds_init(
        const lc_dso_info   *info,
        const cli_module_context *context);


int 
cli_nkn_if_arp_enable(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nkn_if_set_txqueuelen(void *arg,
		const cli_command *cmd,
		const tstr_array  *cmd_line,
		const tstr_array  *params);

int 
cli_nkn_if_cmds_init(
        const lc_dso_info   *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str =         "interface * arp";
    cmd->cc_help_descr =        N_("Configure ARP broadcast options for this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "interface * arp enable";
    cmd->cc_help_descr =        N_("Enable ARP broadcast on this interface");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/net/interface/config/$1$/arp";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_value =        "true";
    cmd->cc_revmap_order =      cro_interface;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_suborder =   1000;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "interface * arp disable";
    cmd->cc_help_descr =        N_("Disable ARP broadcast on this interface");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/net/interface/config/$1$/arp";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_value =        "false";
    cmd->cc_revmap_order =      cro_interface;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_suborder =   1000;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "interface * arp announce";
    cmd->cc_help_descr =        N_("Settings for ARP requests");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "interface * arp announce source-ip";
    cmd->cc_help_descr =        N_("Setting for source IP address in ARP requests");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "interface * arp ignore";
    cmd->cc_help_descr =        N_("Settings for handling ARP requests");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str = 	"interface * arp ignore all";
    cmd->cc_help_descr = 	N_("Ignore all ARP requests");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name  =	"/net/interface/config/$1$/arp/ignore";
    cmd->cc_exec_type  =	bt_uint32;
    cmd->cc_exec_value  =	"8";
    cmd->cc_revmap_order =      cro_interface;
    cmd->cc_revmap_type = 	crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"interface * arp ignore no-ip-match";
    cmd->cc_help_descr = 	N_("Ignore ARP requests, if incoming interface IP address does not match");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name  =	"/net/interface/config/$1$/arp/ignore";
    cmd->cc_exec_type  =	bt_uint32;
    cmd->cc_exec_value  =	"1";
    cmd->cc_revmap_order =      cro_interface;
    cmd->cc_revmap_type = 	crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"interface * arp ignore no-match";
    cmd->cc_help_descr = 	N_("Ignore ARP requests, if the incoming interface IP or sender's IP subnet does not match");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name  =	"/net/interface/config/$1$/arp/ignore";
    cmd->cc_exec_type  =	bt_uint32;
    cmd->cc_exec_value  =	"2";
    cmd->cc_revmap_order =      cro_interface;
    cmd->cc_revmap_type = 	crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"interface * arp ignore none";
    cmd->cc_help_descr = 	N_("Respond to ARP requests for any IP address on any interface (default)");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name  =	"/net/interface/config/$1$/arp/ignore";
    cmd->cc_exec_type  =	bt_uint32;
    cmd->cc_exec_value  =	"0";
    cmd->cc_revmap_order =      cro_interface;
    cmd->cc_revmap_type = 	crt_none;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings("/net/interface/config/*/arp/ignore");
    bail_error(err);

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"interface * arp announce source-ip any";
    cmd->cc_help_descr = 	N_("Send ARP request using source IP from any configured interface (default)");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name  =	"/net/interface/config/$1$/arp/announce";
    cmd->cc_exec_type  =	bt_uint32;
    cmd->cc_exec_value  =	"0";
    cmd->cc_revmap_order =      cro_interface;
    cmd->cc_revmap_type = 	crt_none;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings("/net/interface/config/*/arp/announce");
    bail_error(err);
    
    CLI_CMD_NEW;
    cmd->cc_words_str = 	"interface * arp announce source-ip best";
    cmd->cc_help_descr = 	N_("Send ARP request using source IP from the best matching interface");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name  =	"/net/interface/config/$1$/arp/announce";
    cmd->cc_exec_type  =	bt_uint32;
    cmd->cc_exec_value  =	"2";
    cmd->cc_revmap_order =      cro_interface;
    cmd->cc_revmap_type = 	crt_auto;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str = 	"interface * txqueuelen";
    cmd->cc_help_descr = 	N_("Configure Transmit queue length for this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"interface * txqueuelen *";
    cmd->cc_help_exp_hint = 	N_("Set a number from 1000 to 4096");
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nkn_if_set_txqueuelen;
    cmd->cc_revmap_type = 	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"interface * identify";
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_help_descr = 	N_("Set time-interval for blinking( in seconds)");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required =	ccp_action_rstr_curr;
    cmd->cc_exec_operation =	cxo_action;
    cmd->cc_exec_action_name =	"/nkn/nvsd/interface/actions/blink-interface";
    cmd->cc_exec_name  =	"interface";
    cmd->cc_exec_type  =	bt_string;
    cmd->cc_exec_value  =	"$1$";
    cmd->cc_exec_name2  =	"time";
    cmd->cc_exec_type2  =	bt_uint32;
    cmd->cc_exec_value2  =	"5";
    cmd->cc_revmap_type = 	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"interface * identify *";
    cmd->cc_help_exp_hint = 	N_("Set time-interval for blinking( in seconds)");
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required =	ccp_action_rstr_curr;
    cmd->cc_exec_operation =	cxo_action;
    cmd->cc_exec_action_name =	"/nkn/nvsd/interface/actions/blink-interface";
    cmd->cc_exec_name  =	"interface";
    cmd->cc_exec_type  =	bt_string;
    cmd->cc_exec_value  =	"$1$";
    cmd->cc_exec_name2  =	"time";
    cmd->cc_exec_type2  =	bt_uint32;
    cmd->cc_exec_value2  =	"$2$";
    cmd->cc_revmap_type = 	crt_none;
    CLI_CMD_REGISTER;




bail:
    return err;
}

int 
cli_nkn_if_set_txqueuelen(void *arg,
		const cli_command *cmd,
		const tstr_array  *cmd_line,
		const tstr_array  *params)
{
	int err = 0;
	int32 status = 0;

	UNUSED_ARGUMENT(arg);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	err = lc_launch_quick_status(&status, NULL, false, 4,
			"/sbin/ifconfig",
			tstr_array_get_str_quick(params, 0),
			"txqueuelen",
			tstr_array_get_str_quick(params, 1));
	bail_error(err);

	if (WEXITSTATUS(status) != 0) {
		cli_printf_error(_("error: couldnt set the interface txqueuelen"));
	}
bail:
	return err;
}
