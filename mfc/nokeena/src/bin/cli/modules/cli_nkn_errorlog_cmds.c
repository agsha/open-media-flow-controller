/*
 *
 * File Name: cli_nkn_errorlog_cmds.c 
 *
 * Author: Saravanan
 * 
 * Creation Date: 
 *
 * 
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
#include "nkn_debug.h"

int 
cli_nkn_errorlog_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


static int
cli_errorlog_file_upload_finish(const char *password, clish_password_result result,
    const cli_command *cmd,
    const tstr_array *cmd_line, const tstr_array *params,
    void *data, tbool *ret_continue);

static int
cli_errorlog_time_interval_revmap(void *data, const cli_command *cmd,
                           const bn_binding_array *bindings,
                           const char *name,
                           const tstr_array *name_parts,
                           const char *value, const bn_binding *binding,
                           cli_revmap_reply *ret_reply);


static int
cli_nkn_errorlog_parse_remote_addr(const char *addr,
		char **pip, char **pport);

static int
cli_nkn_errorlog_remote_config(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int 
cli_nkn_errorlog_config_upload(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int 
cli_nkn_errorlog_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_errorlog_warning_message(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);

int
cli_nkn_errorlog_show_continuous(void *data, const cli_command *cmd,
                         const tstr_array *cmd_line,
                         const tstr_array *params);

uint64_t  convert_hex_to_uint64 (const char *str);
const char * convert_uint64_to_hex (uint64_t num, char * buf , int size);

int cli_errorlog_module_show(void);

static int
cli_errorlog_module_unset(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

static int
cli_errorlog_module_set(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

typedef enum {
	celm_network	= 0x0000000000000001,
	celm_http	= 0x0000000000000002,
	celm_dm2	= 0x0000000000000004,
	celm_mm		= 0x0000000000000008,
	celm_bm		= 0x0000000000000010,
	celm_ce		= 0x0000000000000020,
	celm_sched	= 0x0000000000000040,
	celm_ssp	= 0x0000000000000080,
	celm_httphdrs	= 0x0000000000000100,
	celm_fqueue	= 0x0000000000000200,
	celm_system	= 0x0000000000000400,
	celm_fm		= 0x0000000000000800,
	celm_om		= 0x0000000000001000,
	celm_oomgr	= 0x0000000000002000,
	celm_tcp	= 0x0000000000004000,
	celm_tfm	= 0x0000000000008000,
	celm_namespace	= 0x0000000000010000,
	celm_nfs	= 0x0000000000020000,
	celm_rtsp	= 0x0000000000040000,
	celm_am		= 0x0000000000080000,
	celm_mm_promote = 0x0000000000100000,
	celm_fuse	= 0x0000000000200000,
	celm_vpemgr	= 0x0000000000400000,
	celm_gm		= 0x0000000000800000,
	celm_dashboard	= 0x0000000001000000,
	celm_cachelog	= 0x0000000002000000,
	celm_cp		= 0x0000000004000000,
	celm_tunnel	= 0x0000000008000000,
	celm_hrt	= 0x0000000010000000,
	celm_cluster	= 0x0000000020000000,
	celm_ssl	= 0x0000000040000000,
	celm_mfpfile	= 0x0000000080000000,
	celm_mfplive	= 0x0000000100000000,
	celm_l7redir	= 0x0000000200000000,
	celm_em		= 0x0000000400000000,
	celm_resrc_pool	= MOD_RESRC_POOL,
	celm_authmgr	= MOD_AUTHMGR,
	celm_auth_help	= MOD_AUTHHLP,
	celm_adns	= MOD_ADNS,
	celm_proxyd	= MOD_PROXYD,
	celm_geodb	= MOD_GEODB,
	celm_pe_mgr	= MOD_PEMGR,
	celm_cse	= MOD_CSE,
	celm_cde	= MOD_CDE,
	celm_cue	= MOD_CUE,
	celm_ccp	= MOD_CCP,
	celm_crawl	= MOD_CRAWL,
	celm_cim	= MOD_CIM,
	celm_iccp	= MOD_ICCP,
	celm_ucflt	= MOD_UCFLT,
	celm_nknexecd	= MOD_NKNEXECD,
	celm_vcs	= MOD_VCS,
	celm_compress	= MOD_COMPRESS,
	celm_cb		= MOD_CB,
	celm_url_filter = MOD_URL_FILTER,
	celm_dpi_tproxy = MOD_DPI_TPROXY,
	celm_jpsd = MOD_JPSD,


	// Hole!!
	celm_fuselog	= 0x2000000000000000,
	celm_trace	= 0x4000000000000000,
	celm_sys	= 0x8000000000000000,
	celm_all_modules = 0x00FFFFFFFFFFFFFF,

} cli_errorlog_module_t;


cli_expansion_option cli_errorlog_module_opts[] = {
	{"http", N_("Enable logging for HTTP"), (void*) (celm_http | celm_httphdrs | celm_om | celm_oomgr)},
	{"rtsp", N_("Enable logging for RTSP"), (void*) (celm_rtsp | celm_httphdrs | celm_om | celm_oomgr)},
	{"media-cache", N_("Enable logging for media-caches (disk, ram, etc.)"), (void *) (celm_mm | celm_bm | celm_ce | celm_fm | celm_tfm | celm_am | celm_mm_promote | celm_gm | celm_dm2)},
	{"network", N_("Enable logging for network modules"), (void *) (celm_network | celm_cp | celm_tunnel | celm_tcp)},
	{"dm2", N_("Enable logging for DM2"), (void *) (celm_dm2)},
	{"mm", N_("Enable logging for MM"), (void *) (celm_mm)},
	{"bm", N_("Enable logging for BM"), (void *) (celm_bm)},
	{"nfs", N_("Enable logging for NFS origins"), (void *) (celm_nfs)},
	{"mfp-file", N_("Enable logging for Media Flow Publisher - File Streams"), (void *) (celm_mfpfile)},
	{"mfp-live", N_("Enable logging for Media Flow Publisher - Live Streams"), (void *) (celm_mfplive)},
	{"cluster", N_("Enable logging for cluster manager"), (void *)(celm_cluster)},
	{"cll7", N_("Enable logging for L7 redirect"), (void *)(celm_l7redir)},
	{"auth-manager", N_("Enable logging for Authentication manager"), (void *) (celm_em | celm_ssl)},
	{"virtual-player", N_("Enable logging for Virtual Player"), (void *) (celm_ssp | celm_vpemgr)},
	{"resource-pool", N_("Enable logging for Resource Pool"), (void *) (celm_resrc_pool)},
	{"authmgr", N_("Enable logging for AuthMgr"), (void *) (celm_authmgr)},
	{"auth-help", N_("Enable logging for Auth-Help"), (void *) (celm_auth_help)},
	{"adns", N_("Enable logging for ADNS"), (void *) (celm_adns)},
	{"proxyd", N_("Enable logging for Proxyd"), (void *) (celm_proxyd)},
	{"geodb", N_("Enable logging for Geodb"), (void *) (celm_geodb)},
	{"pe_mgr", N_("Enable logging for PE Manager"), (void *) (celm_pe_mgr)},
	{"cse", N_("Enable logging for CSE"), (void *) (celm_cse)},
	{"cde", N_("Enable logging for CDE"), (void *) (celm_cde)},
	{"cue", N_("Enable logging for CUE"), (void *) (celm_cue)},
	{"ccp", N_("Enable logging for CCP"), (void *) (celm_ccp)},
	{"crawl", N_("Enable logging for Crawaler"), (void *) (celm_crawl)},
	{"cim", N_("Enable logging for CIM"), (void *) (celm_cim)},
	{"iccp", N_("Enable logging for ICCP"), (void *) (celm_iccp)},
	{"ucflt", N_("Enable logging for UC-Filter"), (void *) (celm_ucflt)},
	{"nknexecd", N_("Enable logging for NKN-Execd"), (void *) (celm_nknexecd)},
	{"vcs", N_("Enable logging for VCS"), (void *) (celm_vcs)},
	{"compress", N_("Enable logging for Compress"), (void *) (celm_compress)},
	{"cb", N_("Enable logging for CB"), (void *) (celm_cb)},
	{"url-filter", N_("Enable logging for Url Filter"), (void *) celm_url_filter},
	{"dpi-tproxy", N_("Enable logging for DPI TProxy"), (void *) celm_dpi_tproxy},
	{"jpsd", N_("Enable logging for jpsd"), (void *) celm_jpsd},
	{"all", N_("Enable logging for all modules"), (void *)(celm_all_modules)},
	{NULL, NULL, NULL}
};


int
cli_nkn_errorlog_cmds_init(
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
    cmd->cc_words_str =             "errorlog";
    cmd->cc_help_descr =            N_("Configure error log options");
    cmd->cc_flags =                 ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no errorlog";
    cmd->cc_help_descr =            N_("Delete errorlog configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog no";
    cmd->cc_help_descr =            N_("Clear errorlog options");
    cmd->cc_req_prefix_count =      1;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str = 	"errorlog remote";
    cmd->cc_help_descr = 	N_("Configure remote logger options");
    cmd->cc_flags = 		ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"errorlog remote *";
    cmd->cc_help_exp = 		N_("<ip[:port]>");
    cmd->cc_help_exp_hint = 	N_("Configure remote IP and optionally a port "
				"on which the logger receives log messages. (Default port: 7958)");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_help_term = 	N_("Configure remote logger address and use default "
		    		"management port as outbound interface");
    cmd->cc_exec_callback = 	cli_nkn_errorlog_remote_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"errorlog remote * on-interface";
    cmd->cc_help_descr = 	N_("Configure outbound interface to use for logging");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"errorlog remote * on-interface *";
    cmd->cc_help_exp =		N_("<interface>");
    cmd->cc_help_exp_hint = 	N_("outbound interface to use to connect to remote logger");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_callback = 	cli_nkn_errorlog_remote_config;
    CLI_CMD_REGISTER;

    // Bug 1826 introduced consolidated log manager. As a 
    // result this CLI is being moved to hidden
    // and also will display a message when executed.
    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog enable";
    cmd->cc_help_descr =            N_("Enable error logging");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/errorlog/config/enable";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_errorlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no errorlog enable";
    cmd->cc_help_descr =            N_("Disable error log");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/errorlog/config/enable";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_errorlog;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog syslog";
    cmd->cc_help_descr =            N_("Configure syslog options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog syslog replicate";
    cmd->cc_help_descr =            N_("Configure syslog replication");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog syslog replicate enable";
    cmd->cc_help_descr =            N_("Enable syslog replication of error log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/errorlog/config/syslog";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_errorlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog syslog replicate disable";
    cmd->cc_help_descr =            N_("Disable syslog replication of error log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/errorlog/config/syslog";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_errorlog;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog filename";
    cmd->cc_help_descr =            N_("Configure filename");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog filename error.log";
    cmd->cc_help_descr =            N_("Set default filename - error.log");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/errorlog/config/filename";
    cmd->cc_exec_value =            "error.log";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_none;
    cmd->cc_revmap_order =          cro_errorlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog filename *";
    cmd->cc_help_exp =              N_("<filename>");
    cmd->cc_help_exp_hint =         N_("Set custom filename");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/errorlog/config/filename";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_errorlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog level";
    cmd->cc_help_descr =            N_("set the error level");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog level *";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp       = 	    N_("<level>");
    cmd->cc_help_exp_hint  =        N_("\n1: SEVERE\n2: ERROR\n3: WARNING\n4: MESSAGE\n5-7: DEBUG");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/errorlog/config/level";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_errorlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		= 	"errorlog module";
    cmd->cc_help_descr     	= 	N_("set module flag");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no errorlog module";
    cmd->cc_help_descr =            N_("unset module flag");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"errorlog module *";
    cmd->cc_help_exp = 			N_("<module name>");
    cmd->cc_help_exp_hint = 		N_("module name to enable logging");
    cmd->cc_help_options = 		cli_errorlog_module_opts;
    cmd->cc_help_num_options = 	
	    sizeof(cli_errorlog_module_opts)/sizeof(cli_expansion_option);
    cmd->cc_flags = 			ccf_terminal;
    cmd->cc_capab_required =        	ccp_set_rstr_curr;
    cmd->cc_exec_callback = 		cli_errorlog_module_set;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"no errorlog module *";
    cmd->cc_help_exp = 			N_("<module name>");
    cmd->cc_help_exp_hint = 		N_("module name to enable logging");
    cmd->cc_help_options = 		cli_errorlog_module_opts;
    cmd->cc_help_num_options = 	
	    sizeof(cli_errorlog_module_opts)/sizeof(cli_expansion_option);
    cmd->cc_flags = 			ccf_terminal;
    cmd->cc_capab_required =        	ccp_set_rstr_curr;
    cmd->cc_exec_callback = 		cli_errorlog_module_unset;
    CLI_CMD_REGISTER;

#if 0
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog rotate";
    cmd->cc_help_descr =            N_("Configure rotation for log files");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog rotate enable";
    cmd->cc_help_descr =            N_("Turn ON log rotation for errorlog");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/errorlog/config/rotate";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_errorlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog rotate disable";
    cmd->cc_help_descr =            N_("Turn OFF log rotation for errorlog");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/errorlog/config/rotate";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_errorlog;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog rotate filesize";
    cmd->cc_help_descr =            N_("Configure maximum file size per log file");
    CLI_CMD_REGISTER;



    CLI_CMD_NEW;
    cmd->cc_words_str 		=       "errorlog rotate filesize 100";
    cmd->cc_help_descr 		=      	N_("Set default disk storage size for error logs - 100 MiB");
    cmd->cc_flags 		=       ccf_terminal;
    cmd->cc_capab_required 	=       ccp_set_rstr_curr;
    cmd->cc_exec_operation 	=       cxo_set;
    cmd->cc_exec_name 		=       "/nkn/errorlog/config/max-filesize";
    cmd->cc_exec_value 		=       "100";
    cmd->cc_exec_type 		=       bt_uint16;
    cmd->cc_revmap_type 	=       crt_none;
    cmd->cc_revmap_order 	=       cro_errorlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=       "errorlog rotate filesize *";
    cmd->cc_help_exp 		=       N_("<size in MiB>");
    cmd->cc_help_exp_hint 	=      	N_("Set total diskspace to use for error log files (in MiB)");
    cmd->cc_flags 		=       ccf_terminal;
    cmd->cc_capab_required 	=       ccp_set_rstr_curr;
    cmd->cc_exec_operation 	=       cxo_set;
    cmd->cc_exec_name 		=       "/nkn/errorlog/config/max-filesize";
    cmd->cc_exec_value 		=       "$1$";
    cmd->cc_exec_type 		=       bt_uint16;
    cmd->cc_revmap_type 	=       crt_auto;
    cmd->cc_revmap_order 	=       cro_errorlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=       "errorlog rotate time-interval";
    cmd->cc_help_descr 		=       N_("Configure time-interval per log file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 		=       "errorlog rotate time-interval *";
    cmd->cc_help_exp 		=       N_("<time in Minutes>");
    cmd->cc_help_exp_hint 	=      	N_("Set custom rotation time interval (in Minutes)");
    cmd->cc_flags 		=       ccf_terminal;
    cmd->cc_capab_required 	=       ccp_set_rstr_curr;
    cmd->cc_exec_operation 	=       cxo_set;
    cmd->cc_exec_name 		=       "/nkn/errorlog/config/time-interval";
    cmd->cc_exec_callback	=	cli_errorlog_warning_message;
    cmd->cc_revmap_type 	=       crt_manual;
    cmd->cc_revmap_order 	=       cro_errorlog;
    cmd->cc_revmap_names	=	"/nkn/errorlog/config/time-interval";
    cmd->cc_revmap_callback	=	cli_errorlog_time_interval_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog copy";
    cmd->cc_help_descr =            N_("Auto-copy the errorlog based on rotate criteria");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog copy *";
    cmd->cc_flags =                 ccf_terminal | ccf_sensitive_param;
    cmd->cc_magic =                 cfi_errorlog;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =              N_("[scp|sftp|ftp]://<username>:<password>@<hostname>/<path>");
    cmd->cc_exec_callback =         cli_nkn_errorlog_config_upload;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no errorlog copy";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_help_descr =            N_("Disable auto upload of error logs");
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/errorlog/config/upload/pass";
    cmd->cc_exec_name2 =            "/nkn/errorlog/config/upload/url";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog on-the-hour ";
    cmd->cc_help_descr =            N_("Set on the hour log rotation option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog on-the-hour enable";
    cmd->cc_help_descr =            N_("Enable On the hour log rotation");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/errorlog/config/on-the-hour";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_errorlog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "errorlog on-the-hour disable";
    cmd->cc_help_descr =            N_("Disable On the hour log rotation");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/errorlog/config/on-the-hour";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_errorlog;
    CLI_CMD_REGISTER;

    /* Command to view errorlog configuration */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show errorlog";
    cmd->cc_help_descr =            N_("View error log configuration");
    cmd->cc_flags =                 ccf_terminal; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_errorlog_show_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show errorlog continuous";
    cmd->cc_help_descr =            N_("View continuous error log ");
    cmd->cc_flags =                 ccf_terminal; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_errorlog_show_continuous;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show errorlog last";
    cmd->cc_help_descr =            N_("View last lines of error log ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show errorlog last *";
    cmd->cc_help_exp  =              N_("<number>");
    cmd->cc_help_exp_hint  =         N_("Set the number of linesto be displayed");
    cmd->cc_flags =                 ccf_terminal; 
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nkn_errorlog_show_continuous;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings("/nkn/errorlog/config/upload/*");
    bail_error(err);
    
    err = cli_revmap_ignore_bindings("/nkn/errorlog/config/path");
    bail_error(err);

    err = cli_revmap_ignore_bindings("/nkn/errorlog/config/**");
    bail_error(err);


bail:
    return err;
}

static int
cli_errorlog_file_upload_finish(const char *password, clish_password_result result,
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

    if ((!lc_is_prefix("scp://", remote_url, false)) &&
            (!lc_is_prefix("sftp://", remote_url, false))&&
                (!lc_is_prefix("ftp://", remote_url, false))){
        cli_printf_error(_("Bad destination specifier only supports scp or sftp or ftp"));
        goto bail;
    }
    
    if (password) {
        err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/nkn/errorlog/action/upload", 2,
                "remote_url", bt_string, remote_url,
                "password", bt_string, password);
        bail_error(err);
    }
    else {
        err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/nkn/errorlog/action/upload", 1,
                "remote_url", bt_string, remote_url);
        bail_error(err);
    }
bail:
    return err;
}

int 
cli_nkn_errorlog_config_upload(
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
                (remote_url, cli_errorlog_file_upload_finish, cmd, cmd_line, params, NULL);
            bail_error(err);
         }
    else {  
      	err = cli_errorlog_file_upload_finish(NULL, cpr_ok, cmd, cmd_line, params,
               	    NULL, NULL);
        bail_error(err);
       }
bail:
    return err;
}


int
cli_nkn_errorlog_show_config(
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

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_printf_query
                    (_("Media Flow Controller error Log enabled : "
                       "#/nkn/errorlog/config/enable#\n"));
    bail_error(err);
    err = cli_printf_query
            (_("  Replicate to Syslog : "
               "#/nkn/errorlog/config/syslog#\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Error Log filename : "
               "#/nkn/errorlog/config/filename#\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Error Log filesize : "
               "#/nkn/errorlog/config/max-filesize# MiB\n"));
    bail_error(err);

    err = cli_printf_query
                    (_("  Error Log Rotation enabled : "
                       "#/nkn/errorlog/config/rotate#\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Error Log time interval : "
               "#/nkn/errorlog/config/time-interval# Minutes\n"));
    bail_error(err);

    err = cli_printf_query
            (_("  Error log level: "
               "#/nkn/errorlog/config/level# \n"));
    bail_error(err);



    // TODO: braeakway point - show module as text instead of hex
    err = cli_errorlog_module_show();
    bail_error(err);

    //err = cli_printf_query
    //        (_("  Error log module: "
    //           "#/nkn/errorlog/config/mod# (Hex)\n"));
    //bail_error(err);

    err = cli_printf_query
	    (_("  Error log On the hour log rotation : "
	       "#/nkn/errorlog/config/on-the-hour#\n"));

    err = mdc_get_binding_children(cli_mcc, NULL, NULL, true,
            &bindings, true, true, "/nkn/errorlog");
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings,"/nkn/errorlog/config/upload/url",NULL,&url);
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

uint64_t  convert_hex_to_uint64 ( const char *str)
{
	uint64_t num = 0;
	//skip 2
	if (*str == '0' && *(str+1) == 'x')
		str += 2;
	while (*str)
	{
	     num <<= 4; 
	     if (( *str >= 'A' && *str <= 'F'))
	     {
		num += (*str - 'A') + 10;
	     } else if (( *str >= 'a' && *str <= 'f'))
	     {
		num += (*str - 'a') + 10;
	     }	else if ((*str >= '0' && *str <= '9')) {
		num += (*str - '0') ;
	     }
	     ++str; 	
	}
	return num;
}


const char * convert_uint64_to_hex ( uint64_t num, char * buf , int size)
{
        char *list = &buf[size - 1] ;
	int count_max = 0;
        const char * cList = "0123456789ABCDEF";

        *list = '\0';
        while (num)
        {
                int val = num % 16;
                num >>= 4;
                *--list = cList[val];
		++count_max;
        }
	while (count_max < 16)
	{
		*--list = cList[0];
		++count_max;
	}
        *--list = 'x';
        *--list = '0';
        return (const char *)list;
}


int
cli_nkn_errorlog_show_continuous(void *data, const cli_command *cmd,
                         const tstr_array *cmd_line,
                         const tstr_array *params)
{
	int err = 0;
 	tstr_array *argv = NULL;
 	const char *pattern = NULL;
 	tstring *al_filename = NULL;
 	const char *logfile = NULL;
 
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);
 	pattern = tstr_array_get_str_quick(params, 0);
 	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
         	&al_filename, "/nkn/errorlog/config/filename", NULL); 
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
 cli_errorlog_warning_message(void *data, const cli_command *cmd,
 			const tstr_array *cmd_line,
 			const tstr_array *params)
 {
 	int err = 0;
 	char *bn_name = NULL;
 	const char *time_interval = NULL;
 	uint32 ret = 0;
 	tstring *ret_msg = NULL;
 
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);
 	time_interval = tstr_array_get_str_quick(params, 0);
 	bail_null(time_interval);
 
 	bn_name = smprintf("/nkn/errorlog/config/time-interval");
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
 cli_errorlog_time_interval_revmap(void *data, const cli_command *cmd,
                           const bn_binding_array *bindings,
                           const char *name,
                           const tstr_array *name_parts,
                           const char *value, const bn_binding *binding,
                           cli_revmap_reply *ret_reply)
 {
 	int err = 0;
 	tstring *t_name = NULL;
 	tstring *rev_cmd = NULL;
 
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(name_parts);
	UNUSED_ARGUMENT(binding);

        err = bn_binding_array_get_value_tstr_by_name(bindings, name, NULL, &t_name);
	bail_error_null(err, t_name);
	err = ts_new_sprintf(&rev_cmd, "errorlog rotate time-interval  %s\n", ts_str(t_name));
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



static inline int get_current_mask(uint64_t *module)
{
	int err = 0;
	uint64_t module_mod = 0;
	tstring *mod_value = NULL;
	const char *mod_pointer = NULL;

	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &mod_value, "/nkn/errorlog/config/mod", NULL);
	bail_error(err);

	mod_pointer = ts_str(mod_value);
	if ((strlen(mod_pointer) >= 3) && (mod_pointer[0] == '0') && (mod_pointer[1] == 'x')) {
			module_mod = convert_hex_to_uint64(mod_pointer);
	} else {
		module_mod = atol(mod_pointer);
	}

	*module = module_mod;
bail:
	ts_free(&mod_value);
	return err;
}

static inline int set_new_mask(uint64_t mask)
{
	int err = 0;
	unsigned int retcode = 0;
	tstring *retmsg = NULL;
	char *c_mask = NULL;


	c_mask = smprintf("0x%lx", mask);
	bail_null(c_mask);

	err = mdc_set_binding(cli_mcc, 
			&retcode,
			&retmsg,
			bsso_modify,
			bt_string,
			c_mask,
			"/nkn/errorlog/config/mod");
	bail_error(err);
	
bail:
	safe_free(c_mask);
	ts_free(&retmsg);
	return err;
}


int cli_errorlog_module_show(void)
{
	int err = 0;
	unsigned int i;
	uint64_t module_mod = 0, mask = 0;

	err = get_current_mask(&module_mod);
	bail_error(err);

	if (module_mod == 0) {
		cli_printf(_("  Error Log modules configured: - logging turned off - \n"));
		goto bail;
	} else {
		cli_printf(_("  Error Log modules configured: "));
	}

	for (i = 0; i < sizeof(cli_errorlog_module_opts)/sizeof(cli_expansion_option); i++) {
		if (cli_errorlog_module_opts[i].ceo_option == NULL)
			break;

		mask = (uint64_t) (cli_errorlog_module_opts[i].ceo_data);
		if ((module_mod & mask) == mask) {
			cli_printf(_("%s "), cli_errorlog_module_opts[i].ceo_option);
		}
	}
	cli_printf(_("\n"));
bail:
	return err;
}



static int
cli_errorlog_module_set(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *module = NULL;
	unsigned int i;
	uint64_t mask = 0UL, curr_mask = 0UL;

	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);
	UNUSED_ARGUMENT(data);
	module = tstr_array_get_str_quick(params, 0);
	bail_null(module);

	err = get_current_mask(&mask);
	bail_error(err);

	curr_mask = mask;

	for (i = 0; 
		i < (sizeof(cli_errorlog_module_opts)/sizeof(cli_expansion_option)); i++) 
	{
		if (safe_strcmp(cli_errorlog_module_opts[i].ceo_option, module) == 0) 
		{
			mask |= ((uint64_t) cli_errorlog_module_opts[i].ceo_data); 
			break;
		}
	}
	
	err = set_new_mask(mask);
	bail_error(err);
bail:
	return err;
}



static int
cli_errorlog_module_unset(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *module = NULL;
	unsigned int i;
	uint64_t mask = 0UL, curr_mask = 0UL;

	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);
	UNUSED_ARGUMENT(data);

	module = tstr_array_get_str_quick(params, 0);
	bail_null(module);

	err = get_current_mask(&mask);
	bail_error(err);

	curr_mask = mask;

	for (i = 0; 
		i < (sizeof(cli_errorlog_module_opts)/sizeof(cli_expansion_option)); i++) 
	{
		if (safe_strcmp(cli_errorlog_module_opts[i].ceo_option, module) == 0) 
		{
			mask &= ~((uint64_t) cli_errorlog_module_opts[i].ceo_data); 
			break;
		}
	}

	// Some exclusions - 
	// if both http and rtsp was set, but we unset either one, 
	// then the dependent flags for network also get unset. 
	// So set these back conditionally.
	if ((curr_mask & celm_http) && (safe_strcmp(module, "rtsp") == 0)) {
		mask |= (celm_http | celm_httphdrs | celm_om | 
			celm_oomgr);
	}

	if ((curr_mask & celm_rtsp) && (safe_strcmp(module, "http") == 0)) {
		mask |= (celm_rtsp | celm_httphdrs | celm_om | 
			celm_oomgr);
	}

	err = set_new_mask(mask);
	bail_error(err);

bail:
	return err;
}

static int
cli_nkn_errorlog_remote_config(void *data,
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

	err = cli_nkn_errorlog_parse_remote_addr(addr, &ip, &port);
	bail_error(err);


	err = bn_set_request_msg_create(&req, 0);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding(req, bsso_modify, 0,
			"/nkn/errorlog/config/remote/ip",
			bt_ipv4addr,
			0,
			ip, NULL);
	bail_error(err);

	if (port != NULL) {
		err = bn_set_request_msg_append_new_binding(req, bsso_modify, 0,
				"/nkn/errorlog/config/remote/port",
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
					"/nkn/errorlog/config/remote/interface",
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
cli_nkn_errorlog_parse_remote_addr(const char *addr,
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
