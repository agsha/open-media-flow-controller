/*
 * Filename:    cli_nkn_crawllog_cmds.c
 * Date:        2012/02/06
 * Author:      Manikandan V
 *
 * (C) Copyright 2012,  Juniper Networks Inc.
 *  All rights reserved.
 */

#include "common.h"
#include "bnode.h"
#include <climod_common.h>
#include <dso.h>
#include <env_utils.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <alloca.h>
#include "tstring.h"
#include "random.h"
#include "cli_module.h"
#include "clish_module.h"
#include "file_utils.h"
#include "proc_utils.h"
#include "libnkncli.h"
#include "url_utils.h"
#include "tpaths.h"
#include "nkn_defs.h"
#include "type_conv.h"
#include <sys/stat.h>
#include "nkn_mgmt_defs.h"

cli_expansion_option cli_nkn_crawllog_upload_opts[] = {
    {"current", N_("Current log file"), NULL},
    {"all", N_("All log files"), NULL},
    {NULL, NULL, NULL}
};


int 
cli_nkn_crawllog_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_nkn_crawllog_show_continuous(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params);

int
cli_grab_password(
        const char *password,
        clish_password_result result,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        void *data,
        tbool *ret_continue);

int
cli_nkn_crawllog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int 
cli_nkn_crawllog_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload crawllog";
    cmd->cc_help_descr =            N_("Upload a crawl log file.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload crawllog *";
    cmd->cc_help_options =          cli_nkn_crawllog_upload_opts;
    cmd->cc_help_num_options =      sizeof(cli_nkn_crawllog_upload_opts)/sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload crawllog * *";
    cmd->cc_help_exp =              cli_msg_url;
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_crawllog_upload;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show crawllog";
    cmd->cc_help_descr =            N_("View cache log configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show crawllog continuous";
    cmd->cc_help_descr =            N_("View continuous cache log");
    cmd->cc_flags =                 ccf_terminal; 
    cmd->cc_capab_required =        ccf_available_to_all;
    cmd->cc_exec_callback =         cli_nkn_crawllog_show_continuous;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show crawllog last";
    cmd->cc_help_descr =            N_("View last lines of cache log");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show crawllog last *";
    cmd->cc_help_exp  =             N_("<number>");
    cmd->cc_help_exp_hint  =        N_("Number of lines to be displayed>");
    cmd->cc_flags =                 ccf_terminal; 
    cmd->cc_capab_required =        ccf_available_to_all;
    cmd->cc_exec_callback =         cli_nkn_crawllog_show_continuous;
    CLI_CMD_REGISTER;

bail:
    return err;
}

int
cli_nkn_crawllog_show_continuous(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params)
{
	int err = 0;
	tstr_array *argv = NULL;
	const char *pattern = NULL;
	char cl_filename[50] = "crawl.log";
	const char *logfile = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	pattern = tstr_array_get_str_quick(params, 0);

	logfile = smprintf("/var/log/nkn/%s", cl_filename);

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
	tstr_array_free(&argv);
	return(err);
}


int
cli_grab_password(
        const char *password,
        clish_password_result result,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        void *data,
        tbool *ret_continue)
{
    int err = 0;
    tstr_array *names = (tstr_array *) data;
    uint32 nfiles = 0;
    uint32 idx = 0;
    const char *remote_url = NULL;
    char dir[50] = "/var/log/nkn";
    const char *filename = NULL;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(ret_continue);

    if ( result != cpr_ok ) {
        goto bail;
    }

 	remote_url = tstr_array_get_str_quick(params, 1);
	bail_null(remote_url);

    nfiles = tstr_array_length_quick(names);

    lc_log_basic(LOG_NOTICE, _("Nfiles:%u, dir:%s\n"),nfiles,dir);

    for(idx = 0; idx < nfiles; idx++) {
        filename = tstr_array_get_str_quick(names, idx);
        bail_null(filename);
        cli_printf(_("Uploading ... %s\n"), filename);
        if (password) {
            err = mdc_send_action_with_bindings_str_va
                    (cli_mcc, NULL, NULL, "/file/transfer/upload", 4,
                    "local_dir", bt_string, dir,
                    "local_filename", bt_string, filename,
                    "remote_url", bt_string, remote_url,
                    "password", bt_string, password);
            bail_error(err);
        }
        else {
            err = mdc_send_action_with_bindings_str_va
                (cli_mcc, NULL, NULL, "/file/transfer/upload", 3,
                "local_dir", bt_string, dir,
                "local_filename", bt_string, filename,
                "remote_url", bt_string, remote_url);
            bail_error(err);
        }
    }

bail:
    tstr_array_free(&names);
    return err;
}

int
cli_nkn_crawllog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;
    const char *mode = NULL;
    tstr_array *names = NULL; /* freed in grap_password*/
    char *password = NULL;
    tbool prompt_pass = false;
    UNUSED_ARGUMENT(data);

    mode = tstr_array_get_str_quick(params, 0);
    bail_null(mode);

    if ( (strcmp(mode, "current") != 0) &&
         (strcmp(mode, "all") != 0) ) {
        cli_printf_error(_("Unrecognized command \"%s\"."), mode);
        goto bail;
    }

    remote_url = tstr_array_get_str_quick(params, 1);
    bail_null(remote_url);

    if ( strcmp(mode, "current") == 0) {
    	if (lc_is_prefix("scp://", remote_url, false)) {
    		err = tstr_array_new(&names, NULL);
    		bail_error(err);

    		err = tstr_array_append_str(names, "crawl.log");
    		bail_error(err);

    		err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &prompt_pass,
                     "/cli/prompt/empty_password", NULL);
    		bail_error(err);

    		if (prompt_pass) {
    			err = lu_parse_scp_url(remote_url, NULL, &password, NULL, NULL, NULL);

    			if(err) {
    				err = cli_printf_error("Invalid SCP URL format.\n");
    				bail_error(err);
    				goto bail;
    			}

    			if(password == NULL) {
    				err = clish_prompt_for_password(
    						true, NULL, false,
    						cli_grab_password, cmd, cmd_line,
    						params, (void*)names);
    				bail_error(err);
    			}
    			else {
    				err = cli_grab_password(NULL, cpr_ok, cmd,
    						cmd_line, params, (void*)names, NULL);
    				bail_error(err);
    			}
    		}
    	}
    	else {

    		cli_printf_error(_("Bad destination specifier"));

    		goto bail;
       }
    }
    else {
        err = cli_file_get_filenames("/var/log/nkn", &names);
        bail_error_null(err, names);

        err = tstr_array_delete_non_prefixed(names, "crawl");
        bail_error(err);

        if (lc_is_prefix("scp://", remote_url, false)) {
            err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &prompt_pass,
                    "/cli/prompt/empty_password", NULL);
            bail_error(err);
            if (prompt_pass) {
                err = lu_parse_scp_url(remote_url, NULL, &password, NULL, NULL, NULL);
                if(err) {
                    err = cli_printf_error("Invalid SCP URL format.\n");
                    bail_error(err);
                    goto bail;
                }

                if(password == NULL) {
                    err = clish_prompt_for_password(
                            true, NULL, false,
                            cli_grab_password, cmd, cmd_line,
                            params, (void*)names);
                    bail_error(err);
                }
                else {
                    err = cli_grab_password(NULL, cpr_ok, cmd,
                            cmd_line, params, (void*)names, NULL);
                    bail_error(err);
                }
            }
        }
        else {
            cli_printf_error(_("Bad destination specifier"));
            goto bail;
        }
    }

bail:
    return err;
}


