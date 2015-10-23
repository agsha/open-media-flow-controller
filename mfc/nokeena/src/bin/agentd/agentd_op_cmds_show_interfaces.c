/* Filename :   agentd_op_cmds_show_interfaces.c
 * Date     :    22 Dec 2011
 * Author   :   Vijayekkumaran M
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include "agentd_mgmt.h"
#include "agentd_utils.h"
#include "agentd_op_cmds_base.h"
#include "hash_table.h"
#include "addr_utils_ipv4u32.h"

extern int jnpr_log_level;


extern md_client_context * agentd_mcc;


/* ------------------------------------------------------------------------- */
typedef struct agentd_interface_show_context {
    uint32 cisc_magic;
    tbool cisc_show_all;
    bn_binding_array *cisc_config_bindings;
    bn_binding_array *cisc_addr_bindings;
    bn_binding_array *cisc_vlan_bindings;
    ht_table *cisc_aliases;
    agentd_context_t* agentd_context;
} agentd_interface_show_context;

enum {
    cid_interface_show,
    cid_interface_show_brief,
    cid_interface_show_conf,
} ;

static int agentd_show_ipv6_addresses (agentd_interface_show_context *ctxt, const bn_binding_array * bindings,
                                             const char * node_parent);

static int agentd_show_bonds_worker_one(agentd_context_t *context,const char* bond_name);

static const char * agentd_ts_map_true_false(const tstring* value)
{
    if(value==NULL || strcmp(ts_str(value),"true") != 0  )
        return "No";
    else
        return "Yes";

}


static int agentd_cook_duplex_state_value(tstring* os_duplex)
{
    /*
     * input : "full (auto)", "half (auto)"
     * output : "full" or "half"
     * First we try to find the location of ' '.
     * Once we find the position, we trim everything after that.
    */

    int err=0;
    char c=0;
    int i=0;

    bail_null_quiet(os_duplex);

    err = ts_find_char(os_duplex,0,(int)' ', &i);
    bail_error(err);

    if(i>0)
        ts_trim_num_chars(os_duplex,0,ts_length(os_duplex)-i);

    i=0;

    /*
     * in case of bonds, the duplex value received is "UNKNOWN" (sometimes)
     * in those cases we are reseting the string to empty.
     */
    err = ts_find_str(os_duplex,0,"UNKNOWN",&i);
    bail_error(err);

    /*if UNKNOWN is found the position would not be -1 and zap the os_duplex*/
    if( i != -1)
       ts_trim_num_chars(os_duplex,0,ts_length(os_duplex));
bail:
    return err;
}

static int agentd_check_node_presence(const char* node,const char* arg)
{
    node_name_t node_name = {0};
    int err = 0;
    tstring* var=NULL;

    agentd_mdc_get_value_tstr_fmt(&var,node,arg);
    //The err is not checked.Checking var being or not being NULL is sufficient.

    if(var)
    {
        lc_log_debug(jnpr_log_level,"The Value received is %s",ts_str(var) );
        err = 0;
    }
    else
    {
        lc_log_debug(jnpr_log_level,"Could not get node value" );
        err = 1;
    }
bail:

    ts_free(&var);
    return err;
}


static int agentd_collect_interface_aliases(
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

    if (ts_num_chars(ifdevname) > 0)
    {
        err = ht_lookup(aliases, ifdevname, ts_length(ifdevname), &found,
                        NULL, NULL, &alist_v);
        bail_error(err);
        if (found)
        {
            alist = alist_v;
            err = ts_append_sprintf(alist, ",%s", ts_str(name_last_part));
            bail_error(err);
        }
        else
        {
            err = ht_insert(aliases, ifdevname, ts_length(ifdevname),
                            (void *)name_last_part);
            bail_error(err);
        }
    }

 bail:
    ts_free(&ifdevname);
    return(err);
}

static int agentd_interface_validate(const char *ifname, tbool *ret_valid)
{
    int err =0;

    const char* state_fmt = "/net/interface/state/%s/speed";
    const char* config_fmt= "/net/interface/config/%s/comment";

    *ret_valid = false;

    err = agentd_check_node_presence(state_fmt,ifname);

    if(err ==0)
        *ret_valid = true;
    else
    {
        err = agentd_check_node_presence(config_fmt,ifname);
        if(err ==0) *ret_valid = true;
    }

    lc_log_debug(jnpr_log_level,"Interface availability = %d",*ret_valid==true?1:0);

    return err;
}

static int agentd_show_bond_maybe(agentd_interface_show_context *ctxt,const tstring* name_last_part,const tstring *devsource)
{
    int err = 0;
    agentd_context_t *context = ctxt->agentd_context;

    if( devsource == NULL || ( ts_equal_str(devsource,"bond",false)== false) )
        goto bail;

    err = agentd_show_bonds_worker_one(context,ts_str_maybe(name_last_part) );
bail:
    return err;
}

static int agentd_show_if_alias_names_for_config(agentd_interface_show_context *ctxt, const tstring *alias_list)
{
    int err = 0;
    tstr_array *aliases = NULL;
    const char *tmp = NULL;

    bail_null_quiet(alias_list);

    err = ts_tokenize(alias_list,',','\\','"',0,&aliases);

    bail_null(aliases);

    int len = tstr_array_length_quick(aliases);

    bail_error( (len==0) );


        for(int i = 0;i< len;i++)
        {
            tmp = tstr_array_get_str_quick(aliases,i) ;
            if(!tmp) continue;
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_ALIAS_NAME,tmp );
        }

bail:
    tstr_array_free(&aliases);
    return 0;
}

static int agentd_show_if_aliases(agentd_interface_show_context *ctxt, const tstring* name_last_part)
{
    int err =0;
    char *addr_subtree = NULL;
    uint32 num_addr_bindings = 0, i = 0;
    const bn_binding *addr_binding = NULL;

    node_name_t  amask_bn = {0};
    node_name_t  aifname_bn = {0};
    tstring *amask = NULL, *aifname = NULL, *os_arp=NULL;

    const tstring *addrroot_bn = NULL;
    tstr_array *name_parts_var = NULL;
    const tstr_array *name_parts = NULL;
    const tstring *ifdevname = NULL;
    const char *addr_str = NULL;
    char *mask_len = NULL;

    /* /net/interface/address/state/ifdevname/vlan1249/ipv4addr */
    err = bn_binding_name_append_parts_va
        (&addr_subtree, "/net/interface/address/state/ifdevname",
         2, ts_str(name_last_part), "ipv4addr");
    bail_error_null(err, addr_subtree);

    num_addr_bindings = bn_binding_array_length_quick(ctxt->cisc_addr_bindings);

    lc_log_debug(jnpr_log_level,"into show aliases for %s",ts_str(name_last_part) );

    for (i = 0; i < num_addr_bindings; ++i)
    {
        ts_free(&amask);
        safe_free(mask_len);
        ts_free(&aifname);
        ts_free(&os_arp);

        addr_binding = bn_binding_array_get_quick(ctxt->cisc_addr_bindings, i);
        bail_null(addr_binding);

        err = bn_binding_get_name(addr_binding, &addrroot_bn);
        bail_error(err);

        /*
         * We want to find the parent of the addr nodes, which are like:
         * /net/interface/address/state/ifdevname/vlan1249/ipv4addr/10.99.3.241
         */
        if (!lc_is_prefix(addr_subtree, ts_str(addrroot_bn), false))
        {
            continue;
        }

        tstr_array_free(&name_parts_var);
        name_parts = NULL;

        name_parts = bn_binding_get_name_parts_const_quick(addr_binding);
        if (!name_parts)
        {
            err = bn_binding_get_name_parts(addr_binding, &name_parts_var);
            bail_error(err);
            name_parts = name_parts_var;
        }

        if (tstr_array_length_quick(name_parts) != 8)
        {
            continue;
        }

        err = tstr_array_get(name_parts, 5, (tstring **)&ifdevname);
        bail_error(err);

        err = tstr_array_get_str(name_parts, 7, &addr_str);
        bail_error(err);

        if ( (ts_num_chars(ifdevname) <= 0) || !ts_equal(name_last_part, ifdevname))
            continue;


        snprintf(aifname_bn,sizeof(aifname_bn),"%s/ifname", ts_str(addrroot_bn));

        err = bn_binding_array_get_value_tstr_by_name
            (ctxt->cisc_addr_bindings, aifname_bn, NULL,
             &aifname);
        bail_error(err);
        bail_null(aifname);

        /* Any address on the interface itself is not a secondary */
        if (!ts_str_maybe(aifname) ||
            !safe_strcmp(ts_str(aifname), ts_str(ifdevname)))
        {
            continue;
        }
        snprintf(amask_bn,sizeof(amask_bn),"%s/mask", ts_str(addrroot_bn));
        err = bn_binding_array_get_value_tstr_by_name
            (ctxt->cisc_addr_bindings, amask_bn, NULL, &amask);
        bail_error(err);

        err = lia_ipv4addr_maskspec_to_masklen_str(ts_str(amask),&mask_len);
        bail_error(err);


        err = agentd_binding_array_get_value_tstr_fmt(ctxt->cisc_config_bindings,
                &os_arp,"/net/interface/config/%s/arp",ts_str_maybe_empty(aifname) );
        bail_error(err);

        lc_log_debug(jnpr_log_level,"Secondary address:  %s/%s (alias: '%s')\n", addr_str, mask_len, ts_str(aifname) );

        const char* aindex = NULL;
        aindex = ts_str_nth(aifname,ts_length(name_last_part)+1); //Move past the "<name>:"

        OP_EMIT_TAG_START(ctxt->agentd_context, ODCI_ALIAS_DETAILS);
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_NAME,aindex);
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_IP_ADDRESS,addr_str);
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_NETMASK,mask_len);
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_ARP,agentd_ts_map_true_false(os_arp) );
        OP_EMIT_TAG_END(ctxt->agentd_context, ODCI_ALIAS_DETAILS);
    }

bail:
    ts_free(&os_arp);
    ts_free(&amask);
    ts_free(&aifname);
    safe_free(addr_subtree);

    return 0;
}

static int agentd_show_interfaces_show_stats (
        const bn_binding_array *bindings,
        const tstring* name,
        agentd_interface_show_context *ctxt
        )
{
    int err = 0;

    const char* r = ts_str(name);

    tstring* rx_by=NULL;
    tstring* tx_by=NULL;

    tstring* rx_pa=NULL;
    tstring* tx_pa=NULL;

    tstring* rx_mcast=NULL;

    tstring* rx_disc=NULL;
    tstring* tx_disc=NULL;

    tstring* rx_err=NULL;
    tstring* tx_err=NULL;

    tstring* rx_oruns=NULL;
    tstring* tx_oruns=NULL;

    tstring* tx_carrier=NULL;

    tstring* rx_frame=NULL;

    tstring* tx_colli=NULL;

    tstring* tx_queuelen=NULL;

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&rx_by,"%s/stats/rx/bytes",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&tx_by,"%s/stats/tx/bytes",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&rx_pa,"%s/stats/rx/packets",r);
    bail_error(err);
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&tx_pa,"%s/stats/tx/packets",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&rx_mcast,"%s/stats/rx/multicast_packets",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&rx_disc,"%s/stats/rx/discards",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&tx_disc,"%s/stats/tx/discards",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&rx_err,"%s/stats/rx/errors",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&tx_err,"%s/stats/tx/errors",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&rx_oruns,"%s/stats/rx/overruns",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&tx_oruns,"%s/stats/tx/overruns",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&tx_carrier,"%s/stats/tx/carrier",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&rx_frame,"%s/stats/rx/frame",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&tx_colli,"%s/stats/tx/collisions",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&tx_queuelen,"%s/txqueuelen",r);
    bail_error(err);


        OP_EMIT_TAG_START(ctxt->agentd_context, ODCI_INTERFACE_STATS);
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_RX_BY,ts_str(rx_by) );
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_TX_BY,ts_str(tx_by) );

            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_RX_PA,ts_str(rx_pa) );
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_TX_PA,ts_str(tx_pa) );

            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_RX_MCAST,ts_str(rx_mcast) );

            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_RX_DISC,ts_str(rx_disc) );
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_TX_DISC,ts_str(tx_disc) );

            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_RX_ERR,ts_str(rx_err) );
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_TX_ERR,ts_str(tx_err) );

            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_RX_ORUNS,ts_str(rx_oruns) );
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_TX_ORUNS,ts_str(tx_oruns) );

            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_TX_CARRIER,ts_str(tx_carrier) );

            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_RX_FRAME,ts_str(rx_frame) );

            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_TX_COLLI,ts_str(tx_colli) );

            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_TX_QUEUELEN,ts_str(tx_queuelen) );
        OP_EMIT_TAG_END(ctxt->agentd_context, ODCI_INTERFACE_STATS);
bail:
    {
        ts_free(&rx_by);
        ts_free(&tx_by);

        ts_free(&rx_pa);
        ts_free(&tx_pa);

        ts_free(&rx_mcast);

        ts_free(&rx_disc);
        ts_free(&tx_disc);

        ts_free(&rx_err);
        ts_free(&tx_err);

        ts_free(&rx_oruns);
        ts_free(&tx_oruns);

        ts_free(&tx_carrier);

        ts_free(&rx_frame);

        ts_free(&tx_colli);

        ts_free(&tx_queuelen);
    }
    return err;
}
static int agentd_show_interfaces_show_one_config(
                        const bn_binding_array *bindings, uint32 idx,
                        const bn_binding *binding, const tstring *name,
                        const tstr_array *name_components,
                        const tstring *name_last_part,
                        const tstring *value,
                        void *callback_data)
{
    int err = 0;
    uint8 masklen = 0;
    agentd_interface_show_context *ctxt = NULL;
    const bn_binding_array *config_bindings = NULL;

    node_name_t bn_name = {0};

    tstring *alias_ifdevname = NULL;
    tbool is_alias = false;
    void *alias_list_v = NULL;
    const tstring *alias_list = NULL;
    char* o_netmask = NULL;
    tbool found = false;
    tbool display = true, has_display = false;

    tstring *masklen_tstr = NULL;

    tstring* os_comment=NULL;
    tstring* os_enabled=NULL;
#ifdef PROD_FEATURE_DHCP_CLIENT
    tstring* os_dhcp=NULL;
#endif /*PROD_FEATURE_DHCP_CLIENT*/

#ifdef PROD_FEATURE_ZEROCONF
    tstring* os_zeroconf=NULL;
#endif /*PROD_FEATURE_ZEROCONF*/
    tstring* os_ip_addr=NULL;

    tstring* os_alias_of=NULL;

    tstring* os_speed=NULL;
    tstring* os_duplex=NULL;
    tstring* os_mtu=NULL;
    tstring* os_arp=NULL;
    tstring* s_devsource=NULL;

    tstring * os_ipv6_enabled = NULL;

    const char* r = ts_str(name);
    bail_null(r);
    bail_null(callback_data);

    ctxt = callback_data;

    config_bindings = bindings;

    if (ctxt->cisc_show_all)
    {
       snprintf(bn_name,sizeof(bn_name),"/net/interface/config/%s/display",ts_str(name_last_part));

       err = bn_binding_array_get_value_tbool_by_name(config_bindings, bn_name, &has_display, &display);
       bail_error_msg(err, "Failed to get IF display configuration");

       if (!display)
           goto bail;
       /*
        * If has_display is false,
        * If we're showing interface configuration, something's
        * wrong, since we were iterating only over interfaces that
        * had configuration...
        */
       if (!has_display)
       {
           lc_log_basic(LOG_WARNING, I_("Could not find node %s"),bn_name);
           goto bail;
       }
    }

    err = bn_binding_array_get_value_tstr_by_name_fmt( config_bindings, NULL,&alias_ifdevname,
            "/net/interface/config/%s/alias/ifdevname",ts_str(name_last_part)
           );

    bail_error(err);

    if (alias_ifdevname && (ts_length(alias_ifdevname) > 0))
    {
       is_alias = true;
    }


    err=agentd_binding_array_get_value_tstr_fmt(bindings,&masklen_tstr,"%s/addr/ipv4/static/1/mask_len",r);
    bail_error(err);

    if (masklen_tstr)
    {
        err = lia_masklen_str_to_ipv4addr_str(ts_str(masklen_tstr), &o_netmask);
        bail_error(err);
    }
    else
    {
        o_netmask = strdup("");
        bail_null(o_netmask);
    }

    lc_log_debug(jnpr_log_level,"netmask of %s = %s",name?ts_str(name):"*", o_netmask );


    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_comment,"%s/comment",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_enabled,"%s/enable",r);
    bail_error(err);

#ifdef PROD_FEATURE_DHCP_CLIENT
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_dhcp,"%s/addr/ipv4/dhcp",r);
#endif /*PROD_FEATURE_DHCP_CLIENT*/

#ifdef PROD_FEATURE_ZEROCONF
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_zeroconf,"%s/addr/ipv4/zeroconf",r);
#endif /* PROD_FEATURE_ZEROCONF */

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_arp,"%s/arp",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_ip_addr,"%s/addr/ipv4/static/1/ip",r);
    lc_log_debug(jnpr_log_level,"IP Addr = %s",ts_str(os_ip_addr) );

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_ipv6_enabled,"%s/addr/ipv6/enable",r);
    bail_error(err);

    err = ht_lookup(ctxt->cisc_aliases, name_last_part,
                    ts_length(name_last_part), &found, NULL, NULL,
                    &alias_list_v);
    bail_error(err);
    alias_list = alias_list_v;

    if (alias_list && ts_num_chars(alias_list) > 0)
    {
        lc_log_debug(jnpr_log_level,"Aliases : %s",ts_str(alias_list));
    }
    if (is_alias)
    {
        err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_alias_of,"%s/alias/ifdevname",r);
        bail_error(err);

        lc_log_debug(jnpr_log_level,"Alias of = %s",ts_str(os_alias_of) );
    }
    else
    {
        err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_speed,"%s/type/ethernet/speed",r);
        bail_error(err);

        err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_duplex,"%s/type/ethernet/duplex",r);
        bail_error(err);

        err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_mtu,"%s/mtu",r);
        bail_error(err);

        lc_log_debug(jnpr_log_level,"speed = %s",ts_str(os_speed) );
        lc_log_debug(jnpr_log_level,"duplex = %s",ts_str(os_duplex) );
        lc_log_debug(jnpr_log_level,"mtu = %s",ts_str(os_mtu) );
    }

    { //This block retrieves the devsource needed to check if the if is bond
        agentd_mdc_get_value_tstr_fmt(&s_devsource,"/net/interface/state/%s/devsource",ts_str_maybe(name_last_part) );
    }
        OP_EMIT_TAG_START(ctxt->agentd_context, ODCI_INTERFACE_CONFIGURED);
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_NAME,ts_str_maybe_empty(name_last_part) );
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_COMMENT,ts_str_maybe_empty(os_comment) );
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_ENABLED,agentd_ts_map_true_false(os_enabled) );
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_DHCP,agentd_ts_map_true_false(os_dhcp) );
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_ZEROCONF,agentd_ts_map_true_false(os_zeroconf) );
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_IP_ADDRESS,ts_str_maybe_empty(os_ip_addr) );
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_NETMASK,o_netmask );
            OP_EMIT_TAG_START(ctxt->agentd_context, ODCI_IPV6);
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_ENABLED, agentd_ts_map_true_false(os_ipv6_enabled));
                agentd_show_ipv6_addresses (ctxt, bindings, r);
            OP_EMIT_TAG_END(ctxt->agentd_context, ODCI_IPV6);
            OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_ARP,agentd_ts_map_true_false(os_arp) );

            agentd_show_bond_maybe(ctxt,name_last_part,s_devsource);

            if(is_alias)
            {
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_ALIAS_OF,ts_str_maybe_empty(alias_ifdevname) );
            }
            else
            {
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_SPEED,ts_str_maybe_empty(os_speed) );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_DUPLEX,ts_str_maybe_empty(os_duplex) );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_MTU,ts_str_maybe_empty(os_mtu) );
                agentd_show_if_alias_names_for_config(ctxt,alias_list);
            }
        OP_EMIT_TAG_END(ctxt->agentd_context, ODCI_INTERFACE_CONFIGURED);

bail:
     lc_log_debug(jnpr_log_level,"crossed bail...");
     ts_free(&alias_ifdevname);
     ts_free(&masklen_tstr);
     safe_free(o_netmask);

     { //This block is to free objects created to emit XML content

         ts_free(&os_comment);
         ts_free(&os_enabled);
     #ifdef PROD_FEATURE_DHCP_CLIENT
         ts_free(&os_dhcp);
     #endif /*PROD_FEATURE_DHCP_CLIENT*/

     #ifdef PROD_FEATURE_ZEROCONF
         ts_free(&os_zeroconf);
     #endif /*PROD_FEATURE_ZEROCONF*/
         ts_free(&os_ip_addr);

         ts_free(&os_alias_of);

         ts_free(&os_speed);
         ts_free(&os_duplex);
         ts_free(&os_mtu);
         ts_free(&s_devsource);
         ts_free(&os_ipv6_enabled);
     }
    return err;
}

static int agentd_show_interfaces_show_one_state(
                        const bn_binding_array *bindings, uint32 idx,
                        const bn_binding *binding, const tstring *name,
                        const tstr_array *name_components,
                        const tstring *name_last_part,
                        const tstring *value,
                        void *callback_data)
{
    int err = 0;
    lc_log_debug(jnpr_log_level, "showing interface %s = %s", ts_str(name), ts_str(name_last_part) );

    uint8 masklen = 0;
    const char *r = NULL;
    tstring *masklen_tstr = NULL;
    char *o_netmask = NULL;
    uint32 show_type = 0;

    agentd_interface_show_context *ctxt = NULL;

    tbool display = true;
    node_name_t bn_name = {0};
    str_value_t o_speed =  {0};

    tstring *alias_ifdevname = NULL;
    tbool is_alias = false;

    const bn_binding_array *config_bindings = NULL;

    node_name_t  node_name = {0};

    tstring* os_comment=NULL;
    tstring* os_ip_addr=NULL;

    tstring* os_speed=NULL;
    tstring* os_duplex=NULL;
    tstring* os_mtu=NULL;

    tstring* os_admin_up=NULL;
    tstring* os_link_up=NULL;

    tstring* os_devsource=NULL;
    tstring* os_hwaddr=NULL;

    tstring* os_iftype=NULL;
    tstring* os_arp=NULL;

    tstring* os_dhcp=NULL;
    tstring *os_ipv6_enabled=NULL;

    bail_null(callback_data);

    ctxt = callback_data;
    show_type = ctxt->cisc_magic;

    config_bindings = ctxt->cisc_config_bindings;

    if (ctxt->cisc_show_all)
    {
        snprintf(bn_name,sizeof(bn_name),"/net/interface/config/%s/display",
                           ts_str(name_last_part));

        err = bn_binding_array_get_value_tbool_by_name(config_bindings, bn_name, NULL, &display);
        bail_error_msg(err, "Failed to get IF display configuration");

        if (!display)
        {
            goto bail;
        }
    }

    err = bn_binding_array_get_value_tstr_by_name_fmt( config_bindings, NULL,
            &alias_ifdevname,"/net/interface/config/%s/alias/ifdevname",
            ts_str(name_last_part)
            );

    bail_error(err);

    if (alias_ifdevname && (ts_length(alias_ifdevname) > 0))
    {
        is_alias = true;
    }

    r = ts_str(name);

    bail_null(r);


    err=agentd_binding_array_get_value_tstr_fmt(bindings,&masklen_tstr,"%s/addr/ipv4/1/mask_len",r);
    bail_error(err);

    if (masklen_tstr)
    {
        err =lia_masklen_str_to_ipv4addr_str(ts_str(masklen_tstr), &o_netmask);
        bail_error(err);
    }
    else
    {
        o_netmask = strdup("");
        bail_null(o_netmask);
    }

    lc_log_debug(jnpr_log_level,"netmask of %s = %s",name?ts_str(name):"*", o_netmask );



    /* Run-time interface state */

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_admin_up,"%s/flags/oper_up",r);
    bail_error(err);
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_link_up,"%s/flags/link_up",r);
    bail_error(err);
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_ip_addr,"%s/addr/ipv4/1/ip",r);
    bail_error(err);
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_devsource,"%s/devsource",r);
    bail_error(err);
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_speed,"%s/type/ethernet/speed",r);
    bail_error(err);
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_duplex,"%s/type/ethernet/duplex",r);
    bail_error(err);
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_iftype,"%s/iftype",r);
    bail_error(err);
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_mtu,"%s/mtu",r);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(config_bindings,&os_comment,"/net/interface/config/%s/comment",ts_str(name_last_part));
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(config_bindings,&os_arp,"/net/interface/config/%s/arp",ts_str(name_last_part));
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_ipv6_enabled,"%s/addr/settings/ipv6/enable",r);
    bail_error(err);
    
#ifdef PROD_FEATURE_DHCP_CLIENT
    err=agentd_binding_array_get_value_tstr_fmt(config_bindings,&os_dhcp,"/net/interface/config/%s/addr/ipv4/dhcp",ts_str(name_last_part) );
    bail_error(err);
#endif /*PROD_FEATURE_DHCP_CLIENT*/


    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_hwaddr,"%s/type/ethernet/mac",r);
    bail_error(err);

    if (
            !os_hwaddr ||
       ts_length(os_hwaddr) == 0 ||
       ts_cmp_str(os_hwaddr, "N/A", false) == 0
      )
    {
        /*
         * The ethernet mac was empty, see if the top level "hwaddr"
         * has something.
         */
        ts_free(&os_hwaddr);
        err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_hwaddr,"%s/hwaddr",r);
        bail_error(err);
    }

    {//This block does the necessary manipulations on the collected data

        /*
         * the retrieved speed value is like : "1000Mb/s (auto)" or "100Mb/s"
         * we need to only pass the numeric part of the value to MFA
         * */
        unsigned int speed = strtoul(ts_str_maybe_empty(os_speed),NULL,10);
        if(speed>0) snprintf(o_speed,sizeof(o_speed),"%u",speed);

        /*
         * The retrieved duplex has value like : "full (auto)", "half (auto)"
         * but we need to prove only : full or half to MFA
         */
        err = agentd_cook_duplex_state_value(os_duplex);
        bail_error(err);
    }
    { //This block of code is used to do XML output

        int showStats = (show_type != cid_interface_show_brief) && !is_alias;

            OP_EMIT_TAG_START(ctxt->agentd_context, ODCI_INTERFACE_BRIEF);
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_NAME,ts_str_maybe_empty(name_last_part) );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_ADMIN_STATE,agentd_ts_map_true_false(os_admin_up) );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_LINK_STATE,agentd_ts_map_true_false(os_link_up) );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_IP_ADDRESS,ts_str_maybe_empty(os_ip_addr) );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_NETMASK,o_netmask );
                OP_EMIT_TAG_START(ctxt->agentd_context, ODCI_IPV6);
                    OP_EMIT_TAG_VALUE (ctxt->agentd_context, ODCI_ENABLED, agentd_ts_map_true_false(os_ipv6_enabled) );
                    agentd_show_ipv6_addresses (ctxt, bindings, r);
                OP_EMIT_TAG_END(ctxt->agentd_context, ODCI_IPV6);
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_SPEED,o_speed );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_DUPLEX,ts_str_maybe_empty(os_duplex) );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_INTERFACE_TYPE,ts_str_maybe_empty(os_iftype) );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_INTERFACE_SOURCE,ts_str_maybe_empty(os_devsource) );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_MTU,ts_str_maybe_empty(os_mtu) );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_HW_ADDRESS,ts_str_maybe_empty(os_hwaddr) );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_COMMENT,ts_str_maybe_empty(os_comment) );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_ARP,agentd_ts_map_true_false(os_arp) );
                OP_EMIT_TAG_VALUE(ctxt->agentd_context, ODCI_DHCP,agentd_ts_map_true_false(os_dhcp) );

                agentd_show_if_aliases(ctxt,name_last_part);

                agentd_show_bond_maybe(ctxt,name_last_part,os_devsource);

            OP_EMIT_TAG_END(ctxt->agentd_context, ODCI_INTERFACE_BRIEF);

            if (showStats)
            {
                agentd_show_interfaces_show_stats(bindings,name,ctxt);
            }
    }

 bail:
    lc_log_debug(jnpr_log_level,"crossed bail...");
    ts_free(&alias_ifdevname);

    ts_free(&masklen_tstr);
    safe_free(o_netmask);

    { //This block is to free objects created to emit XML content

        ts_free(&os_comment);

        ts_free(&os_ip_addr);

        ts_free(&os_speed);
        ts_free(&os_duplex);
        ts_free(&os_mtu);

        ts_free(&os_admin_up);
        ts_free(&os_link_up);

        ts_free(&os_devsource);
        ts_free(&os_hwaddr);

        ts_free(&os_iftype);
        ts_free(&os_ipv6_enabled);
    };
    return(err);
}

static int agentd_show_interfaces_worker_for_config(agentd_context_t* context, int show_type,const char* if_name)
{
    int err = 0;
    uint32 num_matches = 0;
    node_name_t pattern = {0};
    tbool show_all = false, valid = false;
    bn_binding_array *bindings = NULL;
    agentd_interface_show_context ctxt;

    ht_table *aliases = NULL;

    if (if_name == NULL)
    {
        if_name = "*";
        show_all = true;
    }
    else
    {
        err = agentd_interface_validate(if_name, &valid);
        bail_error(err);

        if (!valid)
        {
            goto bail;
        }
    }

    memset(&ctxt, 0, sizeof(ctxt));

    ctxt.agentd_context = context;
    ctxt.cisc_magic = show_type;
    ctxt.cisc_show_all = show_all;

    {
        snprintf(pattern,sizeof(pattern),"/net/interface/config/%s", if_name);

        err = mdc_get_binding_children(agentd_mcc,NULL, NULL, true, &bindings, true, true,
               "/net/interface/config");
        bail_error(err);

        err = bn_binding_array_sort(bindings);
        bail_error(err);

    }

    { //This block collects the aliases into the ht
        err = ht_new_tstring_tstring(&aliases, true);
        bail_error_null(err, aliases);

        err = mdc_foreach_binding_prequeried(bindings, "/net/interface/config/*", NULL,
                agentd_collect_interface_aliases, aliases, NULL);
        bail_error(err);

        ctxt.cisc_aliases = aliases;
    }

    err = mdc_foreach_binding_prequeried(bindings, pattern, NULL, agentd_show_interfaces_show_one_config,
         &ctxt, &num_matches);
    bail_error(err);

    if (num_matches == 0)
    {
        if (show_all)
        {
            lc_log_debug(jnpr_log_level, "There are no configured interfaces.\n");
        }
        else
        {
            lc_log_debug(jnpr_log_level,"There is no configuration for that interface %s.\n",if_name);
        }
    }

 bail:
    bn_binding_array_free(&bindings);
    ht_free(&aliases);

    return(err);
}

static int agentd_show_interfaces_worker_for_state(agentd_context_t* context, int show_type,const char* if_name)
{
    int err = 0;
    uint32 num_matches = 0;
    node_name_t pattern = {0};
    tbool show_all = false, valid = false;
    bn_binding_array *bindings = NULL;
    agentd_interface_show_context ctxt;
    bn_binding_array *addr_bindings = NULL;
    bn_binding_array *config_bindings = NULL;

    if (if_name == NULL)
    {
        if_name = "*";
        show_all = true;
    }
    else
    {
        err = agentd_interface_validate(if_name, &valid);
//        bail_error(err); //Do not check error since the error is only when the "valid" returned is false.
        err=0;
        if (!valid)
        {
            goto bail;
        }
    }

    memset(&ctxt, 0, sizeof(ctxt));

    ctxt.agentd_context = context;
    ctxt.cisc_magic = show_type;
    ctxt.cisc_show_all = show_all;

    /* Get all the /net/interface/config we need */
    snprintf(pattern,sizeof(pattern),"/net/interface/config/%s", if_name);

    err = mdc_get_binding_children(agentd_mcc,NULL, NULL, true,
            &config_bindings, true, true,"/net/interface/config");
    bail_error(err);

    err = bn_binding_array_sort(config_bindings);
    bail_error(err);
    ctxt.cisc_config_bindings = config_bindings;

    /* Get all the /net/interface/address/state/ifdevname we need */
    snprintf(pattern,sizeof(pattern),"/net/interface/address/state/ifdevname/%s",if_name);

    err = mdc_get_binding_children(agentd_mcc,NULL, NULL, true, &addr_bindings,false, true,
         "/net/interface/address/state/ifdevname");
    bail_error(err);
    err = bn_binding_array_sort(addr_bindings);
    bail_error(err);
    ctxt.cisc_addr_bindings = addr_bindings;

    /* Get all the /net/interface/state we need */
    snprintf(pattern,sizeof(pattern),"/net/interface/state/%s", if_name);

    err = mdc_get_binding_children(agentd_mcc,
         NULL, NULL, true, &bindings, true, true,
         (show_all) ? "/net/interface/state" : pattern);
    bail_error(err);
    err = bn_binding_array_sort(bindings);
    bail_error(err);

    /* Now for each interface, print our block */
    err = mdc_foreach_binding_prequeried(bindings, pattern, NULL, agentd_show_interfaces_show_one_state,
         &ctxt, &num_matches);
    bail_error(err);
    if (num_matches == 0)
    {
        if (show_all)
        {
            lc_log_debug(jnpr_log_level, "No interfaces are detected.\n");
        }
        else
        {
            lc_log_debug(jnpr_log_level,"No interface with that name is detected.\n");
        }
    }

 bail:
    bn_binding_array_free(&bindings);
    bn_binding_array_free(&addr_bindings);
    bn_binding_array_free(&config_bindings);

    return(err);
}
int agentd_show_interface_details(agentd_context_t *context, void *data)
{
    const char const * brief_str        = "brief";
    const char const * configured_str   = "configured";
    int show_type = cid_interface_show;

    int err = 0;
    unsigned int length = 0;
    int extra_params =  0;

    const char* mfc_cluster_name = NULL;
    const char* member_node_name = NULL;
    const char* bond_name = NULL;
    const char* tmp = NULL;
    const char* if_name = NULL;

    oper_table_entry_t *cmd = (oper_table_entry_t *)data;

    length = tstr_array_length_quick(context->op_cmd_parts);

    lc_log_debug(jnpr_log_level, "got %s, cmd-parts-size=%d", cmd->pattern,length );

    agentd_dump_op_cmd_details(context);

    extra_params = length - POS_MFC_MEMBER_NODE_NAME - 1;

    if(extra_params<0)
    {
        err = 1;
        bail_error_msg(err,"param count is less than needed  = %d",extra_params);
    }

    mfc_cluster_name = tstr_array_get_str_quick(context->op_cmd_parts,POS_MFC_CLUSTER_NAME) ;
    member_node_name = tstr_array_get_str_quick(context->op_cmd_parts,POS_MFC_MEMBER_NODE_NAME) ;

    lc_log_debug(jnpr_log_level, "MFC Cluster Name =  %s Member Node Name = %s extra params = %d", mfc_cluster_name?:"",member_node_name?:"",extra_params);

    if(extra_params)
    {
        tmp = tstr_array_get_str_quick(context->op_cmd_parts,POS_MFC_MEMBER_NODE_NAME+1 ) ;

        if(strcmp(tmp,brief_str) == 0)
        {
            show_type = cid_interface_show_brief;
        }
        else if(strcmp(tmp,configured_str) == 0)
        {
            show_type = cid_interface_show_conf;
        }
        else
        {
            if_name = tmp;
        }
        if(extra_params > 1) //if we had received 2 params
            if_name = tstr_array_get_str_quick(context->op_cmd_parts,length-1 ) ;
    }

    lc_log_debug(jnpr_log_level, "interfaces command details member-node = %s | show_type = %d interface = %s",
            member_node_name?:"",
            show_type,
            if_name?:""
            );

        OP_EMIT_TAG_START(context, ODCI_INTERFACES_DETAIL);

        get_interface_details(context,data);

        if(show_type == cid_interface_show_conf)
            err = agentd_show_interfaces_worker_for_config(context,show_type,if_name);
        else if(show_type == cid_interface_show_brief)
            err = agentd_show_interfaces_worker_for_state(context,show_type,if_name);
        else
        {
            err = agentd_show_interfaces_worker_for_config(context,show_type,if_name);
            err = agentd_show_interfaces_worker_for_state(context,show_type,if_name);
        }
        OP_EMIT_TAG_END(context, ODCI_INTERFACES_DETAIL);

//    bail_error(err);

    return 0;
bail:
    context->ret_code = 404; //TODO need to find the right error code

    snprintf(context->ret_msg,sizeof(context->ret_code),"Error getting the interface details %s",if_name?:"");
    return err;
}



static int agentd_bonding_show_one_bond_if(const bn_binding_array *bindings,
                         uint32 idx, const bn_binding *binding,
                         const tstring *name,
                         const tstr_array *name_components,
                         const tstring *name_last_part,
                         const tstring *value,
                         void *callback_data)
{
    int err = 0;

    agentd_context_t* ctxt= callback_data;

    lc_log_debug(jnpr_log_level,"Interface %s\n", ts_str(name_last_part));

       OP_EMIT_TAG_VALUE(ctxt, ODCI_INTERFACE_NAME,ts_str(name_last_part) );

bail:
    return(err);
}
static int agentd_bonding_show_one(
                    const bn_binding_array *bindings, uint32 idx,
                    const bn_binding *binding, const tstring *name,
                    const tstr_array *name_components,
                    const tstring *name_last_part,
                    const tstring *value, void *callback_data)
{
    int err = 0;
    const char *node_name = NULL; /* node name */
    const char *bonding = NULL;

    uint32 num_intfs = 0;

    node_name_t node_name_tmp = {0};

    agentd_context_t* ctxt = callback_data;

    tstring* os_link_mon_time=NULL;
    tstring* os_up_delay_time=NULL;
    tstring* os_down_delay_time=NULL;

    tstring* os_name=NULL;
    tstring* os_enabled=NULL;
    tstring* os_mode=NULL;

    bail_null(ctxt);

    node_name = ts_str(name);
    bail_null(node_name);

    bonding = ts_str(name_last_part);
    bail_null(bonding);

    lc_log_debug(jnpr_log_level,"Showing bond %s and last part = %s",node_name,bonding);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_link_mon_time,"/net/bonding/config/%s/link-mon-time", bonding);
    bail_error(err);
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_up_delay_time,"/net/bonding/config/%s/up-delay-time", bonding);
    bail_error(err);
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_down_delay_time,"/net/bonding/config/%s/down-delay-time", bonding);
    bail_error(err);

    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_name,    "%s/name",  node_name);
    bail_error(err);
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_enabled, "%s/enable",node_name);
    bail_error(err);
    err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_mode,    "%s/mode",  node_name);
    bail_error(err);

    { //This block outputs xml content
            OP_EMIT_TAG_START(ctxt, ODCI_BOND_DETAIL);
                OP_EMIT_TAG_VALUE(ctxt, ODCI_NAME,ts_str(os_name) );
                OP_EMIT_TAG_VALUE(ctxt, ODCI_ENABLED,agentd_ts_map_true_false(os_enabled) );
                OP_EMIT_TAG_VALUE(ctxt, ODCI_MODE,ts_str(os_mode) );
                OP_EMIT_TAG_VALUE(ctxt, ODCI_LINK_MON_TIME,ts_str(os_link_mon_time) );
                OP_EMIT_TAG_VALUE(ctxt, ODCI_DOWN_DEL_TIME,ts_str(os_down_delay_time) );
                OP_EMIT_TAG_VALUE(ctxt, ODCI_UP_DEL_TIME,ts_str(os_up_delay_time) );
                snprintf(node_name_tmp,sizeof(node_name_tmp),"/net/bonding/config/%s/interface/*", bonding);
                err = mdc_foreach_binding_prequeried(bindings,node_name_tmp, NULL, agentd_bonding_show_one_bond_if,ctxt,&num_intfs);
            OP_EMIT_TAG_END(ctxt, ODCI_BOND_DETAIL);
    }

    bail_error(err);

    if (num_intfs == 0)
        lc_log_debug(jnpr_log_level,"No interfaces for %s",ts_str(os_name));

bail:

    ts_free(&os_link_mon_time);
    ts_free(&os_up_delay_time);
    ts_free(&os_down_delay_time);

    ts_free(&os_name);
    ts_free(&os_enabled);
    ts_free(&os_mode);

    return(err);
}

static int agentd_show_bonds_worker_one(agentd_context_t *context,const char* bond_name)
{
    int err = 0;
    uint32 num_bonds = 0;
    bn_binding_array *bindings = NULL;
    node_name_t bonding_bn = {0};
    uint32_array *bif_indices = NULL;
    int32 db_rev = 0;
    uint32 num_bifs = 0;
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;

    bail_null(bond_name);

    err = bn_binding_array_new(&barr);
    bail_error(err);

    err = bn_binding_new_str_autoinval(&binding, "name", ba_value,
            bt_string, 0, bond_name);
    bail_error_null(err, binding);

    err = bn_binding_array_append_takeover(barr, &binding);
    bail_error(err);

    err = mdc_array_get_matching_indices(agentd_mcc,"/net/bonding/config",
            NULL, barr, bn_db_revision_id_none,
         &db_rev, &bif_indices, NULL, NULL);
    bail_error(err);

    num_bifs = uint32_array_length_quick(bif_indices);
    bail_error_msg((num_bifs==0),"bond index received is 0..");

    if(num_bifs>1)
        lc_log_debug(jnpr_log_level,"recived more than one bonds (%d) for given name-%s.. will show the first",num_bifs,bond_name);

    snprintf(bonding_bn,sizeof(bonding_bn),"/net/bonding/config/%d",
                          uint32_array_get_quick(bif_indices, 0));

    lc_log_debug(jnpr_log_level,"going to get bond details under %s",bonding_bn);

    err = mdc_get_binding_children(agentd_mcc,NULL, NULL, false,
            &bindings, true, true, bonding_bn);
    bail_error(err);

//        OP_EMIT_TAG_START(context, ODCI_BOND_LIST);
        err = mdc_foreach_binding_prequeried(bindings, bonding_bn, NULL,
                agentd_bonding_show_one, context,&num_bonds);
//        OP_EMIT_TAG_END(context, ODCI_BOND_LIST);

     bail_error(err);

    if (num_bonds == 0)
        lc_log_debug(jnpr_log_level,"Could not get config nodes of bond=%s",bonding_bn);

 bail:
    bn_binding_array_free(&bindings);
    bn_binding_free(&binding);
    bn_binding_array_free(&barr);
    uint32_array_free(&bif_indices);
    return(err);
}
static int agentd_show_bonds_worker_all(agentd_context_t *context)
{
    int err = 0;
    uint32 num_bonds = 0;
    bn_binding_array *bindings = NULL;

    err = mdc_get_binding_children(agentd_mcc,NULL, NULL, false,
            &bindings, false, true, "/net/bonding/config");
    bail_error(err);

//         OP_EMIT_TAG_START(context, ODCI_BOND_LIST);
              err = mdc_foreach_binding_prequeried(bindings, "/net/bonding/config/*",
                   NULL, agentd_bonding_show_one,context, &num_bonds);
//         OP_EMIT_TAG_END(context, ODCI_BOND_LIST);


    bail_error(err);

    if (num_bonds == 0)
    {
        lc_log_debug(jnpr_log_level,"No bonded interfaces configured.\n");
        bail_error(err);
    }

 bail:
    bn_binding_array_free(&bindings);
    return(err);
}

int agentd_show_bond_details(agentd_context_t *context, void *data)
{
    int err = 0;
    unsigned int length = 0;
    int extra_params =  0;
    const char* mfc_cluster_name = NULL;
    const char* member_node_name = NULL;
    const char* bond_name = NULL;
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;

    length = tstr_array_length_quick(context->op_cmd_parts);

    lc_log_debug(jnpr_log_level, "got %s, cmd-parts-size=%d", cmd->pattern,length );

    agentd_dump_op_cmd_details(context);

    extra_params = length-POS_MFC_MEMBER_NODE_NAME-1;

    if(extra_params<0)
    {
        err = 1;
        bail_error_msg(err,"param count is less than needed  = %d",extra_params);
    }

    mfc_cluster_name = tstr_array_get_str_quick(context->op_cmd_parts,POS_MFC_CLUSTER_NAME) ;
    member_node_name = tstr_array_get_str_quick(context->op_cmd_parts,POS_MFC_MEMBER_NODE_NAME) ;

    lc_log_debug(jnpr_log_level, "MFC Cluster Name =  %s Member Node Name = %s extra params = %d", mfc_cluster_name?:"",member_node_name?:"",extra_params);

    if(extra_params)
    {
        bond_name = tstr_array_get_str_quick(context->op_cmd_parts,POS_MFC_MEMBER_NODE_NAME+1 ) ;
    }

    lc_log_debug(jnpr_log_level, "Show Bond details for = %s",bond_name?:"*");

        OP_EMIT_TAG_START(context, ODCI_BOND_LIST);
        if(bond_name)
            err = agentd_show_bonds_worker_one(context,bond_name);
        else
            err = agentd_show_bonds_worker_all(context);
        OP_EMIT_TAG_END(context, ODCI_BOND_LIST);

    bail_error(err);

bail:

    if(err)
    {
        lc_log_debug(jnpr_log_level,"Error processing command = %d",err);
        context->ret_code = 404; //TODO need to find the right error code
        snprintf(context->ret_msg,sizeof(context->ret_code),"Error getting the interface details %s",bond_name?:"");
    }

    return err;
}

static int agentd_show_ipv6_addresses (agentd_interface_show_context *ctxt, const bn_binding_array * bindings,
                                             const char * node_parent) {
    int err = 0;
    node_name_t ipv6_addr_wc_name = {0}, ipv6_addr_name = {0};
    int ipv6_wc_pos = 0;
    tstr_array * static_ids = NULL;
    uint32 num_addrs = 0, i = 0;
    const char * static_id = NULL;
    tstring * os_ipv6_addr = NULL;
    tstring * os_ipv6_mask_len = NULL;
    str_value_t ipv6_addr_mask = {0};

    lc_log_debug (jnpr_log_level, "IPv6 addresses from node :%s", node_parent);
    if (strstr (node_parent, "/net/interface/config") ) { 
        snprintf(ipv6_addr_name, sizeof(ipv6_addr_name), 
                  "%s/addr/ipv6/static", node_parent);
        snprintf(ipv6_addr_wc_name, sizeof(ipv6_addr_wc_name), 
                  "%s/addr/ipv6/static/*", node_parent);
        ipv6_wc_pos = 7;
    } else if (strstr (node_parent, "/net/interface/state") ) {
        snprintf(ipv6_addr_name, sizeof(ipv6_addr_name), 
                  "%s/addr/ipv6", node_parent);
        snprintf (ipv6_addr_wc_name, sizeof(ipv6_addr_wc_name),
                  "%s/addr/ipv6/*", node_parent);
        ipv6_wc_pos = 6;
    } else {
        lc_log_basic (LOG_ERR, "Incorrect parent node for gettng ipv6 addresses");
        err = 1;
        bail_error (err);
    }
    err = bn_binding_array_get_name_part_tstr_array(bindings,
                                                    ipv6_addr_wc_name,
                                                    ipv6_wc_pos,
                                                    &static_ids);
    bail_error (err);
    num_addrs = tstr_array_length_quick (static_ids);
    lc_log_debug (jnpr_log_level, "Number of IPv6 addresses :%d", num_addrs);
    
    for (i = 0; i < num_addrs; i++) {
        static_id = tstr_array_get_str_quick (static_ids, i);
        bail_null (static_id);
        
        err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_ipv6_addr,
                       "%s/%s/ip",ipv6_addr_name, static_id);
        bail_error(err);
        err=agentd_binding_array_get_value_tstr_fmt(bindings,&os_ipv6_mask_len,
                     "%s/%s/mask_len",ipv6_addr_name, static_id);
        bail_error(err);
      
        snprintf (ipv6_addr_mask , sizeof (ipv6_addr_mask), 
                   "%s/%s", ts_str(os_ipv6_addr), ts_str(os_ipv6_mask_len));
        OP_EMIT_TAG_VALUE (ctxt->agentd_context, ODCI_ADDRESS, ipv6_addr_mask);
        ts_free (&os_ipv6_addr); 
        ts_free (&os_ipv6_mask_len); 
    }

bail:
    tstr_array_free (&static_ids);
    ts_free (&os_ipv6_addr);
    ts_free (&os_ipv6_mask_len);
    return err;
}


