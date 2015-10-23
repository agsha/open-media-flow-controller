/*
 * Filename :   cli_ssld_config_cmds.c
 * Date     :   2011/05/23
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
#include "file_utils.h"
/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/

typedef enum cli_ssld_term_type_en {
    SSLD_KEY_TERM_DT = 1,
    SSLD_CERT_TERM_DT,
    SSLD_CSR_TERM_DT,
}cli_ssld_term_type_t;

typedef struct cli_ssld_term_data_st {
   cli_ssld_term_type_t term_data_type;
   char *file_name;
   char *passphrase;
} cli_ssld_term_data_t;

#define OPENSSL	    "/usr/bin/openssl"

cli_expansion_option cli_ssld_cipher_opts[] = {
	{"des", N_("Specify the cipher suite as des"), (void *) "des"},
	{"des3", N_("Specify the cipher suite as des3"), (void *) "des3"},
	{"aes128", N_("Specify the cipher suite as aes128"), (void *) "aes128"},
	{"aes256", N_("Specify the cipher suite as ase256"), (void *) "aes256"},
	{NULL, NULL, NULL},
};


cli_expansion_option cli_ssld_vhost_cipher_opts[] = {
	{"DHE-RSA-AES256-SHA", N_("Specify the cipher as DHE-RSA-AES256-SHA"), (void *)"DHE-RSA-AES256-SHA"},
	{"DHE-DSS-AES256-SHA", N_("Specify the cipher as DHE-DSS-AES256-SHA"), (void *)"DHE-DSS-AES256-SHA"},
	{"AES256-SHA", N_("Specify the cipher as AES256-SHA"), (void *)"AES256-SHA"},
	{"EDH-RSA-DES-CBC3-SHA", N_("Specify the cipher as EDH-RSA-DES-CBC3-SHA"), (void *)"EDH-RSA-DES-CBC3-SHA"},
	{"EDH-DSS-DES-CBC3-SHA", N_("Specify the cipher as EDH-DSS-DES-CBC3-SHA"), (void *)"EDH-DSS-DES-CBC3-SHA"},
	{"DES-CBC3-SHA", N_("Specify the cipher as DES-CBC3-SHA"), (void *)"DES-CBC3-SHA"},
	{"DES-CBC3-MD5", N_("Specify the cipher as DES-CBC3-MD5"), (void *)"DES-CBC3-MD5"},
	{"DHE-RSA-AES128-SHA", N_("Specify the cipher as DHE-RSA-AES128-SHA"), (void *)"DHE-RSA-AES128-SHA"},
	{"DHE-DSS-AES128-SHA", N_("Specify the cipher as DHE-DSS-AES128-SHA"), (void *)"DHE-DSS-AES128-SHA"},
	{"AES128-SHA", N_("Specify the cipher as AES128-SHA"), (void *)"AES128-SHA"},
	{"RC2-CBC-MD5", N_("Specify the cipher as RC2-CBC-MD5"), (void *)"RC2-CBC-MD5"},
	{"RC4-SHA", N_("Specify the cipher as RC4-SHA"), (void *)"RC4-SHA"},
	{"RC4-MD5", N_("Specify the cipher as RC4-MD5"), (void *)"RC4-MD5"},
	{"RC4-MD5", N_("Specify the cipher as RC4-MD5"), (void *)"RC4-MD5"},
	{"EDH-RSA-DES-CBC-SHA", N_("Specify the cipher as EDH-RSA-DES-CBC-SHA"), (void *)"EDH-RSA-DES-CBC-SHA"},
	{"EDH-DSS-DES-CBC-SHA", N_("Specify the cipher as EDH-DSS-DES-CBC-SHA"), (void *)"EDH-DSS-DES-CBC-SHA"},
	{"DES-CBC-SHA", N_("Specify the cipher as DES-CBC-SHA"), (void *)"DES-CBC-SHA"},
	{"DES-CBC-MD5", N_("Specify the cipher as DES-CBC-MD5"), (void *)"DES-CBC-MD5"},
	{"EXP-EDH-RSA-DES-CBC-SHA", N_("Specify the cipher as EXP-EDH-RSA-DES-CBC-SHA"), (void *)"EXP-EDH-RSA-DES-CBC-SHA"},
	{"EXP-EDH-DSS-DES-CBC-SHA", N_("Specify the cipher as EXP-EDH-DSS-DES-CBC-SHA"), (void *)"EXP-EDH-DSS-DES-CBC-SHA"},
	{"EXP-DES-CBC-SHA", N_("Specify the cipher as EXP-DES-CBC-SHA"), (void *)"EXP-DES-CBC-SHA"},
	{"EXP-RC2-CBC-MD5", N_("Specify the cipher as EXP-RC2-CBC-MD5"), (void *)"EXP-RC2-CBC-MD5"},
	{"EXP-RC2-CBC-MD5", N_("Specify the cipher as EXP-RC2-CBC-MD5"), (void *)"EXP-RC2-CBC-MD5"},
	{"EXP-RC4-MD5", N_("Specify the cipher as EXP-RC4-MD5"), (void *)"EXP-RC4-MD5"},
	{"ECDHE-RSA-AES256-SHA", N_("Specify the cipher as ECDHE-RSA-AES256-SHA"), (void *)"ECDHE-RSA-AES256-SHA"},
	{"ECDHE-ECDSA-AES256-SHA", N_("Specify the cipher as ECDHE-ECDSA-AES256-SHA"), (void *)"ECDHE-ECDSA-AES256-SHA"},
	{"DHE-RSA-CAMELLIA256-SHA", N_("Specify the cipher as DHE-RSA-CAMELLIA256-SHA"), (void *)"DHE-RSA-CAMELLIA256-SHA"},
	{"DHE-DSS-CAMELLIA256-SHA", N_("Specify the cipher as DHE-DSS-CAMELLIA256-SHA"), (void *)"DHE-DSS-CAMELLIA256-SHA"},
	{"AECDH-AES256-SHA", N_("Specify the cipher as AECDH-AES256-SHA"), (void *)"AECDH-AES256-SHA"},
	{"ADH-AES256-SHA", N_("Specify the cipher as ADH-AES256-SHA"), (void *)"ADH-AES256-SHA"},
	{"ADH-CAMELLIA256-SHA", N_("Specify the cipher as ADH-CAMELLIA256-SHA"), (void *)"ADH-CAMELLIA256-SHA"},
	{"ECDH-RSA-AES256-SHA", N_("Specify the cipher as ECDH-RSA-AES256-SHA"), (void *)"ECDH-RSA-AES256-SHA"},
	{"ECDH-ECDSA-AES256-SHA", N_("Specify the cipher as ECDH-ECDSA-AES256-SHA"), (void *)"ECDH-ECDSA-AES256-SHA"},
	{"CAMELLIA256-SHA", N_("Specify the cipher as CAMELLIA256-SHA"), (void *)"CAMELLIA256-SHA"},
	{"PSK-AES256-CBC-SHA", N_("Specify the cipher as PSK-AES256-CBC-SHA"), (void *)"PSK-AES256-CBC-SHA"},
	{"ECDHE-RSA-DES-CBC3-SHA", N_("Specify the cipher as ECDHE-RSA-DES-CBC3-SHA"), (void *)"ECDHE-RSA-DES-CBC3-SHA"},
	{"ECDHE-ECDSA-DES-CBC3-SHA", N_("Specify the cipher as ECDHE-ECDSA-DES-CBC3-SHA"), (void *)"ECDHE-ECDSA-DES-CBC3-SHA"},
	{"AECDH-DES-CBC3-SHA", N_("Specify the cipher as AECDH-DES-CBC3-SHA"), (void *)"AECDH-DES-CBC3-SHA"},
	{"ADH-DES-CBC3-SHA", N_("Specify the cipher as ADH-DES-CBC3-SHA"), (void *)"ADH-DES-CBC3-SHA"},
	{"ECDH-RSA-DES-CBC3-SHA", N_("Specify the cipher as ECDH-RSA-DES-CBC3-SHA"), (void *)"ECDH-RSA-DES-CBC3-SHA"},
	{"ECDH-ECDSA-DES-CBC3-SHA", N_("Specify the cipher as ECDH-ECDSA-DES-CBC3-SHA"), (void *)"ECDH-ECDSA-DES-CBC3-SHA"},
	{"PSK-3DES-EDE-CBC-SHA", N_("Specify the cipher as PSK-3DES-EDE-CBC-SHA"), (void *)"PSK-3DES-EDE-CBC-SHA"},
	{"ECDHE-RSA-AES128-SHA", N_("Specify the cipher as ECDHE-RSA-AES128-SHA"), (void *)"ECDHE-RSA-AES128-SHA"},
	{"ECDHE-ECDSA-AES128-SHA", N_("Specify the cipher as ECDHE-ECDSA-AES128-SHA"), (void *)"ECDHE-ECDSA-AES128-SHA"},
	{"DHE-RSA-SEED-SHA", N_("Specify the cipher as DHE-RSA-SEED-SHA"), (void *)"DHE-RSA-SEED-SHA"},
	{"DHE-DSS-SEED-SHA", N_("Specify the cipher as DHE-DSS-SEED-SHA"), (void *)"DHE-DSS-SEED-SHA"},
	{"DHE-RSA-CAMELLIA128-SHA", N_("Specify the cipher as DHE-RSA-CAMELLIA128-SHA"), (void *)"DHE-RSA-CAMELLIA128-SHA"},
	{"DHE-DSS-CAMELLIA128-SHA", N_("Specify the cipher as DHE-DSS-CAMELLIA128-SHA"), (void *)"DHE-DSS-CAMELLIA128-SHA"},
	{"AECDH-AES128-SHA", N_("Specify the cipher as AECDH-AES128-SHA"), (void *)"AECDH-AES128-SHA"},
	{"ADH-AES128-SHA", N_("Specify the cipher as ADH-AES128-SHA"), (void *)"ADH-AES128-SHA"},
	{"ADH-SEED-SHA", N_("Specify the cipher as ADH-SEED-SHA"), (void *)"ADH-SEED-SHA"},
	{"ADH-CAMELLIA128-SHA", N_("Specify the cipher as ADH-CAMELLIA128-SHA"), (void *)"ADH-CAMELLIA128-SHA"},
	{"ECDH-RSA-AES128-SHA", N_("Specify the cipher as ECDH-RSA-AES128-SHA"), (void *)"ECDH-RSA-AES128-SHA"},
	{"ECDH-ECDSA-AES128-SHA", N_("Specify the cipher as ECDH-ECDSA-AES128-SHA"), (void *)"ECDH-ECDSA-AES128-SHA"},
	{"SEED-SHA", N_("Specify the cipher as SEED-SHA"), (void *)"SEED-SHA"},
	{"CAMELLIA128-SHA", N_("Specify the cipher as CAMELLIA128-SHA"), (void *)"CAMELLIA128-SHA"},
	{"IDEA-CBC-SHA", N_("Specify the cipher as IDEA-CBC-SHA"), (void *)"IDEA-CBC-SHA"},
	{"IDEA-CBC-MD5", N_("Specify the cipher as IDEA-CBC-MD5"), (void *)"IDEA-CBC-MD5"},
	{"PSK-AES128-CBC-SHA", N_("Specify the cipher as PSK-AES128-CBC-SHA"), (void *)"PSK-AES128-CBC-SHA"},
	{"ECDHE-RSA-RC4-SHA", N_("Specify the cipher as ECDHE-RSA-RC4-SHA"), (void *)"ECDHE-RSA-RC4-SHA"},
	{"ECDHE-ECDSA-RC4-SHA", N_("Specify the cipher as ECDHE-ECDSA-RC4-SHA"), (void *)"ECDHE-ECDSA-RC4-SHA"},
	{"AECDH-RC4-SHA", N_("Specify the cipher as AECDH-RC4-SHA"), (void *)"AECDH-RC4-SHA"},
	{"ADH-RC4-MD5", N_("Specify the cipher as ADH-RC4-MD5"), (void *)"ADH-RC4-MD5"},
	{"ECDH-RSA-RC4-SHA", N_("Specify the cipher as ECDH-RSA-RC4-SHA"), (void *)"ECDH-RSA-RC4-SHA"},
	{"ECDH-ECDSA-RC4-SHA", N_("Specify the cipher as ECDH-ECDSA-RC4-SHA"), (void *)"ECDH-ECDSA-RC4-SHA"},
	{"PSK-RC4-SHA", N_("Specify the cipher as PSK-RC4-SHA"), (void *)"PSK-RC4-SHA"},
	{"ADH-DES-CBC-SHA", N_("Specify the cipher as ADH-DES-CBC-SHA"), (void *)"ADH-DES-CBC-SHA"},
	{"EXP-ADH-DES-CBC-SHA", N_("Specify the cipher as EXP-ADH-DES-CBC-SHA"), (void *)"EXP-ADH-DES-CBC-SHA"},
	{"EXP-ADH-RC4-MD5", N_("Specify the cipher as EXP-ADH-RC4-MD5"), (void *)"EXP-ADH-RC4-MD5"},
	{"NULL-SHA", N_("Specify the cipher as NULL-SHA"), (void *)"NULL-SHA"},
	{"NULL-MD5", N_("Specify the cipher as NULL-MD5"), (void *)"NULL-MD5"},
	{"eNULL", N_("Specify the cipher as eNULL"), (void *)"eNULL"},
	{"aNULL", N_("Specify the cipher as aNULL"), (void *)"aNULL"},
	{"ALL", N_("Specify the cipher as ALL"), (void *)"ALL"},
	{"DEFAULT", N_("Specify the cipher as DEFAULT"), (void *)"DEFAULT"},
	{NULL, NULL, NULL},
};

int
cli_show_https_counters_cb(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_ssld_cfg_help(
		void *data,
		cli_help_type help_type,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params,
		const tstring *curr_word,
		void *context);

int
cli_ssld_cfg_cmds_init(
		const lc_dso_info *info,
		const cli_module_context *context);

int
cli_ssld_cfg_import(void *data,
		const cli_command       *cmd,
		const tstr_array        *cmd_line,
		const tstr_array        *params);

int
cli_ssld_cfg_create_hdlr(void *data,
		const cli_command       *cmd,
		const tstr_array        *cmd_line,
		const tstr_array        *params);

int
cli_ssld_cfg_del_hdlr(void *data,
		const cli_command       *cmd,
		const tstr_array        *cmd_line,
		const tstr_array        *params);
int
cli_ssld_cfg_export(void *data,
		const cli_command       *cmd,
		const tstr_array        *cmd_line,
		const tstr_array        *params);

int
cli_ssld_cfg_ls(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_ssld_cfg_show_cipher(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
int
cli_ssld_cfg_unbind(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_ssld_cfg_show_one(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

/* APIs to restrict feature based on license */
static int
cli_ssl_handle_license_change(const char *event_name,
                                   const bn_binding_array *bindings,
                                   void *data);
static int
cli_ssl_cmds_handle_change(const bn_binding_array *old_bindings,
                                const bn_binding_array *new_bindings,
                                void *data);
static int
cli_ssl_cmds_set_flags(const tstr_array *cmds, tbool allowed,
                            cli_command_flags flag);

static int
cli_ssl_cmds_update(tbool allowed, tbool mgmtd_avail);

    int
cli_ssl_restrict_cmds_init(const lc_dso_info *info,
                      const cli_module_context *context);

int
cli_ssld_show_counter_cmds(
		const lc_dso_info *info,
		const cli_module_context *context);

int
cli_ssld_show_counters_internal_cb(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_ssld_cfg_cipher_text(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
/*----------------------------------------------------------------------------
 * LOCAL FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static int
cli_ssld_term_handler(void *data);

static int
cli_ssld_new_key(const tstr_array *params, const tstr_array *cmd_line);

static int
cli_ssld_new_csr(const tstr_array *params, const tstr_array *cmd_line);

static int
cli_ssld_new_cert(const tstr_array *params, const tstr_array *cmd_line);

static int
cli_ssld_cfg_revmap (void *data, const cli_command *cmd,
		const bn_binding_array *bindings,
		const char *name,
		const tstr_array *name_parts,
		const char *value, const bn_binding *binding,
		cli_revmap_reply *ret_reply);

static int
cli_ssld_cfg_import_finish(const char *password, clish_password_result result,
				const cli_command *cmd, const tstr_array *cmd_line,
				const tstr_array *params, void *data,
				tbool *ret_continue);
/* May be used later */
enum {
	cc_ssld_cert = 1,
	cc_ssld_key,
	cc_ssld_csr,
	cc_ssld_ca,
};

int
cli_ssld_cfg_get_items(const char *nd_pattern,
		tstr_array **ret_name);

int
cli_ssld_cfg_is_valid_item(const char *node, const char *match_val,
				    tbool *is_valid);

static int
cli_ssld_cfg_export_finish(const char *password, 
			    clish_password_result result,
			    const cli_command *cmd, 
			    const tstr_array *cmd_line,
			    const tstr_array *params, 
			    void *data,
			    tbool *ret_continue);

static int
cli_ssld_set_timestamp(const char *item, const char *type);

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/


int
cli_ssld_cfg_cmds_init(
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


	err = cli_ssld_show_counter_cmds(info, context);
	bail_error(err);

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl";
	cmd->cc_help_descr =        N_("Configure SSL parameters");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl client-authentication";
	cmd->cc_help_descr =        N_("Configure SSL client-side authentication parameters");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl client-authentication enable";
	cmd->cc_help_descr =        N_("Enable client-side authentication for SSL");
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/ssld/config/client-req/client_auth";
	cmd->cc_exec_value =        "true";
	cmd->cc_exec_type = 	    bt_bool;
	cmd->cc_revmap_order =      cro_ssl;
	cmd->cc_revmap_type =	    crt_auto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl client-authentication disable";
	cmd->cc_help_descr =        N_("Disable client-side authentication for SSL");
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_exec_operation =    cxo_reset;
	cmd->cc_exec_name =         "/nkn/ssld/config/client-req/client_auth";
	cmd->cc_revmap_type =	    crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl server-authentication";
	cmd->cc_help_descr =        N_("Configure SSL server-side authentication parameters");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl server-authentication enable";
	cmd->cc_help_descr =        N_("Enable server-side authentication for SSL");
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/ssld/config/origin-req/server_auth";
	cmd->cc_exec_value =        "true";
	cmd->cc_exec_type = 	    bt_bool;
	cmd->cc_revmap_order =      cro_ssl;
	cmd->cc_revmap_type =	    crt_auto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl server-authentication disable";
	cmd->cc_help_descr =        N_("Disable server-side authentication for SSL");
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_exec_operation =    cxo_reset;
	cmd->cc_exec_name =         "/nkn/ssld/config/origin-req/server_auth";
	cmd->cc_revmap_type =	    crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key";
	cmd->cc_help_descr =        N_("Configure SSL key parameters");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key *";
	cmd->cc_help_exp =          N_("<key Name>");
	cmd->cc_help_exp_hint =     N_("Key File Name");
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/key/*";
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/config/key";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key * create";
	cmd->cc_help_descr =        N_("Create SSL key file");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key * create cipher";
	cmd->cc_help_descr =        N_("Configure SSL cipher suite");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key * create cipher *";
	cmd->cc_help_exp =	    N_("<Cipher>");
	cmd->cc_help_options =      cli_ssld_cipher_opts;
	cmd->cc_help_num_options =  sizeof(cli_ssld_cipher_opts)/sizeof(cli_expansion_option);
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key * create cipher * length";
	cmd->cc_help_descr =        N_("Length of the key(e.x: 128, 512, 1024)");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key * create cipher * length *";
	cmd->cc_help_exp =	    N_("<Length of the key>");
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key * create cipher * length * *";
	cmd->cc_flags =             ccf_terminal | ccf_sensitive_param;
	cmd->cc_help_exp =	    N_("<Passhrase>");
	cmd->cc_help_exp_hint =     N_("Passphrase for key file");
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_magic =		    cc_ssld_key;
	cmd->cc_exec_callback =	    cli_ssld_cfg_create_hdlr;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key * import";
	cmd->cc_help_descr =        N_("Import ssl key");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key * import *";
	cmd->cc_flags =             ccf_sensitive_param | ccf_ignore_length;
	cmd->cc_help_exp =          N_("<scp://username:password@hostname/path>");
	cmd->cc_help_exp_hint =     N_("Specify SCP path to import");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key * import * *";
	cmd->cc_help_exp =          N_("<PASSPHRASE>");
	cmd->cc_help_exp_hint =     N_("Passphrase of the key file");
	cmd->cc_flags =             ccf_terminal | ccf_sensitive_param | ccf_ignore_length;
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_import;
	cmd->cc_magic =		    cc_ssld_key;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key * export";
	cmd->cc_help_descr =        N_("Export key to an external server");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key * export *";
	cmd->cc_help_exp =          N_("<scp://username:password@hostname/path>");
	cmd->cc_help_exp_hint =     N_("specify SCP path to import");
	cmd->cc_flags =             ccf_terminal | ccf_sensitive_param | ccf_ignore_length;
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_export;
	cmd->cc_magic =		    cc_ssld_key;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl key * remove";
	cmd->cc_help_descr =     N_("Remove SSL key file");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_del_hdlr;
	cmd->cc_magic =		    cc_ssld_key;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl csr";
	cmd->cc_help_descr =        N_("Configure certificate signing request");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl csr *";
	cmd->cc_help_exp =          N_("<CSR Name>");
	cmd->cc_help_exp_hint =     N_("Certificate signing request Name");
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/csr/*";
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/config/csr";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl csr * create";
	cmd->cc_help_descr =        N_("Create Certificate Signing Request(CSR)");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl csr * create input-key";
	cmd->cc_help_descr =        N_("Configure SSL key file for Certificate Signing Request(CSR)");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl csr * create input-key *";
	cmd->cc_help_exp =	    N_("<Key Name>");
	cmd->cc_help_exp_hint =     N_("Key file name");
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/key/*";
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/config/key";

	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl csr * create input-key * *";
	cmd->cc_flags =             ccf_terminal | ccf_sensitive_param;
	cmd->cc_help_exp =	    N_("<Passphrase>");
	cmd->cc_help_exp_hint =     N_("Passphrase for key file");
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_magic =		    cc_ssld_csr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_create_hdlr;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl csr * import";
	cmd->cc_help_descr =        N_("Import SSL CSR file");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl csr * import *";
	cmd->cc_help_exp =          N_("<scp://username:password@hostname/path>");
	cmd->cc_help_exp_hint =     N_("Specify SCP path to import");
	cmd->cc_flags =             ccf_terminal | ccf_sensitive_param | ccf_ignore_length;
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_magic =		    cc_ssld_csr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_import;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl csr * export";
	cmd->cc_help_descr =        N_("Export CSR file to external server");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl csr * export *";
	cmd->cc_help_exp =          N_("<scp://username:password@hostname/path>");
	cmd->cc_help_exp_hint =     N_("specify SCP path to import");
	cmd->cc_flags =             ccf_terminal | ccf_sensitive_param | ccf_ignore_length;
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_export;
	cmd->cc_magic =		    cc_ssld_csr;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl csr * remove";
	cmd->cc_help_descr =	    N_("Remove SSL CSR file");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_del_hdlr;
	cmd->cc_magic =		    cc_ssld_csr;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate";
	cmd->cc_help_descr =        N_("Configure SSL certificate parameters");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate *";
	cmd->cc_help_exp =          N_("<Certificate Name>");
	cmd->cc_help_exp_hint =     N_("Change Certifiacte parameters");
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/certificate/*";
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/config/certificate";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate * create";
	cmd->cc_help_descr =        N_("Create SSL certificate");
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate * create validity";
	cmd->cc_help_descr =        N_("SSL certificate validity parameters");
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate * create validity *";
	cmd->cc_help_exp =          N_("<days>");
	cmd->cc_help_exp_hint =     N_("Number of days of validity");
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

#if 0
	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate * create validity * *";
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_help_exp =          N_("<passphrase>");
	cmd->cc_help_exp_hint =        N_("Create SSL certificate");
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_magic =		    cc_ssld_cert;
	cmd->cc_exec_callback =	    cli_ssld_cfg_create_hdlr;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;
#endif

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate * create validity * input-key";
	cmd->cc_help_descr =        N_("Key file for certificate generation");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate * create validity * input-key *";
	cmd->cc_help_exp =          N_("<Key Name>");
	cmd->cc_help_exp_hint =     N_("Key file name");
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/key/*";
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/config/key";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate * create validity * input-key * *";
	cmd->cc_flags =             ccf_terminal| ccf_sensitive_param | ccf_ignore_length;
	cmd->cc_help_exp =          N_("<passphrase>");
	cmd->cc_help_exp_hint =     N_("Pass phrase for key file");
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_magic =		    cc_ssld_cert;
	cmd->cc_exec_callback =	    cli_ssld_cfg_create_hdlr;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate * import";
	cmd->cc_help_descr =        N_("Import ssl certificate");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate * import *";
	cmd->cc_flags =             ccf_terminal | ccf_sensitive_param | ccf_ignore_length;
	cmd->cc_help_exp =          N_("<scp://username:password@hostname/path>");
	cmd->cc_help_exp_hint =     N_("Specify SCP path to import");
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_magic =		    cc_ssld_cert;
	cmd->cc_exec_callback =	    cli_ssld_cfg_import;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate * export";
	cmd->cc_help_descr =        N_("Export SSL certificate to an external server");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate * export *";
	cmd->cc_help_exp =          N_("<scp://username:password@hostname/path>");
	cmd->cc_help_exp_hint =     N_("specify SCP path to import");
	cmd->cc_flags =             ccf_terminal | ccf_sensitive_param | ccf_ignore_length;
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_export;
	cmd->cc_magic =		    cc_ssld_cert;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl ca-certificate";
	cmd->cc_help_descr =        N_("Configure SSL ca-certificate parameters");
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl ca-certificate *";
	cmd->cc_help_exp =          N_("<ca-certificate Name>");
	cmd->cc_help_exp_hint =     N_("CA-Certifiacte Name with .pem extension");
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/cafiles/*";
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/cafiles";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl ca-certificate * import";
	cmd->cc_help_descr =        N_("Import server CA-Certificate certificate");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl ca-certificate * import *";
	cmd->cc_flags =             ccf_terminal | ccf_sensitive_param | ccf_ignore_length;
	cmd->cc_help_exp =          N_("<scp://username:password@hostname/path>");
	cmd->cc_help_exp_hint =     N_("Specify SCP path to import");
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_import;
	cmd->cc_magic =		    cc_ssld_ca;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl ca-certificate * export";
	cmd->cc_help_descr =        N_("Export ca-certificate to an external entity");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl ca-certificate * export *";
	cmd->cc_help_exp =          N_("<scp://username:password@hostname/path>");
	cmd->cc_help_exp_hint =     N_("specify SCP path to export");
	cmd->cc_flags =             ccf_terminal | ccf_sensitive_param | ccf_ignore_length;
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_export;
	cmd->cc_magic =		    cc_ssld_ca;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl ca-certificate * remove";
	cmd->cc_help_descr =     N_("specify the ca-certificate to be removed");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_del_hdlr;
	cmd->cc_magic =		    cc_ssld_ca;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

#if 0
	CLI_CMD_NEW;
	cmd->cc_words_str =         "no ssl";
	cmd->cc_help_descr =        N_("Remove SSL Configurations");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl default certificate";
	cmd->cc_help_descr =        N_("Set SSL default certificate");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl default certificate *";
	cmd->cc_help_exp =          N_("<cert name>");
	cmd->cc_help_exp_hint =     N_("specify certifiacte to be set as default ");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/ssld/config/default/certificate";
	cmd->cc_exec_value =        "$1$";
	cmd->cc_exec_type =	    bt_string;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/certificate/*";
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/config/certificate";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl default key";
	cmd->cc_help_descr =        N_("Set SSL default key");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl default key *";
	cmd->cc_help_exp =          N_("<key name>");
	cmd->cc_help_exp_hint =     N_("specify key to be set as default ");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/ssld/config/default/key";
	cmd->cc_exec_value =        "$1$";
	cmd->cc_exec_type =	    bt_string;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/key/*";
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/config/key";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

#endif
	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl cipher";
	cmd->cc_help_descr =        N_("Cipher to be used for handshaking with SSL origin server");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl cipher *";
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_help_exp =          N_("<cipher>");
	cmd->cc_help_exp_hint =     N_("specify the cipher");
	cmd->cc_help_options =      cli_ssld_vhost_cipher_opts;
	cmd->cc_help_num_options =  sizeof(cli_ssld_vhost_cipher_opts)/sizeof(cli_expansion_option);
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/ssld/config/origin-req/cipher";
	cmd->cc_exec_value =        "$1$";
	cmd->cc_exec_type =	    bt_string;
	cmd->cc_revmap_type =       crt_auto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "ssl certificate * remove";
	cmd->cc_help_descr =     N_("Remove SSL certificate ");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_action_rstr_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_del_hdlr;
	cmd->cc_magic =		    cc_ssld_cert;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "virtual-host";
	cmd->cc_help_descr =        N_("Virtual host settings for SSL");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "virtual-host *";
	cmd->cc_help_exp =          N_("<domain>");
	cmd->cc_help_exp_hint =     N_("Specify the domain (ex., example.com");
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/config/virtual_host";
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/virtual_host/*";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "virtual-host * cipher";
	cmd->cc_help_descr =        N_("cipher settings for domain");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "virtual-host * cipher *";
	cmd->cc_help_exp =          N_("<cipher>");
	cmd->cc_help_exp_hint =     N_("Specify the cipher. Enter multiple cipher seperated by \":\"\n To negate a cipher use \"!\"");
	cmd->cc_flags =		    ccf_terminal | ccf_ignore_length;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_help_options =      cli_ssld_vhost_cipher_opts;
	cmd->cc_help_num_options =  sizeof(cli_ssld_vhost_cipher_opts)/sizeof(cli_expansion_option);
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/ssld/config/virtual_host/$1$/cipher";
//	cmd->cc_exec_callback =     cli_ssld_cfg_cipher_text;
	cmd->cc_exec_value =        "$2$";
	cmd->cc_exec_type =	    bt_string;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "virtual-host * certificate";
	cmd->cc_help_descr =        N_("Specify the certificate to bind with the domain");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "virtual-host * certificate bind";
	cmd->cc_help_descr =        N_("specify the certificate to bind");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "virtual-host * certificate bind *";
	cmd->cc_help_exp =          N_("<cert-name>");
	cmd->cc_help_exp_hint =     N_("specify the certificate to bind");
	//cmd->cc_capab_required =    ccp_set_rstr_curr;
	//cmd->cc_flags =             ccf_terminal;
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/config/certificate";
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/certificate/*";
	cmd->cc_revmap_type = crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "virtual-host * certificate bind * key";
	cmd->cc_help_descr =        N_("specify the key file to bind to the domain");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "virtual-host * certificate bind * key bind";
	cmd->cc_help_descr =        N_("specify the key to bind");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "virtual-host * certificate bind * key bind *";
	cmd->cc_help_exp =          N_("<key-name>");
	cmd->cc_help_exp_hint =     N_("Key file name");
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/config/key";
	cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_name =         "/nkn/ssld/config/virtual_host/$1$/key";
	cmd->cc_exec_value =        "$3$";
	cmd->cc_exec_type = 	    bt_string;
	cmd->cc_exec_name2 =         "/nkn/ssld/config/virtual_host/$1$/cert";
	cmd->cc_exec_value2 =        "$2$";
	cmd->cc_exec_type2 = 	    bt_string;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/key/*";
	cmd->cc_revmap_callback =    cli_ssld_cfg_revmap;
	cmd->cc_revmap_names =      "/nkn/ssld/config/virtual_host/*";
	cmd->cc_revmap_order =      cro_ssl;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no virtual-host";
	cmd->cc_help_descr =        N_("specify the domain to be removed");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no virtual-host *";
	cmd->cc_help_exp =          N_("<domain>");
	cmd->cc_help_exp_hint =     N_("specify the domain to be removed");
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/config/virtual_host";
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
	cmd->cc_help_exp_hint =     N_("Specify the domain");
	cmd->cc_comp_type =         cct_matching_names;
	cmd->cc_exec_operation =    cxo_delete;
	cmd->cc_exec_name =        "/nkn/ssld/config/virtual_host/$1$";
        cmd->cc_comp_pattern =      "/nkn/ssld/config/virtual_host/*";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no virtual-host * cipher";
	cmd->cc_help_descr =        N_("Reset virtual-host cipher settings");
	cmd->cc_flags =		    ccf_terminal;
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_exec_operation =    cxo_reset;
	cmd->cc_exec_name =         "/nkn/ssld/config/virtual_host/$1$/cipher";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no virtual-host * certificate";
	cmd->cc_help_descr =        N_("specify the certificate to un-bind");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no virtual-host * certificate bind";
	cmd->cc_help_descr =        N_("specify the certificate to un-bind");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no virtual-host * certificate bind *";
	cmd->cc_help_exp =          N_("<key-name>");
	cmd->cc_help_exp_hint =     N_("specify the certificate to un-bind");
//	cmd->cc_capab_required =    ccp_set_rstr_curr;
//	cmd->cc_flags =             ccf_terminal;
//	cmd->cc_exec_operation =    cxo_delete;
//	cmd->cc_exec_callback =     cli_ssld_cfg_unbind;
//	cmd->cc_magic =		    cc_ssld_cert;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/certificate/*";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no virtual-host * certificate bind * key";
	cmd->cc_help_descr =        N_("specify the key to un-bind");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no virtual-host * certificate bind * key bind";
	cmd->cc_help_descr =        N_("specify the key  to un-bind");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "no virtual-host * certificate bind * key bind *";
	cmd->cc_help_exp =          N_("<key-name>");
	cmd->cc_help_exp_hint =     N_("specify the key to un-bind");
	cmd->cc_capab_required =    ccp_set_rstr_curr;
	cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
	cmd->cc_exec_callback =     cli_ssld_cfg_unbind;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/key/*";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show ssl";
	cmd->cc_help_descr =        N_("show SSL settings");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show ssl certificate";
	cmd->cc_help_descr =        N_("show SSL certificate settings");
	CLI_CMD_REGISTER;

#if 0
	CLI_CMD_NEW;
	cmd->cc_words_str =         "show ssl certificate list";
	cmd->cc_help_descr =	    "List all the certificates";
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_query_basic_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_ls;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;
#endif

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show ssl certificate *";
	cmd->cc_help_exp =          N_("<cert-name>");
	cmd->cc_help_exp_hint =     N_("specify the certificate to display");
	cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/config/certificate";
	cmd->cc_magic =		    cc_ssld_cert;
	cmd->cc_capab_required =    ccp_query_basic_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_show_one;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/certificate/*";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show ssl ca-certificate";
	cmd->cc_help_descr =        N_("show SSL ca-certificate settings");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show ssl ca-certificate *";
	cmd->cc_help_exp =          N_("<cert-name>");
	cmd->cc_help_exp_hint =     N_("specify the ca-certificate to display");
	cmd->cc_flags =             ccf_terminal | ccf_ignore_length;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/cafiles/*";
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/cafiles";
	cmd->cc_capab_required =    ccp_query_basic_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_show_one;
	cmd->cc_magic =		    cc_ssld_ca;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show virtual-host";
	cmd->cc_help_descr =        N_("show virtual-host settings");
	CLI_CMD_REGISTER;

#if 0
	CLI_CMD_NEW;
	cmd->cc_words_str =         "show virtual-host *";
	cmd->cc_help_exp =          N_("<host-name>");
	cmd->cc_help_exp_hint =     N_("specify virtual-host settings");
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_help_callback =     cli_ssld_cfg_help;
	cmd->cc_help_data =	    (void*)"/nkn/ssld/config/virtual_host";
	cmd->cc_capab_required =    ccp_query_basic_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_show_one;
	cmd->cc_comp_type =         cct_matching_names;
        cmd->cc_comp_pattern =      "/nkn/ssld/config/virtual_host/*";
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;
#endif

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show virtual-host list";
	cmd->cc_help_descr =	    "List all the virtual-host";
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_query_basic_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_ls;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =         "show ssl cipher";
	cmd->cc_help_descr =	    "List Configured Cipher";
	cmd->cc_flags =             ccf_terminal;
	cmd->cc_capab_required =    ccp_query_basic_curr;
	cmd->cc_exec_callback =	    cli_ssld_cfg_show_cipher;
	cmd->cc_revmap_type =       crt_none;
	CLI_CMD_REGISTER;

	err = cli_revmap_ignore_bindings_va(9,
			"/nkn/ssld/config/certificate/**",
			"/nkn/ssld/config/virtual_host/**",
			"/nkn/ssld/config/key/**",
			"/nkn/ssld/config/ca-certificate/**",
			"/nkn/ssld/config/csr/**",
			"/nkn/mfd/licensing/config/ssl_licensed",
			"/nkn/ssld/config/client-req/client_auth",
			"/nkn/ssld/config/origin-req/cipher",
			"/nkn/ssld/config/origin-req/server_auth");
	bail_error(err);


	err = cli_ssl_restrict_cmds_init(info, context);
	bail_error(err);

bail:
	return err;
}


int
cli_ssld_show_counter_cmds(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str           =    "show counters https";
    cmd->cc_help_descr          =    N_("Display HTTPS counters");
    cmd->cc_flags 		=    ccf_terminal ;
    cmd->cc_capab_required 	=    ccp_query_basic_curr;
    cmd->cc_exec_callback 	=    cli_show_https_counters_cb;
    cmd->cc_exec_data 		=    NULL;
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str           =    "show counters internal ssl";
    cmd->cc_flags 		=    ccf_hidden;
    cmd->cc_help_descr          =    N_("Display ssl internal counters");
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str 		=    "show counters internal ssl";
    cmd->cc_help_descr          =    N_("Display SSL debug counters for advance debugging");
    cmd->cc_flags 		=    ccf_terminal;
    cmd->cc_capab_required 	=    ccp_query_basic_curr;
    cmd->cc_exec_callback 	=    cli_ssld_show_counters_internal_cb;
    cmd->cc_exec_data 		=    NULL;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           =    "show counters internal ssl *";
    cmd->cc_help_exp            =    N_("<counter-pattern>");
    cmd->cc_help_exp_hint       =    N_("Specify a pattern for the counter to be displayed(for ex., glob_)");
    cmd->cc_flags               =    ccf_terminal;
    cmd->cc_capab_required      =    ccp_query_basic_curr;
    cmd->cc_exec_callback       =    cli_ssld_show_counters_internal_cb;
    cmd->cc_exec_data           =    NULL;
    CLI_CMD_REGISTER;

bail:
    return err;
}

int
cli_show_https_counters_cb(
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

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ssld/client/tot_open_sock# \n"),
	    "Total number of Client HTTPS Connections");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/https/tot_transactions# \n"),
            "Total number of Client HTTPS Transactions");
    bail_error(err);

    // TODO Collect statistics on port basis

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ssld/client/cur_ssl_sock# \n"),
	    "Total number of Client Open HTTPS Connections");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ssld/client/tot_size_sent# \n"),
	    "Total number of Bytes sent to Client");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ssld/client/tot_size_from# \n"),
	    "Total number of Bytes received from Client");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ssld/client/handshake_success# \n"),
	    "Total number of Client handshake success Count");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ssld/origin/cur_sock# \n"),
	    "Total number of Origin Current Open Sockets");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ssld/origin/tot_sock# \n"),
	    "Total number of Origin  Sockets");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ssld/origin/data_recv# \n"),
	    "Total number of bytes received from Origin");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ssld/origin/data_sent# \n"),
	    "Total number of bytes sent to Origin");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ssld/origin/handshake_failure# \n"),
	    "Total number of Origin handshake failures");
    bail_error(err);

    err = cli_printf_query(_("%-50s: " "#/stat/nkn/ssld/origin/handshake_success# \n"),
	    "Total number of Origin handshake Success Count");
    bail_error(err);

bail:
    return err;
}


int
cli_ssld_show_counters_internal_cb(
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

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(params);

	/* Get the number of parameters first */
	num_params = tstr_array_length_quick(cmd_line);
	lc_log_basic(LOG_DEBUG, _("Num params:%d\n"), num_params);

	err = tstr_array_new ( &argv , NULL);
	bail_error (err);
	err = tstr_array_append_str (argv, "/opt/nkn/bin/nkncnt");
	bail_error (err);
	err = tstr_array_append_str (argv, "-P");
	bail_error (err);
	err = tstr_array_append_str (argv, "/opt/nkn/sbin/ssld");
	bail_error (err);

	if(num_params == 4) {  /*sh counters ssl*/
		/*Just the global level stats is needed*/
		err = tstr_array_append_str (argv, "-s");
		bail_error (err);
		err = tstr_array_append_str (argv, "");
		bail_error (err);
	} else if (num_params == 5) { /*sh counters ssl <counter-name>*/
		grep_pattern = tstr_array_get_str_quick(cmd_line, 4);
		bail_null(grep_pattern);

		err = tstr_array_append_str (argv, "-s");
		bail_error (err);
		err = tstr_array_append_str (argv, grep_pattern);
		bail_error (err);


	}
	err = tstr_array_append_str (argv, "-t");
	bail_error (err);
	err = tstr_array_append_str (argv, "0");
	bail_error (err);

	err = nkn_clish_run_program_fg("/opt/nkn/bin/nkncnt", argv, NULL, NULL, NULL);
	bail_error(err);

bail:
	safe_free(name);
	tstr_array_free(&argv);
	return err;
}

int
cli_ssld_cfg_unbind(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *cert_name = NULL;
	const char *key_name = NULL;
	const char *vhost_name = NULL;
	const char *nd_name = NULL;
	const char *err_msg = NULL;
	tbool is_valid = false;
	node_name_t item_node = {0};
	bn_request *req = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	cert_name = tstr_array_get_str_quick(params, 1);
	bail_null(cert_name);

	key_name = tstr_array_get_str_quick(params, 2);
	bail_null(key_name);

	vhost_name = tstr_array_get_str_quick(params, 0);
	bail_null(vhost_name);

	err = bn_set_request_msg_create(&req, 0);
	bail_error(err);

	nd_name = "/nkn/ssld/config/virtual_host/%s/cert";
	err_msg = "Invalid certificate entry\n";

	snprintf(item_node, sizeof(item_node), nd_name,
			vhost_name);

	err = cli_ssld_cfg_is_valid_item(item_node, cert_name, &is_valid);
	bail_error(err);

	if(!is_valid) {
		cli_printf_error("%s", err_msg);
		goto bail;
	}

	err = bn_set_request_msg_append_new_binding
		(req, bsso_reset, 0, item_node, bt_none, btf_deleted, NULL, NULL);
	bail_error(err);

	nd_name = "/nkn/ssld/config/virtual_host/%s/key";
	err_msg = "Invalid key entry\n";
	is_valid = false;

	snprintf(item_node, sizeof(item_node), nd_name,
			vhost_name);

	err = cli_ssld_cfg_is_valid_item(item_node, key_name, &is_valid);
	bail_error(err);

	if(!is_valid) {
		cli_printf_error("%s", err_msg);
		goto bail;
	}

	err = bn_set_request_msg_append_new_binding
		(req, bsso_reset, 0, item_node, bt_none, btf_deleted, NULL, NULL);
	bail_error(err);

	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
	bail_error(err);

bail:
	bn_request_msg_free(&req);
	return err;
}


int
cli_ssld_cfg_show_one(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *cert_name = NULL;
	file_path_t cert_path = {0};
	int32_t ret_status = 0;
	tstring *output = NULL;
	tbool is_valid = false;
	node_name_t cert_node = {0};

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	cert_name = tstr_array_get_str_quick(params, 0);
	bail_null(cert_name);

	switch(cmd->cc_magic) {
		case cc_ssld_cert:
			snprintf(cert_node, sizeof(cert_node), "/nkn/ssld/config/certificate/%s",
					cert_name);
			snprintf(cert_path, sizeof(cert_path), "%s%s", SSLD_CERT_PATH,
					cert_name);
			err = cli_ssld_cfg_is_valid_item(cert_node, NULL, &is_valid);
			bail_error(err);

			if(!is_valid) {
				cli_printf("Invalid certificate entry\n");
				goto bail;
			}
			break;
		case cc_ssld_ca:
			//snprintf(cert_node, sizeof(cert_node), "/nkn/ssld/config/ca-certificate/%s",
			//		cert_name);
			snprintf(cert_path, sizeof(cert_path), "%s%s", SSLD_CA_PATH,
					cert_name);
			break;
	}


	//openssl x509 -in filename.crt -noout -text 

	err = lc_launch_quick_status(&ret_status, &output, true, 6,
			"/usr/bin/openssl", "x509",
			"-in", cert_path,
			"-noout", "-text");
	bail_error(err);

	if(ret_status) {
		cli_printf_error("Error oppening certificate");
		goto bail;

	} else {

		cli_printf("%s", ts_str(output));

	}

bail:
	ts_free(&output);
	return err;

}
int
cli_ssld_cfg_ls(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	tstr_array *names = NULL;
	uint32 num_names = 0;
	uint32 idx = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	err = cli_ssld_cfg_get_items("/nkn/ssld/config/virtual_host", &names);
	num_names = tstr_array_length_quick(names);

	err = cli_printf(_("Currently configured virtual-hosts  :\n"));
	bail_error(err);

	if(num_names > 0) {

		err = cli_printf("            Host               certificate                key                 cipher\n");
		bail_error(err);

		err = cli_printf(_("===============================================================================================\n"));
		bail_error(err);

		for(idx = 0; idx < num_names; idx++) {

			const char *vhost_name = NULL;
			tstring *cert_name = NULL;
			tstring *key_name = NULL;
			tstring *cipher = NULL;

			node_name_t item_node = {0};

			vhost_name = tstr_array_get_str_quick(names, idx);
			bail_null(vhost_name);

			snprintf(item_node, sizeof(item_node), "/nkn/ssld/config/virtual_host/%s/cert",
											vhost_name);

			err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &cert_name, item_node, NULL);
			bail_error_null(err, cert_name);

			snprintf(item_node, sizeof(item_node), "/nkn/ssld/config/virtual_host/%s/key",
											vhost_name);

			err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &key_name, item_node, NULL);
			bail_error_null(err, key_name);

			snprintf(item_node, sizeof(item_node), "/nkn/ssld/config/virtual_host/%s/cipher",
											vhost_name);

			err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &cipher, item_node, NULL);
			bail_error_null(err, cipher);

			err = cli_printf("%16s %26s %19s %40s\n",
						    vhost_name ? vhost_name : "N/A",
						    ts_nonempty(cert_name) ? ts_str(cert_name) : "N/A",
						    ts_nonempty(key_name) ? ts_str(key_name) : "N/A",
						    ts_nonempty(cipher) ? ts_str(cipher) : "N/A");
			bail_error(err);

			cli_printf("-----------------------------------------------------------------------------------------------\n");
		}
	} else {
		err = cli_printf(_("No virtual-hosts configured\n"));
		bail_error(err);

	}
bail:
	tstr_array_free(&names);
	return err;
}




int
cli_ssld_cfg_del_hdlr(void *data,
		const cli_command       *cmd,
		const tstr_array        *cmd_line,
		const tstr_array        *params)
{
	int err = 0;
	file_path_t  item_path ={0};
	const char *item_name = NULL;
	const char *item_type = NULL;
	node_name_t  item_node = {0};
	tbool is_valid = false;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(cmd);

	/* This can be moved as an action */
	item_name = tstr_array_get_str_quick(params, 0);
	bail_null(item_name);

	switch(cmd->cc_magic) {
		case cc_ssld_cert:
			snprintf(item_path, sizeof(item_path), SSLD_CERT_PATH"%s",
					item_name);
			snprintf(item_node, sizeof(item_node), "/nkn/ssld/config/certificate/%s",
					item_name);

			err = cli_ssld_cfg_is_valid_item(item_node, NULL, &is_valid);
			bail_error(err);
			item_type = "certificate";
		break;
		case cc_ssld_key:
			snprintf(item_path, sizeof(item_path), SSLD_KEY_PATH"%s",
					item_name);
			snprintf(item_node, sizeof(item_node), "/nkn/ssld/config/key/%s",
					item_name);

			err = cli_ssld_cfg_is_valid_item(item_node, NULL, &is_valid);
			bail_error(err);
			item_type = "key";
		break;
		case cc_ssld_csr:
			snprintf(item_path, sizeof(item_path), SSLD_CSR_PATH"%s",
					item_name);
			snprintf(item_node, sizeof(item_node), "/nkn/ssld/config/csr/%s",
					item_name);

			err = cli_ssld_cfg_is_valid_item(item_node, NULL, &is_valid);
			bail_error(err);
			item_type = "csr";
		break;
		case cc_ssld_ca:
			snprintf(item_path, sizeof(item_path), SSLD_CA_PATH"%s",
					item_name);
#if 0
			snprintf(item_node, sizeof(item_node), "/nkn/ssld/config/ca-certificate/%s",
					item_name);
			err = cli_ssld_cfg_is_valid_item(item_node, NULL, &is_valid);
			bail_error(err);
#endif
			is_valid = 1;
			item_type = "CA";
		break;


	}

	//Check if certificate is bound to vhost
	if(!is_valid) {
		cli_printf("Invalid %s entry", item_type);
		goto bail;
	}

	if(err == 0 ) {
		uint32_t ret_code = 0;

		if(cmd->cc_magic != cc_ssld_ca) {

			err = mdc_delete_binding(cli_mcc, &ret_code, NULL, item_node);
			bail_error(err);
		}

		if(!ret_code && cmd->cc_magic == cc_ssld_ca) {
			tbool exists = false;
			err = lf_test_exists(item_path, &exists);
			if(!exists) {
			    cli_printf_error("Error deleteing the file %s", item_name);
			    goto bail;
			}
			err = lf_remove_file(item_path);
		}

	} else {
		cli_printf_error("Error deleting the %s", item_type);
	}

bail:
	return err;
}

int
cli_ssld_cfg_create_hdlr(void *data,
		const cli_command       *cmd,
		const tstr_array        *cmd_line,
		const tstr_array        *params)
{
	int err = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	switch(cmd->cc_magic)
	{
		case cc_ssld_key:
			err = cli_ssld_new_key(params, cmd_line);
			bail_error(err);
			break;
		case cc_ssld_csr:
			err = cli_ssld_new_csr(params, cmd_line);
			bail_error(err);
			break;
		case cc_ssld_cert:
			err = cli_ssld_new_cert(params, cmd_line);
			bail_error(err);
			break;
		case cc_ssld_ca:
			//err = cli_ssld_new_ca(params, cmd_line);
			//bail_error(err);
			break;
	}

bail:
	return err;
}

static int
cli_ssld_new_cert(const tstr_array *params, const tstr_array *cmd_line)
{
	int err = 0;
	int num_params = -1;
	const char *cert_name = NULL;
	const char *input_key = NULL;
	const char *passwd = NULL;
	const char *validity = NULL;
	tstr_array *argv = NULL;
	num_params = tstr_array_length_quick(params);
	file_path_t key_path = {0};
	str_value_t pass_phrase = {0};


	cli_ssld_term_data_t  *csr_term_data = malloc(sizeof(cli_ssld_term_data_t));
	memset(csr_term_data, 0 ,sizeof(cli_ssld_term_data_t));

	UNUSED_ARGUMENT(cmd_line);



	 if (num_params >1 && num_params < 4) {

		// openssl req -x509 -days 365 -newkey rsa:1024 -passout pass:test123 -keyout
		// key-cert.pem -out key-cert.pem
		cert_name = tstr_array_get_str_quick(params, 0);
		bail_null(cert_name);

		validity =  tstr_array_get_str_quick(params, 1);
		bail_null(validity);

		passwd = tstr_array_get_str_quick(params, 2);
		bail_null(passwd);

		snprintf(pass_phrase, sizeof(pass_phrase), "pass:%s", passwd);

		err = tstr_array_new_va_str
			(&argv, NULL, 13, OPENSSL, "req", "-x509",
			 "-days", validity, "-newkey", "rsa:1024", "-passout", pass_phrase,"-keyout", cert_name, "-out",
			 cert_name);
		bail_error(err);

		csr_term_data->term_data_type = SSLD_CERT_TERM_DT;
		csr_term_data->file_name = strdup(cert_name);
		csr_term_data->passphrase = strdup(passwd);


	 } else if(num_params > 1 && num_params < 5 ) {
		 cert_name = tstr_array_get_str_quick(params, 0);
		 bail_null(cert_name);

		 validity =  tstr_array_get_str_quick(params, 1);
		 bail_null(validity);

		 input_key = tstr_array_get_str_quick(params, 2);
		 bail_null(input_key);

		 passwd = tstr_array_get_str_quick(params, 3);
		 bail_null(passwd);

		 snprintf(key_path, sizeof(key_path), "/config/nkn/key/%s",
				 input_key);
		 snprintf(pass_phrase, sizeof(pass_phrase), "pass:%s", passwd);


		 //openssl req -x509  -days 365 -new -passin pass:test123 -key key.pem -out key-cert.pem

		 err = tstr_array_new_va_str
			(&argv, NULL, 12, OPENSSL, "req", "-x509",
			 "-days", validity, "-new", "-passin", pass_phrase, "-key", key_path, "-out",
			 cert_name);
		bail_error(err);

		csr_term_data->term_data_type = SSLD_CERT_TERM_DT;
		csr_term_data->file_name = strdup(cert_name);

	} 	err = clish_run_program_fg(OPENSSL, argv,
			NULL, cli_ssld_term_handler,
			csr_term_data);

bail:
	tstr_array_free(&argv);
	return err;


}



static int
cli_ssld_new_csr(const tstr_array *params, const tstr_array *cmd_line)
{
	int err = 0;
	int num_params = -1;
	const char *csr_name = NULL;
	const char *input_key = NULL;
	const char *passwd = NULL;
	tstr_array *argv = NULL;
	num_params = tstr_array_length_quick(params);
	file_path_t key_path = {0};
	str_value_t pass_phrase = {0};


	cli_ssld_term_data_t  *csr_term_data = malloc(sizeof(cli_ssld_term_data_t));
	memset(csr_term_data, 0 ,sizeof(cli_ssld_term_data_t));

	UNUSED_ARGUMENT(cmd_line);

	csr_name = tstr_array_get_str_quick(params, 0);
	bail_null(csr_name);

	input_key = tstr_array_get_str_quick(params, 1);
	bail_null(input_key);

	passwd = tstr_array_get_str_quick(params, 2);
	bail_null(passwd);

	snprintf(key_path, sizeof(key_path), "/config/nkn/key/%s",
			    input_key);

	snprintf(pass_phrase, sizeof(pass_phrase), "pass:%s", passwd);

	if(num_params > 1 && num_params < 4 ) {
		err = tstr_array_new_va_str
			(&argv, NULL, 9, OPENSSL, "req",
			 "-new", "-passin", pass_phrase, "-key", key_path, "-out", csr_name);
		bail_error(err);

		csr_term_data->term_data_type = SSLD_CSR_TERM_DT;
		csr_term_data->file_name = strdup(csr_name);

		err = clish_run_program_fg(OPENSSL, argv,
				NULL, cli_ssld_term_handler,
				csr_term_data);


	}

bail:
    tstr_array_free(&argv);
	return err;


}

static int
cli_ssld_new_key(const tstr_array *params, const tstr_array *cmd_line)
{
	int err = 0;
	int num_params = -1;
	const char *key_name = NULL;
	const char *cipher = NULL;
	tstr_array *argv = NULL;
	num_params = tstr_array_length_quick(params);
	void *ret_data = NULL;
	tbool ret_found = false;
	//file_path_t key_path = {0};
	str_value_t cipher_switch = {0};
	str_value_t passphrase = {0};
	const char *passwd = NULL;
	cli_ssld_term_data_t  *key_term_data = NULL;

	UNUSED_ARGUMENT(cmd_line);

	key_name = tstr_array_get_str_quick(params, 0);
	bail_null(key_name);

	cipher = tstr_array_get_str_quick(params, 1);
	bail_null(cipher);

	/* Check if we cipher presented by user is within the allowed
	 * list
	 */
	err = cli_expansion_option_get_data(cli_ssld_cipher_opts, -1,
					cipher, &ret_data, &ret_found);
	bail_error(err);

	if(!ret_found) {
	    cli_printf_error("Please use a valid cipher arguemnt");
	    goto bail;
	}

	key_term_data = malloc(sizeof(cli_ssld_term_data_t));
	memset(key_term_data, 0 ,sizeof(cli_ssld_term_data_t));




	/* check if the cipher is an allowed one */

	//snprintf(key_path, sizeof(key_path), "/config/nkn/key/%s",
	//		    key_name);

	snprintf(cipher_switch, sizeof(cipher_switch), "-%s", cipher);

	if(num_params > 1 && num_params < 4 ) {
		err = tstr_array_new_va_str
			(&argv, NULL, 6, OPENSSL, "genrsa",
			 cipher_switch, "-out", key_name, "1024");
		bail_error(err);

		key_term_data->term_data_type = SSLD_KEY_TERM_DT;
		key_term_data->file_name = strdup(key_name);

		err = clish_run_program_fg(OPENSSL, argv,
				NULL, cli_ssld_term_handler,
				key_term_data);


	} else if(num_params > 1 && num_params < 5 ) {
		const char *key_length = NULL;

		key_length =  tstr_array_get_str_quick(params, 2);
		bail_null(key_length);

		passwd = tstr_array_get_str_quick(params, 3);
		bail_null(passwd);

		snprintf(passphrase, sizeof(passphrase), "pass:%s", passwd);

		err = tstr_array_new_va_str
			(&argv, NULL, 8, OPENSSL, "genrsa",
			 "-passout", passphrase, cipher_switch,"-out", key_name, key_length);
		bail_error(err);

		key_term_data->term_data_type = SSLD_KEY_TERM_DT;
		key_term_data->file_name = strdup(key_name);
		key_term_data->passphrase = strdup(passwd);

		err = clish_run_program_fg(OPENSSL, argv,
				NULL, cli_ssld_term_handler,
				key_term_data);


	}

bail:
    tstr_array_free(&argv);
	return err;


}



static int
cli_ssld_set_timestamp(const char *item, const char *type)
{
	int32_t time_stamp = 0;
	str_value_t ts = {0};
	node_name_t nd = {0};
	int err = 0;

	time_stamp = lt_curr_time();
	snprintf(ts, sizeof(ts), "%d", time_stamp);


	snprintf(nd, sizeof(nd), "/nkn/ssld/config/%s/%s/time_stamp",
			type, item);

	err = mdc_set_binding(cli_mcc,
			NULL,
			NULL,
			bsso_modify,
			bt_int32,
			ts,
			nd);
	bail_error(err);
bail:
	return err;
}

static int
cli_ssld_term_handler(void *data)
{
	int err = 0;
	cli_ssld_term_data_t  *term_data = (cli_ssld_term_data_t*)data;
	file_path_t file_path = {0};
	file_path_t ld_path = {0};
	tbool ret_test = false;

	bail_null(term_data);

	switch(term_data->term_data_type)
	{
		case SSLD_KEY_TERM_DT:
			/* Check if file exists */
			snprintf(file_path, sizeof(file_path), "/config/nkn/key/%s",
					term_data->file_name);
			snprintf(ld_path, sizeof(ld_path), "/var/home/root/%s",
					term_data->file_name);

			err = lf_test_exists(ld_path, &ret_test);
			bail_error(err);

			/*Create nodes*/
			if(ret_test) {
				lf_move_file(ld_path, file_path);
				node_name_t passphrase = {0};

				snprintf(passphrase, sizeof(passphrase), "/nkn/ssld/config/key/%s/passphrase",
						term_data->file_name);
				err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_create, bt_string,
						term_data->passphrase, passphrase);
				bail_error(err);

				err = cli_ssld_set_timestamp(term_data->file_name, "key");
				bail_error(err);
			}
			break;
		case SSLD_CERT_TERM_DT:
			/* Check if file exists */
			snprintf(file_path, sizeof(file_path), "/config/nkn/cert/%s",
					term_data->file_name);
			snprintf(ld_path, sizeof(ld_path), "/var/home/root/%s",
					term_data->file_name);

			err = lf_test_exists(ld_path, &ret_test);
			bail_error(err);

			/*Create nodes*/
			if(ret_test) {
				lf_move_file(ld_path, file_path);
				node_name_t key_nd = {0};
				node_name_t passphrase_nd = {0};


				if(term_data->passphrase) {
					snprintf(passphrase_nd, sizeof(passphrase_nd),
							"/nkn/ssld/config/certificate/%s/passphrase",
							term_data->file_name);
					err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_create, bt_string,
							term_data->passphrase, passphrase_nd);
					bail_error(err);
				} else {

					snprintf(key_nd, sizeof(key_nd), "/nkn/ssld/config/certificate/%s",
							term_data->file_name);
					err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_create, bt_string,
							term_data->file_name, key_nd);
					bail_error(err);

				}
				err = cli_ssld_set_timestamp(term_data->file_name, "certificate");
				bail_error(err);
			}

			break;
		case SSLD_CSR_TERM_DT:
			/* Check if file exists */
			snprintf(file_path, sizeof(file_path), "/config/nkn/csr/%s",
					term_data->file_name);
			snprintf(ld_path, sizeof(ld_path), "/var/home/root/%s",
					term_data->file_name);

			err = lf_test_exists(ld_path, &ret_test);
			bail_error(err);

			/*Create nodes*/
			if(ret_test) {
				lf_move_file(ld_path, file_path);
				node_name_t key_nd = {0};
				snprintf(key_nd, sizeof(key_nd), "/nkn/ssld/config/csr/%s",
						term_data->file_name);
				err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_create, bt_string,
						term_data->file_name, key_nd);
				bail_error(err);

				/* No timestamp for CSR */

			}
			break;

		break;

	}
bail:
	//bn_binding_free(&binding);
	//bn_binding_array_free(&barr);
	return(err);
}




int
cli_ssld_cfg_import(void *data,
		const cli_command       *cmd,
		const tstr_array        *cmd_line,
		const tstr_array        *params)
{
	int err = 0;
	const char *remote_url = NULL;
	const char *ca_file = NULL;

	UNUSED_ARGUMENT(data);

	remote_url = tstr_array_get_str_quick(params, 1);
	bail_null(remote_url);

	ca_file = tstr_array_get_str_quick(params, 0);
	bail_null(ca_file);

	if((!lc_is_suffix(".pem", ca_file, false)) && (cmd->cc_magic == cc_ssld_ca)) {
	    err = cli_printf_error("CA file must have an extension of pem");
	    bail_error(err);
	    goto bail;
	}

	if (clish_is_shell()) {
		err = clish_maybe_prompt_for_password_url
			(remote_url, cli_ssld_cfg_import_finish,
			 cmd, cmd_line, params,data);
		bail_error(err);
	}
	else {
		err = cli_ssld_cfg_import_finish(NULL, cpr_ok, cmd,
							cmd_line, params,
							data, NULL);
		bail_error(err);
	}


	bail_error(err);

bail:
	return err;
}

int
cli_ssld_cfg_help(
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
	const char *node_name = NULL;

	bail_null(data);

	node_name = (const char *)data;

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

			err = cli_ssld_cfg_get_items (node_name,
					&names);


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
cli_ssld_cfg_export(void *data,
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
			(remote_url, cli_ssld_cfg_export_finish,
			 cmd, cmd_line, params, NULL);
		bail_error(err);
	}
	else {
		err = cli_ssld_cfg_export_finish(NULL, cpr_ok,
				cmd, cmd_line, params,
				NULL, NULL);
		bail_error(err);
	}


bail:
	return err;


}

/*----------------------------------------------------------------------------
 * LOCAL FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
static int
cli_ssld_cfg_revmap (void *data, const cli_command *cmd,
		const bn_binding_array *bindings,
		const char *name,
		const tstr_array *name_parts,
		const char *value, const bn_binding *binding,
		cli_revmap_reply *ret_reply)
{

	int err = 0;
	const char *virt_host = NULL;
	tstring *cert_name = NULL;
	tstring *key_name = NULL;
	tstring *rev_cmd = NULL;
	tbool found = false;
	tbool active = false;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(bindings);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	bail_null(value);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, &found, &active,
			"/license/feature/SSL/status/active", NULL);
	bail_error(err);

	if(!active)
	    goto bail;

	virt_host = tstr_array_get_str_quick(name_parts, 4);
	bail_null(virt_host);
	
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		NULL, &cert_name, "%s/cert", name);
	bail_error(err);

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
		NULL, &key_name, "%s/key", name);
	bail_error(err);


	if(strlen(value) >1) {

		err = ts_new_sprintf(&rev_cmd,
				"#virtual-host %s certificate bind \"%s\" key bind \"%s\" ",
				virt_host, ts_str_maybe_empty(cert_name), ts_str_maybe_empty(key_name));
		bail_error(err);
	}

	if (NULL != rev_cmd) {
		err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
		bail_error(err);
	}



bail:
	ts_free(&rev_cmd);
	ts_free(&cert_name);
	ts_free(&key_name);
	return err;

}

static int
cli_ssld_cfg_export_finish(const char *password, 
			    clish_password_result result,
			    const cli_command *cmd, 
			    const tstr_array *cmd_line,
			    const tstr_array *params, 
			    void *data,
			    tbool *ret_continue)

{
	int err = 0;
	file_path_t ssl_file_path = {0};
	const char *cert_name = NULL;
	const char *remote_url = NULL;
	const char *ssl_file_dir = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(ret_continue);


	if (result != cpr_ok) {
		goto bail;
	}

	cert_name = tstr_array_get_str_quick(params, 0);
	bail_null(cert_name);


	switch(cmd->cc_magic) {
		case cc_ssld_cert:
			snprintf(ssl_file_path, sizeof(ssl_file_path), SSLD_CERT_PATH"%s", cert_name);
			ssl_file_dir = SSLD_CERT_PATH;
			break;

		case cc_ssld_key:
			snprintf(ssl_file_path, sizeof(ssl_file_path), SSLD_KEY_PATH"%s", cert_name);
			ssl_file_dir = SSLD_KEY_PATH;
			break;
		case cc_ssld_csr:
			snprintf(ssl_file_path, sizeof(ssl_file_path), SSLD_CSR_PATH"%s", cert_name);
			ssl_file_dir = SSLD_CSR_PATH;
			break;
		case cc_ssld_ca:
			snprintf(ssl_file_path, sizeof(ssl_file_path), SSLD_CA_PATH"%s", cert_name);
			ssl_file_dir = SSLD_CA_PATH;
			break;

	}

	remote_url = tstr_array_get_str_quick(params, 1);
	bail_null(remote_url);

	if (password) {
		err = mdc_send_action_with_bindings_str_va(cli_mcc,
				NULL, NULL,
				"/file/transfer/upload", 3,
				"local_path", bt_string, ssl_file_path,
				"remote_url", bt_string, remote_url,
				"password", bt_string, password);
		bail_error(err);
	}
	else {
		err = mdc_send_action_with_bindings_str_va(cli_mcc,
				NULL, NULL,
				"/file/transfer/upload", 3,
				"local_dir", bt_string, ssl_file_dir,
				"local_filename", bt_string, cert_name,
				"remote_url", bt_string, remote_url);
		bail_error(err);
	}

bail:
	return err;
}

int
cli_ssld_cfg_is_valid_item(const char *node, const char *match_val,
				    tbool *is_valid)
{
	int err = 0;
	bn_binding *binding = NULL;
	tstring *value = NULL;

	err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding,
				    node, NULL);
	//bail_error(err);

	if(binding && !match_val) {
		*is_valid = true;
		goto bail;
	}

	if(match_val && binding) {
		err = bn_binding_get_tstr(binding, ba_value, bt_string,
					NULL, &value);
		bail_error(err);

		if(value && ts_equal_str(value , match_val, false)) {
			*is_valid = true;
		}
	}

bail:
	bn_binding_free(&binding);
	ts_free(&value);
	return err;

}

int
cli_ssld_cfg_get_items(const char *nd_pattern,
		tstr_array **ret_name)
{
	int err = 0;
	tstr_array_options opts;
	tstr_array *names = NULL;
	tstr_array *names_config = NULL;

	bail_null(ret_name);

	err = tstr_array_options_get_defaults(&opts);
	bail_error(err);

	opts.tao_dup_policy = adp_delete_old;

	err = tstr_array_new(&names, &opts);
	bail_error(err);

	err = mdc_get_binding_children_tstr_array(cli_mcc,
			NULL, NULL, &names_config,
			nd_pattern, NULL);
	bail_error_null(err, names_config);

	err = tstr_array_concat(names, &names_config);
	bail_error(err);

	*ret_name = names;

bail:
	tstr_array_free(&names_config);
	//Caller to free names
	return err;
}


static int
cli_ssld_cfg_import_finish(const char *password, clish_password_result result,
				const cli_command *cmd, const tstr_array *cmd_line,
				const tstr_array *params, void *data,
				tbool *ret_continue)
{
	int err = 0;
	const char *remote_url = NULL;
	const char *passphrase = NULL;
	const char *name = NULL;
	bn_request *req = NULL;
	uint16 db_file_mode = 0;
	char *db_file_mode_str = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;
	tstring *t_result = NULL;
	bn_binding_array *ret_bindings = NULL;
	str_value_t cert_tmp_name = {0};
	str_value_t upload_type = {0};
	file_path_t  item_path ={0};

	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(ret_continue);

	if(result != cpr_ok) {
		goto bail;
	}

	name = tstr_array_get_str_quick(params, 0);
	bail_null(name);

	snprintf(cert_tmp_name, sizeof(cert_tmp_name), "%s.tmp", name);

	remote_url = tstr_array_get_str_quick(params, 1);
	bail_null(remote_url);

	passphrase = tstr_array_get_str_quick(params, 2);
	/* Not checking NULL here */

	err = bn_action_request_msg_create(&req, "/file/transfer/download");
	bail_error(err);

	err = bn_action_request_msg_append_new_binding
		(req, 0, "remote_url", bt_string, remote_url, NULL);
	bail_error(err);

	if (password) {
		err = bn_action_request_msg_append_new_binding
			(req, 0, "password", bt_string, password, NULL);
		bail_error(err);
	}

	switch(cmd->cc_magic) {
	    case cc_ssld_cert:
		err = bn_action_request_msg_append_new_binding
			(req, 0, "local_dir", bt_string, "/config/nkn/cert/", NULL);
		bail_error(err);
		snprintf(item_path, sizeof(item_path), SSLD_CERT_PATH"%s",
							name);
	    break;

	    case cc_ssld_key:
		err = bn_action_request_msg_append_new_binding
			(req, 0, "local_dir", bt_string, "/config/nkn/key/", NULL);
		bail_error(err);
		snprintf(item_path, sizeof(item_path), SSLD_KEY_PATH"%s",
							name);
	    break;
	    case cc_ssld_csr:
		err = bn_action_request_msg_append_new_binding
			(req, 0, "local_dir", bt_string, "/config/nkn/csr/", NULL);
		bail_error(err);
		snprintf(item_path, sizeof(item_path), SSLD_CSR_PATH"%s",
							name);
	    break;
	    case cc_ssld_ca:
		err = bn_action_request_msg_append_new_binding
			(req, 0, "local_dir", bt_string, "/config/nkn/ca/", NULL);
		bail_error(err);
		snprintf(item_path, sizeof(item_path), SSLD_CA_PATH"%s",
							name);
	    break;
	}
	err = bn_action_request_msg_append_new_binding
		(req, 0, "local_filename", bt_string, cert_tmp_name, NULL);
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
			(req, 0, "progress_oper_type", bt_string, "cert_download", NULL);
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

	bail_error(ret_err);
	/* any error in download will be printed by default 
	 * so,not handling it here */

	/* check Download */
	/* send action to validate cert */
	snprintf(upload_type, sizeof(upload_type), "%d", cmd->cc_magic);

	if(passphrase) {
		err = mdc_send_action_with_bindings_and_results_va(cli_mcc,
				NULL, NULL,
				"/nkn/ssld/action/cert/import", &ret_bindings, 3,
				"name", bt_string, name,
				"passphrase", bt_string, passphrase,
				"type", bt_string, upload_type, NULL, 10);
		bail_error(err);
	} else {
		err = mdc_send_action_with_bindings_and_results_va(cli_mcc,
				NULL, NULL,
				"/nkn/ssld/action/cert/import", &ret_bindings, 3,
				"name", bt_string, name,
				"passphrase", bt_string, " ",
				"type", bt_string, upload_type, NULL, 10);
		bail_error(err);
	}

	if(ret_bindings) {
		err = bn_binding_array_get_value_tstr_by_name(
				ret_bindings, "Result", NULL, &t_result);
		bail_error(err);
		if(t_result) {

			cli_printf("%s\n", ts_str(t_result));
			/* Set the timestamp here */
    			/* for now we are always returning as valid */
			if(cmd->cc_magic == cc_ssld_cert) {
			    err = cli_ssld_set_timestamp(name, "certificate" );
			    bail_error(err);
			} else if(cmd->cc_magic == cc_ssld_key) {
			    err = cli_ssld_set_timestamp(name, "key");
			    bail_error(err);
			}
   
		} else {
			err = lf_remove_file(item_path);
		}
	}

bail:
	ts_free(&t_result);
	safe_free(db_file_mode_str);
	return err;
}

int
cli_ssld_cfg_cipher_text(
		void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    int num_params = 0;
    int err = 0;
    int i;
    tstring *cipher = NULL;
    const char *pval = NULL;
    node_name_t cipher_node = {0};

    err = tstr_array_print(params, "CLIOPTION");
    bail_error(err);

    num_params = tstr_array_length_quick(params);

    ts_new(&cipher);
    if( num_params > 1 ) {
	for (i = 1; i < num_params - 1; i++) {
	    pval = tstr_array_get_str_quick(params, i);
	    if( pval) {
		ts_append_sprintf( cipher, "%s:", pval);
	    }
	}
	ts_append_sprintf( cipher, "%s", tstr_array_get_str_quick(params, num_params - 1 ));
	snprintf(cipher_node, sizeof(cipher_node), "/nkn/ssld/config/virtual_host/%s/cipher", tstr_array_get_str_quick(params, 0));
	err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string,
		ts_str(cipher), cipher_node);
	bail_error(err);
    } else {
	cli_printf_error( _("Error: Enter a  cipher value\n"));
    }


bail:
    ts_free(&cipher);
    return err;

}

/* ------------------------------------------------------------------------- */
static int
cli_ssl_cmds_handle_change(const bn_binding_array *old_bindings,
                                const bn_binding_array *new_bindings,
                                void *data)
{
    int err = 0;
    tbool found = false, allowed = false;

    UNUSED_ARGUMENT(old_bindings);
    UNUSED_ARGUMENT(data);

    err = bn_binding_array_get_value_tbool_by_name
        (new_bindings, "/nkn/mfd/licensing/config/ssl_licensed", &found, &allowed);
    bail_error(err);

    if (found) {
        err = cli_ssl_cmds_update(allowed, true);
        bail_error(err);
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
cli_ssl_handle_license_change(const char *event_name,
                                   const bn_binding_array *bindings,
                                   void *data)
{
    int err = 0;
    tbool found = false, active = false;

    UNUSED_ARGUMENT(event_name);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(data);

    err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, &found, &active,
                       "/license/feature/SSL/status/active", NULL);
    bail_error(err);

    if (found) {
        err = cli_ssl_cmds_update(active, true);
        bail_error(err);
    }
    else {
        lc_log_basic(LOG_WARNING, I_("Could not find node telling us whether "
                                     "ssl commands should be "
                                     "available"));
        goto bail;
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
cli_ssl_cmds_set_flags(const tstr_array *cmds, tbool allowed,
                            cli_command_flags flag)
{
    int err = 0;
    int num_cmds = 0, i = 0;
    const char *cmd_str = NULL;
    cli_command *cmd = NULL;

    num_cmds = tstr_array_length_quick(cmds);
    for (i = 0; i < num_cmds; ++i) {
        cmd_str = tstr_array_get_str_quick(cmds, i);
        bail_null(cmd_str);
        err = cli_get_command_registration(cmd_str, &cmd);
        bail_error(err);
        if (cmd == NULL) {
            /*
             * We don't complain too hard about commands we can't find,
             * since there are various reasons why a command might only
             * be present some of the time: (a) if a PROD_FEATURE is
             * disabled, or (b) if it requires the CLI shell, or (c) if
             * mgmtd is unavailable.
             */
            lc_log_basic(LOG_INFO,
                         _("Could not find command registration \"%s\" to "
                           "suppress; ignoring it"), cmd_str);
            continue;
        }
        if (allowed) {
            cmd->cc_flags &= ~flag;
        }
        else {
            cmd->cc_flags |= flag;
        }
    }

 bail:
    return(err);
}


/*
 * --------------------------------------------
 * NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
 * --------------------------------------------
 *
 *  Add CLIs here that need to be tied the SSL feature
 *  license.
 */
const char *cli_ssl_cmds_unavail_list[] = {
    "ssl",
    "virtual-host",
    "no virtual-host",
    "show ssl",
    "show virtual-host",
    NULL
};


/* ------------------------------------------------------------------------- */
static int
cli_ssl_cmds_update(tbool allowed, tbool mgmtd_avail)
{
    int err = 0;
    tstr_array *ssl_cmds = NULL, *hidden_cmds = NULL;

    if (mgmtd_avail) {
        err = tstr_array_new_argv(&ssl_cmds, NULL, -1,
                                  cli_ssl_cmds_unavail_list);
        bail_error(err);
    }
    else {
    }

    err = cli_ssl_cmds_set_flags(ssl_cmds, allowed, 
                                      ccf_unavailable);
    bail_error(err);

    //err = cli_ssl_cmds_set_flags(hidden_cmds, allowed, ccf_hidden);
    //bail_error(err);

 bail:
    tstr_array_free(&ssl_cmds);
    tstr_array_free(&hidden_cmds);
    return(err);
}

/* ------------------------------------------------------------------------- */
int
cli_ssl_restrict_cmds_init(const lc_dso_info *info, 
                      const cli_module_context *context)
{
    int err = 0;
    //cli_command *cmd = NULL;
    tbool found = false, allowed = false;

    UNUSED_ARGUMENT(info);

    if (context->cmc_mgmt_avail == false) {
        err = cli_ssl_cmds_update(false, false);
        bail_error(err);
        goto bail;
    }

    err = clish_config_change_notif_register(cli_ssl_cmds_handle_change,
                                             NULL);
    bail_error(err);

    err = clish_mgmt_event_notif_register("/license/events/key_state_change",
                                          cli_ssl_handle_license_change,
                                          NULL);
    bail_error(err);

    err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, &found, &allowed,
                      "/license/feature/SSL/status/active", NULL);
    bail_error(err);

    if (!found) {
        lc_log_basic(LOG_WARNING, I_("Could not find node telling us whether "
                                     "ssl commands should be "
                                     "available"));
        goto bail;
    }

    err = cli_ssl_cmds_update(allowed, true);
    bail_error(err);

 bail:
    return(err);
}


int
cli_ssld_cfg_show_cipher(
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

       err = cli_printf_query (_("Cipher Configured   : "
                        "#/nkn/ssld/config/origin-req/cipher#\n"));
        bail_error(err);

bail:
	return err;
}


/*
* TODO: Use lc_launch against nkn_clish_run_proagram_fg as return status is not know and to have better control
*
*/
