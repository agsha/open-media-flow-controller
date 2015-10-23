/*
 * Filename :   cli_nvsd_tfm_cmds.c
 * Date     :   2009/05/01
 * Author   :   Dhruva Narasimhan
 *
 * (C) Copyright 2008,2009 Nokeena Networks, Inc.
 * All rights reserved.
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "climod_common.h"
#include "nkn_defs.h"


int
cli_nvsd_tfm_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_nvsd_tfm_show_cmd(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);




int
cli_nvsd_tfm_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = cli_set_gettext_domain(GETTEXT_DOMAIN);
    bail_error(err);
#endif

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }

    CLI_CMD_NEW;
    cmd->cc_words_str =         "temp-file-cache";
    cmd->cc_help_descr =        N_("Configure temporary file manager (TFM) options");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "temp-file-cache max-files";
    cmd->cc_help_descr =        N_("Configure maximum number of files that can be stored in TFM");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "temp-file-cache max-files *";
    cmd->cc_help_exp =          N_("<number of files>");
    cmd->cc_help_exp_hint =     N_("Maximum number of files to store. (Default - 25)");
    //cmd->cc_flags =             ccf_terminal;
    //cmd->cc_capab_required =    ccp_set_rstr_curr;
    //cmd->cc_exec_operation =    cxo_set;
    //cmd->cc_revmap_type =       crt_auto;
    //cmd->cc_revmap_order =      cro_namespace;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "temp-file-cache max-files * partial-file";
    cmd->cc_help_descr =        N_("Configure partial file options, such as eviction time, etc.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "temp-file-cache max-files * partial-file timeout";
    cmd->cc_help_descr =        N_("Configure partial file eviction time");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "temp-file-cache max-files * partial-file timeout *";
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_help_exp_hint =     N_("Time in seconds for eviction of partial files");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/tfm/config/partial_file/evict_time";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_name2 =         "/nkn/nvsd/tfm/config/max_num_files";
    cmd->cc_exec_type2 =         bt_uint32;
    cmd->cc_exec_value2 =        "$1$";
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "show temp-file-cache";
    cmd->cc_help_descr =        N_("Display TFM configuration");
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_query_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_tfm_show_cmd;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "temp-file-cache max-file-size";
    cmd->cc_help_descr =        N_("Configure maximum number of files that can be stored in TFM");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "temp-file-cache max-file-size *";
    cmd->cc_help_exp =          N_("<bytes>");
    cmd->cc_help_exp_hint =     N_("Maximum file size. (Default - 10000000)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name = 	"/nkn/nvsd/tfm/config/max_file_size";
    cmd->cc_exec_value = 	"$1$";
    cmd->cc_exec_type = 	bt_int32;
    CLI_CMD_REGISTER;



    err = cli_revmap_ignore_bindings_va(4, "/nkn/nvsd/fmgr/config/*",
            "/nkn/nvsd/tfm/config/max_num_files",
            "/nkn/nvsd/tfm/config/max_file_size",
            "/nkn/nvsd/tfm/config/partial_file/evict_time");
    bail_error(err);

bail:
    return err;
}

int
cli_nvsd_tfm_show_cmd(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params)
{
    int err = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_printf_query(_("Number of files that can be stored in TFM : "
                "#/nkn/nvsd/tfm/config/max_num_files#\n"));
    bail_error(err);

    err = cli_printf_query(_("Time for eviction of partial files        : "
                "#/nkn/nvsd/tfm/config/partial_file/evict_time# seconds"));
    bail_error(err);

bail:
    return err;
}

