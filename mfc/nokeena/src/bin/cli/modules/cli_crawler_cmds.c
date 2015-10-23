/*
 * Filename:    cli_crawler_cmds.c
 * Date:        2011/12/20
 * Author:      Varsha Rajagopalan
 *
 * (C) Copyright 2011,  Juniper Networks Inc.
 *  All rights reserved.
 */

#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "cli_module.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

#define CRAWLER_PREFIX "crawler * "
#define MAX_STATUS_STR_SIZE 50

#define CONSUME_CRAWLER_NODES(c) \
    {\
        err = tstr_array_append_sprintf(ret_reply->crr_nodes, c, name);\
        bail_error(err);\
    }

enum {
	cc_magic_autogen_add = 1,
	cc_magic_autogen_del,
	cc_magic_autogen_add_expiry,
	cc_magic_autogen_del_expiry,
};

enum {
	cc_magic_client_domain_add = 1,
	cc_magic_client_domain_del,
};

int 
cli_crawler_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


int
cli_crawler_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


static int
cli_crawler_get_names(
        tstr_array **ret_names,
        tbool hide_display);

static int 
cli_crawler_validate(
        const char *name,
        tbool *ret_valid,
        int *ret_error);
#if 0
int
cli_crawler_name_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context);
#endif

int
cli_crawler_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int
cli_crawler_show_list(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_crawler_set_scheduler(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_crawler_autogen_exec_cb(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

static int
cli_crawler_revmap(void *data, const cli_command *cmd,
	const bn_binding_array *bindings, const char *name,
	const tstr_array *name_parts, const char *value,
	const bn_binding *binding,
	cli_revmap_reply *ret_reply);

int cli_crawler_preloaded_extn(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data);

int cli_crawler_non_preloaded_extn(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data);

int cli_crawler_auto_gen_src(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data);

int validate_time(const char *date, const char *c_time);
/*---------------------------------------------------------------------------*/
int 
cli_crawler_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }


    CLI_CMD_NEW;
    cmd->cc_words_str =         "crawler";
    cmd->cc_help_descr =        N_("Configure crawler options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no crawler";
    cmd->cc_help_descr =        N_("Negate/Clear/Reset crawler options");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "crawler *";
    cmd->cc_help_exp =          N_("<crawler_name>");
    cmd->cc_help_exp_hint =     
        N_("Choose from following list or enter new name for crawler");
    cmd->cc_help_term_prefix =  
        N_("Configure or change the parameters of existing crawler");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/crawler/config/profile/*";
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_crawler_enter_prefix_mode;
    cmd->cc_revmap_order =      cro_crawler;
    cmd->cc_revmap_names =      "/nkn/crawler/config/profile/*";
    cmd->cc_revmap_callback =   cli_crawler_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no crawler *";
    cmd->cc_help_exp =          N_("<crawler_name>");
    cmd->cc_help_exp_hint =     N_("Choose from following list");
    cmd->cc_help_term =         N_("Delete crawler instance from configured database");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/crawler/config/profile/*";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_delete;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;

    /*-----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX"base-url";
    cmd->cc_help_descr =        N_("The origin server from where the content has to be downloaded.");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no";
    cmd->cc_help_descr =        N_("Reset the crawler configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "base-url *";
    cmd->cc_help_exp =          N_("<complete-url>");
    cmd->cc_help_exp_hint =     N_("URL to access the origin-server content");
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/url";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "schedule";
    cmd->cc_help_descr =        N_("Set the start/stop time and refresh interval for a crawler");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "schedule start";
    cmd->cc_help_descr =        N_("Time at which the crawler profile has to be started");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "schedule start *";
    cmd->cc_help_exp =          N_("<yyyy/mm/dd>");
    cmd->cc_help_exp_hint =     N_("date");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "schedule start * *";
    cmd->cc_help_exp =          N_("<hh:mm:ss>");
    cmd->cc_help_exp_hint =     N_("time");
    cmd->cc_flags =             ccf_terminal| ccf_var_args;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/schedule/start";
    cmd->cc_exec_callback =     cli_crawler_set_scheduler;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "schedule start * * | stop";
    cmd->cc_help_descr =        N_("Time at which the crawler profile has to be stopped");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "schedule start * * | stop * ";
    cmd->cc_help_exp =          N_("<yyyy/mm/dd>");
    cmd->cc_help_exp_hint =     N_("date");
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "schedule start * * | stop * *";
    cmd->cc_help_exp =          N_("<hh:mm:ss>");
    cmd->cc_help_exp_hint =     N_("time");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "schedule start * * | refresh-interval";
    cmd->cc_help_descr =        N_("Time interval at which the crawler profile has to refresh the content");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "schedule start * * | refresh-interval *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("minutes");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "link-depth";
    cmd->cc_help_descr =        N_("Set the number of link levels to be recursively crawled relative to base url");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "link-depth *";
    cmd->cc_help_exp =          N_("<0-10>");
    cmd->cc_help_exp_hint =     N_("Depth level. (default value: 10, 0 level indicates base object download)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/link_depth";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint8;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no link-depth";
    cmd->cc_help_descr =        N_("Reset the number of link levels to be recursively crawled relative to base url");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/link_depth";
    cmd->cc_exec_type =         bt_uint8;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "accept-file-extension";
    cmd->cc_help_descr =        N_("Set the file type to be downloaded from origin");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "accept-file-extension *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("File type extension.(Ex: .wmv).");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/file_extension/$2$";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "accept-file-extension * skip-preload";
    cmd->cc_help_descr =        N_("Objects in this namespace which match the extension are not preloaded");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/file_extension/$2$/skip_preload";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no accept-file-extension";
    cmd->cc_help_descr =        N_("Remove the file type extension");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no accept-file-extension *";
    cmd->cc_help_exp =          N_("<File extension to be removed>");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/crawler/config/profile/$1$/file_extension/*";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_delete;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/file_extension/$2$";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no accept-file-extension * skip-preload";
    cmd->cc_help_descr =        N_("Remove the skip-preload configuration");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/file_extension/$2$/skip_preload";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "action";
    cmd->cc_help_descr =        N_("Perform a configurable action during crawling");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "action set";
    cmd->cc_help_descr =        N_("Perform a configurable action during crawling");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "action set expiry";
    cmd->cc_help_descr =        N_("Choose the expiry mechanism for the objects crawled by the crawler instance");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "action set expiry origin-response";
    cmd->cc_help_descr =        N_("Set the expiry time based on the cache-control: max-age HTTP header/Age headers sent the parent MFC");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/origin_response_expiry";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no action set";
    cmd->cc_help_descr =        N_("Perform a configurable action during crawling");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no action set expiry";
    cmd->cc_help_descr =        N_("Choose the expiry mechanism for the objects crawled by the crawler instance");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no action set expiry origin-response";
    cmd->cc_help_descr =        N_("Reset the expiry time based on the cache-control: max-age HTTP header/Age headers sent the parent MFC");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/origin_response_expiry";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "action auto-generate";
    cmd->cc_help_descr =        N_("Perform a auto-generate action");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "action auto-generate *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Destination type to be used for auto generation");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "action auto-generate * source-file";
    cmd->cc_help_descr =        N_("Source file to perform a auto-generate action");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "action auto-generate * source-file *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Source Object to be used for auto generation");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_magic =				cc_magic_autogen_add;
	cmd->cc_exec_callback =		cli_crawler_autogen_exec_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no action";
    cmd->cc_help_descr =        N_("Remove the configured action during crawling");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no action auto-generate";
    cmd->cc_help_descr =        N_("Remove the configured auto-generate action");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no action auto-generate *";
    cmd->cc_help_exp =        	N_("Undo the auto-generate action config");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/crawler/config/profile/$1$/auto_generate_dest/*";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no action auto-generate * source-file";
    cmd->cc_help_descr =        N_("Source file to remove the configured auto-generate action");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no action auto-generate * source-file *";
    cmd->cc_help_exp =    		N_("Undo config of Source Object used for auto generation");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/crawler/config/profile/$1$/auto_generate_src/*";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_magic =				cc_magic_autogen_del;
	cmd->cc_exec_callback =		cli_crawler_autogen_exec_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "action auto-generate * source-file * expiry";
    cmd->cc_help_descr =        N_("For every instance of an object matching the source file extension,"
    		" generate a new object with the configured expiry");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "action auto-generate * source-file * expiry *";
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_help_exp_hint =     N_("Expiry Value (min: 300, max:3153600000)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_magic =				cc_magic_autogen_add_expiry;
	cmd->cc_exec_callback =		cli_crawler_autogen_exec_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         CRAWLER_PREFIX "no action auto-generate * source-file * expiry";
    cmd->cc_help_descr =    	N_("Reset the expiry value configured");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_magic =				cc_magic_autogen_del_expiry;
	cmd->cc_exec_callback =		cli_crawler_autogen_exec_cb;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	CRAWLER_PREFIX "action x-domain";
    cmd->cc_help_descr =	N_("Setup cross domain fetch");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	CRAWLER_PREFIX "no action x-domain";
    cmd->cc_help_descr =	N_("Setup cross domain fetch");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		CRAWLER_PREFIX "no action x-domain crawl";
    cmd->cc_help_descr =        N_("Disable cross domain fetches");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/xdomain";
    cmd->cc_exec_value =        "false";//active value is 1
    cmd->cc_exec_type =         bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		CRAWLER_PREFIX "action x-domain crawl";
    cmd->cc_help_descr =        N_("Enable cross domain fetches");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/xdomain";
    cmd->cc_exec_value =        "true";//active value is 1
    cmd->cc_exec_type =         bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"crawler * status";
    cmd->cc_help_descr =	N_("Set the status of the crawler instance to active or inactive");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 		"crawler * status active";
    cmd->cc_help_descr =        N_("Activate Crawler instance");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/status";
    cmd->cc_exec_value =        "1";//active value is 1
    cmd->cc_exec_type =         bt_uint8;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "crawler * status inactive";
    cmd->cc_help_descr =        N_("Deactivate Crawler instance");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/crawler/config/profile/$1$/status";
    cmd->cc_exec_type =         bt_uint8;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show crawler";
    cmd->cc_help_descr =        N_("Display crawler configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show crawler list";
    cmd->cc_help_descr =         N_("Display the configured crawler instances");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_crawler_show_list;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show crawler *";
    cmd->cc_help_exp =          N_("<crawler name>");
    cmd->cc_help_exp_hint =     "";
    cmd->cc_help_term = 	N_("Display the configuration and status of a particular crawler instance");
    cmd->cc_comp_type = 	cct_matching_names;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_pattern = 	"/nkn/crawler/config/profile/*";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_crawler_show_config;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;


bail:
    return err;
}

int
cli_crawler_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{

    int err = 0;
    int ret_err = 0;
    tbool valid = true;
    const char *c_crawler = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(params);

    c_crawler = tstr_array_get_str_quick(params, 0);
    bail_null(c_crawler);

    if (cli_prefix_modes_is_enabled()) {
        err = cli_crawler_validate(c_crawler, &valid, &ret_err);
        bail_error(err);

        if (!valid ) {

            /* mandate the "complete url" is set for the crawler profile 
             */
            cli_printf_error(_("Crawler instance '%s' doesn't exist.\n Create it first with the "
                        "'crawler <crawler_name> base-url <complete-url>'command."), c_crawler);
            goto bail;
        }

        if (valid) {
            err = cli_prefix_enter_mode(cmd, cmd_line);
            bail_error(err);
        }
    }
    else {
        err = cli_print_incomplete_command_error(cmd, cmd_line);
        bail_error(err);
    }
bail:
    return err;
}

/*
 * Check whether the player already exists or not.
 * Only validates the player name.
 */
static int 
cli_crawler_validate(
        const char *name,
        tbool *ret_valid,
        int *ret_error)
{
    int err = 0;
    bn_binding *binding = NULL;
    node_name_t bn_name = {0};

    bail_null(ret_valid);
    *ret_valid = false;

    /*
     err = cli_crawler_get_names(&t_names, false);
    bail_error(err);
    */
    
    snprintf(bn_name, sizeof(bn_name),"/nkn/crawler/config/profile/%s", name);
    bail_null(bn_name);
    err = mdc_get_binding(cli_mcc, NULL, NULL,
	    false, &binding, bn_name, NULL);
    bail_error(err);


    if (binding) {
        *ret_valid = true;
    }

    if (NULL != ret_error)
        *ret_error = err;

    err = 0;
bail:
    bn_binding_free(&binding);
    return err;
}

/*
 * Get list of crawler players that have been configured in the
 * database
 */
static int
cli_crawler_get_names(
        tstr_array **ret_names,
        tbool hide_display)
{
    int err = 0;
    tstr_array *t_names = NULL;
    tstr_array_options opts;
    tstr_array *t_names_config = NULL;
    uint32 ret_err = 0;
    tstring *t_ret_msg = NULL;

    UNUSED_ARGUMENT(hide_display);

    bail_null(ret_names);
    *ret_names = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&t_names, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            &ret_err, &t_ret_msg, 
            &t_names_config,
            "/nkn/crawler/config/profile", NULL);
    bail_error_null(err, t_names_config);

    err = tstr_array_concat(t_names, &t_names_config);
    bail_error(err);

    *ret_names = t_names;
    t_names = NULL;

bail:
    ts_free(&t_ret_msg);
    tstr_array_free(&t_names);
    tstr_array_free(&t_names_config);
    return err;
}

int
cli_crawler_set_scheduler(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *crawl = NULL;
    const char *start_time = NULL;
    const char *start_day = NULL;
    const char *stop_time = NULL;
    const char *stop_day = NULL;
    const char *refresh = NULL;
    uint32 idx = 0;
    bn_request *req = NULL;
    str_value_t daytime = {0};
    str_value_t stoptime = {0};

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(data);


    crawl = tstr_array_get_str_quick(params, 0);

    start_day = tstr_array_get_str_quick(params, 1);
    start_time = tstr_array_get_str_quick(params, 2);
    if(!crawl || !start_time || !start_day){
	goto bail;
    }

    err = validate_time(start_day, start_time);
    bail_error(err);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    snprintf(daytime, sizeof(daytime),"%s %s", start_day, start_time);
    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0, 
            bt_datetime_sec, 0, daytime, NULL, "/nkn/crawler/config/profile/%s/schedule/start", crawl);
    bail_error(err);
    /* Check if stop time is configured*/
    err = tstr_array_linear_search_str(cmd_line, "stop", 0, &idx);
    if (err != lc_err_not_found && err == 0) {
	stop_day = tstr_array_get_str_quick(cmd_line, idx+1);
	bail_null(stop_day);
	stop_time = tstr_array_get_str_quick(cmd_line, idx+2);
	bail_null(stop_time);

	err = validate_time(stop_day, stop_time);
	bail_error(err);

	snprintf(stoptime, sizeof(stoptime),"%s %s", stop_day, stop_time);
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0, 
		bt_datetime_sec, 0, stoptime, NULL, "/nkn/crawler/config/profile/%s/schedule/stop", crawl);
	bail_error(err);
    }
    /* Check if refresh interval is configured*/
    err = tstr_array_linear_search_str(cmd_line, "refresh-interval", 0, &idx);
    if (err != lc_err_not_found && err == 0) {
	int rt = 0;
	refresh = tstr_array_get_str_quick(cmd_line, idx+1);
	bail_null(refresh);
	rt = atoi(refresh);
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0, 
		bt_uint32, 0, refresh, NULL, "/nkn/crawler/config/profile/%s/schedule/refresh", crawl);
	bail_error(err);
    }
    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);
bail:
    bn_request_msg_free(&req);
    return err;

}


int
cli_crawler_show_list(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 idx = 0, num_names = 0;
    tstr_array *names = NULL;
    uint8 t_status;
    node_name_t bn_name = {0};
    bn_binding *binding = NULL;
    uint8 crawling_status = 0;
    node_name_t bn_name_status = {0};
    bn_binding *binding_status = NULL;
    char crawl_status[MAX_STATUS_STR_SIZE] = {0};
    uint32_t irefresh_interval;
    uint64_t next_trigger_time;
    lt_time_sec stop_time = 0;
    char *next_trigger_str = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_crawler_get_names(&names, true);
    bail_error_null(err, names);

    num_names = tstr_array_length_quick(names);

    if (num_names > 0) {
        err = cli_printf(_("Configured Crawler instances :\n"));
        bail_error(err);
	err = cli_printf_query(_
                ("    %16s : %s\n\n"),
                        "Name", "Status");
   	for (idx = 0 ; idx < num_names; idx++) {
	    snprintf(bn_name, sizeof(bn_name), "/nkn/crawler/config/profile/%s/status",
		    tstr_array_get_str_quick(names, idx));

	    err = mdc_get_binding(cli_mcc, NULL, NULL,
		    false, &binding, bn_name, NULL);
	    bail_error(err);

	    if (binding){
	    	err = bn_binding_get_uint8(binding, ba_value, NULL, &t_status);
	    	bail_error(err);
	    }

	    snprintf(bn_name, sizeof(bn_name), "/nkn/crawler/monitor/profile/external/%s/next_trigger_time", 
		    tstr_array_get_str_quick(names, idx));
	    err = mdc_get_binding(cli_mcc, NULL, NULL,
		    false, &binding, bn_name, NULL);
	    bail_error(err);
	    if(binding) {
		err = bn_binding_get_uint64(binding, ba_value, NULL, &next_trigger_time);
		bail_error(err);
	    }

	    snprintf(bn_name, sizeof(bn_name), "/nkn/crawler/monitor/profile/external/%s/end_ts",
		    tstr_array_get_str_quick(names, idx));
	    err = mdc_get_binding(cli_mcc, NULL, NULL,
		    false, &binding, bn_name, NULL);
	    bail_error(err);

	    if(binding) {
		err = bn_binding_get_datetime_sec(binding, ba_value, NULL, &stop_time);
		bail_error(err);
	    }

	    snprintf(bn_name, sizeof(bn_name), "/nkn/crawler/config/profile/%s/schedule/refresh",
		    tstr_array_get_str_quick(names, idx));
	    err = mdc_get_binding(cli_mcc, NULL, NULL,
		    false, &binding, bn_name, NULL);
	    bail_error(err);
	    if(binding) {
		err = bn_binding_get_uint32(binding, ba_value, NULL, &irefresh_interval);
		bail_error(err);
	    }

	    snprintf(bn_name_status, sizeof(bn_name_status), "/nkn/crawler/monitor/profile/external/%s/status",
	    		tstr_array_get_str_quick(names, idx));

	    err = mdc_get_binding(cli_mcc, NULL, NULL,
		    false, &binding_status, bn_name_status, NULL);
	    bail_error(err);

	    if(binding_status ) {
	    	err = bn_binding_get_uint8(binding_status, ba_value, NULL, &crawling_status);
	    	bail_error(err);

	    	if(crawling_status == 0) {
		    if (irefresh_interval == 0) {
		        if (stop_time > 0){
			   strncpy(crawl_status, "CRAWL_COMPLETED", MAX_STATUS_STR_SIZE);
		        } else {
			    if (next_trigger_time > 0){
			        lt_datetime_sec_to_str(next_trigger_time, true, &next_trigger_str);
			        bail_error(err);
			        *(next_trigger_str + 20) = '\0';
			        snprintf(crawl_status,MAX_STATUS_STR_SIZE,
					"Next crawl time: %s", next_trigger_str);
			    } else{
			        strncpy(crawl_status,"Next crawl time: None",
			    	    MAX_STATUS_STR_SIZE);
			        bail_error(err);
			    }
		        }
		    } else {
			if (next_trigger_time > 0){
			    lt_datetime_sec_to_str(next_trigger_time, true, &next_trigger_str);
			    bail_error(err);
			    *(next_trigger_str + 20) = '\0';
			    snprintf(crawl_status,MAX_STATUS_STR_SIZE,
					"Next crawl time: %s", next_trigger_str);
		         } else{
			    strncpy(crawl_status,"Next crawl time: None",
			    	    MAX_STATUS_STR_SIZE);
			    bail_error(err);
		         } 
		    }
		}
	    	else if((crawling_status == 1) || (crawling_status == 2))
				strncpy(crawl_status, "CRAWL_IN_PROGRESS", MAX_STATUS_STR_SIZE);
	    	else if((crawling_status == 3) || (crawling_status == 4))
				strncpy(crawl_status, "CRAWL_STOPPED", MAX_STATUS_STR_SIZE);
	    	bail_error(err);
	    }


        err = cli_printf_query(_
                    ("    %16s : %s (%s)\n"),
                            tstr_array_get_str_quick(names, idx),
                            t_status ? "Active" : "Inactive", crawl_status);
        bail_error(err);
  	}
    }
    else {
	err = cli_printf_query(_("There are no configured Crawler instances\n"));
	bail_error(err);
    }
    
bail:
    tstr_array_free(&names);
    bn_binding_free(&binding);
    return err;
}

static int
cli_crawler_revmap(void *data, const cli_command *cmd,
               const bn_binding_array *bindings, const char *name,
               const tstr_array *name_parts, const char *value,
               const bn_binding *binding,
               cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL;
    const char *crawl = value; //crawler instance name
    tstring *url = NULL;
    tstring *ldepth = NULL;
    tstring *tstart = NULL;
    tstring *tstop = NULL;
    tstring *trefresh = NULL;
    uint8 istat = 0;
    tbool t_xdomain = 0;
    const char *status[] = {"inactive","active"};
    const bn_binding *cbind = NULL;
    const bn_binding *expiry_bind = NULL;
    tstr_array *match_name_parts = NULL;
    tstr_array *src_match_name_parts = NULL;
    uint32 num_matches = 0;
    uint32 src_num_matches = 0;
    uint32 idx = 0;
    uint64 auto_gen_expiry =0;
    const char *file_ext = NULL;
    node_name_t node_name = {0};
    char* skip_node_name = NULL;
    node_name_t dest_node_name = {0};
    node_name_t src_node_name = {0};
    node_name_t origin_expiry_node_name = {0};
    tbool skip_preload = false;
    tbool origin_expiry = false;
    const char *dest_file = NULL;
    const char *src_file = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(ret_reply);

    /* complete-url*/
    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	                NULL, &url, "%s/url", name);
    bail_error(err);

   	err = ts_new_sprintf(&rev_cmd, "crawler %s base-url %s \n", crawl, ts_str(url));
   	bail_error(err);

    /*link-depth*/
    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	                NULL, &ldepth, "%s/link_depth", name);
    bail_error(err);
    err = ts_append_sprintf(rev_cmd, "   crawler %s link-depth %s\n", crawl, ts_str(ldepth));
    bail_error(err);

    /*Schedule start*/
    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
	                NULL, &tstart, "%s/schedule/start", name);
    bail_error(err);
    if(tstart && !(ts_equal_str(tstart, "1970/01/01 00:00:00",false))){
	err = ts_append_sprintf(rev_cmd, "   crawler %s schedule start %s ", crawl, ts_str(tstart));
	bail_error(err);
	/*Schedule stop - optional*/
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &tstop, "%s/schedule/stop", name);
	bail_error(err);
	if(tstop && !(ts_equal_str(tstop, "1970/01/01 00:00:00",false))) {
	    err = ts_append_sprintf(rev_cmd, "stop %s ", ts_str(tstop));
	    bail_error(err);
	}
	/*Refresh interval -optional*/
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &trefresh, "%s/schedule/refresh", name);
	bail_error(err);
	if(!(ts_equal_str(trefresh, "0", false))) {
	    err = ts_append_sprintf(rev_cmd, "refresh-interval %s ", ts_str(trefresh));
	    bail_error(err);
	}
	err = ts_append_sprintf(rev_cmd, "\n");
    }
    /*File extensions*/
    snprintf(node_name, sizeof(node_name),"%s/file_extension/*", name);
    err = bn_binding_array_get_name_part_tstr_array(bindings, node_name, 6, &match_name_parts);
    bail_error(err);
    if (match_name_parts) {
	num_matches = tstr_array_length_quick(match_name_parts);
	for (idx = 0; idx < num_matches; idx++) {
	    file_ext = tstr_array_get_str_quick(match_name_parts, idx);
	    bail_null(file_ext);

		skip_node_name = smprintf("%s/file_extension/%s/skip_preload", name, file_ext);
		bail_null(skip_node_name);

		err = bn_binding_array_get_value_tbool_by_name(bindings, skip_node_name, NULL, &skip_preload);
		bail_error(err);
		safe_free(skip_node_name);

		if( skip_preload ) {
		    err = ts_append_sprintf(rev_cmd, "   crawler %s accept-file-extension %s skip-preload\n",
			    crawl, file_ext);
		    bail_error(err);
		}else{
	    err = ts_append_sprintf(rev_cmd, "   crawler %s accept-file-extension %s\n",
		    crawl, file_ext);
	    bail_error(err);
		}
#define CONSUME_FILEEXT_NODES(c) \
            {\
                err = tstr_array_append_sprintf(ret_reply->crr_nodes, c, name, file_ext);\
                bail_error(err);\
            }
	    CONSUME_FILEEXT_NODES("%s/file_extension/%s");
	    CONSUME_FILEEXT_NODES("%s/file_extension/%s/skip_preload");
#undef	CONSUME_FILEEXT_NODES
	}
    }
    /*Auto Generate*/

    snprintf(dest_node_name, sizeof(dest_node_name),"%s/auto_generate_dest/*", name);
    err = bn_binding_array_get_name_part_tstr_array(bindings, dest_node_name, 6, &match_name_parts);
    bail_error(err);

    if (match_name_parts)
	num_matches = tstr_array_length_quick(match_name_parts);

    snprintf(src_node_name, sizeof(src_node_name),"%s/auto_generate_src/*", name);
    err = bn_binding_array_get_name_part_tstr_array(bindings, src_node_name, 6, &src_match_name_parts);
    bail_error(err);

    if (src_match_name_parts)
	src_num_matches = tstr_array_length_quick(src_match_name_parts);

	for (idx = 0; idx < src_num_matches; idx++) {
	    dest_file = tstr_array_get_str_quick(match_name_parts, idx);
	    bail_null(dest_file);

	    src_file = tstr_array_get_str_quick(src_match_name_parts, idx);
	    bail_null(src_file);

	    err = bn_binding_array_get_binding_by_name_fmt(bindings, &expiry_bind, "%s/auto_generate_src/%s/auto_gen_expiry", name, src_file);
	    bail_error(err);
	    err = bn_binding_get_uint64(expiry_bind, ba_value, NULL, &auto_gen_expiry);
	    bail_error(err);

	    if(auto_gen_expiry == 315360000) {
	    	err = ts_append_sprintf(rev_cmd, "   crawler %s action auto-generate %s source-file %s\n",
	    			crawl, dest_file, src_file);
	    	bail_error(err);
	    }else{
	    	err = ts_append_sprintf(rev_cmd, "   crawler %s action auto-generate %s source-file %s expiry %ld\n",
	    			crawl, dest_file, src_file, auto_gen_expiry);
	    	bail_error(err);
	    }

#define CONSUME_FILEEXT_NODES(c) \
            {\
                err = tstr_array_append_sprintf(ret_reply->crr_nodes, c, name, dest_file);\
                bail_error(err);\
            }
	    CONSUME_FILEEXT_NODES("%s/auto_generate_dest/%s");
#undef	CONSUME_FILEEXT_NODES

#define CONSUME_FILEEXT_NODES(c) \
            {\
                err = tstr_array_append_sprintf(ret_reply->crr_nodes, c, name, src_file);\
                bail_error(err);\
            }
	    CONSUME_FILEEXT_NODES("%s/auto_generate_src/%s");
	    CONSUME_FILEEXT_NODES("%s/auto_generate_src/%s/auto_gen_expiry");
#undef	CONSUME_FILEEXT_NODES
	}

	snprintf(origin_expiry_node_name, sizeof(origin_expiry_node_name),"%s/origin_response_expiry", name);
	bail_null(origin_expiry_node_name);

	err = bn_binding_array_get_value_tbool_by_name(bindings, origin_expiry_node_name, NULL, &origin_expiry);
	bail_error(err);

	if( origin_expiry ) {
	    err = ts_append_sprintf(rev_cmd, "   crawler %s action set expiry origin-response\n", crawl);
	    bail_error(err);
	}

	err = bn_binding_array_get_binding_by_name_fmt(bindings, &cbind, "%s/xdomain", name);
	bail_error(err);
	err = bn_binding_get_tbool(cbind, ba_value, NULL, &t_xdomain);
	bail_error(err);

	if( t_xdomain ) {
	    err = ts_append_sprintf(rev_cmd, "   crawler %s action x-domain crawl\n", crawl);
	    bail_error(err);
	} else {
	    err = ts_append_sprintf(rev_cmd, "no crawler %s action x-domain crawl\n", crawl);
	    bail_error(err);
	}

   /*Status*/
    err = bn_binding_array_get_binding_by_name_fmt(bindings, &cbind, "%s/status", name);
    bail_error(err);
    err = bn_binding_get_uint8(cbind, ba_value, NULL, &istat);
    bail_error(err);
    err = ts_append_sprintf(rev_cmd, "   crawler %s status %s\n", crawl, status[istat]);
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

    CONSUME_CRAWLER_NODES("%s/xdomain");
    CONSUME_CRAWLER_NODES("%s/schedule/refresh");
    CONSUME_CRAWLER_NODES("%s/schedule/start");
    CONSUME_CRAWLER_NODES("%s/schedule/stop");
    CONSUME_CRAWLER_NODES("%s/link_depth");
    CONSUME_CRAWLER_NODES("%s/url");
    CONSUME_CRAWLER_NODES("%s/auto_generate");
    CONSUME_CRAWLER_NODES("%s/origin_response_expiry");
    CONSUME_CRAWLER_NODES("%s/status");
    CONSUME_CRAWLER_NODES("%s");

bail:
    ts_free(&url);
    ts_free(&ldepth);
    ts_free(&tstart);
    ts_free(&tstop);
    ts_free(&trefresh);
    tstr_array_free(&match_name_parts);
    return err;
}

int
cli_crawler_show_config(
	void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params)
{
    int err = 0;
    const char *crawl_name = NULL;
    const char *file_extn_node = NULL;
    bn_binding *binding = NULL;
    node_name_t bn_name = {0};
    node_name_t auto_gen_src_node = {0};
    uint8 istat = 0;
    uint8 iautogen = 0;
    uint8 crawling_status = 0;
    uint32 num_file_extns = 0;
    uint32_t irefresh_interval;
    uint64_t next_trigger_time;
    lt_time_sec start_time = 0;
    lt_time_sec stop_time = 0;
    char *start_str = NULL;
    char *stop_str = NULL;
    char *next_trigger_str = NULL;
    tbool t_xdomain;
   
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    crawl_name = tstr_array_get_str_quick(params, 0);
    bail_null(crawl_name);

    snprintf(bn_name, sizeof(bn_name),"/nkn/crawler/config/profile/%s", crawl_name);
    bail_null(bn_name);

    err = mdc_get_binding(cli_mcc, NULL, NULL,
	    false, &binding, bn_name, NULL);
    bail_error(err);

    if (NULL == binding ){
	cli_printf_error(_("Invalid crawler instance : %s\n"), crawl_name);
  	goto bail;
    }
   
    err = cli_printf_query(_("Crawler instance: "
                        "#/nkn/crawler/config/profile/%s#\n"), crawl_name);
    bail_error(err);

    err = cli_printf_query(_("Base URL: "
                        "#/nkn/crawler/config/profile/%s/url#\n"), crawl_name);
    bail_error(err);

    err = cli_printf_query(_("Depth: "
                        "#/nkn/crawler/config/profile/%s/link_depth#\n"), crawl_name);
    bail_error(err);

    err = cli_printf_query(_("Refresh interval: "
                        "#/nkn/crawler/config/profile/%s/schedule/refresh#\n"), crawl_name);
    bail_error(err);


    snprintf(bn_name, sizeof(bn_name), "/nkn/crawler/config/profile/%s/status", crawl_name);
    err = mdc_get_binding(cli_mcc, NULL, NULL,
	    false, &binding, bn_name, NULL);
    bail_error(err);
    if(binding) {
	err = bn_binding_get_uint8(binding, ba_value, NULL, &istat);
	bail_error(err);
	err = cli_printf(_("Status: %s\n"), istat?"active":"inactive");
	bail_error(err);
    }

    snprintf(bn_name, sizeof(bn_name), "/nkn/crawler/config/profile/%s/xdomain",
	     crawl_name);

    err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding, bn_name, NULL);
    bail_error(err);

    if (binding){
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_xdomain);
	bail_error(err);
	err = cli_printf(_("X-Domain Fetch: %s\n"),
			 t_xdomain?"Enabled":"Disabled");
	bail_error(err);
    }

	cli_printf(_("  Preloaded file extensions: \n"));

	file_extn_node = smprintf("/nkn/crawler/config/profile/%s/file_extension/*", crawl_name);
	bail_null(file_extn_node);

	err = mdc_foreach_binding(cli_mcc, file_extn_node, NULL, cli_crawler_preloaded_extn,
	    NULL, &num_file_extns);
		bail_error(err);

	if (num_file_extns == 0)
	    cli_printf(_("   NONE\n"));

	cli_printf(_("  Non-preloaded file extensions: \n"));

	err = mdc_foreach_binding(cli_mcc, file_extn_node, NULL, cli_crawler_non_preloaded_extn,
	    NULL, &num_file_extns);
		bail_error(err);

	if (num_file_extns == 0)
	    cli_printf(_("   NONE\n"));

    snprintf(bn_name, sizeof(bn_name), "/nkn/crawler/config/profile/%s/auto_generate", crawl_name);
    err = mdc_get_binding(cli_mcc, NULL, NULL,
	    false, &binding, bn_name, NULL);
    bail_error(err);
    if(binding) {
	err = bn_binding_get_uint8(binding, ba_value, NULL, &iautogen);
	bail_error(err);
	err = cli_printf(_("Auto-generate: %s\n"), iautogen?"ASX from source-file WMV":"None");
	bail_error(err);
    }

    snprintf(auto_gen_src_node, sizeof(auto_gen_src_node), "/nkn/crawler/config/profile/%s/auto_generate_src/*", crawl_name);

	err = mdc_foreach_binding(cli_mcc, auto_gen_src_node, NULL, cli_crawler_auto_gen_src,
	    NULL, NULL);
		bail_error(err);

    err = cli_printf_query(_("Expiry time based on the cache-control Enabled: "
                       "#/nkn/crawler/config/profile/%s/origin_response_expiry#\n"), crawl_name);
    bail_error(err);

	err = cli_printf_query(_("Scheduled start time: "
                        "#/nkn/crawler/config/profile/%s/schedule/start#\n"), crawl_name);
    bail_error(err);

    err = cli_printf_query(_("Scheduled stop time: "
                        "#/nkn/crawler/config/profile/%s/schedule/stop#\n"), crawl_name);
    bail_error(err);

    snprintf(bn_name, sizeof(bn_name), "/nkn/crawler/monitor/profile/external/%s/end_ts", crawl_name);

    err = mdc_get_binding(cli_mcc, NULL, NULL,
	    false, &binding, bn_name, NULL);
    bail_error(err);

    if(binding) {
    	err = bn_binding_get_datetime_sec(binding, ba_value, NULL, &stop_time);
		bail_error(err);
    }

    snprintf(bn_name, sizeof(bn_name), "/nkn/crawler/config/profile/%s/schedule/refresh", crawl_name);
    err = mdc_get_binding(cli_mcc, NULL, NULL,
	    false, &binding, bn_name, NULL);
    bail_error(err);
    if(binding) {
	err = bn_binding_get_uint32(binding, ba_value, NULL, &irefresh_interval);
	bail_error(err);
    }

    snprintf(bn_name, sizeof(bn_name), "/nkn/crawler/monitor/profile/external/%s/next_trigger_time", crawl_name);
    err = mdc_get_binding(cli_mcc, NULL, NULL,
	    false, &binding, bn_name, NULL);
    bail_error(err);
    if(binding) {
	err = bn_binding_get_uint64(binding, ba_value, NULL, &next_trigger_time);
	bail_error(err);
    }

    snprintf(bn_name, sizeof(bn_name), "/nkn/crawler/monitor/profile/external/%s/status", crawl_name);
    err = mdc_get_binding(cli_mcc, NULL, NULL,
	    false, &binding, bn_name, NULL);
    bail_error(err);
    if(binding) {
	err = bn_binding_get_uint8(binding, ba_value, NULL, &crawling_status);
	bail_error(err);
	if(crawling_status == 0) {
		if (irefresh_interval == 0) {
		    if (stop_time > 0){
			err = cli_printf(_("Crawling status: %s\n"), "CRAWL_COMPLETED");
		    } else {
			if (next_trigger_time > 0){
			    lt_datetime_sec_to_str(next_trigger_time, true, &next_trigger_str);
			    bail_error(err);
			    *(next_trigger_str + 20) = '\0';
			    err = cli_printf_query(_("Next crawl time: "
					"%s\n"), next_trigger_str);
			} else{
			    err = cli_printf(_("Next crawl time: None \n"));
			    bail_error(err);
			}
		    }
		} else {
		     if (next_trigger_time > 0){
			lt_datetime_sec_to_str(next_trigger_time, true, &next_trigger_str);
			bail_error(err);
			*(next_trigger_str + 20) = '\0';
			err = cli_printf_query(_("Next crawl time: "
						"%s\n"), next_trigger_str);
		     } else{
			err = cli_printf(_("Next crawl time: None \n"));
			bail_error(err);
		     } 
		}
	} else if((crawling_status == 1) || (crawling_status == 2)) {
		err = cli_printf(_("Crawling status: %s\n"), "CRAWL_IN_PROGRESS");
	} else if((crawling_status == 3) || (crawling_status == 4)) {
		err = cli_printf(_("Crawling status: %s\n"), "CRAWL_STOPPED");
	}
	bail_error(err);
    }


	cli_printf(_("  Last Crawl Details: \n"));

    snprintf(bn_name, sizeof(bn_name), "/nkn/crawler/monitor/profile/external/%s/start_ts", crawl_name);

    err = mdc_get_binding(cli_mcc, NULL, NULL,
	    false, &binding, bn_name, NULL);
    bail_error(err);

    if(binding) {
    	err = bn_binding_get_datetime_sec(binding, ba_value, NULL, &start_time);
    	bail_error(err);
    }

	if (start_time > 0){
		lt_datetime_sec_to_str(start_time, true, &start_str);
		bail_error(err);
		*(start_str + 20) = '\0';
		err = cli_printf(_("start timestamp: %s \n"), start_str);
		bail_error(err);
	}
	else{
		err = cli_printf(_("start timestamp: None \n"));
		bail_error(err);
	}


	if (stop_time > 0){
		lt_datetime_sec_to_str(stop_time, true, &stop_str);
		bail_error(err);
		*(stop_str + 20) = '\0';
		err = cli_printf(_("end timestamp: %s \n"), stop_str);
		bail_error(err);
	}
	else{
		err = cli_printf(_("end timestamp: None \n"));
		bail_error(err);
	}

    err = cli_printf_query(_("Number of objects added: #/nkn/crawler/monitor/profile/external/%s/num_objects_add#\n"), crawl_name);
    bail_error(err);

    err = cli_printf_query(_("Number of objects deleted: #/nkn/crawler/monitor/profile/external/%s/num_objects_del#\n"), crawl_name);
    bail_error(err);

    err = cli_printf_query(_("Number of adds failed: #/nkn/crawler/monitor/profile/external/%s/num_objects_add_fail#\n"), crawl_name);
    bail_error(err);

    err = cli_printf_query(_("Number of deletes failed: "
                        "#/nkn/crawler/monitor/profile/external/%s/num_objects_del_fail#\n"), crawl_name);
    bail_error(err);

bail:
    bn_binding_free(&binding);
    safe_free(start_str);
    safe_free(stop_str);
    return err;
}

int
cli_crawler_autogen_exec_cb(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *c_crawler = NULL;
	const char *c_dest_file = NULL;
	const char *c_src_file = NULL;
	const char *c_expiry = NULL;
    bn_request *req = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

    c_crawler = tstr_array_get_str_quick(params, 0);
	bail_null(c_crawler);

	c_dest_file = tstr_array_get_str_quick(params, 1);
	bail_null(c_dest_file);

	c_src_file = tstr_array_get_str_quick(params, 2);
	bail_null(c_src_file);

	c_expiry = tstr_array_get_str_quick(params, 3);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

	if (cmd->cc_magic == cc_magic_autogen_add){

		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_create, 0,
				bt_string, 0, c_dest_file, NULL, "/nkn/crawler/config/profile/%s/auto_generate_dest/%s", c_crawler, c_dest_file);
		bail_error(err);

		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_create, 0,
				bt_string, 0, c_src_file, NULL, "/nkn/crawler/config/profile/%s/auto_generate_src/%s", c_crawler, c_src_file);
		bail_error(err);

		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
				bt_uint8, 0, "1", NULL, "/nkn/crawler/config/profile/%s/auto_generate", c_crawler);
		bail_error(err);

	}
	if (cmd->cc_magic == cc_magic_autogen_add_expiry){

		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_create, 0,
				bt_string, 0, c_dest_file, NULL, "/nkn/crawler/config/profile/%s/auto_generate_dest/%s", c_crawler, c_dest_file);
		bail_error(err);

		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_create, 0,
				bt_string, 0, c_src_file, NULL, "/nkn/crawler/config/profile/%s/auto_generate_src/%s", c_crawler, c_src_file);
		bail_error(err);

		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
				bt_uint8, 0, "1", NULL, "/nkn/crawler/config/profile/%s/auto_generate", c_crawler);
		bail_error(err);

		err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
				bt_uint64, 0, c_expiry, NULL, "/nkn/crawler/config/profile/%s/auto_generate_src/%s/auto_gen_expiry", c_crawler, c_src_file);
		bail_error(err);
	}

	if (cmd->cc_magic == cc_magic_autogen_del){
		if ((!strcmp(c_dest_file, "asx")) && (!strcmp(c_src_file, "wmv"))){

			err = bn_set_request_msg_append_new_binding_fmt(req, bsso_delete, 0,
				bt_string, 0, c_dest_file, NULL, "/nkn/crawler/config/profile/%s/auto_generate_dest/%s", c_crawler, c_dest_file);
			bail_error(err);

			err = bn_set_request_msg_append_new_binding_fmt(req, bsso_delete, 0,
				bt_string, 0, c_src_file, NULL, "/nkn/crawler/config/profile/%s/auto_generate_src/%s", c_crawler, c_src_file);
			bail_error(err);

			err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
				bt_uint8, 0, "0", NULL, "/nkn/crawler/config/profile/%s/auto_generate", c_crawler);
			bail_error(err);
		}else{
	        err = cli_printf(_("Currently only ASX and WMV are supported for auto-generation"));
			bail_error(err);
		}

	}
	if (cmd->cc_magic == cc_magic_autogen_del_expiry){
		if ((!strcmp(c_dest_file, "asx")) && (!strcmp(c_src_file, "wmv"))){

			err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
					bt_uint64, 0, "315360000", NULL, "/nkn/crawler/config/profile/%s/auto_generate_src/%s/auto_gen_expiry", c_crawler, c_src_file);
			bail_error(err);

		}else{
	        err = cli_printf(_("Currently only ASX and WMV are supported for auto-generation"));
			bail_error(err);
		}

	}
	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);

bail:
	bn_request_msg_free(&req);
	return err;
}

int cli_crawler_non_preloaded_extn(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data)
{
    int err =0;
    tstring *file_extn = NULL;
    const char *crawler_ins = NULL;
    tstring *skip_preload = NULL;

    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    crawler_ins = tstr_array_get_str_quick(name_components, 4);

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &file_extn);
    bail_error(err);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		    &skip_preload, NULL,
		    "/nkn/crawler/config/profile/%s/file_extension/%s/skip_preload",
		    crawler_ins, ts_str(file_extn));
    bail_error(err);


    if (ts_equal_str(skip_preload, "true", true)) {
       err = cli_printf(
                _("    %s   \n"), ts_str(file_extn));
       bail_error(err);
    }

    bail:
	ts_free(&file_extn);
	return(err);
}

int cli_crawler_preloaded_extn(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data)
{
    int err =0;
    tstring *file_extn = NULL;
    const char *crawler_ins = NULL;
    tstring *skip_preload = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    crawler_ins = tstr_array_get_str_quick(name_components, 4);

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &file_extn);
    bail_error(err);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		    &skip_preload, NULL,
		    "/nkn/crawler/config/profile/%s/file_extension/%s/skip_preload",
		    crawler_ins, ts_str(file_extn));
    bail_error(err);

    if (ts_equal_str(skip_preload, "false", true)) {
       err = cli_printf(
                _("    %s   \n"), ts_str(file_extn));
       bail_error(err);
    }

    bail:
	ts_free(&file_extn);
	return(err);
}

int cli_crawler_auto_gen_src(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data)
{
    int err =0;
    tstring *auto_gen_src = NULL;
    const char *crawler_ins = NULL;
    tstring *expiry = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    crawler_ins = tstr_array_get_str_quick(name_components, 4);

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &auto_gen_src);
    bail_error(err);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
		    &expiry, NULL,
		    "/nkn/crawler/config/profile/%s/auto_generate_src/%s/auto_gen_expiry",
		    crawler_ins, ts_str(auto_gen_src));
    bail_error(err);


    err = cli_printf(
                _("Expiry Value for Auto Generation:    %s   \n"), ts_str(expiry));
    bail_error(err);

    bail:
	ts_free(&auto_gen_src);
	return(err);
}

int validate_time(const char *date, const char *c_time)
{
    int err = 0;
    lt_time_sec now = 0;
    lt_time_sec configured = 0;
    lt_date l_date = 0;
    lt_time_sec l_sec = 0;
    char t_time[128];


    /* Get current time */
    now = lt_curr_time();

    /* get configured date */
    err = lt_str_to_date(date, &l_date);
    if (err)
	goto bail_6;

    err = lt_str_to_daytime_sec(c_time, &l_sec);
    if (err)
	goto bail_6;

    err = lt_date_daytime_to_time_sec(l_date, l_sec, &configured);
    if (err)
	goto bail_6;

    if (configured < now) {
	err = 12345678; // some magic which we hope TM will never use as error code
    }

bail_6:
    if (err == 12345678) {
	cli_printf("%% warning: configured time occurs in the past: %s %s\n", date, c_time);
    } else if (err) {
	cli_printf_error("%s (%s %s) \n", lc_enum_to_string_quiet_null(lc_error_code_map, err), date, c_time);
	return err;
    }
    err = 0;
bail:
    return err;
}
