/*
 *
 * File Name: cli_nkn_fuselog_cmds.c 
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
#include "tpaths.h"
#include "nkn_defs.h"

int 
cli_nkn_fuselog_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


static int
cli_fuselog_file_upload_finish(const char *password, clish_password_result result,
    const cli_command *cmd,
    const tstr_array *cmd_line, const tstr_array *params,
    void *data, tbool *ret_continue);


int 
cli_nkn_fuselog_config_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nkn_fuselog_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nkn_fuselog_show_continuous(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params);

int
cli_fuselog_warning_message(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);

static int
cli_fuselog_time_interval_revmap(void *data, const cli_command *cmd,
                           const bn_binding_array *bindings,
                           const char *name,
                           const tstr_array *name_parts,
                           const char *value, const bn_binding *binding,
                           cli_revmap_reply *ret_reply);
int 
cli_nkn_fuselog_cmds_init(
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
    cmd->cc_words_str =             "fuselog";
    cmd->cc_help_descr =            N_("Configure fuse log options");
    cmd->cc_flags =                 ccf_terminal | ccf_prefix_mode_root| ccf_unavailable;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no fuselog";
    cmd->cc_flags =                 ccf_unavailable;
    cmd->cc_help_descr =            N_("Delete fuselog configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog no";
    cmd->cc_help_descr =            N_("Clear fuselog options");
    cmd->cc_req_prefix_count =      1;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog enable";
    cmd->cc_help_descr =            N_("Enable fuse logging");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fuselog/config/enable";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fuselog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no fuselog enable";
    cmd->cc_help_descr =            N_("Disable fuse log");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden | ccf_unavailable;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fuselog/config/enable";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fuselog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog syslog";
    cmd->cc_help_descr =            N_("Configure syslog options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog syslog replicate";
    cmd->cc_help_descr =            N_("Configure syslog replication");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog syslog replicate enable";
    cmd->cc_help_descr =            N_("Enable syslog replication of fuse log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fuselog/config/syslog";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fuselog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog syslog replicate disable";
    cmd->cc_help_descr =            N_("Disable syslog replication of fuse log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fuselog/config/syslog";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fuselog;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog filename";
    cmd->cc_help_descr =            N_("Configure filename");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog filename fuse.log";
    cmd->cc_help_descr =            N_("Set default filename - fuse.log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fuselog/config/filename";
    cmd->cc_exec_value =            "fuse.log";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_none;
    cmd->cc_revmap_order =          cro_fuselog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog filename *";
    cmd->cc_help_exp =              N_("<filename>");
    cmd->cc_help_exp_hint =         N_("Set custom filename");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fuselog/config/filename";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fuselog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog rotate";
    cmd->cc_help_descr =            N_("Configure rotation for log files");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog rotate filesize";
    cmd->cc_help_descr =            N_("Configure maximum file size per log file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=       "fuselog rotate filesize 100";
    cmd->cc_help_descr 		=      	N_("Set default disk storage size for fuse logs - 100 MiB");
    cmd->cc_flags 		=       ccf_terminal;
    cmd->cc_capab_required 	=       ccp_set_rstr_curr;
    cmd->cc_exec_operation 	=       cxo_set;
    cmd->cc_exec_name 		=       "/nkn/fuselog/config/max-filesize";
    cmd->cc_exec_value 		=       "100";
    cmd->cc_exec_type 		=       bt_uint16;
    cmd->cc_revmap_type 	=       crt_none;
    cmd->cc_revmap_order 	=       cro_fuselog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=       "fuselog rotate filesize *";
    cmd->cc_help_exp 		=       N_("<size in MiB>");
    cmd->cc_help_exp_hint 	=      	N_("Set total diskspace to use for fuse log files (in MiB)");
    cmd->cc_flags 		=       ccf_terminal;
    cmd->cc_capab_required 	=       ccp_set_rstr_curr;
    cmd->cc_exec_operation 	=       cxo_set;
    cmd->cc_exec_name 		=       "/nkn/fuselog/config/max-filesize";
    cmd->cc_exec_value 		=       "$1$";
    cmd->cc_exec_type 		=       bt_uint16;
    cmd->cc_revmap_type 	=       crt_auto;
    cmd->cc_revmap_order 	=       cro_fuselog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog rotate time-interval";
    cmd->cc_help_descr =            N_("Configure time-interval per log file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=       "fuselog rotate time-interval *";
    cmd->cc_help_exp 		=       N_("<time in Minutes>");
    cmd->cc_help_exp_hint 	=      	N_("Set custom rotation time interval (in Minutes)");
    cmd->cc_flags 		=       ccf_terminal;
    cmd->cc_capab_required 	=       ccp_set_rstr_curr;
    cmd->cc_exec_operation 	=       cxo_set;
    cmd->cc_exec_name 		=       "/nkn/fuselog/config/time-interval";
    cmd->cc_exec_callback	=	cli_fuselog_warning_message;
    cmd->cc_revmap_type 	=       crt_manual;
    cmd->cc_revmap_order 	=       cro_fuselog;
    cmd->cc_revmap_names 	=       "/nkn/fuselog/config/time-interval";
    cmd->cc_revmap_callback 	=       cli_fuselog_time_interval_revmap;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog copy";
    cmd->cc_help_descr =            N_("Auto-copy the fuselog based on rotate criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog copy *";
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_magic =                 cfi_fuselog;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =              N_("[scp|sftp|ftp]://<username>:<password>@<hostname>/<path>"); 
    cmd->cc_exec_callback =         cli_nkn_fuselog_config_upload;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no fuselog copy";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_help_descr =            N_("Disable auto upload of fuse logs");
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/fuselog/config/upload/pass";
    cmd->cc_exec_name2 =            "/nkn/fuselog/config/upload/url";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog on-the-hour ";
    cmd->cc_help_descr =            N_("Set on the hour log rotation option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog on-the-hour enable";
    cmd->cc_help_descr =            N_("Enable On the hour log rotation");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fuselog/config/on-the-hour";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fuselog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "fuselog on-the-hour disable";
    cmd->cc_help_descr =            N_("Disable On the hour log rotation");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/fuselog/config/on-the-hour";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_fuselog;
    CLI_CMD_REGISTER;

    /* Command to view fuselog configuration */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show fuselog";
    cmd->cc_help_descr =            N_("View fuse log configuration");
    cmd->cc_flags =                 ccf_terminal | ccf_unavailable; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_fuselog_show_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show fuselog continuous";
    cmd->cc_help_descr =            N_("View continuous fuse log ");
    cmd->cc_flags =                 ccf_terminal; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_fuselog_show_continuous;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show fuselog last";
    cmd->cc_help_descr =            N_("View last lines of fuse log ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show fuselog last *";
    cmd->cc_help_exp =             N_("<number>");
    cmd->cc_help_exp_hint =        N_("Give the number of lines to be displayed");
    cmd->cc_flags =                 ccf_terminal; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_fuselog_show_continuous;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings("/nkn/fuselog/config/upload/*");
    bail_error(err);
    
    err = cli_revmap_ignore_bindings("/nkn/fuselog/config/path");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/fuselog/config/**");
    bail_error(err);


bail:
    return err;
}

static int
cli_fuselog_file_upload_finish(const char *password, clish_password_result result,
    const cli_command *cmd,
    const tstr_array *cmd_line, const tstr_array *params,
    void *data, tbool *ret_continue)
{
    int err = 0;
    const char *remote_url = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(ret_continue);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    if (result != cpr_ok) {
        goto bail;
    }

    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    if ((!lc_is_prefix("scp://", remote_url, false)) &&
            (!lc_is_prefix("sftp://", remote_url, false))&&
                (!lc_is_prefix("ftp://", remote_url, false))){
        cli_printf_error(_("Bad destination specifier only supports scp or sftp or ftp"));
        goto bail;
    }

    if (password) {
        err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/nkn/fuselog/action/upload", 2,
                "remote_url", bt_string, remote_url,
                "password", bt_string, password);
        bail_error(err);
    }
    else {
        err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/nkn/fuselog/action/upload", 1,
                "remote_url", bt_string, remote_url);
        bail_error(err);
    }
bail:
    return err;
}

int 
cli_nkn_fuselog_config_upload(
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
        if ((lc_is_prefix("scp://", remote_url, false)) ||
            (lc_is_prefix("sftp://", remote_url, false))||
                (lc_is_prefix("ftp://", remote_url, false))){
            err = clish_maybe_prompt_for_password_url
                (remote_url, cli_fuselog_file_upload_finish, cmd, cmd_line, params, NULL);
            bail_error(err);
         }
    }
    else {
        err = cli_fuselog_file_upload_finish(NULL, cpr_ok, cmd, cmd_line, params,
                    NULL, NULL);
        bail_error(err);
}
bail:
    return err;
}


int 
cli_nkn_fuselog_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    tstring *url = NULL;
    tstring * ret_url_pw_obfuscated = NULL;
    bn_binding_array *bindings = NULL;
    tbool ret_changed = false;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);
    err = cli_printf_query
                    (_("Media Flow Controller Fuse Log enabled : "
                       "#/nkn/fuselog/config/enable#\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Replicate to Syslog : "
               "#/nkn/fuselog/config/syslog#\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Fuse Log filename : "
               "#/nkn/fuselog/config/filename#\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Fuse Log filesize : "
               "#/nkn/fuselog/config/max-filesize# MiB\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Fuse Log time interval : "
               "#/nkn/fuselog/config/time-interval# Minutes\n"));
    bail_error(err);

    err = cli_printf_query
	    (_("  Fuse log On the hour log rotation : "
	       "#/nkn/fuselog/config/on-the-hour#\n"));

    err = mdc_get_binding_children(cli_mcc, NULL, NULL, true,
            &bindings, true, true, "/nkn/fuselog");
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings,"/nkn/fuselog/config/upload/url",NULL,&url);
    bail_error(err);

    if(ts_equal_str(url,"",true )){
         err = cli_printf("  Auto Copy URL : -Not Configured- \n" );
         bail_error(err);
    }
    else{
        err = lu_obfuscate_url_password(ts_str(url),
                                    &ret_url_pw_obfuscated,
                                    &ret_changed );
        bail_error(err);

        err = cli_printf("  Auto Copy URL :%s\n",ts_str(ret_url_pw_obfuscated  ));
        bail_error(err);
    }

bail:
    ts_free(&url);
    ts_free(&ret_url_pw_obfuscated);
    bn_binding_array_free(&bindings);
    return err;

}

int
cli_nkn_fuselog_show_continuous(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params)
{
	int err = 0;
	tstr_array *argv = NULL;
	const char *pattern = NULL;
	tstring *al_filename = NULL;
	const char *logfile = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	pattern = tstr_array_get_str_quick(params, 0);
	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
        	&al_filename, "/nkn/fuselog/config/filename", NULL); 
        bail_error(err);

        if(!al_filename) {
		goto bail;
	}

	logfile = smprintf("/var/log/nkn/%s", ts_str(al_filename));

    
	/*
	 * 'show log continuous [not] matching *'
	 */
	if (pattern) {
        	err = tstr_array_new_va_str(&argv, NULL, 4,
                	prog_tail_path, "-n",
                        pattern, logfile);
	        bail_error(err);

        	err = clish_run_program_fg(prog_tail_path, argv, NULL, NULL, NULL);
	        bail_error(err);
	}	

	/*
	 * 'show log continuous'
	 */
	else {
        	err = tstr_array_new_va_str(&argv, NULL, 3,
                	prog_tail_path, "-f",
                        logfile);
	        bail_error(err);
        
        	err = clish_run_program_fg(prog_tail_path, argv, NULL, NULL, NULL);
	        bail_error(err);
	}	

bail:
	ts_free(&al_filename);
	tstr_array_free(&argv);
	return(err);
}

int
 cli_fuselog_warning_message(void *data, const cli_command *cmd,
 			const tstr_array *cmd_line,
 			const tstr_array *params)
 {
 	int err = 0;
 	char *bn_name = NULL;
 	const char *time_interval = NULL;
 	uint32 ret = 0;
 	tstring *ret_msg = NULL;
 
	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	time_interval = tstr_array_get_str_quick(params, 0);
 	bail_null(time_interval);
 
 	bn_name = smprintf("/nkn/fuselog/config/time-interval");
 	bail_null(bn_name);
 
 	err = mdc_set_binding(cli_mcc, &ret, &ret_msg,
 			bsso_modify, bt_uint32, time_interval, bn_name);
 	if(ret) {
 		goto bail;
 
 	}else {
 	        err = cli_printf("warning: Setting a low value can lead to"
 				" High CPU utilization\n");
 	}
 bail:
 	safe_free(bn_name);
 	ts_free(&ret_msg);
 	return err;
 }
 
static int
cli_fuselog_time_interval_revmap(void *data, const cli_command *cmd,
                           const bn_binding_array *bindings,
                           const char *name,
                           const tstr_array *name_parts,
                           const char *value, const bn_binding *binding,
                           cli_revmap_reply *ret_reply)
{
 	int err = 0;
 	tstring *t_name = NULL;
 	tstring *rev_cmd = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(name_parts);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);
 
         err = bn_binding_array_get_value_tstr_by_name(bindings, name, NULL, &t_name);
         bail_error_null(err, t_name);
 
	 err = ts_new_sprintf(&rev_cmd, "fuselog rotate time-interval  %s\n", ts_str(t_name));
 	 bail_error(err);
 
 
 	 err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
 	 bail_error(err);
 	 err = tstr_array_append_str(ret_reply->crr_nodes, name);
 	 bail_error(err);
 	
 
bail:
 	ts_free(&t_name);
 	ts_free(&rev_cmd);
 	return err;
}
