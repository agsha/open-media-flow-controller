/*
 * Filename :   cli_nkn_accesslog_cmds.c
 * Author :     Dhruva
 * Created On : 01/05/2009
 *
 * (C) Copyright 2008-2009 Nokeena Networks Inc,
 * All rights reserved.
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
#include <strings.h>
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

cli_expansion_option cli_nkn_acclog_upload_opts[] = {
    {"current", N_("Current log file"), NULL},
    {"all", N_("All log files"), NULL},
    {NULL, NULL, NULL}
};

cli_expansion_option cli_nkn_fmslog_upload_opts[] = {
    {"all", N_("All log files"), NULL},
    {NULL, NULL, NULL}
};

#define	    NS_OBJECTS_DIR "/nkn/ns_objects"

/* Ignore reverse-mapping for these nodes */
const char *acclog_revmap_ignore[] = {
    "/nkn/accesslog/config/upload/*",
    "/nkn/accesslog/config/path",
    "/nkn/accesslog/config/serverip",
    "/nkn/accesslog/config/serverport",
    "/nkn/accesslog/config/remote/**",
    "/nkn/errorlog/config/**",
    "/nkn/mfdlog/config/**",
    "/nkn/accesslog/config/profile/*",
    "/nkn/accesslog/config/profile/*/format-class",
    "/nkn/accesslog/config/profile/*/upload/pass",
    "/nkn/accesslog/config/profile/*/upload/url",
    "/nkn/accesslog/config/enable",
    "/nkn/accesslog/config/format-display",
    "/nkn/accesslog/config/on-the-hour",
    "/nkn/accesslog/config/restriction/enable",
    "/nkn/accesslog/config/rotate",
    "/nkn/accesslog/config/syslog",
    "/nkn/accesslog/config/skip_object_size",
    "/nkn/accesslog/config/filename",
    "/nkn/accesslog/config/format",
    "/nkn/accesslog/config/max-fileid",
    "/nkn/accesslog/config/max-filesize",
    "/nkn/accesslog/config/skip_response_code",
    "/nkn/accesslog/config/time-interval",
    "/nkn/accesslog/config/analyzer/enable",
};

enum {
    cid_acclog_respcode_delete = 1,
    cid_acclog_respcode_add,
};


struct cli_upload_logs {
    const char *file_node;
    const char *dir_path;
    int mode_location;
    int rem_url_location;
};
int 
cli_nkn_accesslog_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


int 
cli_nkn_accesslog_format_help(
    void *data,
    cli_help_type help_type,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params, 
    const tstring *curr_word,
    void *context);

int 
cli_nkn_accesslog_config_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
cli_accesslog_file_upload_finish(const char *password, clish_password_result result,
    const cli_command *cmd,
    const tstr_array *cmd_line, const tstr_array *params,
    void *data, tbool *ret_continue);

int 
cli_nkn_accesslog_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int
cli_acclog_config_show_one_terse(const bn_binding_array *bindings,
			   uint32 idx,
			   const bn_binding *binding,
			   const tstring *name,
			   const tstr_array *name_components,
			   const tstring *name_last_part,
			   const tstring *value,
			   void *cb_data);

int
cli_acclog_show_config_one_verbose(void *data,
				   const cli_command *cmd,
				   const tstr_array *cmd_line,
				   const tstr_array *params);

int 
cli_nkn_accesslog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_accesslog_file_completion(void *data, const cli_command *cmd,
                    const tstr_array *cmd_line,
                    const tstr_array *params,
                    const tstring *curr_word,
                    tstr_array *ret_completions);
int
cli_accesslog_warning_message(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);

int
cli_accesslog_fileid_warning_message(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);


int
cli_nkn_log_upload_common(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nkn_cachelog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nkn_tracelog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nkn_streamlog_upload(
        void *data,
        const cli_command *cmd,
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
cli_nkn_fuselog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nkn_fmsaccesslog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nkn_fmsedgelog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nkn_publishlog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int
cli_nkn_accesslog_show_continuous(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params);


static int
cli_nkn_acclog_skip_response_code(void *data, const cli_command *cmd,
				  const tstr_array *cmd_line,
				  const tstr_array *params);

int cli_nkn_upload_logs( struct cli_upload_logs *log_details,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nkn_accesslog_cmds_init(
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
    cmd->cc_words_str =             "accesslog";
    cmd->cc_help_descr =            N_("Configure access log options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog *";
    cmd->cc_help_exp =		    N_("<log-template-name>");
    cmd->cc_help_exp_hint =	    N_("A profile name identifying a configuration template");
    cmd->cc_help_use_comp =	    true;
    cmd->cc_comp_type =		    cct_matching_names;
    cmd->cc_comp_pattern =	    "/nkn/accesslog/config/profile/*";
    cmd->cc_flags =                 ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation  =	    cxo_create;
    cmd->cc_exec_name =		    "/nkn/accesslog/config/profile/$1$";
    cmd->cc_exec_value =	    "$1$";
    cmd->cc_exec_type =		    bt_string;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no accesslog";
    cmd->cc_help_descr =            N_("Clear/Delete accesslog configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no accesslog *";
    cmd->cc_help_exp =		    N_("<log-template-name>");
    cmd->cc_help_exp_hint =	    N_("A template name identifying this log profile");
    cmd->cc_help_term =		    N_("Delete a log profile configuration");
    cmd->cc_help_use_comp =	    true;
    cmd->cc_comp_type =		    cct_matching_names;
    cmd->cc_comp_pattern =	    "/nkn/accesslog/config/profile/*";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation  =	    cxo_delete;
    cmd->cc_exec_name =		    "/nkn/accesslog/config/profile/$1$";
    cmd->cc_exec_value =	    "$1$";
    cmd->cc_exec_type =		    bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * no";
    cmd->cc_help_descr =            N_("Clear accesslog config options");
    cmd->cc_req_prefix_count =      2;
    CLI_CMD_REGISTER;


#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str = 	"accesslog remote";
    cmd->cc_help_descr = 	N_("Configure remote logger options");
    cmd->cc_flags = 		ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"accesslog remote *";
    cmd->cc_help_exp = 		N_("<ip[:port]>");
    cmd->cc_help_exp_hint = 	N_("Configure remote IP and optionally a port "
				"on which the logger receives log messages. (Default port: 7958)");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_help_term = 	N_("Configure remote logger address and use default "
		    		"management port as outbound interface");
    cmd->cc_exec_callback = 	cli_nkn_accesslog_remote_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"accesslog remote * on-interface";
    cmd->cc_help_descr = 	N_("Configure outbound interface to use for logging");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"accesslog remote * on-interface *";
    cmd->cc_help_exp =		N_("<interface>");
    cmd->cc_help_exp_hint = 	N_("outbound interface to use to connect to remote logger");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback = 	cli_nkn_accesslog_remote_config;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog enable";
    cmd->cc_help_descr =            N_("Enable access logging");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden ;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/enable";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no accesslog enable";
    cmd->cc_help_descr =            N_("Disable access log");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden ;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/enable";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog syslog";
    cmd->cc_help_descr =            N_("Configure syslog options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog syslog replicate";
    cmd->cc_help_descr =            N_("Configure syslog replication");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog syslog replicate enable";
    cmd->cc_help_descr =            N_("Enable syslog replication of access log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/syslog";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog syslog replicate disable";
    cmd->cc_help_descr =            N_("Disable syslog replication of access log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/syslog";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;

#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * file-name";
    cmd->cc_help_descr =            N_("Configure filename");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * file-name access.log";
    cmd->cc_help_descr =            N_("Set default filename - access.log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/profile/$1$/filename";
    cmd->cc_exec_value =            "access.log";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_none;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * file-name *";
    cmd->cc_help_exp =              N_("<filename>");
    cmd->cc_help_exp_hint =         N_("Set custom filename");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/profile/$1$/filename";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_order =          cro_accesslog;
    cmd->cc_revmap_names =           "/nkn/accesslog/config/profile/*/filename";
    cmd->cc_revmap_cmds =	    "accesslog $5$ file-name $v$";
    CLI_CMD_REGISTER;
#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog rotate";
    cmd->cc_help_descr =            N_("Configure rotation for log files");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog rotate enable";
    cmd->cc_help_descr =            N_("Turn ON log rotation for accesslog");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/rotate";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog rotate disable";
    cmd->cc_help_descr =            N_("Turn OFF log rotationfor accesslog");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/rotate";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;
#endif
    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * max-size";
    cmd->cc_help_descr =            N_("Configure maximum file size per log file");
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * filesize 100";
    cmd->cc_help_descr =            N_("Set default size per log file - 100 MiB");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/profile/$1$/max-filesize";
    cmd->cc_exec_value =            "100";
    cmd->cc_exec_type =             bt_uint16;
    cmd->cc_revmap_type =           crt_none;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * max-size *";
    cmd->cc_help_exp =              N_("<size-MiB>");
    cmd->cc_help_exp_hint =         N_("Set filesize (in MiB), default = 100");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/profile/$1$/max-filesize";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_type =             bt_uint16;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_order =          cro_accesslog;
    cmd->cc_revmap_names =           "/nkn/accesslog/config/profile/*/max-filesize";
    cmd->cc_revmap_cmds =	    "accesslog $5$ max-size $v$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * max-duration";
    cmd->cc_help_descr =            N_("Configure time-interval per log file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * max-duration *";
    cmd->cc_help_exp =              N_("<minutes>");
    cmd->cc_help_exp_hint =         N_("Set file rotation time interval (in Minutes), default = 15");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/profile/$1$/time-interval";
    cmd->cc_exec_value =	    "$2$";
    cmd->cc_exec_type =		    bt_uint32;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_order =          cro_accesslog;
    cmd->cc_revmap_names =           "/nkn/accesslog/config/profile/*/time-interval";
    cmd->cc_revmap_cmds =	    "accesslog $5$ max-duration $v$";
    CLI_CMD_REGISTER;




/*
    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog disksize-MB";
    cmd->cc_help_descr =            N_("Configure maximum disk space to store log files");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog disksize-MB 1000";
    cmd->cc_help_descr =            N_("Set default disk storage size for access logs - 1000 MBytes");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/total-diskspace";
    cmd->cc_exec_value =            "1000";
    cmd->cc_exec_type =             bt_uint16;
    cmd->cc_revmap_type =           crt_none;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog disksize-MB *";
    cmd->cc_help_exp =              N_("<size in MBytes>");
    cmd->cc_help_exp_hint =         N_("Set total diskspace to use for log files (in MBytes)");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/total-diskspace";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint16;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;
*/

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * format";
    cmd->cc_help_descr =            N_("Configure access log format string");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * format ncsa-combined";
    cmd->cc_help_descr =            N_("ncsa format - %h %V %u %t \"%r\" %s %b \"%{Referer}i\" \"%{User-Agent}i\"");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/profile/$1$/format";
    cmd->cc_exec_value =            "%h %V %u %t \"%r\" %s %b \"%{Referer}i\" \"%{User-Agent}i\""; 
    cmd->cc_exec_type =             bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * format w3c-ext-default";
    cmd->cc_help_descr =            N_("w3c-ext-default format - %h - - %t %r %s %b");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/profile/$1$/format";
    cmd->cc_exec_value =            "%h - - %t %r %s %b";
    cmd->cc_exec_type =             bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * format clf";
    cmd->cc_help_descr =            N_("clf format - %h %V %u %t \"%r\" %s %b");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/profile/$1$/format";
    cmd->cc_exec_value =            "%h %V %u %t \"%r\" %s %b"; 
    cmd->cc_exec_type =             bt_string;
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog format display ";
    cmd->cc_help_descr =            N_("Display of format in the log");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;                                                                 
    cmd->cc_words_str =             "accesslog format display enable";
    cmd->cc_help_descr =            N_("Enable log format display in log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/format-display";
    cmd->cc_exec_value =            "true"; 
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;                                                                 
    cmd->cc_words_str =             "accesslog format display disable";
    cmd->cc_help_descr =            N_("Enable log format display in log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/format-display";
    cmd->cc_exec_value =            "false"; 
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;

#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * format *";
    cmd->cc_help_exp =              N_("<format string>");
    cmd->cc_help_exp_hint =         "";
    cmd->cc_help_callback =         cli_nkn_accesslog_format_help;
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/profile/$1$/format";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;





    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * copy";
    cmd->cc_help_descr =            N_("Auto-copy the accesslog based on rotate criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog * copy *";
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param|ccf_ignore_length;
    cmd->cc_magic =                 cfi_accesslog;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =              N_("Remote URL (Secure-CP, Secure-FTP, FTP)");
    cmd->cc_help_exp_hint =         N_("[scp|sftp|ftp]://<username>:<password>@<hostname>/<path>");
    cmd->cc_exec_callback =         cli_nkn_accesslog_config_upload;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no accesslog * copy";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_help_descr =            N_("Disable auto upload of access logs");
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/accesslog/config/profile/$1$/upload/pass";
    cmd->cc_exec_name2 =            "/nkn/accesslog/config/profile/$1$/upload/url";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog analyzer";
    cmd->cc_help_descr =	    N_("Configure real-time accesslog analyzer");
    cmd->cc_flags =		    ccf_unavailable;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog analyzer enable";
    cmd->cc_help_descr =	    N_("Enable real-time accesslog analyzer");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/analyzer/enable";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "true";
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog analyzer disable";
    cmd->cc_help_descr =            N_("Disable real-time accesslog analyzer");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/analyzer/enable";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "false";
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;



    /* Command to view accesslog configuration */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show accesslog";
    cmd->cc_help_descr =            N_("View access log configuration");
    cmd->cc_flags =                 ccf_terminal; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_accesslog_show_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show accesslog *";
    cmd->cc_help_exp =		    N_("<log-template-name>");
    cmd->cc_help_exp_hint =	    N_("Profile name");
    cmd->cc_help_term =		    N_("Show configuration for profile");
    cmd->cc_help_use_comp =	    true;
    cmd->cc_comp_type =		    cct_matching_values;
    cmd->cc_comp_pattern =	    "/nkn/accesslog/config/profile/*";
    cmd->cc_flags =                 ccf_terminal; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_acclog_show_config_one_verbose;
    CLI_CMD_REGISTER;



    CLI_CMD_NEW;
    cmd->cc_words_str =             "show accesslog * continuous";
    cmd->cc_help_descr =            N_("View continuous access log ");
    cmd->cc_flags =                 ccf_terminal; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_accesslog_show_continuous;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show accesslog * last";
    cmd->cc_help_descr =            N_("View the last lines of access log ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show accesslog * last *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("Set the number of lines to be viewed");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_accesslog_show_continuous;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload";
    cmd->cc_help_descr =            N_("Upload a file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload accesslog";
    cmd->cc_help_descr =            N_("Upload an access log");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload accesslog *";
    cmd->cc_help_exp =		    N_("<log-template-name>");
    cmd->cc_help_exp_hint =	    N_("Profile name");
    cmd->cc_comp_type =		    cct_matching_values;
    cmd->cc_comp_pattern =	    "/nkn/accesslog/config/profile/*";
    cmd->cc_help_use_comp =	    true;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload accesslog * *";
    cmd->cc_help_options =          cli_nkn_acclog_upload_opts;
    cmd->cc_help_num_options =      sizeof(cli_nkn_acclog_upload_opts)/sizeof(cli_expansion_option);
    cmd->cc_magic =                 cfi_accesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload accesslog * * *";
    cmd->cc_help_exp =              N_("Remote URL");
    cmd->cc_help_exp_hint =         N_("<scp://username:password@hostname/path> or <sftp://user@host:path>");
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_magic =                 cfi_accesslog;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_accesslog_upload;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload errorlog";
    cmd->cc_help_descr =            N_("Upload an error log");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload errorlog *";
    cmd->cc_help_options =          cli_nkn_acclog_upload_opts;
    cmd->cc_help_num_options =      sizeof(cli_nkn_acclog_upload_opts)/sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload errorlog * *";
    cmd->cc_help_exp =              cli_msg_url;
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_magic =                 cfi_accesslog;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_log_upload_common;
    cmd->cc_exec_data =		    (void *)"/nkn/errorlog/config/filename";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload cachelog";
    cmd->cc_help_descr =            N_("Upload a cache log file.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload cachelog *";
    cmd->cc_help_options =          cli_nkn_acclog_upload_opts;
    cmd->cc_help_num_options =      sizeof(cli_nkn_acclog_upload_opts)/sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload cachelog * *";
    cmd->cc_help_exp =              cli_msg_url;
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cmd->cc_magic =                 cfi_accesslog;
    cmd->cc_exec_callback =	    cli_nkn_log_upload_common;
    cmd->cc_exec_data =		    (void *)"/nkn/cachelog/config/filename";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload tracelog";
    cmd->cc_help_descr =            N_("Upload a trace log file.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload tracelog *";
    cmd->cc_help_options =          cli_nkn_acclog_upload_opts;
    cmd->cc_help_num_options =      sizeof(cli_nkn_acclog_upload_opts)/sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload tracelog * *";
    cmd->cc_help_exp =              cli_msg_url;
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cmd->cc_magic =                 cfi_accesslog;
    cmd->cc_exec_callback =	    cli_nkn_log_upload_common;
    cmd->cc_exec_data =		    (void *) "/nkn/tracelog/config/filename";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload streamlog ";
    cmd->cc_help_descr =            N_("Upload a stream log file.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload streamlog *";
    cmd->cc_help_options =          cli_nkn_acclog_upload_opts;
    cmd->cc_help_num_options =      sizeof(cli_nkn_acclog_upload_opts)/sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload streamlog * *";
    cmd->cc_help_exp =              cli_msg_url;
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cmd->cc_magic =                 cfi_accesslog;
    cmd->cc_exec_callback =	    cli_nkn_log_upload_common;
    cmd->cc_exec_data =		    (void *)"/nkn/streamlog/config/filename";
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload object ";
    cmd->cc_help_descr =            N_("Upload an object list");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload object list";
    cmd->cc_help_descr =            N_("Upload an object list");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload object list *";
    cmd->cc_help_exp =              "<namespace>";
    cmd->cc_help_exp_hint =         N_("namespace whose object list you want to upload");
    cmd->cc_comp_type =	            cct_matching_values;
    cmd->cc_comp_pattern =          "/nkn/nvsd/namespace/*";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload object list * *";
    cmd->cc_help_exp =              cli_msg_url;
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_magic =                 cfi_objectlist;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_object_list_upload;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog on-the-hour ";
    cmd->cc_help_descr =            N_("Set on the hour log rotation option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog on-the-hour enable";
    cmd->cc_help_descr =            N_("Enable On the hour log rotation");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/on-the-hour";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "accesslog on-the-hour disable";
    cmd->cc_help_descr =            N_("Disable On the hour log rotation");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/accesslog/config/on-the-hour";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_accesslog;
    CLI_CMD_REGISTER;
  
#endif
    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload fuselog ";
    cmd->cc_flags =                 ccf_unavailable;
    cmd->cc_help_descr =            N_("Upload a fuse log file.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload fuselog *";
    cmd->cc_help_options =          cli_nkn_acclog_upload_opts;
    cmd->cc_help_num_options =      sizeof(cli_nkn_acclog_upload_opts)/sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload fuselog * *";
    cmd->cc_help_exp =              cli_msg_url;
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cmd->cc_magic =                 cfi_accesslog;
    cmd->cc_exec_callback =         cli_nkn_fuselog_upload;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload fmsaccesslog ";
    cmd->cc_flags =                 ccf_unavailable;
    cmd->cc_help_descr =            N_("Upload a FMS Access log file.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload fmsaccesslog *";
    cmd->cc_help_options =          cli_nkn_fmslog_upload_opts;
    cmd->cc_help_num_options =      sizeof(cli_nkn_fmslog_upload_opts)/sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload fmsaccesslog * *";
    cmd->cc_help_exp =              cli_msg_url;
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cmd->cc_magic =                 cfi_fmsaccesslog;
    cmd->cc_exec_callback =         cli_nkn_fmsaccesslog_upload;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload fmsedgelog ";
    cmd->cc_flags =                 ccf_unavailable;
    cmd->cc_help_descr =            N_("Upload a FMS Edge log file.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload fmsedgelog *";
    cmd->cc_help_options =          cli_nkn_fmslog_upload_opts;
    cmd->cc_help_num_options =      sizeof(cli_nkn_fmslog_upload_opts)/sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload fmsedgelog * *";
    cmd->cc_help_exp =              cli_msg_url;
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cmd->cc_magic =                 cfi_fmsedgelog;
    cmd->cc_exec_callback =         cli_nkn_fmsedgelog_upload;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload publishlog ";
    cmd->cc_help_descr =            N_("Upload a MFP Publish log file.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload publishlog *";
    cmd->cc_help_options =          cli_nkn_acclog_upload_opts;
    cmd->cc_help_num_options =      sizeof(cli_nkn_acclog_upload_opts)/sizeof(cli_expansion_option);
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "upload publishlog * *";
    cmd->cc_help_exp =              cli_msg_url;
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cmd->cc_magic =                 cfi_publishlog;
    cmd->cc_exec_callback =	    cli_nkn_log_upload_common;
    cmd->cc_exec_data =		    (void *) "/nkn/mfplog/config/filename";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	    "accesslog * object";
    cmd->cc_help_descr =	    N_("Configure log record specific settings");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	    "no accesslog * object";
    cmd->cc_help_descr =	    N_("Clear/reset log record specific settings");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str = 	    "accesslog * object filter";
    cmd->cc_help_descr =	    N_("Configure log filter settings for each record");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	    "no accesslog * object filter";
    cmd->cc_help_descr =	    N_("Clear/reset log filter settings for each record");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	    "accesslog * object filter size";
    cmd->cc_help_descr =	    N_("Configure minimum object size for which a log record is written to file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	    "no accesslog * object filter size";
    cmd->cc_help_descr =	    N_("Reset minimum object size for which a log record is written to file to default value (0)");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_reset;
    cmd->cc_exec_name =		    "/nkn/accesslog/config/profile/$1$/object/filter/size";
    cmd->cc_exec_value =	    "0";
    cmd->cc_exec_type =		    bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	    "accesslog * object filter size *";
    cmd->cc_help_exp =		    N_("<bytes>");
    cmd->cc_help_exp_hint =	    N_("Size of the object");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/accesslog/config/profile/$1$/object/filter/size";
    cmd->cc_exec_value =	    "$2$";
    cmd->cc_exec_type =		    bt_uint32;
    cmd->cc_revmap_type =	    crt_manual;
    cmd->cc_revmap_order =	    cro_accesslog;
    cmd->cc_revmap_names =	    "/nkn/accesslog/config/profile/*/object/filter/size";
    cmd->cc_revmap_cmds =	    "accesslog $5$ object filter size $v$";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str = 	    "accesslog * object filter response-code";
    cmd->cc_help_descr =	    N_("Configure object response code for which a log record is written to file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	    "no accesslog * object filter response-code";
    cmd->cc_help_descr =	    N_("Clear object response-code for which a log record is written to file to default value");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	    "accesslog * object filter response-code *";
    cmd->cc_help_exp =		    N_("<code>");
    cmd->cc_help_exp_hint =	    N_("HTTP response code");
    cmd->cc_flags =		    ccf_terminal | ccf_allow_extra_params;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_callback =	    cli_nkn_acclog_skip_response_code;
    cmd->cc_magic =		    cid_acclog_respcode_add;
    cmd->cc_revmap_type =	    crt_manual;
    cmd->cc_revmap_order =	    cro_accesslog;
    cmd->cc_revmap_names =	    "/nkn/accesslog/config/profile/*/object/filter/resp-code/*";
    cmd->cc_revmap_cmds =	    "accesslog $5$ object filter response-code $v$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	    "no accesslog * object filter response-code *";
    cmd->cc_help_exp =		    N_("<code>");
    cmd->cc_help_exp_hint =	    N_("HTTP response code");
    cmd->cc_help_use_comp =	    true;
    cmd->cc_comp_type =		    cct_matching_names;
    cmd->cc_comp_pattern =	    "/nkn/accesslog/config/profile/$1$/object/filter/resp-code/*";
    cmd->cc_flags =		    ccf_terminal | ccf_allow_extra_params;
    cmd->cc_exec_operation =	    cxo_reset;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_callback =	    cli_nkn_acclog_skip_response_code;
    cmd->cc_magic =		    cid_acclog_respcode_delete;
    CLI_CMD_REGISTER;



    //err = cli_revmap_hide_bindings("/pm/process/nknaccesslog/auto_launch");
    //bail_error(err);

    err = cli_revmap_ignore_bindings_arr(acclog_revmap_ignore);
    bail_error(err);

bail:
    return err;
}



int 
cli_nkn_accesslog_format_help(
    void *data,
    cli_help_type help_type,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params, 
    const tstring *curr_word,
    void *context)
{
    int err = 0;
    uint32 i = 0;
    struct fmt_help_s {
	const char    *c;
	const char    *help;
    } fmt_help[] = {
	{"b",   "bytes_out_no_header"},
	{"c",   "cache hit indicator"},
	{"d",   "origin fetched size"},
	{"e",   "cache hit history"},
	{"f",   "filename"},
	{"g",   "compress_status"},
	{"h",   "remote_host"},
	{"{xxx}i",   "request_header"},
	{"m",   "request_method"},
	{"{xxx}o",   "response_header"},
	{"p",   "object hotness"},
	{"q",   "query_string"},
	{"r",   "request_line"},
	{"s",   "status"},
	{"t",   "timestamp"},
	{"u",   "remote_user"},
	{"v",   "server_name"},
	{"w",   "expired object hit indicator"},
	{"y",   "status_subcode"},
	{"z",	"connection type"},

	{"A",   "request_in_time"},
	{"B",   "first_byte_out_time"},
	{"{xxx}C",   "cookie"},
	{"D",   "time_used_ms"},
	{"E",   "time_used_sec"},
	{"F",   "last_byte_out_time"},
	{"H",   "request_protocol"},
	{"I",   "bytes_in"},
	{"L",   "latency_to_first_byte_out"},
	{"M",   "data_out_ms"},
	{"N",   "Namespace name"},
	{"O",   "bytes_out"},
	{"R",   "cache_revalidate"},
	{"S",   "Origin Server Name"},
	{"U",   "url"},
	{"V",   "http_host"},
	{"X",   "remote_addr"},
	{"Y",   "destintion ip address"},
	{"Z",   "server_port"},

    };
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(params);
    UNUSED_ARGUMENT(help_type);
    UNUSED_ARGUMENT(curr_word);
    UNUSED_ARGUMENT(context);

    for ( i = 0; i < sizeof(fmt_help)/sizeof(struct fmt_help_s); i++) {
	if ( fmt_help[i].c == 0 ) {
	    cli_printf("\n");
	    continue;
	}

	err = cli_printf("%%%s    %-20s\n", fmt_help[i].c, fmt_help[i].help);
	bail_error(err);

    }
bail:
    return err;
}


static int
cli_accesslog_file_upload_finish(const char *password, clish_password_result result,
    const cli_command *cmd,
    const tstr_array *cmd_line, const tstr_array *params,
    void *data, tbool *ret_continue)
{
    int err = 0;
    const char *remote_url = NULL;
    const char *profile = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(ret_continue);
    if (result != cpr_ok) {
        goto bail;
    }

    /* accesslog <profile> copy <scp:....> */

    profile = tstr_array_get_str_quick(params, 0);
    bail_null(profile);

    remote_url = tstr_array_get_str_quick(params, 1);
    bail_null(remote_url);

    if ((!lc_is_prefix("scp://", remote_url, false)) &&
            (!lc_is_prefix("sftp://", remote_url, false)) && 
		(!lc_is_prefix("ftp://", remote_url, false))){
        cli_printf_error(_("Bad destination specifier only supports scp or sftp or ftp"));
        goto bail;
    }

    if (password) {
        err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/nkn/accesslog/action/upload", 3,
		"profile", bt_string, profile,
                "remote_url", bt_string, remote_url,
                "password", bt_string, password);
        bail_error(err);
    }
    else {
        err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/nkn/accesslog/action/upload", 2,
		"profile", bt_string, profile,
                "remote_url", bt_string, remote_url);
        bail_error(err);
    }
bail:
    return err;
}

int 
cli_nkn_accesslog_config_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;

    const char *remote_url = NULL;

    UNUSED_ARGUMENT(data);
    remote_url = tstr_array_get_str_quick(params, 1);
    bail_null(remote_url);
    
    if (((lc_is_prefix("scp://", remote_url, false)) ||
        (lc_is_prefix("sftp://", remote_url, false))) && (clish_is_shell())){
        err = clish_maybe_prompt_for_password_url
                (remote_url, cli_accesslog_file_upload_finish, cmd, cmd_line, params, NULL);
            bail_error(err);
         }
    else {  
      	err = cli_accesslog_file_upload_finish(NULL, cpr_ok, cmd, cmd_line, params,
               	    NULL, NULL);
        bail_error(err);
       }
bail:
    return err;
}

int
cli_nkn_accesslog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *profile = NULL;
    node_name_t fnode = {0}, fname = {0};
    struct cli_upload_logs cli_log_details;

    UNUSED_ARGUMENT(data);

    profile = tstr_array_get_str_quick(params, 0);
    bail_null(profile);

    snprintf(fnode, sizeof(fnode),
	    "/nkn/accesslog/config/profile/%s/filename", profile);
    snprintf(fname, sizeof(fname), "/var/log/nkn/accesslog/%s", profile);

    bzero(&cli_log_details, sizeof (struct cli_upload_logs));

    cli_log_details.file_node = fnode;
    cli_log_details.dir_path = fname;
    cli_log_details.mode_location = 1;
    cli_log_details.rem_url_location = 2;

    /* call common processing function */
    err = cli_nkn_upload_logs(&cli_log_details, cmd, cmd_line, params);
    bail_error(err);


bail:
    return err;
}

/* ------------------------------------------------------------------------- */
int
cli_nkn_upload_logs( struct cli_upload_logs *log_details,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;
    const char *mode = NULL;
    tstr_array *names = NULL;
    tstring *filename = NULL;

    mode = tstr_array_get_str_quick(params, log_details->mode_location);
    bail_null(mode);

    if ((strcmp(mode, "current") != 0) &&
	    (strcmp(mode, "all") != 0) ) {
	cli_printf_error(_("Unrecognized command \"%s\"."), mode);
	goto bail;
    }

    remote_url = tstr_array_get_str_quick(params, log_details->rem_url_location);
    bail_null(remote_url);

    if (strcmp(mode, "current") == 0) {
	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
		&filename, log_details->file_node, NULL);
	bail_error(err);

	err = tstr_array_new(&names, NULL);
	bail_error_null(err, names);

	err = tstr_array_append_str(names, ts_str(filename));
	bail_error(err);

    } else {
	err = cli_file_get_filenames(log_details->dir_path, &names);
	bail_error_null(err, names);

	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
		&filename, log_details->file_node, NULL);
	bail_error(err);

	err = tstr_array_delete_non_prefixed(names, ts_str(filename));
	bail_error(err);
    }

    if (clish_is_shell()) {
	err = clish_maybe_prompt_for_password_url(remote_url,
		cli_grab_password, cmd, cmd_line, params, (void *)names);
	bail_error(err);
    } else {
	err = cli_grab_password(NULL, cpr_ok, cmd, cmd_line, params,
		(void *) names, NULL);
	bail_error(err);
    }

bail:
    /* not free array, callback function will take care of it */
    return err;
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
    const char *dir = NULL;
    cli_file_id fid = cmd->cc_magic;
    const char *filename = NULL;
    const char *profile = NULL;
    char *profile_dir = NULL;
    const char *type = NULL;

    UNUSED_ARGUMENT(ret_continue);
    if ( result != cpr_ok ) {
        goto bail;
    }

    /* BZ 10141: Upload * fails because this expects a profile name.
     * This is valid only for accesslog
     */
    type = tstr_array_get_str_quick(cmd_line, 1);
    bail_null(type);

    dir = lc_enum_to_string_quiet_null(cli_file_id_path_map, fid);
    bail_null(dir);


    if (lc_is_prefix("a", type, true) == true) {
	profile = tstr_array_get_str_quick(params, 0);
	bail_null(profile);

	profile_dir = smprintf("%s/accesslog/%s", dir, profile);

	remote_url = tstr_array_get_str_quick(params, 2);
	bail_null(remote_url);
    } else {
	profile_dir = smprintf("%s", dir);

	remote_url = tstr_array_get_str_quick(params, 1);
	bail_null(remote_url);
    }
    bail_null(profile_dir);

    nfiles = tstr_array_length_quick(names);
    lc_log_basic(LOG_NOTICE, _("Nfiles:%u, dir:%s\n"),nfiles,dir);


    for(idx = 0; idx < nfiles; idx++) {
        filename = tstr_array_get_str_quick(names, idx);
        bail_null(filename);
        cli_printf(_("Uploading ... %s\n"), filename);
        if (password) {
            err = mdc_send_action_with_bindings_str_va
                    (cli_mcc, NULL, NULL, "/file/transfer/upload", 4,
                    "local_dir", bt_string, profile_dir,
                    "local_filename", bt_string, filename,
                    "remote_url", bt_string, remote_url,
                    "password", bt_string, password);
            bail_error(err);
        }
        else {
            err = mdc_send_action_with_bindings_str_va
                (cli_mcc, NULL, NULL, "/file/transfer/upload", 3,
                "local_dir", bt_string, profile_dir,
                "local_filename", bt_string, filename,
                "remote_url", bt_string, remote_url);
            bail_error(err);
        }
    }

bail:
    safe_free(profile_dir);
    tstr_array_free(&names);
    return err;
}


/* ------------------------------------------------------------------------- */
int
cli_accesslog_file_completion(void *data, const cli_command *cmd,
                    const tstr_array *cmd_line,
                    const tstr_array *params,
                    const tstring *curr_word,
                    tstr_array *ret_completions)
{
    int err = 0;
    cli_file_id fid = cmd->cc_magic;
    const char *dir = NULL, *name = NULL;
    uint32 num_names = 0, i = 0;
    tstr_array *names = NULL;
    const char *remove_files = (const char *) data;

    UNUSED_ARGUMENT(params);
    UNUSED_ARGUMENT(cmd_line);

    dir = lc_enum_to_string_quiet_null(cli_file_id_path_map, fid);
    bail_null(dir);

    err = cli_file_get_filenames(dir, &names);
    bail_error_null(err, names);
    /*
     * Remove names that do not match a prefixed pattern
     */
    if (remove_files) {
        err = tstr_array_delete_non_prefixed(names, remove_files);
        bail_error(err);
    }

    num_names = tstr_array_length_quick(names);
    for (i = 0; i < num_names; ++i) {
        name = tstr_array_get_str_quick(names, i);
        bail_null(name);
        if (lc_is_prefix(ts_str(curr_word), name, false)) {
            err = tstr_array_append_str(ret_completions, name);
            bail_error(err);
        }
    }


 bail:
    tstr_array_free(&names);
    return(err);
}



int 
cli_nkn_accesslog_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    uint32 num_matches = 0;

      	
    /* Print a string which is parameterized with names of management
     * bindings to be queried and substituted.  The '#' sign  delimits
     * names of bindings; anything surrounded by a '#' on each side is
     * treated as the name of a binding which should be queried, and its
     * value substituted into the string.  The printf-style format string
     * and the variable-length list of arguments are rendered into a
     * single string first, then the substitution is performed.
     *
     * For example, you could display the interface  duplex like this:
     * cli_printf_queried("The duplex is #/interfaces/%s/duplex#\n", itf);
     */
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(params);
    err = cli_printf_query
                    (_("Media Flow Controller Access Log enabled : "
                       "#/nkn/accesslog/config/enable#\n"));
    bail_error(err);

    err = mdc_get_binding_children(cli_mcc, NULL, NULL, true,
	    &bindings, true, true, "/nkn/accesslog/config/profile/*");
    bail_error(err);

    err = mdc_foreach_binding_prequeried(bindings, "/nkn/accesslog/config/profile/*",
	    NULL, cli_acclog_config_show_one_terse, NULL, &num_matches);
    bail_error(err);

bail:
    bn_binding_array_free(&bindings);
    return err;
}

/* ------------------------------------------------------------------------- */
int
cli_nkn_log_upload_common(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *fname_node = data;
    struct cli_upload_logs cli_log_details;

    bail_require(fname_node);

    bzero(&cli_log_details, sizeof (struct cli_upload_logs));

    cli_log_details.file_node = fname_node;
    cli_log_details.dir_path = "/var/log/nkn";
    cli_log_details.mode_location = 0;
    cli_log_details.rem_url_location = 1;

    /* call common processing function */
    err = cli_nkn_upload_logs(&cli_log_details, cmd, cmd_line, params);
    bail_error(err);

bail:
    return err;
}

/* ------------------------------------------------------------------------- */

int 
cli_nkn_cachelog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;
    const char *mode = NULL;
    char *buf = NULL;
    uint32 buflen = 0;
    uint32 fileid = 0;
    tstr_array *files = NULL;
    tstr_array *names = NULL;
    uint32 len = 0;
    char *password = NULL;
    tstring *al_filename = NULL;
    int el_id = 0, cache_id = 0, tl_id = 0;

    mode = tstr_array_get_str_quick(params, 0);
    tstr_array_free(&files);
    bail_null(mode);
    
    if ( (strcmp(mode, "current") != 0) &&
         (strcmp(mode, "all") != 0) ) {
        cli_printf_error(_("Unrecognized command \"%s\"."), mode);
        goto bail;
    }

    remote_url = tstr_array_get_str_quick(params, 1);
    bail_null(remote_url);

    if ( strcmp(mode, "current") == 0) {

        err = lf_read_file("/var/log/nkn/.nknlogd_log.fileid", &buf, &buflen);
        bail_error(err);

        if (buflen) {
            sscanf(buf, "fileid=%d %d %d %d", &fileid, &el_id, &cache_id, &tl_id);
            err = tstr_array_new(&files, NULL);
            bail_error(err);

            err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/cachelog/config/filename", NULL); 
            bail_error(err);

/*
            err = tstr_array_append_sprintf(files, "%s.%d", 
                    ts_str(al_filename), cache_id);
            bail_error(err);
*/
            err = tstr_array_append_sprintf(files, "%s", 
                    ts_str(al_filename) );
            bail_error(err);
            err = tstr_array_append_sprintf(files, "%s", remote_url);
            bail_error(err);

            err = cli_file_upload(data, cmd, cmd_line, files);
            bail_error(err);
        }
    }
    else {
        tbool prompt_pass = false;
        err = cli_file_get_filenames("/var/log/nkn", &names);
        bail_error_null(err, names);

        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/cachelog/config/filename", NULL); 
        bail_error(err);

        err = tstr_array_delete_non_prefixed(names, ts_str(al_filename));
        bail_error(err);

        len = tstr_array_length_quick(names);
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
    safe_free(buf);
    tstr_array_free(&files);
    /* Fix 1366 - names is freed in cli_grab_password. 
     * We dont want to free it here or we'll end up with a bogus
     * pointer in cli_grab_password
     */
    //tstr_array_free(&names);
    return err;
}



int 
cli_nkn_tracelog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;
    const char *mode = NULL;
    char *buf = NULL;
    uint32 buflen = 0;
    uint32 fileid = 0;
    tstr_array *files = NULL;
    tstr_array *names = NULL;
    uint32 len = 0;
    char *password = NULL;
    tstring *al_filename = NULL;
    int el_id = 0, cache_id = 0, tl_id = 0;

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

        err = lf_read_file("/var/log/nkn/.nknlogd_log.fileid", &buf, &buflen);
        bail_error(err);

        if (buflen) {
            sscanf(buf, "fileid=%d %d %d %d", &fileid, &el_id, &cache_id, &tl_id);
            err = tstr_array_new(&files, NULL);
            bail_error(err);

            err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/tracelog/config/filename", NULL); 
            bail_error(err);
/*

            err = tstr_array_append_sprintf(files, "%s.%d", 
                    ts_str(al_filename), tl_id);
            bail_error(err);
*/

            err = tstr_array_append_sprintf(files, "%s", 
                    ts_str(al_filename));
            bail_error(err);

            err = tstr_array_append_sprintf(files, "%s", remote_url);
            bail_error(err);

            err = cli_file_upload(data, cmd, cmd_line, files);
            bail_error(err);
        }
    }
    else {
        tbool prompt_pass = false;
        err = cli_file_get_filenames("/var/log/nkn", &names);
        bail_error_null(err, names);

        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/tracelog/config/filename", NULL); 
        bail_error(err);

        err = tstr_array_delete_non_prefixed(names, ts_str(al_filename));
        bail_error(err);

        len = tstr_array_length_quick(names);
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
    safe_free(buf);
    tstr_array_free(&files);
    /* Fix 1366 - names is freed in cli_grab_password. 
     * We dont want to free it here or we'll end up with a bogus
     * pointer in cli_grab_password
     */
    //tstr_array_free(&names);
    return err;
}


int 
cli_nkn_streamlog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;
    const char *mode = NULL;
    char *buf = NULL;
    uint32 buflen = 0;
    uint32 fileid = 0;
    tstr_array *files = NULL;
    tstr_array *names = NULL;
    uint32 len = 0;
    char *password = NULL;
    tstring *al_filename = NULL;
    int el_id = 0, cache_id = 0, tl_id = 0, sl_id = 0;

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

        err = lf_read_file("/var/log/nkn/.nknlogd_log.fileid", &buf, &buflen);
        bail_error(err);

        if (buflen) {
            sscanf(buf, "fileid=%d %d %d %d %d", &fileid, &el_id, &cache_id, &tl_id, &sl_id);
            err = tstr_array_new(&files, NULL);
            bail_error(err);

            err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/streamlog/config/filename", NULL); 
            bail_error(err);


            err = tstr_array_append_sprintf(files, "%s.%d", 
                    ts_str(al_filename), sl_id);
            bail_error(err);

            err = tstr_array_append_sprintf(files, "%s", remote_url);
            bail_error(err);

            err = cli_file_upload(data, cmd, cmd_line, files);
            bail_error(err);
        }
    }
    else {
        tbool prompt_pass = false;
        err = cli_file_get_filenames("/var/log/nkn", &names);
        bail_error_null(err, names);

        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/streamlog/config/filename", NULL); 
        bail_error(err);

        err = tstr_array_delete_non_prefixed(names, ts_str(al_filename));
        bail_error(err);

        len = tstr_array_length_quick(names);
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
    safe_free(buf);
    tstr_array_free(&files);
    return err;
}

int 
cli_nkn_fuselog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;
    const char *mode = NULL;
    char *buf = NULL;
    uint32 buflen = 0;
    uint32 fileid = 0;
    tstr_array *files = NULL;
    tstr_array *names = NULL;
    uint32 len = 0;
    char *password = NULL;
    tstring *al_filename = NULL;
    int el_id = 0, cache_id = 0, tl_id = 0;

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

        err = lf_read_file("/var/log/nkn/.nknlogd_log.fileid", &buf, &buflen);
        bail_error(err);

        if (buflen) {
            sscanf(buf, "fileid=%d %d %d %d", &fileid, &el_id, &cache_id, &tl_id);
            err = tstr_array_new(&files, NULL);
            bail_error(err);

            err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/fuselog/config/filename", NULL); 
            bail_error(err);


            err = tstr_array_append_sprintf(files, "%s.%d", 
                    ts_str(al_filename), tl_id);
            bail_error(err);

            err = tstr_array_append_sprintf(files, "%s", remote_url);
            bail_error(err);

            err = cli_file_upload(data, cmd, cmd_line, files);
            bail_error(err);
        }
    }
    else {
        tbool prompt_pass = false;
        err = cli_file_get_filenames("/var/log/nkn", &names);
        bail_error_null(err, names);

        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/fuselog/config/filename", NULL); 
        bail_error(err);

        err = tstr_array_delete_non_prefixed(names, ts_str(al_filename));
        bail_error(err);

        len = tstr_array_length_quick(names);
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
    safe_free(buf);
    tstr_array_free(&files);
    /* Fix 1366 - names is freed in cli_grab_password. 
     * We dont want to free it here or we'll end up with a bogus
     * pointer in cli_grab_password
     */
    //tstr_array_free(&names);
    return err;
}

int 
cli_nkn_fmsaccesslog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;
    const char *mode = NULL;
    char *buf = NULL;
    uint32 buflen = 0;
    uint32 fileid = 0;
    tstr_array *files = NULL;
    tstr_array *names = NULL;
    uint32 len = 0;
    char *password = NULL;
    tstring *al_filename = NULL;
    int el_id = 0, cache_id = 0, tl_id = 0;

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
	cli_printf( _("upload fmsaccesslog current is not supported at present\n"));
        err = lf_read_file("/var/log/nkn/.nknlogd_log.fileid", &buf, &buflen);
        bail_error(err);

        if (buflen) {
            sscanf(buf, "fileid=%d %d %d %d", &fileid, &el_id, &cache_id, &tl_id);
            err = tstr_array_new(&files, NULL);
            bail_error(err);

            err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/fmsaccesslog/config/filename", NULL); 
            bail_error(err);


            err = tstr_array_append_sprintf(files, "%s.%d", 
                    ts_str(al_filename), tl_id);
            bail_error(err);

            err = tstr_array_append_sprintf(files, "%s", remote_url);
            bail_error(err);

            err = cli_file_upload(data, cmd, cmd_line, files);
            bail_error(err);
        }
    }
    else {
        tbool prompt_pass = false;
        err = cli_file_get_filenames("/nkn/adobe/fms/logs", &names);
        bail_error_null(err, names);

        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/fmsaccesslog/config/filename", NULL); 
        bail_error(err);

        err = tstr_array_delete_non_prefixed(names, "access" );
        bail_error(err);

        err = tstr_array_delete_str(names, "access.rot" );
        bail_error(err);

        len = tstr_array_length_quick(names);
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
    safe_free(buf);
    tstr_array_free(&files);
    /* Fix 1366 - names is freed in cli_grab_password. 
     * We dont want to free it here or we'll end up with a bogus
     * pointer in cli_grab_password
     */
    //tstr_array_free(&names);
    return err;
}

int 
cli_nkn_fmsedgelog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;
    const char *mode = NULL;
    char *buf = NULL;
    uint32 buflen = 0;
    uint32 fileid = 0;
    tstr_array *files = NULL;
    tstr_array *names = NULL;
    uint32 len = 0;
    char *password = NULL;
    tstring *al_filename = NULL;
    int el_id = 0, cache_id = 0, tl_id = 0;

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

        err = lf_read_file("/var/log/nkn/.nknlogd_log.fileid", &buf, &buflen);
        bail_error(err);

        if (buflen) {
            sscanf(buf, "fileid=%d %d %d %d", &fileid, &el_id, &cache_id, &tl_id);
            err = tstr_array_new(&files, NULL);
            bail_error(err);

            err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/fmsedgelog/config/filename", NULL); 
            bail_error(err);


            err = tstr_array_append_sprintf(files, "%s.%d", 
                    ts_str(al_filename), tl_id);
            bail_error(err);

            err = tstr_array_append_sprintf(files, "%s", remote_url);
            bail_error(err);

            err = cli_file_upload(data, cmd, cmd_line, files);
            bail_error(err);
        }
    }
    else {
        tbool prompt_pass = false;
        err = cli_file_get_filenames("/nkn/adobe/fms/logs", &names);
        bail_error_null(err, names);

        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/fmsedgelog/config/filename", NULL); 
        bail_error(err);

        err = tstr_array_delete_non_prefixed(names, "edge");
        bail_error(err);

        err = tstr_array_delete_str(names, "edge.rot" );
        bail_error(err);

        len = tstr_array_length_quick(names);
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
    safe_free(buf);
    tstr_array_free(&files);
    /* Fix 1366 - names is freed in cli_grab_password. 
     * We dont want to free it here or we'll end up with a bogus
     * pointer in cli_grab_password
     */
    //tstr_array_free(&names);
    return err;
}

int
cli_nkn_accesslog_show_continuous(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params)
{
	int err = 0;
	tstr_array *argv = NULL;
	const char *pattern = NULL;
	tstring *al_filename = NULL;
	const char *logfile = NULL;
	const char *profile = NULL;
	char *fname = NULL;

	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(data);
	profile = tstr_array_get_str_quick(params, 0);
	bail_null(profile);

	pattern = tstr_array_get_str_quick(params, 1);

	fname = smprintf("/nkn/accesslog/config/profile/%s/filename", profile);
	bail_null(fname);

	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
        	&al_filename, fname, NULL); 
        bail_error(err);

        if(!al_filename) {
		goto bail;
	}

	logfile = smprintf("/var/log/nkn/accesslog/%s/%s", profile, ts_str(al_filename));

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
	safe_free(fname);
	return(err);
}

int
cli_accesslog_warning_message(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
	int err = 0;
	char *bn_name = NULL;
	const char *time_interval = NULL;
	uint32 ret = 0;
	tstring *ret_msg = NULL;
	const char *profile = NULL;
	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	profile = tstr_array_get_str_quick(params, 0);
	bail_null(profile);

	time_interval = tstr_array_get_str_quick(params, 1);
	bail_null(time_interval);

	bn_name = smprintf("/nkn/accesslog/config/profile/%s/time-interval", profile);
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

#if 0
static int
cli_accesslog_time_interval_revmap(void *data, const cli_command *cmd,
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
	err = ts_new_sprintf(&rev_cmd, "accesslog rotate time-interval  %s\n", ts_str(t_name));
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
#endif
int
cli_accesslog_fileid_warning_message(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
	int err = 0;
	char *bn_name = NULL;
	const char *fileid = NULL;
	uint32 ret = 0;
	tstring *ret_msg = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	fileid = tstr_array_get_str_quick(params, 0);
	bail_null(fileid);

	bn_name = smprintf("/nkn/accesslog/config/max-fileid");
	bail_null(bn_name);

	err = mdc_set_binding(cli_mcc, &ret, &ret_msg,
			bsso_modify, bt_uint32, fileid, bn_name);
	if(ret) {
		goto bail;

	}else {
	        err = cli_printf("warning: Increasing max_fileid would take more space on the disk"
				" and hence possibly reduce the performance of MFC.\n" 
				"(The recommended max_fileid is 10.)\n");
	}
bail:
	safe_free(bn_name);
	ts_free(&ret_msg);
	return err;
}


#if 0
static int
cli_accesslog_ret_code(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params)
{
	int err, length, i = 0;
	unsigned int j = 0;
	const char *r_code = NULL;
	const char *s_r_code = NULL;
	tstr_array *r_code_list = NULL;
        uint32 ret = 0;
        tstring *ret_msg = NULL;
	char *bn_name = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
        r_code = tstr_array_get_str_quick(params, 0);
        bail_null(r_code);

	err = ts_tokenize_str(r_code, ' ', '\\', '"', 0, &r_code_list);
	bail_error(err);

	length = tstr_array_length_quick(r_code_list);
	for ( i = 0; i < length; i++){
		s_r_code = tstr_array_get_str_quick(r_code_list, i);
		for (j = 0; j < strlen(s_r_code); j++) {
			if (!(isdigit(s_r_code[j]))){
				cli_printf("Bad input value");
				goto bail;
			}
		}
	}
	if (!atoi(r_code)){
		cli_printf("Bad input value");
                goto bail;
	}

        bn_name = smprintf("/nkn/accesslog/config/skip_response_code");
       	bail_null(bn_name);

       	err = mdc_set_binding(cli_mcc, &ret, &ret_msg,
                        bsso_modify, bt_string, r_code, bn_name);
	bail_error(err);

bail:
        safe_free(bn_name);
        ts_free(&ret_msg);
	return err;
}
#endif
int
cli_nkn_publishlog_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;
    const char *mode = NULL;
    char *buf = NULL;
    uint32 buflen = 0;
    uint32 fileid = 0;
    tstr_array *files = NULL;
    tstr_array *names = NULL;
    uint32 len = 0;
    char *password = NULL;
    tstring *al_filename = NULL;
    int el_id = 0, cache_id = 0, tl_id = 0;

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

        err = lf_read_file("/var/log/nkn/.nknlogd_log.fileid", &buf, &buflen);
        bail_error(err);

        if (buflen) {
            sscanf(buf, "fileid=%d %d %d %d", &fileid, &el_id, &cache_id, &tl_id);
            err = tstr_array_new(&files, NULL);
            bail_error(err);

            err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/mfplog/config/filename", NULL);
            bail_error(err);

            err = tstr_array_append_sprintf(files, "%s", 
                    ts_str(al_filename));
            bail_error(err);

            err = tstr_array_append_sprintf(files, "%s", remote_url);
            bail_error(err);

            err = cli_file_upload(data, cmd, cmd_line, files);
            bail_error(err);
        }
    }
    else {
        tbool prompt_pass = false;
        err = cli_file_get_filenames("/var/log/nkn", &names);
        bail_error_null(err, names);

        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
                    &al_filename, "/nkn/mfplog/config/filename", NULL);
        bail_error(err);

        err = tstr_array_delete_non_prefixed(names, ts_str(al_filename));
        bail_error(err);
        len = tstr_array_length_quick(names);
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
    safe_free(buf);
    tstr_array_free(&files);
    return err;
}


#if 0
static int
cli_nkn_accesslog_remote_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	int nparams = 0;
	const char *addr = NULL;
	char *ip = NULL, *port = NULL;
	const char *interface = NULL;
	bn_request *req = NULL;
	unsigned int ret_code = 0;
	tstring *ret_msg = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	nparams = tstr_array_length_quick(params);

	// get the remote IP and port optionally
	addr = tstr_array_get_str_quick(params, 0);
	bail_null(addr);

	err = cli_nkn_accesslog_parse_remote_addr(addr, &ip, &port);
	bail_error(err);


	err = bn_set_request_msg_create(&req, 0);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding(req, bsso_modify, 0, 
			"/nkn/accesslog/config/remote/ip", 
			bt_ipv4addr, 
			0, 
			ip, NULL);
	bail_error(err);

	if (port != NULL) {
		err = bn_set_request_msg_append_new_binding(req, bsso_modify, 0, 
				"/nkn/accesslog/config/remote/port", 
				bt_uint16, 
				0, 
				port, NULL);
		bail_error(err);
	}

	if (nparams == 2) {
		// user configured an interface also. 
		interface = tstr_array_get_str_quick(params, 1);
		if (interface != NULL) {
			err = bn_set_request_msg_append_new_binding(req, bsso_modify, 0, 
					"/nkn/accesslog/config/remote/interface", 
					bt_string, 
					0, 
					interface, NULL);
			bail_error(err);
		}
	}

	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &ret_code, &ret_msg);
	bail_error(err);


bail:
	if (err == 0) {
		cli_printf(_(
			"Warning: This config change requires a service restart.\n"
			"         Please restart service using 'service restart mod-delivery'\n"
			"         All currently configured accesslog formats on this host are\n"
			"         invalid. Ensure that the remote logger instance is running\n"
			"         and configured correctly.\n"));
	}

	bn_request_msg_free(&req);
	safe_free(ip);
	safe_free(port);
	ts_free(&ret_msg);

	return err;
}

static int 
cli_nkn_accesslog_parse_remote_addr(const char *addr, 
		char **pip, char **pport)
{
	int err = 0;
	const char *p = NULL, *c = NULL, *p_end = NULL;
	unsigned int len = 0;
	char *port = NULL, *ip = NULL;

	bail_null(addr);
	bail_null(pip);
	bail_null(pport);

	*pip = NULL;
	*pport = NULL;

	p = addr;
	len = strlen(addr);

	c = strchr(p, ':');
	if (c != NULL) {
		// port specified
		p_end = p + (c - addr); // account for ':'
		port = alloca(addr + len - p_end + 1);
		c++;
	} else {
		p_end = addr + len;
	}

	// grab ip
	ip = alloca(p_end - p);

	if (ip) {
		strncpy(ip, p, (p_end - p));
		ip[p_end - p] = 0;
	}

	if (port) {
		strncpy(port, c, (addr + len - p_end + 1));
		port[addr + len - p_end] = 0;
	}


	// Allocated memory here is freeed by caller
	if (ip) {
		*pip = smprintf("%s", ip);
	}

	if (port) {
		*pport = smprintf("%s", port);
	}

bail:
	return err;
}
#endif

int
cli_acclog_config_show_one_terse(const bn_binding_array *bindings,
			   uint32 idx,
			   const bn_binding *binding,
			   const tstring *name,
			   const tstr_array *name_components,
			   const tstring *name_last_part,
			   const tstring *value,
			   void *cb_data)
{
    int err = 0;
    tbool is_filter = false;
    tbool is_export = false;
    tstring *t_export = NULL,
	    *t_size = NULL,
	    *t_name = NULL,
	    *t_value = NULL,
	    *t_file = NULL,
	    *t_profile = NULL;
    char *resp_code_pattern = NULL;
    uint32 ret_idx = 0;

    (void) idx;
    (void) binding;
    (void) name_components;
    (void) name_last_part;
    (void) cb_data;
    (void) value;

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_export,
	    "%s/upload/url", ts_str(name));
    bail_error(err);

    if (t_export && ts_num_chars(t_export)) {
	is_export = true;
    }

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_size,
	    "%s/object/filter/size", ts_str(name));
    bail_error(err);

    resp_code_pattern = smprintf("%s/object/filter/resp-code/**",
	    ts_str(name));
    err = bn_binding_array_get_first_matching_value_tstr(
	    bindings, resp_code_pattern, 0, &ret_idx, &t_name, &t_value);
    bail_error(err);

    if ((ret_idx || (t_name && ts_num_chars(t_name))) ||
	    (t_size && ts_num_chars(t_size))) {
	is_filter = true;
    }

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, NULL, &t_file,
	    "%s/filename", ts_str(name));
    bail_error(err);

    if (ts_num_chars(t_file) > 20) {
	ts_trim_num_chars(t_file, 0, ts_length(t_file) - 16);
	ts_append_str(t_file, "...");
    }

    ts_dup(value, &t_profile);
    if (ts_num_chars(t_profile) > 20) {
	ts_trim_num_chars(t_profile, 0, ts_length(t_profile) - 16);
	ts_append_str(t_profile, "...");
    }

    /* Format:
     * ProfileName | Filename | FormatClass | Export | Filter
     * Length: (78 chars, incl. space and |)
     * 20 | 20 | 15 | 5 | 6
     */
    if (idx == 0) {
	cli_printf(_("%-20s | %-20s | %-15s | %-5s | %-6s\n"),
		"Profile Name", "Log File", "Format Type", "Exprt", "Filter");

	cli_printf(_("------------------------------------------------------------------------------\n"));
    }

    err = cli_printf_query(
	    _("%-20s | %-20s | #W:15~/nkn/accesslog/state/profile/%s/format-class# |   %c   |   %c\n"),
	    ts_str(t_profile), ts_str(t_file), ts_str(t_profile),
	    (is_export) ? 'Y' : 'N',
	    (is_filter) ? 'Y' : 'N');

bail:
    ts_free(&t_export);
    ts_free(&t_size);
    ts_free(&t_name);
    ts_free(&t_value);
    ts_free(&t_file);
    ts_free(&t_profile);
    safe_free(resp_code_pattern);
    return err;
}

int
cli_acclog_show_config_one_verbose(void *data,
				   const cli_command *cmd,
				   const tstr_array *cmd_line,
				   const tstr_array *params)
{
    int err = 0;
    uint32 i = 0;
    const char *profile_name = NULL;
    bn_binding_array *bindings = NULL;
    char *pattern = NULL;
    tstr_array *t_resp_codes = NULL;
    tstring *url = NULL;
    char * url_binding = NULL;
    tstring * ret_url_pw_obfuscated = NULL;
    tbool ret_changed = false;

    (void)data;
    (void) cmd;
    (void) cmd_line;

    profile_name = tstr_array_get_str_quick(params, 0);

    pattern = smprintf("/nkn/accesslog/config/profile/%s", profile_name);

    err = mdc_get_binding_children(cli_mcc, NULL, NULL, true,
	    &bindings, true, true, pattern);
    bail_error(err);

    if (bn_binding_array_length_quick(bindings) == 0) {
	/* bad profile name */
	err = 0;
	goto bail;
    }

    cli_printf(_("Access Log Profile: %s\n"), profile_name);

    err = cli_printf_prequeried(bindings, _("  Log Filename : "
		"#%s/filename#\n"), pattern);
    bail_error(err);

    err = cli_printf_prequeried(bindings, _("  Max Filesize : "
		"#%s/max-filesize# MiB\n"), pattern);
    bail_error(err);

    err = cli_printf_prequeried(bindings, _("  Max time duration : "
		"#%s/time-interval# Minutes\n"), pattern);
    bail_error(err);

    err = cli_printf_query(_("  Format Type: "
		"#/nkn/accesslog/state/profile/%s/format-class#\n"), profile_name);
    bail_error(err);
    err = cli_printf_prequeried(bindings, _("  Format : "
		"#%s/format#\n"), pattern);
    bail_error(err);

#if 0
    err = cli_printf_prequeried(bindings, _("  On the hour log rotation : "
		"#e:-Not Configured-~%s/on-the-hour#\n"), pattern);
#endif
    url_binding = smprintf("%s/upload/url",pattern );
    err = bn_binding_array_get_value_tstr_by_name(bindings,url_binding,NULL,&url);
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

    err = cli_printf_prequeried(bindings, _("  Object Size to Skip : "
		"#%s/object/filter/size#\n"), pattern);
    bail_error(err);

    //mdc_get_binding_children_tstr_array

    safe_free(pattern);
    pattern = smprintf("/nkn/accesslog/config/profile/%s/object/filter/resp-code/*",
	    profile_name);

    err = bn_binding_array_get_name_part_tstr_array(bindings,
	    pattern, 8, &t_resp_codes);
    bail_error(err);

    cli_printf(_("  Response Codes to Skip : "));
    if (tstr_array_length_quick(t_resp_codes)) {
	for(i = 0; i < tstr_array_length_quick(t_resp_codes); i++) {
	    cli_printf(_("%s "), tstr_array_get_str_quick(t_resp_codes, i));
	}
	cli_printf("\n");
    } else {
	cli_printf("None\n");
    }


bail:
    tstr_array_free(&t_resp_codes);
    safe_free(pattern);
    ts_free(&url);
    ts_free(&ret_url_pw_obfuscated);
    bn_binding_array_free(&bindings);
    return err;
}


#if 0
static int
cli_acclog_validate_resp_codes(const tstr_array *params)
{
    int err = 0;
    int len = tstr_array_length_quick(params);
    //uint16_t ret_num = 0;
    int i = 0;

    for(i = 1; i < len; i++) {
	//const char *tmp = tstr_array_get_str_quick(params, i);

	//err = lc_str_to_uint16(tmp, &ret_num);
	//bail_error(err);
    }

//bail:
    return err;
}
#endif

static int
cli_nkn_acclog_skip_response_code(void *data, const cli_command *cmd,
				  const tstr_array *cmd_line,
				  const tstr_array *params)
{
    int err = 0;
    const char *profile = NULL;
    int i = 0;
    int num_params = 0;
    const char *param = NULL;
    bn_request *req = NULL;
    int node_id = 0;
    unsigned int ret_val = 0;
    bn_set_subop mode = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);


    profile = tstr_array_get_str_quick(params, 0);
    num_params = tstr_array_length_quick(params);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);


    switch(cmd->cc_magic)
    {
    case cid_acclog_respcode_delete:
	mode = bsso_delete;
	break;
    case cid_acclog_respcode_add:
	mode = bsso_create;
	break;
    default:
	bail_force(lc_err_unexpected_case);
	break;
    }

    for(i = 1; i < num_params; i++) {
	param = tstr_array_get_str_quick(params, i);
	bail_null(param);

	err = bn_set_request_msg_append_new_binding_fmt(req,
		mode, 0, bt_uint16, 0,
		param, &node_id,
		"/nkn/accesslog/config/profile/%s/object/filter/resp-code/%s",
		profile, param);
	bail_error(err);
    }

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &ret_val, NULL);
    bail_error(err);
bail:
    bn_request_msg_free(&req);
    return err;
}


/* ------------------------------------------------------------------------- */
/* End of cli_nkn_accesslog_cmds.c */
