/*
 * Filename :   cli_nvsd_cluster_cmds.c
 * Date     :   2010/08/24
 * Author   :   Manikandan`Vengatachalam
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
#include "nkn_mgmt_defs.h"
#include "libnkncli.h"
#include "nkn_defs.h"

enum {
	crso_add_cluster = 1,
	crso_cluster_method = 2,
	crso_cluster_loadbalance = 3,
};

int
cli_server_map_get_names(
        tstr_array **ret_names,
        tbool hide_display);

/*---------------------------------------------------------------------------*/
int 
cli_cluster_common_req(bn_request *req, const char *cl_name,
	const char *cl_type, const char *rr_method);
static int
cli_nvsd_ns_cluster_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_nvsd_cluster_smap_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

static int
cli_cluster_is_configured(const bn_binding_array *bindings,
	const char *clusterName, tbool *configured);

static int
cli_nvsd_redirect_local_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply);

static int
cli_nvsd_replicate_mode_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply);


int
cli_nvsd_cluster_hash_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int
cli_nvsd_cluster_load_thres_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply);

int
cli_nvsd_cluster_redir_addr_revmap
(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply);

int
cli_nvsd_cluster_redir_path_revmap
(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply);

int
cli_nvsd_cluster_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int cli_nvsd_cluster_dupchk(
		const bn_binding_array *bindings,
		uint32 idx,
		const bn_binding *binding,
		const tstring *name,
		const tstr_array *name_components,
		const tstring *name_last_part,
		const tstring *value,
		void *callback_data);

int
cli_nvsd_ns_cluster_add(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_nvsd_cluster_smap_add(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_nvsd_ns_cluster_delete(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_nvsd_cluster_show_cmd(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);

int
cli_cluster_name_help(
        void *data,
        cli_help_type help_type,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        void *context);

int
cli_cluster_name_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

int
cli_ns_add_cluster_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

static int
cli_cluster_get_names(
        tstr_array **ret_names,
        tbool hide_display);

static int
cli_cluster_name_validate(
        const char *cluster,
        tbool *ret_valid);
int
cli_cluster_name_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int
cli_cluster_chash_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_cluster_loadbalance_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_cluster_lb_replicate_callback(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
int
cli_cluster_hash_callback(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_cluster_addrport_callback(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_cluster_path_callback(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_cluster_local_callback(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

int
cli_cluster_rr_from_lb_to_chr(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
int
cli_cluster_is_valid_name(const char *name, tstring **ret_msg);

int
cli_smap_cluster_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

int cli_nvsd_suffix_show(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
static int
cli_cluster_handle_suffix(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

static int
cli_nvsd_suffix_delete(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
static int
cli_cluster_suffix_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply);
static int
cli_suffix_show_one(const bn_binding_array *bindings, uint32 idx,
	const bn_binding *binding, const tstring *name,
	const tstr_array *name_components,
	const tstring *name_last_part,
	const tstring *value, void *callback_data);
int
cli_cluster_check_namepsace(const char *ns_name, const char *cl_name,
	uint32 *error, error_msg_t *err_msg);
int
cli_show_cl_suffix(const char *ns_name, const char *cl_name, const char *suff_name,
	tbool print_msg);
int
cli_nvsd_cluster_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = cli_set_gettext_domain(GETTEXT_DOMAIN);
    bail_error(err);
#endif

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cluster";
    cmd->cc_help_descr =        N_("Cluster configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no cluster";
    cmd->cc_help_descr =        N_("Removes Cluster configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cluster create";
    cmd->cc_help_descr =        N_("configuring cluster");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no cluster create";
    cmd->cc_help_descr =        N_("configuring cluster");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cluster create *";
    cmd->cc_help_exp =          N_("<cluster name>");
    cmd->cc_help_exp_hint =     N_("Configure a name for the cluster");
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_help_callback =     cli_cluster_name_help;
    cmd->cc_comp_pattern = 		"/nkn/nvsd/cluster/config/*";
    cmd->cc_comp_type = 		cct_matching_names;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_cluster_name_enter_mode;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/cluster/config/$1$";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_clustermap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no cluster create *";
    cmd->cc_help_exp =          N_("cluster name");
    cmd->cc_help_exp_hint =     N_("Clear the cluster name");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_help_use_comp = 	true;
    cmd->cc_comp_pattern = 	"/nkn/nvsd/cluster/config/*";
    cmd->cc_comp_type = 	cct_matching_names;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_delete;
    cmd->cc_exec_name =         "/nkn/nvsd/cluster/config/$1$";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "cluster create * no";
    cmd->cc_help_descr =        N_("Clear/remove existing cluster configuration options");
    cmd->cc_req_prefix_count =  2;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cluster create * server-map";
    cmd->cc_help_descr =        N_("Associating a server-map with the cluster");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cluster create * server-map *";
    cmd->cc_help_exp =          N_("<server-map name>");
    cmd->cc_help_exp_hint =     N_("Associate a server-map to the cluster");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_callback =     cli_smap_cluster_comp;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback  =	cli_nvsd_cluster_smap_add;
    cmd->cc_revmap_order =      cro_clustermap;
    cmd->cc_revmap_type =		crt_manual;
    cmd->cc_revmap_names =		"/nkn/nvsd/cluster/config/*/server-map";
	cmd->cc_revmap_callback = 	cli_nvsd_cluster_smap_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no cluster create * server-map";
    cmd->cc_help_descr =        N_("Remove the associated server-map from the the cluster");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/cluster/config/$1$/server-map";
    cmd->cc_exec_value =        "";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * add";
    cmd->cc_help_descr =        N_("Associating namespace with cluster");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * add";
    cmd->cc_help_descr =        N_("Remove the association of namespace with cluster");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * add cluster";
    cmd->cc_help_descr =        N_("Associating namespace with cluster");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * add cluster *";
    cmd->cc_help_exp =          N_("<cluster name>");
    cmd->cc_help_exp_hint =     N_("<Association of cluster with namespace>");
    cmd->cc_help_use_comp = 	true;
    cmd->cc_comp_callback =     cli_ns_add_cluster_comp;
    cmd->cc_flags =		        ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_basic_curr;
    cmd->cc_exec_callback  =	cli_nvsd_ns_cluster_add;
    cmd->cc_revmap_type =		crt_manual;
    cmd->cc_revmap_order =		cro_namespace;
    cmd->cc_revmap_suborder =   crso_add_cluster;
    cmd->cc_revmap_names =		"/nkn/nvsd/namespace/*/cluster/*/cl-name";
	cmd->cc_revmap_callback = 	cli_nvsd_ns_cluster_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * add cluster";
    cmd->cc_help_descr =        N_("Removing the association of cluster with namespace");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * add cluster *";
    cmd->cc_help_exp =          N_("<cluster name>");
    cmd->cc_help_exp_hint = 		N_("<Removing the association of cluster with namespace>");
    cmd->cc_help_use_comp = 	true;
    cmd->cc_comp_pattern = 	"/nkn/nvsd/namespace/$1$/cluster/*/cl-name";
    cmd->cc_comp_type = 	cct_matching_values;
    cmd->cc_flags =	        ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_basic_curr;
    cmd->cc_exec_callback  =	cli_nvsd_ns_cluster_delete;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster";
    cmd->cc_help_descr =        N_("Configure cluster options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster *";
    cmd->cc_help_exp =          N_("<cluster-Name>");
    cmd->cc_help_exp_hint =     N_("Configuration options for the associated cluster");
    cmd->cc_help_use_comp = 	true;
    cmd->cc_comp_pattern = 		"/nkn/nvsd/namespace/$1$/cluster/*/cl-name";
    cmd->cc_comp_type = 		cct_matching_values;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing";
    cmd->cc_help_descr =        N_("Configure cluster request-routing options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method";
    cmd->cc_help_descr =        N_("Configure cluster request-routing method");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-redirect";
    cmd->cc_help_descr =        N_("Configure cluster request-routing method as consistent-hash-redirect");
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_chash_enter_mode;
    cmd->cc_exec_data =		(void *)"redirect";
    cmd->cc_revmap_type =	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-redirect cluster-hash";
    cmd->cc_help_descr =        N_("Cluster Hash Parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-redirect cluster-hash base-url";
    cmd->cc_help_descr =        N_("Cluster Hash base-url");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_hash_callback;
    cmd->cc_exec_data =		(void *)"redirect";
    cmd->cc_magic	=	1;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-redirect cluster-hash complete-url";
    cmd->cc_help_descr =        N_("Cluster Hash complete-url");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_hash_callback;
    cmd->cc_exec_data =		(void *)"redirect";
    cmd->cc_magic	=	2;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_cluster_method;
    cmd->cc_revmap_names = 		"/nkn/nvsd/cluster/config/*/cluster-hash";
    cmd->cc_revmap_callback =	cli_nvsd_cluster_hash_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-redirect redirect-addr-port";
    cmd->cc_help_descr =        N_("Configure cluster redirect address:port");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-redirect redirect-addr-port *";
    cmd->cc_help_exp =          N_("<address:port>");
    cmd->cc_help_exp_hint =     N_("Configure cluster redirect address:port");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_addrport_callback;
    cmd->cc_exec_data  =	(void *) "redirect";
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_cluster_method;
    cmd->cc_revmap_names =	"/nkn/nvsd/cluster/config/*/redirect-addr-port";
    cmd->cc_revmap_callback =	cli_nvsd_cluster_redir_addr_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-redirect redirect-local";
    cmd->cc_help_descr =        N_("Configure cluster redirect local options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-redirect redirect-local allow";
    cmd->cc_help_descr =        N_("Enable the redirect-local mode for the cluster");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_local_callback;
    cmd->cc_magic	=	1;
    cmd->cc_exec_data =		(void *) "redirect";
    cmd->cc_revmap_type =	crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_cluster_method;
    cmd->cc_revmap_names =	"/nkn/nvsd/cluster/config/*/redirect-local";
    cmd->cc_revmap_callback =   cli_nvsd_redirect_local_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-redirect redirect-local deny";
    cmd->cc_help_descr =        N_("Disable the redirect-local mode for the cluster");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_local_callback;
    cmd->cc_exec_data =		(void *) "redirect";
    cmd->cc_magic	=			2;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-redirect redirect-base-path";
    cmd->cc_help_descr =        N_("Configure cluster redirect base path");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-redirect redirect-base-path *";
    cmd->cc_help_exp =          N_("<Base URL>");
    cmd->cc_help_exp_hint =     N_("Configure cluster redirect base path");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_path_callback;
    cmd->cc_exec_data =		(void *) "redirect";
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_cluster_method;
    cmd->cc_revmap_names = 	"/nkn/nvsd/cluster/config/*/redirect-base-path";
    cmd->cc_revmap_callback =	cli_nvsd_cluster_redir_path_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing load-balance";
    cmd->cc_help_descr =        N_("Configure cluster request-routing as load-balance");
    cmd->cc_flags =             ccf_terminal  | ccf_prefix_mode_root;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_loadbalance_enter_mode;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * cluster";
    cmd->cc_help_descr =        N_("Negates cluster request-routing options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * cluster *";
    cmd->cc_help_exp =          N_("Negates cluster request-routing options");
    cmd->cc_help_use_comp = 	true;
    cmd->cc_comp_pattern = 	"/nkn/nvsd/namespace/$1$/cluster/*/cl-name";
    cmd->cc_comp_type = 	cct_matching_values;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * cluster * request-routing";
    cmd->cc_help_descr =        N_("Negates cluster request-routing options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * cluster * request-routing load-balance";
    cmd->cc_help_descr =        N_("Configures cluster request-routing from load-balance to consistent-hash-redirect");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_rr_from_lb_to_chr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing load-balance load-threshold";
    cmd->cc_help_descr =        N_("Configure cluster load balance threshold");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing load-balance load-threshold *";
    cmd->cc_help_exp =          N_("<threshold value(integer)>");
    cmd->cc_help_exp_hint =     N_("Configure cluster load balance threshold");
    cmd->cc_flags =             ccf_terminal|ccf_ignore_length;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/cluster/config/$2$/load-threshold";
    cmd->cc_exec_value =        "$3$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_name2 =        "/nkn/nvsd/cluster/config/$2$/request-routing-method";
    cmd->cc_exec_value2 =       "2";
    cmd->cc_exec_type2 =        bt_uint32;
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_cluster_loadbalance;
    cmd->cc_revmap_names = 	"/nkn/nvsd/cluster/config/*/load-threshold";
    cmd->cc_revmap_callback =	cli_nvsd_cluster_load_thres_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing load-balance replicate-mode";
    cmd->cc_help_descr =        N_("Configure cluster replicate mode");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing load-balance replicate-mode least-loaded";
    cmd->cc_help_descr =          N_("Configure cluster replicate mode as least-loaded");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/cluster/config/$2$/replicate-mode";
    cmd->cc_exec_value =        "1";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_name2 =        "/nkn/nvsd/cluster/config/$2$/request-routing-method";
    cmd->cc_exec_value2 =       "2";
    cmd->cc_exec_type2 =        bt_uint32;
    cmd->cc_revmap_type =	crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_suborder =   crso_cluster_loadbalance;
    cmd->cc_revmap_names =	"/nkn/nvsd/cluster/config/*/replicate-mode";
    cmd->cc_revmap_callback =   cli_nvsd_replicate_mode_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing load-balance replicate-mode random";
    cmd->cc_help_descr =        N_("Configure cluster replicate mode as random");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/cluster/config/$2$/replicate-mode";
    cmd->cc_exec_value =        "2";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_name2 =        "/nkn/nvsd/cluster/config/$2$/request-routing-method";
    cmd->cc_exec_value2 =       "2";
    cmd->cc_exec_type2 =        bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show cluster";
    cmd->cc_help_descr =	N_("Displays Cluster Configurations");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "show cluster *";
    cmd->cc_help_exp =          N_("<cluster name>");
    cmd->cc_help_exp_hint =     "";
    cmd->cc_help_callback =     cli_cluster_name_help;
    cmd->cc_comp_callback =     cli_cluster_name_completion;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_cluster_show_cmd;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-proxy";
    cmd->cc_help_descr =        N_("Configure cluster request-routing method as consistent-hash-proxy");
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_chash_enter_mode;
    cmd->cc_exec_data =		(void *)"proxy";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-proxy cluster-hash";
    cmd->cc_help_descr =        N_("Cluster Hash Parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-proxy cluster-hash base-url";
    cmd->cc_help_descr =        N_("Cluster Hash base-url");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_hash_callback;
    cmd->cc_magic	=	1;
    cmd->cc_exec_data =		(void *)"proxy";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-proxy cluster-hash complete-url";
    cmd->cc_help_descr =        N_("Cluster Hash complete-url");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_hash_callback;
    cmd->cc_exec_data =		(void *)"proxy";
    cmd->cc_magic	=	2;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-proxy proxy-addr-port";
    cmd->cc_help_descr =        N_("Configure cluster proxy address:port");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-proxy proxy-addr-port *";
    cmd->cc_help_exp =          N_("<address:port>");
    cmd->cc_help_exp_hint =     N_("Configure cluster proxy address:port");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_addrport_callback;
    cmd->cc_exec_data	=	(void *)"proxy";
    /* revamp bindings are handled by redire-add-port */
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-proxy proxy-local";
    cmd->cc_help_descr =        N_("Configure cluster proxy local options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-proxy proxy-local allow";
    cmd->cc_help_descr =        N_("Enable the proxy-local mode for the cluster");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_local_callback;
    cmd->cc_exec_data =		(void *) "proxy";
    cmd->cc_magic	=	1;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-proxy proxy-local deny";
    cmd->cc_help_descr =        N_("Disable the proxy-local mode for the cluster");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_local_callback;
    cmd->cc_exec_data =		(void *) "proxy";
    cmd->cc_magic	=			2;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-proxy proxy-base-path";
    cmd->cc_help_descr =        N_("Configure cluster proxy base path");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * cluster * request-routing method consistent-hash-proxy proxy-base-path *";
    cmd->cc_help_exp =          N_("<Base URL>");
    cmd->cc_help_exp_hint =     N_("Configure cluster proxy base path");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_callback =     cli_cluster_path_callback;
    cmd->cc_exec_data =		(void *) "proxy";
    /* revmap bindings are handled by redir-base-path */
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * cluster * suffix-map ";
    cmd->cc_help_descr =	"Cluster L7 suffix to action map";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * cluster * suffix-map * ";
    cmd->cc_help_exp =          "<suffix name>";
    cmd->cc_help_exp_hint =     "Small case alpha numeric data";;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * cluster * suffix-map * action ";
    cmd->cc_help_descr =	"{ Redirect | Proxy } ";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * cluster * suffix-map * action redirect";
    cmd->cc_help_descr =	"";
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_cluster_handle_suffix;
    cmd->cc_exec_data =		(void *)"2";
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_namespace;
    cmd->cc_revmap_names = 	"/nkn/nvsd/cluster/config/*/suffix/*/action";
    cmd->cc_revmap_callback =	cli_cluster_suffix_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"namespace * cluster * suffix-map * action proxy";
    cmd->cc_help_descr =	"";
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_cluster_handle_suffix;
    cmd->cc_exec_data =		(void *)"1";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"show namespace * cluster";
    cmd->cc_help_descr =	"show cluster configuration";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"show namespace * cluster *";
    cmd->cc_help_exp_hint =	"show configuration of cluster";
    cmd->cc_help_exp =		"<cluster name>";
    cmd->cc_help_use_comp = 	true;
    cmd->cc_comp_pattern = 	"/nkn/nvsd/namespace/$1$/cluster/*/cl-name";
    cmd->cc_comp_type = 	cct_matching_values;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"show namespace * cluster * suffix-map";
    cmd->cc_help_descr =	"show all suffix-map configuration of cluster ";
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_suffix_show;
    cmd->cc_magic =		0;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"show namespace * cluster * suffix-map *";
    cmd->cc_help_exp_hint =	"show suffix-map configuration";
    cmd->cc_help_exp =		"<suffix name>";
    cmd->cc_help_use_comp = 	true;
    cmd->cc_comp_pattern = 	"/nkn/nvsd/cluster/config/$2$/suffix/*";
    cmd->cc_comp_type = 	cct_matching_values;
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_suffix_show;
    cmd->cc_magic =		1;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * cluster * suffix-map";
    cmd->cc_help_descr =        N_("deletes all the suffixes from the cluster");
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_suffix_delete;
    cmd->cc_magic =		0;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * cluster * suffix-map *";
    cmd->cc_help_exp_hint =	"deletes suffix form the cluster";
    cmd->cc_help_exp =		"<suffix name>";
    cmd->cc_help_use_comp = 	true;
    cmd->cc_comp_pattern = 	"/nkn/nvsd/cluster/config/$2$/suffix/*";
    cmd->cc_comp_type = 	cct_matching_values;
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =	cli_nvsd_suffix_delete;
    cmd->cc_magic =		1;
    CLI_CMD_REGISTER;

    const char *cluster_ignore_bindings[] = {
	"/nkn/nvsd/namespace/*/cluster/*",
	"/nkn/nvsd/cluster/config/*/reset",
	"/nkn/nvsd/cluster/config/*/suffix/*",
	"/nkn/nvsd/cluster/config/*/configured",
	"/nkn/nvsd/cluster/config/*/proxy-cluster-hash",
	"/nkn/nvsd/cluster/config/*/namespace",
        "/nkn/nvsd/cluster/config/*/cluster-type",
        "/nkn/nvsd/cluster/config/*/request-routing-method"
    };

    err = cli_revmap_ignore_bindings_arr(cluster_ignore_bindings);
    bail_error(err);

bail:
    return err;

}

int cli_nvsd_cluster_dupchk(const bn_binding_array *bindings, uint32 idx,
			const bn_binding *binding, const tstring *name, const tstr_array *name_components,
			const tstring *name_last_part, const tstring *value, void *callback_data)
{
    int err =0;
    tstring *cluster = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    if (idx != 0)
	cli_printf(_("\n"));

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &cluster);
    bail_error(err);

    if (safe_strcmp (ts_str(cluster) , (char *)callback_data) == 0)
    	{
			 cli_printf(_("Cluster %s already exists. Blocking the duplicate entry\n"), ts_str(cluster));
    	}

    bail:
	ts_free(&cluster);
	return(err);
}

int
cli_nvsd_cluster_smap_add(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
    int err =0;
    const char *cluster_name = NULL;
    const char *smap_name = NULL;
    char *bn_name = NULL;
    char *cl_smap_node = NULL;
    bn_binding *binding = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    smap_name = tstr_array_get_str_quick(params, 1);
    bail_null(smap_name);

    bn_name = smprintf("/nkn/nvsd/server-map/config/%s", smap_name);
    bail_null(bn_name);

    // Check if server-map exists.
    err = mdc_get_binding(cli_mcc, NULL, NULL,
            false, &binding, bn_name, NULL);
    bail_error(err);

    if (NULL == binding) {
        cli_printf_error(_("Invalid server-map: %s\n"), smap_name);
        goto bail;
    }

    cluster_name = tstr_array_get_str_quick(params, 0);
    bail_null(cluster_name);

	cl_smap_node = smprintf("/nkn/nvsd/cluster/config/%s/server-map", cluster_name);
    bail_null(cl_smap_node);

    err = mdc_set_binding(cli_mcc,
        			NULL,
        			NULL,
        			bsso_modify,
        			bt_string,
        			tstr_array_get_str_quick(params, 1),
        			cl_smap_node);
     bail_error(err);

     bail:
     return err;
}

int
cli_nvsd_ns_cluster_add(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;
    bn_binding_array *barr = NULL;
    uint32 code = 0;
    tbool dup_deleted = false;
    tstring *msg = NULL;
    const char *cluster_name = NULL;
    char *node_name = NULL;
    char *bn_name = NULL;
    char *cl_ns_node = NULL;
    const char *ns_name = NULL;
    char *cluster_binding = NULL;
    uint32 num_clusters =0;
    tstring * ns_cluster =NULL;
    bn_binding *binding = NULL;
    tstring *cl_smap = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    cluster_name = tstr_array_get_str_quick(params, 1);
    bail_null(cluster_name);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    err = bn_binding_array_new(&barr);
    bail_error(err);

    bn_name = smprintf("/nkn/nvsd/cluster/config/%s", cluster_name);
    bail_null(bn_name);

    // Check if cluster exists.
    err = mdc_get_binding(cli_mcc, NULL, NULL,
            false, &binding, bn_name, NULL);
    bail_error(err);

    if (NULL == binding) {
        cli_printf_error(_("Invalid cluster : %s\n"), cluster_name);
        goto bail;
    }

	err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &cl_smap, NULL, "/nkn/nvsd/cluster/config/%s/server-map", cluster_name);
	bail_error(err);

	if (ts_equal_str(cl_smap, "", true)) {
		cli_printf(_("Associate a cluster-server-map to the cluster: %s \n before Associating the cluster to the namespace\n"), cluster_name);
		goto bail;
	}

	err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &ns_cluster, NULL, "/nkn/nvsd/cluster/config/%s/namespace", cluster_name);
	bail_error(err);

	if (!ts_equal_str(ns_cluster, "", true)) {
		cli_printf(_("Cluster %s is already associated with namespace : %s\n Cluster cannot be associated with more than one namespace at a time \n"), cluster_name, ts_str(ns_cluster));
		goto bail;
	}

    cluster_binding = smprintf("/nkn/nvsd/namespace/%s/cluster/*/cl-name", ns_name);
    bail_null(cluster_binding);

    err = mdc_foreach_binding(cli_mcc, cluster_binding, NULL, cli_nvsd_cluster_dupchk,
	 	   (void*)cluster_name, &num_clusters);
    bail_error(err);

    node_name = smprintf ("/nkn/nvsd/namespace/%s/cluster", ns_name);

    err = mdc_array_append_single(cli_mcc,
			node_name,
			"cl-name",
			bt_string,
			cluster_name,
			true,
			&dup_deleted,
			&code,
			&msg);
    bail_error(err);

    if (code != 0) {
           goto bail;
    }

    cl_ns_node = smprintf("/nkn/nvsd/cluster/config/%s/namespace", cluster_name);
    bail_null(cl_ns_node);

    err = mdc_set_binding(cli_mcc,
            NULL,
            NULL,
            bsso_modify,
            bt_string,
            ns_name,
            cl_ns_node);
    bail_error(err);

 bail:

    bn_binding_array_free(&barr);
    ts_free(&msg);
    ts_free(&cl_smap);
    safe_free(node_name);
    safe_free(bn_name);
    safe_free(cl_ns_node);
    safe_free(cluster_binding);
    ts_free(&ns_cluster);
  return(err);
}


int
cli_nvsd_ns_cluster_delete(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;
    uint32 error = 0;
    unsigned int error_code = 0;
    bn_binding_array *barr = NULL;
    const char *cluster_name = NULL;
    const char *cl_reset_node =  "/nkn/nvsd/cluster/config/%s/reset";
    char *node_name = NULL;
    const char *ns_name = NULL;
    bn_request * req = NULL;
    char cl_reset[256] = {0};
    error_msg_t err_msg = {0};

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    cluster_name = tstr_array_get_str_quick(params, 1);
    bail_null(cluster_name);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    err = cli_cluster_check_namepsace(ns_name, cluster_name, &error, &err_msg);
    bail_error(err);
    
    if (error) {
	/* not setting the err here, else internal error will be printed */
	cli_printf_error("%s",err_msg);
	goto bail;
    }

    err = bn_binding_array_new(&barr);
    bail_error(err);

    node_name = smprintf ("/nkn/nvsd/namespace/%s/cluster", ns_name);

    err = mdc_array_delete_by_value_single(cli_mcc,
                node_name,
                true, "cl-name", bt_string,
                cluster_name, NULL, &error_code, NULL);
    bail_error(err);
    /* don't do anything else, if this failed */
    bail_error(error_code);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    /* reset cluster */
    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_bool, 0, "true", NULL,
	    cl_reset_node, cluster_name);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
	    bt_string, 0, "", NULL,
	    "/nkn/nvsd/cluster/config/%s/namespace", cluster_name);
    bail_error(err);

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);

    /* set the reset flag again to false, so that we can set it again next time */
    snprintf(cl_reset, sizeof(cl_reset), cl_reset_node, cluster_name);
    err = mdc_modify_binding(cli_mcc,
            NULL, NULL,
            bt_bool, "false", cl_reset);
    bail_error(err);

bail:
    bn_request_msg_free(&req);
    bn_binding_array_free(&barr);
    safe_free(node_name);

    return(err);
}

int
cli_nvsd_cluster_show_cmd(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params)
{
    int err = 0;
    const char *cluster_name = NULL;
    node_name_t bn_name = { 0 };
    bn_binding *binding = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    cluster_name = tstr_array_get_str_quick(params, 0);
    bail_null(cluster_name);

    snprintf(bn_name, sizeof(bn_name), "/nkn/nvsd/cluster/config/%s", cluster_name);

    // Check if cluster exists.
    err = mdc_get_binding(cli_mcc, NULL, NULL,
            false, &binding, bn_name, NULL);
    bail_error(err);

    if (NULL == binding) {
        cli_printf_error(_("Invalid cluster : %s\n"), cluster_name);
        goto bail;
    }

    err = cli_nvsd_show_cluster(cluster_name, 0);
    bail_error(err);

bail:
    bn_binding_free(&binding);
    return err;
}

int
cli_cluster_is_valid_name(const char *name, tstring **ret_msg)
{
    int err = 0;
    int i = 0;
    const char *p = "/\\*:|`\"?.- ";
    int j = 0;
    int k = strlen(name);
    int l = strlen(p);

    if (name[0] == '_') {
        if ( ret_msg != NULL )
            err = ts_new_str(ret_msg, "Bad cluster name. Name cannot have '_' as prefix");
        err = lc_err_not_found;
        goto bail;
    }

    for(i = 0; i < l; i++) {
        for(j = 0; j < k; j++) {
            if ( p[i] == name[j] ) {
                if ( ret_msg != NULL )
                    err = ts_new_sprintf(ret_msg, "Bad name for cluster. Name cannot contain the characters '%s'", p);
                err = lc_err_not_found;
                goto bail;
            }
        }
    }

bail:
    return err;
}

int
cli_cluster_name_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 ret_err = 0;
    tbool valid = false;
    char *binding_name = NULL;
    tstring *ret_msg = NULL;

    UNUSED_ARGUMENT(data);

    err = cli_cluster_is_valid_name(tstr_array_get_str_quick(params, 0), &ret_msg);
    if ( err != 0 ) {
        cli_printf_error(_("%s"), ts_str(ret_msg));
        err = 0;
        goto bail;
    }
    err = cli_cluster_name_validate(tstr_array_get_str_quick(params, 0),
            &valid);
    //bail_error(err);

    if ( !cli_have_capabilities(ccp_set_rstr_curr) && !valid ) {
        cli_printf_error(_("Unknown cluster '%s'"),
                tstr_array_get_str_quick(params, 0));
        err = 0;
        goto bail;
    }
    else if ( !cli_have_capabilities(ccp_set_rstr_curr) && valid ) {
        err = cli_print_incomplete_command_error(cmd, cmd_line);
        bail_error(err);

        goto bail;
    }

    if(!valid) {
        // Node doesnt exist.. create it
            binding_name = smprintf("/nkn/nvsd/cluster/config/%s",
                    tstr_array_get_str_quick(params, 0));
            bail_null(binding_name);

            err = mdc_create_binding(cli_mcc,
                    &ret_err,
                    &ret_msg,
                    bt_string,
                    tstr_array_get_str_quick(params, 0),
                    binding_name);
            bail_error(err);
            bail_error_msg(ret_err, "%s", ts_str(ret_msg));


        valid = true;
    }

    if (cli_prefix_modes_is_enabled()) {
        err = cli_cluster_name_validate(tstr_array_get_str_quick(params, 0),
                &valid);
        bail_error(err);

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
    safe_free(binding_name);
    ts_free(&ret_msg);
    return err;
}

int
cli_cluster_chash_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 ret_code = 0, error = 0;
    const char *cluster_name = NULL;
    char *cl_type = data;
    const char *cluster_type = "", *ns_name = NULL;
    bn_request *req = NULL;
    error_msg_t err_msg = {0};

    if (cl_type == NULL) {
	/* this must not have happened */
	err = 1;
	goto bail;
    }
    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    cluster_name = tstr_array_get_str_quick(params, 1);
    bail_null(cluster_name);

    err = cli_cluster_check_namepsace(ns_name, cluster_name, &error, &err_msg);
    bail_error(err);
    
    if (error) {
	/* not setting the err here, else internal error will be printed */
	cli_printf_error("%s",err_msg);
	goto bail;
    }
    if (0 == strcmp(cl_type, "redirect")) {
	/* request-routing method == redirect (1) */
	cluster_type = "1";
    } else if (0 == strcmp(cl_type, "proxy")) {
	/* request-routing method == proxy (2) */
	cluster_type = "2";
    } else {
	/* unknown Cluster type, error */
	err = 1;
	goto bail;
    }

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    cluster_name = tstr_array_get_str_quick(params, 1);
    bail_null(cluster_name);

    err = cli_cluster_common_req(req, cluster_name, cluster_type, "1");
    bail_error(err);

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &ret_code, NULL);
    bail_error(err);

    /* change prompt only if changes are successful */
    if (ret_code == 0) {
	if (cli_prefix_modes_is_enabled()) {
	    err = cli_prefix_enter_mode(cmd, cmd_line);
	    bail_error(err);
	}
    }

bail:
    bn_request_msg_free(&req);
    return err;
}

int
cli_cluster_loadbalance_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0; 
    uint32 ret_code = 0, error = 0;
    const char *cluster_name = NULL, *ns_name = NULL;
    char *cl_rr_node = NULL;
    error_msg_t err_msg = {0};

    UNUSED_ARGUMENT(data);

    cluster_name = tstr_array_get_str_quick(params, 1);
    bail_null(cluster_name);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    err = cli_cluster_check_namepsace(ns_name, cluster_name, &error, &err_msg);
    bail_error(err);
    
    if (error) {
	/* not setting the err here, else internal error will be printed */
	cli_printf_error("%s",err_msg);
	goto bail;
    }

    cluster_name = tstr_array_get_str_quick(params, 1);
    bail_null(cluster_name);

    cl_rr_node = smprintf("/nkn/nvsd/cluster/config/%s/request-routing-method", cluster_name);
    bail_null(cl_rr_node);

    err = mdc_set_binding(cli_mcc,
            &ret_code,
            NULL,
            bsso_modify,
            bt_uint32,
            "2",
            cl_rr_node);
    bail_error(err);

    if (cli_prefix_modes_is_enabled() && (ret_code == 0)) {
    	err = cli_prefix_enter_mode(cmd, cmd_line);
        bail_error(err);
    }

bail:
	safe_free(cl_rr_node);
    return err;
}

int
cli_cluster_addrport_callback(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 error = 0;
    const char *cluster_name = NULL, *cl_type = data, *ns_name = NULL;
    const char *cl_addr_node = NULL, *cl_ip_port = NULL;
    bn_request *req = NULL;
    const char *cluster_type = NULL;
    error_msg_t err_msg = {0};

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    cluster_name = tstr_array_get_str_quick(params, 1);
    bail_null(cluster_name);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    err = cli_cluster_check_namepsace(ns_name, cluster_name, &error, &err_msg);
    bail_error(err);
    
    if (error) {
	/* not setting the err here, else internal error will be printed */
	cli_printf_error("%s",err_msg);
	goto bail;
    }
    cl_ip_port = tstr_array_get_str_quick(params, 2);
    bail_null(cl_ip_port);

    if (0 == strcmp(cl_type, "redirect")) {
	cluster_type = "1";
	cl_addr_node = "/nkn/nvsd/cluster/config/%s/redirect-addr-port";
    } else if (0 == strcmp(cl_type, "proxy") ){
	cluster_type = "2";
	cl_addr_node = "/nkn/nvsd/cluster/config/%s/proxy-addr-port";
    } else {
	err = EINVAL;
	goto bail;
    }

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    /* set cluster-hash=> base-url/compelete-url */
    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_string, 0, cl_ip_port, NULL, 
	    cl_addr_node,
	    cluster_name);
    bail_error(err);

    err = cli_cluster_common_req(req, cluster_name, cluster_type, "1");
    bail_error(err);

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);
bail:
    bn_request_msg_free(&req);
    return err;
}

int
cli_cluster_path_callback(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 error = 0;
    const char *cluster_name = NULL, *cl_type = data, *ns_name = NULL;
    const char *cl_path_node = NULL, *cl_base_path = NULL;
    bn_request *req = NULL;
    const char *cluster_type = NULL;
    error_msg_t err_msg = { 0 };

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    cluster_name = tstr_array_get_str_quick(params, 1);
    bail_null(cluster_name);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    err = cli_cluster_check_namepsace(ns_name, cluster_name, &error, &err_msg);
    bail_error(err);
    
    if (error) {
	/* not setting the err here, else internal error will be printed */
	cli_printf_error("%s",err_msg);
	goto bail;
    }
    cl_base_path  = tstr_array_get_str_quick(params, 2);
    bail_null(cl_base_path );

    if (0 == strcmp(cl_type, "redirect")) {
	cluster_type = "1";
	cl_path_node = "/nkn/nvsd/cluster/config/%s/redirect-base-path";
    } else if (0 == strcmp(cl_type, "proxy") ){
	cluster_type = "2";
	cl_path_node = "/nkn/nvsd/cluster/config/%s/proxy-base-path";
    } else {
	err = EINVAL;
	goto bail;
    }

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    /* set cluster-base-path value =>  */
    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_string, 0, cl_base_path , NULL, 
	    cl_path_node,
	    cluster_name);
    bail_error(err);

    err = cli_cluster_common_req(req, cluster_name, cluster_type, "1");
    bail_error(err);

    /* sending code pointter as NULL, as it will print message internally */
    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);

bail:
    bn_request_msg_free(&req);
    return err;
}

/* NOTE- not doing any checks for NULL pointers, it is responsibility of caller */
int 
cli_cluster_common_req(bn_request *req, const char *cl_name,
	const char *cl_type, const char *rr_method)
{
    int err = 0;
    tstring *rr_method_old = NULL;
    /* set cluster-type */
    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_uint32, 0, cl_type, NULL, 
	    "/nkn/nvsd/cluster/config/%s/cluster-type",
	    cl_name);
    bail_error(err);

    /* set cluster/<cl-name>/configured to true */
    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_bool, 0, "true", NULL, 
	    "/nkn/nvsd/cluster/config/%s/configured",
	    cl_name);
    bail_error(err);


    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &rr_method_old,
	    NULL, "/nkn/nvsd/cluster/config/%s/request-routing-method", cl_name);
    bail_error_null(err, rr_method_old);

    if (ts_equal_str(rr_method_old, "0", true)) {
	/* set the request routing method */
	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
		bt_uint32, 0, rr_method, NULL, 
		"/nkn/nvsd/cluster/config/%s/request-routing-method",
		cl_name);

	bail_error(err);
    }

bail:
    return err;
}
int
cli_cluster_hash_callback(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 error = 0;
    const char *cluster_name = NULL, *cl_type = data, *ns_name = NULL;
    const char * cl_hash_value = NULL, *cl_hash_node = NULL, *cluster_type = NULL;
    bn_request *req = NULL;
    error_msg_t err_msg = {0};

    UNUSED_ARGUMENT(cmd_line);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    cluster_name = tstr_array_get_str_quick(params, 1);
    bail_null(cluster_name);

    err = cli_cluster_check_namepsace(ns_name, cluster_name, &error, &err_msg);
    bail_error(err);
    
    if (error) {
	/* not setting the err here, else internal error will be printed */
	cli_printf_error("%s",err_msg);
	goto bail;
    }

    if (0 == strcmp(cl_type, "redirect")) {
	cluster_type = "1";
	cl_hash_node = "/nkn/nvsd/cluster/config/%s/cluster-hash";
    } else if (0 == strcmp(cl_type, "proxy") ){
	cluster_type = "2";
	cl_hash_node = "/nkn/nvsd/cluster/config/%s/proxy-cluster-hash";
    } else {
	err = EINVAL;
	goto bail;
    }
    if (cmd->cc_magic == 1) {
	/* basr-url */
	cl_hash_value = "1";
    } else if (cmd->cc_magic ==2) {
	/* complete url */
	cl_hash_value = "2";
    } else {
	/* err */
	err = EINVAL;
	goto bail;
    }
    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    /* set cluster-hash=> base-url/compelete-url */
    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_uint32, 0, cl_hash_value, NULL, 
	    cl_hash_node,
	    cluster_name);
    bail_error(err);

    /* rr-method == 1, means load-balance is not enabled */
    err = cli_cluster_common_req(req, cluster_name, cluster_type,"1");
    bail_error(err);

    /* sending code pointter as NULL, as it will print message internally */
    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);

bail:
    bn_request_msg_free(&req);
    return err;
}


int
cli_cluster_local_callback(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 error = 0;
    const char *cluster_name = NULL, *cl_type = data;
    const char *cluster_type = NULL, *ns_name = NULL;
    const char *cl_local_node = NULL, *cl_local_value = NULL;
    bn_request *req = NULL;
    error_msg_t err_msg = {0};

    UNUSED_ARGUMENT(cmd_line);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    cluster_name = tstr_array_get_str_quick(params, 1);
    bail_null(cluster_name);

    err = cli_cluster_check_namepsace(ns_name, cluster_name, &error, &err_msg);
    bail_error(err);
    
    if (error) {
	/* not setting the err here, else internal error will be printed */
	cli_printf_error("%s",err_msg);
	goto bail;
    }

    if (0 == strcmp(cl_type, "redirect")) {
	cluster_type = "1";
	cl_local_node = "/nkn/nvsd/cluster/config/%s/redirect-local";
    } else if (0 == strcmp(cl_type, "proxy") ){
	cluster_type = "2";
	cl_local_node = "/nkn/nvsd/cluster/config/%s/proxy-local";
    } else {
	err = EINVAL;
	goto bail;
    }

    if (cmd->cc_magic == 1) {
	/* basr-url */
	cl_local_value = "true";
    } else if (cmd->cc_magic ==2) {
	/* complete url */
	cl_local_value = "false";
    } else {
	/* err */
	err = EINVAL;
	goto bail;
    }

    err = bn_set_request_msg_create(&req, 0);

    /* set cluster-local => allow / deny */
    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_bool , 0, cl_local_value, NULL, 
	    cl_local_node,
	    cluster_name);
    bail_error(err);

    err = cli_cluster_common_req(req, cluster_name, cluster_type,"1");
    bail_error(err);

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);

bail:
    bn_request_msg_free(&req);
    return err;
}

static int
cli_cluster_name_validate(
        const char *cluster,
        tbool *ret_valid)
{
    int err = 0;
    tstr_array *cl = NULL;
    uint32 idx = 0;

    bail_null(ret_valid);
    *ret_valid = false;

    err = cli_cluster_get_names(&cl, false);
    bail_error(err);


    err = tstr_array_linear_search_str(cl, cluster, 0, &idx);
    if (lc_err_not_found != err) {
        bail_error(err);
        *ret_valid = true;
    }
    else {
        err = 0;
    }
bail:
    tstr_array_free(&cl);
    return err;
}

static int
cli_cluster_get_names(
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
            "/nkn/nvsd/cluster/config", NULL);
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
cli_cluster_name_help(
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
    const char *cluster = NULL;
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

        err = cli_cluster_name_completion(data, cmd, cmd_line, params,
                curr_word, names);
        bail_error(err);

        num_names = tstr_array_length_quick(names);
        for(i = 0; i < num_names; ++i) {
            cluster = tstr_array_get_str_quick(names, i);
            bail_null(cluster);

            err = cli_add_expansion_help(context, cluster, NULL);
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
cli_smap_cluster_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    char *smap_name = NULL;
    char *binding_name = NULL;
    tstr_array *names = NULL;
    tstring *fmt_type = NULL;
    uint32 num_names = 0;
    uint32 idx = 0;
    tstring *smap_cluster =NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);
    UNUSED_ARGUMENT(curr_word);

    err = cli_server_map_get_names(&names, true);
    bail_error_null(err, names);

    num_names = tstr_array_length_quick(names);

    if (num_names > 0)
    {
        for(idx = 0; idx < num_names; ++idx)
        {

        	smap_name = (char *)tstr_array_get_str_quick(names, idx);

    	    binding_name = smprintf("/nkn/nvsd/server-map/config/%s/format-type", smap_name);
    	    bail_null(binding_name);

    	    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &fmt_type, NULL, binding_name, NULL);
    	    bail_error_null(err, fmt_type);

            if (ts_equal_str(fmt_type, "2", true)) {

            err = ts_new_sprintf(&smap_cluster, "%s", smap_name);
            bail_error_null(err, smap_cluster);

            err = tstr_array_append(ret_completions, smap_cluster);
	        bail_error(err);
            }

        }
    }

bail:
   safe_free(binding_name);
   tstr_array_free (&names);
   safe_free(binding_name);
   ts_free(&smap_cluster);
   ts_free(&fmt_type);
   return err;
}


int
cli_cluster_name_completion(
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
    UNUSED_ARGUMENT(curr_word);

    err = cli_cluster_get_names(&names, true);
    bail_error(err);

    err = tstr_array_concat(ret_completions, &names);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);

bail:
    tstr_array_free(&names);
    return err;
}


int
cli_server_map_get_names(
        tstr_array **ret_names,
        tbool hide_display)
{
    int err = 0;
    tstr_array *names = NULL;
    tstr_array_options opts;
    tstr_array *names_config = NULL;

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
            "/nkn/nvsd/server-map/config", NULL);
    bail_error_null(err, names_config);

    err = tstr_array_concat(names, &names_config);
    bail_error(err);

    *ret_names = names;
    names = NULL;
bail:
    tstr_array_free(&names);
    tstr_array_free(&names_config);
    return err;
}

static int
cli_cluster_suffix_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply)
{
    int err = 0;
    const char *clusterName = NULL, *suffix_name = NULL, *action  = NULL;
    tstring *t_cl_ns = NULL, *rev_cmd = NULL;
    node_name_t ns_cl_node = {0};

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(binding);

    clusterName = tstr_array_get_str_quick(name_parts, 4);
    bail_null(clusterName);

    suffix_name = tstr_array_get_str_quick(name_parts, 6);
    bail_null(suffix_name);

    snprintf(ns_cl_node, sizeof(ns_cl_node), "/nkn/nvsd/cluster/config/%s/namespace", clusterName);
    err = bn_binding_array_get_value_tstr_by_name(bindings, ns_cl_node, NULL, &t_cl_ns);
    bail_error_null(err, t_cl_ns);

    if (0 == strcmp(value, "1")) {
	/* proxy */
	action = "proxy";
    } else if (0 == strcmp(value, "2")) {
	/* redirect */
	action = "redirect";
    } else {
	err = EINVAL;
	bail_error(err);
    }

    if (t_cl_ns != NULL && (0 != strcmp(ts_str(t_cl_ns), ""))) {
	err = ts_new_sprintf(&rev_cmd, "namespace %s cluster %s suffix %s action %s",
		ts_str(t_cl_ns), clusterName, suffix_name, action);
	bail_error(err);
    }

    if (rev_cmd != NULL) {
	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);
    }

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
    bail_error(err);


bail:
    ts_free(&rev_cmd);
    ts_free(&t_cl_ns);
    return err;
}

static int
cli_nvsd_redirect_local_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL;
    tbool t_redirect_local = false, configured = false;
    const char *str_redirect_local = NULL;
    const char *clusterName = NULL;
    const char *str_method_type = NULL, *str_local = NULL;
    char node[256] = { 0 }, cl_type_node[256] = {0}, cl_proxy_node[256] = {0};
    char ns_cl_node[256] = { 0};
    tstring *t_cl_ns = NULL, *t_cluster_type = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    clusterName = tstr_array_get_str_quick(name_parts, 4);
    bail_null(clusterName);

    snprintf(cl_proxy_node, sizeof(cl_proxy_node),
	    "/nkn/nvsd/cluster/config/%s/proxy-local", clusterName);

    err = cli_cluster_is_configured(bindings, clusterName, &configured);
    bail_error(err);

    /* NOTE: we need to consume cl_proxy_node */
    if (configured == 0) {
	goto consume_binding;
    }

    /* get the type of cluster */
    snprintf(cl_type_node, sizeof(cl_type_node), "/nkn/nvsd/cluster/config/%s/cluster-type", clusterName);
    err = bn_binding_array_get_value_tstr_by_name(bindings, cl_type_node, NULL, &t_cluster_type);
    bail_error_null(err, t_cluster_type);

    if (0 == strcmp(ts_str(t_cluster_type), "1")) {
	/* cluster type is "redirect cluster" */
	str_method_type = "consistent-hash-redirect";
	snprintf(node, sizeof(node), "/nkn/nvsd/cluster/config/%s/redirect-local", clusterName);
	str_local = "redirect-local";
    } else if (0 == strcmp(ts_str(t_cluster_type), "2")) {
	/* this is proxy cluster */
	str_method_type = "consistent-hash-proxy";
	snprintf(node, sizeof(node), "/nkn/nvsd/cluster/config/%s/proxy-local", clusterName);
	str_local = "proxy-local";
    } else {
	/* we don't know the type of cluster */
	str_method_type = NULL;
	str_local = NULL;
    }
    err = bn_binding_array_get_value_tbool_by_name(bindings, node, NULL, &t_redirect_local);
    bail_error(err);

    if (t_redirect_local) {
    	str_redirect_local = "allow";
    } else {
    	str_redirect_local = "deny";
    }

    snprintf(ns_cl_node, sizeof(ns_cl_node), "/nkn/nvsd/cluster/config/%s/namespace", clusterName);

    err = bn_binding_array_get_value_tstr_by_name(bindings, ns_cl_node, NULL, &t_cl_ns);
    bail_error(err);

    if (str_method_type != NULL && t_cl_ns != NULL && str_local != NULL) {
	err = ts_new_sprintf(&rev_cmd, "namespace %s cluster %s request-routing method %s %s %s",
    		ts_str(t_cl_ns), clusterName,str_method_type,str_local, str_redirect_local);
	bail_error(err);
    }

    if (rev_cmd != NULL) {
	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);
    }

consume_binding:
    /* Consume nodes - */
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
    bail_error(err);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", cl_proxy_node);
    bail_error(err);


bail:
    ts_free(&rev_cmd);
    return err;
}


static int
cli_nvsd_replicate_mode_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply)
{
    int err = 0;
    tbool configured = false;
    tstring *rev_cmd = NULL;
    tstring *t_replicate_mode = NULL;
    const char *str_replicate_mode = NULL;
    const char *clusterName = NULL;
    char node[256] = { 0 }, cl_rr_node[256] = {0};
    char ns_cl_node[256] = { 0};
    tstring *t_cl_ns = NULL, *t_rr_method = NULL;
    int cl_lb = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    clusterName = tstr_array_get_str_quick(name_parts, 4);
    bail_null(clusterName);

    snprintf(node, sizeof(node), "/nkn/nvsd/cluster/config/%s/replicate-mode", clusterName);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		    NULL, &t_replicate_mode, "%s", node);
    bail_error_null(err ,t_replicate_mode);

    if(!strcmp(ts_str(t_replicate_mode),"1")) {
	    str_replicate_mode = "least-loaded";
    }else if(!strcmp(ts_str(t_replicate_mode),"2")) {
	    str_replicate_mode = "random";
    }
    /* get the req-uest-routing-method of cluster */
    snprintf(cl_rr_node, sizeof(cl_rr_node), "/nkn/nvsd/cluster/config/%s/request-routing-method", clusterName);
    err = bn_binding_array_get_value_tstr_by_name(bindings, cl_rr_node, NULL, &t_rr_method );
    bail_error_null(err, t_rr_method);

    err = cli_cluster_is_configured(bindings, clusterName, &configured);
    bail_error(err);

    /* NOTE: we need to consume cl_proxy_node */
    if (configured == 0) {
	goto consume_binding;
    }
    /* rr-method is set "2", is load-balance is enabled */
    if (0 == strcmp(ts_str(t_rr_method), "2")) {
	/* rr-method has been set  to replicate, so command is valid */
	cl_lb = 1;
    } else {
	/*  rr-method has not been set, so we cannot have this command */
	cl_lb = 0;
    }

    snprintf(ns_cl_node, sizeof(ns_cl_node), "/nkn/nvsd/cluster/config/%s/namespace", clusterName);

    err = bn_binding_array_get_value_tstr_by_name(bindings, ns_cl_node, NULL, &t_cl_ns);
    bail_error(err);

    if (t_cl_ns != NULL && cl_lb != 0){
	err = ts_new_sprintf(&rev_cmd, "namespace %s cluster %s request-routing load-balance replicate-mode %s",
		ts_str(t_cl_ns), clusterName, str_replicate_mode);
	bail_error(err);
    }

    if (rev_cmd != NULL) {
	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);
    }

consume_binding:
    /* Consume nodes */
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
    bail_error(err);

bail:
    ts_free(&rev_cmd);
    ts_free(&t_replicate_mode);
    ts_free(&t_rr_method);

    return err;
}

static int
cli_nvsd_cluster_smap_revmap(void *data, const cli_command *cmd,
        const bn_binding_array *bindings,
        const char *name,
        const tstr_array *name_parts,
        const char *value, const bn_binding *binding,
        cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL;
    const char *cluster = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(binding);

    cluster = tstr_array_get_str_quick(name_parts, 4);
    bail_null(cluster);

    err = ts_new_sprintf(&rev_cmd, "cluster create \"%s\" server-map  \"%s\"",
	    cluster, value);
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes,
	    "/nkn/nvsd/cluster/config/%s/server-map", cluster);
    bail_error(err);

bail:
    ts_free(&rev_cmd);
    return err;
}

static int
cli_nvsd_ns_cluster_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	tstring *rev_cmd = NULL;
	const char *namespace = NULL;
	const char *position = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(bindings);
	UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(binding);

	namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_null(namespace);

	position = tstr_array_get_str_quick(name_parts, 5);
	bail_null(position);

	err = ts_new_sprintf(&rev_cmd, "namespace %s add cluster \"%s\"",
			namespace, value);
	bail_error(err);

	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);

	err = tstr_array_append_sprintf(ret_reply->crr_nodes,
			"/nkn/nvsd/namespace/%s/cluster/%d/cl-name",
			namespace, atoi(position));
	bail_error(err);

bail:
	ts_free(&rev_cmd);
	return err;
}

int
cli_nvsd_cluster_hash_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL;
    tstring *t_cluster_hash = NULL;
    const char *str_cluster_hash = NULL, *str_method_type= NULL;
    const char *clusterName = NULL;
    char node[256] = {0}, ns_cl_node[256] = {0}, cl_type_node[256] = {0} ;
    tstring *t_cl_ns = NULL, *t_cluster_type = NULL;
    tbool configured = false;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(value);

    clusterName = tstr_array_get_str_quick(name_parts, 4);
    bail_null(clusterName);

    snprintf(node, sizeof(node), "/nkn/nvsd/cluster/config/%s/cluster-hash", clusterName);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		    NULL, &t_cluster_hash, "%s", node);
    bail_error_null(err ,t_cluster_hash);

    if(!strcmp(ts_str(t_cluster_hash),"1")) {
    	str_cluster_hash = "base-url";
    }else if(!strcmp(ts_str(t_cluster_hash),"2")) {
    	str_cluster_hash = "complete-url";
    } else {
	/* XXX What to do */
    }

    snprintf(ns_cl_node, sizeof(ns_cl_node), "/nkn/nvsd/cluster/config/%s/namespace", clusterName);

    err = bn_binding_array_get_value_tstr_by_name(bindings, ns_cl_node, NULL, &t_cl_ns);
    bail_error(err);

    /* get the type of cluster */
    snprintf(cl_type_node, sizeof(cl_type_node), "/nkn/nvsd/cluster/config/%s/cluster-type", clusterName);
    err = bn_binding_array_get_value_tstr_by_name(bindings, cl_type_node, NULL, &t_cluster_type);
    bail_error_null(err, t_cluster_type);

    err = cli_cluster_is_configured(bindings, clusterName, &configured);
    bail_error(err);

    /* NOTE: we need to consume nodes, so not going to bail */
    if (configured == 0) {
	goto consume_binding;
    }

    if (0 == strcmp(ts_str(t_cluster_type), "1")) {
	/* cluster type is "redirect cluster" */
	    str_method_type = "consistent-hash-redirect";
    } else if (0 == strcmp(ts_str(t_cluster_type), "2")) {
	/* this is proxy cluster */
	    str_method_type = "consistent-hash-proxy";
    } else {
	/* we don't know the type of cluster */
	str_method_type = NULL;
    }
    if (str_method_type != NULL && t_cl_ns != NULL) {
	err = ts_new_sprintf(&rev_cmd, "namespace %s cluster %s request-routing method %s cluster-hash %s", 
		ts_str(t_cl_ns), clusterName, str_method_type, str_cluster_hash);
	bail_error(err);
    }

    if (rev_cmd != NULL) {
	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);
    }

consume_binding:
    /* Consume nodes - */
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
    bail_error(err);

bail:
    ts_free(&rev_cmd);
    ts_free(&t_cluster_hash);
    return err;
}

int
cli_nvsd_cluster_load_thres_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply)
{
    int err = 0, cl_lb = 0;
    tstring *rev_cmd = NULL;
    tstring *t_load_thresh = NULL;
    const char *clusterName = NULL;
    char *node =NULL;
    char *ns_cl_node = NULL;
    tstring *t_cl_ns = NULL, *t_rr_method = NULL;
    node_name_t cl_rr_node = {0};
    tbool configured = false;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    clusterName = tstr_array_get_str_quick(name_parts, 4);
    bail_null(clusterName);

    node =  smprintf("/nkn/nvsd/cluster/config/%s/load-threshold", clusterName);
    bail_null(node);

    err = bn_binding_array_get_value_tstr_by_name(bindings, node, NULL, &t_load_thresh);
    bail_error(err);

    ns_cl_node =  smprintf("/nkn/nvsd/cluster/config/%s/namespace", clusterName);
    bail_null(ns_cl_node);

    err = bn_binding_array_get_value_tstr_by_name(bindings, ns_cl_node, NULL, &t_cl_ns);
    bail_error(err);

    /* get the request-routing-method of cluster */
    snprintf(cl_rr_node, sizeof(cl_rr_node), "/nkn/nvsd/cluster/config/%s/request-routing-method", clusterName);
    err = bn_binding_array_get_value_tstr_by_name(bindings, cl_rr_node, NULL, &t_rr_method );
    bail_error_null(err, t_rr_method);

    /* rr-method is set "2", is load-balance is enabled */
    if (0 == strcmp(ts_str(t_rr_method), "2")) {
	/* rr-method has been set  to replicate, so command is valid */
	cl_lb = 1;
    } else {
	/*  rr-method has not been set, so we cannot have this command */
	cl_lb = 0;
    }

    err = cli_cluster_is_configured(bindings, clusterName, &configured);
    bail_error(err);

    /* NOTE: we need to consume nodes, so not going to bail */
    if (configured == 0) {
	goto consume_binding;
    }

    if (t_cl_ns != NULL && cl_lb != 0) {
	err = ts_new_sprintf(&rev_cmd, "namespace %s cluster %s request-routing load-balance load-threshold %s",
		ts_str(t_cl_ns), clusterName, ts_str(t_load_thresh));
	bail_error(err);
    }

    if (rev_cmd != NULL) {
	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);
    }

consume_binding:
    /* Consume nodes - */
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
    bail_error(err);

bail:
    ts_free(&rev_cmd);
    ts_free(&t_rr_method);
    safe_free(node);
    ts_free(&t_load_thresh);
    return err;
}

int
cli_nvsd_cluster_redir_addr_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply)
{
    tbool configured = false;
    int err = 0;
    tstring *rev_cmd = NULL;
    tstring *t_redirect_addr = NULL;
    const char *clusterName = NULL, *str_method_type = NULL,
	  *str_addr_port = NULL;
    node_name_t ns_cl_node =  {0}, cl_type_node = {0},
	      node = {0}, proxy_node = {0} ;
    tstring *t_cl_ns = NULL, *t_cluster_type = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    clusterName = tstr_array_get_str_quick(name_parts, 4);
    bail_null(clusterName);

    snprintf(ns_cl_node, sizeof(ns_cl_node), "/nkn/nvsd/cluster/config/%s/namespace", clusterName);
    err = bn_binding_array_get_value_tstr_by_name(bindings, ns_cl_node, NULL, &t_cl_ns);
    bail_error(err);

    /* get the type of cluster */
    snprintf(cl_type_node, sizeof(cl_type_node), "/nkn/nvsd/cluster/config/%s/cluster-type", clusterName);
    err = bn_binding_array_get_value_tstr_by_name(bindings, cl_type_node, NULL, &t_cluster_type);
    bail_error_null(err, t_cluster_type);

    if (0 == strcmp(ts_str(t_cluster_type), "1")) {
	/* cluster type is "redirect cluster" */
	str_method_type = "consistent-hash-redirect";
	str_addr_port = "redirect-addr-port";
	snprintf(node, sizeof(node), "/nkn/nvsd/cluster/config/%s/redirect-addr-port", clusterName);
    } else if (0 == strcmp(ts_str(t_cluster_type), "2")) {
	/* this is proxy cluster */
	str_method_type = "consistent-hash-proxy";
	str_addr_port = "proxy-addr-port";
	snprintf(node, sizeof(node), "/nkn/nvsd/cluster/config/%s/proxy-addr-port", clusterName);
    } else {
	/* we don't know the type of cluster */
	str_method_type = NULL;
	str_addr_port = NULL;
    }

    if (str_method_type != NULL) {
	err = bn_binding_array_get_value_tstr_by_name(bindings, node, NULL, &t_redirect_addr);
	bail_error(err);
    }

    err = cli_cluster_is_configured(bindings, clusterName, &configured);
    bail_error(err);

    /* NOTE: we need to consume nodes, so not going to bail */
    if (configured == 0) {
	goto consume_binding;
    }

    if (t_cl_ns != NULL && str_method_type != NULL && str_addr_port != NULL) {
	err = ts_new_sprintf(&rev_cmd, "namespace %s cluster %s request-routing method %s %s %s", 
		ts_str(t_cl_ns), clusterName, str_method_type, str_addr_port, ts_str(t_redirect_addr));
	bail_error(err);
    }

    if (rev_cmd != NULL) {
	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);
    }

consume_binding:
    /* Consume nodes - */
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
    bail_error(err);

    snprintf(proxy_node, sizeof(proxy_node), "/nkn/nvsd/cluster/config/%s/proxy-addr-port", clusterName);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", proxy_node);
    bail_error(err);

bail:
    ts_free(&rev_cmd);
    ts_free(&t_redirect_addr);
    ts_free(&t_cl_ns);
    return err;
}

int
cli_nvsd_cluster_redir_path_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply)
{
    int err = 0;
    tbool configured = false;
    tstring *rev_cmd = NULL;
    tstring *t_redirect_path = NULL;
    const char *clusterName = NULL, *str_method_type = NULL;
    tstring *t_cl_ns = NULL, *t_cluster_type = NULL;
    node_name_t cl_type_node = {0}, node = {0}, proxy_node = {0},
		ns_cl_node = {0};

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    clusterName = tstr_array_get_str_quick(name_parts, 4);
    bail_null(clusterName);

    snprintf(ns_cl_node, sizeof(ns_cl_node), "/nkn/nvsd/cluster/config/%s/namespace", clusterName);
    err = bn_binding_array_get_value_tstr_by_name(bindings, ns_cl_node, NULL, &t_cl_ns);
    bail_error(err);

    /* get the type of cluster */
    snprintf(cl_type_node, sizeof(cl_type_node), "/nkn/nvsd/cluster/config/%s/cluster-type", clusterName);
    err = bn_binding_array_get_value_tstr_by_name(bindings, cl_type_node, NULL, &t_cluster_type);
    bail_error_null(err, t_cluster_type);

    if (0 == strcmp(ts_str(t_cluster_type), "1")) {
	/* cluster type is "redirect cluster" */
	str_method_type = "consistent-hash-redirect";
	snprintf(node, sizeof(node), "/nkn/nvsd/cluster/config/%s/redirect-base-path", clusterName);
    } else if (0 == strcmp(ts_str(t_cluster_type), "2")) {
	/* this is proxy cluster */
	str_method_type = "consistent-hash-proxy";
	snprintf(node, sizeof(node), "/nkn/nvsd/cluster/config/%s/proxy-base-path", clusterName);
    } else {
	/* we don't know the type of cluster */
	str_method_type = NULL;
    }

    err = cli_cluster_is_configured(bindings, clusterName, &configured);
    bail_error(err);

    /* NOTE: we need to consume nodes, so not going to bail */
    if (configured == 0) {
	goto consume_binding;
    }

    if (str_method_type != NULL) {
	err = bn_binding_array_get_value_tstr_by_name(bindings, node, NULL, &t_redirect_path);
	bail_error(err);
    }

    if (t_cl_ns != NULL && str_method_type != NULL
	    /* base path is not set for cluster */
	    && (t_redirect_path != NULL && strcmp(ts_str(t_redirect_path), ""))) {
	err = ts_new_sprintf(&rev_cmd, "namespace %s cluster %s request-routing method %s %s", 
		ts_str(t_cl_ns), clusterName, str_method_type,  ts_str(t_redirect_path));
	bail_error(err);
    }

    if (rev_cmd != NULL) {
	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);
    }

consume_binding:
    /* Consume nodes - */
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", name);
    bail_error(err);

    snprintf(proxy_node, sizeof(proxy_node), "/nkn/nvsd/cluster/config/%s/proxy-base-path", clusterName);
    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s", proxy_node);
    bail_error(err);

bail:
    ts_free(&rev_cmd);
    ts_free(&t_redirect_path);
    ts_free(&t_cl_ns);
    ts_free(&t_cluster_type);

    return err;
}

int
cli_cluster_rr_from_lb_to_chr(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
    /* Check the request routing for load-balance, if not bail with message*/
    /* if LB, change to CHR & reset the LB params */
    int err = 0;
    uint32 error = 0;
    const char *cluster_name = NULL, *ns_name = NULL;
    char *rr_method_node = NULL;
    const char *lb_thres_node = NULL;
    const char *lb_repli_node = NULL;
    bn_request *req = NULL;
    tstring *rr_method = NULL;
    error_msg_t err_msg = {0};

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    cluster_name = tstr_array_get_str_quick(params, 1);
    bail_null(cluster_name);

    err = cli_cluster_check_namepsace(ns_name, cluster_name, &error, &err_msg);
    bail_error(err);
    
    if (error) {
	/* not setting the err here, else internal error will be printed */
	cli_printf_error("%s",err_msg);
	goto bail;
    }
    rr_method_node = smprintf("/nkn/nvsd/cluster/config/%s/request-routing-method", cluster_name);
    bail_null(rr_method_node);

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &rr_method, NULL, "/nkn/nvsd/cluster/config/%s/request-routing-method", cluster_name);
    bail_error(err);

    if (ts_equal_str(rr_method, "1", true)) {
	/* do nothing */
	goto bail;
    }else if (ts_equal_str(rr_method, "2", true)) {

	err = bn_set_request_msg_create(&req, 0);
	bail_error(err);

	const char *req_routing_node = "/nkn/nvsd/cluster/config/%s/request-routing-method";
	lb_thres_node = "/nkn/nvsd/cluster/config/%s/load-threshold";
	lb_repli_node = "/nkn/nvsd/cluster/config/%s/replicate-mode";

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
		bt_uint32, 0, "1" , NULL,req_routing_node , cluster_name);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		bt_uint32, 0, "80", NULL,lb_thres_node, cluster_name);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_reset, 0,
		bt_uint32, 0, "1", NULL, lb_repli_node, cluster_name);
	bail_error(err);

	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
	bail_error(err);
    } else {
	cli_printf(
		_("  Request Routing Method is not configured. Not a valid operation\n"));
	goto bail;
    }
bail:
    bn_request_msg_free(&req);
	ts_free(&rr_method);
    safe_free(rr_method_node);
     return(err);
}

int
cli_ns_add_cluster_comp(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    char *cluster_name = NULL;
    char *binding_name = NULL;
    tstr_array *names = NULL;
    tstring *clus_ns = NULL;
    uint32 num_names = 0;
    uint32 idx = 0;
    tstring *free_clusters =NULL;
    const char *ns_input = NULL;
    const char *cluster_input = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(curr_word);

    err = cli_cluster_get_names(&names, true);
    bail_error_null(err, names);

    num_names = tstr_array_length_quick(names);

    ns_input = tstr_array_get_str_quick(params, 0);
    bail_null(ns_input);

    cluster_input = tstr_array_get_str_quick(params, 1);
    bail_null(cluster_input);

    if (num_names > 0)
    {
        for(idx = 0; idx < num_names; ++idx)
        {

        	cluster_name = (char *)tstr_array_get_str_quick(names, idx);

    	    binding_name = smprintf("/nkn/nvsd/cluster/config/%s/namespace", cluster_name);
    	    bail_null(binding_name);

    	    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &clus_ns, binding_name, NULL);
    	    bail_error_null(err, clus_ns);

            if ( (ts_equal_str(clus_ns, "", true)) || (ts_equal_str(clus_ns, ns_input, true)) ){

            err = ts_new_sprintf(&free_clusters, "%s", cluster_name);
            bail_error_null(err, free_clusters);

            err = tstr_array_append(ret_completions, free_clusters);
	        bail_error(err);
            }

        }
    }

bail:
   safe_free(binding_name);
   tstr_array_free (&names);
   safe_free(binding_name);
   ts_free(&free_clusters);
   ts_free(&clus_ns);
   return err;
}

int
cli_cluster_handle_suffix(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
    int err = 0;
    uint32 error = 0;
    const char *cl_name = NULL, *ns_name = NULL, *suffix_name = NULL;
    error_msg_t err_msg = {0};
    const char *action = data;
    bn_request *req = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    cl_name = tstr_array_get_str_quick(params, 1);
    bail_null(cl_name);

    suffix_name = tstr_array_get_str_quick(params, 2);
    bail_null(suffix_name);

    err = cli_cluster_check_namepsace(ns_name, cl_name, &error, &err_msg);
    bail_error(err);
    
    if (error) {
	/* not setting the err here, else internal error will be printed */
	cli_printf_error("%s",err_msg);
	goto bail;
    }

    if(strstr(suffix_name, "/")!=NULL){
    	cli_printf_error("Specify the suffix name without / [slash]");
    	goto bail;
    }

    bail_null(action);
    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_string, 0, suffix_name , NULL,
	    "/nkn/nvsd/cluster/config/%s/suffix/%s",cl_name, suffix_name);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
	    bt_uint32, 0, action, NULL,
	    "/nkn/nvsd/cluster/config/%s/suffix/%s/action",cl_name, suffix_name);
    bail_error(err);

    /* NOTE- ret_err, and ret_msg are internally handled */
    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);

bail:
    bn_request_msg_free(&req);
    return err;
}

int
cli_nvsd_suffix_show(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
    int err = 0;
    const char *cl_name = NULL, *ns_name = NULL;
    const char *suffix_name = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    cl_name = tstr_array_get_str_quick(params, 1);
    bail_null(cl_name);

    suffix_name = tstr_array_get_str_quick(params, 2);

    err = cli_show_cl_suffix(ns_name, cl_name, suffix_name, true);
    bail_error(err);

bail:
    return err;
}

int
cli_show_cl_suffix(const char *ns_name, const char *cl_name, const char *suffix_name,
	tbool print_msg)
{
    int err = 0;
    uint32 error =0;
    const char *msg = NULL;
    bn_binding_array * bindings = NULL;
    uint32 num_suffix = 0;
    node_name_t suff_binding = {0}, suff_spec = {0};
    error_msg_t err_msg = {0};

    err = cli_cluster_check_namepsace(ns_name, cl_name, &error, &err_msg);
    bail_error(err);
    
    if (error && print_msg) {
	/* not setting the err here, else internal error will be printed */
	cli_printf_error("%s",err_msg);
	goto bail;
    }

    snprintf(suff_spec, sizeof(suff_spec), "/nkn/nvsd/cluster/config/%s/suffix/*", cl_name);

    if (suffix_name == NULL ) {
	/* user wanted to show all the suffixes */
	snprintf(suff_binding,sizeof (suff_binding), "/nkn/nvsd/cluster/config/%s/suffix", cl_name);
	msg = "No suffix exists";
    } else if (suffix_name != NULL ) {
	/* user wanted us to show only one suffix map details */
	snprintf(suff_binding,sizeof (suff_binding), "/nkn/nvsd/cluster/config/%s/suffix/%s",
		cl_name, suffix_name);
	snprintf(suff_spec, sizeof(suff_spec), "/nkn/nvsd/cluster/config/%s/suffix/%s",
		cl_name, suffix_name);
	msg = "Unknown suffix name";
    } else {
	err = EINVAL;
	bail_error(err);
    }

    lc_log_basic(LOG_DEBUG, "suff-binding %s", suff_binding);
    err = mdc_get_binding_children(cli_mcc, NULL, NULL, true, &bindings, true, true, suff_binding);
    bail_error(err);

    bn_binding_array_dump("DGAUTAM",bindings, LOG_DEBUG);
    err = mdc_foreach_binding_prequeried
	(bindings, suff_spec, NULL, cli_suffix_show_one, NULL, &num_suffix);
    bail_error(err);

    if ((num_suffix == 0) && (print_msg == true)) {
	err = cli_printf_error("%s", msg);
	bail_error(err);
    }

bail:
    return err;
}


static int
cli_suffix_show_one(const bn_binding_array *bindings, uint32 idx,
	const bn_binding *binding, const tstring *name,
	const tstr_array *name_components,
	const tstring *name_last_part,
	const tstring *value, void *callback_data)
{
    int err = 0;
    const char *suffix = ts_str(name_last_part), *action = NULL;
    const bn_binding *action_binding = NULL;
    uint32 action_value = 0;
    node_name_t action_node = {0};

    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(callback_data);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(value);

    bail_null(suffix);

    snprintf(action_node, sizeof(action_node),"%s/action", ts_str(name));
    err = bn_binding_array_get_binding_by_name(bindings, action_node, &action_binding );
    bail_error_null(err, action_binding);

    bn_binding_dump(LOG_DEBUG, "DGAUTAM", action_binding);
    lc_log_basic(LOG_DEBUG, "node- %s, idx = %d", action_node, idx);

    err = bn_binding_get_uint32(action_binding, ba_value, 0, &action_value);
    bail_error(err);

    if (action_value == 1) {
	action = "proxy";
    } else if (action_value == 2) {
	action = "redirect";
    } else {
	action = "Unknown";
    }
    if (idx == 0) {
	cli_printf("   Suffix              Action\n");
	cli_printf(" ------------------   ----------\n");
    }

    cli_printf("   %-16s    %s\n", suffix, action );

bail:
    return err;

}

int
cli_cluster_check_namepsace(const char *ns_name, const char *cl_name,
	uint32 *error, error_msg_t *err_msg)
{
    int err = 0;
    node_name_t bn_name;
    bn_binding *binding = NULL;
    tstring *namespace = NULL;

    snprintf(bn_name, sizeof(bn_name), "/nkn/nvsd/cluster/config/%s", cl_name);

    // Check if cluster exists.
    err = mdc_get_binding(cli_mcc, NULL, NULL,
            false, &binding, bn_name, NULL);
    bail_error(err);

    if (binding == NULL) {
	*error = EINVAL;
	snprintf(*err_msg, sizeof(*err_msg), "cluster doesn't exist");
	goto bail;
    }

    snprintf(bn_name, sizeof(bn_name), "/nkn/nvsd/cluster/config/%s/namespace", cl_name);

    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL,
            NULL, &namespace, bn_name, NULL);
    bail_error(err);

    if ((namespace == NULL) || (0 == strcmp(ts_str(namespace), ""))) {
	*error = EINVAL;
	snprintf(*err_msg, sizeof(*err_msg), "namespace is not added to cluster");
	goto bail;
    }

    if (strcmp(ns_name, ts_str(namespace))) {
	*error = EINVAL;
	snprintf(*err_msg, sizeof(*err_msg), "namespace %s is not added to cluster %s",
		ns_name, cl_name);
	goto bail;
    }
    *error = 0;

bail:
    bn_binding_free(&binding);
    ts_free(&namespace);
    return err;
}
int
cli_nvsd_suffix_delete(void *data, const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
    int err = 0;
    uint32 error = 0 ;
    const char *cl_name = NULL, *ns_name = NULL;
    const char *suffix_name = NULL;
    error_msg_t err_msg = {0};
    tstring *ret_msg = NULL;
    node_name_t suff_binding = {0};

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    ns_name = tstr_array_get_str_quick(params, 0);
    bail_null(ns_name);

    cl_name = tstr_array_get_str_quick(params, 1);
    bail_null(cl_name);

    err = cli_cluster_check_namepsace(ns_name, cl_name, &error, &err_msg);
    bail_error(err);
    
    if (error) {
	/* not setting the err here, else internal error will be printed */
	cli_printf_error("%s",err_msg);
	goto bail;
    }
    suffix_name = tstr_array_get_str_quick(params, 2);

    if (suffix_name == NULL && cmd->cc_magic == 0) {
	/* user wanted to show all the suffixes */
	snprintf(suff_binding,sizeof (suff_binding), "/nkn/nvsd/cluster/config/%s/suffix", cl_name);
	err = mdc_delete_binding_children(cli_mcc, &error, &ret_msg, suff_binding);
	bail_error(error);
    } else if (suffix_name != NULL && cmd->cc_magic == 1) {
	/* user wanted us to show only one suffix map details */
	snprintf(suff_binding,sizeof (suff_binding), "/nkn/nvsd/cluster/config/%s/suffix/%s",
		cl_name, suffix_name);
	err = mdc_delete_binding(cli_mcc, &error, &ret_msg, suff_binding);
	bail_error(error);
    } else {
	err = EINVAL;
	bail_error(err);
    }

    lc_log_basic(LOG_DEBUG, "suff-binding %s, error=> %d, msg - %s",
	    suff_binding, error, ts_str(ret_msg));

bail:
    return err;
}

static int
cli_cluster_is_configured(const bn_binding_array *bindings, const char *clusterName, tbool *configured)
{
    int err = 0;
    tbool configd = false;
    node_name_t config_node = {0};

    snprintf(config_node, sizeof(config_node), "/nkn/nvsd/cluster/config/%s/configured", clusterName);
    err = bn_binding_array_get_value_tbool_by_name(bindings, config_node, NULL, &configd);
    bail_error(err);

    *configured = configd;
bail:
    return err;
}
/*---------------------------------------------------------------------------*/
