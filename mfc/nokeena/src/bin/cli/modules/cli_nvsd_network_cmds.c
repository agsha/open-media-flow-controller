/*
 * Filename :   cli_nvsd_network_cmds.c
 * Date     :   2008/12/15
 * Author   :   Dhruva Narasimhan
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
#include "file_utils.h"
#include "nkn_defs.h"

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
enum{
    cid_tcp_mem,
    cid_tcp_rmem,
    cid_tcp_wmem
};

enum{
    DNS_CACHE_ALL,
    DNS_CACHE_FQDN
};

int
cli_nvsd_network_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_nvsd_network_intf_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


extern int
cli_std_exec(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params);

int
cli_print_restart_msg(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params);

int cli_nvsd_network_connection_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
cli_nvsd_tcp_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

static int
cli_nvsd_port_range_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

static int
cli_nvsd_tcpr_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

static int
cli_nvsd_tcpw_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);


static int cli_nvsd_network_show(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);

int cli_nvsd_network_acl_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components,
        const tstring *name_last_part,
        const tstring *value,
        void *callback_data);

static int cli_nvsd_interface_txqueuelen_show (
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);

int cli_nvsd_tcp_exec(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);

int cli_nvsd_tcp_reset(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);

int cli_nvsd_tcp_write_exec(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);

int cli_nvsd_tcp_read_exec(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);


int cli_nvsd_name_resolver_delete_entry(
		   void *data,
		    const cli_command *cmd,
		    const tstr_array *cmd_line,
		    const tstr_array *params);

int cli_nvsd_name_resolver_delete_all(
		   void *data,
		    const cli_command *cmd,
		    const tstr_array *cmd_line,
		    const tstr_array *params);

int cli_nvsd_name_resolver_cache_timeout(
		   void *data,
		    const cli_command *cmd,
		    const tstr_array *cmd_line,
		    const tstr_array *params);

int 
cli_interface_get_names(
         tstr_array **ret_names,
         tbool hide_display);

static int
cli_nvsd_name_resolver_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply);

int cli_nvsd_name_resolver_show(
		   void *data,
		    const cli_command *cmd,
		    const tstr_array *cmd_line,
		    const tstr_array *params);

int cli_nvsd_name_resolver_cache_show(
		   void *data,
		    const cli_command *cmd,
		    const tstr_array *cmd_line,
		    const tstr_array *params);

static int cli_nvsd_dns_cache_list(const bn_binding_array *arr, uint32 idx,
        		bn_binding *binding, void *data);
int
cli_nvsd_network_intf_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;
    const char *ignore_name[] = {"/net/interface/config/*/offloading/gro/enable",
				 "/net/interface/config/*/offloading/gso/enable",
				 "/net/interface/config/*/offloading/tso/enable"};

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str = "interface * generic-receive-offload";
    cmd->cc_help_descr     = ("Set Generic Receive Offload (GRO) to assemble packets at NIC instead of network stack");
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    cmd->cc_words_str = "interface * generic-receive-offload enable";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_help_descr     = ("Set Generic Receive Offload (GRO) to assemble packets at NIC instead of network stack");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_reset;
    cmd->cc_exec_name      = "/net/interface/config/$1$/offloading/gro/enable";
 //   cmd->cc_exec_type      = bt_bool;
   // cmd->cc_exec_value     = "true";
   // cmd->cc_revmap_order   = cro_interface;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "no interface * generic-receive-offload";
    cmd->cc_help_descr     = ("Unset Generic Receive Offload (GRO) to assemble packets at NIC instead of network stack");
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    CLI_CMD_NEW;
    cmd->cc_words_str = "no interface * generic-receive-offload enable";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_help_descr     = ("Unset Generic Receive Offload (GRO) to assemble packets at NIC instead of network stack");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/net/interface/config/$1$/offloading/gro/enable";
    cmd->cc_exec_type      = bt_bool;
    cmd->cc_exec_value     = "false";
    cmd->cc_revmap_order   = cro_interface;
    cmd->cc_revmap_type    = crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "interface * generic-segmentation-offload";
    cmd->cc_help_descr     = ("Set Generic Segment Offload (GSO) to segment packets at NIC instead of network stack");
    cmd->cc_flags          = ccf_hidden | ccf_ignore_length;
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    cmd->cc_words_str = "interface * generic-segmentation-offload enable";
    cmd->cc_flags          = ccf_terminal | ccf_hidden | ccf_ignore_length;
    cmd->cc_help_descr     = ("Set Generic Segment Offload (GSO) to segment packets at NIC instead of network stack");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_reset;
    cmd->cc_exec_name      = "/net/interface/config/$1$/offloading/gso/enable";
 //   cmd->cc_exec_type      = bt_bool;
   // cmd->cc_exec_value     = "true";
   // cmd->cc_revmap_order   = cro_interface;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "no interface * generic-segmentation-offload";
    cmd->cc_help_descr     = ("Unset Generic Segment Offload (GSO) to segment packets at NIC instead of network stack");
    cmd->cc_flags          = ccf_hidden | ccf_ignore_length;
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    CLI_CMD_NEW;
    cmd->cc_words_str = "no interface * generic-segmentation-offload enable";
    cmd->cc_flags          = ccf_terminal | ccf_hidden | ccf_ignore_length;
    cmd->cc_help_descr     = ("Unset Generic Segment Offload (GSO) to segment packets at NIC instead of network stack");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/net/interface/config/$1$/offloading/gso/enable";
    cmd->cc_exec_type      = bt_bool;
    cmd->cc_exec_value     = "false";
    cmd->cc_revmap_order   = cro_interface;
    cmd->cc_revmap_type    = crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "interface * tcp-segmentation-offload";
    cmd->cc_help_descr     = ("Set TCP Segment Offload (TSO) to segment TCP packets at NIC instead of network stack");
    cmd->cc_flags          = ccf_hidden;;
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    cmd->cc_words_str = "interface * tcp-segmentation-offload enable";
    cmd->cc_flags          = ccf_terminal | ccf_hidden;;
    cmd->cc_help_descr     = ("Set TCP Segment Offload (TSO) to segment TCP packets at NIC instead of network stack");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_reset;
    cmd->cc_exec_name      = "/net/interface/config/$1$/offloading/tso/enable";
 //   cmd->cc_exec_type      = bt_bool;
   // cmd->cc_exec_value     = "true";
   // cmd->cc_revmap_order   = cro_interface;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = "no interface * tcp-segmentation-offload";
    cmd->cc_help_descr     = ("Unset TCP Segment Offload (TSO) to segment TCP packets at NIC instead of network stack");
    cmd->cc_flags          = ccf_hidden;;
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    CLI_CMD_NEW;
    cmd->cc_words_str = "no interface * tcp-segmentation-offload enable";
    cmd->cc_flags          = ccf_terminal | ccf_hidden;;
    cmd->cc_help_descr     = ("Unset TCP Segment Offload (TSO) to segment TCP packets at NIC instead of network stack");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/net/interface/config/$1$/offloading/tso/enable";
    cmd->cc_exec_type      = bt_bool;
    cmd->cc_exec_value     = "false";
    cmd->cc_revmap_order   = cro_interface;
    cmd->cc_revmap_type    = crt_auto;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_arr(ignore_name);
    bail_error(err);


    bail:
	return err;
}

int
cli_nvsd_network_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    //UNUSED_ARGUMENT(info);
    //UNUSED_ARGUMENT(context);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = cli_set_gettext_domain(GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */


    err = cli_nvsd_network_intf_cmds_init(info, context);
    bail_error(err);

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network";
    cmd->cc_help_descr =            N_("Configure network options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network";
    cmd->cc_help_descr =            N_("Reset/Clear network options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network iptables";
    cmd->cc_help_descr =            N_("Configure iptables options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network iptables enable";
    cmd->cc_help_descr =            N_("Allow kernel to demand-load iptables modules.");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/ip-tables";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network iptables disable";
    cmd->cc_help_descr =            N_("Allow kernel to demand-load iptables modules");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/ip-tables";
    cmd->cc_revmap_type =           crt_none;
    CLI_CMD_REGISTER;

#if 0 /*bug 9941*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "network dynamic-routing";
    cmd->cc_help_descr =            N_("Enable BGP configuration for traffic steering");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/dynamic-routing";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network dynamic-routing";
    cmd->cc_help_descr =            N_("Disable BGP configuration for traffic steering");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/dynamic-routing";
    cmd->cc_revmap_type = 			crt_none;
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection";
    cmd->cc_help_descr =            N_("Configure network connection options");
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_flags =                 ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_exec_callback =         cli_nvsd_network_connection_enter_prefix_mode;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection";
    cmd->cc_help_descr =            N_("Negate network connection options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection no";
    cmd->cc_help_descr =            N_("Negate network connection options");
    cmd->cc_req_prefix_count =      2;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection ip-forward";
    cmd->cc_help_descr =            N_("Forwards IPv4 packets that don't match any open connection");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/ip-forward";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection ip-forward";
    cmd->cc_help_descr =            N_("Don't Forwards IPv4 packets that don’t match any open connection");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/ip-forward";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection idle";
    cmd->cc_help_descr =            N_("Configure idle timeout");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection idle timeout";
    cmd->cc_help_descr =            N_("Configure idle timeout");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection idle";
    cmd->cc_help_descr =            N_("Configure idle timeout");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection idle timeout";
    cmd->cc_help_descr =            N_("Set default idle timeout - 60 sec");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/timeout";
    //cmd->cc_revmap_type =           crt_auto;
    //cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection idle timeout *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("Timeout, in seconds");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/timeout";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;
    /*---------------------------------------------------------------------*/


    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection concurrent";
    cmd->cc_help_descr =            
        N_("Configure maximum number of concurrent connections");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection concurrent session";
    cmd->cc_help_descr =            
        N_("Configure maximum number of concurrent sessions");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection concurrent";
    cmd->cc_help_descr =            
        N_("Reset maximum number of concurrent connections");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection concurrent session";
    cmd->cc_help_descr =            
        N_("Reset maximum number of concurrent sessions");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/max_connections/no";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_exec_value =             "1";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection concurrent session port-mapper-disable";
    cmd->cc_help_descr =            N_("Disables port mapper feature");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden;;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/max_connections/pmapper-disable";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_callback =         cli_print_restart_msg;
    cmd->cc_revmap_type =           crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection concurrent session port-mapper-disable";
    cmd->cc_help_descr =            N_("Enables port mapper feature");
    cmd->cc_flags =                 ccf_terminal | ccf_hidden;;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/max_connections/pmapper-disable";
    cmd->cc_exec_callback =         cli_print_restart_msg;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection concurrent session *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("Number of connections");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name2 =            "/nkn/nvsd/network/config/max_connections";
    cmd->cc_exec_value2 =           "$1$";
    cmd->cc_exec_type2 =            bt_uint32;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/max_connections/no";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_exec_value =            "0";
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_order =          cro_mgmt;
    cmd->cc_revmap_names = 		"/nkn/nvsd/network/config/max_connections";
    cmd->cc_revmap_cmds = 		"network connection concurrent session $v$";
    CLI_CMD_REGISTER;
    /*---------------------------------------------------------------------*/

    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection max-bandwidth";
    cmd->cc_help_descr =            N_("Configure maximum bandwidth");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection max-bandwidth";
    cmd->cc_help_descr =            N_("Set default maximum bandwidth - 0 kbps (infinite)");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/max_bandwidth/no";
    cmd->cc_exec_value =            "1";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_none;
    //cmd->cc_revmap_order =          cro_mgmt;
    //cmd->cc_revmap_names =          "/nkn/nvsd/network/config/max_bandwidth";
    //cmd->cc_revmap_cmds =           "no network connection max-bandwidth";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection max-bandwidth *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("Kbps");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name2 =            "/nkn/nvsd/network/config/max_bandwidth";
    cmd->cc_exec_value2 =           "$1$";
    cmd->cc_exec_type2 =            bt_uint32;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/max_bandwidth/no";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_exec_value =            "0";
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_order =          cro_mgmt;
    cmd->cc_revmap_names = 		"/nkn/nvsd/network/config/max_bandwidth";
    cmd->cc_revmap_cmds = 		"network connection max-bandwidth $v$";
    CLI_CMD_REGISTER;
    /*---------------------------------------------------------------------*/

  /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection origin";
    cmd->cc_help_descr =            N_("Configure connection parameter for the origin side");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection origin queue";
    cmd->cc_help_descr =            N_("Configure origin queue parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection origin queue max-delay";
    cmd->cc_help_descr =            N_("Configure maximum delay that the origin queue can take");
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    cmd->cc_words_str =             "network connection origin queue max-delay *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("seconds");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/connection/origin/queue/max_delay";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection origin queue max-num";
    cmd->cc_help_descr =            N_("Configure maximum number of connections"
					" that the origin queue should take");
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    cmd->cc_words_str =             "network connection origin queue max-num *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("number of connections");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/connection/origin/queue/max_num";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection origin";
    cmd->cc_help_descr =            N_("Reset connection parameter for the origin side");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection origin queue";
    cmd->cc_help_descr =            N_("Reset origin queue parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection origin queue max-delay";
    cmd->cc_help_descr =            N_("Reset the time untill which the origin requests are queued.");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/connection/origin/queue/max_delay";
    cmd->cc_revmap_type =           crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection origin queue max-num";
    cmd->cc_help_descr =            N_("Reset the configured number of origin"
					" requests that can be queued.");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/connection/origin/queue/max_num";
    cmd->cc_revmap_type =           crt_none;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/
    /* [no] network connection transparent-proxy port-range <min> <max>                      */
    /*---------------------------------------------------------------------*/

    CLI_CMD_NEW;
    cmd->cc_words_str           = "network connection transparent-proxy";
    cmd->cc_help_descr          = N_("Configure Transparent proxy parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           = "network connection transparent-proxy port-range";
    cmd->cc_help_descr          = N_("Configure restricted Transparent proxy range of source ports to establish connection with origin server");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           = "network connection transparent-proxy port-range *";
    cmd->cc_help_exp            = N_("<min-src-port-number>");
    cmd->cc_help_exp_hint       = N_("<min> - Minimum value of the source port");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           = "network connection transparent-proxy port-range * *";
    cmd->cc_help_exp            = N_("<max-src-port-number>");
    cmd->cc_help_exp_hint       = N_("<max> - Maximum value of the source port");
    cmd->cc_flags               = ccf_terminal;
    cmd->cc_capab_required      = ccp_set_rstr_curr;
    cmd->cc_exec_operation      = cxo_set;
    cmd->cc_exec_name           = "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_min";
    cmd->cc_exec_value          = "$1$";
    cmd->cc_exec_type           = bt_uint16;
    cmd->cc_exec_name2          = "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_max";
    cmd->cc_exec_value2         = "$2$";
    cmd->cc_exec_type2          = bt_uint16;
    cmd->cc_revmap_type         = crt_manual;
    cmd->cc_revmap_callback     = cli_nvsd_port_range_revmap;
    cmd->cc_revmap_order        = cro_mgmt;
    cmd->cc_exec_callback       = cli_print_restart_msg;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str           = "no network connection transparent-proxy";
    cmd->cc_help_descr          = N_("Reset the  Transparent proxy parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network connection transparent-proxy port-range";
    cmd->cc_help_descr =        N_("Disable restricted T-proxy source port range");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_min";
    cmd->cc_exec_value          = D_TP_PORT_RANGE_MIN_DEF_STR;
    cmd->cc_exec_type           = bt_uint16;
    cmd->cc_exec_name2          = "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_max";
    cmd->cc_exec_value2         = D_TP_PORT_RANGE_MAX_DEF_STR;
    cmd->cc_exec_type2          = bt_uint16;
    cmd->cc_exec_callback =     cli_print_restart_msg;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/
    /* [no] network connection origin failover use-dns-response            */
    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection origin failover";
    cmd->cc_help_descr =            N_("Fallback to the next origin server upon connection failure") ;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection origin failover use-dns-response";
    cmd->cc_help_descr =            N_("Use the IP addresses returned by DNS for failover");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/connection/origin/failover/use_dns_response";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "true";
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection origin failover";
    cmd->cc_help_descr =            N_("Reset origin failover parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection origin failover use-dns-response";
    cmd->cc_help_descr =            N_("Remove the IP addresses returned by DNS for failover");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/connection/origin/failover/use_dns_response";
    cmd->cc_revmap_type =           crt_none;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/
    /* [no] network connection origin connect timeout <timeout in secs>    */
    /*---------------------------------------------------------------------*/

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection origin connect";
    cmd->cc_help_descr =            N_("Configure origin connection parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection origin connect timeout";
    cmd->cc_help_descr =            N_("Connection Timeout value");
    CLI_CMD_REGISTER;
    CLI_CMD_NEW;

    cmd->cc_words_str =             "network connection origin connect timeout *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("seconds");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/connection/origin/connect/timeout";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection origin connect";
    cmd->cc_help_descr =            N_("Reset origin connect parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection origin connect timeout";
    cmd->cc_help_descr =            N_("Reset the origin connection timeout value") ;
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/connection/origin/connect/timeout";
    cmd->cc_revmap_type =           crt_none;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection assured-flow-rate";
    cmd->cc_help_descr =            
        N_("Configure Assured Flow Rate (AFR) options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no network connection assured-flow-rate";
    cmd->cc_help_descr =    
        N_("Set default AFR rate - 0 kbps (infinite)");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/assured_flow_rate";
    cmd->cc_exec_value =            "0";
    cmd->cc_exec_type =             bt_uint32;
    //cmd->cc_revmap_type =           crt_auto;
    //cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection assured-flow-rate *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("rate in kbps");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/assured_flow_rate";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;


    /*---------------------------------------------------------------------*/


    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp";
    cmd->cc_help_descr =        N_("Set the TCP parameters");
    CLI_CMD_REGISTER;
 
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network tcp";
    cmd->cc_help_descr =        N_("reset the TCP parameters");
    CLI_CMD_REGISTER;
 
    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp syn-cookie";
    cmd->cc_help_descr =        N_("Enable the syn-cookie");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/syn-cookie";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_value =        "true";
    cmd->cc_revmap_order =      cro_mgmt;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_suborder =   1000;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network tcp syn-cookie";
    cmd->cc_help_descr =        N_("Disable the syn-cookie");
    cmd->cc_flags =             ccf_terminal ;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/syn-cookie";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_value =        "false";
    cmd->cc_revmap_order =      cro_mgmt;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_suborder =   1000;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp syn-cookie half-open-conn";
    cmd->cc_help_descr =        N_("configure the half-open-conn");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp syn-cookie half-open-conn *";
    cmd->cc_help_exp =          N_("<positive number>");
    cmd->cc_help_exp_hint =     N_("Set number between 1 and 16000.");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name2 =        "/nkn/nvsd/network/config/syn-cookie";
    cmd->cc_exec_type2 =        bt_bool;
    cmd->cc_exec_value2 =       "true";
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/syn-cookie/half_open_conn";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_mgmt;
    cmd->cc_revmap_names =      "/nkn/nvsd/network/config/syn-cookie/half_open_conn";
    cmd->cc_revmap_cmds =       "network tcp syn-cookie half-open-conn $v$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp max-orphans";
    cmd->cc_help_descr =        N_("configure the Maximal number of TCP sockets not attached to any user file handle");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp max-orphans *";
    cmd->cc_help_exp =          N_("<positive number>");
    cmd->cc_help_exp_hint =     N_("Set number between 1024 to 524288");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/tcp_max_orphans";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network tcp max-orphans";
    cmd->cc_help_descr =        N_("reset the Maximal number of TCP sockets not attached to any user file handle");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/tcp_max_orphans";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_value =        "131072";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp fin-timeout";
    cmd->cc_help_descr =        N_("configure the timeout value for keeping the sockets in FIN-WAIT-2 state");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp fin-timeout *";
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_help_exp_hint =     N_("Set seconds between 5 and 120");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/tcp_fin_timeout";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network tcp fin-timeout";
    cmd->cc_help_descr =        N_("reset the timeout value for keeping the sockets in FIN-WAIT-2 state");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/tcp_fin_timeout";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_value =        "20";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp max-tw-buckets";
    cmd->cc_help_descr =        N_("configure the max number of TIME_WAIT sockets");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp max-tw-buckets *";
    cmd->cc_help_exp =          N_("<Positive Number>");
    cmd->cc_help_exp_hint =     N_("Set number between 1024 and 1048576");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/tcp_max_tw_buckets";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_revmap_type =       crt_manual;
    cmd->cc_revmap_order =      cro_mgmt;
    cmd->cc_revmap_names =      "/nkn/nvsd/network/config/tcp_max_tw_buckets";
    cmd->cc_revmap_cmds =       "network tcp max-tw-buckets $v$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network tcp max-tw-buckets";
    cmd->cc_help_descr =        N_("reset the max number of TIME_WAIT sockets");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/tcp_max_tw_buckets";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_value =        "16384";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp memory";
    cmd->cc_help_descr =        N_("set the TCP stack value for memory usuage");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp memory low";
    cmd->cc_help_descr =        N_("set the low threshold value");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp memory low *";
    cmd->cc_help_exp =          N_("<pages>");
    cmd->cc_help_exp_hint =     N_("The pages are allocated in the units of 4 KB. Set low between 98304 to 393216");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp memory low * high";
    cmd->cc_help_descr =        N_("set the high threshold value");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp memory low * high *";
    cmd->cc_help_exp =          N_("<pages>");
    cmd->cc_help_exp_hint =     N_("The pages are allocated in the units of 4 KB. Set high between 131072 to 524288");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp memory low * high * maxpage";
    cmd->cc_help_descr =        N_("Set the maximum number of memory pages can be used");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp memory low * high * maxpage *";
    cmd->cc_help_exp =          N_("<pages>");
    cmd->cc_help_exp_hint =     N_("The pages are allocated in the units of 4 KB. Set maxpage between 196608 to 6291456");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_tcp_exec; 
    cmd->cc_revmap_order =      cro_mgmt;
    cmd->cc_revmap_callback =   cli_nvsd_tcp_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network tcp memory";
    cmd->cc_magic =		cid_tcp_mem;
    cmd->cc_help_descr =        N_("reset the TCP stack value for memory usuage");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_tcp_reset;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp write-memory";
    cmd->cc_help_descr =        N_("set the memory usuage values for sendbuffer");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp write-memory min";
    cmd->cc_help_descr =        N_("set the the minimum TCP send buffer space available for a single TCP socket");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network tcp write-memory";
    cmd->cc_magic =             cid_tcp_wmem;
    cmd->cc_help_descr =        N_("reset the memory usuage values for sendbuffer");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_tcp_reset;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp write-memory min *";
    cmd->cc_help_exp =          N_("<KB>");
    cmd->cc_help_exp_hint =     N_("Set minimum between 4096 to 32768");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp write-memory min * default";
    cmd->cc_help_descr =        N_("set the default send buffer space allowed for a single TCP socket ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp write-memory min * default *";
    cmd->cc_help_exp =          N_("<KB>");
    cmd->cc_help_exp_hint =     N_("Set default between 32768 to 1048576");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp write-memory min * default * max";
    cmd->cc_help_descr =        N_("Set the maximum TCP send buffer space");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp write-memory min * default * max *";
    cmd->cc_help_exp =          N_("<KB>");
    cmd->cc_help_exp_hint =     N_("Set maximum between 65536 to 16777216");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_tcp_write_exec; 
    cmd->cc_revmap_order =      cro_mgmt;
    cmd->cc_revmap_callback =   cli_nvsd_tcpr_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp read-memory";
    cmd->cc_help_descr =        N_("set the memory usuage values for read buffer");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp read-memory min";
    cmd->cc_help_descr =        N_("set the the minimum TCP read buffer space available for a single TCP socket");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network tcp read-memory";
    cmd->cc_help_descr =        N_("reset the memory usuage values for read buffer");
    cmd->cc_magic =             cid_tcp_rmem;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_tcp_reset;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp read-memory min *";
    cmd->cc_help_exp =          N_("<KB>");
    cmd->cc_help_exp_hint =     N_("Set minimum between 4096 to 32768");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp read-memory min * default";
    cmd->cc_help_descr =        N_("set the default read buffer space allowed for a single TCP socket ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp read-memory min * default *";
    cmd->cc_help_exp =          N_("<KB>");
    cmd->cc_help_exp_hint =     N_("Set default between 32768 to 1048576");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp read-memory min * default * max";
    cmd->cc_help_descr =        N_("Set the maximum TCP read buffer space");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp read-memory min * default * max *";
    cmd->cc_help_exp =          N_("<KB>");
    cmd->cc_help_exp_hint =     N_("Set max between 65536 to 16777216");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_tcp_read_exec;
    cmd->cc_revmap_order =      cro_mgmt;
    cmd->cc_revmap_callback =   cli_nvsd_tcpw_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp path-mtu";
    cmd->cc_help_descr =        N_("Set the tcp path-mtu parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp path-mtu discovery";
    cmd->cc_help_descr =        N_("Set the tcp path-mtu discovery parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp path-mtu discovery on";
    cmd->cc_help_descr =        N_("Enable the path-mtu discovery");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/path-mtu-discovery";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp path-mtu discovery off";
    cmd->cc_help_descr =        N_("Disable the path-mtu discovery");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/path-mtu-discovery";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp options";
    cmd->cc_help_descr =        N_("Customize TCP options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp options timestamps";
    cmd->cc_help_descr =        N_("Customize tcp timestamps options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp options timestamps enable";
    cmd->cc_help_descr =        "Negotiate timestamps in tcp options during session establishment";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/tcp/timestamp";
    cmd->cc_exec_value =        "true";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network tcp options timestamps disable";
    cmd->cc_help_descr =        "Do not negotiate  timestamps in tcp options during session establishment";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/network/config/tcp/timestamp";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/

     CLI_CMD_NEW;
     cmd->cc_words_str =             "name-resolver";
     cmd->cc_help_descr =            N_("Configure DNS options");
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =             "no name-resolver";
     cmd->cc_help_descr =            N_("Reset/Clear DNS options");
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =             "name-resolver query-mode";
     cmd->cc_help_descr =            N_("Configure DNS query Mode");
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =             "no name-resolver query-mode";
     cmd->cc_help_descr =            N_("Configure network connection options");
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =             "name-resolver query-mode asynchronous";
     cmd->cc_help_descr =	     N_("Enable Asyncronous (non-blocking) DNS resolution");
     cmd->cc_flags =		     ccf_terminal;
     cmd->cc_capab_required =	     ccp_set_rstr_curr;
     cmd->cc_exec_operation =	     cxo_set;
     cmd->cc_exec_name =	     "/nkn/nvsd/network/config/resolver/async-enable";
     cmd->cc_exec_value =	     "true";
     cmd->cc_exec_type =	     bt_bool;
     cmd->cc_exec_callback =         cli_print_restart_msg;
     cmd->cc_revmap_type =           crt_auto;
     cmd->cc_revmap_order =          cro_mgmt;
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =             "no name-resolver query-mode asynchronous";
     cmd->cc_help_descr =	     N_("Enable Asyncronous (non-blocking) DNS resolution");
     cmd->cc_flags =		     ccf_terminal;
     cmd->cc_capab_required =	     ccp_set_rstr_curr;
     cmd->cc_exec_operation =	     cxo_set;
     cmd->cc_exec_name =	     "/nkn/nvsd/network/config/resolver/async-enable";
     cmd->cc_exec_value =	     "false";
     cmd->cc_exec_type =	     bt_bool;
     cmd->cc_exec_callback =         cli_print_restart_msg;
     cmd->cc_revmap_type =           crt_auto;
     cmd->cc_revmap_order =          cro_mgmt;
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =	     "name-resolver cache-timeout";
     cmd->cc_help_descr = 	     N_("Configure cache timeout for entries in the DNS cache");
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =	     "no name-resolver cache-timeout";
     cmd->cc_help_descr =	     N_("Clear user configured DNS cache timeout and use default of 2047 seconds");
     cmd->cc_flags =		     ccf_terminal;
     cmd->cc_capab_required =	     ccp_set_rstr_curr;
     cmd->cc_exec_operation =	     cxo_reset;
     cmd->cc_exec_name =	     "/nkn/nvsd/network/config/resolver/cache-timeout";
     cmd->cc_exec_value =	     "2047";
     cmd->cc_exec_type =	     bt_int32;
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =             "name-resolver adnsd-enabled";
     cmd->cc_help_descr =            N_("Enable using ADNS daemon");
     cmd->cc_flags =                 ccf_terminal;
     cmd->cc_capab_required =        ccp_set_rstr_curr;
     cmd->cc_exec_operation =        cxo_set;
     cmd->cc_exec_name =             "/nkn/nvsd/network/config/resolver/adnsd-enabled";
     cmd->cc_exec_value =            "true";
     cmd->cc_exec_type =             bt_bool;
     cmd->cc_exec_callback =         cli_print_restart_msg;
     cmd->cc_revmap_type =           crt_auto;
     cmd->cc_revmap_order =          cro_mgmt;
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =             "no name-resolver adnsd-enabled";
     cmd->cc_help_descr =            N_("Disable using ADNS daemon");
     cmd->cc_flags =                 ccf_terminal;
     cmd->cc_capab_required =        ccp_set_rstr_curr;
     cmd->cc_exec_operation =        cxo_set;
     cmd->cc_exec_name =             "/nkn/nvsd/network/config/resolver/adnsd-enabled";
     cmd->cc_exec_value =            "false";
     cmd->cc_exec_type =             bt_bool;
     cmd->cc_exec_callback =         cli_print_restart_msg;
     cmd->cc_revmap_type =           crt_auto;
     cmd->cc_revmap_order =          cro_mgmt;
     CLI_CMD_REGISTER;


     CLI_CMD_NEW;
     cmd->cc_words_str =	     "name-resolver cache-timeout random";
     cmd->cc_help_descr =	     N_("Allow MFC to set a randomly selected cache timeout value for a DNS cache entry");
     cmd->cc_flags =		     ccf_terminal;
     cmd->cc_capab_required =	     ccp_set_rstr_curr;
     cmd->cc_exec_operation =	     cxo_set;
     cmd->cc_exec_name =	     "/nkn/nvsd/network/config/resolver/cache-timeout";
     cmd->cc_exec_value =	     "-1";
     cmd->cc_exec_type =	     bt_int32;
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =	     "name-resolver cache-timeout auto";
     cmd->cc_help_descr =	     N_("Allow MFC to use the TTL provided by the DNS server as timeout value for a DNS cache entry");
     cmd->cc_flags =		     ccf_terminal;
     cmd->cc_capab_required =	     ccp_set_rstr_curr;
     cmd->cc_exec_operation =	     cxo_set;
     cmd->cc_exec_name =	     "/nkn/nvsd/network/config/resolver/cache-timeout";
     cmd->cc_exec_value =	     "-100";
     cmd->cc_exec_type =	     bt_int32;
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =	     "name-resolver cache-timeout *";
     cmd->cc_help_exp =		     N_("<seconds>");
     cmd->cc_help_exp_hint =	     N_("Timeout value in seconds (Warning: value of 0 will persist each entry in the cache)");
     cmd->cc_flags =		     ccf_terminal;
     cmd->cc_capab_required =	     ccp_set_rstr_curr;
     cmd->cc_exec_operation =	     cxo_set;
     cmd->cc_exec_callback =         cli_nvsd_name_resolver_cache_timeout;
     cmd->cc_revmap_callback =       cli_nvsd_name_resolver_revmap;
     cmd->cc_revmap_names =          "/nkn/nvsd/network/config/resolver/cache-timeout";
     cmd->cc_revmap_type =           crt_auto;
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =	     "name-resolver cache-delete";
     cmd->cc_help_descr =	     N_("Cache entry removing options");
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =	     "name-resolver cache-delete all";
     cmd->cc_help_descr =	     N_("Deletes all the entries from the Cache");
     cmd->cc_flags =		     ccf_terminal;
     cmd->cc_capab_required =	     ccp_set_rstr_curr;
     cmd->cc_exec_operation =	     cxo_set;
     cmd->cc_exec_callback =         cli_nvsd_name_resolver_delete_all;
     CLI_CMD_REGISTER;

     CLI_CMD_NEW;
     cmd->cc_words_str =	     "name-resolver cache-delete *";
     cmd->cc_help_exp =		     N_("<fqdn>");
     cmd->cc_help_exp_hint =	     N_("Deletes the specified entry from the Cache");
     cmd->cc_flags =		     ccf_terminal;
     cmd->cc_capab_required =	     ccp_set_rstr_curr;
     cmd->cc_exec_operation =	     cxo_set;
     cmd->cc_exec_callback =         cli_nvsd_name_resolver_delete_entry;
     CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show network";
    cmd->cc_help_descr =            N_("Show network configuration");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_query_basic_curr;
    cmd->cc_exec_callback =         cli_nvsd_network_show;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "show name-resolver";
    cmd->cc_help_descr =	    N_("Show DNS configurations");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show name-resolver mode";
    cmd->cc_help_descr =            N_("Show DNS mode");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_query_basic_curr;
    cmd->cc_exec_callback =         cli_nvsd_name_resolver_show;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection socket";
    cmd->cc_help_descr =
        N_("Configure bind socket with interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection socket interface-bind";
    cmd->cc_help_descr =
        N_("Configure bind socket with interface");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection socket interface-bind enable";
    cmd->cc_help_descr =
          N_("Enable bind socket with interface");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/bond_if_cfg_enable";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "true";
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =	    cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection socket interface-bind disable";
    cmd->cc_help_descr =
          N_("Enable bind socket with interface");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/bond_if_cfg_enable";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_value =            "false";
    cmd->cc_revmap_type =           crt_none;
    CLI_CMD_REGISTER;
    /*---------------------------------------------------------------------*/

    /*---------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show name-resolver cache";
    cmd->cc_help_descr =            N_("Show DNS cache entries");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_query_basic_curr;
    cmd->cc_exec_callback =         cli_nvsd_name_resolver_cache_show;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show name-resolver cache *";
    cmd->cc_help_exp =             N_("<fqdn>");
    cmd->cc_help_exp_hint =        N_("Show DNS cache entries for a specific domain") ;
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_query_basic_curr;
    cmd->cc_magic          =        DNS_CACHE_FQDN ;
    cmd->cc_exec_callback =         cli_nvsd_name_resolver_cache_show;
    CLI_CMD_REGISTER;

    /*---------------------------------------------------------------------*/

    /*---------------------------------------------------------------------*/

    CLI_CMD_NEW;
    cmd->cc_words_str =              "network multicast";
    cmd->cc_help_descr =             N_("Configure IP multicast network options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network multicast igmp-version";
    cmd->cc_help_descr =            N_("Set the version of IGMP");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network multicast igmp-version *";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp =              N_("<igmp-version>");
    cmd->cc_help_exp_hint =         N_("IGMP version number: 1, 2 or 3");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/igmp-version";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection origin max-unresponsive-conns";
    cmd->cc_help_descr =            N_("Configure maximum number of unresposive connections");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "network connection origin max-unresponsive-conns *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("number of maximum unresponsive connections");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/network/config/connection/origin/max-unresponsive-conns";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

   /*---------------------------------------------------------------------*/

    err = cli_revmap_ignore_bindings_va(17,
	    "/nkn/nvsd/http/config/ipv6/enabled",
	    "/nkn/ssld/config/network/threads",
	    "/nkn/nvsd/network/config/connection/origin/failover/use_dns_response",
            "/nkn/nvsd/network/config/afr_fast_start",
            "/nkn/nvsd/http/config/om_conn_limit",
            "/nkn/nvsd/network/config/policy",
            "/nkn/nvsd/network/config/stack",
            "/nkn/nvsd/network/config/max_connections/no",
            "/nkn/nvsd/network/config/max_bandwidth/no",
	    "/nkn/nvsd/network/config/ip_conntrack_tcp_timeout_close_wait",
	    "/nkn/nvsd/network/config/ip_conntrack_tcp_timeout_time_wait",
	    "/nkn/nvsd/network/config/ip_conntrack_max_entries",
	    "/nkn/nvsd/network/config/max_connections/pmapper-disable",
	    "/nkn/nvsd/network/config/bond_if_cfg_enable",
	    "/nkn/l4proxy/config/tunnel-list/enable",
	    "/nkn/nvsd/network/config/dynamic-routing",
	    "/nkn/nvsd/network/config/ip-tables");

bail:
    return err;
}

int cli_nvsd_network_acl_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components,
        const tstring *name_last_part,
        const tstring *value,
        void *callback_data)
{
    int err = 0;
    const char *p = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    //cli_printf(_("Name: %s\n"), ts_str(name));

    p = ts_str(name);

    cli_printf_query(_(
                "  %s:\t"
                "#%s/address#\t"
                "#%s/mask#\n"), (const char*)(callback_data), p, p);

    return err;
}


static int cli_nvsd_network_show(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params)
{
	int err = 0;
	char *bn_name = NULL;
	char bn_mtupath[256];
	tbool t_syn_cookie = false;
	tbool t_use_dns = false;
	tbool t_mtupath_discovery = false;
        char bn_ip_forward[256];
        tbool t_ip_forward = false;

	err = cli_printf_query (
            _("Network time out (seconds)                   : "
			"#/nkn/nvsd/network/config/timeout#\n"));
        bail_error(err);

    /* FIX 1241 - Change from "network max connections" to 
     * "network concurrent sessions"
     */
	err = cli_printf_query (
            _("Maximum concurrent sessions                  : "
			"#/nkn/nvsd/network/config/max_connections#\n"));
        bail_error(err);

	err = cli_printf_query (
            _("Maximum origin queue delay                   : "
			"#/nkn/nvsd/network/config/connection/origin/queue/max_delay#\n"));
        bail_error(err);

	err = cli_printf_query (
            _("Maximum origin queue number                  : "
			"#/nkn/nvsd/network/config/connection/origin/queue/max_num#\n"));
        bail_error(err);

	err = cli_printf_query (
            _("Maximum origin unresponsive connections      : "
			"#/nkn/nvsd/network/config/connection/origin/max-unresponsive-conns#\n"));
        bail_error(err);

	/* Origin connection timeout and Use dns response configs */
	bn_name = smprintf("/nkn/nvsd/network/config/connection/origin/failover/use_dns_response");
	bail_null(bn_name);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
                                        &t_use_dns, bn_name, NULL);
	bail_error(err);

	if (t_use_dns){
		err = cli_printf_query (
			_("Origin server failover support               : "
			"Enabled\n"));
		bail_error(err);
	}else{
		err = cli_printf_query (
			_("Origin server failover support               : "
			"Disabled\n"));
		bail_error(err);
	}

	err = cli_printf_query (
			_("Origin server connection timeout value       : "
			"#/nkn/nvsd/network/config/connection/origin/connect/timeout#\n"));
	bail_error(err);
	/* end of Use DNS in response                             */

	err = cli_printf_query (
            _("Per Session assured flow rate (Kbits/sec)    : "
			"#/nkn/nvsd/network/config/assured_flow_rate#\n"));
        bail_error(err);

	//err = cli_printf_query (_("network session afr fast start(sec)   : "
			//"#/nkn/nvsd/network/config/afr_fast_start#\n"));
        //bail_error(err);

	err = cli_printf_query (
            _("Per Session Maximum bandwidth (Kbits/sec)    : "
			"#/nkn/nvsd/network/config/max_bandwidth#\n"));
        bail_error(err);

	err = cli_printf_query (
	    _("Bind Socket to Interface                     : "
			"#/nkn/nvsd/network/config/bond_if_cfg_enable#\n"));
	bail_error(err);

	bn_name = smprintf("/nkn/nvsd/network/config/syn-cookie");
    	bail_null(bn_name);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
					&t_syn_cookie, bn_name, NULL);		
	bail_error(err);

	if (t_syn_cookie){
		err = cli_printf_query (
            	    _("Syn-Cookie                                   : "
                        "Enabled\n"));
        	bail_error(err);

		err = cli_printf_query (
		    _("Syn-Cookie Half-Open-Connection   	     : "
                        "#/nkn/nvsd/network/config/syn-cookie/half_open_conn#\n"));
        	bail_error(err);
	}else{
                err = cli_printf_query (
                    _("Syn-Cookie                                   : "
                        "Disabled\n"));
                bail_error(err);
	}

        err = cli_printf_query (
            _("Max-Orphan                                   : "
                        "#/nkn/nvsd/network/config/tcp_max_orphans#\n"));
        bail_error(err);

        err = cli_printf_query (
            _("FIN-Timeout                                  : "
                        "#/nkn/nvsd/network/config/tcp_fin_timeout#\n"));
        bail_error(err);

        err = cli_printf_query (
            _("MAX-Buckets                                  : "
                        "#/nkn/nvsd/network/config/tcp_max_tw_buckets#\n"));
        bail_error(err);

        err = cli_printf_query (
            _("TCP Memory[low, high, maxpage]               : "
			"#/nkn/nvsd/network/config/tcp_mem/low#   "
			"#/nkn/nvsd/network/config/tcp_mem/high#  "
                        "#/nkn/nvsd/network/config/tcp_mem/maxpage#\n"));
        bail_error(err);

        err = cli_printf_query (
            _("TCP Read Memory[min, default, max]           : "
                        "#/nkn/nvsd/network/config/tcp_rmem/min#   "
                        "#/nkn/nvsd/network/config/tcp_rmem/default#  "
                        "#/nkn/nvsd/network/config/tcp_rmem/max#\n"));
        bail_error(err);

        err = cli_printf_query (
            _("TCP Write Memory[min, default, max]          : "
                        "#/nkn/nvsd/network/config/tcp_wmem/min#   "
                        "#/nkn/nvsd/network/config/tcp_wmem/default#  "
                        "#/nkn/nvsd/network/config/tcp_wmem/max#\n"));
        bail_error(err);

	snprintf(bn_mtupath, 256, "%s", "/nkn/nvsd/network/config/path-mtu-discovery");
	bail_null(bn_mtupath);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
                                        &t_mtupath_discovery, bn_mtupath, NULL);
        bail_error(err);

	if (t_mtupath_discovery){
		err = cli_printf_query (
		    _("PATH-MTU discovery                           : "
		        "Enabled\n"));
	}else{
		err = cli_printf_query (
            _("PATH-MTU discovery                           : "
                        "Disabled\n"));
	}

        snprintf(bn_ip_forward, 256, "%s", "/nkn/nvsd/network/config/ip-forward");
        bail_null(bn_ip_forward);

        err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
                                        &t_ip_forward, bn_ip_forward, NULL);
        bail_error(err);

        if (t_ip_forward){
                err = cli_printf_query (_("IP Forward                                   : Enabled\n"));
        }else{
                err = cli_printf_query (_("IP Forward                                   : Disabled\n"));
        }

	err = cli_nvsd_interface_txqueuelen_show(data, cmd, cmd_line, params);
	bail_error(err);

    err = cli_printf_query (_("IP Tables Enabled                            : "
                    "#/nkn/nvsd/network/config/ip-tables#\n"));
    bail_error(err);

    err = cli_printf_query (_("IGMP Version                                 : "
                    "#/nkn/nvsd/network/config/igmp-version#\n"));

    err = cli_printf_query (_("T-proxy port-range                           : "
                    "#/nkn/nvsd/network/config/connection/transparent-proxy/port_range_min# " 
                    "#/nkn/nvsd/network/config/connection/transparent-proxy/port_range_max#\n"));
    bail_error(err);

bail:
	safe_free(bn_name);
	return err;
}

static int cli_nvsd_interface_txqueuelen_show (
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params)
{
	int err = 0;
        tstr_array *names = NULL;
        uint32 num_names = 0;
        uint32 idx = 0;
    	char *binding_name = NULL;
	tstring *t_devsource = NULL;
	tstring *output = NULL;
	int32 ret_status = 0;
	const char *intf_name = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	err = cli_interface_get_names(&names, true);
	bail_error_null(err, names);

    	num_names = tstr_array_length_quick(names);

    	if (num_names > 0) {
        	for(idx = 0; idx < num_names; ++idx) {
			intf_name = tstr_array_get_str_quick(names, idx);
			binding_name = smprintf("/net/interface/state/%s/devsource", intf_name);
			if(binding_name) {
				err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &t_devsource, binding_name, NULL);
				bail_error(err);
				if( t_devsource != NULL) {
					if((ts_equal_str(t_devsource, "physical", false)) || (ts_equal_str(t_devsource, "bond", false))) {
						err = lc_launch_quick_status(&ret_status, &output, true, 2,
								"/opt/nkn/bin/get_interface_txqueuelen.sh", intf_name);
						if(ret_status) {
							goto bail;
						}
						if( output != NULL){
							cli_printf(_("TXQUEUELEN of Interface %-20s : %s"),
									intf_name, ts_str(output));
						}
					}
				}

			}
			safe_free(binding_name);
		}
	}
	 

bail:
	ts_free(&t_devsource);
	ts_free(&output);
        tstr_array_free(&names);
	return err;
}

int 
cli_interface_get_names(
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
             "/net/interface/config", NULL);
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
/* ------------------------------------------------------------------------ */
int
cli_print_restart_msg(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;

    err = cli_std_exec(data, cmd, cmd_line, params);
    bail_error(err);

    /* Print the restart message */
    err = cli_printf("WARNING: if command successful, please use service restart mod-delivery command for changes to take effect\n");
    bail_error(err);

 bail:
    return(err);
}

/*Handle to set the third parameter*/
int cli_nvsd_tcp_exec(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params)
{
    int err = 0;
    uint32 code = 0;
    tstring *msg = NULL;
    const char *thirdparam = NULL;
    const char *secondparam = NULL;
    const char *firstparam = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    firstparam = tstr_array_get_str_quick(params, 0);
    secondparam = tstr_array_get_str_quick(params, 1);    
    thirdparam = tstr_array_get_str_quick(params, 2);
    bail_null(thirdparam);
    bail_null(firstparam);
    bail_null(secondparam);

    err = mdc_set_binding(cli_mcc,
                &code,
                &msg,
                bsso_modify,
                bt_uint32,
                firstparam,
                "/nkn/nvsd/network/config/tcp_mem/low");
    bail_error(err);

    err = mdc_set_binding(cli_mcc,
                &code,
                &msg,
                bsso_modify,
                bt_uint32,
                secondparam,
                "/nkn/nvsd/network/config/tcp_mem/high");
    bail_error(err);

    err = mdc_set_binding(cli_mcc,
                &code,
                &msg,
                bsso_modify,
                bt_uint32,
                thirdparam,
                "/nkn/nvsd/network/config/tcp_mem/maxpage");
    bail_error(err);

    err = cli_printf("warning: if command successful, please restart mod-delivery service for change to take effect . If not the values may be partially changed. Reapply the command correctly\n");
    bail_error(err);

bail:
	ts_free(&msg);
	return err;
}

/*Handle to set the third parameter*/
int cli_nvsd_tcp_write_exec(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params)
{
    int err = 0;
    uint32 code = 0;
    tstring *msg = NULL;
    const char *thirdparam = NULL;
    const char *secondparam = NULL;
    const char *firstparam = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    firstparam = tstr_array_get_str_quick(params, 0);
    secondparam = tstr_array_get_str_quick(params, 1);
    thirdparam = tstr_array_get_str_quick(params, 2);
    bail_null(thirdparam);
    bail_null(firstparam);
    bail_null(secondparam);

    err = mdc_set_binding(cli_mcc,
                &code,
                &msg,
                bsso_modify,
                bt_uint32,
                firstparam,
                "/nkn/nvsd/network/config/tcp_wmem/min");
    bail_error(err);

    err = mdc_set_binding(cli_mcc,
                &code,
                &msg,
                bsso_modify,
                bt_uint32,
                secondparam,
                "/nkn/nvsd/network/config/tcp_wmem/default");
    bail_error(err);

    err = mdc_set_binding(cli_mcc,
                &code,
                &msg,
                bsso_modify,
                bt_uint32,
                thirdparam,
                "/nkn/nvsd/network/config/tcp_wmem/max");
    bail_error(err);

    err = cli_printf("warning: if command successful, please restart mod-delivery service for change to take effect. If not the values may be partially changed. Reapply the command correctly\n");
    bail_error(err);

bail:
        ts_free(&msg);
        return err;
}

/*Handle to set the third parameter*/
int cli_nvsd_tcp_read_exec(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params)
{
    int err = 0;
    uint32 code = 0;
    tstring *msg = NULL;
    const char *thirdparam = NULL;
    const char *secondparam = NULL;
    const char *firstparam = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    firstparam = tstr_array_get_str_quick(params, 0);
    secondparam = tstr_array_get_str_quick(params, 1);
    thirdparam = tstr_array_get_str_quick(params, 2);
    bail_null(thirdparam);
    bail_null(firstparam);
    bail_null(secondparam);

    err = mdc_set_binding(cli_mcc,
                &code,
                &msg,
                bsso_modify,
                bt_uint32,
                firstparam,
                "/nkn/nvsd/network/config/tcp_rmem/min");
    bail_error(err);

    err = mdc_set_binding(cli_mcc,
                &code,
                &msg,
                bsso_modify,
                bt_uint32,
                secondparam,
                "/nkn/nvsd/network/config/tcp_rmem/default");
    bail_error(err);

    err = mdc_set_binding(cli_mcc,
                &code,
                &msg,
                bsso_modify,
                bt_uint32,
                thirdparam,
                "/nkn/nvsd/network/config/tcp_rmem/max");
    bail_error(err);

    err = cli_printf("warning: if command successful, please restart mod-delivery service for change to take effect. If not the values may be partially changed. Reapply the command correctly\n");
    bail_error(err);

bail:
        ts_free(&msg);
        return err;
}

/*Reseting the tcp parameters*/
int cli_nvsd_tcp_reset(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params)
{
    int err = 0;
    uint32 code = 0;
    tstring *msg = NULL;
    
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    if (cmd->cc_magic == cid_tcp_mem)
    {
    	err = mdc_set_binding(cli_mcc,
        	        &code,
                	&msg,
                	bsso_modify,
                	bt_uint32,
                	"196608",
                	"/nkn/nvsd/network/config/tcp_mem/low");
    	bail_error(err);

    	err = mdc_set_binding(cli_mcc,
        	        &code,
                	&msg,
             	   	bsso_modify,
                	bt_uint32,
                	"262144",
                	"/nkn/nvsd/network/config/tcp_mem/high");
    	bail_error(err);

    	err = mdc_set_binding(cli_mcc,
        	        &code,
                	&msg,
                	bsso_modify,
            	    	bt_uint32,
                	"1572864",
                	"/nkn/nvsd/network/config/tcp_mem/maxpage");
   	bail_error(err);
    }
    else if (cmd->cc_magic == cid_tcp_rmem)
    {
    	        err = mdc_set_binding(cli_mcc,
                        &code,
                        &msg,
                        bsso_modify,
                        bt_uint32,
                        "4096",
                        "/nkn/nvsd/network/config/tcp_rmem/min");
        bail_error(err);

        err = mdc_set_binding(cli_mcc,
                        &code,
                        &msg,
                        bsso_modify,
                        bt_uint32,
                        "87380",
                        "/nkn/nvsd/network/config/tcp_rmem/default");
        bail_error(err);

        err = mdc_set_binding(cli_mcc,
                        &code,
                        &msg,
                        bsso_modify,
                        bt_uint32,
                        "16777216",
                        "/nkn/nvsd/network/config/tcp_rmem/max");
        bail_error(err);

    }
    else if (cmd->cc_magic == cid_tcp_wmem)
    {
	err = mdc_set_binding(cli_mcc,
                        &code,
                        &msg,
                        bsso_modify,
                        bt_uint32,
                        "4096",
                        "/nkn/nvsd/network/config/tcp_wmem/min");
        bail_error(err);

        err = mdc_set_binding(cli_mcc,
                        &code,
                        &msg,
                        bsso_modify,
                        bt_uint32,
                        "65536",
                        "/nkn/nvsd/network/config/tcp_wmem/default");
        bail_error(err);

        err = mdc_set_binding(cli_mcc,
                        &code,
                        &msg,
                        bsso_modify,
                        bt_uint32,
                        "16777216",
                        "/nkn/nvsd/network/config/tcp_wmem/max");
        bail_error(err);

    }

bail:
        ts_free(&msg);
        return err;
}

static int 
cli_nvsd_port_range_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *port_range_min = NULL;
    tstring *port_range_max = NULL;
    char *cmd_str = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    err = tstr_array_append_sprintf (ret_reply->crr_nodes, "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_min");
    bail_error (err);

    err = tstr_array_append_sprintf (ret_reply->crr_nodes, "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_max");
    bail_error (err);

    err = bn_binding_array_get_value_tstr_by_name (bindings, "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_min", NULL,
    &port_range_min);
    bail_error_null (err, port_range_min);

    err = bn_binding_array_get_value_tstr_by_name (bindings, "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_max", NULL,
    &port_range_max);
    bail_error_null (err, port_range_max);

    cmd_str = smprintf ("network connection transparent-proxy port-range %s %s", ts_str (port_range_min), ts_str (port_range_max));
    bail_null (cmd_str);

    err = tstr_array_append_str_takeover (ret_reply->crr_cmds, &cmd_str);
    bail_error (err);

bail:
    ts_free (&port_range_min);
    ts_free (&port_range_max);
    safe_free (cmd_str);

    return err;
}

static int 
cli_nvsd_tcp_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *low = NULL;
    tstring *maxpage = NULL;
    tstring *high = NULL;
    char *cmd_str = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/network/config/tcp_mem/low");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/network/config/tcp_mem/maxpage");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/network/config/tcp_mem/high");
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings,
                "/nkn/nvsd/network/config/tcp_mem/low", NULL,
                &low);
    bail_error_null(err, low);

    err = bn_binding_array_get_value_tstr_by_name(bindings,
                "/nkn/nvsd/network/config/tcp_mem/maxpage", NULL,
                &maxpage);
    bail_error_null(err, maxpage);

    err = bn_binding_array_get_value_tstr_by_name(bindings,
                "/nkn/nvsd/network/config/tcp_mem/high", NULL,
                &high);
    bail_error_null(err, high);

    cmd_str = smprintf("network tcp memory low %s high %s maxpage %s", ts_str(low), ts_str(high), ts_str(maxpage));
    bail_null(cmd_str);

    err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
    bail_error(err);

bail:
    ts_free(&low);
    ts_free(&high);
    ts_free(&maxpage);
    safe_free(cmd_str);
    return err;

}


static int
cli_nvsd_tcpr_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *min = NULL;
    tstring *def = NULL;
    tstring *max = NULL;
    char *cmd_str = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/network/config/tcp_rmem/min");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/network/config/tcp_rmem/default");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/network/config/tcp_rmem/max");
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings,
                "/nkn/nvsd/network/config/tcp_rmem/min", NULL,
                &min);
    bail_error_null(err, min);

    err = bn_binding_array_get_value_tstr_by_name(bindings,
                "/nkn/nvsd/network/config/tcp_rmem/default", NULL,
                &def);
    bail_error_null(err, def);

    err = bn_binding_array_get_value_tstr_by_name(bindings,
                "/nkn/nvsd/network/config/tcp_rmem/max", NULL,
                &max);
    bail_error_null(err, max);

    cmd_str = smprintf("network tcp read-memory min %s default %s max %s", ts_str(min), ts_str(def), ts_str(max));
    bail_null(cmd_str);

    err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
    bail_error(err);

bail:
    ts_free(&min);
    ts_free(&def);
    ts_free(&max);
    safe_free(cmd_str);
    return err;

}

static int
cli_nvsd_tcpw_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *min = NULL;
    tstring *def = NULL;
    tstring *max = NULL;
    char *cmd_str = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(binding);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/network/config/tcp_wmem/min");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/network/config/tcp_wmem/default");
    bail_error(err);

    err = tstr_array_append_sprintf(ret_reply->crr_nodes, "/nkn/nvsd/network/config/tcp_wmem/max");
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(bindings,
		"/nkn/nvsd/network/config/tcp_wmem/min", NULL,
		&min);
    bail_error_null(err, min);

    err = bn_binding_array_get_value_tstr_by_name(bindings,
                "/nkn/nvsd/network/config/tcp_wmem/default", NULL,
                &def);
    bail_error_null(err, def);
  
    err = bn_binding_array_get_value_tstr_by_name(bindings,
                "/nkn/nvsd/network/config/tcp_wmem/max", NULL,
                &max);
    bail_error_null(err, max);
   
    cmd_str = smprintf("network tcp write-memory min %s default %s max %s", ts_str(min), ts_str(def), ts_str(max));
    bail_null(cmd_str);

    err = tstr_array_append_str_takeover(ret_reply->crr_cmds, &cmd_str);
    bail_error(err);

bail:
    ts_free(&min);
    ts_free(&def);
    ts_free(&max);
    safe_free(cmd_str);
    return err;
}


int cli_nvsd_name_resolver_cache_timeout(
		   void *data,
		    const cli_command *cmd,
		    const tstr_array *cmd_line,
		    const tstr_array *params)
{
	int err=0;
	const char *timeout_value = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	timeout_value = tstr_array_get_str_quick(params, 0);

	if(atoi(timeout_value) < 0)
	{
	    err = cli_printf(_("Negative value for cache timeout is not valid. \n"));
	    bail_error(err);

	    goto bail;
	}

   	err = mdc_set_binding(cli_mcc,
        	        NULL,
                	NULL,
                	bsso_modify,
                	bt_int32,
                	timeout_value,
                	"/nkn/nvsd/network/config/resolver/cache-timeout");
    bail_error(err);



bail:

return err;
}

int cli_nvsd_name_resolver_delete_all(
		   void *data,
		    const cli_command *cmd,
		    const tstr_array *cmd_line,
		    const tstr_array *params)
{
	int err=0;
	unsigned int ret_err=0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

    err = mdc_send_action (cli_mcc, &ret_err, NULL,
            "/nkn/nvsd/network/actions/del_dns_cache_all");
    bail_error (err);


bail:

return err;
}

int cli_nvsd_name_resolver_delete_entry(
		   void *data,
		    const cli_command *cmd,
		    const tstr_array *cmd_line,
		    const tstr_array *params)
{
	int err=0;
	unsigned int ret_err=0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	err = mdc_send_action_with_bindings_str_va (cli_mcc, &ret_err, NULL,
			"/nkn/nvsd/network/actions/del_dns_cache_entry", 1, "domain", bt_string,
			tstr_array_get_str_quick(params, 0));
	bail_error (err);

bail:

return err;
}

int cli_nvsd_name_resolver_cache_show(
		   void *data,
		    const cli_command *cmd,
		    const tstr_array *cmd_line,
		    const tstr_array *params)
{
	int err=0;
	uint32 ret_err = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

    bn_binding_array *dns_cache_array = NULL;
    uint32 binding_array_length = 0;
    tbool rechecked_licenses = false;
    const char *firstparam = NULL;

    err = cli_printf(_("DNS Cache Entries \n"));
    bail_error(err);

    err = cli_printf(_(" %30s   %-60s   %8s\n"),"DomainName", "IPAddress(Time-out) list", "Hit-count");
    bail_error(err);

    if(cmd->cc_magic == DNS_CACHE_FQDN) {
          firstparam = tstr_array_get_str_quick(params, 0);
          bail_null(firstparam);

         /* Trigger the action to get the DNS Cache list */
          err = mdc_send_action_with_bindings_and_results_va (cli_mcc, &ret_err, NULL,
                        "/nkn/nvsd/network/actions/get_dns_cache_fqdn",
                        &dns_cache_array, 1, "domain", bt_string,
                        firstparam);

    } else {
         /* Trigger the action to get the DNS Cache list */
          err = mdc_send_action_with_bindings_and_results (cli_mcc, &ret_err, NULL,
                   "/nkn/nvsd/network/actions/get_dns_cache", NULL, &dns_cache_array);
    }

    bail_error (err);

    if (ret_err != 0)
    {
         cli_printf_error(_("error: failed to get DNS Cache Entries\n"));
	 goto bail;
    }
    err = bn_binding_array_length( dns_cache_array, &binding_array_length);
    bail_error (err);

	if (binding_array_length > 1)
	{
		err = bn_binding_array_foreach(dns_cache_array, cli_nvsd_dns_cache_list,
                    &rechecked_licenses);
		bail_error(err);
	}
	else
	{
	   if(cmd->cc_magic == DNS_CACHE_FQDN){
		   cli_printf(_("DNS Entry for %s is not exist in the cache\n"), firstparam);
	   }
	   else
	   {
	   		cli_printf(_("DNS Cache is Empty\n"));
	   }
	}

bail:
bn_binding_array_free(&dns_cache_array);
return err;
}

int cli_nvsd_name_resolver_show(
		   void *data,
		    const cli_command *cmd,
		    const tstr_array *cmd_line,
		    const tstr_array *params)
{
	int err=0;
	char *bn_adns = NULL;
	char *bn_adnsd = NULL;
	tbool t_async_dns = false;
	tbool t_adnsd_enabled = false;
	tstring *adns_timeout = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	bn_adns = smprintf("/nkn/nvsd/network/config/resolver/async-enable");
    	bail_null(bn_adns);

	err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
					&t_async_dns, bn_adns, NULL);
	bail_error(err);

	if (t_async_dns){
		err = cli_printf_query (
            	    _("Asyncronous (non-blocking) DNS resolution    : "
                        "Enabled\n"));
        	bail_error(err);
	}else{
		err = cli_printf_query (
            	    _("Asyncronous (non-blocking) DNS resolution    : "
                        "Disabled\n"));
        	bail_error(err);
	}

        bn_adnsd = smprintf("/nkn/nvsd/network/config/resolver/adnsd-enabled");
        bail_null(bn_adnsd);

        err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL,
                                        &t_adnsd_enabled, bn_adnsd, NULL);
        bail_error(err);

        if (t_adnsd_enabled){
                err = cli_printf_query (
                    _("Asyncronous (non-blocking) DNS Daemon        : Enabled\n"));
                bail_error(err);
        }else{
                err = cli_printf_query (
                    _("Asyncronous (non-blocking) DNS Daemon        : Disabled\n"));
                bail_error(err);
        }


    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &adns_timeout, NULL, "/nkn/nvsd/network/config/resolver/cache-timeout");
	bail_error(err);

	if (ts_equal_str(adns_timeout, "-1", true)) {
		err = cli_printf_query (
            	    _("Cache timeout for entries in the DNS cache   : "
                        "Random\n"));
        	bail_error(err);
	}
	else if (ts_equal_str(adns_timeout, "-100", true)) {
		err = cli_printf_query (
            	    _("Cache timeout for entries in the DNS cache   : "
                        "auto\n"));
        	bail_error(err);
	}
	else
	{
        err = cli_printf_query (
					_("Cache timeout for entries in the DNS cache   : "
                         "#/nkn/nvsd/network/config/resolver/cache-timeout#\n"));
        bail_error(err);

	}

bail:
	safe_free(bn_adns);
	ts_free(&adns_timeout);
	return err;
}

static int
cli_nvsd_name_resolver_revmap(void *data, const cli_command *cmd,
                    const bn_binding_array *bindings, const char *name,
                    const tstr_array *name_parts, const char *value,
                    const bn_binding *binding,
                    cli_revmap_reply *ret_reply)
{
    int err = 0;
    int	timeout = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(binding);
    if (value) {
	timeout = atoi(value);

	err = tstr_array_append_str(ret_reply->crr_nodes, name);
	bail_error(err);

	if (timeout == -1) {
	    err = tstr_array_append_sprintf(ret_reply->crr_cmds,
		    "name-resolver cache-timeout random");
	    bail_error(err);
	} else if (timeout == -100) {
	    err = tstr_array_append_sprintf(ret_reply->crr_cmds,
		    "name-resolver cache-timeout auto");
	    bail_error(err);
	} else {
	    err = tstr_array_append_sprintf(ret_reply->crr_cmds,
		    "name-resolver cache-timeout %d", timeout);
	    bail_error(err);
	}
    }

bail:
    return err;
}

static int cli_nvsd_dns_cache_list(const bn_binding_array *arr, uint32 idx,
        		bn_binding *binding, void *data)
{
	int err = 0;
	tstr_array *tok_array = NULL;
	char *dns_entry = NULL;

	UNUSED_ARGUMENT(arr);
	UNUSED_ARGUMENT(idx);
	UNUSED_ARGUMENT(data);

	err = bn_binding_get_str(binding, ba_value, bt_string, 0, &dns_entry);
	bail_error(err);
	if (strlen(dns_entry) <= 0) {
		goto bail;
	}
	err = ts_tokenize_str(dns_entry, ';', 0, 0, 0, &tok_array);
	bail_error(err);

	err = cli_printf(_(" %30s   %-60s   %8s\n"), ts_str( tstr_array_get_quick(tok_array, 0)),
    		ts_str( tstr_array_get_quick(tok_array, 1)), ts_str( tstr_array_get_quick(tok_array, 2)));
	bail_error(err);


	bail:
	safe_free(dns_entry);
	tstr_array_free(&tok_array);
	return(err);

}
int cli_nvsd_network_connection_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(params);

    if(cli_prefix_modes_is_enabled()) {
        err = cli_prefix_enter_mode(cmd, cmd_line);
        bail_error(err);
    }
    else {
        err = cli_print_incomplete_command_error(cmd, cmd_line);
        bail_error(err);
    }
bail:
    return err;

}
