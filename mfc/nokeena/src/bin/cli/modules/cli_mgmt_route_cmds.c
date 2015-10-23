/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */

/*-----------------------------------------------------------------------------
 * This file was taken from tall-maple source. However, the entire command tree
 * from the original file (cli_route_cmds.c) has been moved as a child of
 * "managament" command tree.
 * --------------------------------------------------------------------------*/

#include "common.h"
#include "climod_common.h"

enum {
    cid_show_routes,
    cid_show_routes_static,
    cid_show_default_gw,
    cid_show_default_gw_static,

    cid_show_ipv6_routes,
    cid_show_ipv6_routes_static,
    cid_show_ipv6_default_gw,
    cid_show_ipv6_default_gw_static,

    cid_no_route_prefix,
    cid_no_route_netmask,
    cid_no_route_nexthop,
    cid_default_gateway,
    cid_no_default_gateway,

    cid_ipv6_default_gateway,
    cid_no_ipv6_default_gateway,
    cid_ipv6_route,
    cid_no_ipv6_route,
};

#define cli_ip_dyn_route_name_prefix \
"/nkn/net/routes/state/ipv4/prefix"

#define cli_ip_route_name_prefix \
"/nkn/net/routes/config/ipv4/prefix"


static uint16 cli_ip_prefix_idx = 6;

typedef struct cli_mgmt_route_context {
    uint32 crc_num_routes;
    tbool crc_static;
    char *crc_header;
    tbool crc_header_printed;
} cli_mgmt_route_context;

cli_interface_completion_type cli_interface_completion_func_routes = NULL;
cli_interface_help_type cli_interface_help_func_routes = NULL;

int cli_mgmt_route_cmds_init(const lc_dso_info *info,
                         const cli_module_context *context);

int cli_ip_route_do_completion(tstr_array **ret_routes, const char *binding,
                               const char *prefix, uint32 cmd_data);

int cli_ip_route_completion(void *data, const cli_command *cmd,
                            const tstr_array *cmd_line,
                            const tstr_array *params,
                            const tstring *curr_word,
                            tstr_array *ret_completions);

int cli_ip_add_route(void *data, const cli_command *cmd,
                     const tstr_array *cmd_line,
                     const tstr_array *params);

int cli_ip_delete_route(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params);

int cli_mgmt_ip_show_routes(void *data, const cli_command *cmd,
                       const tstr_array *cmd_line, const tstr_array *params);

int cli_mgmt_ip_show_route_prefix(const bn_binding_array *bindings, uint32 idx,
                             const bn_binding *binding, const tstring *name, 
                             const tstr_array *name_components,
                             const tstring *name_last_part,
                             const tstring *value, void *callback_data);

int cli_ip_help_netmask(void *data, cli_help_type help_type, 
                        const cli_command *cmd, const tstr_array *cmd_line,
                        const tstr_array *params, const tstring *curr_word,
                        void *context);

static int
cli_ip_revmap_routes(void *data, const cli_command *cmd,
                     const bn_binding_array *bindings,
                     const char *name,
                     const tstr_array *name_parts,
                     const char *value, 
                     const bn_binding *binding,
                     cli_revmap_reply *ret_reply);

/* ------------------------------------------------------------------------- */
/* Called for "ip route * * * *".
 * If the 3rd parameter is a valid bt_ipv4addr, we call through
 * to cli_interface_completion_func_routes(); otherwise, no completions.
 */
static int
cli_mgmt_route_comp_intf(void *data, const cli_command *cmd,
                    const tstr_array *cmd_line,
                    const tstr_array *params,
                    const tstring *curr_word,
                    tstr_array *ret_completions)
{
    int err = 0;
    const char *gw_str = NULL;
    bn_ipv4addr gw = lia_ipv4addr_any;

    gw_str = tstr_array_get_str_quick(params, 2);
    bail_null(gw_str);

    err = lia_str_to_ipv4addr(gw_str, &gw);
    if (err == lc_err_bad_type) {
        /* No completions */
        err = 0;
    }
    else {
        bail_error(err);
        err = cli_interface_completion_func_routes(data, cmd, cmd_line,
                                                   params, curr_word,
                                                   ret_completions);
        bail_error(err);
    }

 bail:
    return(err);
}
 
 
/* ------------------------------------------------------------------------- */
/* Called for "ip route * * * *".
 * If the 3rd parameter is a valid bt_ipv4addr, we call through
 * to cli_interface_help_func_routes(); otherwise, no help.
 */
static int
cli_mgmt_route_help_intf(void *data, cli_help_type help_type, 
                    const cli_command *cmd, const tstr_array *cmd_line,
                    const tstr_array *params, const tstring *curr_word,
                    void *context)
{
    
    int err = 0;
    const char *gw_str = NULL;
    tbool valid = false;

    if (help_type != cht_expansion) {
        goto bail;
    }

    gw_str = tstr_array_get_str_quick(params, 2);
    bail_null(gw_str);

    err = lia_str_is_ipv4addr(gw_str, &valid);
    bail_error(err);

    if (!valid) {
        /* No help */
    }
    else {
        bail_error(err);
        err = cli_interface_help_func_routes(data, help_type, cmd, cmd_line,
                                             params, curr_word, context);
        bail_error(err);
    }

 bail:
    return(err);
}


typedef struct cli_ipv6_route_completion_context {
    tstr_array *circc_completions;
    const char *circc_gateway;
} cli_ipv6_route_completion_context;


/* ------------------------------------------------------------------------- */
int
cli_mgmt_route_cmds_init(const lc_dso_info *info,
                     const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;
    void *cli_interface_completion_func_v = NULL;
    void *cli_interface_help_func_v = NULL;

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = cli_set_gettext_domain(GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }

    err = cli_get_symbol("cli_interface_cmds", "cli_interface_completion",
                         &cli_interface_completion_func_v);
    cli_interface_completion_func_routes = cli_interface_completion_func_v;
    bail_error_null(err, cli_interface_completion_func_routes);

    err = cli_get_symbol("cli_interface_cmds", "cli_interface_help",
                         &cli_interface_help_func_v);
    cli_interface_help_func_routes = cli_interface_help_func_v;
    bail_error_null(err, cli_interface_help_func_routes);

    /* ........................................................................
     * IPv4 route config
     */

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no management ip";
    cmd->cc_help_descr =
        N_("Remove management interface IP specific options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "management ip";
    cmd->cc_help_descr =
        N_("Configure management interface IP specific options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "management ip route";
    cmd->cc_help_descr =
        N_("Add a static route");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "management ip route *";
    cmd->cc_flags |=                ccf_normalize_value;
    cmd->cc_normalize_type =        bt_ipv4addr;
    cmd->cc_help_exp = 
        N_("<network prefix>");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no management ip route";
    cmd->cc_help_descr =
        N_("Remove a static route");
    CLI_CMD_REGISTER;

    /* management ip route <prefix> */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "no management ip route *";
    cmd->cc_magic =                 cid_no_route_prefix;
    cmd->cc_flags |=                ccf_normalize_value;
    cmd->cc_normalize_type =        bt_ipv4addr;
    cmd->cc_help_exp = 
        N_("<network prefix>");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_callback =         cli_ip_route_completion;
    CLI_CMD_REGISTER;

    /* no management ip route <prefix> <mask> */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "no management ip route * *";
    cmd->cc_magic =                 cid_no_route_netmask;
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp = 
        N_("<netmask>");
    cmd->cc_help_term =
	N_("Remove all routes for the specified prefix/mask");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_callback =         cli_ip_route_completion;
    cmd->cc_exec_callback =         cli_ip_delete_route;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "no management ip route * * gateway";
    cmd->cc_help_descr =	    N_("Remove a specific next-hop address for this prefix/mask");
    CLI_CMD_REGISTER;

    /* no management ip route <prefix> <mask> gateway <gw> */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "no management ip route * * gateway *";
    cmd->cc_magic =                 cid_no_route_nexthop;
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp = 
        N_("<destination>");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_callback =         cli_ip_route_completion;
    cmd->cc_exec_callback =         cli_ip_delete_route;
    CLI_CMD_REGISTER;

    /* management ip route <prefix> <mask> */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "management ip route * *";
    /*
     * This won't get used at runtime, since it is overridden by the
     * help callback.  But it is here for the "-D" output (dump
     * command tree, with help strings), which doesn't run custom
     * callbacks.
     */
    cmd->cc_help_exp =
        N_("<netmask or mask length>");

    cmd->cc_help_callback =         cli_ip_help_netmask;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =		    "management ip route * * gateway";
    cmd->cc_help_descr =	    N_("Configure next hop or gateway address");
    CLI_CMD_REGISTER;

    /* management ip route <prefix> <mask> gateway <gw> interface <ifname> */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "management ip route * * gateway *"; 
    cmd->cc_help_exp = 
        N_("<next-hop address>");
    CLI_CMD_REGISTER;

    /* management ip route <prefix> <mask> gateway <gw> source-ip */
    CLI_CMD_NEW;
    cmd->cc_words_str =		    "management ip route * * gateway * source-ip";
    cmd->cc_help_descr =	    N_("Configure source ip address to use");
    CLI_CMD_REGISTER;

    /* management ip route <prefix> <mask> gateway <gw> source-ip <src> */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "management ip route * * gateway * source-ip *";
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_exp = 
        N_("<preferred source IP address>");
    cmd->cc_exec_callback =         cli_ip_add_route;
    cmd->cc_revmap_names =          "/nkn/net/routes/config/ipv4/prefix/*/nh/*";
    cmd->cc_revmap_order =          cro_routing;
    cmd->cc_revmap_callback =       cli_ip_revmap_routes;
    CLI_CMD_REGISTER;

    /* ........................................................................
     * Viewing IPv4 routing config/state.
     *
     * (The root "show ip" command is registered in cli_ip_cmds.c.)
     */

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show management ip";
    cmd->cc_magic =                 cid_show_routes_static;
    cmd->cc_flags |=                ccf_terminal;
    cmd->cc_capab_required =        ccp_query_rstr_curr;
    cmd->cc_help_descr =
        N_("Display routing table");
    cmd->cc_help_term =
        N_("Display active routes, both dynamic and static");
    cmd->cc_exec_callback =         cli_mgmt_ip_show_routes;
    CLI_CMD_REGISTER;


    /* ........................................................................
     * Other init
     */

    /*
     * 1. We don't disable routes, we delete them.
     * 2. There is no CLI command to set the metric.
     * 3. Only one type is ever set by the CLI.
     */
    err = cli_revmap_ignore_bindings_va
        (4,
         "/nkn/net/routes/config/ipv4/prefix/*",
         "/nkn/net/routes/config/ipv4/prefix/*/enable",
         "/nkn/net/routes/config/ipv4/prefix/*/metric",
         "/nkn/net/routes/config/ipv4/prefix/*/nh/*");
    bail_error(err);

#ifdef PROD_FEATURE_IPV6
    err = cli_revmap_ignore_bindings_va
        (5,
         "/net/routes/config/ipv6/prefix/*",
         "/net/routes/config/ipv6/prefix/*/enable",
         "/net/routes/config/ipv6/prefix/*/nh/*",
         "/net/routes/config/ipv6/prefix/*/nh/*/type",
         "/net/routes/config/ipv6/prefix/*/nh/*/metric");
    bail_error(err);
#else
    err = cli_revmap_ignore_bindings_va
        (1,
         "/net/routes/config/ipv6/**");
    bail_error(err);
#endif

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
int
cli_ip_route_do_completion(tstr_array **ret_routes, const char *binding,
                           const char *prefix, uint32 cmd_data)
{
    tstr_array *rt_bindings = NULL;
    tstr_array *completions = NULL;
    int err = 0;
    uint32 num_values = 0, i = 0;
    const char *prefix_str = NULL;
    char *pfx_ip = NULL, *pfx_mask = NULL;
    bn_ipv4addr pfx_ip_num = lia_ipv4addr_any, prefix_addr = lia_ipv4addr_any;
    bn_ipv4prefix pfx_num = lia_ipv4prefix_any;
    char *gw_binding = NULL;
    tstring *gw = NULL;

    bail_null(ret_routes);
    *ret_routes = NULL;

    err = tstr_array_new(&completions, NULL);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
                                              NULL, NULL, &rt_bindings,
                                              binding, NULL);
    bail_error(err);

    num_values = tstr_array_length_quick(rt_bindings);
    for (i = 0; i < num_values; ++i) {
        err = tstr_array_get_str(rt_bindings, i, &prefix_str);
        bail_error(err);

        if (cmd_data == cid_no_route_nexthop) {
            safe_free(gw_binding);
            gw_binding = smprintf("%s/%s/gw", binding, prefix_str);
            err = mdc_get_binding_tstr
              (cli_mcc,
               NULL, NULL, NULL, &gw, gw_binding, NULL);
            bail_error(err);

            err = tstr_array_append_str(completions, ts_str(gw));
            bail_error(err);
            continue;
        }

        err = lia_str_to_ipv4prefix(prefix_str, &pfx_num);
        bail_error(err);

        err = lia_ipv4prefix_get_ipv4addr(&pfx_num, &pfx_ip_num);
        bail_error(err);

        if (cmd_data == cid_no_route_netmask) {
            lia_str_to_ipv4addr(prefix, &prefix_addr);
            if (!lia_ipv4addr_equal_quick(&prefix_addr, &pfx_ip_num)) {
                continue;
            }

            err = lia_ipv4prefix_get_mask_str(&pfx_num, &pfx_mask);
            bail_error(err);

            err = tstr_array_append_str(completions, pfx_mask);
            bail_error(err);
        }
        else {
            err = lia_ipv4addr_to_str(&pfx_ip_num, &pfx_ip);
            bail_error(err);

            err = tstr_array_append_str(completions, pfx_ip);
            bail_error(err);
        }
    }

    *ret_routes = completions;
    completions = NULL;

 bail:
    tstr_array_free(&completions);
    tstr_array_free(&rt_bindings);
    ts_free(&gw);
    safe_free(gw_binding);
    return(err);
}

/* ------------------------------------------------------------------------- */
/* "no ip route <prefix> <netmask> <nexthop>" */
int
cli_ip_route_completion(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params,
                        const tstring *curr_word,
                        tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *routes = NULL;
    const char *prefix = NULL;
    const char *mask = NULL;
    char *masklen = NULL;
    char *config_name = NULL;

    switch (cmd->cc_magic) {

    case cid_no_route_netmask:
        prefix = tstr_array_get_str_quick(params, 0);
        /* Fall through */
    case cid_no_route_prefix:
        err = cli_ip_route_do_completion(&routes, cli_ip_route_name_prefix,
                                         prefix, cmd->cc_magic);
        bail_error(err);

        err = tstr_array_concat(ret_completions, &routes);
        bail_error(err);

        err = tstr_array_delete_non_prefixed(ret_completions,
                                             ts_str(curr_word));
        bail_error(err);
        break;

    case cid_no_route_nexthop:
        prefix = tstr_array_get_str_quick(params, 0);
        mask = tstr_array_get_str_quick(params, 1);

        err = lia_ipv4addr_maskspec_to_masklen_str(mask, &masklen);
        if (err == lc_err_bad_type) {
            err = cli_printf_error("%s", cli_error_netmask);
            bail_error(err);
            goto bail;
        }
        bail_error(err);

        config_name = smprintf("%s/%s\\/%s/nh", cli_ip_route_name_prefix, 
                               prefix, masklen);
        bail_null(config_name);

        err = cli_ip_route_do_completion(&routes, config_name, NULL,
                                         cmd->cc_magic);
        bail_error(err);

        err = tstr_array_concat(ret_completions, &routes);
        bail_error(err);

        err = tstr_array_delete_non_prefixed(ret_completions,
                                             ts_str(curr_word));
        bail_error(err);
        break;

    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

 bail:
    safe_free(masklen);
    safe_free(config_name);
    return(err);
}

/* ------------------------------------------------------------------------- */
/* management ip route <prefix> <mask> <gw> <src>
 * ip route * * * [*]
 * ip default-gateway * [*]
 *
 * The last optional parmeter can be an interface name to modify the
 * destination IP, or it can be omitted. In this case, the last
 * parameter is either a destination IP or an Interface.
 */
int
cli_ip_add_route(void *data, const cli_command *cmd,
                 const tstr_array *cmd_line,
                 const tstr_array *params)
{
    int err = 0;
    const char *prefix = NULL;
    const char *mask = NULL;
    const char *route_type = "gw";
    char *masklen = NULL;
    const char *gw = NULL;
    const char *dev = NULL;
    const char *src = NULL;
    bn_request *req = NULL;
    bn_binding *binding = NULL;

    if (cmd->cc_magic == cid_default_gateway) {
#if 0
        gw = tstr_array_get_str_quick(params, 0);
        prefix = "0.0.0.0";
        mask = "0.0.0.0";
        if (tstr_array_length_quick(params) == 2) {
            dev = tstr_array_get_str_quick(params, 1);
        }
#endif
    }
    else {
        prefix = tstr_array_get_str_quick(params, 0);
        mask = tstr_array_get_str_quick(params, 1);
        gw = tstr_array_get_str_quick(params, 2);
	//dev = tstr_array_get_str_quick(params, 3);
	src = tstr_array_get_str_quick(params, 3);
    }

    err = lia_ipv4addr_maskspec_to_masklen_str(mask, &masklen);
    if (err == lc_err_bad_type) {
        err = cli_printf_error("%s", cli_error_netmask);
        bail_error(err);
        goto bail;
    }
    bail_error(err);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    /* REMXXX NH index of 1 should be flexible */

    err = bn_binding_new_str
        (&binding, "gw", ba_value, bt_ipv4addr, 0, gw);
    if (err == lc_err_bad_type) {
        /* 
         * If the second/third argument isn't an IP address, assume its
         * an interface name
         */
        if (dev) {
            err = cli_printf_error(_("Invalid IP address given for "
                                     "destination\n"));
            bail_error(err);
            goto bail;
        }
        else {
            /* Only the interface was specified */
            dev = gw;
            gw = NULL;
        }

        route_type = "interface";
    }
    else {
        /*
         * If a valid destination is given (even if additionally an
         * interface is specified) consider it of 'gw' type
         */
        route_type = "gw";
    }

    err = bn_set_request_msg_append_new_binding_fmt
        (req, bsso_modify, 0, bt_string, 0, route_type, NULL,
         "%s/%s\\/%s/nh/1/type", cli_ip_route_name_prefix, 
         prefix, masklen);
    bail_error(err);

    if (gw) {
        err = bn_set_request_msg_append_new_binding_fmt
            (req, bsso_modify, 0, bt_ipv4addr, 0, gw, NULL,
             "%s/%s\\/%s/nh/1/gw", cli_ip_route_name_prefix, 
             prefix, masklen);
        bail_error(err);
    }
    else {
        err = bn_set_request_msg_append_new_binding_fmt
            (req, bsso_reset, 0, bt_ipv4addr, 0, "0.0.0.0", NULL,
             "%s/%s\\/%s/nh/1/gw", cli_ip_route_name_prefix, 
             prefix, masklen);
        bail_error(err);
    }

    if (dev) {
        err = bn_set_request_msg_append_new_binding_fmt
            (req, bsso_modify, 0, bt_string, 0, dev, NULL,
             "%s/%s\\/%s/nh/1/interface", cli_ip_route_name_prefix, 
             prefix, masklen);
        bail_error(err);
    }
    else {
        err = bn_set_request_msg_append_new_binding_fmt
            (req, bsso_reset, 0, bt_string, 0, "", NULL,
             "%s/%s\\/%s/nh/1/interface", cli_ip_route_name_prefix, 
             prefix, masklen);
        bail_error(err);
    }

    if (src) {
	err = bn_set_request_msg_append_new_binding_fmt
	    (req, bsso_modify, 0, bt_ipv4addr, 0, src, NULL,
	     "%s/%s\\/%s/nh/1/source", cli_ip_route_name_prefix,
	     prefix, masklen);
	bail_error(err);
    } else {
	err = bn_set_request_msg_append_new_binding_fmt
	    (req, bsso_reset, 0, bt_ipv4addr, 0, "0.0.0.0", NULL,
	     "%s/%s\\/%s/nh/1/source", cli_ip_route_name_prefix,
	     prefix, masklen);
	bail_error(err);
    }

    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);

 bail:
    bn_request_msg_free(&req);
    bn_binding_free(&binding);
    safe_free(masklen);
    return(err);
}

/* ------------------------------------------------------------------------- */
/* "no ip route * * [*]"
 *
 * NOTE: it is somewhat unusual for us to give error messages when
 * you try to delete something that does not exist.  Most of the rest 
 * of our UI does not do so.  But this was specifically requested for
 * this one command in bug 12038.
 */
int
cli_ip_delete_route(void *data, const cli_command *cmd,
                    const tstr_array *cmd_line,
                    const tstr_array *params)
{
    int err = 0;
    const char *prefix = NULL;
    const char *mask = NULL;
    char *masklen = NULL;
    const char *gw = NULL;
    char *name = NULL;
    uint32 num_deleted = 0;
    bn_binding *binding = NULL;

    prefix = tstr_array_get_str_quick(params, 0);
    mask = tstr_array_get_str_quick(params, 1);

    err = lia_ipv4addr_maskspec_to_masklen_str(mask, &masklen);
    if (err == lc_err_bad_type) {
        err = cli_printf_error("%s", cli_error_netmask);
        bail_error(err);
        goto bail;
    }
    bail_error(err);

    switch (cmd->cc_magic) {

    case cid_no_route_netmask:
        name = smprintf("%s/%s\\/%s", cli_ip_route_name_prefix,
                        prefix, masklen);
        bail_null(name);

        err = mdc_get_binding(cli_mcc, NULL, NULL, false, &binding, name,
                              NULL);
        bail_error(err);
        if (binding) {
            err = mdc_delete_binding(cli_mcc, NULL, NULL, name);
            bail_error(err);
        }
        else {
            err = cli_printf_error(_("Could not find route with prefix %s, "
                                     "and mask %s.\n"), prefix, mask);
            bail_error(err);
        }
        break;

    case cid_no_route_nexthop:
        gw = tstr_array_get_str_quick(params, 2);
        name = smprintf("%s/%s\\/%s/nh", cli_ip_route_name_prefix,
                        prefix, masklen);
        bail_null(name);
        err = mdc_array_delete_by_value_single
            (cli_mcc,
             name, true, "gw", bt_ipv4addr, gw, &num_deleted, NULL, NULL);
        bail_error(err);
        if (num_deleted == 0) {
            err = cli_printf_error(_("Could not find route with prefix %s, "
                                     "mask %s,\nand gateway %s\n"), prefix,
                                   mask, gw);
            bail_error(err);
        }
        break;

    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

 bail:
    safe_free(name);
    safe_free(masklen);
    bn_binding_free(&binding);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
cli_ip_show_ipv6_defgw(const bn_binding_array *bindings,
                       uint32 idx, const bn_binding *binding,
                       const tstring *name, const tstr_array *name_components,
                       const tstring *name_last_part,
                       const tstring *value, void *callback_data)
{
    int err = 0;
    const bn_binding *rbind = NULL;
    tstring *gw = NULL, *intf = NULL;
    cli_mgmt_route_context *ctxt = callback_data;

    bail_null(ctxt);

    /*
     * Don't print any reject routes here.
     */
    err = bn_binding_array_get_binding_by_name_rel(bindings, ts_str(name),
                                                   "flags/reject", &rbind);
    bail_error(err);
    if (rbind) {
        goto bail;
    }

    /*
     * We can't use idx to tell if this is the first line, since idx == 0
     * may have been skipped for being a reject route.
     */
    if ((ctxt->crc_num_routes)++ == 0) {
        if (ctxt->crc_static) {
            err = cli_printf(_("Configured default gateways:\n"));
            bail_error(err);
        }
        else {
            err = cli_printf(_("Active default gateways:\n"));
            bail_error(err);
        }
    }

    err = bn_binding_array_get_value_tstr_by_name_rel(bindings, ts_str(name),
                                                      "gw", NULL, &gw);
    bail_error(err);
    err = bn_binding_array_get_value_tstr_by_name_rel(bindings, ts_str(name),
                                                     "interface", NULL, &intf);
    bail_error(err);

    if (ts_nonempty(intf)) {
        err = cli_printf(_("   %s (interface: %s)\n"), ts_str(gw),
                         ts_str(intf));
        bail_error(err);
    }
    else {
        err = cli_printf(_("   %s\n"), ts_str(gw));
        bail_error(err);
    }

 bail:
    ts_free(&gw);
    ts_free(&intf);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
cli_ipv6_show_route_nexthop(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    int err = 0;
    cli_mgmt_route_context *ctxt = callback_data;
    const char *nn = NULL, *prefix_str = NULL;
    const bn_binding *gw = NULL;
    tstring *reject = NULL, *ifname = NULL;
    char *gw_str = NULL;
    bn_ipv6prefix prefix = lia_ipv6prefix_any;
    uint8 mask_len = 0;

    nn = ts_str(name);
    bail_null(nn);

    /* ........................................................................
     * Interface name and "(reject)" share a column.
     *
     * (and we might need 'ifname' for our filtering below...)
     */
    err = bn_binding_array_get_value_tstr_by_name_rel
        (bindings, nn, "flags/reject", NULL, &reject);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name_rel
        (bindings, nn, "interface", NULL, &ifname);
    bail_error(err);

    /* ........................................................................
     * Filter out:
     *   prefix fe80::/LEN (LEN>=10), no gateway
     *   prefix fe80.../128, ifname lo, no gateway
     *
     * (Could have used lia_ipv6addr_is_linklocal() for the first one, 
     * but this way is easier...)
     */
    prefix_str = tstr_array_get_str_quick(name_components, 5);
    bail_null(prefix_str);
    err = bn_binding_array_get_binding_by_name_rel(bindings, nn, "gw", &gw);
    bail_error(err);
    if (gw == NULL) {
        goto bail; /* Probably race condition, route just got deleted */
    }
    err = bn_binding_get_str(gw, ba_value, 0, NULL, &gw_str);
    bail_error(err);
    err = lia_str_to_ipv6prefix(prefix_str, &prefix);
    bail_error(err);
    err = lia_ipv6prefix_get_masklen(&prefix, &mask_len);
    bail_error(err);
    if (lc_is_prefix("fe80::/", prefix_str, true) && mask_len >= 10 &&
        !strcmp(gw_str, "::")) {
        goto bail;
    }
    if (lc_is_prefix("fe80", prefix_str, true) && mask_len == 128 &&
        !strcmp(gw_str, "::") && ts_equal_str(ifname, "lo", false)) {
        goto bail;
    }

    /* ........................................................................
     * Print it!
     *
     * We print the header lazily here in case filtering means there is
     * nothing to print for a given prefix.
     */
    if (ctxt->crc_header_printed == false) {
        err = cli_printf("%s\n", ctxt->crc_header);
        bail_error(err);
        ctxt->crc_header_printed = true;
    }
    err = cli_printf_prequeried
        (bindings,
         _("    %-39s  %-9s  #%s/source#\n"),
         gw_str, reject ? _("(reject)") : 
         (ts_nonempty(ifname) ? ts_str(ifname) : "-"), nn);
    bail_error(err);

 bail:
    ts_free(&reject);
    ts_free(&ifname);
    safe_free(gw_str);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
cli_ipv6_show_route_prefix(const bn_binding_array *bindings, uint32 idx,
                           const bn_binding *binding, const tstring *name, 
                           const tstr_array *name_components,
                           const tstring *name_last_part,
                           const tstring *value, void *callback_data)
{
    int err = 0;
    const char *nn = NULL;
    cli_mgmt_route_context *ctxt = callback_data;
    char *bpat = NULL;

    bail_null(ctxt);

    /*
     * Print header on first iteration.  The one for the 'static' variant
     * just doesn't have the "Source" column.
     */
    if ((ctxt->crc_num_routes)++ == 0) {
        if (ctxt->crc_static) {
            err = cli_printf(_("Destination prefix\n"
                               "    Gateway                                  "
                               "Interface\n"
                               "---------------------------------------------"
                               "---------\n"));
            bail_error(err);
        }
        else {
            err = cli_printf(_("Destination prefix\n"
                               "    Gateway                                  "
                               "Interface  Source\n"
                               "---------------------------------------------"
                               "--------------------------\n"));
            bail_error(err);
        }
    }

    nn = ts_str(name);
    bail_null(nn);

    err = ts_dup_str(value, &(ctxt->crc_header), NULL);
    bail_error(err);

    ctxt->crc_header_printed = false;

    bpat = smprintf("%s/nh/*", ts_str(name));
    bail_null(bpat);

    err = mdc_foreach_binding_prequeried(bindings, bpat, NULL, 
                                         cli_ipv6_show_route_nexthop, ctxt,
                                         NULL);
    bail_error(err);

 bail:
    safe_free(bpat);
    safe_free(ctxt->crc_header);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
cli_mgmt_ip_show_routes(void *data, const cli_command *cmd,
                   const tstr_array *cmd_line, const tstr_array *params)
{
    int err = 0;
    uint32 num_matches = 0;
    char *pattern = NULL;
    tstring *gw = NULL, *intf = NULL;
    cli_mgmt_route_context ctxt;

    memset(&ctxt, 0, sizeof(ctxt));

    switch (cmd->cc_magic) {
    case cid_show_routes_static:
        pattern = smprintf("%s/*/nh/*", cli_ip_route_name_prefix);
        bail_null(pattern);
        err = mdc_foreach_binding(cli_mcc,
                                  pattern, NULL, cli_mgmt_ip_show_route_prefix,
                                  (void *)true, &num_matches);
        bail_error(err);
        if (num_matches == 0) {
            err = cli_printf(_("No statically configured routes.\n"));
            bail_error(err);
        }
        break;
    default:
        bail_force(lc_err_unexpected_case);
    }

 bail:
    ts_free(&gw);
    ts_free(&intf);
    safe_free(pattern);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
cli_mgmt_ip_show_route_prefix(const bn_binding_array *bindings, uint32 idx,
                         const bn_binding *binding, const tstring *name, 
                         const tstr_array *name_components,
                         const tstring *name_last_part,
                         const tstring *value, void *callback_data)
{
    int err = 0;
    const char *nn = NULL;
    tstring *pfx = NULL;
    char *pfx_esc = NULL;
    bn_ipv4prefix pfx_num = lia_ipv4prefix_any;
    bn_ipv4addr pfx_ip_num = lia_ipv4addr_any;
    char *pfx_ip = NULL, *pfx_mask = NULL;
    tbool show_source = (tbool)callback_data;

    nn = ts_str(name);
    bail_null(nn);

    /*
     * Print header on first iteration
     */
    if (idx == 0) {
        if (show_source) {
            err = cli_printf(_("Destination       Mask              Gateway"
                               "           PrefSource\n"));
            bail_error(err);
        }
        else {
            err = cli_printf(_("Destination       Mask              Gateway"
                               "           Interface\n"));
            bail_error(err);
        }
    }

    /*
     * Get the network prefix
     */
    err = bn_binding_name_to_parts_va(ts_str(name), false, 1,
                                      cli_ip_prefix_idx, &pfx);
    bail_error(err);
    err = lia_str_to_ipv4prefix(ts_str(pfx), &pfx_num);
    bail_error(err);

    /*
     * Derive the prefix IP and netmask from the prefix
     */
    err = lia_ipv4prefix_get_ipv4addr(&pfx_num, &pfx_ip_num);
    bail_error(err);

    if (lia_ipv4addr_is_zero_quick(&pfx_ip_num)) {
        pfx_ip = strdup("default");
        bail_null(pfx_ip);
    }
    else {
        err = lia_ipv4addr_to_str(&pfx_ip_num, &pfx_ip);
        bail_error(err);
    }

    err = lia_ipv4prefix_get_mask_str(&pfx_num, &pfx_mask);
    bail_error(err);

    /*
     * And print what we have in 18-character columns (by agreement with
     * the statement that printed the headings above).
     *
     * XXX/EMT: I18N: if the column headers get translated into strings
     * of different lengths, this will be thrown off.
     */
    err = cli_printf_prequeried(bindings,
                                "%-18s%-18s"
                                "#w:18~%s/gw#",
                                pfx_ip, pfx_mask,
                                nn);
    bail_error(err);

    if (show_source) {
        err = cli_printf_prequeried(bindings, "#%s/source#", nn);
        bail_error(err);
    }

    err = cli_printf("\n");
    bail_error(err);
            
 bail:
    ts_free(&pfx);
    safe_free(pfx_esc);
    safe_free(pfx_ip);
    safe_free(pfx_mask);
    return(err);
}

/* ------------------------------------------------------------------------- */
int
cli_ip_help_netmask(void *data, cli_help_type help_type, 
                    const cli_command *cmd, const tstr_array *cmd_line,
                    const tstr_array *params, const tstring *curr_word,
                    void *context)
{
    int err = 0;

    if (help_type == cht_expansion) {
        err = cli_add_expansion_help
            (context, _("<netmask>"), _("e.g. 255.255.255.0"));
        bail_error(err);
        err = cli_add_expansion_help
            (context, _("<mask length>"), _("e.g. /24"));
        bail_error(err);
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Bindings are:
 *     /net/routes/config/ipv4/prefix/<IPV4PREFIX>/nh/<UINT8>/ * /type
 *     /net/routes/config/ipv4/prefix/<IPV4PREFIX>/nh/<UINT8>/ * /gw
 *     /net/routes/config/ipv4/prefix/<IPV4PREFIX>/nh/<UINT8>/ * /interface
 */
static int
cli_ip_revmap_routes(void *data, const cli_command *cmd,
                     const bn_binding_array *bindings,
                     const char *name,
                     const tstr_array *name_parts,
                     const char *value, 
                     const bn_binding *binding,
                     cli_revmap_reply *ret_reply)
{
    int err = 0;
    const char *prefix_str = NULL;
    char *ip_addr_str = NULL, *mask_str = NULL;
    bn_ipv4addr ip_addr = lia_ipv4addr_any, mask = lia_ipv4addr_any;
    bn_ipv4prefix prefix = lia_ipv4prefix_any;
    char *type_name = NULL, *gw_name = NULL, *interface_name = NULL, *src_name = NULL;
    tstring *gw = NULL, *interface = NULL, *src = NULL;
    tstring *rev_cmd = NULL;
    
    prefix_str = tstr_array_get_str_quick(name_parts, 6);
    bail_null(prefix_str);
    
    err = lia_str_to_ipv4prefix(prefix_str, &prefix);
    bail_error(err);

    err = lia_ipv4prefix_get_ipv4addr(&prefix, &ip_addr);
    bail_error(err);
    err = lia_ipv4prefix_get_mask(&prefix, &mask);
    bail_error(err);

    err = lia_ipv4addr_to_str(&ip_addr, &ip_addr_str);
    bail_error(err);

    err = lia_ipv4addr_to_str(&mask, &mask_str);
    bail_error(err);

    type_name = smprintf("%s/type", name);
    bail_null(type_name);

    gw_name = smprintf("%s/gw", name);
    bail_null(gw_name);

    interface_name = smprintf("%s/interface", name);
    bail_null(interface_name);

    src_name = smprintf("%s/source", name);
    bail_null(src_name);

    if (lia_ipv4addr_is_zero_quick(&ip_addr) &&
        lia_ipv4addr_is_zero_quick(&mask)) {
        err = ts_new_str(&rev_cmd, "ip default-gateway");
        bail_error(err);
    }
    else {
        err = ts_new_str(&rev_cmd, "management ip route");
        bail_error(err);

        err = ts_append_sprintf(rev_cmd, " %s %s", ip_addr_str, mask_str);
        bail_error(err);
    }

    err = bn_binding_array_get_value_tstr_by_name
        (bindings, gw_name, NULL, &gw);
    bail_error_null(err, gw);

    if (gw && ts_length(gw) > 0 && strcmp(ts_str(gw), "0.0.0.0")) {
        err = ts_append_sprintf(rev_cmd, " gateway %s", ts_str(gw));
        bail_error(err);
    }

    err = bn_binding_array_get_value_tstr_by_name
        (bindings, interface_name, NULL, &interface);
    bail_error_null(err, interface);

    if (interface && ts_length(interface) > 0) {
        err = ts_append_sprintf(rev_cmd, " %s", ts_str(interface));
        bail_error(err);
    }

    err = bn_binding_array_get_value_tstr_by_name
	(bindings, src_name, NULL, &src);
    bail_error_null(err, src);

    if (src && ts_length(src) > 0) {
	err = ts_append_sprintf(rev_cmd, " source-ip %s", ts_str(src));
	bail_error(err);
    }

    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);

    /* Now account for all of the bindings */
    err = tstr_array_append_str(ret_reply->crr_nodes, type_name);
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_nodes, gw_name);
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_nodes, interface_name);
    bail_error(err);

    err = tstr_array_append_str(ret_reply->crr_nodes, src_name);
    bail_error(err);


 bail:
    safe_free(ip_addr_str);
    safe_free(mask_str);
    safe_free(type_name);
    safe_free(gw_name);
    safe_free(interface_name);
    safe_free(src_name);
    ts_free(&rev_cmd);
    ts_free(&gw);
    ts_free(&interface);
    ts_free(&src);
    return(err);
}
