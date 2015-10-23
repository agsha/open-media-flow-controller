/*
 *
 * File Name: cli_nkn_fmsaccesslog_cmds.c 
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
cli_nkn_fmsaccesslog_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


static int
cli_fmsaccesslog_file_upload_finish(const char *password, clish_password_result result,
    const cli_command *cmd,
    const tstr_array *cmd_line, const tstr_array *params,
    void *data, tbool *ret_continue);


int 
cli_nkn_fmsaccesslog_config_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nkn_fmsaccesslog_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int 
cli_nkn_fmsaccesslog_cmds_init(
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
    cmd->cc_words_str =             "fmsaccesslog";
    cmd->cc_help_descr =            N_("Configure fms access log options");
    cmd->cc_flags =                 ccf_terminal | ccf_prefix_mode_root| ccf_unavailable;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no fmsaccesslog";
    cmd->cc_flags =                 ccf_unavailable;
    cmd->cc_help_descr =            N_("Delete fms access log configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog no";
    cmd->cc_help_descr =            N_("Clear fms accesslog options");
    cmd->cc_req_prefix_count =      1;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog enable";
    cmd->cc_help_descr =            N_("Enable fms access logging");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsaccesslog/config/enable";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsaccesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no fmsaccesslog enable";
    cmd->cc_help_descr =            N_("Disable fms access log");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsaccesslog/config/enable";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsaccesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog syslog";
    cmd->cc_help_descr =            N_("Configure syslog options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog syslog replicate";
    cmd->cc_help_descr =            N_("Configure syslog replication");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog syslog replicate enable";
    cmd->cc_help_descr =            N_("Enable syslog replication of fms access log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsaccesslog/config/syslog";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsaccesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog syslog replicate disable";
    cmd->cc_help_descr =            N_("Disable syslog replication of fms access log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsaccesslog/config/syslog";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsaccesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog filename";
    cmd->cc_help_descr =            N_("Configure filename");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog filename access.00.log";
    cmd->cc_help_descr =            N_("Set default filename - access.00.log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsaccesslog/config/filename";
    cmd->cc_exec_value =            "access.00.log";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_none;
    cmd->cc_revmap_order =          cro_fmsaccesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog filename *";
    cmd->cc_help_exp =              N_("<filename>");
    cmd->cc_help_exp_hint =         N_("Set custom filename");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsaccesslog/config/filename";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsaccesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog rotate";
    cmd->cc_help_descr =            N_("Configure rotation for log files");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog rotate filesize";
    cmd->cc_help_descr =            N_("Configure maximum file size per log file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=       "fmsaccesslog rotate filesize 100";
    cmd->cc_help_descr 		=      	N_("Set default disk storage size for fmsaccess logs - 100 MiB");
    cmd->cc_flags 		=       ccf_terminal;
    cmd->cc_capab_required 	=       ccp_set_rstr_curr;
    cmd->cc_exec_operation 	=       cxo_set;
    cmd->cc_exec_name 		=       "/nkn/fmsaccesslog/config/max-filesize";
    cmd->cc_exec_value 		=       "100";
    cmd->cc_exec_type 		=       bt_uint16;
    cmd->cc_revmap_type 	=       crt_none;
    cmd->cc_revmap_order 	=       cro_fmsaccesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=       "fmsaccesslog rotate filesize *";
    cmd->cc_help_exp 		=       N_("<size in MiB>");
    cmd->cc_help_exp_hint 	=      	N_("Set total diskspace to use for fms access log files (in MiB)");
    cmd->cc_flags 		=       ccf_terminal;
    cmd->cc_capab_required 	=       ccp_set_rstr_curr;
    cmd->cc_exec_operation 	=       cxo_set;
    cmd->cc_exec_name 		=       "/nkn/fmsaccesslog/config/max-filesize";
    cmd->cc_exec_value 		=       "$1$";
    cmd->cc_exec_type 		=       bt_uint16;
    cmd->cc_revmap_type 	=       crt_auto;
    cmd->cc_revmap_order 	=       cro_fmsaccesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog rotate time-interval";
    cmd->cc_help_descr =            N_("Configure time-interval per log file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=       "fmsaccesslog rotate time-interval *";
    cmd->cc_help_exp 		=       N_("<time in Hour>");
    cmd->cc_help_exp_hint 	=      	N_("Set custom rotation time interval (in Hours)");
    cmd->cc_flags 		=       ccf_terminal;
    cmd->cc_capab_required 	=       ccp_set_rstr_curr;
    cmd->cc_exec_operation 	=       cxo_set;
    cmd->cc_exec_name 		=       "/nkn/fmsaccesslog/config/time-interval";
    cmd->cc_exec_value 		=       "$1$";
    cmd->cc_exec_value_multiplier =     60;
    cmd->cc_exec_type 		=       bt_uint32;
    cmd->cc_revmap_type 	=       crt_auto;
    cmd->cc_revmap_order 	=       cro_fmsaccesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog copy";
    cmd->cc_help_descr =            N_("Auto-copy the fmsaccesslog based on rotate criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog copy *";
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_magic =                 cfi_fmsaccesslog;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =              N_("<scp://username:password@hostname/path>"); 
    cmd->cc_exec_callback =         cli_nkn_fmsaccesslog_config_upload;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no fmsaccesslog copy";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_help_descr =            N_("Disable auto upload of fms access logs");
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/fmsaccesslog/config/upload/pass";
    cmd->cc_exec_name2 =            "/nkn/fmsaccesslog/config/upload/url";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog on-the-hour ";
    cmd->cc_help_descr =            N_("Set on the hour log rotation option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog on-the-hour enable";
    cmd->cc_help_descr =            N_("Enable On the hour log rotation");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsaccesslog/config/on-the-hour";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsaccesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fmsaccesslog on-the-hour disable";
    cmd->cc_help_descr =            N_("Disable On the hour log rotation");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fmsaccesslog/config/on-the-hour";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fmsaccesslog;
    CLI_CMD_REGISTER;
    /* Command to view fmsaccesslog configuration */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show fmsaccesslog";
    cmd->cc_help_descr =            N_("View fms access log configuration");
    cmd->cc_flags =                 ccf_terminal| ccf_unavailable; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_fmsaccesslog_show_config;
    CLI_CMD_REGISTER;

    
    err = cli_revmap_ignore_bindings("/nkn/fmsaccesslog/config/upload/*");
    bail_error(err);
    
    err = cli_revmap_ignore_bindings("/nkn/fmsaccesslog/config/path");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/fmsaccesslog/config/**");
    bail_error(err);


bail:
    return err;
}

static int
cli_fmsaccesslog_file_upload_finish(const char *password, clish_password_result result,
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
                cli_mcc, NULL, NULL, "/nkn/fmsaccesslog/action/upload", 2,
                "remote_url", bt_string, remote_url,
                "password", bt_string, password);
        bail_error(err);
    }
    else {
        err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/nkn/fmsaccesslog/action/upload", 1,
                "remote_url", bt_string, remote_url);
        bail_error(err);
    }
bail:
    return err;
}

int 
cli_nkn_fmsaccesslog_config_upload(
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
                (remote_url, cli_fmsaccesslog_file_upload_finish, cmd, cmd_line, params, NULL);
        bail_error(err);
    }
    else {
        err = cli_fmsaccesslog_file_upload_finish(NULL, cpr_ok, cmd, cmd_line, params,
                    NULL, NULL);
        bail_error(err);
}
bail:
    return err;
}


int 
cli_nkn_fmsaccesslog_show_config(
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
    err = cli_printf_query
                    (_("Media Flow Director FMS Access Log enabled : "
                       "#/nkn/fmsaccesslog/config/enable#\n"));
    bail_error(err);
    
    err = cli_printf_query
            (_("  Replicate to Syslog : "
               "#/nkn/fmsaccesslog/config/syslog#\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  FMS Access Log filename : "
               "#/nkn/fmsaccesslog/config/filename#\n"));
    bail_error(err);
#if 0
    err = cli_printf_query
            (_("  FMS Access Log filesize : "
               "#/nkn/fmsaccesslog/config/max-filesize# MiB\n"));
    bail_error(err);

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
		             "/nkn/fmsaccesslog/config/time-interval" ,NULL);
    bail_error(err);
                                                                                                                               
    if (binding ) {
	err = bn_binding_get_uint32(binding,ba_value,NULL,&val);
	bail_error(err);
	bn_binding_free(&binding);
    }
    
    iHour = val/iDivisor;

    err = cli_printf
            (_("  FMS Access Log time interval : "
               "%d Hours\n"), iHour);
    bail_error(err);

    err = cli_printf_query
	    (_("  FMS Access log On the hour log rotation : "
	       "#/nkn/fmsaccesslog/config/on-the-hour#\n"));
   // TODO:Obfuscate the password as in other log modules.
   // Donot use the below code.Refer Bug 11298 and Bug 11151
     err = cli_printf_query
            (_("  Auto Copy URL : "
               "#/nkn/fmsaccesslog/config/upload/url#\n"));
    bail_error(err);
#endif

bail:
    return err;
}


