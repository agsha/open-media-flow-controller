/*
 * Filename :   cli_adnsd_cmds.c
 * Date     :   23 May 2011
 * Author   :   Kiran Desai
 *
 * (C) Copyright 2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "libnkncli.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
cli_adnsd_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

static int cli_adnsd_show(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);

int
cli_adnsd_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =		"adns-daemon";
    cmd->cc_help_descr =	N_("Configure adns-daemon options");
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =		"show adns-daemon";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_help_descr = 	N_("Display adns-daemon configurations");
    cmd->cc_exec_callback =     cli_adnsd_show;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/

    const char *adnsd_ignore_bindings[] = {
	"/nkn/adnsd/config/*"
    };
    err = cli_revmap_ignore_bindings_arr(adnsd_ignore_bindings);

bail:
    return err;
}

static int cli_adnsd_show(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params)
{

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    return 0;
}
