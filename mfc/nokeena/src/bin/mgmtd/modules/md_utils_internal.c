/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */


#include <netinet/in.h>

#ifdef PROD_TARGET_OS_LINUX
#undef __STRICT_ANSI__ /* rtnetlink.h fails on i386 without this */
#include <linux/route.h>
#include <linux/ipv6_route.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <bits/sockaddr.h>
#include <asm/types.h>
#endif

#include <arpa/inet.h>

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "common.h"
#include "file_utils.h"
#include "bswap.h"
#include "md_main.h"
#include "md_utils.h"
#include "md_utils_internal.h"
#include "md_upgrade.h"
#include "tpaths.h"
#include "tnet_utils.h"
#include "mdmod_common.h"
#include "md_commit.h"
#include "md_commit_internal.h"

/* an AF_INET socket used for ioctl() interaction about interfaces */
static int md_interface_sock = -1;

static const lt_dur_ms mnu_routing_max_patience_ms = 3000;

#define MD_NETLINK_LOG_LEVEL LOG_INFO

#define SNDBUF_SIZE     1024
#define ROUTE_BUF_SIZE    64   /* More than needed, but no harm */

TYPED_ARRAY_IMPL_NEW_NONE(md_mgmt_route_rec, md_mgmt_route_rec *, NULL);

TYPED_ARRAY_IMPL_NEW_NONE(md_intf_network, md_intf_network *, NULL);

TYPED_ARRAY_IMPL_NEW_NONE(md_if_addr_rec, md_if_addr_rec *, NULL);

typedef struct md_net_utils_get_addr_context {
    tstr_array *mngac_addrs;
    tbool mngac_include_link_local;
} md_net_utils_get_addr_context;

lc_enum_string_map md_image_signature_handling_type_map[] = {
    {misht_none, "(handling type unspecified)"},
    {misht_ignore, "ignore"},
    {misht_check, "check"},
    {misht_require, "require"},
};

static int md_interface_update_if_addr_netlink(md_if_addr_rec_array *addrs,
                                               tbool add_if_missing,
                                               tbool is_link,
                                               tbool is_delete,
                                               uint32 flags_compare_mask,
                                               uint64 update_id,
                                               uint32 nl_idx,
                                               const bn_inetaddr *match_addr,
                                               int match_mask_len,
                                               int ifindex,
                                               const char *iflabel,
                                               uint32 ifa_flags,
                                               uint32 scope,
                                               uint32 preferred_lft,
                                               uint32 valid_lft,
                                               lt_time_ms cstamp_ms,
                                               lt_time_ms tstamp_ms);

static int md_mgmt_rts_netlink_update_record(md_mgmt_route_rec_array *routes,
                                        uint32 idx, uint32 subidx,
                                        const void *dest,
					const void *src,
                                        int mask_len, const void *gw,
                                        int family, int ifindex,
                                        uint32 metric, uint32 weight,
                                        int protocol, int64 lifetime,
                                        unsigned char table, tbool mpath);


/* ------------------------------------------------------------------------- */
/* Fetch the set of detected interface names, and return it in the
 * form of a bn_binding_array, with one binding for each interface,
 * each having the name of the root node of the state tree for that
 * interface.
 *
 * Why do we go through this instead of just calling
 * md_interface_get_ifnames_cached() directly and getting back a list
 * of names?  Just to reduce risk by avoiding rewriting some code
 * which works with a bn_binding_array.
 */
static int
md_get_interface_name_bindings(bn_binding_array **ret_intfs)
{
    int err = 0;
    const tstr_array *intf_names = NULL;
    const char *intf_name = NULL;
    char *bname = NULL;
    bn_binding_array *intfs = NULL;
    bn_binding *intf_binding = NULL;
    int num_intfs = 0, i = 0;

    bail_null(ret_intfs);

    err = bn_binding_array_new(&intfs);
    bail_error(err);

    /*
     * Pass zero here to make sure we don't get cached information.
     * The cache automatically invalidates itself on every commit, but
     * we may not be running in the context of a commit.  If we just
     * iterated under "/net/interface/state", it would use a 5-second
     * timeout on the state, which may leave us out of date!  If an
     * interface just recently showed up, we might incorrectly think
     * it was ineligible to be a device for a route.
     *
     * XXXX/EMT: but if we are running within a commit, we'd rather
     * not keep doing it over and over again!
     */
    err = md_interface_get_ifnames_cached(0, true, &intf_names);
    bail_error(err);

    num_intfs = tstr_array_length_quick(intf_names);
    for (i = 0; i < num_intfs; ++i) {
        intf_name = tstr_array_get_str_quick(intf_names, i);
        if (intf_name == NULL) {
            continue;
        }

        safe_free(bname);
        bname = smprintf("/net/interface/state/%s", intf_name);
        bail_null(bname);

        bn_binding_free(&intf_binding);
        err = bn_binding_new_str(&intf_binding, bname, ba_value, bt_string, 0,
                                 intf_name);
        bail_error(err);

        err = bn_binding_array_append_takeover(intfs, &intf_binding);
        bail_error(err);
    }

 bail:
    safe_free(bname);
    bn_binding_free(&intf_binding); /* Only on error */
    if (ret_intfs) {
        *ret_intfs = intfs;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_netutils_is_interface_listed(const char *intf_name,
                                const md_mgmt_route_rec_array *routes_have,
                                tbool *ret_listed)
{
    int err = 0;
    int i = 0, num_routes = 0;
    const md_mgmt_route_rec *route = NULL;
    tbool listed = false;

    bail_null(intf_name);
    bail_null(routes_have);

    num_routes = md_mgmt_route_rec_array_length_quick(routes_have);
    for (i = 0; i < num_routes; ++i) {
        route = md_mgmt_route_rec_array_get_quick(routes_have, i);
        if (route == NULL) {
            continue;
        }
        if (!safe_strcmp(intf_name, route->mrr_ifname)) {
            listed = true;
            break;
        }
    }

 bail:
    if (ret_listed) {
        *ret_listed = listed;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_intf_network_new(md_intf_network **ret_intf)
{
    int err = 0;
    md_intf_network *intf = NULL;

    bail_null(ret_intf);

    intf = malloc(sizeof(*intf));
    bail_null(intf);
    memset(intf, 0, sizeof(*intf));

 bail:
    if (ret_intf) {
        *ret_intf = intf;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_intf_network_free(md_intf_network **inout_intf)
{
    int err = 0;
    md_intf_network *mri = NULL;

    bail_null(inout_intf);
    mri = *inout_intf;
    if (mri) {
        safe_free(mri->min_ifname);
        safe_free(*inout_intf);
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static void
md_intf_network_free_for_array(void *elem)
{
    int err = 0;
    md_intf_network *mrr = elem;

    err = md_intf_network_free(&mrr);
    bail_error(err);

 bail:
    return;
}


/* ------------------------------------------------------------------------- */
/* Given the binding name root for the state subtree of an interface
 * already confirmed to be up, add entries to 'intf_nets' for all
 * of its prefixes.
 *
 * family: address family: must be AF_INET or AF_INET6.
 */
static int
md_netutils_get_intfnets_one(md_commit *commit, const mdb_db *db,
                             const char *ifname, int family,
                             md_intf_network_array *intf_nets,
                             int *ret_num_found)
{
    int err = 0;
    const char *fam_str = NULL;
    char *addr_parent = NULL;
    bn_binding_array *addrs = NULL;
    int j = 0, num_addrs = 0;
    int num_found = 0;
    md_intf_network *intf_net = NULL;
    bn_inetaddr addr = lia_inetaddr_any;
    uint8 mask_len = 0;
    const bn_binding *addr_binding = NULL;
    char *net_str = NULL;
    char *bname = NULL;
    tbool found = false, ipv6_enabled = false;

    switch (family) {
    case AF_INET:
        fam_str = "ipv4addr";
        break;

    case AF_INET6:
        fam_str = "ipv6addr";
        break;

    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

    /*
     * We check state instead of config here because (a) in general we
     * are state-driven; and (b) we want to consider interfaces that
     * we are not managing, like virbr*, etc., and get accurate answers
     * for those interfaces.
     */
    bname = smprintf("/net/interface/state/%s/addr/settings/ipv6/enable",
                     ifname);
    bail_null(bname);
    err = mdb_get_node_value_tbool(commit, db, bname, 0, &found,
                                   &ipv6_enabled);
    bail_error(err);
    if (!found) {
        lc_log_basic(LOG_INFO, "Missing node %s for expected interface "
                     "(race condition?)", bname);
        ipv6_enabled = false; /* It was probably just deleted */
    }

    /*
     * NOTE: this assumes that you can iterate underneath
     * /net/interface/address/state/ifdevname/<IFNAME>/ipv4addr
     * /net/interface/address/state/ifdevname/<IFNAME>/ipv6addr
     */

    addr_parent = smprintf("/net/interface/address/state/ifdevname/%s/%s",
                           ifname, fam_str);
    bail_null(addr_parent);

    err = mdb_iterate_binding(commit, db, addr_parent, 0, &addrs);
    bail_error(err);

    num_addrs = bn_binding_array_length_quick(addrs);
    for (j = 0; j < num_addrs; ++j) {
        addr_binding = bn_binding_array_get_quick(addrs, j);
        bail_null(addr_binding);

        /*
         * This binding is probably the address we want, but for some
         * reason it's a string, rather than an address type.  There
         * is also a literal "address" binding underneath it which
         * contains an address.  Not sure what else could be in here
         * besides the address in string form, but in any case, we
         * don't trust it.
         */

        if (family == AF_INET) {
            err = mdb_get_child_value_ipv4addr_inetaddr
                (commit, db, "address", addr_binding, 0, &addr);
            bail_error(err);
        }
        else {
            err = mdb_get_child_value_ipv6addr_inetaddr
                (commit, db, "address", addr_binding, 0, &addr);
            bail_error(err);
        }

        err = mdb_get_child_value_uint8(commit, db, "mask_len",
                                        addr_binding, 0, &mask_len);
        if (err) {
            err = 0;
            continue;
        }

        err = md_intf_network_new(&intf_net);
        bail_error(err);

        intf_net->min_ifname = strdup(ifname);
        bail_null(intf_net->min_ifname);

        err = lia_inetaddr_masklen_masked_to_inetprefix
            (&addr, mask_len, &(intf_net->min_prefix));
        bail_error(err);

        intf_net->min_flags = (ipv6_enabled) ? mif_ipv6_enabled : 0;

        safe_free(net_str);
        err = lia_inetprefix_to_str(&(intf_net->min_prefix), &net_str);
        bail_error(err);

        lc_log_basic(LOG_DEBUG, "INTERFACE %s network: %s", ifname, net_str);

        err = md_intf_network_array_append_takeover(intf_nets, &intf_net);
        bail_error(err);

        ++num_found;
    }

 bail:
    if (ret_num_found) {
        *ret_num_found = num_found;
    }
    bn_binding_array_free(&addrs);
    safe_free(addr_parent);
    safe_free(net_str);
    safe_free(bname);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Figure out if the specified interface is up for the specified
 * protocol/family.  If it is, append its name to the provided string
 * array.  'num_found' is the number of addresses we found for that
 * protocol for that interface, so if it's positive, the interface
 * gets added for sure.  It's only if it's zero that we have to check.
 */
static int
md_netutils_update_intf_proto_up(const char *ifname, int family,
                                 int num_found, tstr_array *intfs_up)
{
    int err = 0;
    tbool up = false;
    const char *fam_str = NULL;
    char *sname = NULL, *sval = NULL;

    bail_null(ifname);
    bail_null(intfs_up);

    if (num_found > 0) {
        up = true;
    }
    else {
        switch (family) {
        case AF_INET:
            fam_str = "ipv4";
            break;

        case AF_INET6:
            fam_str = "ipv6";
            break;

        default:
            bail_force(lc_err_unexpected_case);
            break;
        }
        sname = smprintf("/net/%s/conf/%s", fam_str, ifname);
        bail_null(sname);
        err = md_utils_sysctl_get(sname, &sval);
        bail_error(err);
        up = (sval != NULL);
    }

    if (up) {
        err = tstr_array_append_str(intfs_up, ifname);
        bail_error(err);
    }

 bail:
    safe_free(sname);
    safe_free(sval);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
md_mgmt_get_interface_networks(md_commit *commit, const mdb_db *db,
                          tbool include_dev_only_routes,
                          const md_mgmt_route_rec_array *routes_have,
                          md_intf_network_array **ret_intf_nets,
                          tstr_array **ret_intfs_ipv4_up,
                          tstr_array **ret_intfs_ipv6_up);
int
md_mgmt_get_interface_networks(md_commit *commit, const mdb_db *db,
                          tbool include_dev_only_routes,
                          const md_mgmt_route_rec_array *routes_have,
                          md_intf_network_array **ret_intf_nets,
                          tstr_array **ret_intfs_ipv4_up,
                          tstr_array **ret_intfs_ipv6_up)
{
    int err = 0;
    md_intf_network_array *intf_nets = NULL;
    bn_binding_array *intfs = NULL;
    int i = 0, num_intfs = 0;
    int j = 0, num_recs = 0;
    tstring *intf_name = NULL, *gw = NULL;
    tbool oper_up = false, link_up = false;
    const bn_binding *binding = NULL, *addr_binding = NULL;
    tstr_array *static_route_bnames = NULL, *bname_parts = NULL;
    int num_static_routes = 0;
    const char *sr_bname = NULL, *network_str = NULL;
    tbool listed = false, found_intf = false;
    bn_inetprefix inetprefix = lia_inetprefix_any;
    md_intf_network *intf_net = NULL;
    tstr_array *intfs_ipv4_up = NULL;
    tstr_array *intfs_ipv6_up = NULL;
    int num_found = 0;

    bail_null(commit);
    bail_null(db);
    bail_null(ret_intf_nets);

    err = md_intf_network_array_new(&intf_nets);
    bail_error(err);

    err = tstr_array_new(&intfs_ipv4_up, NULL);
    bail_error(err);

    err = tstr_array_new(&intfs_ipv6_up, NULL);
    bail_error(err);

    /* ........................................................................
     * Add the networks of all up interfaces.
     */
    err = md_get_interface_name_bindings(&intfs);
    bail_error(err);

    num_intfs = bn_binding_array_length_quick(intfs);
    for (i = 0; i < num_intfs; ++i) {
        binding = bn_binding_array_get_quick(intfs, i);
        bail_null(binding);

        ts_free(&intf_name);
        err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
                                  &intf_name);
        bail_error(err);

        err = mdb_get_child_value_tbool(commit, db, "flags/oper_up", binding,
                                        0, &oper_up);
        if (err == lc_err_unexpected_null) {
            err = 0;
            lc_log_basic(LOG_DEBUG, "INTERFACE %s: could not get oper_up flag",
                         ts_str(intf_name));
            continue;
        }
        bail_error(err);
        if (!oper_up) {
            lc_log_basic(LOG_DEBUG, "INTERFACE %s is admin down",
                         ts_str(intf_name));
            continue;
        }

        err = mdb_get_child_value_tbool(commit, db, "flags/link_up", binding,
                                        0, &link_up);
        if (err == lc_err_unexpected_null) {
            err = 0;
            lc_log_basic(LOG_DEBUG, "INTERFACE %s: could not get link_up flag",
                         ts_str(intf_name));
            continue;
        }
        bail_error(err);
        if (!link_up) {
            /*
             * Here's where we check the routing table, to see if this
             * interface should be "recalled to life"...
             */
            listed = false;
            if (routes_have) {
                err = md_netutils_is_interface_listed(ts_str(intf_name),
                                                      routes_have, &listed);
                bail_error(err);
            }

            if (listed) {
                lc_log_basic(LOG_DEBUG, "INTERFACE %s is admin up and link "
                             "down -- HOWEVER, it appears as a device in "
                             "the routing table!  So considering it as "
                             "link up", ts_str(intf_name));
            }
            else {
                lc_log_basic(LOG_DEBUG, "INTERFACE %s is admin up, but "
                             "link down", ts_str(intf_name));
                continue;
            }
        }

        /*
         * We now have an up interface, so add all of its prefixes to
         * our array.
         */
        err = md_netutils_get_intfnets_one(commit, db, ts_str(intf_name),
                                           AF_INET, intf_nets, &num_found);
        bail_error(err);
        err = md_netutils_update_intf_proto_up(ts_str(intf_name), AF_INET,
                                               num_found, intfs_ipv4_up);
        bail_error(err);
        err = md_netutils_get_intfnets_one(commit, db, ts_str(intf_name),
                                           AF_INET6, intf_nets, &num_found);
        bail_error(err);
        err = md_netutils_update_intf_proto_up(ts_str(intf_name), AF_INET6,
                                               num_found, intfs_ipv6_up);
        bail_error(err);
    }

    /* ........................................................................
     * If requested, add the prefixes of device-only routes too.
     */

    if (include_dev_only_routes) {
        err = mdb_get_matching_tstr_array
            (commit, db, "/nkn/net/routes/config/*/prefix/*/nh/*", 0,
             &static_route_bnames);
        bail_error(err);
        num_static_routes = tstr_array_length_quick(static_route_bnames);
        for (i = 0; i < num_static_routes; ++i) {
            sr_bname = tstr_array_get_str_quick(static_route_bnames, i);
            bail_null(sr_bname);

            ts_free(&gw);
            err = mdb_get_node_attrib_tstr_fmt
                (commit, db, 0, ba_value, NULL, &gw, "%s/gw", sr_bname);
            bail_error(err);

            /*
             * We're only interested in static routes which do not
             * specify an IP nexthop, and do specify an interface name.
             */
	    // TODO: Why is this check there??
#if 0
            if (!ts_equal_str(gw, "0.0.0.0", false)) {
                continue;
            }
#endif
            ts_free(&intf_name);
            err = mdb_get_node_attrib_tstr_fmt
                (commit, db, 0, ba_value, NULL, &intf_name, "%s/interface",
                 sr_bname);
            bail_error(err);
            if (ts_length(intf_name) == 0) {
                continue;
            }

            /*
             * We're also only interested in such routes where the
             * interface they name is actually up.
             */
            found_intf = false;
            num_recs = md_intf_network_array_length_quick(intf_nets);
            for (j = 0; j < num_recs; ++j) {
                intf_net = md_intf_network_array_get_quick(intf_nets, j);
                bail_null(intf_net);
                if (intf_net->min_flags & mif_from_route) {
                    continue;
                }
                if (ts_equal_str(intf_name, intf_net->min_ifname, false)) {
                    found_intf = true;
                    break;
                }
            }

            intf_net = NULL;
            if (!found_intf) {
                continue;
            }

            /*
             * If we get here, the route meets all of our criteria.
             *
             * Add a mapping between this route's interface name, and
             * the route's prefix, as if that interface were on that
             * network.
             */
            tstr_array_free(&bname_parts);
            err = bn_binding_name_to_parts(sr_bname, &bname_parts, NULL);
            bail_error(err);
            network_str = tstr_array_get_str_quick(bname_parts, 6);
            complain_null_msg(network_str, "Unexpected bname %s", sr_bname);
            if (network_str == NULL) {
                continue;
            }

            err = md_intf_network_new(&intf_net);
            bail_error(err);

            err = ts_dup_str(intf_name, &(intf_net->min_ifname), NULL);
            bail_error(err);

            err = lia_str_to_inetprefix(network_str, &(intf_net->min_prefix));
            bail_error(err);

            intf_net->min_flags = mif_from_route;

            err = md_intf_network_array_append_takeover(intf_nets, &intf_net);
            bail_error(err);
        }
    }

 bail:
    bn_binding_array_free(&intfs);
    ts_free(&intf_name);
    tstr_array_free(&static_route_bnames);
    tstr_array_free(&bname_parts);
    ts_free(&gw);
    if (ret_intf_nets) {
        *ret_intf_nets = intf_nets;
    }
    if (ret_intfs_ipv4_up) {
        *ret_intfs_ipv4_up = intfs_ipv4_up;
    }
    else {
        tstr_array_free(&intfs_ipv4_up);
    }
    if (ret_intfs_ipv6_up) {
        *ret_intfs_ipv6_up = intfs_ipv6_up;
    }
    else {
        tstr_array_free(&intfs_ipv6_up);
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
int
md_mgmt_route_rec_free(md_mgmt_route_rec **inout_rec)
{
    int err = 0;
    md_mgmt_route_rec *mrr = NULL;

    bail_null(inout_rec);
    mrr = *inout_rec;
    if (mrr) {
        safe_free(mrr->mrr_ifname);
        tstr_array_free(&(mrr->mrr_valid_intfs));
        safe_free(*inout_rec);
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static void
md_mgmt_route_rec_free_for_array(void *elem)
{
    int err = 0;
    md_mgmt_route_rec *mrr = elem;

    err = md_mgmt_route_rec_free(&mrr);
    bail_error(err);

 bail:
    return;
}


/* ------------------------------------------------------------------------- */
int
md_mgmt_route_rec_compare(const md_mgmt_route_rec *r1, const md_mgmt_route_rec *r2,
                     tbool devname_special, tbool ignore_devname,
                     tbool ignore_protocol)
{
    int err = 0;
    int retval = 0;
    uint32 idx = 0;
    int cmp = 0;

    /* These are mutually exclusive */
    bail_require(!devname_special || !ignore_devname);

    if (r1 == NULL && r2 == NULL) {
        return(0);
    }
    else if (r1 == NULL) {
        return(-1);
    }
    else if (r2 == NULL) {
        return(1);
    }

    err = lia_inetprefix_cmp(&(r1->mrr_prefix), &(r2->mrr_prefix), &cmp);
    bail_error(err);
    if (cmp != 0) {
        return(cmp);
    }

    err = lia_inetaddr_cmp(&(r1->mrr_gateway), &(r2->mrr_gateway), &cmp);
    bail_error(err);
    if (cmp != 0) {
        return(cmp);
    }

    if (r1->mrr_metric < r2->mrr_metric) {
        return(-1);
    }
    else if (r1->mrr_metric > r2->mrr_metric) {
        return(1);
    }

    if (r1->mrr_weight < r2->mrr_weight) {
        return(-1);
    }
    else if (r1->mrr_weight > r2->mrr_weight) {
        return(1);
    }

    if (!ignore_protocol) {
        if (r1->mrr_protocol < r2->mrr_protocol) {
            return(-1);
        }
        else if (r1->mrr_protocol > r2->mrr_protocol) {
            return(1);
        }
    }

    if (ignore_devname) {
        return(0);
    }

    cmp = safe_strcmp(r1->mrr_ifname, r2->mrr_ifname);

    if (devname_special && cmp && r1->mrr_valid_intfs && r2->mrr_ifname) {
        err = tstr_array_linear_search_str(r1->mrr_valid_intfs, r2->mrr_ifname,
                                           0, &idx);
        if (err == lc_err_not_found) {
            /*
             * The "have" route is not on any of the interfaces marked
             * as valid by the "want" route, so we do not consider
             * them to match.  We know the result of safe_strcmp() will
             * be nonzero due to the test above.
             */
            err = 0;
            retval = cmp;
        }
        else {
            bail_error(err);
            retval = 0;
        }
        return(retval);
    }
    else {
        return(cmp);
    }

 bail:
    return(0);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmt_route_rec_compare_for_array(const void *elem1, const void *elem2)
{
    int err = 0;
    int result = 0;
    const md_mgmt_route_rec *r1 = NULL;
    const md_mgmt_route_rec *r2 = NULL;

    bail_null(elem1);
    bail_null(elem2);
    r1 = *(const md_mgmt_route_rec * const *)elem1;
    r2 = *(const md_mgmt_route_rec * const *)elem2;
    bail_null(r1);
    bail_null(r2);

    result = md_mgmt_route_rec_compare(r1, r2, false, false, false);

 bail:
    return(result);
}


/* ------------------------------------------------------------------------- */
int
md_mgmt_route_rec_array_new(md_mgmt_route_rec_array **ret_arr)
{
    int err = 0;
    array_options opts;
    md_mgmt_route_rec_array *arr = NULL;

    bail_null(ret_arr);

    err = array_options_get_defaults(&opts);
    bail_error(err);

    opts.ao_elem_free_func = md_mgmt_route_rec_free_for_array;
    opts.ao_elem_compare_func = md_mgmt_route_rec_compare_for_array;

    /*
     * We need to allow duplicates to coexist, at least due to the way
     * we currently read routing state.  We read a file out of /proc
     * which does not show everything about a route -- notably, it
     * omits the "protocol".  Since the protocol is part of the key,
     * this means there may be multiple lines which appear identical,
     * because their differences are hidden.  We then resolve these
     * with a followup Netlink query, from which we derive their
     * protocols.
     */
    opts.ao_dup_policy = adp_add_new;

    err = md_mgmt_route_rec_array_new_full(&arr, &opts);
    bail_error(err);

 bail:
    if (ret_arr) {
        *ret_arr = arr;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
int
md_mgmt_route_rec_new(md_mgmt_route_rec **ret_mrr)
{
    int err = 0;
    md_mgmt_route_rec *mrr = NULL;

    bail_null(ret_mrr);

    mrr = malloc(sizeof(*mrr));
    bail_null(mrr);
    memset(mrr, 0, sizeof(*mrr));

    mrr->mrr_ifindex = -1;
    mrr->mrr_family = -1;
    mrr->mrr_table = -1;
    mrr->mrr_protocol = -1;

 bail:
    if (ret_mrr) {
        *ret_mrr = mrr;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmt_route_rec_to_string(const md_mgmt_route_rec *mrr, tstring **ret_str)
{
    int err = 0;
    tstring *str = NULL;
    char *dest_str = NULL, *gw_str = NULL;

    bail_null(mrr);
    bail_null(ret_str);

    err = lia_inetprefix_to_str(&(mrr->mrr_prefix), &dest_str);
    bail_error(err);

    err = lia_inetaddr_to_str(&(mrr->mrr_gateway), &gw_str);
    bail_error(err);

    err = ts_new_sprintf(&str, "%s --> %s", dest_str, gw_str);
    bail_error(err);

    if (mrr->mrr_ifname) {
        err = ts_append_sprintf(str, " (%s)", mrr->mrr_ifname);
        bail_error(err);
    }

 bail:
    safe_free(dest_str);
    safe_free(gw_str);
    if (ret_str) {
        *ret_str = str;
    }
    return(err);
}


#ifdef PROD_TARGET_OS_LINUX

/* ------------------------------------------------------------------------- */
static int
md_mgmt_rts_read_ipv4_route_info(md_mgmt_route_rec_array *routes)
{
    int err = 0;
    md_mgmt_route_rec *mrr = NULL;
    char *fc = NULL; /* file contents of file_route_ipv4_stats */
    char *rp = NULL;
    char ifname[128];
    uint32 dest = 0, gw = 0, mask = 0;
    int32 flags = 0, refcnt = 0, use = 0;
    int32 mtu = 0, window = 0, irtt = 0;
    uint32 metric = 0;
    int nm = 0;
    bn_ipv4addr ipv4addr = lia_ipv4addr_any;
    bn_ipv4addr ipv4mask = lia_ipv4addr_any;

    err = lf_read_file(file_route_ipv4_stats, &fc, NULL);
    bail_error(err);

    rp = strchr(fc, '\n'); /* Skip first line (headers) */
    if (rp == NULL) {
        /*
         * This file seems to come out empty sometimes briefly during
         * boot.  Later on, even when there are no routes, it at least
         * still has the header.
         */
        goto bail;
    }
    ++rp;

    while (true) {
        nm = sscanf(rp, "%127s %X %X %X %d %d %d %X %d %d %d\n", ifname, &dest,
                    &gw, &flags, &refcnt, &use, &metric, &mask, &mtu,
                    &window, &irtt);
        if (nm != 11) {
            break;
        }

        if (flags & RTF_UP) {
            err = md_mgmt_route_rec_new(&mrr);
            bail_error(err);

            mrr->mrr_family = AF_INET;

            mrr->mrr_ifname = strdup(ifname);
            bail_null(mrr->mrr_ifname);

            mrr->mrr_flags = flags;
            mrr->mrr_metric = metric;
            mrr->mrr_type = (flags & RTF_REJECT) ? mrt_reject : mrt_unicast;

            ipv4addr.bi_addr.s_addr = dest;
            ipv4mask.bi_addr.s_addr = mask;
            err = lia_ipv4addr_mask_to_inetprefix(&ipv4addr, &ipv4mask,
                                                  &(mrr->mrr_prefix));
            bail_error(err);

            ipv4addr.bi_addr.s_addr = gw;
            err = lia_ipv4addr_to_inetaddr(&ipv4addr, &(mrr->mrr_gateway));
            bail_error(err);

            if (!strcmp(ifname, "-") || !strcmp(ifname, "*")) {
                mrr->mrr_ifindex = -1;
            }
            else {
                err = md_interface_get_ifindex(ifname, &(mrr->mrr_ifindex));
                bail_error(err);
            }

	    err = lia_ipv4addr_to_inetaddr(&lia_ipv4addr_any, &(mrr->mrr_srcaddr));
	    bail_error(err);

            err = md_mgmt_route_rec_array_append_takeover(routes, &mrr);
            bail_error(err);
        }

        rp = strchr(rp, '\n'); /* Move to next line */
        if (rp == NULL) {
            break;
        }
        ++rp;
    }

 bail:
    safe_free(fc);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Send an rtnetlink request to retrieve the list of address in the system,
 * and return the fd of the socket on which the reply is to be received.
 */
static int
md_netlink_request_addrs(int family, tbool do_links, int *ret_fd)
{
    int err = 0;
    int nlsock = -1;
    struct sockaddr_nl snl;
    unsigned char send_buf[SNDBUF_SIZE];
    struct nlmsghdr *send_hdr = NULL;
    struct ifaddrmsg *send_addrhdr = NULL;
    struct msghdr msg;
    struct iovec iov;
    ssize_t numchars = 0;

    memset(&snl, 0, sizeof(snl));
    memset(send_buf, 0, sizeof(send_buf));
    memset(&msg, 0, sizeof(msg));
    memset(&iov, 0, sizeof(iov));

    bail_null(ret_fd);

    nlsock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    bail_require_errno(nlsock >= 0, "socket()");

    snl.nl_family = AF_NETLINK;
    snl.nl_pid = 0;
    snl.nl_groups = 0;

    err = bind(nlsock, (struct sockaddr *)&snl, sizeof(snl));
    bail_error_errno(err, "bind()");

    send_hdr = (struct nlmsghdr *)send_buf;
    send_hdr->nlmsg_type = !do_links ? RTM_GETADDR : RTM_GETLINK;
    send_hdr->nlmsg_pid = getpid();
    send_hdr->nlmsg_seq = 0;
    send_hdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP | NLM_F_ACK;
    send_hdr->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    if (!do_links) {
        send_addrhdr = (struct ifaddrmsg *)NLMSG_DATA(send_hdr);
        send_addrhdr->ifa_family = family; /* Only addrs from this family */
    }
    bail_require(SNDBUF_SIZE >= send_hdr->nlmsg_len);

    iov.iov_base = send_hdr;
    iov.iov_len = send_hdr->nlmsg_len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    numchars = sendmsg(nlsock, &msg, 0);
    bail_require_errno((numchars >= 0), "sendmsg()");

    lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: sent request");

 bail:
    if (err) {
        safe_close(&nlsock);
    }
    if (ret_fd) {
        *ret_fd = nlsock;
    }
    return(err);
}

/* ------------------------------------------------------------------------- */
/* Send an rtnetlink request to retrieve the list of routes in the system,
 * and return the fd of the socket on which the reply is to be received.
 */
static int
md_mgmt_rts_netlink_request_routes(int family, int *ret_fd)
{
    int err = 0;
    int nlsock = -1;
    struct sockaddr_nl snl;
    unsigned char send_buf[SNDBUF_SIZE];
    struct nlmsghdr *send_hdr = NULL;
    struct rtmsg *send_rthdr = NULL;
    struct msghdr msg;
    struct iovec iov;
    ssize_t numchars = 0;

    memset(&snl, 0, sizeof(snl));
    memset(send_buf, 0, sizeof(send_buf));
    memset(&msg, 0, sizeof(msg));
    memset(&iov, 0, sizeof(iov));

    bail_null(ret_fd);

    nlsock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    bail_require_errno(nlsock >= 0, "socket()");

    snl.nl_family = AF_NETLINK;
    snl.nl_pid = 0;
    snl.nl_groups = 0;

    err = bind(nlsock, (struct sockaddr *)&snl, sizeof(snl));
    bail_error_errno(err, "bind()");

    send_hdr = (struct nlmsghdr *)send_buf;
    send_hdr->nlmsg_type = RTM_GETROUTE;
    send_hdr->nlmsg_pid = getpid();
    send_hdr->nlmsg_seq = 0;
    send_hdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP | NLM_F_ACK;
    send_hdr->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    send_rthdr = (struct rtmsg *)NLMSG_DATA(send_hdr);
    /*
     * Do not set
     * send_rthdr->rtm_table = RT_TABLE_MAIN;
     * since we want to get routes from all tables.
     * (Actually, setting it doesn't seem to do this filtering for us
     * anyway...)
     */
    send_rthdr->rtm_family = family; /* Only get routes from this family */
    bail_require(SNDBUF_SIZE >= send_hdr->nlmsg_len);

    iov.iov_base = send_hdr;
    iov.iov_len = send_hdr->nlmsg_len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    numchars = sendmsg(nlsock, &msg, 0);
    bail_require_errno((numchars >= 0), "sendmsg()");

    lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: sent request");

 bail:
    if (err) {
        safe_close(&nlsock);
    }
    if (ret_fd) {
        *ret_fd = nlsock;
    }
    return(err);
}

/* ------------------------------------------------------------------------- */
/* Send an rtnetlink request to receive async updates about addresses,
 * and return the fd of the socket on which the replies are to be received.
 */
int
md_netlink_request_addr_updates(int family, int *ret_fd)
{
    int err = 0;
    int nlsock = -1;
    struct sockaddr_nl snl;
    unsigned char send_buf[SNDBUF_SIZE];
    struct nlmsghdr *send_hdr = NULL;
    struct ifaddrmsg *send_addrhdr = NULL;
    struct msghdr msg;
    struct iovec iov;
    ssize_t numchars = 0;

    memset(&snl, 0, sizeof(snl));
    memset(send_buf, 0, sizeof(send_buf));
    memset(&msg, 0, sizeof(msg));
    memset(&iov, 0, sizeof(iov));

    bail_null(ret_fd);

    nlsock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    bail_require_errno(nlsock >= 0, "socket()");

    snl.nl_family = AF_NETLINK;
    snl.nl_pid = 0;
    snl.nl_groups = RTMGRP_LINK;  /* For the admin up flag changing */
    if (family == AF_UNSPEC) {
        snl.nl_groups |= RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;
    }
    else if (family == AF_INET) {
        snl.nl_groups |= RTMGRP_IPV4_IFADDR;
    }
    else if (family == AF_INET6) {
        snl.nl_groups |= RTMGRP_IPV6_IFADDR;
    }

    err = bind(nlsock, (struct sockaddr *)&snl, sizeof(snl));
    bail_error_errno(err, "bind()");

    lc_log_basic(MD_NETLINK_LOG_LEVEL,
                 "RTNETLINK: created listening socket for addresses");

 bail:
    if (err) {
        safe_close(&nlsock);
    }
    if (ret_fd) {
        *ret_fd = nlsock;
    }
    return(err);
}

/* ------------------------------------------------------------------------- */
/* Given a socket to which a netlink message has presumably already
 * been sent, await the response, and put it into the buffer provided.
 */
int
md_netlink_read_response(int nlsock, uint8 **ret_rcvbuf, uint32 *ret_msglen)
{
    int err = 0;
    const uint8 *rcvbuf_next = NULL;
    uint8 *rcvbuf = NULL, *rcvbuf_new = NULL;
    uint32 msglen = 0;
    uint32 rcvbuf_size = 0, rcvbuf_avail = 0;
    int rflags = 0;
    int resize_pass = 0;
    tbool guess_size_only = false, size_too_small = false;
    struct sockaddr_nl snl;
    struct iovec iov;
    struct msghdr msg;
    int numbytes = 0;
    struct nlmsghdr *nlhdr = NULL;
    struct nlmsgerr *nlerr_hdr = NULL;
    tbool is_readable = false;
    int chunk = 0;

    memset(&snl, 0, sizeof(snl));
    memset(&iov, 0, sizeof(iov));
    memset(&msg, 0, sizeof(msg));

    rcvbuf_size = MD_NETLINK_RCVBUF_INITIAL_SIZE;
    rcvbuf = malloc(rcvbuf_size);
    bail_null(rcvbuf);
    rcvbuf_next = rcvbuf + msglen;

    while (true) {
        errno = 0;
        is_readable = lf_is_readable_timed(nlsock,
                                           mnu_routing_max_patience_ms);
        bail_require_errno((errno == 0), "poll() error");

        if (!is_readable) {
            lc_log_basic(LOG_WARNING, "RTNETLINK: timed out reading answer "
                         "from routing socket");
            break;
        }

        /* This loop is for realloc() retries only */
        resize_pass = 0;
        do {
            errno = 0;
            memset(&snl, 0, sizeof(snl));
            bail_null(rcvbuf);
            resize_pass++;

            /* Determine the size before trying to read */
            if (resize_pass == 1) {
                guess_size_only = true;
                rflags = MSG_PEEK;
            }
            else {
                guess_size_only = false;
                rflags = 0;
            }

            rcvbuf_next = rcvbuf + msglen;
            rcvbuf_avail = rcvbuf_size - msglen;

            iov.iov_base = (void *)rcvbuf_next;
            iov.iov_len = rcvbuf_avail;

            msg.msg_name = (void *)&snl;
            msg.msg_namelen = sizeof(snl);
            msg.msg_iov = &iov;
            msg.msg_iovlen = 1;
            msg.msg_control = NULL;
            msg.msg_controllen = 0;
            msg.msg_flags = 0;

            numbytes = recvmsg(nlsock, &msg, rflags);

            /* All our other error processing is outside */
            if (numbytes == -1 &&
                (errno != ENOBUFS && errno != ENOMEM)) {
                break;
            }
            if (numbytes == 0 || numbytes < -1) {
                break;
            }

            /* We have a good guess for the new size here */
            size_too_small = false;
            if (errno == 0 && rcvbuf_avail < (uint32) numbytes) {
                rcvbuf_size = rcvbuf_size +
                    numbytes - rcvbuf_avail + 4096;
                size_too_small = true;
            }
            else if (errno != 0 || msg.msg_flags & MSG_TRUNC) {
                /* Just double our size and hope for the best */
                rcvbuf_size = rcvbuf_size * 2 + 4096;
                size_too_small = true;
            }

            if (size_too_small) {
                bail_require(rcvbuf_size <= MD_NETLINK_RCVBUF_MAX_SIZE);
                rcvbuf_new = realloc(rcvbuf, rcvbuf_size);
                bail_null(rcvbuf_new);
                rcvbuf = rcvbuf_new;
                rcvbuf_new = NULL;

                continue;
            }
            if (guess_size_only) {
                continue;
            }

            /* We do not need to resize and try again */
            break;

        } while(resize_pass < 20);

        if (numbytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            else {
                bail_error_errno(numbytes, "recvmsg()");
            }
        }

        if (numbytes == 0) {
            lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: fd closed");
            goto bail;
        }

        lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: chunk[%d] read "
                     "(%d bytes)", chunk++, (int)numbytes);

        if ((msg.msg_flags & MSG_TRUNC) == MSG_TRUNC) {
            lc_log_basic(LOG_WARNING, "RTNETLINK: got MSG_TRUNC");
            err = lc_err_io_error;
            goto bail;
        }

        nlhdr = (struct nlmsghdr *) rcvbuf_next;

        if (nlhdr->nlmsg_type == NLMSG_ERROR) {
            nlerr_hdr = (struct nlmsgerr *) NLMSG_DATA(nlhdr);
            if (nlerr_hdr->error == 0) {
                /*
                 * The netlink man page is unclear on this, but
                 * empirically, it doesn't seem to send these in the
                 * normal case.  The response is generally just a
                 * NLMSG_DONE message.  Maybe the NLMSG_ERROR
                 * acknowledgement is only if there was some problem
                 * and it can't send a normal response.
                 */
                lc_log_basic(LOG_INFO, "RTNETLINK: got ACK (NLMSG_ERROR + "
                             "zero error)");
            }
            else {
                lc_log_basic(LOG_ERR, "RTNETLINK: error from routing socket "
                             "(%d): %s", nlerr_hdr->error,
                             strerror(-(nlerr_hdr->error)));
            }
            break;
        }

        if (nlhdr->nlmsg_type == NLMSG_DONE) {
            lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: got NLMSG_DONE");
            break;
        }

        rcvbuf_next += numbytes;
        msglen += numbytes;

        if (msglen >= rcvbuf_size) {
            lc_log_basic(LOG_WARNING, "RTNETLINK: ran out of buffer space "
                         "reading the netlink response");
            break;
        }

        if (!(nlhdr->nlmsg_flags & NLM_F_MULTI)) {
            lc_log_basic(MD_NETLINK_LOG_LEVEL,
                         "RTNETLINK: not a multipart, done");
            break;
        }
    }

    lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: received response: "
                 "%d bytes", msglen);

 bail:
    if (ret_rcvbuf && !err) {
        *ret_rcvbuf = rcvbuf;
        rcvbuf = NULL;
    }
    safe_free(rcvbuf);
    safe_free(rcvbuf_new);
    if (ret_msglen) {
        *ret_msglen = msglen;
    }
    return(err);
}

/* ------------------------------------------------------------------------- */
/* Given a Netlink address query response in a buffer, containing a set
 * of addreses we are interested in, read the response.  For each entry,
 * extract enough information to identify the entry, plus the preferred
 * and valid lifetimes.  Look up the corresponding md_if_addr_rec record,
 * and update its mar_preferred_lft and mar_valid_lft fields.
 */
int
md_netlink_process_address_response(const uint8 *rcvbuf,
                                    uint32 msglen,
                                    md_if_addr_rec_array *addrs,
                                    int family,
                                    tbool process_deletes,
                                    uint64 update_id)
{
    int err = 0;
    const struct nlmsghdr *nlhdr = NULL;
    uint32 msg_num = 1; /* Start from 1 for consistency with route #'ing */
    int rtcount = 0;
    const struct rtattr *rtap = NULL;
    const struct ifa_cacheinfo *ifaci = NULL;
    const struct ifaddrmsg *ifa = NULL;
    const struct ifinfomsg *ifi = NULL;
    uint32 preferred_lft = 0, valid_lft = 0;
    lt_time_ms cstamp_ms = 0, tstamp_ms = 0;
    int ifindex = -1;
    int mask_len =  0;
    bn_inetaddr ina = lia_inetaddr_any;
    bn_inetaddr ina_local = lia_inetaddr_any;
    bn_inetaddr ina_broad = lia_inetaddr_any;
    tbool add_if_missing = false;
    char ifname[IFNAMSIZ + 1];
    char iflabel[IFNAMSIZ + 1];
    uint32 flags = 0, scope = 0, iff_flags = 0;
    tbool is_delete = false, is_addr = false, is_link = false;

    for (nlhdr = (struct nlmsghdr *)rcvbuf;
        NLMSG_OK(nlhdr, msglen);
        nlhdr = NLMSG_NEXT(nlhdr, msglen)) {

        if (nlhdr->nlmsg_type == NLMSG_ERROR ||
            nlhdr->nlmsg_type == NLMSG_DONE) {
            lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: breaking out due "
                         "to nlmsg_type %s",
                         (nlhdr->nlmsg_type == NLMSG_ERROR) ?
                         "NLMSG_ERROR" : "NLMSG_DONE");
            break;
        }

        if (!(nlhdr->nlmsg_type == RTM_NEWADDR ||
              nlhdr->nlmsg_type == RTM_NEWLINK ||
              (process_deletes && (nlhdr->nlmsg_type == RTM_DELADDR)) ||
              (process_deletes && (nlhdr->nlmsg_type == RTM_DELLINK)))) {
            lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: continuing due "
                         "to nlmsg_type %d", nlhdr->nlmsg_type);
            continue;
        }

        is_addr = (nlhdr->nlmsg_type == RTM_NEWADDR ||
                   nlhdr->nlmsg_type == RTM_DELADDR);
        is_link = (nlhdr->nlmsg_type == RTM_NEWLINK ||
                   nlhdr->nlmsg_type == RTM_DELLINK);
        is_delete = (nlhdr->nlmsg_type == RTM_DELADDR ||
                     nlhdr->nlmsg_type == RTM_DELLINK);

        bail_require(is_addr != is_link);

        ifi = NULL;
        ifa = NULL;

        if (is_link) {
            ifi = (const struct ifinfomsg *) NLMSG_DATA(nlhdr);

            ifindex = ifi->ifi_index;
            iff_flags = ifi->ifi_flags;

            /*
             * We only care about the iff flags (which are in ifi) and
             * the ifname, stored in IFLA_IFNAME .
             */
            ifname[0] = '\0';
            for (rtap = IFLA_RTA(ifi), rtcount = IFLA_PAYLOAD(nlhdr);
                 RTA_OK(rtap, rtcount); rtap = RTA_NEXT(rtap, rtcount)) {

                if (rtap->rta_type == IFLA_IFNAME) {
                    err = safe_strlcpy(ifname,
                                       RTA_DATA(rtap) ? RTA_DATA(rtap) : "");
                    bail_error(err);
                    break;
                }
            }
            /*
             * NOTE: the IFLA_PROTINFO is here (a peer of IFLA_IFNAME),
             * and it contains the IFLA_INET6_FLAGS, which exposes the
             * IF_RA_MANAGED and IF_RA_OTHERCONF flags.  These could in
             * theory be wanted in the future for DHCPv6, to know if we
             * should auto start it.
             */
            ina_local = lia_inetaddr_any;
            err = md_interface_update_if_addr_netlink(addrs,
                                                      true,
                                                      is_link,
                                                      is_delete,
                                                      (IFF_UP),
                                                      update_id,
                                                      msg_num++,
                                                      &ina_local, 0,
                                                      ifindex,
                                                      ifname,
                                                      iff_flags,
                                                      0, 0, 0,
                                                      0, 0);
            bail_error(err);

            continue;
        }
        bail_require(is_addr);

        ifa = (const struct ifaddrmsg *) NLMSG_DATA(nlhdr);

        if (family == AF_UNSPEC ||
            (ifa->ifa_family != AF_INET && ifa->ifa_family != AF_INET6)) {
            if ((ifa->ifa_family != AF_INET && ifa->ifa_family != AF_INET6) ||
                (family != AF_UNSPEC && family != ifa->ifa_family)) {
                lc_log_basic(MD_NETLINK_LOG_LEVEL,
                             "RTNETLINK: skipping message from family %d",
                             ifa->ifa_family);
                continue;
            }
        }

        /*
         * IPv4 contains:
         *
         *    struct ifaddrmsg [fixed: family, prefix len,
         *                             flags, scope, ifindex]
         *    IFA_ADDRESS [4 bytes, opt]
         *    IFA_LOCAL [4 bytes, opt]
         *    IFA_BROADCAST [4 bytes, opt]
         *    IFA_LABEL [string, opt]
         *
         * IPv6 contains:
         *
         *    struct ifaddrmsg [fixed: family, prefix len,
         *                             flags, scope, ifindex]
         *    IFA_ADDRESS [16 bytes, struct in6_addr, req]
         *    struct ifa_cacheinfo [preferred, valid, create stamp, upd stamp]
         */

        ifindex = ifa->ifa_index;
        mask_len = ifa->ifa_prefixlen;
        flags = ifa->ifa_flags;
        scope = ifa->ifa_scope;

        /* Zero everything we'll pull out */
        preferred_lft = md_lifetime_infinity;
        valid_lft = md_lifetime_infinity;
        cstamp_ms = 0;
        tstamp_ms = 0;
        ina = lia_inetaddr_any;
        ina_local = lia_inetaddr_any;
        ina_broad = lia_inetaddr_any;
        iflabel[0] = '\0';

        /* ................................................................
         * Each message is a single address record, with multiple
         * attributes.  Loop over all the attributes, saving the
         * ones we care about.
         */
        for (rtap = IFA_RTA(ifa), rtcount = IFA_PAYLOAD(nlhdr);
             RTA_OK(rtap, rtcount); rtap = RTA_NEXT(rtap, rtcount)) {
            switch (rtap->rta_type) {
            case IFA_ADDRESS:
                if (ifa->ifa_family == AF_INET6) {
                    err = lia_in6addr_to_inetaddr
                        ((const struct in6_addr *)RTA_DATA(rtap), &ina);
                    bail_error(err);
                }
                else if (ifa->ifa_family == AF_INET) {
                    err = lia_inaddr_to_inetaddr
                        ((const struct in_addr *)RTA_DATA(rtap), &ina);
                    bail_error(err);
                }
                break;

            case IFA_LOCAL:
                if (ifa->ifa_family == AF_INET) {
                    err = lia_inaddr_to_inetaddr
                        ((const struct in_addr *)RTA_DATA(rtap), &ina_local);
                    bail_error(err);
                }
                break;

            case IFA_BROADCAST:
                if (ifa->ifa_family == AF_INET) {
                    err = lia_inaddr_to_inetaddr
                        ((const struct in_addr *)RTA_DATA(rtap), &ina_broad);
                    bail_error(err);
                }
                break;

            case IFA_LABEL:
                if (ifa->ifa_family == AF_INET) {
                    err = safe_strlcpy(iflabel,
                                       RTA_DATA(rtap) ? RTA_DATA(rtap) : "");
                    bail_error(err);
                }
                break;

            case IFA_CACHEINFO:
                if (ifa->ifa_family == AF_INET6) {
                    ifaci = (struct ifa_cacheinfo *)RTA_DATA(rtap);

                    preferred_lft = ifaci->ifa_prefered;
                    valid_lft = ifaci->ifa_valid;
                    cstamp_ms = (uint64) ifaci->cstamp * 10;
                    tstamp_ms = (uint64) ifaci->tstamp * 10;
                }
                break;

            default:
                break;
            }
        }

        if (lia_inetaddr_equal_quick(&ina_local, &lia_inetaddr_any)) {
            ina_local = ina;
        }
        if (lia_inetaddr_equal_quick(&ina, &lia_inetaddr_any)) {
            ina = ina_local;
        }

        /* ................................................................
         * We have now read everything interesting about this address.
         * Now update our record to reflect the info we found.  For IPv4
         * this will be an new addition, for IPv6 it will be updating an
         * existing record.
         */
        if ((family == AF_UNSPEC &&
             lia_inetaddr_get_family_quick(&ina_local) != AF_UNSPEC) ||
            lia_inetaddr_get_family_quick(&ina_local) == family) {

            add_if_missing = true;
            err = md_interface_update_if_addr_netlink(addrs,
                                                      add_if_missing,
                                                      is_link,
                                                      is_delete,
                                                      0xfffffffful,
                                                      update_id,
                                                      msg_num++,
                                                      &ina_local,
                                                      mask_len, ifindex,
                                                      iflabel,
                                                      flags,
                                                      scope,
                                                      preferred_lft,
                                                      valid_lft,
                                                      cstamp_ms,
                                                      tstamp_ms);
            bail_error(err);
        }
    }

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_interface_update_if_addr_netlink(md_if_addr_rec_array *addrs,
                                    tbool add_if_missing,
                                    tbool is_link,
                                    tbool is_delete,
                                    uint32 flags_compare_mask,
                                    uint64 update_id,
                                    uint32 nl_idx,
                                    const bn_inetaddr *match_addr,
                                    int match_mask_len,
                                    int ifindex,
                                    const char *iflabel,
                                    uint32 ifa_flags,
                                    uint32 scope,
                                    uint32 preferred_lft,
                                    uint32 valid_lft,
                                    lt_time_ms cstamp_ms,
                                    lt_time_ms tstamp_ms)
{
    int err = 0;
    int i = 0, num_addrs = 0;
    md_if_addr_rec *addr = NULL, prev_addr;
    tbool found_match = false, readded = false;
    tbool unspec = false;
    char addr_buff[INET6_ADDRSTRLEN];
    md_if_addr_rec *mar = NULL;
    char ifname_buf[IFNAMSIZ + 1];

    bail_null(addrs);
    bail_null(match_addr);

    /*
     * Try to find the if_addr_rec which goes with this addr/mask_len .
     * We match only on the address, masklen and ifindex.  The iflabel
     * must also match if it has a ':' in it, as this is how IPv4 alias
     * addresses come to us.
     */
    found_match = false;
    num_addrs = md_if_addr_rec_array_length_quick(addrs);
    for (i = 0; i < num_addrs; ++i) {
        addr = md_if_addr_rec_array_get_quick(addrs, i);
        bail_null(addr);

        if ((is_link == addr->mar_self_is_link) &&
            lia_inetaddr_equal_quick(match_addr,
                                     &(addr->mar_addr)) &&
            match_mask_len == addr->mar_masklen &&
            ifindex == (int) addr->mar_ifindex &&
            (is_link || !iflabel || iflabel[0] == '\0' ||
             strchr(iflabel, ':') == NULL ||
             !safe_strcmp(iflabel, addr->mar_alias_ifname))) {

            found_match = true;
            prev_addr = *addr;

            if (is_link) {
                addr->mar_ifname[0] = '\0';
                err = safe_strlcpy(addr->mar_ifname, iflabel);
                bail_error(err);
                bail_require(strchr(addr->mar_ifname, ':') == NULL);

                addr->mar_flags = ifa_flags;

                if (addr->mar_self_deleted && !is_delete) {
                    addr->mar_self_added = true;
                }
                addr->mar_self_deleted = is_delete;
                if (!is_delete && !addr->mar_self_changed) {
                    /* See if any non-"extended" changes in the record */
                    if (safe_strcmp(prev_addr.mar_ifname, addr->mar_ifname) ||
                        safe_strcmp(prev_addr.mar_alias_ifname,
                                    addr->mar_alias_ifname) ||
                        prev_addr.mar_scope != addr->mar_scope ||
                        (prev_addr.mar_flags & flags_compare_mask) !=
                        (addr->mar_flags & flags_compare_mask)) {
                        addr->mar_self_changed = true;
                    }
                }
                addr->mar_self_is_link = is_link;
                addr->mar_self_update_id = update_id;
                break;
            }

            bail_require(!is_link);

            /* XXXX should have this from netlink in parent! */
            /* Get the ifname from the ifindex */
            memset(ifname_buf, 0, sizeof(ifname_buf));
            errno = 0;
            if (!if_indextoname(ifindex, ifname_buf) || errno) {
                /*
                 * This could happen on an interface going away.  We
                 * keep the same ifname as last time.
                 */
                lc_log_basic(LOG_INFO, "Could not find ifindex %d", ifindex);
            }
            else {
                addr->mar_ifname[0] = '\0';
                err = safe_strlcpy(addr->mar_ifname, ifname_buf);
                bail_error(err);
            }
            bail_require(strchr(addr->mar_ifname, ':') == NULL);

            /* Set IPv4 alias "subinterface" name iff any */
            if (!is_delete) {
                addr->mar_alias_ifname[0] = '\0';
            }
            if (iflabel && *iflabel && strchr(iflabel, ':')) {
                bail_require(lc_is_prefix(addr->mar_ifname, iflabel, false));
                bail_require(iflabel[strlen(addr->mar_ifname)] == ':');

                err = safe_strlcpy(addr->mar_alias_ifname, iflabel);
                bail_error(err);
            }

            addr->mar_scope = scope;
            addr->mar_flags = ifa_flags;

            addr->mar_preferred_lft = preferred_lft;
            addr->mar_valid_lft = valid_lft;
            addr->mar_created = cstamp_ms;
            addr->mar_updated = tstamp_ms;

            if (addr->mar_self_deleted && !is_delete) {
                addr->mar_self_added = true;
            }
            addr->mar_self_deleted = is_delete;
            if (!is_delete && !addr->mar_self_changed) {
                /* See if there were non-"extended" changes in the record */
                if (safe_strcmp(prev_addr.mar_ifname, addr->mar_ifname) ||
                    safe_strcmp(prev_addr.mar_alias_ifname,
                                addr->mar_alias_ifname) ||
                    prev_addr.mar_scope != addr->mar_scope ||
                    (prev_addr.mar_flags & flags_compare_mask) !=
                    (addr->mar_flags & flags_compare_mask)) {
                    addr->mar_self_changed = true;
                }
            }
            addr->mar_self_is_link = is_link;
            addr->mar_self_update_id = update_id;

            break;
        }
    }
    if (!found_match && add_if_missing && !is_delete) {
        err = md_if_addr_rec_new(&mar);
        bail_error(err);

        if (is_link) {
            mar->mar_ifindex = ifindex;
            mar->mar_ifname[0] = '\0';
            err = safe_strlcpy(mar->mar_ifname, iflabel);
            bail_error(err);
            bail_require(strchr(mar->mar_ifname, ':') == NULL);

            mar->mar_addr = *match_addr;
            mar->mar_masklen = match_mask_len;

            mar->mar_flags = ifa_flags;

            mar->mar_self_deleted = false;
            mar->mar_self_added = true;
            mar->mar_self_changed = true;
            mar->mar_self_is_link = is_link;
            mar->mar_self_update_id = update_id;

            err = md_if_addr_rec_array_append_takeover(addrs, &mar);
            bail_error(err);

            goto done;
        }

        bail_require(!is_link);

        /* XXXXX should have this from netlink in parent! */
        /* Get the ifname from the ifindex */
        errno = 0;
        memset(ifname_buf, 0, sizeof(ifname_buf));
        if (!if_indextoname(ifindex, ifname_buf) || errno) {
            lc_log_basic(LOG_INFO, "Could not find ifindex %d", ifindex);
        }
        else {
            err = safe_strlcpy(mar->mar_ifname, ifname_buf);
            bail_error(err);
        }
        bail_require(strchr(mar->mar_ifname, ':') == NULL);

        mar->mar_ifindex = ifindex;

        /* Set IPv4 alias "subinterface" name, with the colon in it iff any */
        if (iflabel && *iflabel && strcmp(iflabel, mar->mar_ifname)) {
            bail_require(lc_is_prefix(mar->mar_ifname, iflabel, false));
            bail_require(iflabel[strlen(mar->mar_ifname)] == ':');

            err = safe_strlcpy(mar->mar_alias_ifname, iflabel);
            bail_error(err);
        }

        mar->mar_addr = *match_addr;
        mar->mar_masklen = match_mask_len;

        mar->mar_scope = scope;
        mar->mar_flags = ifa_flags;

        mar->mar_preferred_lft = preferred_lft;
        mar->mar_valid_lft = valid_lft;
        mar->mar_created = cstamp_ms;
        mar->mar_updated = tstamp_ms;

        mar->mar_self_deleted = false;
        mar->mar_self_added = true;
        mar->mar_self_changed = true;
        mar->mar_self_is_link = is_link;
        mar->mar_self_update_id = update_id;

        err = md_if_addr_rec_array_append_takeover(addrs, &mar);
        bail_error(err);
    }

 done:
    if (match_addr->bin_family == AF_UNSPEC) {
        unspec = true;
    }
    else {
        err = lia_inetaddr_to_str_buf(match_addr, addr_buff,
                                      sizeof(addr_buff));
        complain_error(err);
    }
    lc_log_basic
        (MD_NETLINK_LOG_LEVEL,
         "RTNETLINK: %sif_addr entry %u %smatched: %s/%u, "
         "ifindex %d, label '%s', flags 0x%x, preferred lft %u, valid lft %u",
         is_delete ? "DELETE " : "", nl_idx, (found_match) ? "" : "NOT ",
         unspec ? "AF_UNSPEC" : (err ? "ERROR" : addr_buff),
         match_mask_len,
         ifindex,
         iflabel,
         ifa_flags,
         preferred_lft,
         valid_lft);

 bail:
    md_if_addr_rec_free(&mar);
    if (err) {
        lc_log_basic(LOG_WARNING, "RTNETLINK: entry %u ERROR", nl_idx);
    }
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_netlink_read_if_addrs(md_if_addr_rec_array *if_addrs,
                         uint64 update_id,
                         int family, tbool include_links)
{
    int err = 0;
    int nlsock = -1;
    uint8 *rcvbuf = NULL;
    uint32 msglen = 0;
    const char *fam_descr = NULL;

    if (!md_proto_ipv6_supported()) {
        if (family == AF_INET6) {
            goto bail;
        }
        else if (family == AF_UNSPEC) {
            family = AF_INET;
        }
        bail_require(family == AF_INET);
    }

    bail_null(if_addrs);
    bail_require(family == AF_INET || family == AF_INET6 ||
                 family == AF_UNSPEC);

    fam_descr = (family == AF_INET ? "IPv4" :
                 (family == AF_INET6 ? "IPv6" : "IPv4 and IPv6"));

    lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: Reading %s if_addrs",
                 fam_descr);

    err = md_netlink_request_addrs(family, false, &nlsock);
    bail_error(err);

    safe_free(rcvbuf);
    err = md_netlink_read_response(nlsock, &rcvbuf, &msglen);
    bail_error(err);

    err = md_netlink_process_address_response(rcvbuf, msglen,
                                              if_addrs, family, false,
                                              update_id);
    bail_error(err);

    if (include_links) {
        err = md_netlink_request_addrs(AF_UNSPEC, true, &nlsock);
        bail_error(err);

        safe_free(rcvbuf);
        err = md_netlink_read_response(nlsock, &rcvbuf, &msglen);
        bail_error(err);

        err = md_netlink_process_address_response(rcvbuf, msglen,
                                                  if_addrs, family, false,
                                                  update_id);
        bail_error(err);
    }

 bail:
    lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: Done reading %s if_addrs",
                 fam_descr);
    safe_close(&nlsock);
    safe_free(rcvbuf);
    return(err);
}

/* ------------------------------------------------------------------------- */
int
md_interface_read_if_addrs(tbool include_ipv4, tbool include_ipv6,
                           tbool include_link,
                           uint64 update_id,
                           md_if_addr_rec_array **ret_if_addrs)
{
    int err = 0;
    md_if_addr_rec_array *if_addrs = NULL;

    bail_null(ret_if_addrs);

    if (include_ipv4 && !include_ipv6) {
        err = md_if_addr_rec_array_new(&if_addrs);
        bail_error(err);

        err = md_netlink_read_if_addrs(if_addrs, update_id, AF_INET,
                                       include_link);
        bail_error(err);
    }
    else if (include_ipv6) {
        /* XXXX this proc file parser is more because we had it first */
        err = md_interface_read_ipv6_if_addr_info(NULL, &if_addrs);
        bail_error(err);

        err = md_netlink_read_if_addrs(if_addrs, update_id,
                                       include_ipv4 ? AF_UNSPEC : AF_INET6,
                                       include_link);
        bail_error(err);
    }

    err = md_if_addr_rec_array_sort(if_addrs);
    bail_error(err);

 bail:
    if (ret_if_addrs) {
        *ret_if_addrs = if_addrs;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Given a Netlink route query response in a buffer, containing a set of
 * routes we are interested in, read the response.  For each entry,
 * extract enough information to identify the entry, plus the "protocol"
 * (rtm_protocol) and "lifetime".  Look up the corresponding
 * md_mgmt_route_rec record, and update its mrr_protocol and mrr_lifetime
 * fields.
 */
static int
md_mgmt_rts_netlink_process_response(uint8 *rcvbuf, uint32 msglen,
                                md_mgmt_route_rec_array *routes,
                                int family, uint32 *entry_idx_p)
{
    int err = 0;
    struct nlmsghdr *nlhdr = NULL;
    struct rtmsg *rtmsg = NULL;
    struct rtattr *rtap = NULL;
    int rtcount = 0;
    long hz = sysconf(_SC_CLK_TCK);
    uint32 metric = 0;
    struct rta_cacheinfo *rtci = NULL;
    char dest[ROUTE_BUF_SIZE + 1];
    char src[ROUTE_BUF_SIZE + 1];
    char gw[ROUTE_BUF_SIZE + 1];
    size_t gw_len = 0;
    size_t dest_len = 0;
    size_t src_len = 0;
    int ifindex = -1;
    int mask_len =  0;
    int protocol = -1;
    int64 lifetime = 0;
    unsigned char rtable = 0;
    int rta_table = 0;

    struct rtattr *rtap_nh = NULL;
    struct rtnexthop *nh = NULL;
    size_t nh_len = 0;

    /*
     * Keep track of any extra nexthops we get from RTA_MULTIPATH.
     * ('gw' and 'gw_len' hold whatever we get from top-level
     * RTA_GATEWAY items.)  The bytes we get from RTA_GATEWAY items
     * within an RTA_MULTIPATH item are all packed into enh_buf.  The
     * length of each item goes into enh_lens, so the sum of the
     * numbers in that array should always equal the length of
     * enh_buf.  enh_ifindexes and enh_weights hold the ifindex and
     * metric we get to go with each nexthop.  The rest are just
     * temporary storage.
     */
    tbuf *enh_buf = NULL;
    uint32_array *enh_lens = NULL;
    uint32_array *enh_ifindexes = NULL;
    uint32_array *enh_weights = NULL;
    uint32 enh_len = 0, enh_offset = 0, enh_weight = 0;
    uint32 num_enhs = 0, i = 0;
    const void *enh = NULL;

    bail_null(entry_idx_p);

    for (nlhdr = (struct nlmsghdr *)rcvbuf;
        NLMSG_OK(nlhdr, msglen);
        nlhdr = NLMSG_NEXT(nlhdr, msglen)) {

        /*
         * These only apply to the entry with which they were read.
         */
        tb_free(&enh_buf);
        uint32_array_free(&enh_lens);
        uint32_array_free(&enh_ifindexes);
        uint32_array_free(&enh_weights);

        if (nlhdr->nlmsg_type == NLMSG_ERROR ||
            nlhdr->nlmsg_type == NLMSG_DONE) {
            lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: breaking out due "
                         "to nlmsg_type %s",
                         (nlhdr->nlmsg_type == NLMSG_ERROR) ?
                         "NLMSG_ERROR" : "NLMSG_DONE");
            break;
        }

        if (nlhdr->nlmsg_type != RTM_NEWROUTE) {
            lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: continuing due "
                         "to nlmsg_type %d", nlhdr->nlmsg_type);
            continue;
        }

        rtmsg = (struct rtmsg *)NLMSG_DATA(nlhdr);

        /*
         * If we got back a message for the wrong family, ignore it.
         * This is unlikely since we set rtm_family in our request;
         * this is just a sanity check.
         *
         * We do NOT filter on rtm_table.  We want both the "main"
         * and "local" tables at least; and we also want the other
         * ones, whose entries we will remove from our array.
         */
        if (family == AF_INET && rtmsg->rtm_family == AF_INET) {
            lifetime = -1; /* This doesn't apply to IPv4 */
        }
        else if (family == AF_INET6 && rtmsg->rtm_family == AF_INET6) {
            lifetime = -1; /* We'll read it out of the response below */
        }
        else {
            lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: skipping message "
                         "from family %d", rtmsg->rtm_family);
            continue;
        }

        /*
         * For some reason, we get to fish these straight out of the
         * rtmsg, rather than from attributes within it.
         */
        protocol = rtmsg->rtm_protocol;
        mask_len = rtmsg->rtm_dst_len;
        rtable = rtmsg->rtm_table;

        /* ................................................................
         * Each message is a single route record, with multiple
         * attributes.  Loop over all the attributes, saving the
         * ones we care about.
         */
        ifindex = -1;
        metric = 0;
        memset(dest, 0, sizeof(dest));
	memset(src, 0, sizeof(src));
        memset(gw, 0, sizeof(gw));
        for (rtap = RTM_RTA(rtmsg), rtcount = RTM_PAYLOAD(nlhdr);
             RTA_OK(rtap, rtcount); rtap = RTA_NEXT(rtap, rtcount)) {
            switch (rtap->rta_type) {
            case RTA_OIF:
                ifindex = *(int *)(RTA_DATA(rtap));
                break;

            case RTA_DST:
                dest_len = RTA_PAYLOAD(rtap);
                bail_require(dest_len <= sizeof(dest));
                memset(dest, 0, sizeof(dest));
                memcpy(dest, RTA_DATA(rtap), dest_len);
                break;

	    case RTA_PREFSRC:
		src_len = RTA_PAYLOAD(rtap);
		bail_require(src_len <= sizeof(src));
		memset(src, 0, sizeof(src));
		memcpy(src, RTA_DATA(rtap), src_len);
		break;

            case RTA_GATEWAY:
                gw_len = RTA_PAYLOAD(rtap);
                bail_require(gw_len <= sizeof(gw));
                memset(gw, 0, sizeof(gw));
                memcpy(gw, RTA_DATA(rtap), gw_len);
                break;

            case RTA_PRIORITY:
                /*
                 * Documentation suggests we want RTA_METRICS, which is
                 * called "route metric".  But empirically, we get the
                 * info we want from RTA_PRIORITY, and I have found some
                 * sample code that calls what it gets from this the
                 * "metric".
                 */
                metric = *((unsigned *) RTA_DATA(rtap));
                break;

            case RTA_CACHEINFO:
                if (family == AF_INET6) {
                    rtci = RTA_DATA(rtap);
                    if ((rtmsg->rtm_flags & RTM_F_CLONED) ||
                        (rtci && rtci->rta_expires)) {
                        lifetime = rtci->rta_expires / hz;
                    }
                }
                break;

            case RTA_MULTIPATH:
                /* ............................................................
                 * Handle multipath routes.  This is the one place where
                 * we might end up getting whole new entries that were not
                 * present in the data we got from /proc.
                 *
                 * Some semi-helpful sample code can be found at:
                 *
                 * https://github.com/chesteve/RouteFlow/blob/master/
                 *                    rf-slave/flwtable.cc
                 *
                 * However, it is WRONG in how it iterates over the
                 * nexthops!
                 *
                 * Notes on data structures:
                 *
                 *   - rtap (struct rtattr *): single top-level attrib.
                 *     Length 4 + 16 * number of nexthops.
                 *     4 byte header has length and type (RTA_MULTIPATH).
                 *
                 *   - nh (struct rtnexthop *): single nexthop attrib.
                 *     Length 16: 8 bytes of fields (len, flags, hops/weight,
                 *     ifindex), plus 8 bytes for an attrib to hold gateway.
                 *     Advances by 16 every time we do nh = RTNH_NEXT(nh).
                 *
                 *   - rtap_nh (struct rtattr *): single attrib within
                 *     nexthop struct.  Length 4 + sizeof(gw).  Since this is
                 *     IPv4 only, sizeof(gw) is 4, so total length 8.
                 *     4 byte header has length and type (RTA_GATEWAY).
                 *
                 *   - RTA_DATA(rtap_nh): 4-byte buffer containing the raw
                 *     gateway address.
                 *
                 * Part of the problem with flwtable.cc's approach is that
                 * it uses RTA_NEXT to get through the inner list, not
                 * RTNH_NEXT.  When we switch to the RTNH_ macros
                 * wholesale, that has us using RTNH_OK as the loop
                 * terminator.  That essentially seems to rely on the data
                 * that comes AFTER this entry falling within a specific
                 * range to tell us we have fallen out of the entry.
                 * e.g. if we had three nexthops, rtap's length would be 52
                 * (4 + 16*3).  We'd have 48 in nh_len.  Once we fall off
                 * the end, we essentially cast the 16 bytes after our
                 * attribute ends as a 'struct rtnexthop *', and look at
                 * its length field.  If it is >=8 (sizeof(struct nexthop))
                 * and <=48 (the total number of bytes our three nexthops
                 * took), then we're OK.  So it keeps us in the loop just
                 * fine, since each real entry will have a 16 there.  But
                 * to get out, we have to be outside that range.  It so
                 * happens in my tests that there is a 96 there... but can
                 * we count on that?
                 *
                 * Note that this is the same 96 that causes our RTA_OK()
                 * test in the outer loop to fail as well, and fall out of
                 * the main attrib loop and process this route entry.  That
                 * code has been there for a while, and was modeled on more
                 * trustworthy sources.  So apparently that's part of the
                 * convention: the response contains a delimiter field that
                 * causes these _OK tests to fail, by having too large a
                 * length value.
                 */
                lc_log_basic(MD_NETLINK_LOG_LEVEL,
                             "RTNETLINK: got special rta_type: "
                             "RTA_MULTIPATH");

                if (enh_buf == NULL) {
                    err = tb_new(&enh_buf);
                    bail_error(err);
                }
                if (enh_lens == NULL) {
                    err = uint32_array_new(&enh_lens);
                    bail_error(err);
                }
                if (enh_ifindexes == NULL) {
                    err = uint32_array_new(&enh_ifindexes);
                    bail_error(err);
                }
                if (enh_weights == NULL) {
                    err = uint32_array_new(&enh_weights);
                    bail_error(err);
                }

                /*
                 * XXX/EMT: is there anything for us to do with rtnh_flags?
                 * We'll want flags for our entry, but perhaps the
                 * rtmsg->rtm_flags we took from the top-level attrib
                 * applies to everything.  rtnh_flags might be an entirely
                 * different namespace of flags.
                 */
                for (nh = (struct rtnexthop *)RTA_DATA(rtap),
                         nh_len = RTA_PAYLOAD(rtap);
                     RTNH_OK(nh, nh_len);
                     nh = RTNH_NEXT(nh)) {
                    rtap_nh = RTNH_DATA(nh);
                    if (rtap_nh->rta_type == RTA_GATEWAY) {
                        enh_len = RTA_PAYLOAD(rtap_nh);
                        err = tb_append_buf(enh_buf, RTA_DATA(rtap_nh),
                                            enh_len);
                        bail_error(err);
                        err = uint32_array_append(enh_lens, enh_len);
                        bail_error(err);
                        err = uint32_array_append(enh_ifindexes,
                                                  nh->rtnh_ifindex);
                        bail_error(err);

                        /*
                         * This may seem weird, but it has been validated
                         * empirically, as well as in kernel sources that
                         * say things like "nhp->rtnh_hops = nh->nh_weight-1".
                         * Note that a weight of 1 is the default, and a
                         * weight of 0 is invalid.  Weights range in [1..256].
                         */
                        err = uint32_array_append(enh_weights,
                                                  (uint32)(nh->rtnh_hops) + 1);
                        bail_error(err);
                    }
                    else {
                        lc_log_basic(MD_NETLINK_LOG_LEVEL,
                                     "RTNETLINK: ignoring unrecognized "
                                     "nexthop rta_type: %d",
                                     rtap_nh->rta_type);
                    }
                }
                break;

            case RTA_TABLE:
                /*
                 * Not sure if this is ever supposed to be different
                 * from what we got from rtmsg->rtm_table.  Just look
                 * at it for a sanity check.
                 */
                rta_table = *(int*)RTA_DATA(rtap);
                lc_log_basic((rta_table == rtable) ?
                             LOG_DEBUG : LOG_INFO,
                             "RTNETLINK: got rta_table %d, which %s match "
                             "the number %d we got earlier from "
                             "rtmsg->rtm_table", rta_table,
                             (rta_table == rtable) ? "does" : "does NOT",
                             rtable);
                break;

            default:
                /*
                 * There are other message types that we don't care about.
                 * e.g. most of our entries seem to get RTA_PREFSRC.
                 */
                break;
            }
        }

        /* ................................................................
         * We have now read everything interesting about this route.
         * Now update our record to reflect the protocol we found.
         *
         * The raw fields we now have populated are:
         *   - dest + mask_len (needs conversion)
         *   - gw              (needs conversion)
         *   - family
         *   - ifindex
         *   - metric
         *   - protocol
         *   - lifetime
         *   - rtable
         *
         * Instead of the gw, we may have 'enh_buf', 'enh_lens',
         * 'enh_ifindexes', and 'enh_weights'.  These allow us to pass
         * multiple nexthops (each with ifindex and weight) if we have
         * encountered RTA_MULTIPATH.  If present, these are INSTEAD
         * of an RTA_GATEWAY, which would leave gw empty.  Note that
         * we can't just keep them independent and update with gw iff
         * gw_len is >0, since some single routes have no gateway.
         * So it's either/or.
         */
        num_enhs = uint32_array_length_quick(enh_lens);
        if (num_enhs == 0) {
            err = md_mgmt_rts_netlink_update_record(routes, *entry_idx_p, 0,
                                               dest, src, mask_len, gw, family,
                                               ifindex, metric, 0,
                                               protocol, lifetime, rtable,
                                               false);
            bail_error(err);
        }

        for (i = 0, enh_offset = 0; i < num_enhs; ++i) {
            bail_null(enh_buf);
            enh = tb_buf(enh_buf) + enh_offset;
            ifindex = uint32_array_get_quick(enh_ifindexes, i);
            enh_weight = uint32_array_get_quick(enh_weights, i);
            err = md_mgmt_rts_netlink_update_record(routes, *entry_idx_p, i,
                                               dest, src, mask_len, enh, family,
                                               ifindex, metric, enh_weight,
                                               protocol, lifetime, rtable,
                                               true);
            bail_error(err);
            enh_len = uint32_array_get_quick(enh_lens, i);
            enh_offset += enh_len;
        }

        ++(*entry_idx_p);
    }

 bail:
    tb_free(&enh_buf);
    uint32_array_free(&enh_lens);
    uint32_array_free(&enh_ifindexes);
    uint32_array_free(&enh_weights);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* 'dest' and 'gw' should be (const struct in_addr *) or
 * (const struct in6_addr *), according to 'family'.
 *
 * The 'mpath' parameter declares that this is a nexthop for a
 * multipath route.  This means (a) if we don't find the entry, we add
 * it, since the file in /proc only lists the first nexthop in a
 * multipath route, and (b) we set the 'multipath' flag on the entry.
 */
static int
md_mgmt_rts_netlink_update_record(md_mgmt_route_rec_array *routes, uint32 entry_idx,
                             uint32 subidx, const void *dest, const void *src, int mask_len,
                             const void *gw, int family, int ifindex,
                             uint32 metric, uint32 weight, int protocol,
                             int64 lifetime, unsigned char rtable, tbool mpath)
{
    int err = 0;
    bn_inetprefix dest_inetprefix = lia_inetprefix_any;
    bn_inetaddr gw_inetaddr = lia_inetaddr_any;
    bn_inetaddr src_inetaddr = lia_inetaddr_any;
    int i = 0, num_routes = 0;
    md_mgmt_route_rec *route = NULL;
    md_mgmt_route_rec *new_route = NULL;
    tbool found_match = false, deleted = false;
    char *dest_str = NULL, *gw_str = NULL, *src_str = NULL;
    const char *rtable_str = NULL, *proto_str = NULL;
    const char *fate = NULL;
    char ifname_buf[IF_NAMESIZE + 1];
    char *ifname = NULL;
    char subidx_str[2] = {0, 0};

    if (family == AF_INET) {
        err = lia_inaddr_masklen_to_inetprefix
            ((const struct in_addr *)dest, mask_len,
             &dest_inetprefix);
        bail_error(err);
        err = lia_inaddr_to_inetaddr
            ((const struct in_addr *)gw, &gw_inetaddr);
        bail_error(err);

	err = lia_inaddr_to_inetaddr
	    ((const struct in_addr *)src, &src_inetaddr);
    }
    else {
        err = lia_in6addr_masklen_to_inetprefix
            ((const struct in6_addr *)dest, mask_len,
             &dest_inetprefix);
        bail_error(err);
        err = lia_in6addr_to_inetaddr
            ((const struct in6_addr *)gw, &gw_inetaddr);
        bail_error(err);
    }

    /* ................................................................
     * Now try to find a route record of ours that matches this one.
     * The fields we now have populated are:
     *   - dest_inetprefix
     *   - gw_inetaddr
     *   - family
     *   - ifindex
     *   - metric   (not part of the key)
     *   - protocol (to be added to the matching record)
     *   - lifetime (to be added to the matching record)
     *   - rtable (if RT_TABLE_MAIN or RT_TABLE_LOCAL, to be added to the
     *             matching record; otherwise, the matching record is to
     *             be removed)
     *
     * Note: the metric is not part of the key that uniquely identifies
     * a routing table entry.
     */
    found_match = false;
    num_routes = md_mgmt_route_rec_array_length_quick(routes);
    for (i = 0; i < num_routes; ++i) {
        route = md_mgmt_route_rec_array_get_quick(routes, i);
        bail_null(route);

        /*
         * When looking for a match, only consider entries which don't
         * have mrr_table set yet.  This is our way of not mapping
         * multiple netlink entries to the same record (e.g. if they
         * are identical, but in different tables).
         */
        if (route->mrr_table < 0 &&
            lia_inetprefix_equal_quick(&dest_inetprefix,
                                       &(route->mrr_prefix)) &&
            lia_inetaddr_equal_quick(&gw_inetaddr,
                                     &(route->mrr_gateway)) &&
            ifindex == route->mrr_ifindex &&
            metric == route->mrr_metric) {
            complain_require(family == route->mrr_family);
            found_match = true;

            if (rtable == RT_TABLE_MAIN || rtable == RT_TABLE_LOCAL) {
                route->mrr_protocol = protocol;
                route->mrr_lifetime = lifetime;
                route->mrr_table = (int64)rtable;
                route->mrr_multipath = mpath;
                route->mrr_entry_idx = entry_idx;
		/* Update source address if available */
		route->mrr_srcaddr = src_inetaddr;
                if (mpath) {
                    route->mrr_weight = weight;
                }
            }
            else {
                err = md_mgmt_route_rec_array_delete(routes, i);
                bail_error(err);
                deleted = true;
            }
            break;
        }
    }

    if (deleted) {
        fate = "matched and deleted";
    }
    else if (found_match) {
        fate = "matched";
    }
    else if (mpath) {
        fate = "not matched, but added";
    }
    else {
        fate = "not matched";
    }

    if (!found_match && mpath) {
        err = md_mgmt_route_rec_new(&new_route);
        bail_error(err);

        memset(ifname_buf, 0, sizeof(ifname_buf));
        errno = 0;
        ifname = if_indextoname(ifindex, ifname_buf);
        if (ifname == NULL) {
            complain_null_errno(ifname, "Getting ifname for ifindex %d",
                                ifindex);
            strlcpy(ifname_buf, "UNKNOWN", sizeof(ifname_buf));
        }

        new_route->mrr_ifname = strdup(ifname_buf);
        bail_null(new_route->mrr_ifname);

        /*
         * XXX/EMT: we have no 'flags' here -- how do you get them
         * from netlink?  We passed up rtnh_flags before, because it
         * seemed like those might be a different set of flags.
         * Maybe anything in a multipath route would have a particular
         * set of flags.  For now, just stub out with zero.  Hopefully
         * there is no such thing as a multipath reject route!
         */
        int flags = 0;

        new_route->mrr_ifindex = ifindex;
        new_route->mrr_metric = metric;
        new_route->mrr_weight = weight;
        new_route->mrr_multipath = mpath;
        new_route->mrr_entry_idx = entry_idx;
        new_route->mrr_type = (flags & RTF_REJECT) ? mrt_reject : mrt_unicast;
        new_route->mrr_family = family;
        new_route->mrr_prefix = dest_inetprefix;
        new_route->mrr_gateway = gw_inetaddr;
        new_route->mrr_protocol = protocol;
        new_route->mrr_table = rtable;
        new_route->mrr_flags = flags;
        new_route->mrr_lifetime = lifetime;
	new_route->mrr_srcaddr = src_inetaddr;

        err = md_mgmt_route_rec_array_append_takeover(routes, &new_route);
        bail_error(err);
    }

    /*
     * XXX/EMT: perf: nicer to only render these if they're actually
     * going to get logged, which isn't often.
     */
    err = lia_inetprefix_to_str(&dest_inetprefix, &dest_str);
    bail_error(err);
    err = lia_inetaddr_to_str(&gw_inetaddr, &gw_str);
    bail_error(err);
    err = lia_inetaddr_to_str(&src_inetaddr, &src_str);
    bail_error(err);
    err = md_rts_table_to_string(rtable, &rtable_str);
    complain_error(err);
    err = md_rts_protocol_to_string(protocol, &proto_str);
    complain_error(err);

    if (mpath) {
        /* Not likely >52 of these... */
        subidx_str[0] = (subidx < 26) ? subidx + 'a' : (subidx - 26) + 'A';
    }

    lc_log_basic
        (MD_NETLINK_LOG_LEVEL,
         "RTNETLINK: entry %u%s %s%s: %s --> %s (as %s), "
         "table %u (%s), ifindex %d, metric %u, weight %u, protocol %d (%s)",
         entry_idx, subidx_str, mpath ? "(multipath) " : "", fate, dest_str,
         gw_str, src_str, rtable, rtable_str, ifindex, metric, weight, protocol,
         proto_str);

 bail:
    if (err) {
        lc_log_basic(LOG_WARNING, "RTNETLINK: entry %u%s ERROR", entry_idx,
                     subidx_str);
    }
    safe_free(dest_str);
    safe_free(gw_str);
	safe_free(src_str);
    md_mgmt_route_rec_free(&new_route); /* Only in error case */
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmt_rts_read_route_protocols(md_mgmt_route_rec_array *routes, int family,
                            uint32 *entry_idx_p)
{
    int err = 0;
    int nlsock = -1;
    uint8 *rcvbuf = NULL;
    uint32 msglen = 0;
    const char *fam_descr = NULL;

    if (family == AF_INET6 && !md_proto_ipv6_supported()) {
        goto bail;
    }

    bail_null(routes);
    bail_require(family == AF_INET || family == AF_INET6);

    fam_descr = (family == AF_INET ? "IPv4" : "IPv6");

    lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: Reading %s routes",
                 fam_descr);

    err = md_mgmt_rts_netlink_request_routes(family, &nlsock);
    bail_error(err);

    safe_free(rcvbuf);
    err = md_netlink_read_response(nlsock, &rcvbuf, &msglen);
    bail_error(err);

    err = md_mgmt_rts_netlink_process_response(rcvbuf, msglen,
                                          routes, family, entry_idx_p);
    bail_error(err);

 bail:
    lc_log_basic(MD_NETLINK_LOG_LEVEL, "RTNETLINK: Done reading %s routes",
                 fam_descr);
    safe_close(&nlsock);
    safe_free(rcvbuf);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
md_mgmt_rts_read_route_info(tbool include_ipv4, tbool include_ipv6,
                       md_mgmt_route_rec_array **ret_routes)
{
    int err = 0;
    md_mgmt_route_rec_array *routes = NULL, *routes_ipv6 = NULL;

    /*
     * This is the "entry index" which is used to uniquely identify
     * each routing entry in the full list of routes at a point in time.
     * We declare it here so it can be unique across families.  We start
     * from 1 so that any routes left with an entry_idx of 0 can be
     * recognized as not having been set.
     */
    uint32 entry_idx = 1;

    bail_null(ret_routes);

    err = md_mgmt_route_rec_array_new(&routes);
    bail_error(err);

    if (include_ipv4) {
        err = md_mgmt_rts_read_ipv4_route_info(routes);
        bail_error(err);
        err = md_mgmt_rts_read_route_protocols(routes, AF_INET, &entry_idx);
        bail_error(err);
    }
#if 0
    if (include_ipv6) {
        err = md_mgmt_route_rec_array_new(&routes_ipv6);
        bail_error(err);
        err = md_mgmt_rts_read_ipv6_route_info(routes_ipv6);
        bail_error(err);
        err = md_mgmt_rts_read_route_protocols(routes_ipv6, AF_INET6, &entry_idx);
        bail_error(err);
        err = md_mgmt_route_rec_array_concat(routes, &routes_ipv6);
        bail_error(err);
    }
#endif

    err = md_mgmt_route_rec_array_sort(routes);
    bail_error(err);

 bail:
    md_mgmt_route_rec_array_free(&routes_ipv6); /* Only in error case */
    if (ret_routes) {
        *ret_routes = routes;
    }
    return(err);
}

#else /* PROD_TARGET_OS_LINUX */

int
md_mgmt_rts_read_route_info(md_mgmt_route_rec_array **ret_routes)
{
    int err = 0;

    bail_null(ret_routes);

    lc_log_debug(LOG_NOTICE, "NYI");

    *ret_routes = NULL;

 bail:
    return(err);
}

#endif /* PROD_TARGET_OS_LINUX */

