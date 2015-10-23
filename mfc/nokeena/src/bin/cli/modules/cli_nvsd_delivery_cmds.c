/*
 * Filename :   cli_nvsd_delivery_cmds.c
 * Date     :   2009/01/16
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

cli_expansion_option cli_delivery_proto_opts[] = {
    {"http", N_("Configure HTTP protocol options"), (void*) "http"},
    {"rtmp", N_("Configure RTMP protocol options"), (void*) "rtmp"},
    {"rtsp", N_("Configure RTSP protocol options"), (void*) "rtsp"},
    {"rtp", N_("Configure RTP protocol options"), (void*) "rtp"},

    {NULL, NULL, NULL},
};

int
cli_nvsd_delivery_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

#if 0
int cli_nvsd_delivery_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

#endif
int
cli_nvsd_delivery_cmds_init(
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
    cmd->cc_words_str =             "delivery";
    cmd->cc_help_descr =            N_("Configure delivery protocol options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery";
    cmd->cc_help_descr =            N_("Clear protocol options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol";
    cmd->cc_help_descr =            N_("Configure delivery protocol options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol";
    cmd->cc_help_descr =            N_("Clear delivery protocol options");
    CLI_CMD_REGISTER;


#if 0 
    /****************************************************************
     *      THIS SECTION MOVED TO cli_nvsd_http_cmds.c
     ***************************************************************/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http";
    cmd->cc_help_descr =            N_("Configure HTTP protocol options");
    //cmd->cc_help_exp =              N_("Protocol");
    //cmd->cc_help_exp_hint =         N_("Protocol options");
    //cmd->cc_help_options =          cli_delivery_proto_opts;
    //cmd->cc_help_num_options =      sizeof(cli_delivery_proto_opts)/
                                        //sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http";
    cmd->cc_help_descr =            N_("Clear HTTP protocol options");
    //cmd->cc_help_exp =              N_("Protocol");
    //cmd->cc_help_exp_hint =         N_("Protocol options");
    //cmd->cc_help_options =          cli_delivery_proto_opts;
    //cmd->cc_help_num_options =      sizeof(cli_delivery_proto_opts)/
                                        //sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http";
    cmd->cc_help_descr =            N_("Configure delivery protocol options");
    cmd->cc_flags =                 ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_callback =         cli_nvsd_delivery_enter_prefix_mode;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http";
    cmd->cc_help_descr =            N_("Clear delivery protocol options");
    //cmd->cc_flags =                 ccf_terminal; 
    //cmd->cc_capab_required =        ccp_set_rstr_curr;
    //cmd->cc_exec_callback =         cli_proto_enter_prefix_mode;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http no";
    cmd->cc_help_descr =            N_("Disable/Negate protocol options");
    cmd->cc_req_prefix_count =      3;
    CLI_CMD_REGISTER;
#endif

bail:
    return err;
}

#if 0
int cli_nvsd_delivery_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;

    if(cli_prefix_modes_is_enabled()) {
        err = cli_prefix_enter_mode(cmd, cmd_line);
        bail_error(err);
    }
    else {
        err = cli_print_incomplete_command_error(cmd, cmd_line);
        bail_error(err);
    }
bail:
    return err;

}
#endif
