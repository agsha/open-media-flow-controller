/*
 * Filename :   cli_nkn_file_cmds.c
 * Date     :   2010/07/15
 * Author   :   Ramalingam Chandran
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
#include "tpaths.h"
#include "file_utils.h"
#include "clish_module.h"
#include "proc_utils.h"
#include "nkn_defs.h"
/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int cli_nkn_file_cmds_init(const lc_dso_info *info,
                       const cli_module_context *context);

int cli_nkn_file_delete(void *arg,
                const cli_command *cmd,
                const tstr_array  *cmd_line,
                const tstr_array  *params);

int
cli_nkn_delete_all(void *arg,
	const cli_command *cmd,
	const tstr_array  *cmd_line,
	const tstr_array  *params);

int cli_nkn_file_cmds_init(const lc_dso_info *info,
                       const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;
   
    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str =             "file debug-dump delete all";
    cmd->cc_help_descr =            N_("Delete all the debug-dump files");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_action_basic_curr;
    cmd->cc_exec_callback =         cli_nkn_file_delete;
    CLI_CMD_REGISTER; 

    CLI_CMD_NEW;
    cmd->cc_words_str =             "file backtraces";
    cmd->cc_help_descr =            N_("Delete all the backtrace files");
    cmd->cc_flags =                 ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "file backtraces delete";
    cmd->cc_help_descr =            N_("Delete all the backtrace files");
    cmd->cc_flags =                 ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "file backtraces delete all";
    cmd->cc_help_descr =            N_("Delete all the backtrace files");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_action_rstr_curr;
    cmd->cc_exec_callback =         cli_nkn_delete_all;
    cmd->cc_exec_data =		    (void *)"/var/nkn/backtraces";
    CLI_CMD_REGISTER;
bail:
    return(err);
}

int
cli_nkn_delete_all(void *arg,
	const cli_command *cmd,
	const tstr_array  *cmd_line,
	const tstr_array  *params)
{
    int err = 0;
    const char *path = (const char *)arg;

    bail_null(path);

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = lf_remove_dir_contents(path);
    bail_error(err);

bail:
    return err;
}

int cli_nkn_file_delete(void *arg,
		const cli_command *cmd,
		const tstr_array  *cmd_line,
		const tstr_array  *params)
{
    int err = 0;
    int32 status = 0;
    tstring **output = NULL;

    UNUSED_ARGUMENT(arg);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = lc_launch_quick(&status, output, 1, "/opt/nkn/bin/dump_del.py");
    bail_error(err);

bail:
    return err;
}

