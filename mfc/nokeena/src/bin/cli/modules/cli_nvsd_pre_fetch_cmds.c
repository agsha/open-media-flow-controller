/*
 * Filename :   cli_nvsd_pre_fetch.c
 * Date     :   06/09/2010
 * Author   :   Ramalingam Chandran
 *
 * (C) Copyright 2010 Juniper Networks, Inc.
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
#include "file_utils.h"
#include "ttime.h"
#include "nkn_defs.h"

/*----------------------------------------------------------------------------
 * Local Variables
 *---------------------------------------------------------------------------*/
enum {
cid_prefetch_immediate = 1,
cid_prefetch_time = 2,
cid_prefetch_time_date = 3
};

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
cli_nvsd_pre_fetch_cmds_init(
        const lc_dso_info   *info,
        const cli_module_context *context);

int
cli_nvsd_prefetch_hdlr(void *data,
                const cli_command       *cmd,
                const tstr_array        *cmd_line,
                const tstr_array        *params);

int cli_nvsd_prefetch_finish(const char *password,
                                clish_password_result result,
                                const cli_command *cmd,
                                const tstr_array *cmd_line,
                                const tstr_array *params,
                                void *data,
                                tbool *ret_continue);

int
cli_nvsd_prefetch_list_hdlr(void *data,
                const cli_command       *cmd,
                const tstr_array        *cmd_line,
                const tstr_array        *params);

int
cli_nvsd_prefetch_no_hdlr(void *data,
                const cli_command       *cmd,
                const tstr_array        *cmd_line,
                const tstr_array        *params);

int
cli_prefetch_get_names(
        tstr_array **ret_names,
        tbool hide_display);

int
cli_prefetch_name_help(
        void *data,
        cli_help_type help_type,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        void *context);

int
cli_pf_name_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);


/*----------------------------------------------------------------------------
 * FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
int
cli_nvsd_pre_fetch_cmds_init(
        const lc_dso_info   *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pre-fetch";
    cmd->cc_help_descr =        N_("Configure pre-fetch");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no pre-fetch";
    cmd->cc_help_descr =        N_("Delete the pre-fetch file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pre-fetch *";
    cmd->cc_help_exp =          N_("<scp://username:password@hostname/path/filename>");
    cmd->cc_help_exp_hint =     N_("SCP path of the text file which contains the list of urls");
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_flags =             ccf_terminal | ccf_sensitive_param | ccf_ignore_length;
    cmd->cc_exec_callback =     cli_nvsd_prefetch_hdlr;
    cmd->cc_magic = 		cid_prefetch_immediate;
    cmd->cc_revmap_type    =    crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pre-fetch * fetch-time";
    cmd->cc_help_descr =        N_("Configure the fetch time and date");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pre-fetch * fetch-time *";
    cmd->cc_help_exp =          cli_msg_time;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_callback =     cli_nvsd_prefetch_hdlr;
    cmd->cc_magic = 		cid_prefetch_time;
    cmd->cc_revmap_type    =    crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pre-fetch * fetch-time * *";
    cmd->cc_help_exp =          cli_msg_date;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_callback =     cli_nvsd_prefetch_hdlr;
    cmd->cc_magic = 		cid_prefetch_time_date;
    cmd->cc_revmap_type    =    crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no pre-fetch *";
    cmd->cc_help_exp =          N_("<filename>");
    cmd->cc_help_exp_hint =     N_("Specify the file name to stop the prefetch");
    cmd->cc_help_callback =     cli_prefetch_name_help;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/nvsd/prefetch/config/*";
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_callback =     cli_nvsd_prefetch_no_hdlr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show pre-fetch";
    cmd->cc_help_descr =        N_("List the scheduled url files");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show pre-fetch list";
    cmd->cc_help_descr  =       N_("List the scheduled url files scheduled for download");
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_callback =     cli_nvsd_prefetch_list_hdlr;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va(4, 
		    "/nkn/prefetch/config/*",
		    "/nkn/prefetch/config/*/time",
		    "/nkn/prefetch/config/*/date", 
		    "/nkn/prefetch/config/*/status");
bail:
    return err;
}

/****************************************************************************/
/* Function to Delete the scheduled job                                     */
/****************************************************************************/
int 
cli_nvsd_prefetch_no_hdlr(void *data,
		const cli_command 	*cmd,
		const tstr_array 	*cmd_line,
		const tstr_array	*params)
{
    int err = 0;
    const char* filename = NULL;
    int32 status = 0;
    tstring *ret_output = NULL;
    char *prefetch_status_node = NULL;
    tbool is_exist = false;
    tstr_array *names = NULL;
    uint32 list_num = 0, i = 0;
    tbool lock_exist = false;
    char *path = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    /*Get the file name to delete*/
    filename = tstr_array_get_str_quick(params, 0);
    bail_error_null(err, filename);

    /*Get the job list*/
    err = cli_prefetch_get_names(&names, true);
    bail_error_null(err, names);

    list_num = tstr_array_length_quick(names);

    /*If there is no jobs are available*/
    if (list_num == 0){
        cli_printf("The job is not available.\n");
	goto bail;
    }else{
         for (i = 0; i  < list_num; ++i){
	    if (strcmp(filename, tstr_array_get_str_quick(names, i)) == 0) {
		is_exist = true;
		break;
	    }
         }
    }
    
    if (is_exist){
    	/*Delete the scheduled uri list file*/
    	err = lc_launch_quick_status(&status, &ret_output, true, 3, "/opt/nkn/bin/prefetch.py", "delete", filename);    
    	bail_error(err);

	cli_printf("%s\n", ts_str(ret_output));

	/*if the lock file exists dont continue*/
	path = smprintf("/nkn/prefetch/lockfile/%s.lck", filename);
        lf_test_exists(path, &lock_exist);	
	if (lock_exist)
		goto bail;

        prefetch_status_node = smprintf("/nkn/prefetch/config/%s/status", filename);
        bail_null(prefetch_status_node);

        err = mdc_set_binding(cli_mcc,
            NULL,
            NULL,
            bsso_modify,
            bt_string,
            "delete",
            prefetch_status_node);
        bail_error(err);
        cli_printf("The job is successfully deleted\n");

    }else {
	cli_printf("The job does not exists in the scheduled list\n");
    }

bail:
    ts_free(&ret_output);
    safe_free(prefetch_status_node);
    safe_free(path);
    tstr_array_free(&names);
    return err;

}

/****************************************************************************/
/* Function to list the scheduled jobs                                      */
/****************************************************************************/
int
cli_nvsd_prefetch_list_hdlr(void *data,
		const cli_command	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array 	*params)
{
    int err = 0;
    tstr_array *names = NULL;
    uint32 list_num = 0, i = 0;
    
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    /*Get the job list*/
    err = cli_prefetch_get_names(&names, true);
    bail_error_null(err, names);

    list_num = tstr_array_length_quick(names);	

    /*If there is no jobs are available*/
    if (list_num == 0){
	cli_printf("No scheduled jobs are available.\n");
    }else{
        /*Print the scheduled jobs*/
        err = cli_printf_query(_("    %16s    %s        %s          %s\n\n"), 
				"Job Name", "Time", "Date", "Status");
		for (i = 0; i  < list_num; ++i)
		{
			err = cli_printf_query(_
			("    %16s    #/nkn/prefetch/config/%s/time#   " 
				      "#/nkn/prefetch/config/%s/date#   "
			     	      " #/nkn/prefetch/config/%s/status#\n"),
					tstr_array_get_str_quick(names, i),
					tstr_array_get_str_quick(names, i),
					tstr_array_get_str_quick(names, i),
					tstr_array_get_str_quick(names, i));
			bail_error(err);
		}
    }
bail:
    tstr_array_free(&names);
    return (err);
}

/****************************************************************************/
/* Function to get the Job name and status                                  */
/****************************************************************************/

int 
cli_prefetch_get_names(
        tstr_array **ret_names,
        tbool hide_display)
{
    int err = 0;
    tstr_array *names = NULL;
    tstr_array_options opts;
    tstr_array *names_config = NULL;
    char *bn_name = NULL;
    bn_binding *display_binding = NULL;

    UNUSED_ARGUMENT(hide_display);

    bail_null(ret_names);
    *ret_names = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&names, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL, NULL, &names_config, 
            "/nkn/prefetch/config", NULL);
    bail_error_null(err, names_config);

    err = tstr_array_concat(names, &names_config);
    bail_error(err);

    *ret_names = names;
    names = NULL;
bail:
    tstr_array_free(&names);
    tstr_array_free(&names_config);
    safe_free(bn_name);
    bn_binding_free(&display_binding);
    return err;
}

/****************************************************************************/
/* Function to trigger the prefetch download                                */
/****************************************************************************/
int
cli_nvsd_prefetch_hdlr(void *data,
		const cli_command 	*cmd,
		const tstr_array	*cmd_line,
		const tstr_array	*params)
{
    int err = 0;
    const char *remote_url = NULL;

    UNUSED_ARGUMENT(data);

    /*Get the SCP URL*/
    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    if (clish_is_shell()) {
        err = clish_maybe_prompt_for_password_url
                (remote_url, cli_nvsd_prefetch_finish, cmd, cmd_line, params, NULL);
        bail_error(err);
    }
    else {
        err = cli_nvsd_prefetch_finish(NULL, cpr_ok, cmd, cmd_line, params,
                    NULL, NULL);
        bail_error(err);
    }

bail:
	return err;
}

/****************************************************************************/
/* Function to download the url file into MFC                               */
/****************************************************************************/
int 
cli_nvsd_prefetch_finish(const char *password,
			clish_password_result result,
			const cli_command *cmd,
		     	const tstr_array *cmd_line,
		        const tstr_array *params,
			void *data,
			tbool *ret_continue)
{
    int err = 0;
    const char *remote_url = NULL;
    bn_request *req = NULL;
    const char *inptime = NULL;
    const char *inpdate = NULL;
    const char *job_status = NULL; 
    lt_date current_date;
    lt_time_sec current_time;
    uint32 ret_err = 0;
    tstring *ret_msg = NULL;
    tstr_array *name_parts = NULL;
    uint32 length = 0;
    char *prefetch_node = NULL;
    char *prefetch_time_node = NULL;
    char *prefetch_date_node = NULL;
    char *prefetch_status_node = NULL;
    const char *filename = NULL;
    char *date_str = NULL;
    char *time_str = NULL;
    const char *print_msg = NULL;

    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(ret_continue);

    if (result != cpr_ok) {
        goto bail;
    }

    /*Get the SCP URL*/
    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    /*Check whether it is an valid SCP URL*/
    if(! lc_is_prefix("scp://", remote_url, false))
    {
        cli_printf_error(_("Bad source specifier\n"));
	goto bail;
    }

    err = lt_time_to_date_daytime_sec(lt_curr_time(), &current_date,
                                          &current_time);
    bail_error(err);

    err = lt_date_to_str(current_date, &date_str, NULL);
    bail_error(err);

    err = lt_daytime_sec_to_str(current_time, &time_str);
    bail_error(err);


    switch (cmd->cc_magic) {

    case cid_prefetch_immediate:
        
        job_status = "immediate";
	print_msg = "The prefetch job is in progress";
	break;

    case cid_prefetch_time:

        inptime = tstr_array_get_str_quick(params, 1);
        bail_error_null(err,inptime);
      
        strlcpy(time_str, inptime, 9);
	job_status = "scheduled";
        print_msg = "The prefetch job is successfully scheduled";
	break;

    case cid_prefetch_time_date:

        inptime = tstr_array_get_str_quick(params, 1);
        inpdate = tstr_array_get_str_quick(params, 2);

        strlcpy(time_str, inptime, 9);
        strlcpy(date_str, inpdate, 11);
	job_status = "scheduled";
        print_msg = "The prefetch job is successfully scheduled";
	break;
    }

    /*Sanity for the time check to be greater than the current time*/
    lt_date f_date;
    lt_time_sec f_daytimesec;
    lt_time_sec f_timesec;
    lt_time_sec c_timesec;
  
    const char *itimestr = time_str;
    const char *idatestr = date_str;
    
    c_timesec = lt_curr_time();
    lt_str_to_date (idatestr, &f_date);
    lt_str_to_daytime_sec (itimestr, &f_daytimesec);
    lt_date_daytime_to_time_sec (f_date, f_daytimesec, &f_timesec);

    if (f_timesec < c_timesec) {
    	cli_printf_error("Enter a valid time and date\n");
	goto bail;
    }  

    /*Creating Action item to trigger the download*/
    err = bn_action_request_msg_create(&req, "/file/transfer/download");
    bail_error(err);

    bail_null(remote_url);
    err = bn_action_request_msg_append_new_binding 
		(req, 0,"remote_url", bt_string, remote_url, NULL);
    bail_error(err);

    if (password) {
	err = bn_action_request_msg_append_new_binding
                (req, 0,"password", bt_string, password, NULL);
   	bail_error(err);			
    }

    /*Specify the location in MFC where the file should be downloaded*/
    err = bn_action_request_msg_append_new_binding
                (req, 0,"local_dir", bt_string, "/nkn/prefetch/jobs", NULL);
    bail_error(err);

    /*Permission to overwrite the downloaded file*/
    err = bn_action_request_msg_append_new_binding
               (req, 0, "allow_overwrite", bt_bool, "true", NULL);
    bail_error(err);

    /*Trigger the action to download the file*/
    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &ret_err, &ret_msg);
    bail_error(err);    

 
    if (!ret_err)
    {
	err =   ts_tokenize_str(remote_url, '/', '\\', '"', ttf_ignore_leading_separator, &name_parts);
    	length = tstr_array_length_quick(name_parts);
    	filename = tstr_array_get_str_quick(name_parts, (length-1));

	prefetch_time_node = smprintf("/nkn/prefetch/config/%s/time", filename);
        bail_null(prefetch_time_node);
	prefetch_date_node = smprintf("/nkn/prefetch/config/%s/date", filename);
	bail_null(prefetch_date_node);
	prefetch_status_node = smprintf("/nkn/prefetch/config/%s/status", filename);
	bail_null(prefetch_status_node);

    	prefetch_node = smprintf("/nkn/prefetch/config/%s", filename);
        bail_null(prefetch_node);
	
	ret_err = 0;

    	err = mdc_create_binding(cli_mcc, &ret_err, NULL,
            bt_string, filename, prefetch_node);
	bail_error(err);

	if (ret_err){
	    goto bail;
	}

        err = mdc_set_binding(cli_mcc,
            &ret_err,
            NULL,
            bsso_modify,
            bt_time_sec,
	    time_str,
            prefetch_time_node);
    	bail_error(err);

        if (ret_err){
            goto bail;
        }
        
	err = mdc_set_binding(cli_mcc,
	    &ret_err,
            NULL,
            bsso_modify,
            bt_date,
	    date_str,
            prefetch_date_node);
        bail_error(err);

        if (ret_err){
            goto bail;
        }

        err = mdc_set_binding(cli_mcc,
            NULL,
            NULL,
            bsso_modify,
            bt_string,
	    job_status,
            prefetch_status_node);
        bail_error(err);

        cli_printf("%s\n", print_msg);
    }

bail:
    tstr_array_free(&name_parts);
    safe_free(prefetch_node);
    safe_free(prefetch_time_node);
    safe_free(prefetch_date_node);
    safe_free(prefetch_status_node);
    safe_free(time_str);
    safe_free(date_str);
    ts_free (&ret_msg);
    bn_request_msg_free(&req);
    return err;
}


/****************************************************************************/
//Help Handler function to list the scheduled jobs
/****************************************************************************/

int
cli_prefetch_name_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context)
{
    int err = 0;
    tstr_array *names = NULL;
    const char *prefetchname = NULL;
    int i = 0, num_names = 0;


    switch(help_type)
    {
    	case cht_termination:
            if(cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
                err = cli_add_termination_help(context, 
                	GT_(cmd->cc_help_term_prefix, cmd->cc_gettext_domain));
            	bail_error(err);
            }
            else {
                err = cli_add_termination_help(context,
                        GT_(cmd->cc_help_term, cmd->cc_gettext_domain));
                bail_error(err);
            }
        break;

        case cht_expansion:
            if(cmd->cc_help_exp) {
               err = cli_add_expansion_help(context,
                        GT_(cmd->cc_help_exp, cmd->cc_gettext_domain),
                        GT_(cmd->cc_help_exp_hint, cmd->cc_gettext_domain));
               bail_error(err);
            }

            err = tstr_array_new(&names, NULL);
            bail_error(err);

            err = cli_pf_name_completion(data, cmd, cmd_line, params, 
                    curr_word, names);
            bail_error(err);

            num_names = tstr_array_length_quick(names);
            for(i = 0; i < num_names; ++i) {
                prefetchname = tstr_array_get_str_quick(names, i);
                bail_null(prefetchname);
            
                err = cli_add_expansion_help(context, prefetchname, NULL);
                bail_error(err);
            }
        break;
    
        default:
            bail_force(lc_err_unexpected_case);
        break;
    }
bail:
    tstr_array_free(&names);
    return err;
}


/****************************************************************************/
//Function for prefetch job name completion
/****************************************************************************/
int
cli_pf_name_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *names = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_prefetch_get_names(&names, true);
    bail_error(err);

    err = tstr_array_concat(ret_completions, &names);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);

bail:
    tstr_array_free(&names);
    return err;
}
