/*
 * Filename :   cli_bgp_cmds.c
 *
 * (C) Copyright 2011 Juniper Networks.
 * All rights reserved.
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "proc_utils.h"
#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "libnkncli.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

#define VTYSH_PATH       "/usr/local/bin/vtysh"

typedef enum bgp_sense_ {
    BGP_DELETE = 0,
    BGP_ADD = 1
} bgp_sense_t;

const char *bgp_ignore_bindings[] = {
        "/nkn/bgp/config/local_as/*",
        "/nkn/bgp/config/local_as/*/neighbor/*",
        "/nkn/bgp/config/local_as/*/neighbor/*/shutdown",
        "/nkn/bgp/config/local_as/*/network/*",
        "/nkn/bgp/config/local_as/*/redist_conn",
        "/nkn/bgp/config/local_as/*/router_id",
        "/nkn/bgp/config/local_as/*/timer_ka",
        "/nkn/bgp/config/local_as/*/timer_hold",
       "/nkn/bgp/config/local_as/*/log_neighbor",
};
/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/

int cli_bgp_cmds_init(const lc_dso_info *info,
                      const cli_module_context *context);

int cli_bgp_network(void *data, const cli_command *cmd,
                    const tstr_array *cmd_line,
                    const tstr_array *params);

int cli_bgp_neighbor_shutdown_revmap(void *data, const cli_command *cmd,
                             const bn_binding_array *bindings, const char *name,
                             const tstr_array *name_parts, const char *value,
                             const bn_binding *binding,
                             cli_revmap_reply *ret_reply);

int cli_bgp_redist_conn_revmap(void *data, const cli_command *cmd,
                             const bn_binding_array *bindings, const char *name,
                             const tstr_array *name_parts, const char *value,
                             const bn_binding *binding,
                             cli_revmap_reply *ret_reply);

int cli_bgp_log_neighbor_revmap(void *data, const cli_command *cmd,
                             const bn_binding_array *bindings, const char *name,
                             const tstr_array *name_parts, const char *value,
                             const bn_binding *binding,
                             cli_revmap_reply *ret_reply);

int cli_bgp_router_id_revmap(void *data, const cli_command *cmd,
                             const bn_binding_array *bindings, const char *name,
                             const tstr_array *name_parts, const char *value,
                             const bn_binding *binding,
                             cli_revmap_reply *ret_reply);

int cli_bgp_timers_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int cli_show_bgp(void *arg,
        const cli_command *cmd,
        const tstr_array  *cmd_line,
        const tstr_array  *params);

int cli_clear_bgp(void *arg,
        const cli_command *cmd,
        const tstr_array  *cmd_line,
        const tstr_array  *params);

/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
cli_bgp_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router";
    cmd->cc_help_descr      = N_("Configure router");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router";
    cmd->cc_help_descr      = N_("Remove router configurations");
    CLI_CMD_REGISTER; 

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp";
    cmd->cc_help_descr      = N_("Configure BGP");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp";
    cmd->cc_help_descr      =  N_("Remove BGP configurations");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp *";
    cmd->cc_help_exp        = N_("<1-4294967295>");
    cmd->cc_help_exp_hint   = N_("AS number");
    cmd->cc_flags           = ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_comp_type       = cct_matching_names;
    cmd->cc_comp_pattern    = "/nkn/bgp/config/local_as/*";
    cmd->cc_exec_operation  = cxo_set;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$";
    cmd->cc_exec_value      = "$1$";
    cmd->cc_exec_type       = bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp *";
    cmd->cc_help_exp        = N_("<1-4294967295>");
    cmd->cc_help_exp_hint   = N_("AS number");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_comp_type       = cct_matching_names;
    cmd->cc_comp_pattern    = "/nkn/bgp/config/local_as/*";
    cmd->cc_exec_operation  = cxo_delete;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$";
    cmd->cc_exec_type       = bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * no";
    cmd->cc_help_descr      =  N_("Clear/remove existing BGP configuration options");
    cmd->cc_req_prefix_count =  3;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * neighbor";
    cmd->cc_help_descr      = N_("Specify neighbor router");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp * neighbor";
    cmd->cc_help_descr      = N_("Specify neighbor router");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * neighbor *";
    cmd->cc_help_exp        = N_("<A.B.C.D>");
    cmd->cc_help_exp_hint   = N_("IP address");
    cmd->cc_comp_type       = cct_matching_names;
    cmd->cc_comp_pattern    = "/nkn/bgp/config/local_as/$1$/neighbor/*";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp * neighbor *";
    cmd->cc_help_exp        = N_("<A.B.C.D>");
    cmd->cc_help_exp_hint   = N_("IP address");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_comp_type       = cct_matching_names;
    cmd->cc_comp_pattern    = "/nkn/bgp/config/local_as/$1$/neighbor/*";
    cmd->cc_exec_operation  = cxo_delete;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$/neighbor/$2$";
    cmd->cc_exec_type       = bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * neighbor * remote-as";
    cmd->cc_help_descr      = N_("Remote AS number");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * neighbor * remote-as *";
    cmd->cc_help_exp        = N_("<1-4294967295>");
    cmd->cc_help_exp_hint   = N_("AS number");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_exec_operation  = cxo_set;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$/neighbor/$2$/remote_as";
    cmd->cc_exec_value      = "$3$";
    cmd->cc_exec_type       = bt_string;
    cmd->cc_revmap_type     = crt_auto;
    cmd->cc_revmap_order    = cro_bgp;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * neighbor * shutdown";
    cmd->cc_help_descr      = N_("Shutdown the neighbor");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_exec_operation  = cxo_set;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$/neighbor/$2$/shutdown";
    cmd->cc_exec_value      = "true";
    cmd->cc_exec_type       = bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp * neighbor * shutdown";
    cmd->cc_help_descr      = N_("Cancel shutdown of the neighbor");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_exec_operation  = cxo_reset;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$/neighbor/$2$/shutdown";
    cmd->cc_exec_value      = "false";
    cmd->cc_exec_type       = bt_string;
    cmd->cc_revmap_type     = crt_manual;
    cmd->cc_revmap_order    = cro_bgp;
    cmd->cc_revmap_names    = "/nkn/bgp/config/local_as/*/neighbor/*/shutdown";
    cmd->cc_revmap_callback = cli_bgp_neighbor_shutdown_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * network";
    cmd->cc_help_descr      = N_("Specify a network to announce via BGP");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp * network";
    cmd->cc_help_descr      = N_("Specify a network to remove");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * network *";
    cmd->cc_help_exp        = N_("<A.B.C.D>");
    cmd->cc_help_exp_hint   = N_("Prefix");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp * network *";
    cmd->cc_help_exp        = N_("<A.B.C.D>");
    cmd->cc_help_exp_hint   = N_("Prefix");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * network * *";
    cmd->cc_help_exp        = N_("/<0-32>");
    cmd->cc_help_exp_hint   = N_("Mask length");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_magic           = BGP_ADD;
    cmd->cc_exec_callback   = cli_bgp_network;
    cmd->cc_revmap_order    = cro_bgp;
    cmd->cc_revmap_type     = crt_manual;
    cmd->cc_revmap_names    = "/nkn/bgp/config/local_as/*/network/*/*";
    cmd->cc_revmap_cmds     = "router bgp $5$ network $7$ /$8$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp * network * *";
    cmd->cc_help_exp        = N_("/<0-32>");
    cmd->cc_help_exp_hint   = N_("Mask length");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_magic           = BGP_DELETE;
    cmd->cc_exec_callback   = cli_bgp_network;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * redistribute";
    cmd->cc_help_descr      = N_("Redistribute routes");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp * redistribute";
    cmd->cc_help_descr      = N_("Stop redistribution");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * redistribute connected";
    cmd->cc_help_descr      = N_("Redistribute connected");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_exec_operation  = cxo_set;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$/redist_conn";
    cmd->cc_exec_value      = "true";
    cmd->cc_exec_type       = bt_string;
    cmd->cc_revmap_type     = crt_manual;
    cmd->cc_revmap_order    = cro_bgp;
    cmd->cc_revmap_names    = "/nkn/bgp/config/local_as/*/redist_conn";
    cmd->cc_revmap_callback = cli_bgp_redist_conn_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp * redistribute connected";
    cmd->cc_help_descr      = N_("Remove connected routes");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_exec_operation  = cxo_set;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$/redist_conn";
    cmd->cc_exec_value      = "false";
    cmd->cc_exec_type       = bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * bgp";
    cmd->cc_help_descr      = N_("BGP specific commands");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp * bgp";
    cmd->cc_help_descr      = N_("BGP specific commands");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * bgp log-neighbor-changes";
    cmd->cc_help_descr      = N_("Log neighbor up/down and reset reason");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_exec_operation  = cxo_reset;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$/log_neighbor";
    cmd->cc_revmap_type     = crt_none;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp * bgp log-neighbor-changes";
    cmd->cc_help_descr      = N_("Do not log neighbor up/down and reset reason");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_exec_operation  = cxo_set;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$/log_neighbor";
    cmd->cc_exec_value      = "false";
    cmd->cc_exec_type       = bt_string;
    cmd->cc_revmap_type     = crt_manual;
    cmd->cc_revmap_order    = cro_bgp;
    cmd->cc_revmap_names    = "/nkn/bgp/config/local_as/*/log_neighbor";
    cmd->cc_revmap_callback = cli_bgp_log_neighbor_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * bgp router-id";
    cmd->cc_help_descr      = N_("Override default router ID");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * bgp router-id *";
    cmd->cc_help_exp        = N_("<A.B.C.D>");
    cmd->cc_help_exp_hint   = N_("Manually configured router id");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_exec_operation  = cxo_set;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$/router_id";
    cmd->cc_exec_value      = "$2$";
    cmd->cc_exec_type       = bt_string;
    cmd->cc_revmap_type     = crt_manual;
    cmd->cc_revmap_order    = cro_bgp;
    cmd->cc_revmap_names    = "/nkn/bgp/config/local_as/*/router_id";
    cmd->cc_revmap_callback = cli_bgp_router_id_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp * bgp router-id";
    cmd->cc_help_descr      = N_("Use default router ID");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_exec_operation  = cxo_set;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$/router_id";
    cmd->cc_exec_value      = "false";
    cmd->cc_exec_type       = bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * timers";
    cmd->cc_help_descr      = N_("Adjust routing timers");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp * timers";
    cmd->cc_help_descr      = N_("Use default routing timers");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * timers bgp";
    cmd->cc_help_descr      = N_("BGP timers");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * timers bgp *";
    cmd->cc_help_exp        = N_("<0-21845>");
    cmd->cc_help_exp_hint   = N_("Keepalive interval");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "router bgp * timers bgp * *";
    cmd->cc_help_exp        = N_("<0-65535>");
    cmd->cc_help_exp_hint   = N_("Holdtime");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_exec_operation  = cxo_set;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$/timer_ka";
    cmd->cc_exec_value      = "$2$";
    cmd->cc_exec_type       = bt_string;
    cmd->cc_exec_name2      = "/nkn/bgp/config/local_as/$1$/timer_hold";
    cmd->cc_exec_value2     = "$3$";
    cmd->cc_exec_type2      = bt_string;
    cmd->cc_revmap_type     = crt_manual;
    cmd->cc_revmap_order    = cro_bgp;
    cmd->cc_revmap_names    = "/nkn/bgp/config/local_as/*/timer_ka";
    cmd->cc_revmap_callback = cli_bgp_timers_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "no router bgp * timers bgp";
    cmd->cc_help_descr      = N_("BGP timers");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_set_rstr_curr;
    cmd->cc_exec_operation  = cxo_set;
    cmd->cc_exec_name       = "/nkn/bgp/config/local_as/$1$/timer_ka";
    cmd->cc_exec_value      = "false";
    cmd->cc_exec_type       = bt_string;
    cmd->cc_exec_name2      = "/nkn/bgp/config/local_as/$1$/timer_hold";
    cmd->cc_exec_value2     = "false";
    cmd->cc_exec_type2      = bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "show bgp";
    cmd->cc_help_descr      = N_("Display BGP information");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_query_rstr_curr;
    cmd->cc_exec_callback   = cli_show_bgp;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str       = "clear bgp";
    cmd->cc_help_descr      = N_("Clear BGP neighbors");
    cmd->cc_flags           = ccf_terminal;
    cmd->cc_capab_required  = ccp_query_rstr_curr;
    cmd->cc_exec_callback   = cli_clear_bgp;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_arr(bgp_ignore_bindings);
bail:
    return err;
}

/* 
 *  [no] router bgp <local-as> network <prefix> mask <mask>
 */
int cli_bgp_network(void *data, const cli_command *cmd,
                     const tstr_array *cmd_line,
                     const tstr_array *params)
{
    int err = 0;
    const char *local_as = NULL, *prefix_str = NULL, *mask_str = NULL;
    node_name_t network_nd = {0};
    uint32      ret_err = 0;
    tstring     *ret_msg = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(cmd);

    local_as  = tstr_array_get_str_quick(params, 0);
    bail_null(local_as);
    prefix_str = tstr_array_get_str_quick(params, 1);
    bail_null(prefix_str);
    mask_str = tstr_array_get_str_quick(params, 2);
    bail_null(mask_str);  
 
    if (*mask_str != '/' || *(mask_str + 1) == 0) {
        cli_printf("Mask length must start with a '/' followed by a number\n");
        goto bail;
    }

    /* don't use the leading '/' */    
    mask_str++;
    snprintf(network_nd, sizeof(network_nd), 
             "/nkn/bgp/config/local_as/%s/network/%s/%s",
             local_as, prefix_str, mask_str);
    switch(cmd->cc_magic) {
    case BGP_ADD:
        err = mdc_create_binding(cli_mcc,
                    &ret_err,
                    &ret_msg,
                    bt_string,
                    mask_str,
                    network_nd);
        bail_error(err);
        bail_error_msg(ret_err, "%s", ts_str(ret_msg));
        break;

    case BGP_DELETE:
        err = mdc_delete_binding(cli_mcc, &ret_err, &ret_msg, network_nd);
        bail_error_msg(ret_err, "%s", ts_str(ret_msg));
        bail_error(err);
        break;
    }

bail:
    ts_free(&ret_msg);
    return(err);
}
   
int cli_bgp_neighbor_shutdown_revmap(void *data, const cli_command *cmd,
                             const bn_binding_array *bindings, const char *name,
                             const tstr_array *name_parts, const char *value,
                             const bn_binding *binding,
                             cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL;
    const char *local_as = NULL, *neighbor = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name_parts);

    if (value == NULL) {
        goto bail;
    }

    if (strcmp(value, "false") == 0) {
        local_as = tstr_array_get_str_quick(name_parts, 4);
        bail_null(local_as);
        neighbor = tstr_array_get_str_quick(name_parts, 6);
        bail_null(local_as);

        err = ts_new_sprintf(&rev_cmd, "no router bgp %s neighbor %s shutdown",
                local_as, neighbor);
        bail_error(err);

        err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
        bail_error(err);
    }

bail:
    ts_free(&rev_cmd);
    return err;
}

int cli_bgp_redist_conn_revmap(void *data, const cli_command *cmd,
                             const bn_binding_array *bindings, const char *name,
                             const tstr_array *name_parts, const char *value,
                             const bn_binding *binding,
                             cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL;
    const char *local_as = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name_parts);

    if (value == NULL) {
        goto bail;
    }

        local_as = tstr_array_get_str_quick(name_parts, 4);
        bail_null(local_as);

        err = ts_new_sprintf(&rev_cmd, "%srouter bgp %s redistribute connected",
                ((strcmp(value, "false") == 0)? "no ": ""), local_as);
        bail_error(err);

        err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
        bail_error(err);

bail:
    ts_free(&rev_cmd);
    return err;
}
 
int cli_bgp_log_neighbor_revmap(void *data, const cli_command *cmd, 
                             const bn_binding_array *bindings, const char *name,
                             const tstr_array *name_parts, const char *value,
                             const bn_binding *binding,
                             cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL;
    const char *local_as = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd); 
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name_parts);

    if (value == NULL) { 
        goto bail;
    }
    
        local_as = tstr_array_get_str_quick(name_parts, 4);
        bail_null(local_as);
    
        err = ts_new_sprintf(&rev_cmd, "%srouter bgp %s bgp log-neighbor-changes",
                 ((strcmp(value, "false") == 0) ? "no ": ""), local_as);

        bail_error(err);
    
        err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
        bail_error(err);

bail:
    ts_free(&rev_cmd);
    return err;
}

int cli_bgp_router_id_revmap(void *data, const cli_command *cmd,
                             const bn_binding_array *bindings, const char *name,
                             const tstr_array *name_parts, const char *value,
                             const bn_binding *binding,
                             cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL;
    const char *local_as = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd); 
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name_parts);

    if (value == NULL) {
        goto bail;
    }
    
    if (strcmp(value, "false") != 0 && strcmp(value, "init") != 0) {
        local_as = tstr_array_get_str_quick(name_parts, 4);
        bail_null(local_as);

        err = ts_new_sprintf(&rev_cmd, "router bgp %s bgp router-id %s", 
                local_as, value);
        bail_error(err);

        err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
        bail_error(err);
    }

    /* Consume bindings */
    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);

bail:
    ts_free(&rev_cmd);
    return err;
}

int cli_bgp_timers_revmap(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
    int err = 0;
    tstring *rev_cmd = NULL;
    tstring *holdtime = NULL;
    const char *local_as = NULL;
    node_name_t holdtimer_nd = {0};

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd); 
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name_parts);

    if (value == NULL) {
        goto bail;
    }

    local_as = tstr_array_get_str_quick(name_parts, 4);
    bail_null(local_as);

    snprintf(holdtimer_nd, sizeof(holdtimer_nd), 
             "/nkn/bgp/config/local_as/%s/timer_hold", local_as);

    if (strcmp(value, "false") != 0 && strcmp(value, "init") != 0) {

        err = mdc_get_binding_tstr(cli_mcc,
                               NULL, NULL, NULL,
                               &holdtime,
                               holdtimer_nd,
                               NULL);
        bail_error(err);

        err = ts_new_sprintf(&rev_cmd, "router bgp %s timers bgp %s %s", 
                    local_as, value, ts_str(holdtime));
        bail_error(err);

        err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
        bail_error(err);
    }

    /* Consume bindings */
    err = tstr_array_append_str(ret_reply->crr_nodes, name);
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_nodes, holdtimer_nd);
    bail_error(err);

bail:
    ts_free(&rev_cmd);
    ts_free(&holdtime);
    return err;
}

int cli_show_bgp(void *arg,
        const cli_command *cmd,
        const tstr_array  *cmd_line,
        const tstr_array  *params)
{
    int err = 0;
    tstr_array *local_as_array = NULL;
    const char *local_as = NULL;
    int num_names = 0;
    int32   status = 0;
    tstring *ret_output = NULL;

    UNUSED_ARGUMENT(arg);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL, NULL, &local_as_array,
            "/nkn/bgp/config/local_as", NULL);
    bail_error_null(err, local_as_array);
   
    num_names = tstr_array_length_quick(local_as_array);
   
    /* BGP only allows one AS at a time */
    if (num_names > 1) {
        cli_printf_error(_("There are more than one BGP Local AS number\n"));
        goto bail;    
    } 

    if (num_names == 1) {
        local_as = tstr_array_get_str_quick(local_as_array, 0);
        bail_null(local_as);
        cli_printf(_("BGP local AS number is %s\n\n"), local_as);
    }

    err = lc_launch_quick_status(&status, &ret_output, true, 3,
            VTYSH_PATH, "-c", "show ip bgp neighbors");
    bail_error(err);

    if (ret_output) { 
        cli_printf(_("%s\n"), ts_str(ret_output));
    }

    err = lc_launch_quick_status(&status, &ret_output, true, 3,
            VTYSH_PATH, "-c", "show ip bgp");
    bail_error(err);

    if (ret_output) {
        cli_printf(_("%s\n"), ts_str(ret_output));
    }

bail:
    tstr_array_free(&local_as_array);
    return (err);
}

int cli_clear_bgp(void *arg,
        const cli_command *cmd,
        const tstr_array  *cmd_line,
        const tstr_array  *params)
{
    int     err = 0;

    UNUSED_ARGUMENT(arg);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = mdc_send_action_with_bindings_str_va(
                cli_mcc, NULL, NULL, "/nkn/bgp/actions/clear_bgp", 0,
                NULL, bt_NONE, NULL);
    bail_error(err);

 bail:
    return err;
}


