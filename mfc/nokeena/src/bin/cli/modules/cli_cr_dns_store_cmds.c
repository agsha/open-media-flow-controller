/*
 * Filename:    cli_cr_dns_store_cmds.c
 * Date:        2012/4/16
 * Author:      Manikandan V
 *
 * (C) Copyright 2012,  Juniper Networks Inc.
 *  All rights reserved.
 */

#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "cli_module.h"

int 
cli_cr_dns_store_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


int
cli_cr_dns_store_pop_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_cr_dns_store_cache_group_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_cr_dns_store_cache_entity_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_cr_dns_store_domain_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


int
cli_stored_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

extern int
cli_std_exec(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);

static int
cli_cg_validate_and_bind(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

static int
cli_pop_validate_and_bind(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

#if 0
int
cli_cr_domain_staic_routing_cfg_callback(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);
#endif

typedef char node_name_t[256];
/*---------------------------------------------------------------------------*/
int 
cli_cr_dns_store_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns-store";
    cmd->cc_help_descr =        N_("Configure dns-store options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns-store max-domains";
    cmd->cc_help_descr =        N_("Configure dns-store maximum no.of domains");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns-store max-domains *";
    cmd->cc_help_exp  =        N_("<No.of domains>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/global/max_domains";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns-store max-cache-entities";
    cmd->cc_help_descr =        N_("Configure dns-store maximum no.of cache-entities");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns-store max-cache-entities *";
    cmd->cc_help_exp =        N_("<No.of cache entities>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/global/max_cache_entities";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns-store listen";
    cmd->cc_help_descr =        N_("Configure dns-store listen params");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns-store listen port";
    cmd->cc_help_descr =        N_("Configure dns-store listen params");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns-store listen path";
    cmd->cc_help_descr =        N_("Configure dns-store listen params");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns-store listen path *";
    cmd->cc_help_exp =        N_("use UDS for dns-store configuration");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/global/listen_path";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns-store listen port *";
    cmd->cc_help_exp =        N_("use network socket for dns-store configuration");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/global/listen_port";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns-store debug-assert";
    cmd->cc_help_descr =        N_("Configure dns-store debug options");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns-store debug-assert enable";
    cmd->cc_help_descr =        N_("Enable the debug assert for cr-dns-store");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/global/debug_assert";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "dns-store debug-assert disable";
    cmd->cc_help_descr =        N_("Disable the debug assert for cr-dns-store");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/global/debug_assert";
    CLI_CMD_REGISTER;

    err = cli_cr_dns_store_pop_cmds_init(info, context);
    bail_error(err);

    err = cli_cr_dns_store_cache_group_cmds_init(info, context);
    bail_error(err);

    err = cli_cr_dns_store_cache_entity_cmds_init(info, context);
    bail_error(err);

    err = cli_cr_dns_store_domain_cmds_init(info, context);
    bail_error(err);

bail:
    return err;
}


int
cli_cr_dns_store_pop_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop";
    cmd->cc_help_descr =        N_("Configure POP params");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop add";
    cmd->cc_help_descr =        N_("Configure POP params");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop delete";
    cmd->cc_help_descr =        N_("Negate/Clear/Reset POP params");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop add *";
    cmd->cc_help_exp =          N_("<pop_name>");
    cmd->cc_help_exp_hint =     N_("enter new name for POP");
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/pop/*";
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_create;
    cmd->cc_exec_callback =     cli_stored_enter_mode;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/pop/$1$";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop delete *";
    cmd->cc_help_exp =          N_("<pop_name>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_delete;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/pop/*";
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/pop/$1$";
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop add * location";
    cmd->cc_help_descr =        N_("Configure POP location params");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop delete * location";
    cmd->cc_help_descr =        N_("Negate/Clear/Reset POP location params");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop add * location latitude";
    cmd->cc_help_descr =        N_("Configure POP latitude");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop delete * location latitude";
    cmd->cc_help_descr =        N_("Negate/Clear/Reset POP latitude");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop add * location longitude";
    cmd->cc_help_descr =        N_("Configure POP longitude");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop delete * location longitude";
    cmd->cc_help_descr =        N_("Negate/Clear/Reset POP longitude");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop add * location latitude *";
    cmd->cc_help_exp =          N_("<latitude>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/pop/$1$/location/latitude";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop delete * location latitude *";
    cmd->cc_help_exp =          N_("<latitude>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/pop/$1$/location/latitude";
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop add * location longitude *";
    cmd->cc_help_exp =          N_("<longitude>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/pop/$1$/location/longitude";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "pop delete * location longitude *";
    cmd->cc_help_exp =          N_("<longitude>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/pop/$1$/location/longitude";
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

bail:
    return err;
}

int
cli_cr_dns_store_cache_group_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-group";
    cmd->cc_help_descr =        N_("Configure Cache grouping params");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-group add";
    cmd->cc_help_descr =        N_("Cofigure a Cache group");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-group delete";
    cmd->cc_help_descr =        N_("Negate/Clear/Reset Cache group");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-group add *";
    cmd->cc_help_exp =          N_("<cache-group name>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_create;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/cache-group/*";
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-group/$1$";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-group delete *";
    cmd->cc_help_exp =          N_("<cache-group name>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_delete;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/cache-group/*";
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-group/$1$";
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

 bail:
    return err;
}

int
cli_cr_dns_store_cache_entity_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity";
    cmd->cc_help_descr =        N_("Configure Cache Entity params");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add";
    cmd->cc_help_descr =        N_("Configure Cache Entity");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity delete";
    cmd->cc_help_descr =        N_("Negate/Clear/Reset Cache Entity");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add *";
    cmd->cc_help_exp =          N_("<cache_entity_name>");
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/cache-entity/*";
    cmd->cc_exec_callback =     cli_stored_enter_mode;
    cmd->cc_exec_operation =    cxo_create;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity delete *";
    cmd->cc_help_exp =          N_("<cache_entity_name>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_delete;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/cache-entity/*";
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$";
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * ipv4address";
    cmd->cc_help_descr =        N_("Configue an address for cache-entity");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * ipv4address *";
    cmd->cc_help_exp =          N_("<ipv4address>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$/ipv4address";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * ipv6address";
    cmd->cc_help_descr =        N_("Configue an address for cache-entity");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * ipv6address *";
    cmd->cc_help_exp =          N_("<ipv6address>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$/ipv6address";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * cname";
    cmd->cc_help_descr =        N_("Configue an address for cache-entity");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * cname *";
    cmd->cc_help_exp =          N_("<CNAME>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$/cname";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * bind";
    cmd->cc_help_descr =        N_("Binding options for cache entity");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * bind cache-group";
    cmd->cc_help_descr =        N_("Bind a cache group for cache entity");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * bind pop";
    cmd->cc_help_descr =        N_("Bind a POP for cache entity");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * remove";
    cmd->cc_help_descr =        N_(" Remove the bound options for cache entity");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * remove cache-group";
    cmd->cc_help_descr =        N_("Remove a cache group for cache entity");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * remove pop";
    cmd->cc_help_descr =        N_("Remove a POP for cache entity");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * bind cache-group *";
    cmd->cc_help_exp =          N_("<cache_group_name>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/cache-group/*";
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$/cache-group/$2$";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
	cmd->cc_exec_callback  =	cli_cg_validate_and_bind;
	cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * bind pop *";
    cmd->cc_help_exp =          N_("<pop_name>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/pop/*";
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$/pop/$2$";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
	cmd->cc_exec_callback  =	cli_pop_validate_and_bind;
	cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * remove cache-group *";
    cmd->cc_help_exp =          N_("<cache_group_name>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/cache-group/*";
    cmd->cc_exec_operation =    cxo_delete;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$/cache-group/$2$";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * remove pop *";
    cmd->cc_help_exp =          N_("<pop_name>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/pop/*";
    cmd->cc_exec_operation =    cxo_delete;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$/pop/$2$";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * water-mark";
    cmd->cc_help_descr =        N_("Configue an water mark value for cache-entity");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * water-mark *";
    cmd->cc_help_exp =          N_("<Number>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$/water_mark";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * load-feedback-method";
    cmd->cc_help_descr =        N_("Configure load feedback method");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * load-feedback-method XML";
    cmd->cc_help_descr =        N_("Enable XML based Load feedback method");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$/lf_method";
    cmd->cc_exec_value =        "1";
    cmd->cc_exec_type =         bt_uint8;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * load-feedback-method SNMP";
    cmd->cc_help_descr =        N_("Enable SNMP based Load feedback method");
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$/lf_method";
    cmd->cc_exec_value =        "2";
    cmd->cc_exec_type =         bt_uint8;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * load-feedback-method polling-freq";
    cmd->cc_help_descr =        N_("Configure polling frequency load feedback");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * load-feedback-method polling-freq *";
    cmd->cc_help_exp =          N_("<Polling-frequency>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$/poll_freq";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * load-feedback-method port";
    cmd->cc_help_descr =        N_("Configure load feedback method port number");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "cache-entity add * load-feedback-method port *";
    cmd->cc_help_exp =          N_("<Port number>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cache-entity/$1$/port";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

bail:
    return err;
}

int
cli_cr_dns_store_domain_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain";
    cmd->cc_help_descr =        N_("Configure domain record options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add";
    cmd->cc_help_descr =        N_("Add a domain record");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain delete";
    cmd->cc_help_descr =        N_("Negate/Clear/Reset domain record");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add *";
    cmd->cc_help_exp =          N_("<domain_name>");
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/cr_domain/*";
    cmd->cc_exec_operation =    cxo_create;
    cmd->cc_exec_callback =     cli_stored_enter_mode;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cr_domain/$1$";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain delete *";
    cmd->cc_help_exp =          N_("<domain_name>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_delete;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/cr_domain/*";
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cr_domain/$1$";
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * last-resort";
    cmd->cc_help_descr =        N_("Configure a last-resort option for a domain");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * last-resort *";
    cmd->cc_help_exp =          N_("<Last-resort address>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cr_domain/$1$/last_resort";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * ttl";
    cmd->cc_help_descr =        N_("Configure a ttl value for a domain");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * ttl *";
    cmd->cc_help_exp =          N_("<ttl value>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cr_domain/$1$/ttl";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing";
    cmd->cc_help_descr =        N_("Configure the routing options for a domain");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing static";
    cmd->cc_help_descr =        N_("Enable static routing");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cr_domain/$1$/routing";
    cmd->cc_exec_value =        "1";
    cmd->cc_exec_type =         bt_uint8;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing rr";
    cmd->cc_help_descr =        N_("Enable round robin routing");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cr_domain/$1$/routing";
    cmd->cc_exec_value =        "2";
    cmd->cc_exec_type =         bt_uint8;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing geo-load";
    cmd->cc_help_descr =        N_("Enable/configure geo and load info based routing");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cr_domain/$1$/routing";
    cmd->cc_exec_value =        "3";
    cmd->cc_exec_type =         bt_uint8;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing geo-load weight";
    cmd->cc_help_descr =        N_("Configure the weightage for geo and load based routing");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing geo-load weight geo-cost";
    cmd->cc_help_descr =        N_("Configure the weightage for geo info based routing");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing geo-load weight geo-cost *";
    cmd->cc_help_exp =          N_("<weightage value number>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cr_domain/$1$/geo_weight";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing geo-load weight load-cost";
    cmd->cc_help_descr =        N_("Configure the weightage for load info based routing");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing geo-load weight load-cost *";
    cmd->cc_help_exp =          N_("<weightage value number>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cr_domain/$1$/load_weight";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing static ip-range *";
    cmd->cc_help_exp =          N_("<start range>");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing static ip-range * *";
    cmd->cc_help_exp =          N_("<end range>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cr_domain/$1$/static_ip_range/start";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_exec_name2 =        "/nkn/cr_dns_store/config/cr_domain/$1$/static_ip_range/end";
    cmd->cc_exec_value2 =       "$3$";
    cmd->cc_exec_type2 =        bt_string;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing static ip-range";
    cmd->cc_help_descr =        N_("Configure static ip range for a domain");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing static ip-range *";
    cmd->cc_help_exp =          N_("<start range>");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing static ip-range * *";
    cmd->cc_help_exp =          N_("<end range>");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing static ip-range * * cache-group";
    cmd->cc_help_descr =          N_("<Bind a cache-group with this ip-range>");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * routing static ip-range * * cache-group *";
    cmd->cc_help_exp =          N_("<cache-group name>");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/cache-group/*";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
	cmd->cc_exec_callback  =	cli_cr_domain_staic_routing_cfg_callback;
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * cache-group";
    cmd->cc_help_descr =        N_("Configure cache-group for domain");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain add * cache-group *";
    cmd->cc_help_exp =          N_("<cache-group-name>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cr_dns_store/config/cache-group/*";
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cr_domain/$1$/cache_group";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
	cmd->cc_exec_callback  =	cli_cg_validate_and_bind;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "domain delete * cache-group";
    cmd->cc_help_descr =        N_("<cache-group-name>");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/cr_dns_store/config/cr_domain/$1$/cache_group";
    CLI_CMD_REGISTER;

bail:
    return err;
}


int
cli_stored_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;

    if(cli_prefix_modes_is_enabled()) {
        err = cli_prefix_enter_mode(cmd, cmd_line);
        bail_error(err);
    }
    else {
        err = cli_print_incomplete_command_error(cmd, cmd_line);
        bail_error(err);
    }

    cli_std_exec(data, cmd, cmd_line, params);


bail:
    return err;

}


static int
cli_pop_validate_and_bind(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
    int err = 0;
    bn_binding *binding = NULL;
    node_name_t bn_name = {0};

    snprintf(bn_name, sizeof(bn_name),"/nkn/cr_dns_store/config/pop/%s", tstr_array_get_str_quick(params, 1));
    bail_null(bn_name);

    err = mdc_get_binding(cli_mcc, NULL, NULL,
	    false, &binding, bn_name, NULL);
    bail_error(err);


    if (binding) {
        cli_std_exec(data, cmd, cmd_line, params);
    }
    else
    {
    	cli_printf_error(_(" POP: %s doesn't exists"), tstr_array_get_str_quick(params, 1));
    }

bail:
    bn_binding_free(&binding);
    return err;
}

static int
cli_cg_validate_and_bind(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
    int err = 0;
    bn_binding *binding = NULL;
    node_name_t bn_name = {0};

    snprintf(bn_name, sizeof(bn_name),"/nkn/cr_dns_store/config/cache-group/%s", tstr_array_get_str_quick(params, 1));
    bail_null(bn_name);

    err = mdc_get_binding(cli_mcc, NULL, NULL,
	    false, &binding, bn_name, NULL);
    bail_error(err);


    if (binding) {
        cli_std_exec(data, cmd, cmd_line, params);
    }
    else
    {
    	cli_printf_error(_(" Cache-group: %s doesn't exists"), tstr_array_get_str_quick(params, 1));
    }

bail:
    bn_binding_free(&binding);
    return err;
}

#if 0

int
cli_cr_domain_staic_routing_cfg_callback(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	const char *c_domain = NULL;
	const char *c_start_range = NULL;
	const char *c_end_range = NULL;
	const char *c_cache_group = NULL;
    bn_request *req = NULL;
	node_name_t iprange_index = {0};

	c_domain = tstr_array_get_str_quick(params, 0);
	bail_null(c_domain);

	c_start_range = tstr_array_get_str_quick(params, 1);
	bail_null(c_start_range);

	c_end_range = tstr_array_get_str_quick(params, 2);
	bail_null(c_end_range);

	c_cache_group = tstr_array_get_str_quick(params, 3);

	snprintf(iprange_index , sizeof(iprange_index), "%s%s%s", c_start_range, c_end_range, c_cache_group);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);


	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_create, 0,
			bt_string, 0, iprange_index, NULL, "/nkn/cr_dns_store/config/cr_domain/%s/static_ip_range/%s", c_domain, iprange_index);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
			bt_string, 0, c_start_range, NULL, "/nkn/cr_dns_store/config/cr_domain/%s/static_ip_range/%s/start", c_domain, iprange_index);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
			bt_string, 0, c_end_range, NULL, "/nkn/cr_dns_store/config/cr_domain/%s/static_ip_range/%s/end", c_domain, iprange_index);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding_fmt(req, bsso_modify, 0,
			bt_string, 0, c_cache_group, NULL, "/nkn/cr_dns_store/config/cr_domain/%s/static_ip_range/%s/cg", c_domain, iprange_index);
	bail_error(err);

	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);

bail:
	bn_request_msg_free(&req);
	return err;
}
#endif
