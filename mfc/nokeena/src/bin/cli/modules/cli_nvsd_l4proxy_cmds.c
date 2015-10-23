/*
 * Filename :   cli_nvsd_l4proxy_cmds.c
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
#include "nkn_mgmt_defs.h"

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
cli_nvsd_l4proxy_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

static int cli_nvsd_l4proxy_show(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);

/* 
 * DOMAIN_FILTER_FUTURE_REL mainly compiles-out the already added CLI
 * during developement but these would be needed in near future
 * mgmtd nodes will exists but unused
 * XXX-DON'T DEFINE IT IT YOU ARE NOT SURE.
 */
#ifdef DOMAIN_FILTER_FUTURE_REL
static int cli_nvsd_l4proxy_domain_show(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);
#endif

int
cli_nvsd_l4proxy_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str =		"domain-filter";
    cmd->cc_help_descr =	N_("Configure domain-filter options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"no domain-filter";
    cmd->cc_help_descr =	N_("Reset/Clear domain-filter options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"show domain-filter";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_help_descr = 	N_("Display domain-filter configurations");
    cmd->cc_exec_callback =     cli_nvsd_l4proxy_show;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "domain-filter enable";
    cmd->cc_help_descr =            N_("Enables domain-filter. Please do 'pm process nvsd restart' & 'pm process proxyd restart'");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/l4proxy/config/enable";
    cmd->cc_exec_value =	    "true";
    cmd->cc_exec_type =		    bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "domain-filter disable";
    cmd->cc_help_descr =            N_("Disables domain-filter. Please do 'pm process nvsd restart' & 'pm process proxyd restart'");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/l4proxy/config/enable";
    cmd->cc_exec_value =	    "false";
    cmd->cc_exec_type =		    bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =		"domain-filter tunnel-list";
    cmd->cc_help_descr =	N_("Tunnel all the requests");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"no domain-filter tunnel-list";
    cmd->cc_help_descr =	N_("Resets domain-filter tunnel-list configuration ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"domain-filter cache-fail";
    cmd->cc_help_descr =	N_("Action to take when Cache-Engine is not available ");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	    =	"domain-filter tunnel-list all";
    cmd->cc_help_descr	    =	"Turn on tunneling all the incoming connections";
    cmd->cc_flags	    =	ccf_terminal;
    cmd->cc_capab_required  =	ccp_set_rstr_curr;
    cmd->cc_exec_operation  =	cxo_set;
    cmd->cc_exec_name	    =	"/nkn/nvsd/l4proxy/config/tunnel-list/enable";
    cmd->cc_exec_value	    =	"true";
    cmd->cc_exec_type	    =	bt_bool;
    cmd->cc_revmap_type	    =	crt_auto;
    cmd->cc_revmap_order    =	cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	    =	"no domain-filter tunnel-list all";
    cmd->cc_help_descr	    =	"Turn off tunneling for all the incoming connections";
    cmd->cc_flags	    =	ccf_terminal;
    cmd->cc_capab_required  =	ccp_set_rstr_curr;
    cmd->cc_exec_operation  =	cxo_reset;
    cmd->cc_exec_name	    =	"/nkn/nvsd/l4proxy/config/tunnel-list/enable";
    cmd->cc_exec_type	    =	bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	    =	"domain-filter cache-fail tunnel";
    cmd->cc_help_descr	    =	"Tunnel connection to origin";
    cmd->cc_flags	    =	ccf_terminal;
    cmd->cc_capab_required  =	ccp_set_rstr_curr;
    cmd->cc_exec_operation  =	cxo_set;
    cmd->cc_exec_name	    =	"/nkn/nvsd/l4proxy/config/cache-fail/action";
    cmd->cc_exec_value	    =	"0";
    cmd->cc_exec_type	    =	bt_uint32;
    cmd->cc_revmap_type	    =	crt_auto;
    cmd->cc_revmap_order    =	cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	    =	"domain-filter cache-fail reject";
    cmd->cc_help_descr	    =	"Reject the connection on cache-failure";
    cmd->cc_flags	    =	ccf_terminal;
    cmd->cc_capab_required  =	ccp_set_rstr_curr;
    cmd->cc_exec_operation  =	cxo_set;
    cmd->cc_exec_name	    =	"/nkn/nvsd/l4proxy/config/cache-fail/action";
    cmd->cc_exec_value	    =	"1";
    cmd->cc_exec_type	    =	bt_uint32;
    cmd->cc_revmap_type	    =	crt_auto;
    cmd->cc_revmap_order    =	cro_mgmt;
    CLI_CMD_REGISTER;


#ifdef DOMAIN_FILTER_FUTURE_REL
    CLI_CMD_NEW;
    cmd->cc_words_str =		"l4proxy domain white-list";
    cmd->cc_help_descr =	N_("Configure l4proxy white-list ip addresses");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		"show l4proxy domain";
    cmd->cc_help_descr = 	N_("Display L4proxy domain configurations");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	    =	"show l4proxy domain white-list";
    cmd->cc_help_descr	    =	"Display l4proxy white-list domains";
    cmd->cc_flags	    =	ccf_terminal;
    cmd->cc_capab_required  =	ccp_query_basic_curr;
    cmd->cc_exec_callback   =	cli_nvsd_l4proxy_domain_show;
    /* 1 means white-list */
    cmd->cc_exec_data	    =	(void *)1;
    cmd->cc_revmap_type	    =	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str	    =	"show l4proxy domain black-list";
    cmd->cc_help_descr	    =	"Display l4proxy black-list domains";
    cmd->cc_flags	    =	ccf_terminal;
    cmd->cc_capab_required  =	ccp_query_basic_curr;
    cmd->cc_exec_callback   =	cli_nvsd_l4proxy_domain_show;
    /* 0 means black-list */
    cmd->cc_exec_data	    =	(void *)0;
    cmd->cc_revmap_type	    =	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy domain white-list enable";
    cmd->cc_help_descr =            N_("Enables l4proxy white-list");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/l4proxy/config/wlist/enable";
    cmd->cc_exec_value =	    "true";
    cmd->cc_exec_type =		    bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy domain white-list disable";
    cmd->cc_help_descr =            N_("Disables l4proxy white-list ");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/l4proxy/config/wlist/enable";
    cmd->cc_exec_value =	    "false";
    cmd->cc_exec_type =		    bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy domain black-list enable";
    cmd->cc_help_descr =            N_("Enables l4proxy black-list");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/l4proxy/config/blist/enable";
    cmd->cc_exec_value =	    "true";
    cmd->cc_exec_type =		    bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy domain black-list disable";
    cmd->cc_help_descr =            N_("Disables l4proxy black-list");
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/l4proxy/config/blist/enable";
    cmd->cc_exec_value =	    "false";
    cmd->cc_exec_type =		    bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy domain white-list threshold";
    cmd->cc_help_descr =	    "set threashold value for white-list";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy domain white-list threshold *";
    cmd->cc_help_exp_hint =         "set threshold value for white-list";
    cmd->cc_help_exp =	            "<number>";
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/l4proxy/config/wlist/threshold";
    cmd->cc_exec_value =	    "$1$";
    cmd->cc_exec_type =		    bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy domain white-list domain-ip";
    cmd->cc_help_descr =	    "set domain-ip for white-list";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy domain white-list domain-ip *";
    cmd->cc_help_exp_hint =	    "add domain-ip to l4proxy white-list";
    cmd->cc_help_exp =		    "<number>";
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_create;
    cmd->cc_exec_name =		    "/nkn/nvsd/l4proxy/config/wlist/ip/$1$";
    cmd->cc_exec_value =	    "$1$"; /* 1 means whitelist */
    cmd->cc_exec_type =		    bt_string;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str		=   "no l4proxy domain ";
    cmd->cc_help_descr		=   "Reset l4proxy settings ";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str		=   "no l4proxy domain white-list";
    cmd->cc_help_descr		=   "delete white-list domain ip";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str		=   "no l4proxy domain white-list domain-ip";
    cmd->cc_help_descr		=   "delete white-list domain ip";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no l4proxy domain white-list domain-ip *";
    cmd->cc_help_exp =	            "add domain-ip to l4proxy white-list";
    cmd->cc_help_exp_hint =	    "<number>";
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_delete;
    cmd->cc_exec_name =		    "/nkn/nvsd/l4proxy/config/wlist/ip/$1$";
    cmd->cc_exec_type =		    bt_string;
    cmd->cc_revmap_type =           crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy domain black-list domain-ip";
    cmd->cc_help_descr =	    "set domain-ip for black-list";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy domain black-list domain-ip *";
    cmd->cc_help_exp_hint =	    "add domain-ip to l4proxy black-list";
    cmd->cc_help_exp =		    "<number>";
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_create;
    cmd->cc_exec_name =		    "/nkn/nvsd/l4proxy/config/blist/ip/$1$";
    cmd->cc_exec_value =	    "$1$"; /* 1 means whitelist */
    cmd->cc_exec_type =		    bt_string;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str		=   "no l4proxy domain black-list";
    cmd->cc_help_descr		=   "delete black-list domain ip";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str		=   "no l4proxy domain black-list domain-ip";
    cmd->cc_help_descr		=   "delete black-list domain ip";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no l4proxy domain black-list domain-ip *";
    cmd->cc_help_exp =	            "add domain-ip to l4proxy black-list";
    cmd->cc_help_exp_hint =	    "<number>";
    cmd->cc_flags =		    ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_delete;
    cmd->cc_exec_name =		    "/nkn/nvsd/l4proxy/config/blist/ip/$1$";
    cmd->cc_exec_type =		    bt_string;
    cmd->cc_revmap_type =           crt_none;
    CLI_CMD_REGISTER;
    /* XXX - Do we need following commands */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy use-client-ip";
    cmd->cc_help_descr =            N_("Configure l4proxy use-client-ip");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy use-client-ip *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("1: enable use-client-ip spoofing, 0: disable spoofing ip.");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/l4proxy/config/use_client_ip";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy del-ip-list";
    cmd->cc_help_descr =            N_("Configure l4proxy del_ip_list");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy del-ip-list *";
    cmd->cc_help_exp =              N_("0.0.0.0");
    cmd->cc_help_exp_hint =         N_("Remove this IP address from cacheable and block ip list");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/l4proxy/config/del_ip_list";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy add-cacheable-ip-list";
    cmd->cc_help_descr =            N_("Configure l4proxy add_cacheable_ip_list");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy add-cacheable-ip-list *";
    cmd->cc_help_exp =              N_("0.0.0.0");
    cmd->cc_help_exp_hint =         N_("add this destination IP address for forwarding to nvsd");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/l4proxy/config/add_cacheable_ip_list";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no l4proxy add-cacheable-ip-list";
    cmd->cc_help_descr =            N_("no Configure l4proxy add_cacheable_ip_list");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no l4proxy add-cacheable-ip-list *";
    cmd->cc_help_exp =              N_("0.0.0.0");
    cmd->cc_help_exp_hint =         N_("remove this IP address from cacheable ip list");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/nvsd/l4proxy/config/add_cacheable_ip_list";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_string;
    CLI_CMD_REGISTER;



    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy add-block-ip-list";
    cmd->cc_help_descr =            N_("Configure l4proxy add_block_ip_list");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "l4proxy add-block-ip-list *";
    cmd->cc_help_exp =              N_("0.0.0.0");
    cmd->cc_help_exp_hint =         N_("add this destination IP so MFC can block those traffic.");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/l4proxy/config/add_block_ip_list";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_mgmt;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no l4proxy add-block-ip-list";
    cmd->cc_help_descr =            N_("no Configure l4proxy add_block_ip_list");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no l4proxy add-block-ip-list *";
    cmd->cc_help_exp =              N_("0.0.0.0");
    cmd->cc_help_exp_hint =         N_("remove this IP address from block ip list");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/nvsd/l4proxy/config/add_block_ip_list";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_string;
    CLI_CMD_REGISTER;

#endif
    /*---------------------------------------------------------------------*/

    const char *domain_filter_ignore_bindings[] = {
	"/nkn/nvsd/l4proxy/config/*",
	"/nkn/nvsd/l4proxy/config/blist/enable",
	"/nkn/nvsd/l4proxy/config/blist/ip/*",
	"/nkn/nvsd/l4proxy/config/wlist/enable",
	"/nkn/nvsd/l4proxy/config/wlist/threshold",
	"/nkn/nvsd/l4proxy/config/wlist/ip/*",
	"/nkn/nvsd/l4proxy/config/use_client_ip",
	"/nkn/nvsd/l4proxy/config/add_block_ip_list",
	"/nkn/nvsd/l4proxy/config/add_cacheable_ip_list",
	"/nkn/nvsd/l4proxy/config/del_ip_list",
    };
    err = cli_revmap_ignore_bindings_arr(domain_filter_ignore_bindings);

bail:
    return err;
}

#ifdef DOMAIN_FILTER_FUTURE_REL
static int cli_nvsd_l4proxy_domain_show(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params)
{
    int err = 0;
    uint32 *white_list = (uint32 *)data, num_bindings = 0, i = 0;
    node_name_t node_name = {0};
    bn_binding_array *bindings = NULL;
    bn_binding *binding = NULL;
    tstring *t_value = NULL;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    if (white_list == 0) {
	/* this is black list */
	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/l4proxy/config/blist/ip/*");
    } else {
	/* this is white list */
	snprintf(node_name, sizeof(node_name), "/nkn/nvsd/l4proxy/config/wlist/ip/*");
    }
    err = mdc_get_binding_children(cli_mcc, NULL, NULL, true, &bindings, true, true,node_name);
    bail_error(err);

    err = bn_binding_array_length(bindings, &num_bindings);
    bail_error(err);
    if (num_bindings == 0) {
	/* no domain ip addresses to show */
	err = 0;
	goto bail;
    }
    cli_printf("L4 Proxy domain %s :\n", white_list? "white-list": "black-list");
    bn_binding_array_dump("DGAUTAM", bindings, LOG_NOTICE);
    for ( i = 0; i < num_bindings; i++) {
	binding = bn_binding_array_get_quick(bindings, i);
	if (binding == NULL) {
	    continue;
	}
	err = bn_binding_get_tstr(binding, ba_value, bt_NONE, NULL, &t_value);
	bail_error(err);
	cli_printf("  %s\n",  ts_str_maybe(t_value));
	ts_free(&t_value);
    }

bail:
    bn_binding_array_free(&bindings);
    return err;
}

#endif

static int cli_nvsd_l4proxy_show(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params)
{
	int err = 0;
	uint32 cache_fail = 0;
	bn_binding *binding = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	err = cli_printf_query (
            _("  Domain-Filter : #n:enable~/nkn/nvsd/l4proxy/config/enable#\n"));
        bail_error(err);

	err = cli_printf_query (
            _("  Tunnel All Connections : "
			"#n:enable~/nkn/nvsd/l4proxy/config/tunnel-list/enable#\n"));
        bail_error(err);

	err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding,
		"/nkn/nvsd/l4proxy/config/cache-fail/action", NULL);
	bail_error_null(err, binding);

	err = bn_binding_get_uint32(binding, ba_value, NULL, &cache_fail);
	bail_error(err);

	err = cli_printf_query (
            "  Cache-Fail Action : %s", cache_fail == 1 ? "Reject":"Tunnel" );
        bail_error(err);


bail:
	bn_binding_free(&binding);
	return err;
}
