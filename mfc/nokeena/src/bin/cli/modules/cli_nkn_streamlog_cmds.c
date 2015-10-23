/*
 * Filename :   cli_nkn_streamlog_cmds.c
 * Author :     Dhruva
 * Created On : 01/05/2009
 *
 * (C) Copyright 2008-2009 Nokeena Networks Inc,
 * All rights reserved.
 */

#include "common.h"
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

int 
cli_nkn_streamlog_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


int 
cli_nkn_streamlog_format_help(
    void *data,
    cli_help_type help_type,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params, 
    const tstring *curr_word,
    void *context);

int 
cli_nkn_streamlog_config_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
cli_streamlog_file_upload_finish(const char *password, clish_password_result result,
    const cli_command *cmd,
    const tstr_array *cmd_line, const tstr_array *params,
    void *data, tbool *ret_continue);

int 
cli_nkn_streamlog_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nkn_streamlog_show_continuous(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params);

int
cli_streamlog_warning_message(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);

static int
cli_streamlog_time_interval_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nkn_streamlog_parse_remote_addr(const char *addr,
		char **pip, char **pport);

static int
cli_nkn_streamlog_remote_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int 
cli_nkn_streamlog_cmds_init(
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
    cmd->cc_words_str =             "streamlog";
    cmd->cc_help_descr =            N_("Configure stream log options");
    cmd->cc_flags =                 ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no streamlog";
    cmd->cc_help_descr =            N_("Delete streamlog configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog no";
    cmd->cc_help_descr =            N_("Clear streamlog options");
    cmd->cc_req_prefix_count =      1;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str = 	"streamlog remote";
    cmd->cc_help_descr = 	N_("Configure remote logger options");
    cmd->cc_flags = 		ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"streamlog remote *";
    cmd->cc_help_exp = 		N_("<ip[:port]>");
    cmd->cc_help_exp_hint = 	N_("Configure remote IP and optionally a port "
				"on which the logger receives log messages. (Default port: 7958)");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_help_term = 	N_("Configure remote logger address and use default "
		    		"management port as outbound interface");
    cmd->cc_exec_callback = 	cli_nkn_streamlog_remote_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"streamlog remote * on-interface";
    cmd->cc_help_descr = 	N_("Configure outbound interface to use for logging");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"streamlog remote * on-interface *";
    cmd->cc_help_exp =		N_("<interface>");
    cmd->cc_help_exp_hint = 	N_("outbound interface to use to connect to remote logger");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback = 	cli_nkn_streamlog_remote_config;
    CLI_CMD_REGISTER;


    // Bug 1826 introduced consolidated log manager. As a 
    // result this CLI is being moved to hidden
    // and also will display a message when executed.
    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog enable";
    cmd->cc_help_descr =            N_("Enable stream logging");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden ;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/enable";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_streamlog;
 
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no streamlog enable";
    cmd->cc_help_descr =            N_("Disable stream log");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden ;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/enable";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog syslog";
    cmd->cc_help_descr =            N_("Configure syslog options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog syslog replicate";
    cmd->cc_help_descr =            N_("Configure syslog replication");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog syslog replicate enable";
    cmd->cc_help_descr =            N_("Enable syslog replication of stream log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/syslog";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog syslog replicate disable";
    cmd->cc_help_descr =            N_("Disable syslog replication of stream log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/syslog";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog filename";
    cmd->cc_help_descr =            N_("Configure filename");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog filename stream.log";
    cmd->cc_help_descr =            N_("Set default filename - stream.log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/filename";
    cmd->cc_exec_value =            "stream.log";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_none;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog filename *";
    cmd->cc_help_exp =              N_("<filename>");
    cmd->cc_help_exp_hint =         N_("Set custom filename");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/filename";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog rotate";
    cmd->cc_help_descr =            N_("Configure rotation for log files");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog rotate enable";
    cmd->cc_help_descr =            N_("Turn ON log rotation for streamlog");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/rotate";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog rotate disable";
    cmd->cc_help_descr =            N_("Turn OFF log rotation for streamlog");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/rotate";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog rotate filesize";
    cmd->cc_help_descr =            N_("Configure maximum file size per log file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog rotate filesize 100";
    cmd->cc_help_descr =            N_("Set default size per log file - 100 MiBytes");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/max-filesize";
    cmd->cc_exec_value =            "100";
    cmd->cc_exec_type =             bt_uint16;
    cmd->cc_revmap_type =           crt_none;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog rotate filesize *";
    cmd->cc_help_exp =              N_("<size in MiB>");
    cmd->cc_help_exp_hint =         N_("Set custom filesize (in MiB)");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/max-filesize";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint16;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog rotate time-interval";
    cmd->cc_help_descr =            N_("Configure time-interval per log file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog rotate time-interval *";
    cmd->cc_help_exp =              N_("<time in Minutes>");
    cmd->cc_help_exp_hint =         N_("Set custom rotation time interval (in Minutes)");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/time-interval";
    cmd->cc_exec_callback =	    cli_streamlog_warning_message;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_order =          cro_streamlog;
    cmd->cc_revmap_names =          "/nkn/streamlog/config/time-interval";
    cmd->cc_revmap_callback =       cli_streamlog_time_interval_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog format";
    cmd->cc_help_descr =            N_("Configure stream log format string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog format clf";
    cmd->cc_help_descr =              N_("Configure %h %c %t %x \"%r\" %s %I %O");
    /*cmd->cc_help_exp_hint =         "";
    cmd->cc_help_callback =         cli_nkn_streamlog_format_help;*/
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/format";
    cmd->cc_exec_value =            "%h %c %t %x \"%r\" %s %I %O"; 
    cmd->cc_exec_type =             bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog format display ";
    cmd->cc_help_descr =            N_("Display of format in the log");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;                                                                 
    cmd->cc_words_str =             "streamlog format display enable";
    cmd->cc_help_descr =            N_("Enable log format display in log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/format-display";
    cmd->cc_exec_value =            "true"; 
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;                                                                 
    cmd->cc_words_str =             "streamlog format display disable";
    cmd->cc_help_descr =            N_("Enable log format display in log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/format-display";
    cmd->cc_exec_value =            "false"; 
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog format *";
    cmd->cc_help_exp =              N_("<format string>");
    cmd->cc_help_exp_hint =         "";
    cmd->cc_help_callback =         cli_nkn_streamlog_format_help;
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/format";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog copy";
    cmd->cc_help_descr =            N_("Auto-copy the streamlog based on rotate criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog copy *";
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_magic =                 cfi_streamlog;
    cmd->cc_help_exp =              N_("[scp|sftp|ftp]://<username>:<password>@<hostname>/<path>"); 
    cmd->cc_exec_callback =         cli_nkn_streamlog_config_upload;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no streamlog copy";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_help_descr =            N_("Disable auto upload of stream logs");
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/streamlog/config/upload/pass";
    cmd->cc_exec_name2 =            "/nkn/streamlog/config/upload/url";
    CLI_CMD_REGISTER;
   
    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog on-the-hour ";
    cmd->cc_help_descr =            N_("Set on the hour log rotation option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog on-the-hour enable";
    cmd->cc_help_descr =            N_("Enable On the hour log rotation");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/on-the-hour";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "streamlog on-the-hour disable";
    cmd->cc_help_descr =            N_("Disable On the hour log rotation");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/streamlog/config/on-the-hour";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_streamlog;
    CLI_CMD_REGISTER;

    /* Command to view streamlog configuration */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show streamlog";
    cmd->cc_help_descr =            N_("View stream log configuration");
    cmd->cc_flags =                 ccf_terminal; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_streamlog_show_config;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show streamlog continuous";
    cmd->cc_help_descr =            N_("View continuous stream log");
    cmd->cc_flags =                 ccf_terminal; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_streamlog_show_continuous;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show streamlog last";
    cmd->cc_help_descr =            N_("View last lines of stream log");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show streamlog last *";
    cmd->cc_help_exp =               N_("<number>");
    cmd->cc_help_exp_hint =          N_("Number of lines to be displayed");
    cmd->cc_flags =                 ccf_terminal; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_streamlog_show_continuous;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings("/nkn/streamlog/config/upload/*");
    bail_error(err);
    
    err = cli_revmap_ignore_bindings("/nkn/streamlog/config/path");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/streamlog/config/serverip");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/streamlog/config/serverport");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/streamlog/config/remote/**");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/errorlog/config/**");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/mfdlog/config/**");
    bail_error(err);


bail:
    return err;
}

int 
cli_nkn_streamlog_format_help(
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
        char    c;
        const char    *help;
    } fmt_help[] = {
        {'t',   "Local time"},
        {'c',   "Destination IP"},
        {'d',   "Origin Server IP"},
        {'h',   "Client Source IP"},
        {'x',   "Transaction Code"},
        {'r',   "Streaming URI (urlSuffix)"},
        {'l',   "URI Stem"},
        {'s',   "Status"},
        {'p',   "Client Media Player version"},
        {'o',   "Client OS"},
        {'v',   "Client OS Version"},
        {'u',   "Client CPU type"},
        {'f',   "Length of the stream in seconds"},
        {'a',   "Average bandwidth in bytes per second"},
        {'i',   "Player information used in the header (User-Agent)"},
        {'j',   "Timestamp (seconds since Epoch) when transaction ended"},

        {0, NULL},
        
        {'C',   "Unique Client-side streaming identifier"},
        {'S',   "Unique Server-side streaming identifier"},
        {'T',   "GMT time when the transaction ended"},
        {'B',   "Time when client started receiving the stream"},
        {'E',   "Time when client stopped receiving the stream"},
        {'P',   "Total number of packets delivered to the client"},
        {'L',   "Protocol used during connection"},
        {'R',   "Transport protocol used during connection"},
        {'A',   "Action performed (SPLIT, PROXY, CACHE, CONNECTING)"},
        {'D',   "Number of packets dropped"},
        {'M',   "Duration of time the client received the content"},
        {'X',   "Streaming product used to create and stream content"},
        {'K',   "Number of packets resent to the client"},
        {'I',   "Number of bytes received"},
        {'O',   "Number of bytes transmitted"},
        {'N',   "Namespace name"},

    };

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(help_type);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);
    UNUSED_ARGUMENT(curr_word);
    UNUSED_ARGUMENT(context);


    for ( i = 0; i < sizeof(fmt_help)/sizeof(struct fmt_help_s); i++) {
        if ( fmt_help[i].c == 0 ) {
            cli_printf("\n");
            continue;
        }

        err = cli_printf("%%%c    %-20s\n", fmt_help[i].c, fmt_help[i].help);
        bail_error(err);

    }
#if 0
    err = cli_printf("<string>           Specify format string\n");
    bail_error(err);
    err = cli_printf(
            " %%t   local time              \n"
            " %%9  rfc931 Auth server                   \n"
            " %%b  bytes_out_no_header                \n"
            " %%c  cache hit indicator    %%D  time_used_ms \n"
            "                            %%E  time_used_sec \n"
            " %%f  filename               %%H  request_protocol \n"
            " %%h  remote_host            %%I  bytes_in \n"
            " %%i  header                 %%N  namespace name \n"
            "                            %%O  bytes_out \n"
            "                            %%U  url \n"
            " %%m  request_method         %%V  http_host \n"
            " %%o  response_header                               \n"
            " %%q  query_string           %%X  remote_addr \n"
            " %%r  request_line           %%Y  local_addr \n"
            " %%s  status                 %%Z  server_port \n"
            " %%t  timestamp \n"
            " %%u  remote_user \n"
            " %%v  server_name \n"
            "                 \n"
            " %%y  status_subcode \n");
    bail_error(err);
#endif

bail:
    return err;
}

int 
cli_nkn_streamlog_show_config(
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
                    (_("Media Flow Contoller Stream Log enabled : "
                       "#/nkn/streamlog/config/enable#\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Replicate to Syslog : "
               "#/nkn/streamlog/config/syslog#\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Stream Log filename : "
               "#/nkn/streamlog/config/filename#\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Stream Log filesize : "
               "#/nkn/streamlog/config/max-filesize# MiB\n"));
    bail_error(err);

    err = cli_printf_query
                    (_("  Stream Log Rotation enabled : "
                       "#/nkn/streamlog/config/rotate#\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Stream Log time interval : "
               "#/nkn/streamlog/config/time-interval# Minutes\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Stream log format : "
               "#/nkn/streamlog/config/format#\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Stream log display format : "
               "#/nkn/streamlog/config/format-display#\n"));
    bail_error(err);

    err = cli_printf_query
	    (_("  Stream log On the hour log rotation : "
	       "#/nkn/streamlog/config/on-the-hour#\n"));

    err = mdc_get_binding_children(cli_mcc, NULL, NULL, true,
            &bindings, true, true, "/nkn/streamlog");
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings,"/nkn/streamlog/config/upload/url",NULL,&url);
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

static int
cli_streamlog_file_upload_finish(const char *password, clish_password_result result,
    const cli_command *cmd,
    const tstr_array *cmd_line, const tstr_array *params,
    void *data, tbool *ret_continue)
{
    int err = 0;
    const char *remote_url = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);
    UNUSED_ARGUMENT(ret_continue);

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
                cli_mcc, NULL, NULL, "/nkn/streamlog/action/upload", 2,
                "remote_url", bt_string, remote_url,
                "password", bt_string, password);
        bail_error(err);
    }
    else {
        err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/nkn/streamlog/action/upload", 1,
                "remote_url", bt_string, remote_url);
        bail_error(err);
    }
bail:
    return err;
}



int 
cli_nkn_streamlog_config_upload(
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

    if (((lc_is_prefix("scp://", remote_url, false)) ||
        (lc_is_prefix("sftp://", remote_url, false))) && (clish_is_shell())){
        err = clish_maybe_prompt_for_password_url
                (remote_url, cli_streamlog_file_upload_finish, cmd, cmd_line, params, NULL);
            bail_error(err);
         }
    else {  
      	err = cli_streamlog_file_upload_finish(NULL, cpr_ok, cmd, cmd_line, params,
               	    NULL, NULL);
        bail_error(err);
       }

bail:
    return err;
}

int
cli_nkn_streamlog_show_continuous(void *data, const cli_command *cmd,
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
        	&al_filename, "/nkn/streamlog/config/filename", NULL); 
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
cli_streamlog_warning_message(void *data, const cli_command *cmd,
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

	bn_name = smprintf("/nkn/streamlog/config/time-interval");
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
cli_streamlog_time_interval_revmap(void *data, const cli_command *cmd,
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
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(name_parts);

        err = bn_binding_array_get_value_tstr_by_name(bindings, name, NULL, &t_name);
	bail_error_null(err, t_name);
	err = ts_new_sprintf(&rev_cmd, "streamlog rotate time-interval  %s\n", ts_str(t_name));
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


static int
cli_nkn_streamlog_remote_config(void *data,
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

	err = cli_nkn_streamlog_parse_remote_addr(addr, &ip, &port);
	bail_error(err);


	err = bn_set_request_msg_create(&req, 0);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding(req, bsso_modify, 0,
			"/nkn/streamlog/config/remote/ip",
			bt_ipv4addr,
			0,
			ip, NULL);
	bail_error(err);

	if (port != NULL) {
		err = bn_set_request_msg_append_new_binding(req, bsso_modify, 0,
				"/nkn/streamlog/config/remote/port",
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
					"/nkn/streamlog/config/remote/interface",
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
cli_nkn_streamlog_parse_remote_addr(const char *addr,
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



