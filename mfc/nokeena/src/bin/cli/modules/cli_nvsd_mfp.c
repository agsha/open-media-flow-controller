/*
 *
 * Filename:  cli_nvsd_mfp.c
 * Date:      2011/04/28
 * Author:    Varsha Rajagopalan
 *
 * (C) Copyright 2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include "common.h"
#include <climod_common.h>
#include <dso.h>
#include <env_utils.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tstring.h"
#include "random.h"
#include "cli_module.h"
#include "clish_module.h"
#include "file_utils.h"
#include "proc_utils.h"
#include "libnkncli.h"
#include "nkn_defs.h"

#define NKNCNT_FILE_PREFIX 	"/var/opt/tms/sysdumps/"
#define	NKNCNT_FILE	"counters"
#define	NKNCNT_BINARY_PATH	"/opt/nkn/bin/nkncnt"
#define NKNCNT_FILE_PREFIX 	"/var/opt/tms/sysdumps/"
#define FILE_PUBLISHER    "/opt/nkn/sbin/file_mfpd"
#define LIVE_PUBLISHER    "/opt/nkn/sbin/live_mfpd"

#define MAX_NODE_LENGTH 512
#define FILE_SESSIONS  1
#define LIVE_SESSIONS 2
//const int32 cli_mfp_init_order = INT32_MAX - 3;

int cli_nvsd_mfp_init(const lc_dso_info *info,
		    const cli_module_context *context);
cli_expansion_option cli_set_default[] = {
	{ "_", N_("Display all (use defaults)"), NULL},
	{ NULL, NULL, NULL },
};

/*
 *
 */

/* set frequency , save file and durationi wrapper function */
#if 0
static int
mfp_set_fre_sav_dur(
		tbool found,
		const char *pattern_str,
		const char *time_frequency,
		const char *time_duration,
		const char *file_name,
		tstr_array ** arr);
#endif

int
cli_mfp_live_show_counters_internal_cb(void *data,
	const cli_command *cmd, const tstr_array *cmd_line,
	const tstr_array *params);

int
cli_mfp_file_show_counters_internal_cb(void *data,
	const cli_command *cmd, const tstr_array *cmd_line,
        const tstr_array *params);

extern int
cli_std_exec(void *data, const cli_command *cmd,
	const tstr_array *cmd_line, const tstr_array *params);
int
cli_session_name_completion(void *data,
	const cli_command *cmd,	const tstr_array *cmd_line,
	const tstr_array *params, const tstring *curr_word,
	tstr_array *ret_completions);

int
cli_session_name_help(void *data,
	cli_help_type help_type, const cli_command *cmd,
	const tstr_array *cmd_line, const tstr_array *params,
	const tstring *curr_word, void *context);
int
cli_mfp_show_counters_internal_cb(void *data,
	const cli_command *cmd, const tstr_array *cmd_line,
	const tstr_array *params);

static int
cli_get_list_of_sessions(tstr_array **sess_names, tbool isfile);

static int
mfp_generate_file_name_for_counters(const char *sess_name, char **file_name);

int cli_reg_mfp_stats_cmds(const lc_dso_info *info,
	const cli_module_context *context);

/* ------------------------------------------------------------------------- */
int
cli_nvsd_mfp_init(const lc_dso_info *info,
	const cli_module_context *context)
{
    int err = 0;

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }
    err = cli_reg_mfp_stats_cmds(info, context);




bail:
    return (err);
}


int
cli_reg_mfp_stats_cmds(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str           =    "show counters live-pub";
    cmd->cc_help_descr          =    N_("Display LIVE publisher counters");
    cmd->cc_flags 		=    ccf_terminal ;
    cmd->cc_capab_required 	=    ccp_query_basic_curr;
    cmd->cc_exec_callback 	=    cli_mfp_live_show_counters_internal_cb;
    cmd->cc_exec_data 		=    NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           =    "show counters live-pub session-name";
    cmd->cc_help_descr          =     N_("Describes some key stats and counters for a session in LIVE publisher");
    cmd->cc_exec_data 		=     NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           =     "show counters file-pub";
    cmd->cc_help_descr          =     N_("Display FILE publisher counters");
    cmd->cc_flags 		=     ccf_terminal ;
    cmd->cc_capab_required 	=     ccp_query_basic_curr;
    cmd->cc_exec_callback 	=     cli_mfp_file_show_counters_internal_cb;
    cmd->cc_exec_data 		=     NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           =     "show counters file-pub session-name";
    cmd->cc_help_descr          =      N_("Describes some key stats and counters for the session in FILE publisher");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=     	"show counters live-pub session-name *";
    cmd->cc_help_exp 		=      	N_("<name>");
    cmd->cc_help_callback       =       cli_session_name_help;
    cmd->cc_comp_callback       =       cli_session_name_completion;
    cmd->cc_flags 		=	ccf_terminal;
    cmd->cc_capab_required 	= 	ccp_query_basic_curr;
    cmd->cc_exec_callback 	= 	cli_mfp_live_show_counters_internal_cb;
    cmd->cc_exec_data 		=     	NULL;
    cmd->cc_magic	        =       LIVE_SESSIONS;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=     	"show counters file-pub session-name * ";
    cmd->cc_help_exp 		=      	N_("<name>");
    cmd->cc_help_callback       =       cli_session_name_help;
    cmd->cc_comp_callback       =       cli_session_name_completion;
    cmd->cc_flags 		=	ccf_terminal;
    cmd->cc_capab_required 	= 	ccp_query_basic_curr;
    cmd->cc_exec_callback 	= 	cli_mfp_file_show_counters_internal_cb;
    cmd->cc_exec_data 		=     	NULL;
    cmd->cc_magic	        =       FILE_SESSIONS;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=     	"show counters live-pub internal";
    cmd->cc_help_descr          =      N_("Describes all the counters for the LIVE publisher");
    cmd->cc_flags 		=	ccf_hidden| ccf_terminal;
    cmd->cc_capab_required 	= 	ccp_query_basic_curr;
    cmd->cc_exec_callback 	= 	cli_mfp_show_counters_internal_cb;
    cmd->cc_exec_data 		=     	NULL;
    cmd->cc_magic	        =       LIVE_SESSIONS;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=     	"show counters file-pub internal";
    cmd->cc_help_descr          =      N_("Display all the counters for the FILE publisher");
    cmd->cc_flags 		=	ccf_hidden | ccf_terminal;
    cmd->cc_capab_required 	= 	ccp_query_basic_curr;
    cmd->cc_exec_callback 	= 	cli_mfp_show_counters_internal_cb;
    cmd->cc_exec_data 		=     	NULL;
    cmd->cc_magic	        =       FILE_SESSIONS;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           =       "show counters live-pub internal *";
    cmd->cc_help_exp            =       N_("<name>");
    cmd->cc_help_exp_hint       =       N_("Counter pattern name");
    cmd->cc_help_options        =       cli_set_default;
    cmd->cc_help_num_options    =       sizeof(cli_set_default) /
					sizeof(cli_expansion_option);
    cmd->cc_comp_type           =       cct_use_help_options;
    cmd->cc_flags               =       ccf_terminal;
    cmd->cc_capab_required      =       ccp_query_basic_curr;
    cmd->cc_exec_callback       =       cli_mfp_show_counters_internal_cb;
    cmd->cc_exec_data           =       NULL;
    cmd->cc_magic	        =       LIVE_SESSIONS;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           =       "show counters file-pub internal *";
    cmd->cc_help_exp            =       N_("<name>");
    cmd->cc_help_exp_hint       =       N_("Counter pattern name");
    cmd->cc_help_options        =       cli_set_default;
    cmd->cc_help_num_options    =       sizeof(cli_set_default) /
					sizeof(cli_expansion_option);
    cmd->cc_comp_type           =       cct_use_help_options;
    cmd->cc_flags               =       ccf_terminal;
    cmd->cc_capab_required      =       ccp_query_basic_curr;
    cmd->cc_exec_callback       =       cli_mfp_show_counters_internal_cb;
    cmd->cc_exec_data           =       NULL;
    cmd->cc_magic	        =       FILE_SESSIONS;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           =       "show counters live-pub internal * save";
    cmd->cc_help_descr          =       N_("To save all thecounters of LIVE publisher to a file");
    cmd->cc_flags               =       ccf_terminal;
    cmd->cc_capab_required      =       ccp_query_basic_curr;
    cmd->cc_exec_callback       =       cli_mfp_show_counters_internal_cb;
    cmd->cc_exec_data           =       NULL;
    cmd->cc_magic	        =       LIVE_SESSIONS;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           =       "show counters file-pub internal * save";
    cmd->cc_help_descr          =       N_("To save all thecounters of FILE publisher to a file");
    cmd->cc_flags               =       ccf_terminal;
    cmd->cc_capab_required      =       ccp_query_basic_curr;
    cmd->cc_exec_callback       =       cli_mfp_show_counters_internal_cb;
    cmd->cc_exec_data           =       NULL;
    cmd->cc_magic	        =       FILE_SESSIONS;
    CLI_CMD_REGISTER;

bail:
    return err;
}
#if 0
static int 
mfp_set_fre_sav_dur( 
		tbool found,
		const char *pattern_str, 
		const char *time_frequency, 
		const char *time_duration,
		const char *file_name,
		tstr_array **arr)
{
	int err = 0;

	err = tstr_array_new ( arr , NULL);
	bail_error (err);
	err = tstr_array_append_str (*arr, NKNCNT_BINARY_PATH);
	bail_error (err);
	if (time_frequency)
	{
		err = tstr_array_append_str (*arr, "-t");	
		bail_error(err);
		err = tstr_array_append_str (*arr, time_frequency);	
		bail_error(err);
	} else {
		err = tstr_array_append_str (*arr, "-t");	
		bail_error(err);
		err = tstr_array_append_str (*arr, "0");	
		bail_error(err);
	}

	if (time_duration)
	{
		err = tstr_array_append_str (*arr, "-l");	
		bail_error(err);
		err = tstr_array_append_str (*arr, time_duration);	
		bail_error(err);
	} else {
		err = tstr_array_append_str (*arr, "-l");	
		bail_error(err);
		err = tstr_array_append_str (*arr, "0");	
		bail_error(err);
	}
	if (file_name)
	{
		err = tstr_array_append_str (*arr, "-S");	
		bail_error(err);
		err = tstr_array_append_str (*arr, file_name);	
		bail_error(err);
	}
	if (found)
	{
		err = tstr_array_append_str (*arr, "-s");
		bail_error(err);
		err = tstr_array_append_str (*arr, "_");
		bail_error(err);
	} else {
		err = tstr_array_append_str (*arr, "-s");
		bail_error(err);
		err = tstr_array_append_str (*arr, pattern_str); 
		bail_error(err);
	}

bail:
	return err;

}
#endif

static int
mfp_generate_file_name_for_counters(const char *sess_name,
				char **file_name) 
{


	int err = 0;
	tstring *hostname = NULL;
	time_t timeval = 0;
	struct tm *timeinfo = NULL;
	char time_string_buffer[40] = { 0 };
	
	bail_null(sess_name);
        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &hostname,
                          "/system/hostname", NULL);
        bail_error(err);
        bail_null_msg(hostname,"host name is null some thing is wrong");

	/* get current time */
	time(&timeval);
	timeinfo = localtime (&timeval);

	/* format current time to yearmonthday-hourminutesecond format */
	strftime (time_string_buffer, sizeof(time_string_buffer), "%y%m%d-%H%M%S",timeinfo);
	*file_name = smprintf("%s%s-%s-%s-%s.txt", NKNCNT_FILE_PREFIX, sess_name, NKNCNT_FILE, ts_str(hostname), time_string_buffer);
	bail_null_msg(*file_name,"filename generated is null");

bail:
	ts_free(&hostname);
	return err;

}

int
cli_mfp_live_show_counters_internal_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	int num_params = 0;
	tstr_array *argv = NULL;
	char *name = NULL;
	const char *grep_pattern = NULL;
	char t_session_node[MAX_NODE_LENGTH];
	tstring *curr_name = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(params);

	/* Get the number of parameters first */
	num_params = tstr_array_length_quick(cmd_line);

	err = tstr_array_new ( &argv , NULL);
	bail_error (err);
	err = tstr_array_append_str (argv, "NKNCNT_BINARY_PATH");
	bail_error (err);
	err = tstr_array_append_str (argv, "-P");
	bail_error (err);
	err = tstr_array_append_str (argv, LIVE_PUBLISHER);
	bail_error (err);
	if(num_params == 3) {  /*sh counters live-pub*/
	    /*Just the global level stats is needed*/
	    err = tstr_array_append_str (argv, "-s");
	    bail_error (err);
	    err = tstr_array_append_str (argv, "glob_mfp_live");
	    bail_error (err);
	} else if (num_params == 5) { /*sh counters live-pub session-name <session>*/
	    err = tstr_array_append_str (argv, "-s");
	    bail_error (err);
	    grep_pattern = tstr_array_get_str_quick(cmd_line, 4); //The last parameter is the session name
	    bail_null(grep_pattern);
	    snprintf(t_session_node, sizeof(t_session_node), "/nkn/nvsd/mfp/config/%s/name", grep_pattern);
	    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &curr_name,
		    t_session_node, NULL);
	    bail_error(err);
	    err = tstr_array_append_str (argv, ts_str(curr_name));
	    bail_error (err);
	}

	err = nkn_clish_run_program_fg(NKNCNT_BINARY_PATH, argv, NULL, NULL, NULL);
	bail_error(err);
bail:
    safe_free(name);
    tstr_array_free(&argv);
    ts_free(&curr_name);
    return err;

} /* end of cli_nkn_show_counters_internal_cb() */

int
cli_mfp_file_show_counters_internal_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	int num_params = 0;
	tstr_array *argv = NULL;
	char *name = NULL;
	const char *grep_pattern = NULL;
	char t_session_node[MAX_NODE_LENGTH];

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(params);

	/* Get the number of parameters first */
	num_params = tstr_array_length_quick(cmd_line);
	lc_log_basic(LOG_NOTICE, _("Num params:%d\n"), num_params);

	err = tstr_array_new ( &argv , NULL);
	bail_error (err);
	err = tstr_array_append_str (argv, "NKNCNT_BINARY_PATH");
	bail_error (err);
	err = tstr_array_append_str (argv, "-P");
	bail_error (err);
	err = tstr_array_append_str (argv, FILE_PUBLISHER);
	bail_error (err);
	if(num_params == 3) {  /*sh counters file-pub*/
	    /*Just the global level stats is needed*/
	    err = tstr_array_append_str (argv, "-s");
	    bail_error (err);
	    err = tstr_array_append_str (argv, "glob_mfp_file");
	    bail_error (err);
	} else if (num_params == 5) { /*sh counters file-pub session-name <session>*/
	    tstring *curr_name = NULL; 
	    err = tstr_array_append_str (argv, "-s");
	    bail_error (err);
	    grep_pattern = tstr_array_get_str_quick(cmd_line, 4); //The last parameter is the session name
	    bail_null(grep_pattern);
	    snprintf(t_session_node, sizeof(t_session_node), "/nkn/nvsd/mfp/config/%s/name", grep_pattern);
	    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &curr_name,
		    t_session_node, NULL);
	    bail_error(err);
	    err = tstr_array_append_str (argv, ts_str(curr_name));
	    bail_error (err);
	}
	err = nkn_clish_run_program_fg(NKNCNT_BINARY_PATH, argv, NULL, NULL, NULL);
	bail_error(err);

bail:
    safe_free(name);
    tstr_array_free(&argv);
    return err;

} /* end of cli_nkn_show_counters_internal_cb() */
static int
cli_get_list_of_sessions(
	tstr_array **sess_names, tbool isfile)
{
    int err = 0;
    tstr_array *names = NULL;
    tstr_array_options opts;
    tstr_array *names_config = NULL;

    UNUSED_ARGUMENT(isfile);

    bail_null(sess_names);
    *sess_names = NULL;
    
    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);
    opts.tao_dup_policy = adp_delete_old;
    err = tstr_array_new(&names, &opts);
    bail_error(err);
    err = mdc_get_binding_children_tstr_array(cli_mcc,
	    NULL, NULL, &names_config,
	    "/nkn/nvsd/mfp/config", NULL);
    bail_error_null(err, names_config);

    /*
     * Check if the session-list is for file /live.
     * If  isfile is true then check if media-type is FILE
     * else  check if the names_config media-type is LIVE
     * before  putting it into the array for session-name completion.
     */
    err = tstr_array_concat(names, &names_config);
    bail_error(err);
    
    *sess_names = names;
    names = NULL;
bail:
    tstr_array_free(&names);
    tstr_array_free(&names_config);
    return err;
}

int
cli_session_name_completion(
	void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params,
	const tstring *curr_word,
	tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *names = NULL;
    int num_sessions = 0;
    int i;
    tstring *media_type = NULL;
    const char *session = NULL;
    uint32  code = 0;
    
    tstr_array *actual_list = NULL;
    tstr_array_options opts;
    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);
    opts.tao_dup_policy = adp_delete_old;
    err = tstr_array_new(&actual_list, &opts);
    bail_error(err);


    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_get_list_of_sessions(&names, true);
    bail_error(err);
    
    /* Get the number of parameters first */
    num_sessions = tstr_array_length_quick(names);
    if(num_sessions > 0){
	    for(i = 0; i < num_sessions; ++i) {
		session = tstr_array_get_str_quick(names, i);
		bail_null(session);
		err = mdc_get_binding_tstr_fmt
		    (cli_mcc, &code, NULL, NULL, &media_type, NULL,
		     "/nkn/nvsd/mfp/config/%s/media-type", session);
		bail_error(err);
		if (cmd->cc_magic == FILE_SESSIONS) {
		    if(ts_equal_str(media_type , "File", false)){
			tstr_array_append_str(actual_list, session);
		    }
		}else {
		    if(ts_equal_str(media_type , "Live", false)){
			tstr_array_append_str(actual_list, session);
		    }
		}

	    }

    }


    err = tstr_array_concat(ret_completions, &actual_list);
    bail_error(err);
    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);



bail:
    tstr_array_free(&names);
    return err;

}

int
cli_session_name_help(
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
    const char *namespace = NULL;
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
	    err = cli_session_name_completion(data, cmd, cmd_line, params,
		    curr_word, names);
	    bail_error(err);
	    num_names = tstr_array_length_quick(names);
	    for(i = 0; i < num_names; ++i) {
		namespace = tstr_array_get_str_quick(names, i);
		bail_null(namespace);
		err = cli_add_expansion_help(context, namespace, NULL);
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

int
cli_mfp_show_counters_internal_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	int num_params = 0;
	tstr_array *argv = NULL;
	char *name = NULL;
	const char *grep_pattern = NULL;
	tstring *curr_name = NULL;
	char *file_name = NULL;
	const char *process = NULL;
	const char *process_path = NULL;
	uint32 code = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(params);

	/* Get the number of parameters first */
	num_params = tstr_array_length_quick(cmd_line);

	err = tstr_array_new ( &argv , NULL);
	bail_error (err);
	err = tstr_array_append_str (argv, "NKNCNT_BINARY_PATH");
	bail_error (err);

	err = tstr_array_append_str (argv, "-P");
	bail_error (err);

	if(cmd->cc_magic == FILE_SESSIONS) {
	    err = tstr_array_append_str (argv, FILE_PUBLISHER);
	    bail_error (err);
	    process = "FILE_PUB";
	    process_path = FILE_PUBLISHER;
	}else {
	    err = tstr_array_append_str (argv, LIVE_PUBLISHER);
	    bail_error (err);
	    process = "LIVE_PUB";
	    process_path = LIVE_PUBLISHER;
	}

	/*Set the time duration and frequency as 0 to dump the counter value once*/
	err = tstr_array_append_str (argv, "-t");	
	bail_error(err);
	err = tstr_array_append_str (argv, "0");	
	bail_error(err);
	
	err = tstr_array_append_str (argv, "-l");	
	bail_error(err);
	err = tstr_array_append_str (argv, "0");	
	bail_error(err);

	if(num_params == 4) {  /*sh counters live-pub internal*/
	    /*Just the global level stats is needed*/
	    err = tstr_array_append_str (argv, "-s");
	    bail_error (err);
	    err = tstr_array_append_str (argv, "_");
	    bail_error (err);
	    err = nkn_clish_run_program_fg(NKNCNT_BINARY_PATH, argv, NULL, NULL, NULL);
	    bail_error(err);
	} else if (num_params == 5) { /*sh counters live-pub internal <pattern>*/
	    err = tstr_array_append_str (argv, "-s");
	    bail_error (err);
	    grep_pattern = tstr_array_get_str_quick(cmd_line, 4); //The last parameter is the pattern
	    bail_null(grep_pattern);
	    if(strlen(grep_pattern) == 0) {
		err = tstr_array_append_str (argv, "_");
		bail_error (err);
	    }else {
		err = tstr_array_append_str (argv, grep_pattern);
		bail_error (err);
	    }
	    err = nkn_clish_run_program_fg(NKNCNT_BINARY_PATH, argv, NULL, NULL, NULL);
	    bail_error(err);
	} else if (num_params == 6) {/*sh counter live-pub/file-pub internal pattern save */
	    grep_pattern = tstr_array_get_str_quick(cmd_line, 4); //The last parameter is the pattern
	    bail_null(grep_pattern);
	    if(strlen(grep_pattern) == 0) {
		err = tstr_array_append_str (argv, "_");
		bail_error (err);
	    }else {
		err = tstr_array_append_str (argv, grep_pattern);
		bail_error (err);
	    }
	    /*Get a filename to save*/
	    err = mfp_generate_file_name_for_counters(process, &file_name);
	    bail_error(err);
	   /*Calling action handler to do the job in the background*/
	    err = mdc_send_action_with_bindings_str_va(cli_mcc,
		    &code, NULL,
		    "/nkn/nvsd/mfp/actions/getcounter", 5,
		    "process" , bt_string, process_path,
		    "frequency", bt_string, "0",
		    "duration", bt_string, "0",
		    "filename", bt_string, file_name,
		    "pattern", bt_string, grep_pattern);
	    bail_error(err);
	    if(code) {
		err = cli_printf(_("Some error in parameter setting.Command execution failed\n"));
		goto bail;
	    }

	    err = cli_printf(_("Counter file generated:%s\n"), file_name);
	    bail_error(err);

	}
bail:
    safe_free(name);
    safe_free(file_name);
    tstr_array_free(&argv);
    ts_free(&curr_name);
    return err;

}
