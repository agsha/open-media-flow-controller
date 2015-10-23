/*
 * Filename :   cli_nvsd_rtstream_media_cmds.c
 * Date     :   2009/08/18
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
#include "nkn_defs.h"


enum {
    cid_rtstream_media_add,
    cid_rtstream_media_delete
};

cli_expansion_option cli_nvsd_rts_media_libs[] = {
    {"libraw_udp.so.1.0.1",       N_("For Amino STB"), (void*) "libraw_udp.so.1.0.1"},
    {"libmpeg2ts_h221.so.1.0.1",  N_("For ZillionTV STB"), (void*) "libmpeg2ts_h221.so.1.0.1"},
    {"libmp4.so.1.0.1",           N_("MPEG-4 encode"), (void*) "libmp4.so.1.0.1"},
    {"librtsp.so.1.0.1",          N_("For MPEG-1, MPEG-2, MP3, WAV, etc."), (void*) "librtsp.so.1.0.1"},
    {"libankeena_fmt.so.1.0.1",   N_("Default library for any media-codecs"), (void*) "libankeena_fmt.so.1.0.1"},

    {NULL, NULL, NULL},
};

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
cli_nvsd_rtstream_media_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int cli_nvsd_rtstream_media_show_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int cli_nvsd_rts_media_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_rtstream_media_show_one(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);
/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
cli_nvsd_rtstream_media_cmds_init(
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
    cmd->cc_words_str =             "rt-stream";
    cmd->cc_help_descr =            N_("Configure rtstream protocol options");
    cmd->cc_flags =                 ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no rt-stream";
    cmd->cc_help_descr =            N_("Clear rt-stream protocol options");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "rt-stream media-encode";
    cmd->cc_help_descr =            N_("Configure media-encode option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no rt-stream media-encode";
    cmd->cc_help_descr =            N_("Clear media encoding options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "rt-stream media-encode *";
    cmd->cc_help_exp =              N_("<extension>");
    cmd->cc_help_exp_hint =         N_("File extension (.mpg, .ts, default, etc)");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "rt-stream media-encode * *";
    cmd->cc_help_exp =              N_("<library>");
    cmd->cc_help_options =          cli_nvsd_rts_media_libs;
    cmd->cc_help_num_options =      sizeof(cli_nvsd_rts_media_libs)/sizeof(cli_expansion_option);
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_callback =         cli_nvsd_rts_media_config;
    cmd->cc_magic =                 cid_rtstream_media_add;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no rt-stream media-encode *";
    cmd->cc_help_exp =              N_("<name>");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_values;
    cmd->cc_comp_pattern =          "/nkn/nvsd/rtstream/config/media-encode/*/extension";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_callback =         cli_nvsd_rts_media_config;
    cmd->cc_magic =                 cid_rtstream_media_delete;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show rt-stream";
    cmd->cc_help_descr =            N_("Display rt-stream configuration options");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_query_rstr_curr;
    cmd->cc_exec_callback =         cli_nvsd_rtstream_media_show_cmd;
    CLI_CMD_REGISTER;


    err = cli_revmap_ignore_bindings_va(1,
            "/nkn/nvsd/rtstream/config/media-encode/**");
    bail_error(err);


bail:
    return err;
}


/*--------------------------------------------------------------------------*/
int cli_nvsd_rtstream_media_show_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 num_matches = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = mdc_foreach_binding(cli_mcc, 
            "/nkn/nvsd/rtstream/config/media-encode/*", NULL,
            cli_nvsd_rtstream_media_show_one, NULL, &num_matches);
    bail_error(err);

    if ( num_matches == 0) {
        cli_printf(_("No Media encode mappings configured.\n"));
    }

bail :
    return err;
}

int cli_nvsd_rts_media_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

    switch (cmd->cc_magic) {
    case cid_rtstream_media_add:
        err = bn_binding_array_new(&barr);
        bail_error(err);

        err = bn_binding_new_str_autoinval(&binding, "extension", ba_value,
                bt_string, 0, tstr_array_get_str_quick(params, 0));
        bail_error(err);

        err = bn_binding_array_append_takeover(barr, &binding);
        bail_error(err);

        err = bn_binding_new_str_autoinval(&binding, "module", ba_value, 
                bt_string, 0, tstr_array_get_str_quick(params, 1));
        bail_error(err);

        err = bn_binding_array_append_takeover(barr, &binding);
        bail_error(err);

        err = mdc_array_append(cli_mcc, 
                "/nkn/nvsd/rtstream/config/media-encode", 
                barr, true, 0, NULL, NULL, &code, NULL);
        bail_error(err);
        bail_error(code);


        break;

    case cid_rtstream_media_delete:
        err = mdc_array_delete_by_value_single(cli_mcc, 
                "/nkn/nvsd/rtstream/config/media-encode", 
                true, "extension", bt_string, 
                tstr_array_get_str_quick(params, 0), NULL, NULL, NULL);
        bail_error(err);
        break;
    }


bail:
    bn_binding_array_free(&barr);
    bn_binding_free(&binding);
    return err;
}

int
cli_nvsd_rtstream_media_show_one(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    int err = 0;
    char *c_ext = NULL;
    char *c_lib = NULL;
    tstring *t_ext = NULL;
    tstring *t_lib = NULL;

    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    c_ext = smprintf("%s/extension", ts_str(name));
    bail_null(c_ext);

    c_lib = smprintf("%s/module", ts_str(name));
    bail_null(c_lib);

    err = bn_binding_array_get_value_tstr_by_name(bindings, c_ext, NULL, &t_ext);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, c_lib, NULL, &t_lib);
    bail_error(err);

    if (idx == 0) {
        cli_printf(_(
                    "  S.No.  Extension     Module\n"));
        cli_printf(_(
                    "---------------------------------\n"));
    }
    cli_printf(_(
                "   %3d.   %5s     %-10s\n"), idx+1, ts_str(t_ext), ts_str(t_lib));

bail:
   safe_free(c_ext);
   safe_free(c_lib);
   ts_free(&t_ext);
   ts_free(&t_lib);
    return err;
}

