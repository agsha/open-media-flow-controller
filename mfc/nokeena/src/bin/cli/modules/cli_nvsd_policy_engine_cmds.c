/*
 * Filename :   cli_nvsd_policy_engine_cmds.c
 * Date     :   2008/12/15
 * Author   :   Thilak Raj S
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
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
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/

cli_expansion_option cli_pe_srvr_proto_opts[] = {
    {"ICAP", N_("Configure protcol as ICAP"), (void *) "ICAP"},
    {NULL, NULL, NULL},
};

int
cli_nvsd_pe_srvr_uri_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int
cli_nvsd_pe_srvr_cmd_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_nvsd_policy_engine_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);

int
cli_nvsd_policy_engine_srvr_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);
int
cli_nvsd_pe_cmd_hdlr(void *data,
		const cli_command       *cmd,
		const tstr_array        *cmd_line,
		const tstr_array        *params);

int
cli_nvsd_pe_fetch_hdlr(void *data,
		const cli_command       *cmd,
		const tstr_array        *cmd_line,
		const tstr_array        *params);

int
cli_nvsd_pe_upload_hdlr(void *data,
		const cli_command       *cmd,
		const tstr_array        *cmd_line,
		const tstr_array        *params);

int
cli_nvsd_pe_show_one(
		const bn_binding_array *bindings,
		uint32 idx,
		const bn_binding *binding,
		const tstring *name,
		const tstr_array *name_components,
		const tstring *name_last_part,
		const tstring *value,
		void *cb_data);

int
cli_nvsd_pe_help(
		void *data, 
		cli_help_type help_type,
		const cli_command *cmd, 
		const tstr_array *cmd_line,
		const tstr_array *params, 
		const tstring *curr_word,
		void *context);

int
cli_nvsd_pe_srvr_ls(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_nvsd_pe_srvr_show_one(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_nvsd_pe_show_cntrs(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

#define POLICY_SH_DIR 		"/opt/nkn/bin/policy/"
#define MAX_OUTPUT		256
/*----------------------------------------------------------------------------
 * LOCAL FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static int
cli_nvsd_pe_host_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_pe_get_policies(
		tstr_array **ret_names,
		tbool hide_display);

static int
cli_nvsd_pe_new(const tstr_array *params);

static int
cli_nvsd_pe_edit(const tstr_array *params);

static int
cli_nvsd_pe_delete(const tstr_array *params);

static int
cli_nvsd_pe_list(const tstr_array *params);

static int
cli_nvsd_pe_commit(const tstr_array *params);

static int
cli_nvsd_pe_revert(const tstr_array *params);

enum {
	cc_pe_new = 1,
	cc_pe_edit,
	cc_pe_delete,
	cc_pe_list,
	cc_pe_commit,
	cc_pe_revert,
};

enum {
	cc_pe_srvr_del = 1,
	cc_pe_srvr_hst_del,

};

static int
cli_nvsd_pe_run_script(const char *abs_path,
			const tstr_array *argv,
			char *str_ret_output,
			int32_t *ret_status);

static int
cli_nvsd_pe_upload_finish(const char *password,
			    clish_password_result result,
			    const cli_command *cmd, 
			    const tstr_array *cmd_line,
			    const tstr_array *params, 
			    void *data,
			    tbool *ret_continue);

static int
cli_nvsd_pe_fetch_finish(const char *password, 
			    clish_password_result result,
			    const cli_command *cmd, 
			    const tstr_array *cmd_line,
			    const tstr_array *params,
			    void *data,
			    tbool *ret_continue);
cli_help_callback  cli_pe_generic_help = NULL;

typedef int (*cli_nvsd_pe_get_item_t) (const char *nd_pattern,
			    tstr_array **ret_names);
cli_nvsd_pe_get_item_t cli_nvsd_pe_get_item = NULL;

typedef int (*cli_nvsd_pe_is_valid_item_t)(const char *node, const char *match_val,
                                    tbool *is_valid);

cli_nvsd_pe_is_valid_item_t cli_nvsd_pe_is_valid_item = NULL;

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/

int
cli_nvsd_policy_engine_srvr_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);
	void *cli_pe_get_items_func_v = NULL;
	void *cli_pe_is_valid_item_func_v = NULL;

	err = cli_get_symbol("cli_ssld_cfg_cmds", "cli_ssld_cfg_get_items",
			&cli_pe_get_items_func_v);
	cli_nvsd_pe_get_item = cli_pe_get_items_func_v;
	bail_error_null(err, cli_nvsd_pe_get_item);

	err = cli_get_symbol("cli_ssld_cfg_cmds", "cli_ssld_cfg_is_valid_item",
			&cli_pe_is_valid_item_func_v);
	cli_nvsd_pe_is_valid_item = cli_pe_is_valid_item_func_v;
	bail_error_null(err, cli_nvsd_pe_is_valid_item);

#ifdef PROD_FEATURE_I18N_SUPPORT
	err = cli_set_gettext_domain(GETTEXT_DOMAIN);
	bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */


	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy server";
	cmd->cc_help_descr =        N_("Define external policy servers");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy server *";
	cmd->cc_help_exp =          N_("<server_name>");
	cmd->cc_help_data =	    (void*)"/nkn/nvsd/policy_engine/config/server";
	cmd->cc_help_use_comp =	    true;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/nvsd/policy_engine/config/server/*";
	cmd->cc_help_exp_hint =     N_("Define external policy server name");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy server * host";
	cmd->cc_help_descr =        N_("Edit a configured policy");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy server * host *";
	cmd->cc_help_exp =          N_("<FQDN>");
	cmd->cc_help_exp_hint =     N_("Define external policy server ip/domain name and port");
	cmd->cc_help_use_comp =	    true;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/nvsd/policy_engine/config/server/$1$/endpoint/*";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy server * host * *";
	cmd->cc_help_exp =          N_("<port>");
	cmd->cc_help_exp_hint =     N_("Define external policy server ip/domain name and port");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name2 =        "/nkn/nvsd/policy_engine/config/server/$1$/endpoint/$2$";
	cmd->cc_exec_value2 =       "$2$";
	cmd->cc_exec_type2 = 	    bt_string;
	cmd->cc_exec_name =         "/nkn/nvsd/policy_engine/config/server/$1$/endpoint/$2$/port";
	cmd->cc_exec_value =        "$3$";
	cmd->cc_exec_type = 	    bt_uint16;
	cmd->cc_revmap_order =      cro_policy;
	cmd->cc_revmap_names =      "/nkn/nvsd/policy_engine/config/server/*/endpoint/*";
	cmd->cc_revmap_callback =   cli_nvsd_pe_host_revmap;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy server * protocol";
	cmd->cc_help_descr =        N_("Define external policy server protocol");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy server * protocol *";
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_help_options =      cli_pe_srvr_proto_opts;
	cmd->cc_help_num_options =  sizeof(cli_pe_srvr_proto_opts)/sizeof(cli_pe_srvr_proto_opts);
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/nvsd/policy_engine/config/server/$1$/proto";
	cmd->cc_exec_value =        "$2$";
	cmd->cc_exec_type = 	    bt_string;
	cmd->cc_revmap_type =	    crt_auto;
	cmd->cc_revmap_order =	    cro_policy;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy server * uri";
	cmd->cc_help_descr =        N_("Define external policy server uri");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy server * uri *";
	cmd->cc_help_exp =          N_("<uri>");
	cmd->cc_help_exp_hint =     N_("Define external policy server uri");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr ;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/nvsd/policy_engine/config/server/$1$/uri";
	cmd->cc_exec_value =        "$2$";
	cmd->cc_exec_type = 	    bt_string;
	cmd->cc_revmap_order =      cro_policy;
	cmd->cc_revmap_type =	    crt_auto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy server * enable";
	cmd->cc_help_descr =        N_("Enable this external policy server");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/nvsd/policy_engine/config/server/$1$/enabled";
	cmd->cc_exec_value =        "true";
	cmd->cc_exec_type = 	    bt_bool;
	cmd->cc_revmap_order =      cro_policy;
	cmd->cc_revmap_type =       crt_auto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy server * disable";
	cmd->cc_help_descr =        N_("Disable this external policy");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_reset;
	cmd->cc_exec_name =         "/nkn/nvsd/policy_engine/config/server/$1$/enabled";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show policy";
	cmd->cc_help_descr =        N_("Show policy engine related information");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show policy server";
	cmd->cc_help_descr =        N_("Show external policy configuration");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show policy server list";
	cmd->cc_help_descr =	    "Show external policy configuration list";
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_query_basic_curr;
	cmd->cc_exec_callback =	    cli_nvsd_pe_srvr_ls;
	cmd->cc_magic = 	    cc_pe_list;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show policy server *";
	cmd->cc_help_exp =          N_("<server-name>");
	cmd->cc_help_exp_hint =     N_("Show external policy configuration list");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_help_use_comp =     true;
	cmd->cc_capab_required =    ccp_query_basic_curr;
	cmd->cc_exec_callback =	    cli_nvsd_pe_srvr_show_one;
	cmd->cc_revmap_type =       crt_none;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/nvsd/policy_engine/config/server/*";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no policy";
	cmd->cc_help_descr =        N_("Remove external policy");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no policy server";
	cmd->cc_help_descr =        N_("Remove external policy server");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no policy server *";
	cmd->cc_help_exp =          N_("<server_name>");
	cmd->cc_help_use_comp =     true;
	cmd->cc_help_exp_hint =     N_("Remove external policy server");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/nvsd/policy_engine/config/server/*";
	cmd->cc_exec_operation =    cxo_delete;
	cmd->cc_exec_name =         "/nkn/nvsd/policy_engine/config/server/$1$";
	cmd->cc_exec_callback =	    cli_nvsd_pe_srvr_cmd_hdlr;
	cmd->cc_magic =		    cc_pe_srvr_del;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no policy server * host";
	cmd->cc_help_descr =        N_("Remove external policy server host");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no policy server * host *";
	cmd->cc_help_exp =          N_("<FQDN>");
	cmd->cc_help_exp_hint =     N_("Remove external policy server host");
	cmd->cc_help_use_comp =	    true;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/nvsd/policy_engine/config/server/$1$/endpoint/*";
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_delete;
	cmd->cc_exec_name =         "/nkn/nvsd/policy_engine/config/server/$1$/endpoint/$2$";
	cmd->cc_revmap_type =       crt_none;
	cmd->cc_comp_type =         cct_matching_names;
	cmd->cc_exec_callback =	    cli_nvsd_pe_srvr_cmd_hdlr;
	cmd->cc_magic =		    cc_pe_srvr_hst_del;
	CLI_CMD_REGISTER;

	/* All policy command are actions,though they store configs */
	err = cli_revmap_ignore_bindings_va(4,
			"/nkn/nvsd/policy_engine/config/server/*/namespace/*",
			"/nkn/nvsd/policy_engine/config/server/*",
			"/nkn/nvsd/policy_engine/config/server/*/endpoint/*",
			"/nkn/nvsd/policy_engine/config/server/*/enabled");

	bail_error(err);

bail:
	return err;
}

int
cli_nvsd_policy_engine_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

#ifdef PROD_FEATURE_I18N_SUPPORT
	err = cli_set_gettext_domain(GETTEXT_DOMAIN);
	bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */


	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy";
	cmd->cc_help_descr =        N_("Configure policies");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy new";
	cmd->cc_help_descr =        N_("Create a new policy");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy new *";
	cmd->cc_help_exp =          N_("<policy_file>");
	cmd->cc_help_exp_hint =     N_("Create a policy file");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =	    cli_nvsd_pe_cmd_hdlr;
	cmd->cc_magic = 	    cc_pe_new;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy edit";
	cmd->cc_help_descr =        N_("Edit a configured policy");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy edit *";
	cmd->cc_help_exp =          N_("<policy_file>");
	cmd->cc_help_exp_hint =     N_("Edit a policy file");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_help_callback =	    cli_nvsd_pe_help;
	cmd->cc_exec_callback =	    cli_nvsd_pe_cmd_hdlr;
	cmd->cc_magic = 	    cc_pe_edit;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/nvsd/policy_engine/config/policy/script/*";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy delete";
	cmd->cc_help_descr =        N_("Delete a configured policy");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy delete *";
	cmd->cc_help_exp =          N_("<policy_file>");
	cmd->cc_help_exp_hint =     N_("delete a policy file");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_help_callback =	    cli_nvsd_pe_help;
	cmd->cc_exec_callback =	    cli_nvsd_pe_cmd_hdlr;
	cmd->cc_magic = 	    cc_pe_delete;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/nvsd/policy_engine/config/policy/script/*";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy commit";
	cmd->cc_help_descr =        N_("Commit a policy");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy commit *";
	cmd->cc_help_exp =          N_("<policy_file>");
	cmd->cc_help_exp_hint =     N_("commit a policy file");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_help_callback =	    cli_nvsd_pe_help;
	cmd->cc_exec_callback =	    cli_nvsd_pe_cmd_hdlr;
	cmd->cc_magic = 	    cc_pe_commit;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/nvsd/policy_engine/config/policy/script/*";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy revert";
	cmd->cc_help_descr =        N_("Revert a policy to its older version");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy revert *";
	cmd->cc_help_exp =          N_("<policy_file>");
	cmd->cc_help_exp_hint =     N_("revert a policy file");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_help_callback =	    cli_nvsd_pe_help;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =	    cli_nvsd_pe_cmd_hdlr;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/nvsd/policy_engine/config/policy/script/*";
	cmd->cc_magic = 	    cc_pe_revert;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy list";
	cmd->cc_help_descr =        N_("List all the configured policies");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy list all";
	cmd->cc_help_descr =	    "List all the policies";
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_query_basic_curr;
	cmd->cc_exec_callback =	    cli_nvsd_pe_cmd_hdlr;
	cmd->cc_magic = 	    cc_pe_list;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy fetch";
	cmd->cc_help_descr =        N_("Fetch the policy from a remote location");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy fetch *";
	cmd->cc_help_exp =          N_("<Policy name>");
	cmd->cc_help_exp_hint =     N_("specify the name of the policy");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy fetch * *";
	cmd->cc_help_exp =          N_("<scp://username:password@hostname/path>");
	cmd->cc_help_exp_hint =     N_("URL of the file");
	cmd->cc_flags =             ccf_terminal|ccf_ignore_length|ccf_sensitive_param;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =	    cli_nvsd_pe_fetch_hdlr;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy upload";
	cmd->cc_help_descr =        N_("Upload the policy to a remote location");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy upload *";
	cmd->cc_help_exp =          N_("<Policy name>");
	cmd->cc_help_exp_hint =     N_("specify the name of the policy");
	cmd->cc_comp_type =         cct_matching_names;
	cmd->cc_help_callback =	    cli_nvsd_pe_help;
	cmd->cc_comp_pattern =      "/nkn/nvsd/policy_engine/config/policy/script/*";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "policy upload * *";
	cmd->cc_help_exp =          N_("<scp://username:password@hostname/path>");
	cmd->cc_help_exp_hint =     N_("URL of the file");
	cmd->cc_flags =             ccf_terminal|ccf_ignore_length;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_callback =	    cli_nvsd_pe_upload_hdlr;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show policy counters";
	cmd->cc_help_descr =        N_("Display policy engine counters");
	cmd->cc_flags =		    ccf_terminal;
	cmd->cc_capab_required =    ccp_query_basic_curr;
	cmd->cc_exec_callback =     cli_nvsd_pe_show_cntrs;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	/* All policy command are actions,though they store configs */
	err = cli_revmap_ignore_bindings_va(2,
			"/nkn/nvsd/policy_engine/config/policy/script/*",
			"/nkn/nvsd/policy_engine/config/policy/script/*/commited");
	bail_error(err);

	err = cli_nvsd_policy_engine_srvr_cmds_init(info, context);
	bail_error(err);

bail:
	return err;
}

int
cli_nvsd_pe_cmd_hdlr(void *data,
		const cli_command       *cmd,
		const tstr_array        *cmd_line,
		const tstr_array        *params)
{
	int err = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);

	switch(cmd->cc_magic)
	{
		case cc_pe_new:
			err = cli_nvsd_pe_new(params);
			bail_error(err);
			break;

		case cc_pe_edit:
			err = cli_nvsd_pe_edit(params);
			bail_error(err);
			break;

		case cc_pe_delete:
			err = cli_nvsd_pe_delete(params);
			bail_error(err);
			break;

		case cc_pe_list:
			err = cli_nvsd_pe_list(params);
			bail_error(err);
			break;

		case cc_pe_commit:
			err = cli_nvsd_pe_commit(params);
			bail_error(err);
			break;

		case cc_pe_revert:
			err = cli_nvsd_pe_revert(params);
			bail_error(err);
			break;

	}


bail:
	return err;
}
int
cli_nvsd_pe_fetch_hdlr(void *data,
		const cli_command       *cmd,
		const tstr_array        *cmd_line,
		const tstr_array        *params)
{
	int err = 0;
	const char *remote_url = NULL;

	UNUSED_ARGUMENT(data);

	remote_url = tstr_array_get_str_quick(params, 1);
	bail_null(remote_url);

	if (clish_is_shell()) {
		err = clish_maybe_prompt_for_password_url
			(remote_url, cli_nvsd_pe_fetch_finish,
			 cmd, cmd_line, params, NULL);
		bail_error(err);
	}
	else {
		err = cli_nvsd_pe_fetch_finish(NULL, cpr_ok,
				cmd, cmd_line, params,
				NULL, NULL);
		bail_error(err);
	}


bail:
	return err;
}

int
cli_nvsd_pe_upload_hdlr(void *data,
		const cli_command       *cmd,
		const tstr_array        *cmd_line,
		const tstr_array        *params)
{
	int err = 0;
	const char *remote_url = NULL;

	UNUSED_ARGUMENT(data);

	remote_url = tstr_array_get_str_quick(params, 1);
	bail_null(remote_url);

	if (clish_is_shell()) {
		err = clish_maybe_prompt_for_password_url
			(remote_url, cli_nvsd_pe_upload_finish,
			 cmd, cmd_line, params, NULL);
		bail_error(err);
	}
	else {
		err = cli_nvsd_pe_upload_finish(NULL, cpr_ok,
				cmd, cmd_line, params,
				NULL, NULL);
		bail_error(err);
	}


bail:
	return err;

}

int
cli_nvsd_pe_srvr_show_one(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	tstr_array *vhost_lst = NULL;
	node_name_t ep_nd = {0};
	node_name_t pe_nd = {0};
	const char *srvr_name = NULL;
	uint32 num_names = 0;
	uint32 idx = 0;
	tbool is_valid = false;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	srvr_name = tstr_array_get_str_quick(params, 0);
	bail_null(srvr_name);

	snprintf(pe_nd, sizeof(pe_nd), "/nkn/nvsd/policy_engine/config/server/%s", srvr_name);
	err = cli_nvsd_pe_is_valid_item(pe_nd, NULL, &is_valid);
	bail_error(err);

	if(!is_valid) {
		cli_printf_error("Invalid Policy Engine server");
		goto bail;
	}

	err = cli_printf("Policy Server Name: %s\n", srvr_name);
	bail_error(err);

	err = cli_printf_query("  Enabled: "
			"#/nkn/nvsd/policy_engine/config/server/%s/enabled#\n", srvr_name);
	bail_error(err);

	err = cli_printf_query("  URI: "
			"#/nkn/nvsd/policy_engine/config/server/%s/uri#\n", srvr_name);
	bail_error(err);

	err = cli_printf_query("  Proto: "
			"#/nkn/nvsd/policy_engine/config/server/%s/proto#\n", srvr_name);
	bail_error(err);


	snprintf(ep_nd, sizeof(ep_nd), "/nkn/nvsd/policy_engine/config/server/%s/endpoint", srvr_name);

	err = cli_nvsd_pe_get_item(ep_nd, &vhost_lst);
	num_names = tstr_array_length_quick(vhost_lst);


	if(num_names > 0) {
		const char *ep_name = NULL;
		err = cli_printf(_("  Currently configured Hosts :\n"));
		bail_error(err);

		for(idx = 0; idx < num_names; idx++) {
			ep_name = tstr_array_get_str_quick(vhost_lst, idx);
			bail_null(ep_name);

			err = cli_printf("   Host : "
					"%s\n", ep_name);
			bail_error(err);

			err = cli_printf_query("   Port : "
					"#/nkn/nvsd/policy_engine/config/server/%s/endpoint/%s/port#\n",
					    srvr_name, ep_name);
			bail_error(err);

		}
	} else {
		err = cli_printf(_("No Hosts configured\n"));
		bail_error(err);

	}
bail:
	tstr_array_free(&vhost_lst);
	return err;
}

int
cli_nvsd_pe_srvr_ls(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	tstr_array *names = NULL;
	tstr_array *vhost_lst = NULL;
	uint32 num_names = 0;
	uint32 idx = 0;
	uint32 i = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	err = cli_nvsd_pe_get_item("/nkn/nvsd/policy_engine/config/server", &names);
	num_names = tstr_array_length_quick(names);

	err = cli_printf(_("Currently configured External Servers   :\n"));
	bail_error(err);

	if(num_names > 0) {

		err = cli_printf("   Policy Server                                  Namespace\n");
		bail_error(err);

		err = cli_printf_query(_("============================================================\n"));
		bail_error(err);

		for(idx = 0; idx < num_names; idx++) {

			const char *cert_name = NULL;
			const char *vhost_name = NULL;
			node_name_t vhost_node = {0};

			uint32 num_ns = 0;

			cert_name = tstr_array_get_str_quick(names, idx);
			bail_null(cert_name);

			snprintf(vhost_node, sizeof(vhost_node), "/nkn/nvsd/policy_engine/config/server/%s/namespace",
											cert_name);

			err = cli_nvsd_pe_get_item(vhost_node, &vhost_lst);
			bail_error_null(err, vhost_lst);

			num_ns = tstr_array_length_quick(vhost_lst);

			if(num_ns)
			    vhost_name = tstr_array_get_str_quick(vhost_lst, 0);

			err = cli_printf_query("%16s%43s\n",cert_name, vhost_name ? vhost_name : "N/A");
			bail_error(err);

			for(i = 1; i < num_ns; i++) {
				vhost_name = tstr_array_get_str_quick(vhost_lst, i);
				err = cli_printf_query("%59s\n",vhost_name ? vhost_name: "N/A");
				bail_error(err);
			}
			cli_printf("------------------------------------------------------------\n");
			tstr_array_free(&vhost_lst);
		}
	} else {
		err = cli_printf(_("No External Policy Servers configured\n"));
		bail_error(err);

	}
bail:
	tstr_array_free(&names);
	tstr_array_free(&vhost_lst);
	return err;
}
/*----------------------------------------------------------------------------
 * LOCAL FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/


/* 
* TODO: Use lc_launch against nkn_clish_run_proagram_fg as return status is not know and to have better control
*
*/

static int cli_nvsd_pe_run_script(const char *abs_path, const tstr_array *argv, char *str_ret_output, int32_t *ret_status )
{

	int err = 0;
	uint32 param_count = 0;
	uint32 i;
	lc_launch_params *lp = NULL;
	lc_launch_result *lr = NULL;

	lr = malloc(sizeof(*lr));
	bail_null(lr);
	memset(lr, 0 ,sizeof(*lr));

	err = lc_launch_params_get_defaults(&lp);
	bail_error_null(err, lp);

	/* num of params */
	param_count = tstr_array_length_quick(argv);

	err = ts_new_str(&(lp->lp_path), abs_path);
	bail_error(err);

	err = tstr_array_new(&(lp->lp_argv), 0);
	bail_error(err);

	err = tstr_array_insert_str(lp->lp_argv, 0, abs_path);
	bail_error(err);

	for(i = 1; i< param_count; i++)
	{
		const char *param_value = NULL;

		param_value = tstr_array_get_str_quick(argv , i);

		if(!param_value)
			break;

		err = tstr_array_insert_str(lp->lp_argv, i,
				param_value);

	}

	lp->lp_wait = true;
	lp->lp_env_opts = eo_preserve_all;
	lp->lp_log_level = LOG_INFO;

	lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
	lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;

	err = lc_launch(lp, lr);
	bail_error(err);

	*ret_status = lr->lr_exit_status;

	if(lr->lr_exit_status == -1) {
		lc_log_basic(LOG_DEBUG, "unable to get the proc status");

	} else {

		lc_log_basic(LOG_DEBUG, "got some output");
		lc_log_basic(LOG_DEBUG, "%s",lr->lr_captured_output ? ts_str(lr->lr_captured_output): "");
		if (lr->lr_captured_output)
			strncpy(str_ret_output, ts_str(lr->lr_captured_output), MAX_OUTPUT - 1);
			str_ret_output[MAX_OUTPUT] = '\0';

	}

bail :
	lc_launch_params_free(&lp);
	lc_launch_result_free_contents(lr);
	return err;

}


static int 
cli_nvsd_pe_new(const tstr_array *params)
{
	int err = 0;
	const char *tcl_file = NULL;
	tstr_array *cmd_params = NULL;
	uint32 ret_err = 0;
	char *pe_name = NULL;

	tcl_file = tstr_array_get_str_quick(params, 0);
	bail_null(tcl_file);

	pe_name = smprintf("/nkn/nvsd/policy_engine/config/policy/script/%s", tcl_file);
	bail_null(pe_name);

	err = mdc_create_binding(cli_mcc, &ret_err, NULL, bt_string,
			tcl_file, pe_name);
	bail_error(err);

	if(ret_err) // create node failed
		goto bail;

	err = tstr_array_new_va_str(&cmd_params, NULL, 2, POLICY_SH_DIR"pe_new.sh", tcl_file);
	bail_error(err);

	err = nkn_clish_run_program_fg(POLICY_SH_DIR"pe_new.sh", cmd_params, NULL, NULL, NULL);
	bail_error(err);

bail:
	safe_free(pe_name);
	tstr_array_free(&cmd_params);
	return err;
}

static int 
cli_nvsd_pe_edit(const tstr_array *params)
{
	int err = 0;
	const char *tcl_file = NULL;
	tstr_array *cmd_params = NULL;

	tcl_file = tstr_array_get_str_quick(params, 0);
	bail_null(tcl_file);

	err = tstr_array_new_va_str(&cmd_params, NULL, 2, POLICY_SH_DIR"pe_edit.sh", tcl_file);
	bail_error(err);

	err = nkn_clish_run_program_fg(POLICY_SH_DIR"pe_edit.sh", cmd_params, NULL, NULL, NULL);
	bail_error(err);

bail:
	tstr_array_free(&cmd_params);
	return err;
}

static int 
cli_nvsd_pe_delete(const tstr_array *params)
{
	int err = 0;
	const char *tcl_file = NULL;
	tstr_array *cmd_params = NULL;
	char *pe_name = NULL;
	tstring *t_ns = NULL;
	int32_t ret_status = 0;
	char ret_output[MAX_OUTPUT];

	memset(ret_output, 0 ,sizeof(ret_output));

	tcl_file = tstr_array_get_str_quick(params, 0);
	bail_null(tcl_file);

	//Check to see if this policy is associated with some namespace.
	// Trigger an action to see if the policy is associated with namespace
	// 3rd argument is false to say that we are not intereseted if the namespace is active

	if(nkn_cli_is_policy_bound(tcl_file, &t_ns, "false", "false")) {
		goto bail;
	}

	pe_name = smprintf("/nkn/nvsd/policy_engine/config/policy/script/%s", tcl_file);
	bail_null(pe_name);

	err = mdc_delete_binding(cli_mcc, NULL, NULL, pe_name);
	bail_error(err);

	err = tstr_array_new_va_str(&cmd_params, NULL, 2, POLICY_SH_DIR"pe_del.sh", tcl_file);
	bail_error(err);

	err = cli_nvsd_pe_run_script(POLICY_SH_DIR"pe_del.sh", cmd_params, ret_output, &ret_status);
	bail_error(err);

	if(!ret_status && strlen(ret_output) > 0) {
	    cli_printf("%s",ret_output);
	}


bail:
	safe_free(pe_name);
	tstr_array_free(&cmd_params);
	return err;
}

static int 
cli_nvsd_pe_list(const tstr_array *params)
{
	int err = 0;
	tstr_array *cmd_params = NULL;
	uint32 ret_matches = 0;

	UNUSED_ARGUMENT(params);
	cli_printf("     policy                 Namespace Associated\n");
	cli_printf("---------------------------------------------------\n");

	err = mdc_foreach_binding(cli_mcc, "/nkn/nvsd/policy_engine/config/policy/script/*", NULL,
			cli_nvsd_pe_show_one, NULL, &ret_matches);
	bail_error(err);

	if (ret_matches == 0) {
		cli_printf(
				_("No policies configured."));
	}

bail:
	tstr_array_free(&cmd_params);
	return err;
}

static int
cli_nvsd_pe_host_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *t_port = NULL;
	const char *cc_policy_srvr = NULL;
	node_name_t port_nd = {0};

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);


	snprintf(port_nd, sizeof(port_nd), "%s/port", name);

	cc_policy_srvr = tstr_array_get_str_quick(name_parts, 5);
	bail_null(cc_policy_srvr);

	err = bn_binding_array_get_value_tstr_by_name(bindings, port_nd, NULL, &t_port);
	bail_error_null(err, t_port);

	/* Handle the case of content-type-any */
	err = tstr_array_append_sprintf(ret_reply->crr_cmds,
			"policy server %s host %s %s",
			cc_policy_srvr, value, ts_str(t_port));
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/port", name);
	bail_error(err);


bail:
	ts_free(&t_port);
	return err;
}


static int
cli_nvsd_pe_commit(const tstr_array *params)
{
	int err = 0;
	const char *tcl_file = NULL;
	tstr_array *cmd_params = NULL;
	char *bn_policy_commited = NULL;
	char *time_stamp_node = NULL;
	tstring *t_ns = NULL;
	char *bn_name = NULL;
	bn_binding *binding = NULL;
	char *policy_file = NULL;
	tstring *output = NULL;
	int32 ret_status = 0;
	char ret_output[MAX_OUTPUT];
	int32_t time_stamp = 0;
	str_value_t ts = {0};

	memset(ret_output, 0 ,sizeof(ret_output));

	/* check to see if the policy file is correct- call the executable from here or the script ? */
	tcl_file = tstr_array_get_str_quick(params, 0);
	bail_null(tcl_file);

	//Check to see if that policy exists
	bn_name = smprintf("/nkn/nvsd/policy_engine/config/policy/script/%s",tcl_file);
	bail_null(bn_name);

	err = mdc_get_binding(cli_mcc, NULL, NULL,
			false, &binding, bn_name, NULL);
	bail_error(err);

	if (NULL == binding) {
		cli_printf_error(_("Invalid Policy : %s\n"), tcl_file);
		goto bail;
	}

	/* Check for the policy syntax here */
	policy_file = smprintf("/nkn/policy_engine/%s.tcl.scratch",tcl_file);
	bail_null(policy_file);

	err = lc_launch_quick_status(&ret_status, &output, true, 2,
			"/opt/nkn/bin/pecheck", policy_file);
	bail_error(err);

	if(ret_status && output) {
		cli_printf_error("Policy commit failed due to below syntax error\n");
		cli_printf_error("%s", ts_str(output));
		goto bail;
	}

	err = tstr_array_new_va_str(&cmd_params, NULL, 2, POLICY_SH_DIR"pe_com.sh", tcl_file);
	bail_error(err);

	ret_status = 0;
	err = cli_nvsd_pe_run_script(POLICY_SH_DIR"pe_com.sh", cmd_params, ret_output, &ret_status);
	bail_error(err);

	if(!ret_status && strlen(ret_output) > 0) {
	    cli_printf("%s",ret_output);
	}

	bn_policy_commited = smprintf("/nkn/nvsd/policy_engine/config/policy/script/%s/commited",tcl_file);
	bail_null(bn_policy_commited);

	err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_bool,
			"true", bn_policy_commited);
	bail_error(err);

	/* Not interested in return status */
	nkn_cli_is_policy_bound(tcl_file, &t_ns, "true", "true");

	if(t_ns && (ts_length(t_ns) > 1) ) {
		/*Set the time stamp here */
		time_stamp_node = smprintf("/nkn/nvsd/namespace/%s/policy/time_stamp", ts_str(t_ns));
		bail_null(time_stamp_node);

		time_stamp = lt_curr_time();
		snprintf(ts, sizeof(ts), "%d", time_stamp);

		err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_int32,
				ts, time_stamp_node);
		bail_error(err);

	}


bail:
	safe_free(bn_policy_commited);
	tstr_array_free(&cmd_params);
	bn_binding_free(&binding);
	safe_free(bn_name);
	safe_free(policy_file);
	ts_free(&output);
	ts_free(&t_ns);
	return err;
}

static int 
cli_nvsd_pe_revert(const tstr_array *params)
{
	int err = 0;
	const char *tcl_file = NULL;
	tstr_array *cmd_params = NULL;
	tstring *t_ns = NULL;
	int32_t ret_status = 0;
	char ret_output[MAX_OUTPUT];

	memset(ret_output, 0 ,sizeof(ret_output));

	tcl_file = tstr_array_get_str_quick(params, 0);
	bail_null(tcl_file);

	/* check to see if an active namespace is associatd with this policy
	*  If so,ask the user to inactivate the namespace
	*/
	# if 0
	if(nkn_cli_is_policy_bound(tcl_file, NULL)) {
		goto bail;
	}
#endif
	if(nkn_cli_is_policy_bound(tcl_file, &t_ns, "true", "false")) {
		if(t_ns) {
			// Get the namespace and see if its active
			char *ns_active = NULL;
			tbool is_ns_active = false;
			ns_active = smprintf("/nkn/nvsd/namespace/%s/status/active", ts_str(t_ns));
			err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, &is_ns_active, ns_active, NULL);
			bail_error(err);

			if(is_ns_active) {
				//cli_printf("Policy is assocaited with a active namespace %s", ts_str(t_ns));
				goto bail;
			}

		}
//		goto bail;
	}


	err = tstr_array_new_va_str(&cmd_params, NULL, 2, POLICY_SH_DIR"pe_revert.sh", tcl_file);
	bail_error(err);

	err = cli_nvsd_pe_run_script(POLICY_SH_DIR"pe_revert.sh", cmd_params, ret_output, &ret_status);
	bail_error(err);

	if(!ret_status  && strlen(ret_output) > 0 ) {
	    cli_printf("%s",ret_output);
	}


	/*
	* If succesfull ask the user to activate the namespace again
	*/

bail:	
	tstr_array_free(&cmd_params);
	return err;
}

static int
cli_nvsd_pe_upload_finish(const char *password, 
			    clish_password_result result,
			    const cli_command *cmd, 
			    const tstr_array *cmd_line,
			    const tstr_array *params, 
			    void *data,
			    tbool *ret_continue)
{
	int err = 0;
	char *policy_path = NULL;
	const char *policy_file = NULL;
	const char *remote_url = NULL;
	tstring *tcl_file = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(ret_continue);


	if (result != cpr_ok) {
		goto bail;
	}

	policy_file = tstr_array_get_str_quick(params, 0);
	bail_null(policy_file);

	err = ts_new_sprintf(&tcl_file, "%s.tcl.scratch", policy_file);
	bail_error_null(err, tcl_file);

	policy_path = smprintf("/nkn/policy_engine/%s", ts_str(tcl_file));
	bail_null(policy_path);

	remote_url = tstr_array_get_str_quick(params, 1);
	bail_null(remote_url);

	if (password) {
		err = mdc_send_action_with_bindings_str_va(cli_mcc,
				NULL, NULL,
				"/file/transfer/upload", 3,
				"local_path", bt_string, policy_path,
				"remote_url", bt_string, remote_url,
				"password", bt_string, password);
		bail_error(err);
	}
	else {
		err = mdc_send_action_with_bindings_str_va(cli_mcc,
				NULL, NULL,
				"/file/transfer/upload", 3,
				"local_dir", bt_string, "/nkn/policy_engine/",
				"local_filename", bt_string, ts_str(tcl_file),
				"remote_url", bt_string, remote_url);
		bail_error(err);
	}

bail:
	safe_free(policy_path);
	ts_free(&tcl_file);
	return err;
}

/* Copied from samara*/
static int
cli_nvsd_pe_fetch_finish(const char *password, 
			    clish_password_result result,
			    const cli_command *cmd, 
			    const tstr_array *cmd_line,
			    const tstr_array *params, 
			    void *data,
			    tbool *ret_continue)
{
	const char *url = NULL;
	const char *download_file = NULL;
	tstring *policy_file = NULL;
	int err = 0;
	bn_request *req = NULL;
	uint16 db_file_mode = 0;
	char *db_file_mode_str = NULL;
	char *pe_name = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(ret_continue);


	download_file = tstr_array_get_str_quick(params, 0);
	bail_null(download_file);

	err = ts_new_sprintf(&policy_file, "%s.tcl.scratch", download_file);
	bail_error_null(err, policy_file);

	url = tstr_array_get_str_quick(params, 1);
	bail_null(url);

	if (result != cpr_ok) {
		goto bail;
	}

	err = bn_action_request_msg_create(&req, "/file/transfer/download");
	bail_error(err);

	bail_null(url);
	err = bn_action_request_msg_append_new_binding
		(req, 0, "remote_url", bt_string, url, NULL);
	bail_error(err);

	if (password) {
		err = bn_action_request_msg_append_new_binding
			(req, 0, "password", bt_string, password, NULL);
		bail_error(err);
	}

	err = bn_action_request_msg_append_new_binding
		(req, 0, "local_dir", bt_string, "/nkn/policy_engine/", NULL);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding
		(req, 0, "local_filename", bt_string, ts_str_maybe(policy_file), NULL);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding
		(req, 0, "allow_overwrite", bt_bool, "true", NULL);
	bail_error(err);

	/*
	 * If we're not in the CLI shell, we won't be displaying progress,
	 * so there's no need to track it.
	 *
	 * XXX/EMT: we should make up our own progress ID, and start and
	 * end our own operation, and tell the action about it with the
	 * progress_image_id parameter.  This is so we can report errors
	 * that happen outside the context of the action request.  But
	 * it's not a big deal for now, since almost nothing happens
	 * outside of this one action, so errors are unlikely.
	 */
	if (clish_progress_is_enabled()) {
		err = bn_action_request_msg_append_new_binding
			(req, 0, "track_progress", bt_bool, "true", NULL);
		bail_error(err);
		err = bn_action_request_msg_append_new_binding
			(req, 0, "progress_oper_type", bt_string, "image_download", NULL);
		bail_error(err);
		err = bn_action_request_msg_append_new_binding
			(req, 0, "progress_erase_old", bt_bool, "true", NULL);
		bail_error(err);
	}

	db_file_mode = 0600;
	db_file_mode_str = smprintf("%d", db_file_mode);
	bail_null(db_file_mode_str);

	err = bn_action_request_msg_append_new_binding
		(req, 0, "mode", bt_uint16, db_file_mode_str, NULL);
	bail_error(err);

	if (clish_progress_is_enabled()) {
		err = clish_send_mgmt_msg_async(req);
		bail_error(err);
		err = clish_progress_track(req, NULL, 0, &ret_err, &ret_msg);
		bail_error(err);
	}
	else {
		err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &ret_err, &ret_msg);
		bail_error(err);
	}
	/* create policy if the download is a success */
	if(!ret_err) {

		pe_name = smprintf("/nkn/nvsd/policy_engine/config/policy/script/%s", download_file);
		bail_null(pe_name);

		err = mdc_create_binding(cli_mcc, &ret_err, NULL, bt_string,
				download_file, pe_name);
		bail_error(err);
	}

bail:
	safe_free(pe_name);
	ts_free(&ret_msg);
	ts_free(&policy_file);
	safe_free(db_file_mode_str);
	bn_request_msg_free(&req);
	return(err);
}

/*
tbool 
cli_nvsd_pe_is_policy_bound(const char *policy, tstring **associated_ns, const char *ns_active_check, const char *str_silent)
{
	int err = 0;
	uint32 ret_err = 0;
	tbool is_policy_bound = false;
	bn_binding *bn_policy = NULL;
	bn_binding *bn_ns_active = NULL;
	bn_binding *bn_silent = NULL;

	bn_binding_array *bn_bindings = NULL;
	bn_binding_array *barr = NULL;
	tstring *ns = NULL;
	
	err = bn_binding_array_new(&barr);
	bail_error(err);


	err = bn_binding_new_str_autoinval(&bn_policy, "policy", ba_value,
			bt_string, 0, policy);
	bail_error(err);

	err = bn_binding_new_str_autoinval(&bn_ns_active, "ns_active", ba_value,
			bt_bool, 0, ns_active_check);
	bail_error(err);

	err = bn_binding_new_str_autoinval(&bn_silent, "silent", ba_value,
			bt_bool, 0, str_silent);
	bail_error(err);


	err = bn_binding_array_append_takeover(barr, &bn_policy);
	bail_error(err);

	err = bn_binding_array_append_takeover(barr, &bn_ns_active);
	bail_error(err);

	err = bn_binding_array_append_takeover(barr, &bn_silent);
	bail_error(err);


	err = mdc_send_action_with_bindings_and_results(cli_mcc,
			&ret_err,
			NULL,
			"/nkn/nvsd/policy_engine/actions/check_policy_namespace_dependancy",
			barr,
			&bn_bindings);
	bail_error(err);

	if(bn_bindings &&  associated_ns != NULL ) {
		err = bn_binding_array_get_value_tstr_by_name(
				bn_bindings, "associated_ns", NULL, &ns);
		bail_error(err);
		*associated_ns = ns;
	}

	if(ret_err) {
		is_policy_bound = true;
		goto bail;
	}
bail:
	bn_binding_array_free(&barr);
	bn_binding_array_free(&bn_bindings);
	bn_binding_free(&bn_policy);
	bn_binding_free(&bn_ns_active);
	bn_binding_free(&bn_silent);
	return is_policy_bound;

}
*/

int
cli_nvsd_pe_show_one(
		const bn_binding_array *bindings,
		uint32 idx,
		const bn_binding *binding,
		const tstring *name,
		const tstr_array *name_components,
		const tstring *name_last_part,
		const tstring *value,
		void *cb_data)
{
	int err = 0;
	tstring *t_ns = NULL;

	UNUSED_ARGUMENT(bindings);
	UNUSED_ARGUMENT(idx);
	UNUSED_ARGUMENT(binding);
	UNUSED_ARGUMENT(name_components);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(cb_data);

	bail_null(name);
	bail_null(name_last_part);

	cli_printf_query(
			_(" %10s "), ts_str(name_last_part));

	nkn_cli_is_policy_bound(ts_str(name_last_part), &t_ns, "false", "true");	

	if(t_ns) {
		cli_printf("%14s%16s\n","",ts_str(t_ns));
	} else {
		cli_printf("%14s%16s\n","","N/A");

	}


bail:
	ts_free(&t_ns);
	return err;
}

static int
cli_nvsd_pe_get_policies(
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
			"/nkn/nvsd/policy_engine/config/policy/script", NULL);
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

int
cli_nvsd_pe_srvr_cmd_hdlr(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *pe_srvr_name = NULL;
	node_name_t item_node = {0};
	tbool is_valid = false;
	const char *host = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);

	pe_srvr_name = tstr_array_get_str_quick(params, 0);
	bail_null(pe_srvr_name);


	switch(cmd->cc_magic)
	{
		case cc_pe_srvr_del:
			snprintf(item_node, sizeof(item_node), "/nkn/nvsd/policy_engine/config/server/%s", pe_srvr_name);

			err = cli_nvsd_pe_is_valid_item(item_node, NULL, &is_valid);
			bail_error(err);

			if(!is_valid) {
				cli_printf_error("Invalid Policy Engine server");
				goto bail;
			}

			err = mdc_delete_binding(cli_mcc, NULL, NULL, item_node);
			bail_error(err);

			break;

		case cc_pe_srvr_hst_del:

			host= tstr_array_get_str_quick(params, 1);
			bail_null(host);

			snprintf(item_node, sizeof(item_node), "/nkn/nvsd/policy_engine/config/server/%s/endpoint/%s",
					pe_srvr_name, host);

			err = cli_nvsd_pe_is_valid_item(item_node, NULL, &is_valid);
			bail_error(err);

			if(!is_valid) {
				cli_printf_error("Invalid Host entry");
				goto bail;
			}
			err = mdc_delete_binding(cli_mcc, NULL, NULL, item_node);
			bail_error(err);
			break;

	}

bail:
	return err;
}

int
cli_nvsd_pe_help(
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
	const char *policy = NULL;
	int i = 0, num_names = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);
	UNUSED_ARGUMENT(curr_word);

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

			err = cli_nvsd_pe_get_policies(
					&names,
					true);


			num_names = tstr_array_length_quick(names);
			for(i = 0; i < num_names; ++i) {
				policy = tstr_array_get_str_quick(names, i);
				bail_null(policy);

				err = cli_add_expansion_help(context, policy, NULL);
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
cli_nvsd_pe_srvr_uri_revmap (void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err = 0;
    const char *cmd_text = NULL;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name);

    cmd_text = (char*)data;

    if (strcmp("", value))
    {
        err = tstr_array_append_sprintf
            (ret_reply->crr_cmds, cmd_text,
                        tstr_array_get_str_quick(name_parts, 5), value);
        bail_error(err);
    }
    /* Consume all nodes */
    err = tstr_array_append_str(ret_reply->crr_nodes,
		    name);
    bail_error(err);

 bail:
    return(err);
}

int
cli_nvsd_pe_show_cntrs(
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

	err = cli_printf("Policy engine evaluation counters:\n");
	bail_error(err);

	err = cli_printf_query(_("%-50s: " "#/nkn/nvsd/policy_engine/stats/tot_eval_http_recv_req# \n"),
			"HTTP receive request from client");
	bail_error(err);
	err = cli_printf_query(_("%-50s: " "#/nkn/nvsd/policy_engine/stats/tot_eval_http_send_resp# \n"),
			"HTTP send response to client");
	bail_error(err);
	err = cli_printf_query(_("%-50s: " "#/nkn/nvsd/policy_engine/stats/tot_eval_om_send_req# \n"),
			"HTTP send request to origin");
	bail_error(err);
	err = cli_printf_query(_("%-50s: " "#/nkn/nvsd/policy_engine/stats/tot_eval_om_recv_resp# \n"),
			"HTTP receive response from origin");
	bail_error(err);

	err = cli_printf("\nPolicy engine error counters:\n");
	bail_error(err);
	err = cli_printf_query(_("%-50s: " "#/nkn/nvsd/policy_engine/stats/tot_eval_err_http_recv_req# \n"),
			"HTTP receive request from client");
	bail_error(err);
	err = cli_printf_query(_("%-50s: " "#/nkn/nvsd/policy_engine/stats/tot_eval_err_http_send_resp# \n"),
			"HTTP send response to client");
	bail_error(err);
	err = cli_printf_query(_("%-50s: " "#/nkn/nvsd/policy_engine/stats/tot_eval_err_om_send_req# \n"),
			"HTTP send request to origin");
	bail_error(err);
	err = cli_printf_query(_("%-50s: " "#/nkn/nvsd/policy_engine/stats/tot_eval_err_om_recv_resp# \n"),
			"HTTP receive response from origin");
	bail_error(err);


bail:
	return err;
}

