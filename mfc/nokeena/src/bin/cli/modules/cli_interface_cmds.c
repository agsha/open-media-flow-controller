/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */

static const char rcsid[] = "$Id: cli_interface_cmds.c,v 1.152 2012/06/09 02:08:18 et Exp $";
#define UNUSED_ARGUMENT(x) (void )x

#include <stdio.h>
#include "climod_common.h"
#include "hash_table.h"

/* XXX/EMT */
//#include "../../../lib/libcli/cli_standard_handlers.h"

/*
 * XXX/EMT: we use ccf_ignore_length in a few places below, where a 
 * long cc_help_exp misaligns the columns.  Would be nice to think of
 * a better string which fit...
 */

/* ------------------------------------------------------------------------- */
cli_expansion_option cli_interface_duplexes[] = {
    {"half", N_("Half duplex"), NULL},
    {"full", N_("Full duplex"), NULL},
    {"auto", N_("Automatically detect duplex"), NULL}
};

cli_expansion_option cli_interface_speeds[] = {
    {"10", N_("10 Mbit/sec"), NULL},
    {"100", N_("100 Mbit/sec"), NULL},
    {"1000", N_("1000 Mbit/sec"), NULL},
    {"10000", N_("10000 Mbit/sec"), NULL},
    {"auto", N_("Automatically detect speed"), NULL}
};

enum {
    cid_interface_show,
    cid_interface_show_brief,
    cid_interface_show_conf,
    cid_interface_normal,
    cid_interface_alias,

    cid_interface_ipv6_addr,
    cid_interface_ipv6_addr_delete,
    cid_interface_ipv6_addr_delete_all,
};

extern int cli_std_exec(void *data, const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params);

extern int cli_std_comp_options(void *data, const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params,
	const tstring *curr_word,
	tstr_array *ret_completions);

extern int cli_std_help_options(void *data, cli_help_type help_type,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params,
	const tstring *curr_word,
	void *context);

extern int cli_std_help_completion(void *data, cli_help_type help_type,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params,
	const tstring *curr_word,
	void *context);


#ifdef PROD_FEATURE_BONDING
enum {
    cid_intf_bonding_add,
    cid_intf_bonding_delete
};
#endif /* PROD_FEATURE_BONDING */

typedef struct cli_interface_show_context {
    uint32 cisc_magic;
    tbool cisc_show_all;
    bn_binding_array *cisc_config_bindings;
    bn_binding_array *cisc_addr_bindings;
    bn_binding_array *cisc_vlan_bindings;
    ht_table *cisc_aliases;
    tbool cisc_ipv6_enable_overall;
    tbool cisc_dhcpv6_stateless;
} cli_interface_show_context;

int cli_interface_cmds_init(const lc_dso_info *info, 
                           const cli_module_context *context);

int cli_interface_get_if_list(tstr_array **ret_interfaces, tbool hide_display);

int cli_interface_validate(const char *ifname, tbool *ret_valid);

int cli_interface_enter_mode(void *data, const cli_command *cmd,
                             const tstr_array *cmd_line,
                             const tstr_array *params);

int cli_interface_check_exec(void *data, const cli_command *cmd,
                             const tstr_array *cmd_line,
                             const tstr_array *params);

int cli_interface_speed_help(void *data, cli_help_type help_type, 
                             const cli_command *cmd,
                             const tstr_array *cmd_line,
                             const tstr_array *params,
                             const tstring *curr_word,
                             void *context);

int cli_interface_speed_completion(void *data, const cli_command *cmd,
                                   const tstr_array *cmd_line,
                                   const tstr_array *params,
                                   const tstring *curr_word,
                                   tstr_array *ret_completions);

int cli_interface_ip_addr(void *data, const cli_command *cmd,
                          const tstr_array *cmd_line,
                          const tstr_array *params);

static int cli_interface_ipv6_addr(void *data, const cli_command *cmd,
                                   const tstr_array *cmd_line,
                                   const tstr_array *params);

int cli_interface_addr_revmap(void *data, const cli_command *cmd,
                              const bn_binding_array *bindings,
                              const char *name,
                              const tstr_array *name_parts,
                              const char *value, const bn_binding *binding,
                              cli_revmap_reply *ret_reply);

static int cli_interface_ipv6_addr_completion(void *data,
                                              const cli_command *cmd,
                                              const tstr_array *cmd_line,
                                              const tstr_array *params,
                                              const tstring *curr_word,
                                              tstr_array *ret_completions);

static int cli_interface_ipv6_addr_revmap(void *data, const cli_command *cmd,
                                          const bn_binding_array *bindings,
                                          const char *name,
                                          const tstr_array *name_parts,
                                          const char *value, 
                                          const bn_binding *binding,
                                          cli_revmap_reply *ret_reply);

int cli_interface_show_one(const bn_binding_array *bindings, uint32 idx,
                           const bn_binding *binding, const tstring *name,
                           const tstr_array *name_components,
                           const tstring *name_last_part,
                           const tstring *value,
                           void *callback_data);

int cli_interface_show(void *data, const cli_command *cmd,
                       const tstr_array *cmd_line,
                       const tstr_array *params);

#ifdef PROD_FEATURE_BONDING
int cli_interface_bonding(void *data, const cli_command *cmd,
                          const tstr_array *cmd_line,
                          const tstr_array *params);
#endif /* PROD_FEATURE_BONDING */

/* ------------------------------------------------------------------------- */
/* There are two subtrees under /net/interface: config and state.
 * Static configuration is stored under config, and runtime state is
 * stored under state.  Note that initially, no interfaces are
 * configured, so there is nothing under config, though all detected
 * interfaces immediately show up under state.
 */


/* ------------------------------------------------------------------------- */
int
cli_interface_cmds_init(const lc_dso_info *info, 
                           const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = cli_set_gettext_domain(GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface";
    cmd->cc_flags |=                ccf_acl_inheritable;
    cli_acl_add(cmd,                tacl_sbm_net);
    cli_acl_opers_exec(cmd,         tao_set_modify, tao_set_create,
                                    tao_action);
    cli_acl_set_modes(cmd,          cms_config);
    cmd->cc_help_descr = 
        N_("Configure network interfaces");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface";
    cmd->cc_flags |=                ccf_acl_inheritable;
    cli_acl_add(cmd,                tacl_sbm_net);
    cli_acl_opers_exec(cmd,         tao_set_modify, tao_set_delete,
                                    tao_action);
    cli_acl_set_modes(cmd,          cms_config);
    cmd->cc_help_descr =
        N_("Clear certain network interface settings");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_flags |=                ccf_prefix_mode_root;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =
        N_("<interface name>");

    /* Suppress termination help if prefix modes are disabled */
    cmd->cc_help_term =             "";

    cmd->cc_help_term_prefix =
        N_("Enter interface mode");
    cmd->cc_help_callback =         cli_interface_help;
    cmd->cc_comp_callback =         cli_interface_completion;
    cmd->cc_exec_callback =         cli_interface_enter_mode;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface *";
    cmd->cc_help_exp =
        N_("<interface name>");
    cmd->cc_help_callback =         cli_interface_help;
    cmd->cc_comp_callback =         cli_interface_completion;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * no";
    cmd->cc_help_descr =
        N_("Clear certain network interface settings");
    cmd->cc_req_prefix_count =      2;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * comment";
    cmd->cc_help_descr =         
        N_("Add a comment for this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * comment *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =              N_("<comment>");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/net/interface/config/$1$/comment";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * comment";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Remove the comment from this interface");
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/net/interface/config/$1$/comment";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * create";
    cmd->cc_flags |=                ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =         
        N_("Create a configuration record for this interface");
    cmd->cc_exec_operation =        cxo_create;
    cmd->cc_exec_name =             "/net/interface/config/$1$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_value =            "$1$";
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    cmd->cc_revmap_suborder =       -10;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * create";
    cmd->cc_flags |=                ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Delete the configuration for this interface");
    cmd->cc_exec_operation =        cxo_delete;
    cmd->cc_exec_name =             "/net/interface/config/$1$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * alias";
    cmd->cc_help_descr =
        N_("Configure an alias on this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * alias *";
    cmd->cc_help_exp =
        N_("<alias index>");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * alias * ip";
    cmd->cc_help_descr =
        N_("Configure the IP information for this alias interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * alias * ip address";
    cmd->cc_help_descr =
        N_("Configure the IP address and netmask for this alias interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * alias * ip address *";
    cmd->cc_help_exp =              cli_msg_ip_addr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * alias * ip address * *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_magic =                 cid_interface_alias;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    /* XXX/EMT: this should be two options */
    cmd->cc_help_exp = 
        N_("<netmask>");
    cmd->cc_help_exp_hint =
        N_("e.g. 255.255.255.248, or <mask length>, e.g. /29");
    cmd->cc_exec_callback =         cli_interface_ip_addr;
    /* Get's revmapped under regular static IP address configuration */
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * alias";
    cmd->cc_help_descr =
        N_("Remove an alias on this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * alias *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cli_acl_opers_exec(cmd,         tao_set_delete); 
    cmd->cc_help_exp =
        N_("<alias index>");
    cmd->cc_exec_operation =        cxo_delete;
    cmd->cc_exec_name =             "/net/interface/config/$1$:$2$";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * duplex";
    cmd->cc_help_descr =
        N_("Configure the duplex of this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * duplex *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =
        N_("<duplex>");
    cmd->cc_help_options =          cli_interface_duplexes;
    cmd->cc_help_num_options =      sizeof(cli_interface_duplexes) /
        sizeof(cli_expansion_option);
    cmd->cc_comp_type =             cct_use_help_options;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =       "/net/interface/config/$1$/type/ethernet/duplex";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_callback =         cli_interface_check_exec;
#if 0
    cmd->cc_revmap_type =           crt_auto;
#else
    /*
     * This is just done as a test case for bug 10088.  The crt_auto
     * is perfectly functional, and otherwise preferable.
     */
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_names =      "/net/interface/config/*/type/ethernet/duplex";
    cmd->cc_revmap_cmds =           "interface $4$ duplex $v$";
#endif
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * duplex";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Reset the duplex setting for this interface to its default");
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =       "/net/interface/config/$1$/type/ethernet/duplex";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * speed";
    cmd->cc_help_descr =
        N_("Configure the speed of this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * speed *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =
        N_("<speed>");
    cmd->cc_help_options =          cli_interface_speeds;
    cmd->cc_help_num_options =      sizeof(cli_interface_speeds) /
        sizeof(cli_expansion_option);
    cmd->cc_help_callback =         cli_interface_speed_help;
    cmd->cc_comp_callback =         cli_interface_speed_completion;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =        "/net/interface/config/$1$/type/ethernet/speed";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * speed";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Reset the speed setting for this interface to its default");
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =        "/net/interface/config/$1$/type/ethernet/speed";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * mtu";
    cmd->cc_help_descr =
        N_("Configure the MTU of this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * mtu *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =
        N_("<MTU in bytes>");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/net/interface/config/$1$/mtu";
    cmd->cc_exec_type =             bt_uint16;
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * mtu";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Reset the MTU for this interface to its default");
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/net/interface/config/$1$/mtu";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * display";
    cmd->cc_flags |=                ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =         
        N_("Un-hide this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/net/interface/config/$1$/display";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * display";
    cmd->cc_flags |=                ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =         
        N_("Hide this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/net/interface/config/$1$/display";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * ip";
    cmd->cc_help_descr =  
        N_("Configure IP options on this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * ip address";
    cmd->cc_help_descr =
        N_("Configure the IP address and netmask for this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * ip address *";
    cmd->cc_help_exp =              cli_msg_ip_addr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * ip address * *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_magic =                 cid_interface_normal;
    /* XXX/EMT: this should be two options */
    cmd->cc_help_exp = 
        N_("<netmask>");
    cmd->cc_help_exp_hint =
        N_("e.g. 255.255.255.248, or <mask length>, e.g. /29");
    cmd->cc_exec_callback =         cli_interface_ip_addr;
    cmd->cc_revmap_names =        "/net/interface/config/*/addr/ipv4/static/*";
    cmd->cc_revmap_order =          cro_interface;
    cmd->cc_revmap_callback =       cli_interface_addr_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * ip";
    cmd->cc_help_descr =
        N_("Negate certain IP options for this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * ip address";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cli_acl_opers_exec(cmd,         tao_set_delete); 
    cmd->cc_help_descr =
        N_("Erase the IP address and netmask for this interface");
    cmd->cc_exec_operation =        cxo_delete;
    cmd->cc_exec_name =         "/net/interface/config/$1$/addr/ipv4/static/1";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    CLI_CMD_REGISTER;

#ifdef PROD_FEATURE_DHCP_CLIENT
    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * dhcp";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Enable DHCP for this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/net/interface/config/$1$/addr/ipv4/dhcp";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * dhcp";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Disable DHCP for this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/net/interface/config/$1$/addr/ipv4/dhcp";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * dhcp renew";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cli_acl_opers_exec(cmd,         tao_action);
    cli_acl_set_modes(cmd,          cms_enable);
    cmd->cc_help_descr =
        N_("Renew DHCP lease for this interface");
    cmd->cc_exec_operation =        cxo_action;
    cmd->cc_exec_action_name =      "/net/interface/actions/dhcp/renew";
    cmd->cc_exec_name =             "ifname";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_value =            "$1$";
    CLI_CMD_REGISTER;

#else 

    /*
     * The mgmtd bindings are not conditional on
     * PROD_FEATURE_DHCP_CLIENT, so we have to do this to avoid getting
     * warnings on reverse-mapping.
     */
    err = cli_revmap_ignore_bindings_va
        (2,
         "/net/interface/config/*/addr/ipv4/dhcp",
         "/net/interface/config/*/addr/ipv6/dhcp/enable");
    bail_error(err);

#endif /* PROD_FEATURE_DHCP_CLIENT */

#ifdef PROD_FEATURE_ZEROCONF
    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * zeroconf";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Enable zeroconf for this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =
        "/net/interface/config/$1$/addr/ipv4/zeroconf";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * zeroconf";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Disable zeroconf for this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =
        "/net/interface/config/$1$/addr/ipv4/zeroconf";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

#else

    /*
     * The mgmtd bindings are not conditional on
     * PROD_FEATURE_ZEROCONF, so we have to do this to avoid getting
     * warnings on reverse-mapping.
     */
    err = cli_revmap_ignore_bindings_va
        (1, "/net/interface/config/*/addr/ipv4/zeroconf");
    bail_error(err);

#endif /* PROD_FEATURE_ZEROCONF */

#ifdef PROD_FEATURE_IPV6

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * ipv6";
    cmd->cc_help_descr =
        N_("Configure IPv6 options on this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * ipv6";
    cmd->cc_help_descr =
        N_("Clear certain IPv6 options on this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * ipv6 enable";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Enable IPv6 on this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =
        "/net/interface/config/$1$/addr/ipv6/enable";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface_ipv6;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * ipv6 enable";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Disable IPv6 on this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =
        "/net/interface/config/$1$/addr/ipv6/enable";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface_ipv6;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * ipv6 dhcp";
    cmd->cc_help_descr =
        N_("Configure DHCPv6 on this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * ipv6 dhcp";
    cmd->cc_help_descr =
        N_("Clear certain DHCPv6 options on this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * ipv6 dhcp client";
    cmd->cc_help_descr =
        N_("Configure DHCPv6 client on this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * ipv6 dhcp client";
    cmd->cc_help_descr =
        N_("Clear certain DHCPv6 client options on this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * ipv6 dhcp client enable";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Enable DHCPv6 on this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =
        "/net/interface/config/$1$/addr/ipv6/dhcp/enable";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface_ipv6;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * ipv6 dhcp client enable";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Disable DHCPv6 on this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =
        "/net/interface/config/$1$/addr/ipv6/dhcp/enable";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface_ipv6;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * ipv6 dhcp client renew";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cli_acl_opers_exec(cmd,         tao_action);
    cli_acl_set_modes(cmd,          cms_enable);
    cmd->cc_help_descr =
        N_("Renew DHCPv6 lease for this interface");
    cmd->cc_exec_operation =        cxo_action;
    cmd->cc_exec_action_name =      "/net/interface/actions/ipv6/dhcp/renew";
    cmd->cc_exec_name =             "ifname";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_value =            "$1$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * ipv6 address";
    cmd->cc_help_descr =
        N_("Configure IPv6 addressing for this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * ipv6 address";
    cmd->cc_magic =                 cid_interface_ipv6_addr_delete_all;
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cli_acl_opers_exec(cmd,         tao_set_delete);
    cmd->cc_help_descr =
        N_("Remove IPv6 addresses for this interface");
    cmd->cc_help_term =
        N_("Remove all IPv6 addresses for this interface");
    cmd->cc_exec_callback =         cli_interface_ipv6_addr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * ipv6 address autoconfig";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Enable IPv6 stateless address autoconfiguration for this "
           "interface");
    cmd->cc_help_term =
        N_("Enable IPv6 stateless address autoconfiguration for this "
           "interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =
        "/net/interface/config/$1$/addr/ipv6/slaac/enable";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface_ipv6;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * ipv6 address autoconfig";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Disable IPv6 stateless address autoconfiguration on this "
           "interface");
    cmd->cc_help_term =
        N_("Disable IPv6 stateless address autoconfiguration on this "
           "interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =
        "/net/interface/config/$1$/addr/ipv6/slaac/enable";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface_ipv6;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =
        "interface * ipv6 address autoconfig default";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Enable learning routes from address autoconfiguration");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =
        "/net/interface/config/$1$/addr/ipv6/slaac/default_route";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface_ipv6;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =
        "no interface * ipv6 address autoconfig default";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Disable learning routes from address autoconfiguration");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =
        "/net/interface/config/$1$/addr/ipv6/slaac/default_route";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface_ipv6;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =
        "interface * ipv6 address autoconfig privacy";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Use privacy extensions for address autoconfiguration");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =
        "/net/interface/config/$1$/addr/ipv6/slaac/privacy";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface_ipv6;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =
        "no interface * ipv6 address autoconfig privacy";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Disable privacy extensions for address autoconfiguration");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =
        "/net/interface/config/$1$/addr/ipv6/slaac/privacy";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface_ipv6;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * ipv6 address *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_flags |=                ccf_ignore_length;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_magic =                 cid_interface_ipv6_addr;
    cmd->cc_help_exp =
        N_("<IPv6 addr>/<len>");
    cmd->cc_help_exp_hint =
        N_("e.g. 2001:db8:1234::5678/64");
    cmd->cc_exec_callback =         cli_interface_ipv6_addr;
    cmd->cc_revmap_names =       "/net/interface/config/*/addr/ipv6/static/*";
    cmd->cc_revmap_order =          cro_interface_ipv6;
    cmd->cc_revmap_callback =       cli_interface_ipv6_addr_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * ipv6 address *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_flags |=                ccf_ignore_length;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cli_acl_opers_exec(cmd,         tao_set_delete);
    cmd->cc_magic =                 cid_interface_ipv6_addr_delete;
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_callback =         cli_interface_ipv6_addr_completion;
    cmd->cc_help_exp =
        N_("<IPv6 addr>/<len>");
    cmd->cc_help_exp_hint =
        N_("e.g. 2001:db8:1234::5678/64");
    cmd->cc_exec_callback =         cli_interface_ipv6_addr;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va
        (1,
         "/net/interface/config/*/addr/ipv6/static/*");
    bail_error(err);

#else

    /*
     * The mgmtd bindings are not conditional on
     * PROD_FEATURE_IPV6, so we have to do this to avoid getting
     * warnings on reverse-mapping.
     */
    err = cli_revmap_ignore_bindings_va
        (1, "/net/interface/config/*/addr/ipv6/**");
    bail_error(err);

#endif /* PROD_FEATURE_IPV6 */


#ifdef PROD_FEATURE_BRIDGING
    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * bridge-group";
    cmd->cc_help_descr =
        N_("Add this interface to a bridge group");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * bridge-group *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =
        N_("<bridge group>");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_names;
    cmd->cc_comp_pattern =          "/net/bridge/config/*";
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =        
        "/net/bridge/config/$2$/interface/$1$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * bridge-group";
    cmd->cc_help_descr =
        N_("Remove this interface from a bridge group");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * bridge-group *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cli_acl_opers_exec(cmd,         tao_set_delete); 
    cmd->cc_help_exp =
        N_("<Bridge group>");
    cmd->cc_help_term =
        N_("Remove this interface from the bridge group");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_names;
    cmd->cc_exec_operation =        cxo_delete;
    cmd->cc_comp_pattern =          
        "/net/bridge/config/*/interface/$1$";
    cmd->cc_exec_name =             "/net/bridge/config/$2$/interface/$1$";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * bridge-group * path-cost";
    cmd->cc_help_descr =
        N_("Configure the path cost for this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * bridge-group * path-cost *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =
        N_("<path cost>");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =        
        "/net/bridge/config/$2$/interface/$1$/path_cost";
    cmd->cc_exec_type =             bt_int32;
    cmd->cc_exec_value =            "$3$";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * bridge-group * path-cost";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Clear path cost configuration for this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             
        "/net/bridge/config/$2$/interface/$1$/path_cost";
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_callback =         cli_interface_check_exec;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * bridge-group * priority";
    cmd->cc_help_descr =
        N_("Configure the priority for this interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * bridge-group * priority *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =
        N_("<priority>");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =        
        "/net/bridge/config/$2$/interface/$1$/priority";
    cmd->cc_exec_type =             bt_int32;
    cmd->cc_exec_value =            "$3$";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * bridge-group * priority";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Clear priority configuration for this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             
        "/net/bridge/config/$2$/interface/$1$/priority";
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_callback =         cli_interface_check_exec;
    CLI_CMD_REGISTER;
#endif /* PROD_FEATURE_BRIDGING */

#ifdef PROD_FEATURE_BONDING
    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * bond";
    cmd->cc_help_descr =
        N_("Add this interface to a bonded interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * bond *";
    cmd->cc_magic =                 cid_intf_bonding_add;
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =
        N_("<bonded interface>");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_values;
    cmd->cc_comp_pattern =          "/net/bonding/config/*/name";
    cmd->cc_exec_callback =         cli_interface_bonding;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * bond";
    cmd->cc_help_descr =
        N_("Remove this interface from a bonded interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * bond *";
    cmd->cc_magic =                 cid_intf_bonding_delete;
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cli_acl_opers_exec(cmd,         tao_set_delete); 
    cmd->cc_help_exp =
        N_("<bonded interface>");
    cmd->cc_help_term =
        N_("Remove this interface from the bonded interface");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_values;
    cmd->cc_comp_pattern =          "/net/bonding/config/*/name";
    cmd->cc_exec_callback =         cli_interface_bonding;
    CLI_CMD_REGISTER;
#endif /* PROD_FEATURE_BONDING */

    CLI_CMD_NEW;
    cmd->cc_words_str =             "interface * shutdown";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =
        N_("Disable this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/net/interface/config/$1$/enable";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no interface * shutdown";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_descr =            
        N_("Enable this interface");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/net/interface/config/$1$/enable";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_callback =         cli_interface_check_exec;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_interface;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show interfaces";
    cmd->cc_magic =                 cid_interface_show;
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_flags |=                ccf_acl_inheritable;
    cli_acl_add(cmd,                tacl_sbm_net);
    cli_acl_opers_exec(cmd,         tao_query);
    cli_acl_set_modes(cmd,          cms_enable);
    cmd->cc_capab_required =        ccp_query_rstr_curr;
    cmd->cc_help_descr =
        N_("Display detailed running state for all interfaces");
    cmd->cc_help_term =
        N_("Display detailed running state for all interfaces");
    cmd->cc_exec_callback =         cli_interface_show;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show interfaces *";
    cmd->cc_magic =                 cid_interface_show;
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_query_rstr_curr;
    cmd->cc_help_exp =
        N_("<interface name>");
    cmd->cc_help_term =           
       N_("Display detailed running state for this interface");
    cmd->cc_help_callback =         cli_interface_help;
    cmd->cc_comp_callback =         cli_interface_completion;
    cmd->cc_exec_callback =         cli_interface_show;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show interfaces configured";
    cmd->cc_magic =                 cid_interface_show_conf;
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_query_rstr_curr;
    cmd->cc_help_descr =
        N_("Display configuration for all interfaces");
    cmd->cc_exec_callback =         cli_interface_show;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show interfaces brief";
    cmd->cc_magic =                 cid_interface_show_brief;
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_query_rstr_curr;
    cmd->cc_help_descr =
        N_("Display brief running state for all interfaces");
    cmd->cc_exec_callback =         cli_interface_show;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show interfaces * configured";
    cmd->cc_magic =                 cid_interface_show_conf;
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_query_rstr_curr;
    cmd->cc_help_descr =
        N_("Display configuration for this interface");
    cmd->cc_exec_callback =         cli_interface_show;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show interfaces * brief";
    cmd->cc_magic =                 cid_interface_show_brief;
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_query_rstr_curr;
    cmd->cc_help_descr =
        N_("Display brief running state for this interface");
    cmd->cc_exec_callback =         cli_interface_show;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va
        (3,
         "/net/interface/config/*/addr/ipv4/static/*",
         "/net/interface/config/*/enslaved",
         "/net/interface/config/*/alias/*");
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
int
cli_interface_get_if_list(tstr_array **ret_interfaces, tbool hide_display)
{
    int err = 0;
    tstr_array *ifs = NULL, *del_ifs = NULL;
    tstr_array_options opts;
    tstr_array *ifs_config = NULL, *ifs_state = NULL;
    uint32 i = 0, num_ifs = 0;
    char *bn_name = NULL;
    bn_binding *display_binding = NULL;
    tbool display = false;
    const char *intf = NULL;

    bail_null(ret_interfaces);
    *ret_interfaces = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&ifs, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array
        (cli_mcc,
         NULL, NULL, &ifs_config, "/net/interface/config", NULL);
    bail_error_null(err, ifs_config);

    err = mdc_get_binding_children_tstr_array
        (cli_mcc,
         NULL, NULL, &ifs_state, "/net/interface/state", NULL);
    bail_error_null(err, ifs_state);

    /*
     * We now have two lists of interfaces that probably overlap.
     * But the array we're throwing them into is set up to discard
     * duplicates.
     */

    err = tstr_array_concat(ifs, &ifs_config);
    bail_error(err);

    err = tstr_array_concat(ifs, &ifs_state);
    bail_error(err);

    if (hide_display) {
        err = tstr_array_new(&del_ifs, NULL);
        bail_error(err);
        
        num_ifs = tstr_array_length_quick(ifs);
        for (i = 0; i < num_ifs; i++) {
            intf = tstr_array_get_str_quick(ifs, i);
            
            bn_name =
                smprintf("/net/interface/config/%s/display", intf);
            bail_null(bn_name);
            
            /* If there is explicit config for the interface, don't
             * hide it.
             */
            display = true;

            err = mdc_get_binding(cli_mcc, NULL, NULL, false, &display_binding,
                                  bn_name, NULL);
            bail_error_msg(err, "Failed to get IF display configuration");
            
            if (display_binding) {
                err = bn_binding_get_tbool(display_binding, ba_value, NULL,
                                           &display);
                bail_error(err);
            }
            
            if (!display) {
                err = tstr_array_append_str(del_ifs, intf);
                bail_error(err);
            }
            
            safe_free(bn_name);
            bn_binding_free(&display_binding);
        }
        
        num_ifs = tstr_array_length_quick(del_ifs);
        for (i = 0; i < num_ifs; i++) {
            intf = tstr_array_get_str_quick(del_ifs, i);
            
            err = tstr_array_delete_str(ifs, intf);
            bail_error(err);
        }
    }
        
    *ret_interfaces = ifs;
    ifs = NULL;

 bail:
    tstr_array_free(&ifs);
    tstr_array_free(&del_ifs);
    tstr_array_free(&ifs_config);
    tstr_array_free(&ifs_state);
    safe_free(bn_name);
    bn_binding_free(&display_binding);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
cli_interface_validate(const char *ifname, tbool *ret_valid)
{
    int err = 0;
    tstr_array *ifs = NULL;
    uint32 idx = 0;

    bail_null(ret_valid);
    *ret_valid = false;

    err = cli_interface_get_if_list(&ifs, false);
    bail_error(err);

    err = tstr_array_linear_search_str(ifs, ifname, 0, &idx);
    if (err != lc_err_not_found) {
        bail_error(err);
        *ret_valid = true;
    }
    else {
        err = cli_printf_error(_("Unknown interface %s\n"), ifname);
        bail_error(err);
    }

 bail:
    tstr_array_free(&ifs);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
cli_interface_help(void *data, cli_help_type help_type, 
                   const cli_command *cmd, const tstr_array *cmd_line,
                   const tstr_array *params, const tstring *curr_word,
                   void *context)
{
    int err = 0;
    tstr_array *ifnames = NULL;
    const char *ifname = NULL;
    tstring *comment = NULL;
    int i = 0, num_names = 0;

    switch(help_type) {

    case cht_termination:
        if (cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
            err = cli_add_termination_help
                (context, GT_(cmd->cc_help_term_prefix,
                              cmd->cc_gettext_domain));
            bail_error(err);
        }
        else {
            err = cli_add_termination_help
                (context, GT_(cmd->cc_help_term, cmd->cc_gettext_domain));
            bail_error(err);
        }
        break;

    case cht_expansion:
        if (cmd->cc_help_exp) {
            err = cli_add_expansion_help
                (context, GT_(cmd->cc_help_exp, cmd->cc_gettext_domain),
                 GT_(cmd->cc_help_exp_hint, cmd->cc_gettext_domain));
            bail_error(err);
        }

        err = tstr_array_new(&ifnames, NULL);
        bail_error(err);
        
        err = cli_interface_completion(data, cmd, cmd_line, params, curr_word,
                                       ifnames);
        bail_error(err);
        
        num_names = tstr_array_length_quick(ifnames);
        for (i = 0; i < num_names; ++i) {
            ifname = tstr_array_get_str_quick(ifnames, i);
            bail_null(ifname);
            ts_free(&comment);
            err = mdc_get_binding_tstr_fmt
                (cli_mcc, NULL, NULL, NULL, &comment, NULL,
                 "/net/interface/config/%s/comment", ifname);
            bail_error(err);
            err = cli_add_expansion_help(context, ifname, 
                                         ts_str_maybe(comment));
            bail_error(err);
        }
        break;

    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

 bail:
    tstr_array_free(&ifnames);
    ts_free(&comment);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
cli_interface_completion(void *data, const cli_command *cmd,
                         const tstr_array *cmd_line,
                         const tstr_array *params,
                         const tstring *curr_word,
                         tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *ifs = NULL;

    err = cli_interface_get_if_list(&ifs, true);
    bail_error(err);

    err = tstr_array_concat(ret_completions, &ifs);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);

 bail:
    tstr_array_free(&ifs);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
cli_interface_enter_mode(void *data, const cli_command *cmd,
                         const tstr_array *cmd_line,
                         const tstr_array *params)
{
    int err = 0;
    tbool valid = false;

    if (cli_prefix_modes_is_enabled()) {
        err = cli_interface_validate(tstr_array_get_str_quick(params, 0),
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
    return(err);
}


/* ------------------------------------------------------------------------- */
int
cli_interface_speed_help(void *data, cli_help_type help_type, 
                         const cli_command *cmd,
                         const tstr_array *cmd_line,
                         const tstr_array *params,
                         const tstring *curr_word,
                         void *context)
{
    int err = 0;
    const char *ifname = NULL;
    char *bname = NULL;
    tstr_array *speeds = NULL;
    const char *speed = NULL;
    int i = 0, num_speeds = 0;
    char *speed_descr = NULL;

    if (help_type != cht_expansion) {
        goto bail;
    }

    err = cli_add_expansion_help(context, _("<speed>"), NULL);
    bail_error(err);

    ifname = tstr_array_get_str_quick(params, 0);
    bail_null(ifname);

    bname = smprintf("/net/interface/state/%s/type/ethernet/supported/speeds",
                     ifname);
    bail_null(bname);

    err = mdc_get_binding_children_tstr_array
        (cli_mcc, NULL, NULL, &speeds, bname, NULL);
    bail_error(err);

    /*
     * If we didn't get any results back, we don't know what the
     * options are, so just offer everything.
     */
    if (speeds == NULL || tstr_array_length_quick(speeds) == 0) {
        err = cli_std_help_options(data, help_type, cmd, cmd_line, params,
                                   curr_word, context);
        bail_error(err);
    }

    num_speeds = tstr_array_length_quick(speeds);
    for (i = 0; i < num_speeds; ++i) {
        speed = tstr_array_get_str_quick(speeds, i);
        bail_null(speed);
        if (lc_is_prefix(ts_str(curr_word), speed, false)) {
            safe_free(speed_descr);
            speed_descr = smprintf(_("%s Mbit/sec"), speed);
            bail_null(speed_descr);
            err = cli_add_expansion_help(context, speed, speed_descr);
            bail_error(err);
        }
    }

    if (lc_is_prefix(ts_str(curr_word), "auto", false)) {
        err = cli_add_expansion_help(context, "auto",
                                     _("Automatically detect speed"));
        bail_error(err);
    }

 bail:
    safe_free(bname);
    tstr_array_free(&speeds); /* Only in error case */
    safe_free(speed_descr);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
cli_interface_speed_completion(void *data, const cli_command *cmd,
                               const tstr_array *cmd_line,
                               const tstr_array *params,
                               const tstring *curr_word,
                               tstr_array *ret_completions)
{
    int err = 0;
    const char *ifname = NULL;
    char *bname = NULL;
    tstr_array *speeds = NULL;

    ifname = tstr_array_get_str_quick(params, 0);
    bail_null(ifname);

    bname = smprintf("/net/interface/state/%s/type/ethernet/supported/speeds",
                     ifname);
    bail_null(bname);

    err = mdc_get_binding_children_tstr_array
        (cli_mcc, NULL, NULL, &speeds, bname, NULL);
    bail_error(err);

    /*
     * If we didn't get any results back, we don't know what the
     * options are, so just offer everything.
     */
    if (speeds == NULL || tstr_array_length_quick(speeds) == 0) {
        err = cli_std_comp_options(data, cmd, cmd_line, params, curr_word,
                                   ret_completions);
        bail_error(err);
        goto bail;
    }

    /*
     * Take advantage of the infrastructure screening these for having
     * the right prefix: easier for us this way.
     */

    err = tstr_array_concat(ret_completions, &speeds);
    bail_error(err);

    err = tstr_array_append_str(ret_completions, "auto");
    bail_error(err);

 bail:
    safe_free(bname);
    tstr_array_free(&speeds); /* Only in error case */
    return(err);
}


/* ------------------------------------------------------------------------- */
int
cli_interface_check_exec(void *data, const cli_command *cmd,
                         const tstr_array *cmd_line,
                         const tstr_array *params)
{
    int err = 0;
    const char *ifname = NULL;
    tbool valid = false;

    ifname = tstr_array_get_str_quick(params, 0);
    bail_null(ifname);

    err = cli_interface_validate(ifname, &valid);
    bail_error(err);

    if (valid) {
        err = cli_std_exec(data, cmd, cmd_line, params);
        bail_error(err);
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
int 
cli_interface_ip_addr(void *data, const cli_command *cmd,
                      const tstr_array *cmd_line,
                      const tstr_array *params)
{
    int err = 0;
    const char *itf = NULL;
    const char *addr = NULL;
    const char *alias = NULL;
    const char *mask = NULL;
    char *mask_len = NULL;
    bn_request *req = NULL;
    tbool valid = false;
    char *ifname = NULL;
    tbool is_alias = false;

    itf = tstr_array_get_str_quick(params, 0);
    bail_null(itf);

    if (cmd->cc_magic == cid_interface_alias) {
        alias = tstr_array_get_str_quick(params, 1);
        bail_null(alias);
        addr = tstr_array_get_str_quick(params, 2);
        bail_null(addr);
        mask = tstr_array_get_str_quick(params, 3);
        bail_null(mask);
        is_alias = true;
    }
    else {
        addr = tstr_array_get_str_quick(params, 1);
        bail_null(addr);
        mask = tstr_array_get_str_quick(params, 2);
        bail_null(mask);
    }

    err = cli_interface_validate(itf, &valid);
    bail_error(err);

    if (!valid) {
        goto bail;
    }

    if (alias) {
        ifname = smprintf("%s:%s", itf, alias);
    }
    else {
        ifname = strdup(itf);
    }
    bail_null(ifname);

    err = lia_ipv4addr_maskspec_to_masklen_str(mask, &mask_len);
    if (err == lc_err_bad_type) {
        err = cli_printf_error("%s", cli_error_netmask);
        bail_error(err);
        goto bail;
    }
    bail_error(err);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt
        (req, bsso_modify, 0, bt_ipv4addr, 0, addr, NULL,
         "/net/interface/config/%s/addr/ipv4/static/1/ip", ifname);
    bail_error(err);

    err = bn_set_request_msg_append_new_binding_fmt
        (req, bsso_modify, 0, bt_uint8, 0, mask_len, NULL,
         "/net/interface/config/%s/addr/ipv4/static/1/mask_len", ifname);
    bail_error(err);

    /*
     * Though 'is_alias' is a boolean, there are actually three cases:
     *
     *   1. Using "interface * alias * ip address * *" to set an address
     *      on an alias (is_alias == true).
     *
     *   2. Using "interface * ip address * *" to set an address 
     *      on a non-alias interface (is_alias == false).
     *
     *   3. Using "interface * ip address * *" to set an address 
     *      on an alias, by constructing the name manually, e.g.
     *      "ether1:7" (is_alias == false).  This case is the subject
     *      of bug 14597.  We have no official way of detecting this,
     *      but since alias name construction seems to be done 
     *      informally with smprintf() et al. elsewhere, we'll just
     *      follow suit and search for a ':'.  Frankly we probably
     *      don't need to set these at all unless is_alias is true,
     *      since they should never be getting set for a non-alias
     *      interface -- but we'll just leave that alone for now.
     */
    if (!is_alias && strchr(ifname, ':') != NULL) {
        /*
         * Case 3 -- leave these two nodes alone.  In general they will
         * already have been set before.  If the user has done something
         * like "interface ether1:7 create" before, they may not be
         * populated correctly, see bug 11523, but that is not our
         * concern.
         */
    }
    else {
        err = bn_set_request_msg_append_new_binding_fmt
            (req, bsso_modify, 0, bt_string, 0, is_alias ? itf : "", NULL,
             "/net/interface/config/%s/alias/ifdevname", ifname);
        bail_error(err);
        
        err = bn_set_request_msg_append_new_binding_fmt
            (req, bsso_modify, 0, bt_string, 0, is_alias ? alias : "", NULL,
             "/net/interface/config/%s/alias/index", ifname);
        bail_error(err);
    }

    err = mdc_send_mgmt_msg(cli_mcc,
                            req, false, NULL, NULL, NULL);
    bail_error(err);

 bail:
    safe_free(ifname);
    safe_free(mask_len);
    bn_request_msg_free(&req);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
cli_interface_ipv6_addr(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params)
{
    int err = 0;
    const char *ifname = NULL;
    const char *masklen = NULL;
    const char *slash = NULL;
    const char *addrmasklen = NULL;
    char *addr = NULL;
    char *root_bname = NULL;
    bn_request *req = NULL;
    tbool valid = false;
    tbool is_alias = false;
    bn_binding *binding = NULL;
    bn_binding_array *children = NULL;
    tbool dup_deleted = false;
    uint32 num_deleted = 0;

    ifname = tstr_array_get_str_quick(params, 0);
    bail_null(ifname);

    err = cli_interface_validate(ifname, &valid);
    bail_error(err);
    if (!valid) {
        goto bail;
    }

    /*
     * If they're adding or removing a single address, make up an
     * array of child bindings expressing which address that is.
     */
    if (cmd->cc_magic != cid_interface_ipv6_addr_delete_all) {
        addrmasklen = tstr_array_get_str_quick(params, 1);
        bail_null(addrmasklen);
        if (!(slash = strchr(addrmasklen, '/'))) {
            err = cli_printf_error(cli_error_ipv6addr_masklen);
            bail_error(err);
            goto bail;
        }

        masklen = slash + 1;
        addr = strldup(addrmasklen, (slash - addrmasklen + 1));
        bail_null(addr);

        err = bn_binding_array_new(&children);
        bail_error(err);

        err = bn_binding_new_str_autoinval(&binding, "ip", ba_value, 
                                           bt_ipv6addr, 0, addr);
        bail_error(err);
        err = bn_binding_array_append_takeover(children, &binding);
        bail_error(err);

        err = bn_binding_new_str_autoinval(&binding, "mask_len", ba_value, 
                                           bt_uint8, 0, masklen);
        bail_error(err);
        err = bn_binding_array_append_takeover(children, &binding);
        bail_error(err);
    }

    root_bname = smprintf("/net/interface/config/%s/addr/ipv6/static",
                          ifname);
    bail_null(root_bname);

    switch (cmd->cc_magic) {

    case cid_interface_ipv6_addr:
        err = mdc_array_append(cli_mcc, root_bname, children, true, 0, NULL,
                               &dup_deleted, NULL, NULL);
        bail_error(err);
        if (dup_deleted) {
            /*
             * Could print an error, but unless the whining switch 
             * per bug 10477 is enabled, we probably shouldn't.
             */
        }
        break;

    case cid_interface_ipv6_addr_delete:
        err = mdc_array_delete_by_value(cli_mcc, root_bname, true, children,
                                        0, NULL, &num_deleted, NULL, NULL);
        bail_error(err);
        if (num_deleted == 0) {
            /*
             * Could print an error, but unless the whining switch 
             * per bug 10477 is enabled, we probably shouldn't.
             */
        }
        break;

    case cid_interface_ipv6_addr_delete_all:
        err = mdc_delete_binding_children(cli_mcc, NULL, NULL, root_bname);
        bail_error(err);
        break;

    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

 bail:
    safe_free(addr);
    safe_free(root_bname);
    bn_request_msg_free(&req);
    bn_binding_free(&binding);
    bn_binding_array_free(&children);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
cli_interface_addr_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings,
                          const char *name,
                          const tstr_array *name_parts,
                          const char *value, const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err = 0;
    char *bname_ip = NULL, *bname_masklen = NULL;
    char *bname_alias_ifdevname = NULL, *bname_alias_index = NULL;
    tstring *ip = NULL, *masklen = NULL;
    const bn_binding *alias_binding = NULL;
    char *alias_ifdevname = NULL, *alias_index = NULL;
    char *esc_name = NULL;

    bname_ip = smprintf("%s/ip", name);
    bail_null(bname_ip);

    bname_masklen = smprintf("%s/mask_len", name);
    bail_null(bname_masklen);

    err = bn_binding_array_get_value_tstr_by_name
        (bindings, bname_ip, NULL, &ip);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name
        (bindings, bname_masklen, NULL, &masklen);
    bail_error(err);

    bname_alias_ifdevname = smprintf
        ("/net/interface/config/%s/alias/ifdevname",
         tstr_array_get_str_quick(name_parts, 3));
    bail_null(bname_alias_ifdevname);

    err = bn_binding_array_get_binding_by_name(bindings, bname_alias_ifdevname,
                                               &alias_binding);
    bail_error(err);

    if (alias_binding) {
        err = bn_binding_get_str(alias_binding, ba_value, bt_string, NULL,
                                 &alias_ifdevname);
        bail_error(err);
    }

    bname_alias_index = smprintf("/net/interface/config/%s/alias/index",
                                 tstr_array_get_str_quick(name_parts, 3));
    bail_null(bname_alias_index);

    err = bn_binding_array_get_binding_by_name(bindings, bname_alias_index,
                                               &alias_binding);
    bail_error(err);

    if (alias_binding) {
        err = bn_binding_get_str(alias_binding, ba_value, bt_string, NULL,
                                 &alias_index);
        bail_error(err);
    }

    err = tstr_array_append_str(ret_reply->crr_nodes, bname_ip);
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_nodes, bname_masklen);
    bail_error(err);

    if (alias_ifdevname && alias_index && (strlen(alias_ifdevname) > 0) &&
        (strlen(alias_index) > 0)) {
        err = cli_escape_str(alias_ifdevname, &esc_name);
        bail_error(err);
        err = tstr_array_append_sprintf
            (ret_reply->crr_cmds,
             "interface %s alias %s ip address %s /%s",
             esc_name, alias_index, ts_str(ip), ts_str(masklen));
        bail_error(err);
    }
    else {
        err = cli_escape_str(tstr_array_get_str_quick(name_parts, 3),
                             &esc_name);
        bail_error(err);
        err = tstr_array_append_sprintf
            (ret_reply->crr_cmds, 
             "interface %s ip address %s /%s",
             esc_name, ts_str(ip), ts_str(masklen));
        bail_error(err);
    }

 bail:
    safe_free(bname_ip);
    safe_free(bname_masklen);
    safe_free(bname_alias_ifdevname);
    safe_free(bname_alias_index);
    safe_free(alias_ifdevname);
    safe_free(alias_index);
    ts_free(&ip);
    ts_free(&masklen);
    safe_free(esc_name);
    return(err);
}


static int
cli_interface_ipv6_addr_completion(void *data, const cli_command *cmd,
                                   const tstr_array *cmd_line,
                                   const tstr_array *params,
                                   const tstring *curr_word,
                                   tstr_array *ret_completions)
{
    int err = 0;
    const char *ifname = NULL;
    char *bname = NULL;
    tstr_array *addr_ids = NULL;
    char *ncs = NULL;
    uint32 num_addrs = 0, addr_idx = 0;
    const char *addr_num = NULL;
    tstring *ip_str = NULL, *masklen_str = NULL;

    ifname = tstr_array_get_str_quick(params, 0);
    bail_null(ifname);

    bname = smprintf("/net/interface/config/%s/addr/ipv6/static",
                          ifname);
    bail_null(bname);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
                                   NULL, NULL, &addr_ids,
                                   bname, NULL);
    bail_error(err);

    num_addrs = tstr_array_length_quick(addr_ids);
    for (addr_idx = 0; addr_idx < num_addrs; addr_idx++) {
        addr_num = tstr_array_get_str_quick(addr_ids, addr_idx);
        bail_null(addr_num);

        ts_free(&ip_str);
        err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
                                       &ip_str, NULL,
                                       "%s/%s/ip",
                                       bname, addr_num);
        bail_error(err);

        ts_free(&masklen_str);
        err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL,
                                       &masklen_str, NULL,
                                       "%s/%s/mask_len",
                                       bname, addr_num);
        bail_error(err);

        if (ts_str_maybe(ip_str) && ts_str_maybe(masklen_str)) {
            safe_free(ncs);
            ncs = smprintf("%s/%s", ts_str(ip_str), ts_str(masklen_str));
            bail_null(ncs);

            err = tstr_array_append_str_takeover(ret_completions, &ncs);
            bail_error(err);
        }
    }

 bail:
    tstr_array_free(&addr_ids);
    safe_free(ncs);
    safe_free(bname);
    return(err);
}

static int
cli_interface_ipv6_addr_revmap(void *data, const cli_command *cmd,
                               const bn_binding_array *bindings,
                               const char *name,
                               const tstr_array *name_parts,
                               const char *value, const bn_binding *binding,
                               cli_revmap_reply *ret_reply)
{
    int err = 0;
    char *bname_ip = NULL, *bname_masklen = NULL;
    tstring *ip = NULL, *masklen = NULL;
    char *esc_name = NULL;

    bname_ip = smprintf("%s/ip", name);
    bail_null(bname_ip);

    bname_masklen = smprintf("%s/mask_len", name);
    bail_null(bname_masklen);

    err = bn_binding_array_get_value_tstr_by_name
        (bindings, bname_ip, NULL, &ip);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name
        (bindings, bname_masklen, NULL, &masklen);
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_nodes, bname_ip);
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_nodes, bname_masklen);
    bail_error(err);

    err = cli_escape_str(tstr_array_get_str_quick(name_parts, 3),
                         &esc_name);
    bail_error(err);
    err = tstr_array_append_sprintf
        (ret_reply->crr_cmds,
         "interface %s ipv6 address %s/%s",
         esc_name, ts_str(ip), ts_str(masklen));
    bail_error(err);

 bail:
    safe_free(bname_ip);
    safe_free(bname_masklen);
    ts_free(&ip);
    ts_free(&masklen);
    safe_free(esc_name);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
cli_interface_get_source_one(const bn_binding_array *bindings,
                             uint32 idx, const bn_binding *binding,
                             const tstring *name,
                             const tstr_array *name_components,
                             const tstring *name_last_part,
                             const tstring *value, void *callback_data)
{
    int err = 0;
    tstring *source = callback_data;
    const char *vm_name = NULL, *ifnum_str = NULL;

    if (ts_equal(source, value)) {
        vm_name = tstr_array_get_str_quick(name_components, 3);
        bail_null(vm_name);
        ifnum_str = tstr_array_get_str_quick(name_components, 5);
        bail_null(ifnum_str);
        err = ts_clear(source);
        bail_error(err);
        err = ts_append_sprintf(source, _(" (VM '%s', interface %s)"),
                                vm_name, ifnum_str);
        bail_error(err);
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
cli_interface_get_source(const char *ifname, tstring **ret_source)
{
    int err = 0;
    tstring *source = NULL;

    bail_null(ifname);
    bail_null(ret_source);

    err = ts_new(&source);
    bail_error(err);

    if (!lc_is_prefix(md_virt_ifname_prefix, ifname, false)) {
        goto bail;
    }

    /* A hack, but easier than having a context structure */
    err = ts_append_str(source, ifname);
    bail_error(err);

    /* XXX/EMT: PERF */
    err = mdc_foreach_binding(cli_mcc, "/virt/state/vm/*/interface/*/name",
                              NULL, cli_interface_get_source_one, source,
                              NULL);
    bail_error(err);

    if (ts_equal_str(source, ifname, false)) {
        err = ts_clear(source);
        bail_error(err);
    }

 bail:
    if (ret_source) {
        *ret_source = source;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
int
cli_interface_show_one(const bn_binding_array *bindings, uint32 idx,
                       const bn_binding *binding, const tstring *name,
                       const tstr_array *name_components,
                       const tstring *name_last_part,
                       const tstring *value,
                       void *callback_data)
{
    int err = 0;
    const char *r = NULL;
    const char *ifname = NULL;
    tstring *masklen_tstr = NULL;
    char *mask_len = NULL;
    char *netmask = NULL;
    uint32 show_type = 0;
    const bn_binding *addr_binding = NULL;
    uint32 num_addr_bindings = 0, i = 0;
    const char *addr_str = NULL;
    const tstring *addrroot_bn = NULL;
    const tstring *ifdevname = NULL;
    char *amask_bn = NULL, *aifname_bn = NULL;
    tstring *amask = NULL, *aifname = NULL;
    char *status_bn = NULL;
    tstring *status = NULL;
    tstr_array *name_parts_var = NULL;
    const tstr_array *name_parts = NULL;
    cli_interface_show_context *ctxt = NULL;
    bn_binding *display_binding = NULL;
    tbool display = true, has_display = false;
    char *bn_name = NULL;
    tstring *alias_ifdevname = NULL;
    tbool is_alias = false;
    tstring *devsource = NULL, *hwaddr = NULL;
    tstring *bonding_master = NULL, *bridge_group = NULL;
    const bn_binding_array *config_bindings = NULL;
    char *addr_subtree = NULL;
    tbool found = false;
    void *alias_list_v = NULL;
    const tstring *alias_list = NULL;
    tstring *intf_source = NULL;
    uint32 num_addrs = 0;
    tstr_array *static_ids = NULL;
    const char *static_id = NULL;
    char *ipv6_static_wc_name = NULL, *ipv6_static = NULL;
    tstring *ipv6_enabled_str = NULL;
    tbool ipv6_enabled = false;
    int addr_pass = 0;
    tbool addr_pass_did_header = false;
    uint32 num_ipv6_addrs = 0;
    tbool dhcp_running = false, dhcp_lease = false;
    tbool dhcpv6_running = false, dhcpv6_lease = false;
    const char *dhcp_summary = NULL, *dhcpv6_summary = NULL;

    bail_null(callback_data);
    ctxt = callback_data;
    show_type = ctxt->cisc_magic;

    config_bindings = (show_type == cid_interface_show_conf ? bindings : 
                       ctxt->cisc_config_bindings);

    /* ........................................................................
     * Check if we're supposed to display the interface at all.
     */
    if (ctxt->cisc_show_all) {
        bn_name = smprintf("/net/interface/config/%s/display",
                           ts_str(name_last_part));
        bail_null(bn_name);

        err = bn_binding_array_get_value_tbool_by_name
            (config_bindings, bn_name, &has_display, &display);
        bail_error_msg(err, "Failed to get IF display configuration");

        if (!display) {
            goto bail;
        }

        /*
         * If has_display is false, that usually means the interface
         * does not have any configuration.  If we're showing
         * interface state, that is fine, we want to show it anyway.
         * If we're showing interface configuration, something's
         * wrong, since we were iterating only over interfaces that
         * had configuration...
         */
        if (!has_display && show_type == cid_interface_show_conf) {
            lc_log_basic(LOG_WARNING, I_("Could not find node %s"),
                         bn_name);
            goto bail;
        }
    }

    /* ........................................................................
     * Print the header first, before any graft points.
     */
    if (idx != 0) {
        err = cli_printf("\n");
        bail_error(err);
    }
    ifname = ts_str(name_last_part);
    bail_null(ifname);
    if (show_type == cid_interface_show_conf) {
        err = cli_printf(_("Interface %s configuration:\n"), ifname);
        bail_error(err);
    }
    else {
        err = cli_printf(_("Interface %s status:\n"), ifname);
        bail_error(err);
    }
    err = cli_printf_prequeried
        (config_bindings,
         _("   Comment:            #/net/interface/config/%s/comment#\n"),
         ifname);
    bail_error(err);

/* ========================================================================= */
/* Customer-specific graft point 3: print status or configuration
 * output before all of ours.
 *
 * This runs for each interface, after we have printed the header line,
 * but before we have printed any actual configuration or state
 * (besides the comment, which is part of the header).
 *
 * Please note that 'show_type' is set to one of the following, to reflect
 * which command got us here:
 *     - cid_interface_show
 *     - cid_interface_show_brief
 *     - cid_interface_show_conf
 *
 * Note also that everything you print should be indented by three spaces
 * to match the rest of the display.
 * =========================================================================
 */
#ifdef INC_CLI_INTERFACE_CMDS_INC_GRAFT_POINT
#undef CLI_INTERFACE_CMDS_INC_GRAFT_POINT
#define CLI_INTERFACE_CMDS_INC_GRAFT_POINT 3
#include "../bin/cli/modules/cli_interface_cmds.inc.c"
#endif /* INC_CLI_INTERFACE_CMDS_INC_GRAFT_POINT */

    /* ........................................................................
     * Gather some info we'll need later.
     */
    err = bn_binding_array_get_value_tstr_by_name_fmt
        (config_bindings, NULL, &alias_ifdevname, 
         "/net/interface/config/%s/alias/ifdevname",
         ifname);
    bail_error(err);

    if (alias_ifdevname && (ts_length(alias_ifdevname) > 0)) {
        is_alias = true;
    }

    r = ts_str(name);
    bail_null(r);

    if (show_type == cid_interface_show_conf) {
        err = bn_binding_array_get_value_tstr_by_name_fmt
            (bindings, NULL, &masklen_tstr, 
             "%s/addr/ipv4/static/1/mask_len", r);
        bail_error(err);
    }
    else {
        err = bn_binding_array_get_value_tstr_by_name_fmt
            (bindings, NULL, &masklen_tstr, 
             "%s/addr/ipv4/1/mask_len", r);
        bail_error(err);
    }

    if (masklen_tstr) {
        err = lia_masklen_str_to_ipv4addr_str(ts_str(masklen_tstr),
                                              &netmask);
        bail_error(err);
    }
    else {
        netmask = strdup("");
        bail_null(netmask);
    }

    /* ........................................................................
     * Display interface configuration.
     */
    if (show_type == cid_interface_show_conf) {
        err = cli_printf_prequeried
            (bindings,
             _("   Enabled:            #%s/enable#\n"), r);
        bail_error(err);

#ifdef PROD_FEATURE_DHCP_CLIENT
        err = cli_printf_prequeried
            (bindings,
             _("   DHCP:               #%s/addr/ipv4/dhcp#\n"), r);
        bail_error(err);
#endif /* PROD_FEATURE_DHCP_CLIENT */

#ifdef PROD_FEATURE_ZEROCONF
        err = cli_printf_prequeried
            (bindings,
             _("   Zeroconf:           #%s/addr/ipv4/zeroconf#\n"), r);
        bail_error(err);
#endif /* PROD_FEATURE_ZEROCONF */

        err = cli_printf_prequeried
            (bindings,
             _("   IP address:         #%s/addr/ipv4/static/1/ip#\n"
               "   Netmask:            %s\n"), r, netmask);
        bail_error(err);

        /* Display all IPv6 addresses configured on the interface */
#ifdef PROD_FEATURE_IPV6
        err = bn_binding_array_get_value_tbool_by_name_rel
            (bindings, r, "addr/ipv6/enable", &found, &ipv6_enabled);
        bail_error(err);

        err = cli_printf
            (_("   IPv6 enabled:       %s%s\n"),
             lc_bool_to_friendly_str(ipv6_enabled),
             (ipv6_enabled && !(ctxt->cisc_ipv6_enable_overall)) ?
             _(" (but disabled overall)") : "");
        bail_error(err);

        err = cli_printf_prequeried
            (bindings,
             _("   Autoconf enabled:   #%s/addr/ipv6/slaac/enable#\n"
               "   Autoconf route:     #%s/addr/ipv6/slaac/default_route#\n"
               "   Autoconf privacy:   #%s/addr/ipv6/slaac/privacy#\n"),
             r, r, r);
        bail_error(err);

#ifdef PROD_FEATURE_DHCP_CLIENT
        /*
         * This looks a little better above the autoconf stuff, but
         * we put it down here for now as it seems a bit less likely 
         * to be used.
         *
         * Also, note that the order here should be the same as the
         * ordering on the Web UI page.
         */
        err = cli_printf_prequeried
            (bindings,
             _("   DHCPv6 enabled:     #%s/addr/ipv6/dhcp/enable#\n"), r);
        bail_error(err);
#endif /* PROD_FEATURE_DHCP_CLIENT */

        /*
         * These are: /net/interface/config/ether2/addr/ipv6/static/1/ip
         */
        ipv6_static = smprintf("%s/addr/ipv6/static", r);
        bail_null(ipv6_static);

        ipv6_static_wc_name = smprintf("%s/addr/ipv6/static/*", r);
        bail_null(ipv6_static_wc_name);
        err = bn_binding_array_get_name_part_tstr_array(bindings,
                                                        ipv6_static_wc_name,
                                                        7,
                                                        &static_ids);
        bail_error(err);

        num_addrs = tstr_array_length_quick(static_ids);
        err = cli_printf(_("   IPv6 addresses:     %d\n"), num_addrs);
        bail_error(err);
        for (i = 0; i < num_addrs; i++) {
            static_id = tstr_array_get_str_quick(static_ids, i);
            bail_null(static_id);

            err = cli_printf_prequeried
                (bindings,
                 _("   IPv6 address:       #%s/%s/ip#/#%s/%s/mask_len#\n"),
                 ipv6_static, static_id,
                 ipv6_static, static_id);
            bail_error(err);
        }

#endif /* PROD_FEATURE_IPV6 */

        err = ht_lookup(ctxt->cisc_aliases, name_last_part,
                        ts_length(name_last_part), &found, NULL, NULL,
                        &alias_list_v);
        bail_error(err);
        alias_list = alias_list_v;
        
        if (alias_list && ts_num_chars(alias_list) > 0) {
            err = cli_printf
                (_("   Aliases:            %s\n"),
                 ts_str(alias_list));
            bail_error(err);
        }

        if (is_alias) {
            err = cli_printf_prequeried
                (bindings,
                 _("   Alias of:           #%s/alias/ifdevname#\n"), r);
            bail_error(err);
        }
        else {
            err = cli_printf_prequeried
                (bindings,
                 _("   Speed:              #%s/type/ethernet/speed#\n"
                   "   Duplex:             #%s/type/ethernet/duplex#\n"
                   "   MTU:                #%s/mtu#\n"),
                 r, r, r);
            bail_error(err);
        }
    }

    /* ........................................................................
     * Display interface runtime state
     */
    else {
        dhcp_running = false;
        err = bn_binding_array_get_value_tbool_by_name_rel
            (bindings, r, "dhcp/v4/running", NULL, &dhcp_running);
        bail_error(err);
        dhcp_lease = false;
        err = bn_binding_array_get_value_tbool_by_name_rel
            (bindings, r, "dhcp/v4/lease_active", NULL, &dhcp_lease);
        bail_error(err);
        if (!dhcp_running) {
            dhcp_summary = _("no");
        }
        else if (!dhcp_lease) {
            dhcp_summary = _("yes (but no valid lease)");
        }
        else {
            dhcp_summary = _("yes");
        }

        err = cli_printf_prequeried
            (bindings,
             _("   Admin up:           #%s/flags/oper_up#\n"
               "   Link up:            #%s/flags/link_up#\n"
               "   DHCP running:       %s\n"
               "   IP address:         #%s/addr/ipv4/1/ip#\n"
               "   Netmask:            %s\n"),
             r, r, dhcp_summary, r, netmask);

        /* /net/interface/address/state/ifdevname/vlan1249/ipv4addr */
        err = bn_binding_name_append_parts_va
            (&addr_subtree, "/net/interface/address/state/ifdevname", 
             2, ifname, "ipv4addr");
        bail_error_null(err, addr_subtree);

        num_addr_bindings =
            bn_binding_array_length_quick(ctxt->cisc_addr_bindings);
        for (i = 0; i < num_addr_bindings; ++i) {
            ts_free(&amask);
            safe_free(amask_bn);
            safe_free(aifname_bn);
            ts_free(&aifname);

            addr_binding = 
                bn_binding_array_get_quick(ctxt->cisc_addr_bindings, i);
            bail_null(addr_binding);

            err = bn_binding_get_name(addr_binding, &addrroot_bn);
            bail_error(err);

            /* 
             * We want to find the parent of the addr nodes, which are like:
             * /net/interface/address/state/ifdevname/vlan1249/ipv4addr/
             * 10.99.3.241 
             */
            if (!lc_is_prefix(addr_subtree, ts_str(addrroot_bn), false)) {
                continue;
            }

            tstr_array_free(&name_parts_var);
            name_parts = NULL;

            name_parts = bn_binding_get_name_parts_const_quick(addr_binding);
            if (!name_parts) {
                err = bn_binding_get_name_parts(addr_binding, &name_parts_var);
                bail_error(err);
                name_parts = name_parts_var;
            }
            if (tstr_array_length_quick(name_parts) != 8) {
                continue;
            }

            err = tstr_array_get(name_parts, 5, (tstring **)&ifdevname);
            bail_error(err);

            err = tstr_array_get_str(name_parts, 7, &addr_str);
            bail_error(err);

            if ((ts_num_chars(ifdevname) > 0) &&
                ts_equal(name_last_part, ifdevname)) {

                aifname_bn = smprintf("%s/ifname", ts_str(addrroot_bn));
                err = bn_binding_array_get_value_tstr_by_name
                    (ctxt->cisc_addr_bindings, aifname_bn, NULL,
                     &aifname);
                bail_error(err);

                /* Any address on the interface itself is not a secondary */
                if (!ts_str_maybe(aifname) || 
                    !safe_strcmp(ts_str(aifname), ts_str(ifdevname))) {
                    continue;
                }

                amask_bn = smprintf("%s/mask", ts_str(addrroot_bn));
                err = bn_binding_array_get_value_tstr_by_name
                    (ctxt->cisc_addr_bindings, amask_bn, NULL, &amask);
                bail_error(err);

                err = lia_ipv4addr_maskspec_to_masklen_str(ts_str(amask),
                                                           &mask_len);
                bail_error(err);

                err = cli_printf(_("   Secondary address:  %s/%s "
                                   "(alias: '%s')\n"),
                                 addr_str, mask_len, ts_str(aifname));
                bail_error(err);
            }
        }

        /* Display IPv6 settings info */

        ipv6_enabled = false;

#ifdef PROD_FEATURE_IPV6
        err = bn_binding_array_get_value_tstr_by_name_fmt
            (bindings,
             NULL, &ipv6_enabled_str,
             "%s/addr/settings/ipv6/enable", r);
        bail_error(err);
        if (ts_equal_str(ipv6_enabled_str, "true", false)) {
            ipv6_enabled = true;
        }

        dhcpv6_running = false;
        err = bn_binding_array_get_value_tbool_by_name_rel
            (bindings, r, "dhcp/v6/running", NULL, &dhcpv6_running);
        bail_error(err);
        dhcpv6_lease = false;
        err = bn_binding_array_get_value_tbool_by_name_rel
            (bindings, r, "dhcp/v6/lease_active", NULL, &dhcpv6_lease);
        bail_error(err);
        if (!dhcpv6_running) {
            dhcpv6_summary = _("no");
        }
        else if (!dhcpv6_lease) {
            dhcpv6_summary = _("yes (but no valid lease)");
        }
        else if (ctxt->cisc_dhcpv6_stateless) {
            dhcpv6_summary = _("yes (stateless)");
        }
        else {
            dhcpv6_summary = _("yes");
        }

        err = cli_printf_prequeried
            (bindings,
             _("   IPv6 enabled:       #%s/addr/settings/ipv6/enable#\n"), r);
        bail_error(err);
        if (ipv6_enabled) {
            err = cli_printf_prequeried
                (bindings,
                   _("   Autoconf enabled:   "
                     "#%s/addr/settings/ipv6/slaac/enable#\n"
                     "   Autoconf route:     "
                     "#%s/addr/settings/ipv6/slaac/default_route#\n"
                     "   Autoconf privacy:   "
                     "#%s/addr/settings/ipv6/slaac/privacy#\n"
                     "   DHCPv6 running:     %s\n"),
                 r, r, r, dhcpv6_summary);
            bail_error(err);
        }
#endif

        /* Display all IPv6 addresses found on the interface */

        /* /net/interface/address/state/ifdevname/vlan1249/ipv6addr */
        safe_free(addr_subtree);
        err = bn_binding_name_append_parts_va
            (&addr_subtree, "/net/interface/address/state/ifdevname",
             2, ifname, "ipv6addr");
        bail_error_null(err, addr_subtree);

        /* Make two passes: the first counts, the second prints all */
        num_ipv6_addrs = 0;

        for (addr_pass = 0; addr_pass < 2; addr_pass++) {
            num_addr_bindings =
                bn_binding_array_length_quick(ctxt->cisc_addr_bindings);
            for (i = 0; i < num_addr_bindings; ++i) {
                ts_free(&amask);
                ts_free(&status);

                addr_binding =
                    bn_binding_array_get_quick(ctxt->cisc_addr_bindings, i);
                bail_null(addr_binding);

                err = bn_binding_get_name(addr_binding, &addrroot_bn);
                bail_error(err);

                /*
                 * We want to find the parent of the addr nodes, which are like:
                 * /net/interface/address/state/ifdevname/vlan1249/ipv6addr/
                 * 3456::27
                 */
                if (!lc_is_prefix(addr_subtree, ts_str(addrroot_bn), false)) {
                    continue;
                }

                tstr_array_free(&name_parts_var);
                name_parts = NULL;

                name_parts = bn_binding_get_name_parts_const_quick(addr_binding);
                if (!name_parts) {
                    err = bn_binding_get_name_parts(addr_binding, &name_parts_var);
                    bail_error(err);
                    name_parts = name_parts_var;
                }
                if (tstr_array_length_quick(name_parts) != 8) {
                    continue;
                }

                err = tstr_array_get(name_parts, 5, (tstring **)&ifdevname);
                bail_error(err);

                err = tstr_array_get_str(name_parts, 7, &addr_str);
                bail_error(err);

                if (ts_nonempty(ifdevname) &&
                    ts_equal(name_last_part, ifdevname)) {

                    if (addr_pass == 0) {
                        num_ipv6_addrs++;
                    }
                    else {
                        if (!addr_pass_did_header) {
                            if (ipv6_enabled || num_ipv6_addrs != 0) {
                                err = cli_printf(_("   IPv6 addresses:     %d\n"),
                                                 num_ipv6_addrs);
                                bail_error(err);
                            }
                            addr_pass_did_header = true;
                        }

                        ts_free(&amask);
                        safe_free(amask_bn);
                        amask_bn = smprintf("%s/mask_len",
                                            ts_str(addrroot_bn));
                        err = bn_binding_array_get_value_tstr_by_name
                            (ctxt->cisc_addr_bindings, amask_bn, NULL, &amask);
                        bail_error(err);

                        ts_free(&status);
                        safe_free(status_bn);
                        status_bn = smprintf("%s/status",
                                             ts_str(addrroot_bn));
                        err = bn_binding_array_get_value_tstr_by_name
                            (ctxt->cisc_addr_bindings, status_bn, NULL,
                             &status);
                        bail_error(err);
                        /* Print nothing for preferred addresses */
                        if (!safe_strcmp(ts_str_maybe(status), "preferred")) {
                            ts_free(&status);
                        }
                        err = cli_printf(
                                   _("   IPv6 address:       %s/%s%s%s%s\n"),
                                   addr_str, ts_str(amask),
                                   ts_str_maybe(status) ? " (" : "",
                                   ts_str_maybe(status) ? ts_str(status) : "",
                                   ts_str_maybe(status) ? ")" : "");
                        bail_error(err);
                    }
                }
            }
        }

        err = bn_binding_array_get_value_tstr_by_name_rel
            (bindings, r, "devsource", NULL, &devsource);
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_rel
            (bindings, r, "type/ethernet/mac", NULL, &hwaddr);
        bail_error(err);

        if (!hwaddr || ts_length(hwaddr) == 0 ||
            ts_cmp_str(hwaddr, "N/A", false) == 0) {
            /* 
             * The ethernet mac was empty, see if the top level "hwaddr"
             * has something.
             */
            ts_free(&hwaddr);
            err = bn_binding_array_get_value_tstr_by_name_rel
                (bindings, r, "hwaddr", NULL, &hwaddr);
            bail_error(err);
        }

        err = cli_interface_get_source(ifname, &intf_source);
        bail_error(err);

        err = cli_printf_prequeried
            (bindings,
             _("   Speed:              #%s/type/ethernet/speed#\n"
               "   Duplex:             #%s/type/ethernet/duplex#\n"
               "   Interface type:     #%s/iftype#\n"
               "   Interface ifindex:  #%s/ifindex#\n"
               "   Interface source:   #%s/devsource#%s\n"),
             r, r, r, r, r, ts_str(intf_source));
        bail_error(err);

        err = bn_binding_array_get_value_tstr_by_name_rel
            (bindings, r, "bonding_master", NULL, &bonding_master);
        bail_error(err);

        if (bonding_master && ts_length(bonding_master) > 0) {
            err = cli_printf(_("   Bonding master:     %s\n"),
                             ts_str(bonding_master));
            bail_error(err);
        }

        err = bn_binding_array_get_value_tstr_by_name_rel
            (bindings, r, "bridge_group", NULL, &bridge_group);
        bail_error(err);

        if (bridge_group && ts_length(bridge_group) > 0) {
            err = cli_printf(_("   Bridge group:       %s\n"),
                             ts_str(bridge_group));
            bail_error(err);
        }

        err = cli_printf_prequeried
            (bindings,
             _("   MTU:                #%s/mtu#\n"
               "   HW address:         %s\n"),
             r, ts_str_maybe_empty(hwaddr));
        bail_error(err);

        /*
         * Note: this is always a config node, and from a perf
         * perspective we wish it weren't causing a seperate query here.
         */
        err = cli_printf_query
            (_("   Comment:            #/net/interface/config/%s/comment#\n"),
             ts_str(name_last_part));
        bail_error(err);

	err = cli_printf_query(_("\n   ARP announce setting:            #/net/interface/proc/intf/%s/conf/arp_announce#\n"), ts_str(name_last_part));
	bail_error(err);
	err = cli_printf_query(_("   ARP ignore setting:              #/net/interface/proc/intf/%s/conf/arp_ignore#\n"), ts_str(name_last_part));
	bail_error(err);
	err = cli_printf_query(_("   Generic Receive Offload:         #/net/interface/phy/state/%s/offloading/gro#\n"), ts_str(name_last_part));
	bail_error(err);
	err = cli_printf_query(_("   Generic Segment Offload:         #/net/interface/phy/state/%s/offloading/gso#\n"), ts_str(name_last_part));
	bail_error(err);
	err = cli_printf_query(_("   TCP Segment Offload:             #/net/interface/phy/state/%s/offloading/tso#\n"), ts_str(name_last_part));
	bail_error(err);

        if (!safe_strcmp(ts_str_maybe(devsource), "vlan")) {
            err = cli_printf_prequeried(ctxt->cisc_vlan_bindings,
                "   VLAN id:            #/net/vlan/state/vlans/%s/vlanid#\n",
                ifname);
            bail_error(err);
            err = cli_printf_prequeried(ctxt->cisc_vlan_bindings,
               "   VLAN interface:     #/net/vlan/state/vlans/%s/interface#\n",
               ifname);
            bail_error(err);
        }

        if ((show_type != cid_interface_show_brief) && !is_alias) {

            err = cli_printf("\n");
            bail_error(err);
            
/* ========================================================================= */
/* Customer-specific graft point 4: print status output after most of
 * ours, but before table of interface statistics.
 *
 * Note that this only runs for the full "show interfaces [<ifname>]"
 * command, not for the "brief" or "configured" variants.  It also
 * does not run for interface aliases.
 *
 * Note also that everything you print should be indented by three spaces
 * to match the rest of the display.
 * =========================================================================
 */
#ifdef INC_CLI_INTERFACE_CMDS_INC_GRAFT_POINT
#undef CLI_INTERFACE_CMDS_INC_GRAFT_POINT
#define CLI_INTERFACE_CMDS_INC_GRAFT_POINT 4
#include "../bin/cli/modules/cli_interface_cmds.inc.c"
#endif /* INC_CLI_INTERFACE_CMDS_INC_GRAFT_POINT */

            err = cli_printf_prequeried
                (bindings,
                 _("   RX bytes:           "
                   "#w:18~%s/stats/rx/bytes#"
                   "  TX bytes:       #%s/stats/tx/bytes#\n"
                   "   RX packets:         "
                   "#w:18~%s/stats/rx/packets#"
                   "  TX packets:     #%s/stats/tx/packets#\n"
                   "   RX mcast packets:   "
                   "#w:18~%s/stats/rx/multicast_packets#"
                   "  TX discards:    #%s/stats/tx/discards#\n"
                   "   RX discards:        "
                   "#w:18~%s/stats/rx/discards#"
                   "  TX errors:      #%s/stats/tx/errors#\n"
                   "   RX errors:          "
                   "#w:18~%s/stats/rx/errors#"
                   "  TX overruns:    #%s/stats/tx/overruns#\n"
                   "   RX overruns:        "
                   "#w:18~%s/stats/rx/overruns#"
                   "  TX carrier:     #%s/stats/tx/carrier#\n"
                   "   RX frame:           "
                   "#w:18~%s/stats/rx/frame#"
                   "  TX collisions:  #%s/stats/tx/collisions#\n"
                   "                       "
                   "                  "
                   "  TX queue len:   #%s/txqueuelen#\n"),
                 r, r, r, r, r, r, r, r, r, r, r, r, r, r, r);
            bail_error(err);
        }
    }

/* ========================================================================= */
/* Customer-specific graft point 5: print configuration or status
 * output after all of ours.
 *
 * Note that everything you print should be indented by three spaces
 * to match the rest of the display.
 * =========================================================================
 */
#ifdef INC_CLI_INTERFACE_CMDS_INC_GRAFT_POINT
#undef CLI_INTERFACE_CMDS_INC_GRAFT_POINT
#define CLI_INTERFACE_CMDS_INC_GRAFT_POINT 5
#include "../bin/cli/modules/cli_interface_cmds.inc.c"
#endif /* INC_CLI_INTERFACE_CMDS_INC_GRAFT_POINT */

 bail:
    ts_free(&alias_ifdevname);
    bn_binding_free(&display_binding);
    safe_free(bn_name);
    tstr_array_free(&name_parts_var);
    ts_free(&amask);
    safe_free(amask_bn);
    ts_free(&status);
    safe_free(status_bn);
    ts_free(&masklen_tstr);
    safe_free(netmask);
    safe_free(mask_len);
    safe_free(aifname_bn);
    ts_free(&aifname);
    ts_free(&devsource);
    ts_free(&hwaddr);
    ts_free(&bonding_master);
    ts_free(&bridge_group);
    ts_free(&intf_source);
    safe_free(addr_subtree);
    tstr_array_free(&static_ids);
    safe_free(ipv6_static_wc_name);
    safe_free(ipv6_static);
    ts_free(&ipv6_enabled_str);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
cli_interface_check_aliases(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    int err = 0;
    ht_table *aliases = callback_data;
    tstring *ifdevname = NULL;
    void *alist_v = NULL;
    tstring *alist = NULL;
    tbool found = false;

    err = bn_binding_array_get_value_tstr_by_name_rel
        (bindings, ts_str(name), "alias/ifdevname", NULL, &ifdevname);
    bail_error_null(err, ifdevname);

    /*
     * name_last_part contains the name of the alias interface.
     * ifdevname contains the name of the original interface that was aliased.
     * Our hash table maps the latter to a list of the former.
     */

    if (ts_num_chars(ifdevname) > 0) {
        err = ht_lookup(aliases, ifdevname, ts_length(ifdevname), &found,
                        NULL, NULL, &alist_v);
        bail_error(err);
        if (found) {
            alist = alist_v;
            err = ts_append_sprintf(alist, ", %s", ts_str(name_last_part));
            bail_error(err);
        }
        else {
            err = ht_insert(aliases, ifdevname, ts_length(ifdevname),
                            (void *)name_last_part);
            bail_error(err);
        }
    }

 bail:
    ts_free(&ifdevname);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* show interfaces
 * show interfaces <itf name>
 * show interfaces configured
 * show interfaces brief
 * show interfaces <itf name> configured
 * show interfaces <itf name> brief
 */
int
cli_interface_show(void *data, const cli_command *cmd,
                   const tstr_array *cmd_line,
                   const tstr_array *params)
{
    int err = 0;
    uint32 num_matches = 0;
    const char *if_name = NULL;
    char *pattern = NULL;
    tbool show_all = false, valid = false;
    bn_binding_array *bindings = NULL;
    cli_interface_show_context ctxt;
    bn_binding_array *addr_bindings = NULL, *vlan_bindings = NULL;
    bn_binding_array *config_bindings = NULL;
    ht_table *aliases = NULL;

/* ========================================================================= */
/* Customer-specific graft point 1: code to run before ours.
 *
 * You may assume that cmd->magic will contain one of the enums
 * starting with 'cid_interface_show'.  Please search the registrations
 * for 'cli_interface_show' to see which commands will land you in this
 * code.  For example, if you don't want to run any code if the user
 * specified an interface name, then verify that
 * 'tstr_array_get_str_quick(params, 0)' returns NULL.
 * =========================================================================
 */
#ifdef INC_CLI_INTERFACE_CMDS_INC_GRAFT_POINT
#undef CLI_INTERFACE_CMDS_INC_GRAFT_POINT
#define CLI_INTERFACE_CMDS_INC_GRAFT_POINT 1
#include "../bin/cli/modules/cli_interface_cmds.inc.c"
#endif /* INC_CLI_INTERFACE_CMDS_INC_GRAFT_POINT */

    if_name = tstr_array_get_str_quick(params, 0);
    if (if_name == NULL) {
        if_name = "*";
        show_all = true;
    }
    else {
        err = cli_interface_validate(if_name, &valid);
        bail_error(err);
        
        if (!valid) {
            goto bail;
        }
    }

    memset(&ctxt, 0, sizeof(ctxt));

    ctxt.cisc_magic = cmd->cc_magic;
    ctxt.cisc_show_all = show_all;

    err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
                                &(ctxt.cisc_ipv6_enable_overall),
                                "/net/general/config/ipv6/enable", NULL);
    bail_error(err);

    err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
                                &(ctxt.cisc_dhcpv6_stateless),
                                "/net/general/config/ipv6/dhcp/stateless",
                                NULL);
    bail_error(err);

    switch (cmd->cc_magic) {

    case cid_interface_show:
    case cid_interface_show_brief:
        /* Get all the /net/interface/config we need */
        safe_free(pattern);
        pattern = smprintf("/net/interface/config/%s", if_name);
        bail_null(pattern);
        err = mdc_get_binding_children
            (cli_mcc,
             NULL, NULL, true, &config_bindings, true, true, 
             (show_all) ? "/net/interface/config" : pattern);
        bail_error(err);
        err = bn_binding_array_sort(config_bindings);
        bail_error(err);
        ctxt.cisc_config_bindings = config_bindings;

        /* Get all the /net/interface/address/state/ifdevname we need */
        safe_free(pattern);
        pattern = smprintf("/net/interface/address/state/ifdevname/%s",
                           if_name);
        bail_null(pattern);
        err = mdc_get_binding_children
            (cli_mcc,
             NULL, NULL, true, &addr_bindings,
             false, true,
             "/net/interface/address/state/ifdevname");
        bail_error(err);
        err = bn_binding_array_sort(addr_bindings);
        bail_error(err);
        ctxt.cisc_addr_bindings = addr_bindings;

        /* Get all the /net/vlan/state/vlans we need */
        safe_free(pattern);
        pattern = smprintf("/net/vlan/state/vlans/%s", if_name);
        err = mdc_get_binding_children
            (cli_mcc,
             NULL, NULL, false, &vlan_bindings,
             false, true,
             (show_all) ? "/net/vlan/state/vlans" : pattern);
        bail_error(err);
        err = bn_binding_array_sort(vlan_bindings);
        bail_error(err);
        ctxt.cisc_vlan_bindings = vlan_bindings;

        /* Get all the /net/interface/state we need */
        safe_free(pattern);
        pattern = smprintf("/net/interface/state/%s", if_name);
        bail_null(pattern);
        err = mdc_get_binding_children
            (cli_mcc,
             NULL, NULL, true, &bindings, true, true, 
             (show_all) ? "/net/interface/state" : pattern);
        bail_error(err);
        err = bn_binding_array_sort(bindings);
        bail_error(err);

        /* Now for each interface, print our block */
        err = mdc_foreach_binding_prequeried
            (bindings, pattern, NULL, cli_interface_show_one,
             &ctxt, &num_matches);
        bail_error(err);
        if (num_matches == 0) {
            if (show_all) {
                err = cli_printf_error(_("No interfaces are detected.\n"));
                bail_error(err);
            }
            else {
                err = cli_printf_error(_("No interface with that name is "
                                         "detected.\n"));
                bail_error(err);
            }
        }
        break;

    case cid_interface_show_conf:
        pattern = smprintf("/net/interface/config/%s", if_name);
        bail_null(pattern);
        err = mdc_get_binding_children
            (cli_mcc,
             NULL, NULL, true, &bindings, true, true, 
             (show_all) ? "/net/interface/config" : pattern);
        bail_error(err);
        err = bn_binding_array_sort(bindings);
        bail_error(err);
        err = ht_new_tstring_tstring(&aliases, true);
        bail_error_null(err, aliases);
        err = mdc_foreach_binding_prequeried
            (bindings, "/net/interface/config/*", NULL,
             cli_interface_check_aliases, aliases, NULL);
        bail_error(err);
        ctxt.cisc_aliases = aliases;
        err = mdc_foreach_binding_prequeried
            (bindings, pattern, NULL, cli_interface_show_one,
             &ctxt, &num_matches);
        bail_error(err);
        if (num_matches == 0) {
            if (show_all) {
                err = cli_printf_error
                    (_("There are no configured interfaces.\n"));
                bail_error(err);
            }
            else {
                err = cli_printf_error
                    (_("There is no configuration for that interface.\n"));
                bail_error(err);
            }
        }
        break;

    default:
        bail_force(lc_err_unexpected_case);
    }

/* ========================================================================= */
/* Customer-specific graft point 2: code to run after ours.
 *
 * Note that this may get skipped in some errors cases; see code above.
 * =========================================================================
 */
#ifdef INC_CLI_INTERFACE_CMDS_INC_GRAFT_POINT
#undef CLI_INTERFACE_CMDS_INC_GRAFT_POINT
#define CLI_INTERFACE_CMDS_INC_GRAFT_POINT 2
#include "../bin/cli/modules/cli_interface_cmds.inc.c"
#endif /* INC_CLI_INTERFACE_CMDS_INC_GRAFT_POINT */

 bail:
    bn_binding_array_free(&bindings);
    bn_binding_array_free(&addr_bindings);
    bn_binding_array_free(&vlan_bindings);
    bn_binding_array_free(&config_bindings);
    safe_free(pattern);
    ht_free(&aliases);
    return(err);
}


/* ------------------------------------------------------------------------- */
#ifdef PROD_FEATURE_BONDING
int
cli_interface_bonding(void *data, const cli_command *cmd,
                      const tstr_array *cmd_line, const tstr_array *params)
{
    int err = 0;
    const char *ifname = NULL;
    tbool valid = false;
    const char *bond_name = NULL;
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32_array *bif_indices = NULL;
    int32 db_rev = 0;
    uint32 num_bifs = 0;
    bn_request *req = NULL;

    ifname = tstr_array_get_str_quick(params, 0);
    bail_null(ifname);

    err = cli_interface_validate(ifname, &valid);
    bail_error(err);

    if (!valid) {
        goto bail; /* XXX ok? */
    }

    bond_name = tstr_array_get_str_quick(params, 1);

    err = bn_binding_array_new(&barr);
    bail_error(err);

    err = bn_binding_new_str_autoinval
        (&binding, "name", ba_value, bt_string, 0, bond_name);
    bail_error_null(err, binding);

    err = bn_binding_array_append_takeover(barr, &binding);
    bail_error(err);

    err = mdc_array_get_matching_indices
        (cli_mcc,
         "/net/bonding/config", NULL, barr, bn_db_revision_id_none,
         &db_rev, &bif_indices, NULL, NULL);
    bail_error(err);

    num_bifs = uint32_array_length_quick(bif_indices);
    if (num_bifs != 1) {
        err = cli_printf_error(_("Unrecognized bond '%s'.\n"), bond_name);
        bail_error(err);
        goto bail;
    }

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    switch (cmd->cc_magic) {
    case cid_intf_bonding_add:
        err = bn_set_request_msg_append_new_binding_fmt
            (req, bsso_modify, 0, bt_string, 0, ifname, NULL,
             "/net/bonding/config/%d/interface/%s",
             uint32_array_get_quick(bif_indices, 0), ifname);
        bail_error(err);
        break;

    case cid_intf_bonding_delete:
        err = bn_set_request_msg_append_new_binding_fmt
            (req, bsso_delete, 0, bt_string, 0, ifname, NULL,
             "/net/bonding/config/%d/interface/%s",
             uint32_array_get_quick(bif_indices, 0), ifname);
        bail_error(err);
        break;

    default:
        bail_force(lc_err_unexpected_case);
    }

    err = bn_set_request_msg_set_option_cond_db_revision_id(req, db_rev);
    bail_error(err);

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);

 bail:
    bn_binding_free(&binding);
    uint32_array_free(&bif_indices);
    bn_request_msg_free(&req);
    bn_binding_array_free(&barr);
    return(err);
}
#endif /* PROD_FEATURE_BONDING */
