/*
 * Filename :   cli_nvsd_ramcache_cmds.c
 * Date     :   2008/12/15
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
#include "clish_module.h"
#include "libnkncli.h"
#include "proc_utils.h"
#include "nkn_defs.h"


#define	MAX_CACHESIZE_AUTO	"0"

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
cli_nvsd_ramcache_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_nvsd_ramcache_show_cmd(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);

extern int
cli_std_exec(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params);

int
cli_print_restart_msg(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params);

int 
cli_nvsd_ramcache_object_list(
	void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params);


static int
cli_nvsd_ramcache_smallattri_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

int
cli_nvsd_ramcache_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = cli_set_gettext_domain(GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache";
    cmd->cc_help_descr =        N_("Configure RAM cache options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no ram-cache";
    cmd->cc_help_descr =        N_("Disable RAM cache options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache cache-size";
    cmd->cc_help_descr =        N_("Configure ram-cache size");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache small-attribute";
    cmd->cc_help_descr =        N_("Configure smaller buffer for object headers/attributes in RAM cache");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no ram-cache small-attribute";
    cmd->cc_help_descr =        N_("Remove small attributes from RAM-Cache");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name2 =         "/nkn/nvsd/buffermgr/config/attributecount";
    cmd->cc_exec_name =         "/nkn/nvsd/buffermgr/config/attributesize";
    cmd->cc_exec_callback =     cli_print_restart_msg;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache small-attribute size";
    cmd->cc_help_descr =        N_("Size of each header/attribute (in bytes)");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache small-attribute size *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("Supported Sizes 1024, 1536, 2048, 2560, 3072");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache small-attribute size * count";
    cmd->cc_help_descr =        N_("Number of buffers used for small attributes");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache small-attribute size * count *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("value should be between 0 to 64000000 [default: 0]");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name2 =        "/nkn/nvsd/buffermgr/config/attributecount";
    cmd->cc_exec_value2 =       "$2$";
    cmd->cc_exec_type2 =        bt_uint32;
    cmd->cc_exec_name =         "/nkn/nvsd/buffermgr/config/attributesize";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_exec_callback =     cli_print_restart_msg;
    cmd->cc_revmap_order =      cro_media_cache;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_callback =   cli_nvsd_ramcache_smallattri_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache small-buffers";
    cmd->cc_help_descr =        N_("Configure ram-cache small buffers");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache small-buffers scale-factor";
    cmd->cc_help_descr =        N_("Configure ram-cache small buffers scaling");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache small-buffers scale-factor *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("Value should be between 0 to 8 [default: 2]");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/buffermgr/config/pageratio";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_callback =     cli_print_restart_msg;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache cache-size 0";
    cmd->cc_help_descr =        N_("Set default ram cache size to AUTO");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/buffermgr/config/max_cachesize";
    cmd->cc_exec_value =        "0";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_callback =     cli_print_restart_msg;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache cache-size *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("Size in MiB");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/buffermgr/config/max_cachesize";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_callback =     cli_print_restart_msg;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache dict-size-mb";
   /* cmd->cc_flags =             ccf_hidden; bug# 7030*/
    cmd->cc_help_descr =        N_("Configure cache dictionary size");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache dict-size-mb 0";
    cmd->cc_help_descr =        N_("Set default cache dictionary size to AUTO");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/buffermgr/config/max_dictsize";
    cmd->cc_exec_value =        "0";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_callback =     cli_print_restart_msg;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache dict-size-mb *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("Size in MiB [0  -  32768]");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/buffermgr/config/max_dictsize";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_callback =     cli_print_restart_msg;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache lru-scale";
    cmd->cc_help_descr =        N_("Configure scale factor to increase the hotness of Least Recently Used Buffer");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache lru-scale *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("Scale factor 1 to 100");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/buffermgr/config/lru-scale";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show ram-cache";
    cmd->cc_help_descr =        N_("Display buffer-manager configuration");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_ramcache_show_cmd;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show ram-cache object";
    cmd->cc_help_descr =        N_("Display ram-cache object list");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show ram-cache object request-list";
    cmd->cc_help_descr =        N_("Display ram-cache object list");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_ramcache_object_list;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache sync-interval";
    cmd->cc_help_descr =        N_("Configure nkn_attr_sync_interval");
    //cmd->cc_flags =             ccf_hidden; bug# 7461
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache sync-interval *";
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_help_exp_hint =     N_("Default value : 14400");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/buffermgr/config/attr_sync_interval";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache revalidate-minsize";
    cmd->cc_help_descr =        N_("Configure revalidate window size");
    //cmd->cc_flags =             ccf_hidden; bug# 7461
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache revalidate-minsize *";
    cmd->cc_help_exp =          N_("<size>");
    cmd->cc_help_exp_hint =     N_("Revalidation size, in KiB [Default: 1]");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/buffermgr/config/revalidate_minsize";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_value_multiplier = 1024;
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "thread-pool";
    cmd->cc_help_descr =        N_("Configure thread-pool stack size");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "thread-pool stack-size";
    cmd->cc_help_descr =        N_("Configure thread-pool stack size");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "thread-pool stack-size *";
    cmd->cc_help_exp =          N_("<size>");
    cmd->cc_help_exp_hint =     N_("Stack size, in bytes");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/buffermgr/config/threadpool_stack_size";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint64;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_unconsumed;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache cache-size unsuccessful-responses";
    cmd->cc_help_descr =        N_("Configure RAM cache size for caching non-2xx responses from origin");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "ram-cache cache-size unsuccessful-responses *";
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Value in percentage (0-20%)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/buffermgr/config/unsuccessful_response/cachesize";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_callback =     cli_print_restart_msg;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_media_cache;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va(4,
             "/nkn/nvsd/buffermgr/config/revalidate_window",
             "/nkn/nvsd/buffermgr/config/lru-scale",
	     "/nkn/nvsd/buffermgr/config/attributesize",
	     "/nkn/nvsd/buffermgr/config/attributecount");
    bail_error(err);

bail:
    return err;
}


int
cli_nvsd_ramcache_show_cmd(
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

    err = cli_printf_query(_("\nConfigured Attribute Sync Interval: "
                "#/nkn/nvsd/buffermgr/config/attr_sync_interval#"
                " secs\n"));
    bail_error(err);

    err = cli_printf_query(_("Configured Revalidate Object Minimum Size: "
                "#/nkn/nvsd/buffermgr/config/revalidate_minsize#"
                " KiBytes\n"));
    bail_error(err);

    err = cli_printf_query(_("Configured Revalidate Window Size: "
                "#/nkn/nvsd/buffermgr/config/revalidate_window#"
                " secs\n\n"));
    bail_error(err);

    err = cli_printf_query(_("Maximum available RAM Cache size : "
                "#/nkn/nvsd/buffermgr/monitor/max_cachesize_calculated#"
                " MiBytes\n\n"));
    bail_error(err);


    err = cli_printf_query(_("Configured Max RAM Cache size : "
                "#/nkn/nvsd/buffermgr/config/max_cachesize#"
                " MiBytes\n\n"));
    bail_error(err);

    err = cli_printf_query(_("Current RAM Cache scale factor: "
                "#/nkn/nvsd/buffermgr/config/pageratio# \n\n"));
    bail_error(err);

    err = cli_printf_query(_("Current RAM Cache small attribute count: "
                "#/nkn/nvsd/buffermgr/monitor/attributecount#"
                " \n\n"));
    bail_error(err);


    err = cli_printf_query(_("Configured RAM Cache small attribute count: "
                "#/nkn/nvsd/buffermgr/config/attributecount#"
                " \n\n"));
    bail_error(err);
    
    err = cli_printf_query(_("Configured RAM Cache small attribute size: "
                "#/nkn/nvsd/buffermgr/config/attributesize#"
                " \n\n"));
    bail_error(err);

    err = cli_printf_query(_("Auto Calculated Max RAM Cache Dictionary size : "
                "#/nkn/nvsd/buffermgr/monitor/cal_max_dict_size#"
                " MiBytes\n\n"));
    bail_error(err);

    err = cli_printf_query(_("Configured Max RAM Cache Dictionary size : "
                "#/nkn/nvsd/buffermgr/config/max_dictsize#"
                " MiBytes\n\n"));
    bail_error(err);

    cli_printf(_("Note : Configured Max RAM Cache size of 0 indicates \n Maximum available RAM Cache size value\n"));

    err = cli_printf_query(_("Configured unsuccessful response RAM cache percentage: "
                "#/nkn/nvsd/buffermgr/config/unsuccessful_response/cachesize#"
                " \n\n"));
    bail_error(err);
bail:
    return err;
}


int
cli_print_restart_msg(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;

    err = cli_std_exec(data, cmd, cmd_line, params);
    bail_error(err);

    /* Print the restart message */
    err = cli_printf("WARNING: if command successful, please use service restart mod-delivery command for changes to take effect\n");
    bail_error(err);

 bail:
    return(err);
}

int 
cli_nvsd_ramcache_object_list(
	void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params)
{
    int err = 0;
    uint32 ret_err = 0;
    tstr_array *argv = NULL;
    tstring *ret_output = NULL;
    int status = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);
    err = mdc_send_action_with_bindings_str_va
            (cli_mcc, &ret_err, NULL, "/nkn/nvsd/buffermgr/actions/object-list",
            0, NULL, bt_none, NULL);
    bail_error(err);

    if (ret_err != 0) {
        cli_printf(_("Error in executing command.\n"));
        goto bail;
    }

    /*
     * Read the first 50 lines of the list and display only them
     */
    err = lc_launch_quick_status(&status, &ret_output, true, 4, "/usr/bin/head", "-n", "54", "/var/log/nkn/buffermgr_object_list");
    bail_error(err);
    cli_printf("%s", ts_str(ret_output));

#if 0
    err = tstr_array_new_va_str(&argv, NULL, 4,
	    "/usr/bin/head", "-n",
	    "54", "/var/log/nkn/buffermgr_object_list");
    bail_error(err);
    err = lc_lauch_quick_status("/usr/bin/head", argv, NULL, NULL, NULL);
#endif

#if 0
    tmp_filename = (char*) file_out;
    // This is where the file needs to be dumped
    if (clish_paging_is_enabled()) {
        err = tstr_array_new_va_str(&argv, NULL, 9, prog_cli_pager_path,
                "-i", 
                "-M",
                "-e", 
                "+G", 
                "-X",
                "-d",
                "--",
                tmp_filename);
        bail_error(err);

        err = clish_run_program_fg(prog_cli_pager_path, argv, NULL,
                cli_nvsd_am_show_terminate, tmp_filename);
        bail_error(err);
    } 
    else {
        cat_prog = prog_zcat_path;
        err = tstr_array_new_va_str(&argv, NULL, 3, cat_prog, "--",
                tmp_filename);
        bail_error(err);

        err = clish_run_program_fg(cat_prog, argv, NULL,
                cli_nvsd_am_show_terminate, tmp_filename);
        bail_error(err);
    }
#endif /* 0 */
    cli_printf( _("The complete list is available in the file /var/log/nkn/buffermgr_object_list\n"));

bail:
    tstr_array_free(&argv);
    return err;
}

static int
cli_nvsd_ramcache_smallattri_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *t_small_attri_size = NULL;
    tstring *t_small_attri_count = NULL;
    tstring *rev_cmd = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(name_parts);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/buffermgr/config/attributesize",
            NULL, &t_small_attri_size);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/nkn/nvsd/buffermgr/config/attributecount",
            NULL, &t_small_attri_count);
    bail_error(err);

    err = ts_new_str(&rev_cmd, "ram-cache small-attribute size ");
    bail_error(err);

    err = ts_append_sprintf(rev_cmd, " %s count %s", ts_str(t_small_attri_size), ts_str(t_small_attri_count));
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/buffermgr/config/attributesize");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/buffermgr/config/attributecount");
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

bail:
	ts_free(&t_small_attri_size);
	ts_free(&t_small_attri_count);
	ts_free(&rev_cmd);
    return err;
}
