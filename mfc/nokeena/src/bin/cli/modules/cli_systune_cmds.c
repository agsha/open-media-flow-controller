
/*
 *
 * Filename:  cli_systune_cmds.c
 * Date:      2011/06/28
 * Author:    Dhruva Narasimhan
 *
 * (C) Copyright 2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include "common.h"
#include <climod_common.h>
#include <dso.h>
#include <env_utils.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tstring.h"
#include "random.h"
#include "cli_module.h"
#include "clish_module.h"
#include "file_utils.h"
#include "proc_utils.h"
#include "libnkncli.h"




int cli_systune_cmds_init(const lc_dso_info *info,
			const cli_module_context *context);

int cli_systune_drop_cache_cmd_init(const lc_dso_info *info,
				const cli_module_context *context);
int cli_systune_show(void *data,
		    const cli_command *cmd,
		    const tstr_array *cmd_line,
		    const tstr_array *params);


int cli_systune_cmds_init(const lc_dso_info *info,
			const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    if (context->cmc_mgmt_avail == false) {
	goto bail;
    }

    CLI_CMD_NEW;
    cmd->cc_words_str		= "module";
    cmd->cc_help_descr		= N_("Configure and/or tune modules "
				    "that control system behavior");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str		= "no module";
    cmd->cc_help_descr		= N_("Reset configuration that "
				    "control system behavior");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str		= "show module";
    cmd->cc_help_descr		= N_("Display module specific tuning parameters");
    cmd->cc_flags		= ccf_terminal;
    cmd->cc_capab_required	= ccp_query_basic_curr;
    cmd->cc_exec_callback	= cli_systune_show;
    CLI_CMD_REGISTER;

    err = cli_systune_drop_cache_cmd_init(info, context);
    bail_error(err);



bail:
    return err;
}

int cli_systune_drop_cache_cmd_init(const lc_dso_info *info,
				const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    (void) info;
    (void) context;

    CLI_CMD_NEW;
    cmd->cc_words_str		= "module drop-cache";
    cmd->cc_help_descr		= N_("Module that handles free buffers in kernel");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str		= "no module drop-cache";
    cmd->cc_help_descr		= N_("Reset configuration of module that "
				    "handles free buffers in kernel");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str		= "module drop-cache free-buffer";
    cmd->cc_help_descr		= N_("Direct module to look at free "
				    "buffers in kernel");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str		= "no module drop-cache free-buffer";
    cmd->cc_help_descr		= N_("Reset configuration of module to look "
				    "at free buffers in kernel");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str		= "module drop-cache free-buffer threshold";
    cmd->cc_help_descr		= N_("Specify the free-buffer threshold "
				    "in percentage");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str		= "no module drop-cache free-buffer threshold";
    cmd->cc_help_descr		= N_("Reset the threshold to default");
    cmd->cc_flags		= ccf_terminal;
    cmd->cc_capab_required	= ccp_set_rstr_curr;
    cmd->cc_exec_operation	= cxo_reset;
    cmd->cc_exec_name		= "/pm/nokeena/drop_cache/config/threshold";
    cmd->cc_exec_value		= "5";
    cmd->cc_exec_type		= bt_uint8;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str		= "module drop-cache free-buffer threshold *";
    cmd->cc_help_exp		= N_("<percent>");
    cmd->cc_help_exp_hint	= N_("Choose a value from 1 to 100, for "
				    "the percentage of free buffers");
    cmd->cc_flags		= ccf_terminal;
    cmd->cc_capab_required	= ccp_set_priv_curr;
    cmd->cc_exec_operation	= cxo_set;
    cmd->cc_exec_name		= "/pm/nokeena/drop_cache/config/threshold";
    cmd->cc_exec_value		= "$1$";
    cmd->cc_exec_type		= bt_uint8;
    cmd->cc_revmap_order	= cro_systune;
    cmd->cc_revmap_type		= crt_auto;
    CLI_CMD_REGISTER;

bail:
    return err;
}


int cli_systune_show(void *data,
		    const cli_command *cmd,
		    const tstr_array *cmd_line,
		    const tstr_array *params)
{
    int err = 0;

    (void) data;
    (void) cmd;
    (void) cmd_line;
    (void) params;

    err = cli_printf(_("System Tuning Paramaters\n"));
    bail_error(err);

    err = cli_printf_query(_("  Drop-Cache Threshold        : "
		"#/pm/nokeena/drop_cache/config/threshold# %%\n"));
    bail_error(err);

bail:
    return 0;
}
