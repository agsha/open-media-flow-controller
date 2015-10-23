/*
 *
 * File Name: cli_nkn_fmsedgelog_cmds.c 
 *
 * Author: Varsha
 * 
 * Creation Date:10/29/2009 
 *
 * 
 */

#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "file_utils.h"
#include "url_utils.h"
#include "nkn_defs.h"

int 
cli_nkn_fmsedgelog_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


static int
cli_fmsedgelog_file_upload_finish(const char *password, clish_password_result result,
    const cli_command *cmd,
    const tstr_array *cmd_line, const tstr_array *params,
    void *data, tbool *ret_continue);


int 
cli_nkn_fmsedgelog_config_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nkn_fmsedgelog_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int 
cli_nkn_fmsedgelog_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);

#ifdef PROD_FEATURE_I18N_SUPPORT
        /* This is pretty much redundant and can be skipped in your
         * modules, unless you want internationalization support.
         */
        err = cli_set_gettext_domain(GETTEXT_DOMAIN);
        bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog";
    cmd->cc_help_descr =            N_("Configure fms edge log options");
    cmd->cc_flags =                 ccf_terminal | ccf_prefix_mode_root| ccf_unavailable;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no fmsedgelog";
    cmd->cc_flags =                 ccf_unavailable;
    cmd->cc_help_descr =            N_("Delete fms edge log configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog no";
    cmd->cc_help_descr =            N_("Clear fms edgelog options");
    cmd->cc_req_prefix_count =      1;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog enable";
    cmd->cc_help_descr =            N_("Enable fms edge logging");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsedgelog/config/enable";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsedgelog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no fmsedgelog enable";
    cmd->cc_help_descr =            N_("Disable fms edge log");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsedgelog/config/enable";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsedgelog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog syslog";
    cmd->cc_help_descr =            N_("Configure syslog options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog syslog replicate";
    cmd->cc_help_descr =            N_("Configure syslog replication");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog syslog replicate enable";
    cmd->cc_help_descr =            N_("Enable syslog replication of fms edge log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsedgelog/config/syslog";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsedgelog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog syslog replicate disable";
    cmd->cc_help_descr =            N_("Disable syslog replication of fms edge log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsedgelog/config/syslog";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsedgelog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog filename";
    cmd->cc_help_descr =            N_("Configure filename");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog filename edge.00.log";
    cmd->cc_help_descr =            N_("Set default filename - edge.00.log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsedgelog/config/filename";
    cmd->cc_exec_value =            "edge.00.log";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_none;
    cmd->cc_revmap_order =          cro_fmsedgelog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog filename *";
    cmd->cc_help_exp =              N_("<filename>");
    cmd->cc_help_exp_hint =         N_("Set custom filename");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsedgelog/config/filename";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsedgelog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog rotate";
    cmd->cc_help_descr =            N_("Configure rotation for log files");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog rotate filesize";
    cmd->cc_help_descr =            N_("Configure maximum file size per log file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=       "fmsedgelog rotate filesize 100";
    cmd->cc_help_descr 		=      	N_("Set default disk storage size for fmsedge logs - 100 MiB");
    cmd->cc_flags 		=       ccf_terminal;
    cmd->cc_capab_required 	=       ccp_set_rstr_curr;
    cmd->cc_exec_operation 	=       cxo_set;
    cmd->cc_exec_name 		=       "/nkn/fmsedgelog/config/max-filesize";
    cmd->cc_exec_value 		=       "100";
    cmd->cc_exec_type 		=       bt_uint16;
    cmd->cc_revmap_type 	=       crt_none;
    cmd->cc_revmap_order 	=       cro_fmsedgelog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=       "fmsedgelog rotate filesize *";
    cmd->cc_help_exp 		=       N_("<size in MiB>");
    cmd->cc_help_exp_hint 	=      	N_("Set total diskspace to use for fms edge log files (in MiB)");
    cmd->cc_flags 		=       ccf_terminal;
    cmd->cc_capab_required 	=       ccp_set_rstr_curr;
    cmd->cc_exec_operation 	=       cxo_set;
    cmd->cc_exec_name 		=       "/nkn/fmsedgelog/config/max-filesize";
    cmd->cc_exec_value 		=       "$1$";
    cmd->cc_exec_type 		=       bt_uint16;
    cmd->cc_revmap_type 	=       crt_auto;
    cmd->cc_revmap_order 	=       cro_fmsedgelog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog rotate time-interval";
    cmd->cc_help_descr =            N_("Configure time-interval per log file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=       "fmsedgelog rotate time-interval *";
    cmd->cc_help_exp 		=       N_("<time in Hour>");
    cmd->cc_help_exp_hint 	=      	N_("Set custom rotation time interval (in Hours)");
    cmd->cc_flags 		=       ccf_terminal;
    cmd->cc_capab_required 	=       ccp_set_rstr_curr;
    cmd->cc_exec_operation 	=       cxo_set;
    cmd->cc_exec_name 		=       "/nkn/fmsedgelog/config/time-interval";
    cmd->cc_exec_value 		=       "$1$";
    cmd->cc_exec_value_multiplier =     60;
    cmd->cc_exec_type 		=       bt_uint32;
    cmd->cc_revmap_type 	=       crt_auto;
    cmd->cc_revmap_order 	=       cro_fmsedgelog;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog copy";
    cmd->cc_help_descr =            N_("Auto-copy the fmsedgelog based on rotate criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog copy *";
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_magic =                 cfi_fmsedgelog;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =              N_("<scp://username:password@hostname/path>"); 
    cmd->cc_exec_callback =         cli_nkn_fmsedgelog_config_upload;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no fmsedgelog copy";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_help_descr =            N_("Disable auto upload of fms edge logs");
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/fmsedgelog/config/upload/pass";
    cmd->cc_exec_name2 =            "/nkn/fmsedgelog/config/upload/url";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog on-the-hour ";
    cmd->cc_help_descr =            N_("Set on the hour log rotation option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog on-the-hour enable";
    cmd->cc_help_descr =            N_("Enable On the hour log rotation");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsedgelog/config/on-the-hour";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsedgelog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsedgelog on-the-hour disable";
    cmd->cc_help_descr =            N_("Disable On the hour log rotation");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsedgelog/config/on-the-hour";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsedgelog;
    CLI_CMD_REGISTER;

    /* Command to view fmsedgelog configuration */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show fmsedgelog";
    cmd->cc_help_descr =            N_("View fms edge log configuration");
    cmd->cc_flags =                 ccf_terminal| ccf_unavailable; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_fmsedgelog_show_config;
    CLI_CMD_REGISTER;

    
    err = cli_revmap_ignore_bindings("/nkn/fmsedgelog/config/upload/*");
    bail_error(err);
    
    err = cli_revmap_ignore_bindings("/nkn/fmsedgelog/config/path");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/fmsedgelog/config/**");
    bail_error(err);


bail:
    return err;
}

static int
cli_fmsedgelog_file_upload_finish(const char *password, clish_password_result result,
    const cli_command *cmd,
    const tstr_array *cmd_line, const tstr_array *params,
    void *data, tbool *ret_continue)
{
    int err = 0;
    const char *remote_url = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(ret_continue);

    if (result != cpr_ok) {
        goto bail;
    }

    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    if (!lc_is_prefix("scp://", remote_url, false)) {
        cli_printf_error(_("Bad destination specifier"));
        goto bail;
    }

    if (password) {
        err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/nkn/fmsedgelog/action/upload", 2,
                "remote_url", bt_string, remote_url,
                "password", bt_string, password);
        bail_error(err);
    }
    else {
        err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/nkn/fmsedgelog/action/upload", 1,
                "remote_url", bt_string, remote_url);
        bail_error(err);
    }
bail:
    return err;
}

int 
cli_nkn_fmsedgelog_config_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;

    UNUSED_ARGUMENT(data);

    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    if (clish_is_shell()) {
        err = clish_maybe_prompt_for_password_url
                (remote_url, cli_fmsedgelog_file_upload_finish, cmd, cmd_line, params, NULL);
        bail_error(err);
    }
    else {
        err = cli_fmsedgelog_file_upload_finish(NULL, cpr_ok, cmd, cmd_line, params,
                    NULL, NULL);
        bail_error(err);
}
bail:
    return err;
}


int 
cli_nkn_fmsedgelog_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    bn_binding *binding = NULL;
    int iDivisor = 60;
    int iHour =0;
    uint32 val=0, code = 0;
   
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_printf_query
                    (_("Media Flow Director FMS Edge Log enabled : "
                       "#/nkn/fmsedgelog/config/enable#\n"));
    bail_error(err);
    
    err = cli_printf_query
            (_("  Replicate to Syslog : "
               "#/nkn/fmsedgelog/config/syslog#\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  FMS Edge Log filename : "
               "#/nkn/fmsedgelog/config/filename#\n"));
    bail_error(err);
    err = cli_printf_query
            (_("  FMS Edge Log filesize : "
               "#/nkn/fmsedgelog/config/max-filesize# MiB\n"));
    bail_error(err);

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
		             "/nkn/fmsedgelog/config/time-interval" ,NULL);
    bail_error(err);
                                                                                                                               
    if (binding ) {
	err = bn_binding_get_uint32(binding,ba_value,NULL,&val);
	bail_error(err);
	bn_binding_free(&binding);
    }
    
    iHour = val/iDivisor;

    err = cli_printf
            (_("  FMS Edge Log time interval : "
               "%d Hours\n"), iHour);
    bail_error(err);

    err = cli_printf_query
	    (_("  FMS Edge log On the hour log rotation : "
	       "#/nkn/fmsedgelog/config/on-the-hour#\n"));

    err = cli_printf_query
            (_("  Auto Copy URL : "
               "#/nkn/fmsedgelog/config/upload/url#\n"));
    bail_error(err);

bail:
    return err;
}


