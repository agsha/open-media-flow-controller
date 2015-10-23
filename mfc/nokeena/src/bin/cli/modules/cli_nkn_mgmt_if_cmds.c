/*
 * Filename :   cli_nkn_mgmt_if_cmds.c
 * Date     :   2010/02/04
 * Author   :   Manikandan Vengatachalam 
 *
 * (C) Copyright 2008-2010 Nokeena Networks, Inc.
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
cli_nkn_mgmt_if_cmds_init(
        const lc_dso_info   *info,
        const cli_module_context *context);

static int
cli_print_restart_msg(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);

extern int
cli_std_exec(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params);

int cli_mgmt_show(void *data, const cli_command *cmd,
	const tstr_array *cmd_line, const tstr_array *params);

/* ------------------------------------------------------------------------ */
int
cli_print_restart_msg(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;

    err = cli_std_exec(data, cmd, cmd_line, params);
    bail_error(err);

    /* Print the restart message */
    err = cli_printf("WARNING: if command successful, please reboot the SYSTEM, for the interface change to take effect\n");
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------ */
int 
cli_nkn_mgmt_if_cmds_init(
        const lc_dso_info   *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str =         "management";
    cmd->cc_help_descr =        N_("Configure the Management Interface ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no management";
    cmd->cc_help_descr =        N_("Remove/Clear Management Interface specific options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "management interface";
    cmd->cc_help_descr =        N_("Configure the Management Interface ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "management interface name";
    cmd->cc_help_descr =        N_("Set an interface as management interface(ex.,eth0)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"management interface *";
    cmd->cc_help_exp_hint = 	N_("Set a mac-address for eth0");
    cmd->cc_help_exp = 		N_("<aa:bb:cc:dd:ee:ff>");
    cmd->cc_help_use_comp = 	true;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_comp_type =         cct_matching_values;
    cmd->cc_comp_pattern =      "/net/interface/state/*/hwaddr";
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_exec_type =         bt_string;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/mgmt-if/config/interface";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_callback  =    cli_print_restart_msg;
    cmd->cc_revmap_type = 	crt_none;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings("/nkn/mgmt-if/config/interface");
    bail_error(err);

    CLI_CMD_NEW;
    cmd->cc_words_str =         "management interface name *";
    cmd->cc_help_exp_hint =     N_("Interface name(ex.,eth0)");
    cmd->cc_help_exp =          N_("<interface-name>");
    cmd->cc_help_use_comp =     true;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_comp_type =         cct_matching_values;
    cmd->cc_comp_pattern =      "/net/interface/config/*";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_type  =        bt_string;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/mgmt-if/config/if-name";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"show management";
    cmd->cc_help_descr =	N_("Show management IP options");
    CLI_CMD_REGISTER;

bail:
    return err;
}

int cli_mgmt_show(void *data, const cli_command *cmd,
	const tstr_array *cmd_line, const tstr_array *params)
{
    int err = 0;
bail:
    return err;
}

/* End of cli_nkn_mgmt_if_cmds.c */
