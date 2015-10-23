/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */

#ifndef __MD_UTILS_INTERNAL_H_
#define __MD_UTILS_INTERNAL_H_

/** \cond */
static const char *rcsid_h__MD_UTILS_INTERNAL_H_ __attribute__((unused)) =
    "$Id: md_utils_internal.h,v 1.40 2011/11/15 20:12:24 et Exp $";
/** \endcond */

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ============================================================================
 * PLEASE NOTE: this file contains utilities for internal use only!
 * These APIs are used by Samara base mgmtd modules, but are NOT
 * intended to be used by customer modules.  The prototypes and
 * semantics/behavior of these APIs should not be assumed to be stable.
 *
 * (For legacy reasons, many of the symbols in here begin with
 * "md_net_utils", but that does not mean they are public.)
 * ============================================================================
 */

#include "tacl.h"
#include "mdb_db.h"
#include "md_mod_reg.h"


int md_utils_internal_init(void);
int md_utils_internal_deinit(void);


/** @name Routes */
/*@{*/

/*
 * There are really only two types of routes: "unicast" and "reject".
 * The other two entries here ("gw" and "interface") are present for
 * backward compatibility with the old IPv4 route nodes (both what the
 * config nodes will accept, and what the state nodes will return).
 * We have four tables mapping between these enums and strings:
 *
 *   - md_route_type_new_map: just the ones we wish we had, for use with
 *     all the new nodes: IPv6 config, IPv6 state, and Inet state.
 *
 *   - md_route_type_v4_config_map: the ones we'll accept for IPv4
 *     config, and the enums they map to.  Note that we'd rather
 *     map both mrt_gw and mrt_interface to mrt_unicast, and we do
 *     so for mrt_gw -- but we keep mrt_interface separate because
 *     it had one special semantic: it causes us to ignore the
 *     'gw' binding.
 *
 *   - md_route_type_v4_state_map: the ones we'll return for IPv4
 *     state, where "gw" means "unicast".
 *
 *   - md_route_type_old_map: not used by Samara code, but kept for
 *     legacy customer code.  Has the same contents as md_route_type_map
 *     had before we supported IPv6.
 */
typedef enum {
    mrt_none = 0,
    mrt_gw,
    mrt_interface,
    mrt_unicast,
    mrt_reject,
} md_route_type;

static const lc_enum_string_map md_route_type_new_map[] = {
    {mrt_none,       "none"},
    {mrt_unicast,    "unicast"},
    {mrt_reject,     "reject"},
    {0, NULL}
};

static const lc_enum_string_map md_route_type_ipv4_config_map[] = {
    {mrt_none,       "none"},
    {mrt_unicast,    "gw"},
    {mrt_interface,  "interface"},
    {mrt_unicast,    "unicast"},
    {mrt_reject,     "reject"},
    {0,              NULL}
};

static const lc_enum_string_map md_route_type_ipv4_state_map[] = {
    {mrt_none,       "none"},
    {mrt_unicast,    "gw"},
    {mrt_interface,  "gw"},
    {mrt_unicast,    "gw"},
    {mrt_reject,     "reject"},
    {0,              NULL}
};

static const lc_enum_string_map md_route_type_old_map[] = {
    {mrt_none,       "none"},
    {mrt_gw,         "gw"},
    {mrt_interface,  "interface"},
    {mrt_reject,     "reject"},
    {0, NULL}
};

static const char md_if_devsource_unknown[]= "UNKNOWN";

/* ------------------------------------------------------------------------- */
/** Contains a record of one route.  These may be static routes from
 * configuration, or dynamic routes.
 *
 * Dynamic routes are read from multiple sources.  To start out, we read
 * from /proc/net/route and /proc/net/ipv6_route.  But these sources both
 * leave out some per-route fields, and leave out some routes.  So we
 * then perform a netlink query to fill in the blanks.
 *
 * Why don't we just use Netlink for the whole thing?  (a) To avoid risk,
 * since the /proc code has been stable for a long time, and we don't want
 * to suffer any unexpected change in behavior; and (b) because /proc also
 * leaves out some routes that we actually wanted left out.  So we are
 * selective about what supplements we accept from Netlink.
 *
 * So when we read from Netlink, we add any missing route entries
 * which come from an IPv4 multipath route (RTA_MULTIPATH).  Also, we
 * fill in fields which were missing from /proc:
 *   - For multipath routes: mrr_multipath, mrr_weight.
 *   - For all routes: mrr_protocol, mrr_lifetime.
 *   - For all routes (which show up at all in Netlink, which hopefully is
 *     everything!): mrr_entry_idx (not read from Netlink either, but our
 *     own synthetic index to reflect which routes came from the same entry
 *     vs. from distinct entries).
 */
typedef struct md_mgmt_route_rec {
    char *mrr_ifname;
    int32 mrr_ifindex;
    md_route_type mrr_type;

    /*
     * How are these different?  Each routing entry has a metric,
     * which is found in the file in /proc.  The weight only pertains
     * to multipath routes, where a single routing entry has multiple
     * nexthops.  Each nexthop in a multipath route has a weight.
     */
    uint32 mrr_metric;
    uint32 mrr_weight;

    /*
     * Most routes have mrr_multipath as 'false', and an mrr_entry_idx
     * which is unique within the address family.  However, IPv4 routes
     * may be multipath, meaning multiple nexthops within a single
     * table entry.  These have mrr_multipath as 'true', and all nexthops
     * within the same entry share the same mrr_entry_idx (though they
     * appear in different md_mgmt_route_recs).
     */
    tbool mrr_multipath;
    uint32 mrr_entry_idx;

    /**
     * Only populated for records of routes we want to have (derived
     * from configuration), which do not have an interface name
     * specified; i.e. iff mrr_ifname is NULL.  This will contain the
     * list of the interfaces which would be valid choices for the
     * device of this route, calculated by inspection of interface
     * state.
     */
    tstr_array *mrr_valid_intfs;

    /*
     * The old fields (mrr_dest, mrr_mask, and mrr_gw) have been removed.
     * They are supplanted by mrr_prefix and mrr_gateway.
     */

    /*
     * Address family: AF_INET or AF_INET6
     */
    int mrr_family;

    /*
     * These are the replacements for mrr_dest, mrr_mask, and mrr_gw.
     * They should both have the same family (AF_INET or AF_INET6)
     * as in mrr_family.
     */
    bn_inetprefix mrr_prefix;
    bn_inetaddr mrr_gateway;

    /*
     * "Protocol" of the route, or -1 if unknown.  "Protocol" is a
     * misleading term -- this is really an indicator of the origin of
     * the route, i.e. how did it get into the routing table.
     *
     * If set for a record representing route state, this is the
     * rtm_protocol field from the 'struct rtmsg' we got back from the
     * kernel when asking about the route: it is one of the
     * RTPROT... defines from /usr/include/linux/rtnetlink.h (or one
     * of our special ones, of which MD_ROUTE_ORIGIN_DHCP is currently
     * the only one).  We have an enum map in md_route_protocol_map.
     *
     * If set for a record representing route config, this is the
     * protocol the route should have when it is installed, generally
     * based on whether the route came from static config or from an
     * overlay.  We need to have this as part of the config record so
     * it can be used as a comparison criterion.
     */
    int mrr_protocol;

    /* ........................................................................
     * The fields in this section are for internal use by the routing
     * module, to make notes on configuration records.  They will
     * never be set in an md_mgmt_route_rec returned from
     * md_rts_read_route_info().
     */

    /*
     * When set by the routing module, this means this is a record
     * which is determined to contain an "invalid" route (one which
     * would not be successful if we tried to apply it, e.g. because
     * of current interface state).
     */
    tbool mrr_invalid;

    /* ........................................................................
     * Fields past this point are only populated for dynamic routes,
     * for exposure in the monitoring nodes.  They are not part of
     * routing configuration.
     */

    /*
     * Which table the route exists in.  These are members of rt_class_t,
     * from /usr/include/linux/rtnetlink.h.  When populating the table
     * with routes we find, we leave out any which are confirmed to be
     * in some table other than RT_TABLE_MAIN or RT_TABLE_LOCAL.
     * So everything will have one of those values, or -1 meaning we
     * didn't find the route in the results of our netlink query.
     */
    int64 mrr_table;

    /*
     * Flags from the 4th field of /proc/net/routes, or the 9th field
     * of /proc/net/ipv6_routes.
     */
    uint32 mrr_flags;

    /*
     * Lifetime of the route, for IPv6 only.
     *
     * A value of -1 means no lifetime is set.  This will be the case for
     * all IPv4 routes, and for whichever IPv6 routes do not report a
     * lifetime.  A value >= 0 means we determined a lifetime.
     */
    int64 mrr_lifetime;


    /*
     * Source address, if any
     */
    bn_inetaddr mrr_srcaddr;

} md_mgmt_route_rec;

int md_mgmt_route_rec_new(md_mgmt_route_rec **ret_mrr);

int md_mgmt_route_rec_free(md_mgmt_route_rec **inout_rec);

TYPED_ARRAY_HEADER_NEW_NONE(md_mgmt_route_rec, struct md_mgmt_route_rec *);

int md_mgmt_route_rec_array_new(md_mgmt_route_rec_array **ret_arr);

/**
 * Compare two route records.
 *
 * If 'devname_special' and 'ignore_devname' are both false, the match
 * must be exact.  These two flags are mutually exclusive.
 *
 * If 'devname_special' is true, and only the device names do not
 * match, we will still consider it a match if r1 has no device name,
 * but r2's device name is on r1's list of eligible device names.
 *
 * If 'ignore_devname' is true, then the device name will not be
 * considered for purposes of matching.
 */
int md_mgmt_route_rec_compare(const md_mgmt_route_rec *r1, const md_mgmt_route_rec *r2,
                         tbool devname_special, tbool ignore_devname,
                         tbool ignore_protocol);

/* ------------------------------------------------------------------------- */
/** Read dynamic route information.  Create an array of (md_mgmt_route_rec *)
 * that reflects the routes found.
 *
 * \param include_ipv4 Should we include IPv4 routes from
 * /proc/net/routes?
 *
 * \param include_ipv6 Should we include IPv6 routes from
 * /proc/net/ipv6_routes?
 *
 * \retval ret_routes The array of routes found.
 */
int md_mgmt_rts_read_route_info(tbool include_ipv4, tbool include_ipv6,
                           md_mgmt_route_rec_array **ret_routes);

/*@}*/

/** @name Interfaces */
/*@{*/

/**
 * Return the list of statically configured IPv4 addresses on each of
 * the interfaces given.  The list may include 0.0.0.0 .  The first
 * duplicate IP return will never be 0.0.0.0, but may be NULL on on
 * duplicates.
 *
 * \param commit
 * \param db the config database holding the bindings
 * \param intf_names list of interfaces
 * \param ret_ip_addrs List of IPv4 addresses.  May contain 0.0.0.0 .
 * \param ret_first_dup Return first duplicate IP (if any).  Ignores 0.0.0.0 .
 */
int md_get_ipv4_static_addrs(md_commit *commit, const mdb_db *db,
                             const tstr_array *intf_names,
                             tstr_array **ret_ip_addrs,
                             tstring **ret_first_dup);

/*
 * Return the list of all "active" IPv4 addresses on each of the
 * interfaces given.  This is derived from the interface state nodes.
 * Since there is no concept of status for IPv4 addresses, there is
 * no filtering done here as there is with md_get_ipv6_active_addrs().
 */
int md_get_ipv4_active_addrs(md_commit *commit, const mdb_db *db,
                             const tstr_array *intf_names,
                             tstr_array **ret_ipv4_addrs);

/**
 * Return the list of statically configured IPv6 addresses on each of
 * the interfaces given.  The returned list may include :: The first
 * duplicate IP return will never be ::, but may be NULL on on
 * duplicates.
 *
 * \param commit
 * \param db the config database holding the bindings
 * \param intf_names list of interfaces
 * \param ret_ip_addrs List of IPv6 addresses.  May contain :: .
 * \param ret_first_dup Return first duplicate IPv6 (if any).  Ignores :: .
 */
int md_get_ipv6_static_addrs(md_commit *commit, const mdb_db *db,
                             const tstr_array *intf_names,
                             tstr_array **ret_ip_addrs,
                             tstring **ret_first_dup);

/*
 * Return the list of all "active" IPv6 addresses on each of the
 * interfaces given.  This is derived from the interface state nodes,
 * filtering the IPv6 addresses listed based on their "status".  For
 * an address to be considered active, its status must be "preferred"
 * or "optimistic"; any other status disqualifies the address.
 *
 * Link local addresses are only included if 'include_link_local' is
 * true.  In this case, a zone/scope ID (taken from the ifindex of the
 * interface in question) is appended, e.g.
 * "fe80::21e:c9ff:fe29:8867%1" for the link-local address of the
 * interface with an ifindex of 1.
 */
int md_get_ipv6_active_addrs(md_commit *commit, const mdb_db *db,
                             const tstr_array *intf_names,
                             tbool include_link_local,
                             tstr_array **ret_ipv6_addrs);

/**
 * Flags that can describe an interface we get back from
 * md_get_interface_networks().
 */
typedef enum {

    /**
     * This record does not reflect a real up interface, but rather
     * reflects a static route found in configuration, with no IP
     * nexthop, and this interface name as its device.
     */
    mif_from_route = 1 << 0,

    /**
     * Is this interface enabled for IPv6 in configuration?
     * This is only relevant if mif_from_route is not set, and
     * the two flags are mutually exclusive.
     */
    mif_ipv6_enabled = 1 << 1,

} md_interface_flags;

/** Bit field of ::md_interface_flags ORed together */
typedef uint32 md_interface_flags_bf;

/**
 * Record of a single interface to be returned from
 * md_get_interface_networks().
 */
typedef struct md_intf_network {
    char *min_ifname;
    bn_inetprefix min_prefix;
    md_interface_flags_bf min_flags;
} md_intf_network;

TYPED_ARRAY_HEADER_NEW_NONE(md_intf_network, struct md_intf_network *);

int md_intf_network_array_new(md_intf_network_array **ret_arr);

/**
 * Scan current interface state, and return the list of the networks
 * covered by currently up interfaces.
 *
 * \param commit Commit state.
 *
 * \param db Configuration database through which to query state.
 *
 * \param include_dev_only_routes In addition to the runtime state of
 * the interfaces, also consider static routes that specify only a device,
 * and no nexthop.  Treat such routes the same as the interface specified
 * as its device.  e.g. if there was a route 192.168.0.0 /24 --> ether1,
 * then add an entry representing that ether1 was on that network.
 *
 * \param routes_have If this parameter is specified, then in addition
 * to the runtime state of the interfaces, also consider routes
 * currently in the routing table, This would only come into play if
 * there was an interface which was (a) admin up, (b) link down, and
 * (c) appeared as a device on a route in the routing table.  Such an
 * interface would be treated as link up (except we'd log a message to
 * distinguish the cases); see bug 12453.
 *
 * \retval ret_intfs Array of prefixes found on all up interfaces.
 */
int md_get_interface_networks(md_commit *commit, const mdb_db *db,
                              tbool include_dev_only_routes,
                              const md_mgmt_route_rec_array *routes_have,
                              md_intf_network_array **ret_intfs,
                              tstr_array **ret_intfs_ipv4_up,
                              tstr_array **ret_intfs_ipv6_up);

/*
 * Fetch information about "listen" interfaces.
 *
 * listen_enable_bname: name of binding node which tells if listen is
 *     enabled for the feature in question.
 *
 * listen_intfs_bname_root: name of parent node of wildcard whose values
 *     are the names of interfaces for which listen is enabled for the
 *     feature in question.
 *
 * include_link_local: tells us if we should include IPv6 link-local
 *     addresses in the address list.  If not, they are simply omitted.
 *     If so, they are included, and each one is fully qualified with a
 *     scope ID, i.e. a '%' character followed by the ifindex of the
 *     interface it goes with.
 *
 * ret_listen_active: is listen feature ACTIVE for this interface?
 *     This means (a) the listen feature is configured enabled,  and
 *     (b) there is at least one configured listen interface which is
 *     eligible to be a listen interface.  Note that even if this is
 *     'false', 'relevant_intf_names' might still get some interface
 *     names; but it does mean that 'active_intf_names' will be empty.
 *
 * ret_relevant_intf_names: list of interface names for which listen is
 *     and/or was enabled for this feature.  It doesn't matter if it
 *     no longer is, or if it's not eligible; we still want to consider
 *     changes to those interfaces.  This is to be used for filtering
 *     the changelist for relevancy.
 *
 * ret_active_intf_names: list of interface names for which listen is
 *     currently enabled (in the new_db), and eligible.
 *
 * ret_listen_intf_addrs: list of active addresses on the interfaces in
 *     ret_active_intf_names, i.e. those on which we should listen.
 *     By "active", we exclude e.g. IPv6 addresses still marked "tentative".
 */
int md_net_utils_get_listen_config(md_commit *commit,
                                   const mdb_db *old_db,
                                   const mdb_db *new_db,
                                   const char *listen_enable_bname,
                                   const char *listen_intfs_bname_root,
                                   tbool include_link_local,
                                   tbool *ret_listen_active,
                                   tstr_array **ret_relevant_intf_names,
                                   tstr_array **ret_active_intf_names,
                                   tstr_array **ret_listen_intf_addrs);

/**
 * Given the name parts of a binding node, and a list of interface names,
 * return 'true' if the binding node is the DHCP or zeroconf binding for
 * one of the named interfaces.
 *
 * This is meant to be used by commit apply functions for features
 * offering "listen" controls.  Call this function with the name parts
 * of a changed binding, and a list of interfaces relevant to the
 * listen feature.  We return 'true' if you need to reconsider your
 * configuration to see if it has changed.
 *
 * This leaves out most other config node changes which might be
 * relevant to listen, because it is assumed that the caller is
 * listening for interface state change events, and reacts to those as
 * well.  So our job is to return 'true' for those cases which would
 * NOT be detected by interface state change events.  That is only
 * DHCP and zeroconf, since either of those being enabled disqualifies
 * an interface for "listen".
 */
tbool md_net_utils_relevant_for_listen_quick(const tstr_array *name_parts,
                                             const tstr_array *intf_names);

/**
 * Check if specified interface name is eligible to be used as a
 * listen interface, as far as its configuration goes.
 *
 * The requirements are:
 *
 *    (Interface exists and is enabled) AND
 *    (Interface has neither DHCP nor zeroconf enabled) AND
 *    ((Interface has at least one static nonzero IPv4 address) OR
 *     (Interface has IPv6 enabled AND IPv6 is enabled overall))
 *
 * \param commit
 * \param db
 * \param ifname Name of interface to check.
 * \retval ret_ok Returns if interface is eligible to be a listen interface.
 */
int md_interface_ok_for_listen(md_commit *commit, const mdb_db *db,
                               const char *ifname, tbool *ret_ok);

/**
 * Delete the specified node (that ends with interface name), if the
 * interface is not a configured one.
 * \param commit
 * \param inout_db the config database holding the binding.
 * \param node_name name of the node to be deleted.
 * \param ret_removed return flag to specify if the node was removed or not (optional parameter).
 */
int md_remove_node_iff_bad_interface(md_commit *commit, mdb_db *inout_db,
                        const char *node_name, tbool *ret_removed);

/**
 * Return an array of lines from the output of an interface scan.
 * This is currently used to find installed interfaces and addresses.
 * It is not used to gather statistics.
 *
 * \param ret_ifstats_lines
 */
int md_do_interface_scan(tstr_array **ret_ifstats_lines);

/* Default cache time */
enum {
    md_if_scan_cache_ms = 5000,       /* ifconfig -a */
};

/**
 * Return an array of lines from the output of an interface scan.
 * The results may be returned from a cache.
 *
 * \param max_age
 * \param ret_ifstats_lines
 */
int md_do_interface_scan_cached(lt_time_ms max_age,
                                const tstr_array **ret_ifstats_lines);

/**
 * Return a list of detected interface names.  According to the
 * 'include_aliases' parameter, the list may or may not include
 * aliases.  The base list comes from the contents of /sys/class/net
 * in the filesystem, and includes all "actual" interfaces, including
 * bonds, bridges, and vlans, but NOT aliases.  The list with aliases
 * is the union of the first list with the results of calling
 * SIOCGIFCONF.
 *
 * NOTE: this is a const array being returned.  The caller must dup
 * the returned array if they're going to keep a reference to it, as
 * subsequent calls to this function may destroy earlier return values.
 */
int
md_interface_get_ifnames_cached(lt_time_ms max_age, tbool include_aliases,
                                const tstr_array **ret_ifnames);

/* Default cache time */
enum {
    md_interface_ifnames_cache_ms = 5000      /* /sys/class/net */
};

/**
 * Check to see if the interface is currently present in the underlying
 * system's interface table.
 */
int md_interface_exists(const char *ifname, tbool *ret_exists);

/**
 * Check to see if there is any interface that is DHCP enabled.
 */
int md_interface_dhcp_exists(md_commit *commit, const mdb_db *db,
        tbool *ret_exists);

/**
 * Return an array of lines from the output of a bridge scan
 * \param ret_bridge_lines
 */
int md_do_bridge_scan(tstr_array **ret_bridge_lines);


/**
 * Given an IP address and a masklen, make sure that the address does
 * not match any of the types specified by ::invalid_mask, and that the
 * prefix has non-zero host and network parts.  A zero address and
 * masklen are allowed if invalid_mask does not contain the relevant
 * lia_addrtype_unspecified flag.  The masklen is allowed to be /32 or
 * /128 (no host portion).
 *
 * When used with things like ::lia_addrtype_match_endpoint , this can
 * be used to validate a prefix for use on an interface.
 */

int md_net_validate_ipv4addr_masklen(md_commit *commit,
                                     const char *bad_msg_end,
                                     bn_ipv4addr ip, uint8 masklen,
                                     lia_addrtype_flags_bf invalid_mask,
                                     tbool *ret_is_valid);

int md_net_validate_ipv6addr_masklen(md_commit *commit,
                                     const char *bad_msg_end,
                                     bn_ipv6addr ip, uint8 masklen,
                                     lia_addrtype_flags_bf invalid_mask,
                                     tbool *ret_is_valid);

int md_net_validate_inetaddr_masklen(md_commit *commit,
                                     const char *bad_msg_end,
                                     bn_inetaddr ip, uint8 masklen,
                                     lia_addrtype_flags_bf invalid_mask,
                                     tbool *ret_is_valid);

typedef enum {
    mast_universe = 0x00,
    mast_host =     0x10,
    mast_link =     0x20,
    mast_site =     0x40,
} md_addr_scope_type;

/* ------------------------------------------------------------------------- */
/** Contains a record of one interface address for IPv4 or IPv6 (read
 *  from /proc/net/if_inet6 and/or netlink).
 *
 */

#define md_lifetime_infinity (0xFFFFFFFFUL)

typedef struct md_if_addr_rec {
    char mar_ifname[33];
    uint32 mar_ifindex;

    /*
     * The interface name with a ':' in it, if this address is an IPv4
     * alias, otherwise the empty string.
     */
    char mar_alias_ifname[33];

    bn_inetaddr mar_addr;
    uint8 mar_masklen;

    uint32 mar_scope; /* md_addr_scope_type */

    /*
     * For addresses, these are IFA flags like IFA_F_PERMANENT from
     * <linux/if_addr.h> .  If this is a link, these are IFF flags like
     * IFF_UP from <linux/if.h> .
     */
    uint32 mar_flags;

    /* "Extended" fields only available via netlink, and which change */
    uint32 mar_preferred_lft;
    uint32 mar_valid_lft;
    lt_dur_ms mar_created;
    lt_dur_ms mar_updated;

    /* Internal use state info */
    tbool mar_self_deleted; /* Set if now gone */
    tbool mar_self_added;   /* Set if added this pass, even if later deleted */
    tbool mar_self_changed; /* NOTE: does not cover the "extended" fields, or
                             * (for links) flags besides IFF_UP */
    tbool mar_self_is_link; /* Otherwise is an addr, the default */
    uint64 mar_self_update_id;
} md_if_addr_rec;

int md_if_addr_rec_free(md_if_addr_rec **inout_rec);

TYPED_ARRAY_HEADER_NEW_NONE(md_if_addr_rec, struct md_if_addr_rec *);

int md_if_addr_rec_array_new(md_if_addr_rec_array **ret_arr);

int md_if_addr_rec_new(md_if_addr_rec **ret_mar);

int md_if_addr_rec_compare(const md_if_addr_rec *r1, const md_if_addr_rec *r2);

int md_if_addr_rec_compare_changed(const md_if_addr_rec *r1,
                                   const md_if_addr_rec *r2);

int md_if_addr_rec_compare_changed_for_array(const void *elem1,
                                             const void *elem2);

/* ------------------------------------------------------------------------- */
/**
 * Read dynamic interface address information for IPv4 and/or IPv6.
 * Creates an array of (md_if_addr_rec *) that reflects the addresses
 * found.
 *
 * \param include_ipv4 Should we include IPv4 addresses
 *
 * \param include_ipv6 Should we include IPv6 addresses
 *
 * \param include_link Should we include link information
 *
 * \param update_id Add set in any updated or added records
 *
 * \retval ret_addrs The array of interface addresses found.
 */
int md_interface_read_if_addrs(tbool include_ipv4, tbool include_ipv6,
                               tbool include_link,
                               uint64 update_id,
                               md_if_addr_rec_array **ret_if_addrs);

/* Max size for a netlink receive buferr */
#define MD_NETLINK_RCVBUF_MAX_SIZE       16777216

/* Typical size for a netlink receive buferr */
#define MD_NETLINK_RCVBUF_INITIAL_SIZE   16384

/**
 * Given a netlink socket, read a netlink response off of it into a
 * provided buffer.
 *
 * \param nlsock A netlink socket, created by something like
 * md_netlink_request_addr_updates() .
 *
 * \param ret_rcvbuf A buffer which the call must free().
 *
 * \param ret_msglen The number of bytes used on return of the
 * caller-provided buffer.
 */
int md_netlink_read_response(int nlsock,
                             uint8 **ret_rcvbuf,
                             uint32 *ret_msglen);

/**
 * Request that the kernel send async notifications of link and address
 * changes.  The raw bytes can be read using md_netlink_read_response(),
 * and converted to md_if_addr_rec's by
 * md_netlink_process_address_response().
 *
 * \param family AF_INET, AF_INET6 or AF_UNSPEC (both).
 *
 * \retval ret_fd The fd to read the events from.
 */
int md_netlink_request_addr_updates(int family, int *ret_fd);

/**
 * Given a netlink response, presumably read with
 * md_netlink_read_response() , walk the netlink link and address
 * messages that make up the response, and convert these to new or
 * existing records in the 'addrs' array.
 *
 * \param rcvbuf Buffer containing the raw netlink message.
 *
 * \param msglen Number of bytes used in rcvbuf.
 *
 * \param addrs An in/out array of md_if_addr_rec's which is updated
 * based on the netlink message.
 *
 * \param family If we should process AF_INET, AF_INET6 or both
 * (AF_UNSPEC) addresses.
 *
 * \param process_deletes If set, netlink deletes will cause existing
 * matching records to have their 'mar_self_deleted' flag set.
 * Otherwise, netlink address deletes are ignored.
 */
int md_netlink_process_address_response(const uint8 *rcvbuf,
                                        uint32 msglen,
                                        md_if_addr_rec_array *addrs,
                                        int family,
                                        tbool process_deletes,
                                        uint64 update_id);


/**
 * Read dynamic interface address information for IPv6.  Creates an
 * array of (md_if_addr_rec *) that reflects the addresses found.  Only
 * a subset of the fields are filled in compared with
 * md_interface_read_if_addrs() .  In particular, the extended fields
 * containing the lifetime and timestamp information will be 0.
 *
 * \param match_ifname Interface name to restrict address to.
 *
 * \retval ret_addrs The array of IPv6 interface addresses found.
 */
int md_interface_read_ipv6_if_addr_info(const char *match_ifname,
                                        md_if_addr_rec_array **ret_addrs);

/**
 * Search the given list for the specified IPv4 or IPv6 address on all
 * interfaces, possibly restricting the search to a single interface.
 * Only a single record (or none) is returned.  The interface name may
 * be NULL, but the address must always be specified.
 */
int md_interface_find_if_addr(const md_if_addr_rec_array *if_addrs,
                              const char *match_ifname,
                              const bn_inetaddr *match_addr,
                              tbool *ret_found,
                              md_if_addr_rec *ret_found_addr);

/*
 * DEPRECATED: Use md_interface_find_if_addr() .
 *
 * Search for the specified IPv4 or IPv6 address on all interfaces,
 * possibly restricting the search to a single interface.
 *
 * For IPv4 addresses, the alias number in use / to use is returned, and
 * the returned ifname will be that of the alias subinterface (ether1:8).
 *
 * For IPv6 addresses, the alias number is NULL, and the returned ifname
 * will be the name of a real interface (ifname if it was specified).
 */
int md_interface_find_addr(const char *ifname,
                           const bn_inetaddr *faddr,
                           tbool *ret_found,
                           int32 *ret_ipv4_alias_num,
                           tstring **ret_ifname,
                           uint8 *ret_masklen);

/**
 * Return a string describing the status of an address with the given
 * ifa flags.  These flags are returned in md_if_addr_rec's from
 * md_interface_find_if_addr() and friends.
 */
int md_interface_get_addr_status(uint32 ifa_flags,
                                 tstring **ret_addr_status);

/**
 * Return a string describing what the source of the interface is.  This
 * is one of physical, bridge, bond, alias, vlan, loopback, virtual.
 */
int md_interface_get_devsource(const char *ifname,
                               tstring **ret_devsource);

/**
 * Look up the index of an interface by the specified name.
 * This is done using an ioctl().
 */
int md_interface_get_ifindex(const char *ifname, int32 *ret_if_index);

/*@}*/

/* ------------------------------------------------------------------------- */
/* A fairly specialized ACL-related check, centralized here because
 * we happen to need the same logic in a number of places.  Determine
 * if an operation should be denied, and set the contents of
 * ret_response if so.  If the response was already denied, we never
 * change that.
 *
 * Specifically, we deny the operation unless:
 *
 *   (the commit structure belongs to 'auth_group' OR
 *    (the commit structure has a superset of the auth groups held by
 *     the named user in the 'old_db' AND in the 'new_db'))
 *
 * XXX/EMT: maybe this should take a set of auth groups as varargs,
 * not just one?  Currently we're commonly called with
 * taag_auth_set_9, but shouldn't these places also pass taag_admin?
 * Or see comments in rbac-status.txt...
 */
int md_utils_acl_check_user_commit(md_commit *commit, const mdb_db *old_db,
                                   const mdb_db *new_db, const char *username,
                                   ta_auth_group auth_group,
                                   mdm_node_acl_response *inout_response);


/* ------------------------------------------------------------------------- */
/* Option flags to md_utils_acl_check_path().
 */
typedef enum {

    muacpf_none = 0,

    /*
     * The 'permitted_path' parameter names a directory, and anything
     * under the subtree rooted at this directory should be permitted.
     */
    muacpf_subtree = 1 << 0,

} md_utils_acl_check_path_flags;

/** Bit field of ::md_utils_acl_check_path_flags ORed together */
typedef uint32 md_utils_acl_check_path_flags_bf;


/* ------------------------------------------------------------------------- */
/* A helper function to be called from a custom ACL callback of an
 * internal action, to further refine its accessibility in certain cases.
 *
 * The intended use of this is for generic file manipulation actions,
 * which must have very restricted (admin-only) ACLs because they let
 * you specify a full path, and therefore can be used on any file.
 * Some of these generic actions are used for specific feature areas
 * which want to allow certain non-admin auth groups access to that
 * feature area.  Generally the files relevant to the feature area are
 * in a particular directory or subtree.
 *
 * Therefore this API lets you specify a directory, and an ACL which
 * should apply to the action IFF the parameters to the action specify
 * a file in that directory.  (We also take flags, which can be used
 * to specify whether or not this is a subtree.)
 *
 * \param commit Commit structure.
 *
 * \param db Config db structure.
 *
 * \param action_path The full path upon which the action proposes to
 * operate.  By default this is expected to be just a directory name,
 * though this could be changed by specifying different flags.
 * In the default case, this can be taken from the 'local_dir' return
 * from mdc_path_synth_from_bindings().
 *
 * \param permitted_path The path name we try to match action_path
 * against.  If there is a match, the ACL provided is applied to this
 * action.  By default this is expected to be just a directory name,
 * and the match behavior is to just check if it is equal to
 * action_path (i.e. the proposed target of the action is exactly this
 * directory).  This behavior could be changed by specifying different
 * flags.
 *
 * \param flags Option flags, e.g. to modify match behavior.
 *
 * \param acl An ACL to be applied to this action if the action_path
 * matches the permitted_path in the manner described above.
 *
 * \retval inout_resp If the ACL given is determined to apply, and it
 * grants access, then this will be changed to grant access.  Otherwise,
 * it will not be changed.  i.e. calls to this function can only grant
 * further access, not revoke access.  Therefore, the ACL registered
 * on the action should be the most restrictive, and the calls to this
 * function should be for the cases where more permissiveness is
 * required.
 */
int md_utils_acl_check_path(md_commit *commit, const mdb_db *db,
                            const char *action_path,
                            const char *permitted_path,
                            md_utils_acl_check_path_flags_bf flags,
                            const ta_acl *acl,
                            mdm_node_acl_response *inout_resp);


/* ------------------------------------------------------------------------- */
/* Helper function for implementation of specialized file name actions.
 * The caller is expected to be an internal action handler for an action
 * which renames a file living within a specific directory.  We do not
 * support having a directory substructure, or moving the file to a
 * different directory.
 *
 * \param commit Commit structure.
 *
 * \param db Database structure.
 *
 * \param params Binding parameters to this action.  We look for three
 * bindings, listed below.  We expect to find all three; the first two are
 * mandatory, and while the third is optional for the user, it should have
 * a default declared in the action registration.
 *   - old_filename (string): full filename (including extension) of
 *     source file to be renamed
 *   - new_filename (string): full destination filename (including
 *     extension) to which the file should be renamed.
 *   - allow_overwrite (bool): if the destination filename already exists,
 *     should we proceed with overwriting it?  If not, and it does exist,
 *     an error code and message are returned, but no errors are logged,
 *     as this is generally a user "error".
 *
 * \param dir Directory in which the files are to be found.
 *
 * \param preserve_extension Should we require that the new filename
 * have the same extension as the old filename?
 */
int md_fileutil_rename(md_commit *commit, const mdb_db *db,
                       const bn_binding_array *params, const char *dir,
                       tbool preserve_extension);

/* ------------------------------------------------------------------------- */
/* Image related internal data types and function prototypes */

typedef enum {
    misht_none = 0,
    misht_ignore = 1,
    misht_check = 2,
    misht_require = 3,
} md_image_signature_handling_type;

extern lc_enum_string_map md_image_signature_handling_type_map[];

int md_utils_image_signature_get_min_verify(md_commit *commit,
                                            const mdb_db *db,
                                            md_image_signature_handling_type
                                            *ret_min_sig_verify);

#ifdef __cplusplus
}
#endif

#endif /* __MD_UTILS_INTERNAL_H_ */
