/*
 * Filename :   cli_nvsd_debug_cmds.c
 * Date     :   2008/12/02
 * Author   :   Dhruva Narasimhan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "climod_common.h"
#include "nkn_defs.h"



/*----------------------------------------------------------------------------
 * TYPEDEF DECLARATIONS
 *---------------------------------------------------------------------------*/
typedef enum {
    dbgopt_verbose = 1,
    dbgopt_packet,
    dbgopt_trace,
    dbgopt_internal,

} debug_options;

typedef enum {
    dbgmod_network      = 0x00000001,
    dbgmod_http         = 0x00000002,
    dbgmod_dm           = 0x00002004,
    dbgmod_scheduler    = 0x00000040,
    dbgmod_filecache    = 0x00000838,
    dbgmod_proxy        = 0x00001000,
    dbgmod_ssp          = 0x00000080,
    dbgmod_ftp,
    dbgmod_accesslog,
    dbgmod_errorlog,
    dbgmod_am,

} debug_module;


/*----------------------------------------------------------------------------
 * VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
cli_expansion_option cli_nvsd_debug_module_opts[] = {
    {"http",        N_("HTTP debug options"),       (void*) dbgmod_http},
    {"scheduler",   N_("Scheduler debug options"),  (void*) dbgmod_scheduler},
    {"am",          N_("Analytics debug options"),  (void*) dbgmod_am},
    {"file-cache",  N_("File-cache debug options"), (void*) dbgmod_filecache},
    {"ftp",         N_("FTP debug options"),        (void*) dbgmod_ftp},
    {"accesslog",   N_("Access Log debug options"), (void*) dbgmod_accesslog},
    {"errorlog",    N_("Error Log debug options"),  (void*) dbgmod_errorlog},
    {"network",     N_("Network debug options"),    (void*) dbgmod_network},
    {"dm",          N_("Disk Manager debug options"), (void*) dbgmod_dm},
    {"proxy",       N_("Proxy debug options"),      (void*) dbgmod_proxy},
    {"ssp",         N_("SSP debug options"),        (void*) dbgmod_ssp},
    {NULL, NULL, NULL}
};

cli_expansion_option cli_nvsd_debug_level_opts[] = {
    {"verbose", N_("Log a lot of debug messages."), (void *) dbgopt_verbose},
    {"packets", N_("Packet level debug messages"),  (void *) dbgopt_packet},
    {"trace",   N_("Log trace messages"),           (void *) dbgopt_trace},
    {"internal",N_("Log internal messages"),        (void *) dbgopt_internal},
    {NULL, NULL, NULL}
};


/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
cli_nvsd_debug_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


int cli_nvsd_debug_options_exec(
        void  *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


/*----------------------------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static int
cli_nvsd_debug_extract_flags(
        const char *flag_str, 
        cli_expansion_option options[],
        uint32 *ret_flags, tbool *ret_flags_ok);


/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
cli_nvsd_debug_cmds_init(
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
#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =     "debug";
    cmd->cc_help_descr =    N_("Set debug options and levels");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =     "debug *";
    cmd->cc_help_exp =      N_("<module>");
    cmd->cc_help_options =  cli_nvsd_debug_module_opts;
    cmd->cc_help_exp_hint = N_("Choose one of the modules below");
    cmd->cc_help_num_options = 
        sizeof(cli_nvsd_debug_module_opts)/sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =     "debug * *";
    cmd->cc_flags =         ccf_terminal;
    cmd->cc_help_exp =      N_("<level>");
    cmd->cc_help_exp_hint = N_("One of the levels below");
    cmd->cc_help_options = cli_nvsd_debug_level_opts;
    cmd->cc_help_num_options = 
        sizeof(cli_nvsd_debug_level_opts)/sizeof(cli_expansion_option);
    cmd->cc_exec_callback = cli_nvsd_debug_options_exec;
    CLI_CMD_REGISTER;
#endif /* 0 */

    CLI_CMD_NEW;
    cmd->cc_words_str =     	    "assert";
    cmd->cc_flags =                 ccf_hidden;
    cmd->cc_help_descr =            N_("Enable soft assertion");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "assert enable";
    cmd->cc_help_descr =            N_("Enable soft assertion");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/debug/config/assert/enabled";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no assert";
    cmd->cc_help_descr =            N_("Disable soft assertion");
    cmd->cc_flags =                 ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no assert enable";
    cmd->cc_help_descr =            N_("Disable soft assertion");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/debug/config/assert/enabled";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/

    const char *debug_bindings[] = {
	"/nkn/debug/log/ps/**",
	"/nkn/debug/log/all/level",
	"/nkn/nvsd/debug/config/assert/enabled"
    };
    err = cli_revmap_ignore_bindings_arr(debug_bindings);
    bail_error(err);
bail:
    return err;
}


/*----------------------------------------------------------------------------
 * Callback function when the debug command is executed.
 *  - Scan the command line parameters on what was given, convert to 
 *  their enum equivalents
 *  - Big switch case that should set the config nodes correctly.
 *--------------------------------------------------------------------------*/
int cli_nvsd_debug_options_exec(
        void  *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *module_str = NULL,
          *level_str = NULL;
    uint32 module_flag = 0, level_flag = 0;
    tbool flag_ok = false;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    module_str = tstr_array_get_str_quick(params, 0);
    level_str = tstr_array_get_str_quick(params, 1);


    err = cli_nvsd_debug_extract_flags(module_str,
            cli_nvsd_debug_module_opts,
            &module_flag,
            &flag_ok);
    bail_error(err);
    if (!flag_ok) {
        goto bail;
    }

    flag_ok = false;
    err = cli_nvsd_debug_extract_flags(level_str,
            cli_nvsd_debug_level_opts,
            &level_flag,
            &flag_ok);
    bail_error(err);
    if (!flag_ok) {
        goto bail;
    }

#if 0
    lc_log_basic(LOG_NOTICE, "module: %s (%d), level: %s (%d)", 
            module_str, module_flag, level_str, level_flag);
#endif
    switch(module_flag)
    {
    case dbgmod_http:
        /* Set the config node here */
        break;
    case dbgmod_scheduler:
        /* Set the config node here */
        break;
    case dbgmod_am:
        /* Set the config node here */
        break;
    case dbgmod_filecache:
        /* Set the config node here */
        break;
    case dbgmod_ftp:
        /* Set the config node here */
        break;
    case dbgmod_accesslog:
        /* Set the config node here */
        break;
    case dbgmod_errorlog:
        /* Set the config node here */
        break;
    case dbgmod_network:
        /* Set the config node here */
        break;
    case dbgmod_dm:
        /* Set the config node here */
        break;
    case dbgmod_proxy:
        /* Set the config node here */
        break;
    case dbgmod_ssp:
        /* Set the config node here */
        break;
    default:
        break;
    }

bail:
    return err;
}


/*----------------------------------------------------------------------------
 * Helper function to iterate through an option list and return the data
 * (flag).
 *--------------------------------------------------------------------------*/
static int
cli_nvsd_debug_extract_flags(
        const char *flag_str, 
        cli_expansion_option options[],
        uint32 *ret_flags, tbool *ret_flags_ok)
{
    int err = 0;
    void *vflag = NULL;
    tbool found = false;
    

    bail_null(flag_str);
    bail_null(ret_flags);
    bail_null(ret_flags_ok);
    *ret_flags = 0;
    *ret_flags_ok = false;

    err = cli_expansion_option_get_data(
            options,
            -1,
            flag_str,
            &vflag,
            &found);
    bail_error(err);

    if (found) {
        *ret_flags = (uint32)(int_ptr) vflag;
        *ret_flags_ok = true;
    }

bail:
    return err;
}
