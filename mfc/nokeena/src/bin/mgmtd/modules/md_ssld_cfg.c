/*
 *
 * Filename:  md_ssld_cfg.c
 * Date:      2011/05/23
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "nkn_mgmt_defs.h"
#include "file_utils.h"
#include "ssld_mgmt.h"
/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
typedef struct ssl_cfg_cert_cb_arg {
	lc_launch_result* lr;
	tstring* cert_name;
	tstring* passphrase;
	char *type;
} ssl_cfg_cert_cb_arg_t;


const bn_str_value md_ssld_cfg_initial_values[] = {
    {"/nkn/ssld/config/virtual_host/wildcard", bt_string, "wildcard"},
    {"/nkn/ssld/config/virtual_host/wildcard/cert", bt_string, ""},
    {"/nkn/ssld/config/virtual_host/wildcard/key", bt_string, ""},
    {"/nkn/ssld/config/virtual_host/wildcard/cipher", bt_string, ""},
};

/*
 * Include the cipher text here "!<cipher-text>:"
 * This handles the not form of the cipher and enusres that the correct keyword has been written
 */
#define  SUPPORTED_CIPHER_LIST "!DHE-RSA-AES256-SHA:!DHE-DSS-AES256-SHA:!AES256-SHA:EDH-RSA-DES-CBC3-SHA:!EDH-DSS-DES-CBC3-SHA:!DES-CBC3-SHA:DES-CBC3-MD5:!DHE-RSA-AES128-SHA:!DHE-DSS-AES128-SHA:!AES128-SHA:!RC2-CBC-MD5:!RC4-SHA:!RC4-MD5:!EDH-RSA-DES-CBC-SHA:!EDH-DSS-DES-CBC-SHA:!DES-CBC-SHA:DES-CBC-MD5:!EXP-EDH-RSA-DES-CBC-SHA:!EXP-EDH-DSS-DES-CBC-SHA:!EXP-DES-CBC-SHA:!EXP-RC2-CBC-MD5:!EXP-RC4-MD5:!ECDHE-RSA-AES256-SHA:!ECDHE-ECDSA-AES256-SHA:!DHE-RSA-CAMELLIA256-SHA:!DHE-DSS-CAMELLIA256-SHA:!AECDH-AES256-SHA:!ADH-AES256-SHA:!ADH-CAMELLIA256-SHA:!ECDH-RSA-AES256-SHA:!ECDH-ECDSA-AES256-SHA:!CAMELLIA256-SHA:!PSK-AES256-CBC-SHA:!ECDHE-RSA-DES-CBC3-SHA:!ECDHE-ECDSA-DES-CBC3-SHA:!AECDH-DES-CBC3-SHA:!ADH-DES-CBC3-SHA:!ECDH-RSA-DES-CBC3-SHA:!ECDH-ECDSA-DES-CBC3-SHA:!PSK-3DES-EDE-CBC-SHA:!ECDHE-RSA-AES128-SHA:!ECDHE-ECDSA-AES128-SHA:!DHE-RSA-SEED-SHA:!DHE-DSS-SEED-SHA:!DHE-RSA-CAMELLIA128-SHA:!DHE-DSS-CAMELLIA128-SHA:!AECDH-AES128-SHA:!ADH-AES128-SHA:!ADH-SEED-SHA:!ADH-CAMELLIA128-SHA:!ECDH-RSA-AES128-SHA:!ECDH-ECDSA-AES128-SHA:!SEED-SHA:!CAMELLIA128-SHA:!IDEA-CBC-SHA:!IDEA-CBC-MD5:!PSK-AES128-CBC-SHA:!ECDHE-RSA-RC4-SHA:!ECDHE-ECDSA-RC4-SHA:!AECDH-RC4-SHA:!ADH-RC4-MD5:!ECDH-RSA-RC4-SHA:!ECDH-ECDSA-RC4-SHA:!PSK-RC4-SHA:!ADH-DES-CBC-SHA:!EXP-ADH-DES-CBC-SHA:!EXP-ADH-RC4-MD5:!NULL-SHA:!NULL-MD5:!eNULL:!aNULL:!ALL:!DEFAULT:"

int md_ssld_cfg_init(const lc_dso_info *info, void *data);


int md_ssld_set_config(md_commit *commit,
                        mdb_db **db,
                        const char *action_name,
                        bn_binding_array *params,
                        void *cb_arg);

static int
md_ssld_set_csr(md_commit *commit,
		mdb_db **db,
		const char *name, const char *passphrase);

/*----------------------------------------------------------------------------
 * LOCAL FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_ssld_cfg_ug_rules = NULL;

static int
md_ssld_set_cert(md_commit *commit,
		mdb_db **db,
		const char *name, const char *passphrase);

static int
md_ssld_set_key(md_commit *commit,
		mdb_db **db,
		const char *name, const char *passphrase);
static int
md_ssld_set_ca(md_commit *commit,
		mdb_db **db,
		const char *name, const char *passphrase);
static int
md_ssld_cfg_import_cert_finalize(md_commit *commit, const mdb_db *db,
		const char *action_name, void *cb_arg,
		void *finalize_arg, pid_t pid,
		int wait_status, tbool completed);

static int
md_ssld_cfg_import_cert(md_commit *commit,
		const mdb_db *db,
		const char *action_name,
		bn_binding_array *params,
		void *cb_arg);

static int
md_ssld_cfg_commit_check(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg);

static int
md_ssld_cfg_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg);


static int
md_ssld_cfg_add_initial(md_commit *commit, mdb_db *db, void *arg);

int
md_ssld_cfg_is_cert_valid(md_commit *commit, const mdb_db *old_db, tbool *valid,
					const char *match_val, const char *nd_fmt, const char*val);

static int
md_ssld_cfg_cert_is_bound_to_vhost(md_commit *commit, const mdb_db *old_db,
					const char *cert_name,tbool *bound);

static int
md_ssld_cfg_key_is_bound_to_vhost(md_commit *commit, const mdb_db *db,
					const char *key_name,tbool *bound);

static int
md_ssld_validate_cipher(md_commit *commit, tstring *cipher) ;
static int
md_ca_files_iterate(md_commit *commit, const mdb_db *db,
                         const char *parent_node_name,
                         const uint32_array *node_attrib_ids,
                         int32 max_num_nodes, int32 start_child_num,
                         const char *get_next_child,
                         mdm_mon_iterate_names_cb_func iterate_cb,
                         void *iterate_cb_arg, void *arg);
static int
md_ca_files_get(md_commit *commit, const mdb_db *db,
                     const char *node_name,
                     const bn_attrib_array *node_attribs,
                     bn_binding **ret_binding, uint32 *ret_node_flags,
                     void *arg);

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
int md_ssld_set_config(md_commit *commit,
                        mdb_db **db,
                        const char *action_name,
                        bn_binding_array *params,
                        void *cb_arg)
{

    int err = 0;
    const bn_binding *binding = NULL;
    char *str_type = NULL;
    char *name = NULL;
    char *passphrase = NULL;
    uint32_t type = -1;

    err = bn_binding_array_get_binding_by_name(params, "name", &binding);
    bail_error_null(err, binding);

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &name);
    bail_error_null(err, name);

    err = bn_binding_array_get_binding_by_name(params, "type", &binding);
    bail_error_null(err, binding);

    err = bn_binding_get_str(binding, ba_value, bt_string,  NULL, &str_type);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name(params, "passphrase", &binding);
    bail_error_null(err, binding);

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &passphrase);
    //bail_error_null(err, passphrase);

    type = strtoul(str_type, NULL, 0);
    /* type can be used to set config for various objects such as cert, csr..etc */
    switch(type)
    {
	case 1:
	    err = md_ssld_set_cert(commit, db, name, passphrase);
	    bail_error(err);
	    break;
	case 2:
	    err = md_ssld_set_key(commit, db, name, passphrase);
	    bail_error(err);
	    break;
	case 3:
	    err = md_ssld_set_csr(commit, db, name, passphrase);
	    bail_error(err);
	    break;
	case 4:
	    //err = md_ssld_set_ca(commit, db, name, passphrase);
	    //bail_error(err);
	    break;
	default:
	    break;
    }

bail:
    safe_free(name);
    safe_free(passphrase);
    safe_free(str_type);
    return err;
}

static int
md_ssld_set_csr(md_commit *commit,
		mdb_db **db,
		const char *name, const char *passphrase)
{
    int err = 0;
    bn_request *req = NULL;
    node_name_t node = {0};
    bn_binding *binding = NULL;
    uint16 ret_code = 0;
    tstring *ret_msg = NULL;

    err = bn_set_request_msg_create(&req, 0);
    bail_error_null(err, req);

    snprintf(node, sizeof(node), "/nkn/ssld/config/csr/%s", name);

    bn_binding_free(&binding);
    err = bn_binding_new_str(&binding, node, ba_value, bt_string, 0, name);
    bail_error_null(err, binding);

    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);

    err = md_commit_handle_commit_from_module(commit, req, NULL,
	    &ret_code, &ret_msg, NULL, NULL);
    complain_error(err);
bail:
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}


static int
md_ssld_set_key(md_commit *commit,
		mdb_db **db,
		const char *name, const char *passphrase)
{
    int err = 0;
    bn_request *req = NULL;
    node_name_t node = {0};
    bn_binding *binding = NULL;
    uint16 ret_code = 0;
    tstring *ret_msg = NULL;
    int32_t time_stamp = 0;
    str_value_t ts = {0};


    time_stamp = lt_curr_time();
    snprintf(ts, sizeof(ts), "%d", time_stamp);

    err = bn_set_request_msg_create(&req, 0);
    bail_error_null(err, req);

    snprintf(node, sizeof(node), "/nkn/ssld/config/key/%s", name);

    err = bn_binding_new_str(&binding, node, ba_value, bt_string, 0, name);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);

    snprintf(node, sizeof(node), "/nkn/ssld/config/key/%s/passphrase", name);

    err = bn_binding_new_str(&binding, node, ba_value, bt_string, 0, passphrase);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);

    snprintf(node, sizeof(node), "/nkn/ssld/config/key/%s/time_stamp", name);
    bn_binding_free(&binding);
    err = bn_binding_new_str(&binding, node, ba_value, bt_int32, 0, ts);
    bail_error_null(err, binding);

    err = md_commit_handle_commit_from_module(commit, req, NULL,
	    &ret_code, &ret_msg, NULL, NULL);
    complain_error(err);
bail:
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}



static int
md_ssld_set_cert(md_commit *commit,
		mdb_db **db,
		const char *name, const char *passphrase)
{
    int err = 0;
    bn_request *req = NULL;
    node_name_t node = {0};
    bn_binding *binding = NULL;
    uint16 ret_code = 0;
    tstring *ret_msg = NULL;
    int32_t time_stamp = 0;
    str_value_t ts = {0};

    time_stamp = lt_curr_time();
    snprintf(ts, sizeof(ts), "%d", time_stamp);

    err = bn_set_request_msg_create(&req, 0);
    bail_error_null(err, req);

    snprintf(node, sizeof(node), "/nkn/ssld/config/certificate/%s", name);

    err = bn_binding_new_str(&binding, node, ba_value, bt_string, 0, name);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);

    snprintf(node, sizeof(node), "/nkn/ssld/config/certificate/%s/passphrase", name);

    err = bn_binding_new_str(&binding, node, ba_value, bt_string, 0, passphrase);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);

    snprintf(node, sizeof(node), "/nkn/ssld/config/certificate/%s/time_stamp", name);
    bn_binding_free(&binding);
    err = bn_binding_new_str(&binding, node, ba_value, bt_int32, 0, ts);
    bail_error_null(err, binding);

    err = md_commit_handle_commit_from_module(commit, req, NULL,
	    &ret_code, &ret_msg, NULL, NULL);
    complain_error(err);
bail:
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}


static int
md_ssld_set_ca(md_commit *commit,
		mdb_db **db,
		const char *name, const char *passphrase)
{
    int err = 0;
    bn_request *req = NULL;
    node_name_t node = {0};
    bn_binding *binding = NULL;
    uint16 ret_code = 0;
    tstring *ret_msg = NULL;


    err = bn_set_request_msg_create(&req, 0);
    bail_error_null(err, req);

    snprintf(node, sizeof(node), "/nkn/ssld/config/ca-certificate/%s", name);

    err = bn_binding_new_str(&binding, node, ba_value, bt_string, 0, name);
    bail_error_null(err, binding);
    err = bn_set_request_msg_append(req, bsso_modify, 0, binding, NULL);
    bail_error(err);
    bn_binding_free(&binding);

    err = md_commit_handle_commit_from_module(commit, req, NULL,
	    &ret_code, &ret_msg, NULL, NULL);
    complain_error(err);
    err = md_commit_set_return_status_msg_fmt(commit, ret_code,
	    _("%s"), ts_str_maybe_empty(ret_msg));
    bail_error(err);
bail:
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}

int
md_ssld_cfg_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("ssld_cfg", 2,
	    "/nkn/ssld", "/nkn/ssld/config",
	    modrf_all_changes,
	    md_ssld_cfg_commit_side_effects, NULL,
	    md_ssld_cfg_commit_check,
	    NULL,
	    NULL, NULL,
	    0,
	    0,
	    md_generic_upgrade_downgrade,
	    &md_ssld_cfg_ug_rules,
	    md_ssld_cfg_add_initial,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    &module);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/key/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = NKN_MAX_SSL_KEY_STR;
    node->mrn_limit_str_no_charlist = NKN_SSL_ALLOWED_CHARS;
    node->mrn_limit_wc_count_max = NKN_MAX_SSL_KEYS;
    node->mrn_bad_value_msg =       "Error creating Node";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/key/*/passphrase";
    node->mrn_value_type =         bt_string;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal | mrf_flags_reg_config_literal_secure;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/key/*/time_stamp";
    node->mrn_value_type =         bt_int32;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "0";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/key/*/host/*";
    node->mrn_value_type =         bt_string;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/csr/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = NKN_MAX_SSL_CSR_STR;
    node->mrn_limit_str_no_charlist = NKN_SSL_ALLOWED_CHARS;
    node->mrn_limit_wc_count_max = NKN_MAX_SSL_CSR;
    node->mrn_bad_value_msg =       "Error creating Node";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/certificate/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = NKN_MAX_SSL_CERT_STR;
    node->mrn_limit_str_no_charlist = NKN_SSL_ALLOWED_CHARS;
    node->mrn_limit_wc_count_max = NKN_MAX_SSL_CERTS;
    node->mrn_bad_value_msg =       "Error creating Node";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/certificate/*/host/*";
    node->mrn_value_type =         bt_string;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/certificate/*/key_file";
    node->mrn_value_type =         bt_string;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/certificate/*/passphrase";
    node->mrn_value_type =         bt_string;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal | mrf_flags_reg_config_literal_secure;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/certificate/*/time_stamp";
    node->mrn_value_type =         bt_int32;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "0";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/virtual_host/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_wc_count_max = NKN_MAX_SSL_HOST;
    node->mrn_bad_value_msg =       "Error creating Node";
    node->mrn_limit_str_max_chars = NKN_MAX_SSL_VHOST_STR;
    node->mrn_limit_str_no_charlist = NKN_SSL_ALLOWED_CHARS;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/virtual_host/*/cert";
    node->mrn_value_type =         bt_string;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/virtual_host/*/key";
    node->mrn_value_type =         bt_string;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/virtual_host/*/cipher";
    node->mrn_value_type =         bt_string;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/origin-req/cipher";
    node->mrn_value_type =         bt_string;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "DEFAULT";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/client-req/client_auth";
    node->mrn_value_type =         bt_bool;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);
    \
	err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/origin-req/server_auth";
    node->mrn_value_type =         bt_bool;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/ssld/config/network/threads";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int = 2;
    node->mrn_limit_num_min_int = 1;
    node->mrn_limit_num_max_int = 4;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "network ssld-threads";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/ca-certificate/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = NKN_MAX_SSL_CERT_STR;
    node->mrn_limit_str_no_charlist = NKN_SSL_ALLOWED_CHARS;
    node->mrn_limit_wc_count_max = NKN_MAX_SSL_CERTS;
    node->mrn_bad_value_msg =       "Error creating Node";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/config/ca-certificate/*/time_stamp";
    node->mrn_value_type =         bt_int32;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "0";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /* Action Nodes */
    err = mdm_new_action(module, &node, 3);
    bail_error(err);
    node->mrn_name               = "/nkn/ssld/action/cert/import";
    node->mrn_node_reg_flags     = mrf_flags_reg_action;
    node->mrn_cap_mask           = mcf_cap_action_basic;
    node->mrn_action_async_start_func =  md_ssld_cfg_import_cert;
    node->mrn_actions[0]->mra_param_name =  "name";
    node->mrn_actions[0]->mra_param_type =  bt_string;
    node->mrn_actions[1]->mra_param_name =  "type";
    node->mrn_actions[1]->mra_param_type =  bt_string;
    node->mrn_actions[2]->mra_param_name =  "passphrase";
    node->mrn_actions[2]->mra_param_type =  bt_string;
    node->mrn_description        = "" ;
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_action(module, &node, 3);
    bail_error(err);
    node->mrn_name =            "/nkn/ssld/action/set_vals";
    node->mrn_node_reg_flags =  mrf_flags_reg_action;
    node->mrn_cap_mask =        mcf_cap_action_basic;
    node->mrn_action_config_func = md_ssld_set_config;
    node->mrn_action_arg =      NULL;
    node->mrn_description =     "";
    node->mrn_actions[0]->mra_param_name = "name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "type";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_actions[2]->mra_param_name = "passphrase";
    node->mrn_actions[2]->mra_param_type = bt_string;

    err = mdm_add_node(module, &node);
    bail_error(err);


    /*
     * Monitoring node for the CA files
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/ssld/cafiles/*";
    node->mrn_value_type        = bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_mon_get_func      = md_ca_files_get;
    node->mrn_mon_iterate_func  = md_ca_files_iterate;
    node->mrn_description       = "List of CA files available";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /* Monitoring Nodes */


    /* Upgrade processing nodes */

    err = md_upgrade_rule_array_new(&md_ssld_cfg_ug_rules);
    bail_error(err);
    ra = md_ssld_cfg_ug_rules;

    /*! Added in module version 2
     */

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type              =       MUTT_ADD;
    rule->mur_name                      =       "/nkn/ssld/config/origin-req/cipher";
    rule->mur_new_value_type            =       bt_string;
    rule->mur_new_value_str             =       "DEFAULT";
    rule->mur_cond_name_does_not_exist  =       true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type              =       MUTT_ADD;
    rule->mur_name                      =       "/nkn/ssld/config/origin-req/server_auth";
    rule->mur_new_value_type            =       bt_bool;
    rule->mur_new_value_str             =       "false";
    rule->mur_cond_name_does_not_exist  =       true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type              =       MUTT_ADD;
    rule->mur_name                      =       "/nkn/ssld/config/network/threads";
    rule->mur_new_value_type            =       bt_uint32;
    rule->mur_new_value_str             =       "2";
    rule->mur_cond_name_does_not_exist  =       true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_DELETE;
    rule->mur_name =           "/nkn/ssld/config/client_auth";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type 		=	MUTT_ADD;
    rule->mur_name 			=       "/nkn/ssld/config/client-req/client_auth";
    rule->mur_new_value_type            =       bt_bool;
    rule->mur_new_value_str             =       "false";
    rule->mur_cond_name_does_not_exist  =       true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type 		=	MUTT_ADD;
    rule->mur_name 			=       "/nkn/ssld/config/ca-certificate/*";
    rule->mur_new_value_type            =       bt_string;
    rule->mur_new_value_str             =       "";
    rule->mur_cond_name_does_not_exist  =       true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type 		=	MUTT_ADD;
    rule->mur_name 			=       "/nkn/ssld/config/ca-certificate/*/time_stamp";
    rule->mur_new_value_type            =       bt_int32;
    rule->mur_new_value_str             =       "0";
    rule->mur_cond_name_does_not_exist  =       true;
    MD_ADD_RULE(ra);
bail:
    return(err);
}

/*----------------------------------------------------------------------------
 * LOCAL FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
static int
md_ssld_cfg_import_cert(md_commit *commit,
		const mdb_db *db,
		const char *action_name,
		bn_binding_array *params,
		void *cb_arg)
{

    int err = 0;
    tstring *t_name = NULL;
    tstring *t_type = NULL;
    tstring *t_passphrase = NULL;
    str_value_t tmp_str = {0};
    ssl_cfg_cert_cb_arg_t* ssl_cb_arg = NULL;

    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;

    lr = malloc(sizeof(*lr));
    bail_null(lr);
    memset(lr, 0, sizeof(*lr));

    ssl_cb_arg = malloc(sizeof(ssl_cfg_cert_cb_arg_t));
    memset(ssl_cb_arg, 0, sizeof(ssl_cfg_cert_cb_arg_t));

    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, lp);

    /* 1.Fetch the cert
     * 2.Verify the cert
     * 3.Set the Nodes
     */


    err = bn_binding_array_get_value_tstr_by_name
	(params, "name", NULL, &t_name);
    bail_error_null(err, t_name);

    err = bn_binding_array_get_value_tstr_by_name
	(params, "type", NULL, &t_type);
    bail_error_null(err, t_type);

    err = bn_binding_array_get_value_tstr_by_name
	(params, "passphrase", NULL, &t_passphrase);

    snprintf(tmp_str, sizeof(tmp_str), "%s%s",
	    SSLD_CERT_PATH, ts_str(t_name));

    /* Validate certificate
     * 1)Check if cert has the key.
     * 2)Check if cert and key match (optional)
     * 3)create the nodes
     */

    err = ts_new_str(&(lp->lp_path), SSLD_CERT_VERIFY);
    bail_error(err);

    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 4,
	    SSLD_CERT_VERIFY, ts_str(t_name),
	    ts_str(t_type), ts_str_maybe_empty(t_passphrase));

    bail_error(err);

    lp->lp_wait = false;

    lp->lp_env_opts = eo_preserve_all;

    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

    err = lc_launch(lp, lr);
    bail_error(err);

    ssl_cb_arg->lr = lr;
    ssl_cb_arg->cert_name = t_name;
    ssl_cb_arg->passphrase = t_passphrase;
    ssl_cb_arg->type = strdup(ts_str(t_type));

    err = md_commit_set_completion_proc_async(commit, true);

    err = md_commit_add_completion_proc_state_action(commit,
	    lr->lr_child_pid, lr->lr_exit_status,
	    lr->lr_child_pid == -1,
            "md_ssld_cfg_import_cert",
	    md_ssld_cfg_import_cert_finalize,
	    ssl_cb_arg);
    bail_error(err);

bail:
    lc_launch_params_free(&lp);
    /* not freeing the below*/
    /*
       t_name
       t_type
       t_passphrase
     */
    if(lr && err !=0 ) {
	lc_launch_result_free_contents(lr);
	safe_free(lr);
    }
    return err;

}

static int
md_ssld_cfg_import_cert_finalize(md_commit *commit, const mdb_db *db,
		const char *action_name, void *cb_arg,
		void *finalize_arg, pid_t pid,
		int wait_status, tbool completed)
{
    int err = 0;
    ssl_cfg_cert_cb_arg_t* ssl_cb_arg = NULL;
    lc_launch_result *lr = NULL;
    const tstring *output = NULL;
    tstring *msg = NULL;
    uint16 code = 0;


    bn_request *req = NULL;
    bn_binding *binding = NULL;

    bail_null(commit);
    bail_null(finalize_arg);
    ssl_cb_arg = (ssl_cfg_cert_cb_arg_t*)finalize_arg;

    bail_null(ssl_cb_arg->cert_name);
    bail_null(ssl_cb_arg->passphrase);
    bail_null(ssl_cb_arg->type);

    lr = (lc_launch_result*)ssl_cb_arg->lr;
    bail_null(lr);

    if(completed) {
	lr->lr_child_pid = -1;
	lr->lr_exit_status = wait_status;
    }

    err = lc_launch_complete_capture(lr);
    bail_error(err);

    err = lc_check_exit_status_full(lr->lr_exit_status, NULL, LOG_INFO,
	    LOG_WARNING, LOG_ERR,
	    SSLD_CERT_VERIFY);
    bail_error(err);

    output = lr->lr_captured_output;
    bail_null(output);

    if(strcmp(ts_str(output), "Valid\n") == 0) {
	/*set the nodes */
	err = bn_action_request_msg_create(&req,
		"/nkn/ssld/action/set_vals");
	bail_error_null(err, req);

	err = bn_action_request_msg_append_new_binding
	    (req, 0, "name", bt_string, ts_str(ssl_cb_arg->cert_name), NULL);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding
	    (req, 0, "passphrase", bt_string, ts_str(ssl_cb_arg->passphrase), NULL);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding
	    (req, 0, "type", bt_string, ssl_cb_arg->type, NULL);
	bail_error(err);

	err = md_commit_handle_action_from_module(
		commit, req, NULL, &code, &msg, NULL, NULL);
	bail_error(err);
	if (code ) {
	    err = md_commit_set_return_status_msg_fmt(commit, 1,
		    _("Import Failed : %s"), ts_str_maybe_empty(msg));
	    bail_error(err);

	} else {

	    err = bn_binding_new_str(&binding, "Result", ba_value, bt_string, 0, "Import Successful");
	    bail_error(err);
	}

    } else {
	const char *ret_msg;
	if(strcmp(ssl_cb_arg->type, "1") == 0) {
	    ret_msg = "Certificate Validation Failed";
	} else if (strcmp(ssl_cb_arg->type, "2") == 0) {
	    ret_msg = "Key Validation Failed";
	} else if (strcmp(ssl_cb_arg->type, "3") == 0) {
	    ret_msg = "CSR Validation Failed";
	} else if (strcmp(ssl_cb_arg->type, "4") == 0) {
	    ret_msg = "CA Validation Failed";
	} else {
	    ret_msg = "Error Occured";
	}

	err = bn_binding_new_str(&binding, "Result", ba_value, bt_string, 0, ret_msg);
	bail_error(err);

	lc_log_basic(LOG_NOTICE, "Verfication failed - %s object", ts_str(output));

    }

    err = md_commit_binding_append(commit, 0, binding);
    bail_error(err);


bail:
    bn_binding_free(&binding);
    bn_request_msg_free(&req);

    if (lr) {
	lc_launch_result_free_contents(lr);
	safe_free(lr);
    }
    if(ssl_cb_arg) {
	ts_free(&ssl_cb_arg->cert_name);
	ts_free(&ssl_cb_arg->passphrase);
	safe_free(ssl_cb_arg->type);
	safe_free(ssl_cb_arg);
    }
    return err;
}

/*
Should be used like below
static int
md_ssld_cfg_is_cert_valid(md_commit *commit, const mdb_db *old_db, tbool *valid,
					const char *match_val, const char *nd_fmt, ...)
*/
int
md_ssld_cfg_is_cert_valid(md_commit *commit, const mdb_db *db, tbool *valid,
					const char *match_val, const char *nd_fmt, const char*val)
{
    int err = 0;
    str_value_t str = {0};
    bn_binding *binding = NULL;
    tstring *value = NULL;

    bail_null(nd_fmt);
    snprintf(str, sizeof(str), nd_fmt, val);

    err = mdb_get_node_binding(commit, db, str, 0, &binding);
    bail_error(err);

    if(!match_val && binding) {

	*valid = true;

    } else if (binding) {
	err = bn_binding_get_tstr(binding, ba_value, 0, 0, &value);
	bail_error(err);

	if(value && ts_equal_str(value, match_val, false)) {
	    *valid = true;
	}
    }

bail:
    bn_binding_free(&binding);
    ts_free(&value);
    return err;
}

static int
md_ssld_cfg_cert_is_bound_to_vhost(md_commit *commit, const mdb_db *db,
					const char *cert_name,tbool *bound)
{
    int err = 0;
    node_name_t host_nd = {0};
    tstr_array *host_map = NULL;

    snprintf(host_nd, sizeof(host_nd), "/nkn/ssld/config/certificate/%s/host/*",
	    cert_name);

    err = mdb_get_matching_tstr_array(commit,
	    db,
	    host_nd,
	    0,
	    &host_map);
    bail_error(err);

    if(host_map && tstr_array_length_quick(host_map) ) {

	*bound = true;
    }

bail:
    tstr_array_free(&host_map);
    return err;
}

static int
md_ssld_cfg_key_is_bound_to_vhost(md_commit *commit, const mdb_db *db,
					const char *key_name,tbool *bound)
{
    int err = 0;
    node_name_t host_nd = {0};
    tstr_array *host_map = NULL;

    snprintf(host_nd, sizeof(host_nd), "/nkn/ssld/config/key/%s/host/*",
	    key_name);

    err = mdb_get_matching_tstr_array(commit,
	    db,
	    host_nd,
	    0,
	    &host_map);
    bail_error(err);

    if(host_map && tstr_array_length_quick(host_map) ) {

	*bound = true;
    }

bail:
    tstr_array_free(&host_map);
    return err;
}


static int
md_ssld_cfg_virthost_has_cert_bound(md_commit *commit, const mdb_db *old_db,
					const char *virt_host,tbool *bound)
{
    int err = 0;
    tstring *t_cert_name = NULL;
    uint16_t j = 0;
    node_name_t cert = {0};
    *bound = false;

    snprintf(cert, sizeof(cert), "/nkn/ssld/config/virtual_host/%s/cert",
	    virt_host);
    err = mdb_get_node_value_tstr(NULL,
	    old_db,
	    cert,
	    0,
	    NULL,
	    &t_cert_name);
    bail_error(err);


    if(!ts_equal_str(t_cert_name, "", false)){
	*bound = true;
    }

bail:
    ts_free(&t_cert_name);
    return err;
}

static int
md_ssld_cfg_virthost_has_key_bound(md_commit *commit, const mdb_db *old_db,
					const char *virt_host,tbool *bound)
{
    int err = 0;
    tstring *t_key_name = NULL;
    uint16_t j = 0;
    node_name_t key  = {0};
    *bound = false;

    snprintf(key, sizeof(key), "/nkn/ssld/config/virtual_host/%s/key",
	    virt_host);
    err = mdb_get_node_value_tstr(NULL,
	    old_db,
	    key,
	    0,
	    NULL,
	    &t_key_name);
    bail_error(err);


    if(!ts_equal_str(t_key_name, "", false)){
	*bound = true;
    }

bail:
    ts_free(&t_key_name);
    return err;
}

static int
md_ssld_cfg_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg)
{
    int err = 0;
    int i, num_changes = 0;
    const mdb_db_change *change = NULL;
    tbool one_shot, initial;

    err = md_commit_get_commit_mode(commit, &one_shot);
    bail_error(err);

    err = md_commit_get_commit_initial(commit, &initial);
    bail_error(err);

    if(!one_shot && initial) {
	goto bail;
    }
    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			"nkn", "ssld", "config", "virtual_host", "*", "key")))
	{


	    const 	bn_attrib *new_val = NULL;
	    const 	bn_attrib *old_val = NULL;
	    tstring *ts_old_val = NULL;
	    tstring *ts_new_val = NULL;

	    const char *old_key_name = NULL;
	    const char *new_key_name = NULL;


	    if(change->mdc_old_attribs) {
		old_val = bn_attrib_array_get_quick(change->mdc_old_attribs,
			ba_value);

		err = bn_attrib_get_tstr(old_val, NULL, bt_string,
			NULL, &ts_old_val);
		bail_error(err);
		old_key_name = ts_str(ts_old_val);

		/* add the new val to the list mainitained by key object
		   remove the old val from the same list */
		//	if(ts_old_val && !ts_equal_str(ts_old_val, "", false)) {
		/* remove the old entry */
		const char *virt_host = NULL;
		virt_host = tstr_array_get_str_quick(change->mdc_name_parts, 4);

		err = mdb_set_node_str(commit, inout_new_db,
			bsso_delete, 0, bt_none,
			"",
			"/nkn/ssld/config/key/%s/host/%s",
			old_key_name, virt_host);
		bail_error(err);



		//	}
	    }

	    if(change->mdc_new_attribs) {
		const char *virt_host = NULL;
		tbool is_valid = false;

		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);

		err = bn_attrib_get_tstr(new_val, NULL, bt_string,
			NULL, &ts_new_val);
		bail_error(err);

		if(ts_equal_str(ts_new_val, "", false))
		    continue;

		new_key_name = ts_str(ts_new_val);

		//	if(ts_new_val && !ts_equal_str(ts_new_val, "", false)) {
		/* remove the old entry */
		virt_host = tstr_array_get_str_quick(change->mdc_name_parts, 4);

		err = md_ssld_cfg_is_cert_valid(commit,inout_new_db,
			&is_valid, NULL,"/nkn/ssld/config/key/%s", new_key_name);
		bail_error(err);

		if(is_valid) {
		    err = mdb_set_node_str(commit, inout_new_db,
			    bsso_create, 0, bt_string,
			    virt_host,
			    "/nkn/ssld/config/key/%s/host/%s",
			    new_key_name, virt_host);
		    bail_error(err);
		}


		//	}
	    }
	    ts_free(&ts_old_val);
	    ts_free(&ts_new_val);

	} else if ( (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5,
			"nkn", "ssld", "config", "certificate", "*")))
	{
	} else if ( (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5,
			"nkn", "ssld", "config", "key", "*")))
	{
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			"nkn", "ssld", "config", "virtual_host", "*", "cert"))) {


	    const 	bn_attrib *new_val = NULL;
	    const 	bn_attrib *old_val = NULL;
	    tstring *ts_old_val = NULL;
	    tstring *ts_new_val = NULL;

	    const char *old_cert_name = NULL;
	    const char *new_cert_name = NULL;


	    if(change->mdc_old_attribs) {
		old_val = bn_attrib_array_get_quick(change->mdc_old_attribs,
			ba_value);

		err = bn_attrib_get_tstr(old_val, NULL, bt_string,
			NULL, &ts_old_val);
		bail_error(err);
		old_cert_name = ts_str(ts_old_val);

		/* add the new val to the list mainitained by cert object
		   remove the old val from the same list */
		//	if(ts_old_val && !ts_equal_str(ts_old_val, "", false)) {
		/* remove the old entry */
		const char *virt_host = NULL;
		virt_host = tstr_array_get_str_quick(change->mdc_name_parts, 4);

		err = mdb_set_node_str(commit, inout_new_db,
			bsso_delete, 0, bt_none,
			"",
			"/nkn/ssld/config/certificate/%s/host/%s",
			old_cert_name, virt_host);
		bail_error(err);



		//	}
	    }

	    if(change->mdc_new_attribs) {
		const char *virt_host = NULL;
		tbool is_valid = false;

		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);

		err = bn_attrib_get_tstr(new_val, NULL, bt_string,
			NULL, &ts_new_val);
		bail_error(err);

		if(ts_equal_str(ts_new_val, "", false))
		    continue;

		new_cert_name = ts_str(ts_new_val);

		//	if(ts_new_val && !ts_equal_str(ts_new_val, "", false)) {
		/* remove the old entry */
		virt_host = tstr_array_get_str_quick(change->mdc_name_parts, 4);

		err = md_ssld_cfg_is_cert_valid(commit,inout_new_db,
			&is_valid, NULL,"/nkn/ssld/config/certificate/%s", new_cert_name);
		bail_error(err);

		if(is_valid) {
		    err = mdb_set_node_str(commit, inout_new_db,
			    bsso_create, 0, bt_string,
			    virt_host,
			    "/nkn/ssld/config/certificate/%s/host/%s",
			    new_cert_name, virt_host);
		    bail_error(err);
		}
		//	}
	    }
	    ts_free(&ts_old_val);
	    ts_free(&ts_new_val);

	}
    }
bail:
    return err;
}


static int
md_ssld_cfg_commit_check(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    const char *virt_host = NULL;
    const char *cert_name = NULL;
    int i, num_changes = 0;
    const mdb_db_change *change = NULL;

    /*
     * No checking to be done.
     */
    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

	if ( (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "ssld", "config", "virtual_host", "*")))
	{
	    virt_host = tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    bail_null(virt_host);
	    if(mdct_delete == change->mdc_change_type) {

		tbool bound = false;
		err = md_ssld_cfg_virthost_has_cert_bound(commit,old_db,
			virt_host,&bound);

		if(bound) {
		    err = md_commit_set_return_status_msg_fmt(
			    commit, 1,
			    _("error: Virtual Host has certificate bound to it,Please unbind\n") );
		    bail_error(err);
		    goto bail;
		}
		bound = false;
		err = md_ssld_cfg_virthost_has_key_bound(commit,old_db,
			virt_host,&bound);


		if(bound) {
		    err = md_commit_set_return_status_msg_fmt(
			    commit, 1,
			    _("error: Virtual Host has key bound to it,Please unbind\n") );
		    bail_error(err);
		    goto bail;
		}


	    }

	} else if ( (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			"nkn", "ssld", "config", "virtual_host", "*", "cert")))
	{
	    const 	bn_attrib *new_val = NULL;
	    tstring *value = NULL;
	    if(mdct_add == change->mdc_change_type ||
		    mdct_modify == change->mdc_change_type) {
		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);

		err = bn_attrib_get_tstr(new_val, NULL, bt_string,
			NULL, &value);
		bail_error_null(err, value);

		//If its a node rst,Do nothing
		if(ts_equal_str(value, "", false))
		    continue;
		cert_name = ts_str(value);

		tbool is_valid = false;

		err = md_ssld_cfg_is_cert_valid(commit,new_db,
			&is_valid, NULL,"/nkn/ssld/config/certificate/%s", cert_name);

		if(!is_valid) {
		    err = md_commit_set_return_status_msg_fmt(
			    commit, 1,
			    _("error:Invalid certificate\n"));
		    bail_error(err);
		    goto bail;
		}

	    }
	    ts_free(&value);
	} else if ( (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5,
			"nkn", "ssld", "config", "certificate", "*")))
	{

	    cert_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);;
	    bail_null(cert_name);
	    if(mdct_delete == change->mdc_change_type) {
		tbool bound = false;

		err = md_ssld_cfg_cert_is_bound_to_vhost(commit, old_db,
			cert_name, &bound);
		bail_error(err);

		if(bound) {
		    err = md_commit_set_return_status_msg_fmt(
			    commit, 1,
			    "error:Certificate bound to a Virtual-Host.Please unbind\n");
		    bail_error(err);
		    goto bail;
		}

	    }
	}
	else if ( (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			"nkn", "ssld", "config", "virtual_host", "*", "key")))
	{
	    const 	bn_attrib *new_val = NULL;
	    tstring *value = NULL;
	    if(mdct_add == change->mdc_change_type ||
		    mdct_modify == change->mdc_change_type) {
		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);

		err = bn_attrib_get_tstr(new_val, NULL, bt_string,
			NULL, &value);
		bail_error_null(err, value);

		//If its a node rst,Do nothing
		if(ts_equal_str(value, "", false))
		    continue;
		cert_name = ts_str(value);

		tbool is_valid = false;

		err = md_ssld_cfg_is_cert_valid(commit,new_db,
			&is_valid, NULL,"/nkn/ssld/config/key/%s", cert_name);

		if(!is_valid) {
		    err = md_commit_set_return_status_msg_fmt(
			    commit, 1,
			    _("error:Invalid key\n") );
		    bail_error(err);
		    goto bail;
		}

	    }
	    ts_free(&value);
	} else if ( (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5,
			"nkn", "ssld", "config", "key", "*")))
	{

	    cert_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);;
	    bail_null(cert_name);
	    if(mdct_delete == change->mdc_change_type) {
		tbool bound = false;

		err = md_ssld_cfg_key_is_bound_to_vhost(commit, old_db,
			cert_name, &bound);
		bail_error(err);

		if(bound) {
		    err = md_commit_set_return_status_msg_fmt(
			    commit, 1,
			    "error:Key bound to a Virtual-Host.Please unbind\n");
		    bail_error(err);
		    goto bail;
		}

	    }
	} else if  ( (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			"nkn", "ssld", "config", "virtual_host", "*", "cipher")) ||
		(bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "ssld", "config", "origin-req", "cipher"))) {
	    if(mdct_modify == change->mdc_change_type) {
		const 	bn_attrib *new_val = NULL;
		tstring *value = NULL;
		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);
		err = bn_attrib_get_tstr(new_val, NULL, bt_string,
			NULL, &value);
		if (value && (ts_length(value) > 0)){
		    err = md_ssld_validate_cipher(commit, value);
		    bail_error(err);

		}

	    }
	} else if ( (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5,
			"nkn", "ssld", "config", "ca-certificate", "*")))
	{

	    cert_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);;
	    bail_null(cert_name);

	    if(!(lc_is_suffix(".pem", cert_name, false))) {
		err = md_commit_set_return_status_msg_fmt(
			commit, 1,
			"CA-Certificate name should have .pem file extension");
		bail_error(err);
		goto bail;
	    }
	}


    }
    err = md_commit_set_return_status(commit,
	    0);
    bail_error(err);

bail:
    return(err);
}

static int
md_ssld_cfg_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    bn_binding *binding = NULL;

    err = mdb_create_node_bindings
	(commit, db, md_ssld_cfg_initial_values,
	 sizeof(md_ssld_cfg_initial_values) / sizeof(bn_str_value));
    bail_error(err);

bail:
    bn_binding_free(&binding);
    return(err);
}
/*
 * This function validates if the cipher string entered by user are
 * valid.
 * If error is encountered, the error is printed by this function and the
 * commit for the node is terminated.
 */
static int
md_ssld_validate_cipher(md_commit *commit, tstring *cipher)
{
    int err = 0;
    tstr_array *cipher_arr = NULL;
    int num_params = 0, i ;
    str_value_t uniq_cipher = {0};


    if (!cipher) {
	err = 1;
	goto bail;
    }


    /* Tokenize the user-input string seperated by ':'
     * each token is compared against the allowed cipher-string-list
     */
    err = ts_tokenize(cipher, ':', '\0', '\0', ttf_ignore_trailing_separator, &cipher_arr);
    bail_error(err);
    bail_null(cipher_arr);

    num_params = tstr_array_length_quick(cipher_arr);

    for(i = 0;i <num_params; i++){
	const char *value = tstr_array_get_str_quick(cipher_arr, i);
	if (value) {
	    char *isPresent = NULL;
	    /*
	     * Ensure that the complete cipher text has been entered
	     * Partial cipher -text will be considered as an error
	     */
	    snprintf(uniq_cipher, sizeof(uniq_cipher), "%s:", value);
	    isPresent = strstr(SUPPORTED_CIPHER_LIST, uniq_cipher);
	    if (!isPresent) {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("error: Invalid cipher :%s\n"), value );
		err = 1;
		goto bail;
	    }
	    //safe_free(isPresent);
	}
    }

bail:
    tstr_array_free(&cipher_arr);
    return err;
}
static int
md_ca_files_get(md_commit *commit, const mdb_db *db,
                     const char *node_name,
                     const bn_attrib_array *node_attribs,
                     bn_binding **ret_binding, uint32 *ret_node_flags,
                     void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *filename = NULL;

    /* XXX/EMT: should validate filename */

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error(err);

    num_parts = tstr_array_length_quick(parts);
    bail_require(num_parts == 4);

    filename = tstr_array_get_str_quick(parts, 3);
    bail_null(filename);

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string, 0,
	    filename);
    bail_error(err);

bail:
    tstr_array_free(&parts);
    return(err);
}

static int
md_ca_files_iterate(md_commit *commit, const mdb_db *db,
                         const char *parent_node_name,
                         const uint32_array *node_attrib_ids,
                         int32 max_num_nodes, int32 start_child_num,
                         const char *get_next_child,
                         mdm_mon_iterate_names_cb_func iterate_cb,
                         void *iterate_cb_arg, void *arg)
{
    int err = 0;
    tstr_array *names = NULL;
    const char *name = NULL;
    const char *pattern = NULL;
    uint32 num_names = 0, i = 0;

    /*
     * XXX/EMT to do:
     *   - have wildcard be a numeric ID; make absolute path a child
     *   - also have datetime children that say when the first and last
     *     entries in the log file are
     *   - return the current log file too with ID 0.
     */

    err = lf_dir_list_names("/config/nkn/ca", &names);
    bail_error_null(err, names);


    num_names = tstr_array_length_quick(names);
    for (i = 0; i < num_names; ++i) {
	name = tstr_array_get_str_quick(names, i);
	bail_null(name);
	if ((lc_is_suffix(".pem", name, false)) ){
	    err = (*iterate_cb)(commit, db, name, iterate_cb_arg);
	    bail_error(err);
	}
    }

bail:
    tstr_array_free(&names);
    return(err);
}

/* ---------------------------------END OF FILE---------------------------------------- */

