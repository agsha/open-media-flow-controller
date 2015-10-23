/*
 * File:    md_mgmtd_routes.c
 * Author: Dhruva Narasimhan (Derived from TM/Samara)
 * Date: 6/17/2012
 */

#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "mdmod_common.h"
#include "proc_utils.h"
#include "md_utils.h"
#include "md_upgrade.h"
#include "tpaths.h"
#include "mdm_events.h"
#include "bswap.h"
#include "libevent_wrapper.h"
#include "md_main.h"		/* XXX/EMT: for md_lwc */

#ifdef PROD_TARGET_OS_LINUX
#undef __STRICT_ANSI__		/* rtnetlink.h fails on i386 without this */
#include <linux/rtnetlink.h>
#endif

#define MD_ROUTES_LOG_LEVEL LOG_INFO

/* XXX: JUNIPER ADDED
 * This is the worst thing to do. However, we dont have this file in a
 * library yet, so no choice but to do this.
 */
#include "md_utils_internal.c"

/* ------------------------------------------------------------------------- */
/* The IPv4 nodes are:
 *
 *   0    1    2     3      4     5         6        7      8       9
 * /nkn/net/routes/config/ipv4/prefix/<IPV4PREFIX>/enable
 *					          /metric
 *                                                /nh     /<UINT8>/type
 *                                                /nh     /<UINT8>/gw
 *                                                /nh     /<UINT8>/interface
 *                                                /nh     /<UINT8>/source
 *
 * (Linux allows the metric to be set on a per-next-hop basis in IPv4 also.
 * But we are stuck with these nodes lest we break any existing code.)
 */

/* XXX: JUNIPER ADDED
 * We retained the original indexing scheme, since we want to overlay
 * the routes. The original node is /net/routes/config...., and so the
 * prefix part is at index 5. But when we get the binding, we check
 * if the binding name is preceded with a /nkn. If so, then we use the
 * prefix index for that node as prefix_idx + 1.
 *
 * So for all purposes this is retained for 5
 */
static const uint32 md_mgmtd_routes_prefix_idx = 5;

static const char md_mgmtd_routes_default_metric[] = "1";
static uint32 md_mgmtd_routes_default_metric_int = 1;

typedef enum {
    mro_none = 0,
    mro_add,
    mro_delete,
} md_route_oper;

static const lc_enum_string_map md_route_oper_map[] = {
    /*
     * We need "append" for IPv4 instead of "add", or else it won't
     * consider the nexthop part of the key, and we'll get
     * "RTNETLINK answers: File exists" when we try to add a route
     * which only differs in nexthop.  This does not appear necessary
     * for IPv6 ("add" seems to work too), but we'll stick with
     * "append" everywhere for consistency.
     */
    {mro_add, "append"},
    {mro_delete, "del"},
    {0, NULL}
};


/*
 * How long will we wait after getting an interface state change event
 * (including link_up or link_down) before reasserting routes?  If we
 * get another, we'll reset this timer, to see if more arrive.
 */
static const lt_dur_ms md_mgmtd_routes_reassert_event_short_ms = 800;
static lew_event *md_mgmtd_routes_reassert_event_short = NULL;

/*
 * What's the maximum time after getting an interface state change
 * event (including link up or link down) that we'll wait before
 * reasserting routes?  This is just a failsafe to make sure the
 * reassert does happen, even if we get a steady stream of events
 * which keep making us reset the other timer.
 */
static const lt_dur_ms md_mgmtd_routes_reassert_event_long_ms = 2000;
static lew_event *md_mgmtd_routes_reassert_event_long = NULL;

/*
 * A list of interface names for which we have received interface
 * state change events, but not yet reasserted routes.  When we do
 * reassert, we'll enumerate these interface names in the "reason"
 * for the log message, then clear the list.
 */
tstr_array *md_mgmtd_routes_ifnames_changed = NULL;


/* ------------------------------------------------------------------------- */

static md_upgrade_rule_array *md_mgmtd_routes_upgrade_rules = NULL;

/*
 * Structure to track the routing overlays applied from each
 * particular interface.  Note that we only support overlays for the
 * default route, which is why a route can be expressed as a string:
 * it is just a string representation of the nexthop.
 *
 * See md_resolver_if_overlay for more comments, on how the
 * per-interface tracking works.
 */
typedef struct md_mgmtd_routes_if_overlay {
    tstring *mrio_ifname;
    tstr_array *mrio_routes;
} md_mgmtd_routes_if_overlay;

const char md_mgmtd_routes_acting_overlay_ifname[] =
    "/net/general/state/acting_overlay_ifname";

TYPED_ARRAY_HEADER_NEW_NONE(md_mgmtd_routes_if_overlay,
			    struct md_mgmtd_routes_if_overlay *);
TYPED_ARRAY_IMPL_NEW_NONE(md_mgmtd_routes_if_overlay,
			  md_mgmtd_routes_if_overlay *, NULL);

md_mgmtd_routes_if_overlay_array *md_mgmtd_routes_if_overlays = NULL;

static int md_mgmtd_routes_get_extras(md_commit * commit,
				      const mdb_db * db,
				      const char *ifname,
				      tstr_array ** ret_extra_routes);

int md_mgmtd_routes_init(const lc_dso_info * info, void *data);

int md_mgmtd_routes_deinit(const lc_dso_info * info, void *data);

static int md_mgmtd_routes_commit_check(md_commit * commit,
					const mdb_db * old_db,
					const mdb_db * new_db,
					mdb_db_change_array * change_list,
					void *arg);

static int
md_mgmtd_routes_handle_acting_overlay_ifname_change(md_commit * commit,
						    const mdb_db * db,
						    const char *event_name,
						    const bn_binding_array
						    * bindings,
						    void *cb_reg_arg,
						    void *cb_arg);

static int md_mgmtd_routes_commit_apply(md_commit * commit,
					const mdb_db * old_db,
					const mdb_db * new_db,
					mdb_db_change_array * change_list,
					void *arg);

static int md_mgmtd_routes_check_relevance(const mdb_db_change_array * arr,
					   uint32 idx,
					   mdb_db_change * change,
					   void *data);

static int md_mgmtd_routes_change_route(int family,
					md_route_oper oper,
					const char *prefix,
					const bn_inetaddr * gw,
					const char *gw_str,
					const bn_inetaddr * src,
					const char *src_str,
					const char *metric,
					const char *dev, int protocol,
					tstring ** ret_errors);

static int md_mgmtd_routes_overlay_add(md_commit * commit,
				       const mdb_db * db,
				       const char *action_name,
				       bn_binding_array * params,
				       void *cb_arg);

static int md_mgmtd_routes_overlay_clear_action(md_commit * commit,
						const mdb_db * db,
						const char *action_name,
						bn_binding_array * params,
						void *cb_arg);

static int md_mgmtd_routes_recheck(md_commit * commit, const mdb_db * db,
				   const char *action_name,
				   bn_binding_array * params,
				   void *cb_arg);

static int md_mgmtd_routes_dump_rec(int log_level, const char *prefix,
				    const md_mgmt_route_rec * rec);

static int
md_mgmtd_routes_reassert_state(md_commit * commit, const mdb_db * db,
			       const char *reason);

static int
md_mgmtd_routes_overlay_clear(md_commit * commit, const mdb_db * db,
			      const char *ifname);

/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_if_overlay_free(md_mgmtd_routes_if_overlay ** inout_mrio)
{
    int err = 0;
    md_mgmtd_routes_if_overlay *mrio = NULL;

    bail_null(inout_mrio);
    mrio = *inout_mrio;
    if (mrio) {
	ts_free(&(mrio->mrio_ifname));
	tstr_array_free(&(mrio->mrio_routes));
	safe_free(*inout_mrio);
    }

  bail:
    return (err);
}


/* ------------------------------------------------------------------------- */
static void md_mgmtd_routes_if_overlay_free_for_array(void *elem)
{
    int err = 0;
    md_mgmtd_routes_if_overlay *mrio = elem;

    err = md_mgmtd_routes_if_overlay_free(&mrio);
    bail_error(err);

  bail:
    return;
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_if_overlay_array_new(md_mgmtd_routes_if_overlay_array **
				     ret_arr)
{
    int err = 0;
    array_options ao;

    bail_null(ret_arr);

    err = array_options_get_defaults(&ao);
    bail_error(err);

    ao.ao_elem_free_func = md_mgmtd_routes_if_overlay_free_for_array;

    err = md_mgmtd_routes_if_overlay_array_new_full(ret_arr, &ao);
    bail_error(err);

  bail:
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_get_extras(md_commit * commit, const mdb_db * db,
			   const char *ifname,
			   tstr_array ** ret_extra_routes)
{
    int err = 0;
    md_mgmtd_routes_if_overlay *mrio = NULL, *mrio_ref = NULL;
    int i = 0, num_ifs = 0;
    tstr_array_options opts;
    tbool found = false;
    tstring *overlay_ifname = NULL;

    if (ifname == NULL) {
	err = mdb_get_node_value_tstr(commit, db,
				      md_mgmtd_routes_acting_overlay_ifname,
				      0, NULL, &overlay_ifname);
	bail_error(err);

	/*
	 * overlay_ifname could be NULL if there is no acting overlay
	 * primary (because no interfaces are eligible).  We still
	 * have to return a pair of tstr_arrays, so we make up a dummy
	 * record with a NULL ifname, which will never have anything
	 * else in it.
	 */
	ifname = ts_str_maybe(overlay_ifname);
    }

    num_ifs = md_mgmtd_routes_if_overlay_array_length_quick
	(md_mgmtd_routes_if_overlays);
    for (i = 0; i < num_ifs; ++i) {
	mrio = md_mgmtd_routes_if_overlay_array_get_quick
	    (md_mgmtd_routes_if_overlays, i);
	bail_null(mrio);
	if ((mrio->mrio_ifname == NULL && ifname == NULL) ||
	    (mrio->mrio_ifname != NULL && ifname != NULL &&
	     ts_equal_str(mrio->mrio_ifname, ifname, false))) {
	    found = true;
	    break;
	}
    }

    if (!found) {
	mrio = malloc(sizeof(*mrio));
	bail_null(mrio);
	memset(mrio, 0, sizeof(*mrio));

	if (ifname) {
	    err = ts_new_str(&(mrio->mrio_ifname), ifname);
	    bail_error(err);
	}

	err = tstr_array_options_get_defaults(&opts);
	bail_error(err);
	opts.tao_dup_policy = adp_delete_new;

	err = tstr_array_new(&(mrio->mrio_routes), &opts);
	bail_error(err);

	mrio_ref = mrio;
	err = md_mgmtd_routes_if_overlay_array_append_takeover
	    (md_mgmtd_routes_if_overlays, &mrio_ref);
	bail_error(err);
    }

    /*
     * At this point, mrio points to the appropriate record, whether
     * fished out of the array, or newly created and now added to the
     * array.
     */

  bail:
    ts_free(&overlay_ifname);
    if (mrio && ret_extra_routes) {
	*ret_extra_routes = mrio->mrio_routes;
    }
    mrio = NULL;		/* We didn't own this; it's in md_mgmtd_routes_if_overlays */
    return (err);
}


/* ------------------------------------------------------------------------- */
/* Figure out which interfaces a route could be applied to, by
 * scanning the list of provided interfaces, and determining which of
 * their networks the nexthop is in.  Of this set, only keep the one
 * (or more, if there's a tie) with the most specific netmask.
 *
 * If the record specifies an interface name, return 'true' or 'false'
 * to reflect if the name is on the list of eligible interfaces.
 * If the record does not specify an interface name, store the list
 * of eligible interfaces in the record, and always return 'true'
 * for keep_rec.
 */
static int
md_route_check_route_intfs(md_mgmt_route_rec * rec,
			   const md_intf_network_array * intf_nets,
			   const tstr_array * intfs_ipv4_up,
			   const tstr_array * intfs_ipv6_up,
			   tbool * ret_keep_rec)
{
    int err = 0;
    tbool keep_rec = true;
    int i = 0, num_intfs = 0;
    const md_intf_network *intf_net = NULL;
    bn_inetaddr prefix_network = lia_inetaddr_any;
    bn_inetaddr gw_network = lia_inetaddr_any;
    uint8 prefix_masklen = 0, best_prefix_masklen = 0;
    tbool valid = false;
    tstr_array *best_intfs = NULL;
    uint32 idx = 0, flags = 0;
    char buff[INET6_ADDRSTRLEN] = { 0 };
    uint16 family = 0;
    const tstr_array *eligible_intfs = NULL;

    bail_null(rec);
    bail_null(intf_nets);
    bail_null(ret_keep_rec);

    err = tstr_array_new(&best_intfs, NULL);
    bail_error(err);

    /* ........................................................................
     * If the route specifies a device but not an IP nexthop, then
     * only that one device matches it.  Don't apply our checks below,
     * since obviously zero will not be in any of the desired
     * networks!
     */
    if (lia_inetaddr_is_zero_quick(&(rec->mrr_gateway))) {
	if (rec->mrr_ifname == NULL) {
	    /*
	     * XXX/EMT: would be better to return a message to the user,
	     * and log something at NOTICE, just the first time such a
	     * route is added, but not every subsequent time we reassert.
	     * Should also say what the prefix/netmask is.
	     */
	    err = md_mgmtd_routes_dump_rec
		(LOG_INFO, "Routing: ignoring route with neither a next "
		 "hop nor a device", rec);
	    complain_error(err);
	    keep_rec = false;
	} else {
	    /*
	     * Don't add it to best_intfs; see handling of
	     * rec->mrr_ifname below.  We'd just conclude that
	     * keep_rec should be true, but we don't store a list of
	     * matching interfaces in this case.
	     *
	     * But we only want to keep the record if the interface is
	     * actually up for the relevant protocol/family.
	     */
	    if (rec->mrr_family == AF_INET) {
		eligible_intfs = intfs_ipv4_up;
	    } else {
		eligible_intfs = intfs_ipv6_up;
	    }
	    err = tstr_array_linear_search_str(eligible_intfs,
					       rec->mrr_ifname, 0, &idx);
	    if (err == lc_err_not_found) {
		err = 0;
		keep_rec = false;
	    } else {
		bail_error(err);
		keep_rec = true;
	    }

	    if (!keep_rec) {
		err = md_mgmtd_routes_dump_rec
		    (MD_ROUTES_LOG_LEVEL,
		     "Routing: ignoring route with no IP "
		     "nexthop bound for an interface which is down for this "
		     "protocol/family", rec);
		complain_error(err);
	    }
	}
	goto bail;
    }

    /* ........................................................................
     * Now check the networks of up interfaces.  Only look at the ones of
     * the same family as our md_mgmt_route_rec.
     */
    num_intfs = md_intf_network_array_length_quick(intf_nets);
    for (i = 0; i < num_intfs; ++i) {
	intf_net = md_intf_network_array_get_quick(intf_nets, i);
	bail_null(intf_net);

	err = lia_inetprefix_get_family(&(intf_net->min_prefix), &family);
	bail_error(err);
	if (family != rec->mrr_family) {
	    continue;
	}

	if ((intf_net->min_flags & mif_ipv6_enabled) == 0 &&
	    rec->mrr_family == AF_INET6) {
	    /*
	     * For an IPv6 route, do not consider any interface
	     * which is disabled for IPv6.
	     */
	    continue;
	}

	err = lia_inetprefix_get_inetaddr(&(intf_net->min_prefix),
					  &prefix_network);
	bail_error(err);

	err = lia_inetprefix_get_masklen(&(intf_net->min_prefix),
					 &prefix_masklen);
	bail_error(err);

	err =
	    lia_inetaddr_bitop_masklen(&(rec->mrr_gateway), prefix_masklen,
				       lbo_a_and_b, &gw_network);
	bail_error_msg(err, "gateway %s, masklen %u",
		       lia_inetaddr_to_str_buf_quick(&(rec->mrr_gateway),
						     buff, sizeof(buff)),
		       prefix_masklen);

	err = lia_inetaddr_equal(&gw_network, &prefix_network, &valid);
	bail_error(err);

	/*
	 * We only want to keep it if it's as specific, or more
	 * specific than, that most specific one seen so far.
	 */
	if (valid && prefix_masklen >= best_prefix_masklen) {
	    if (prefix_masklen > best_prefix_masklen) {
		err = tstr_array_empty(best_intfs);
		bail_error(err);
	    }
	    err = tstr_array_append_str(best_intfs, intf_net->min_ifname);
	    bail_error(err);
	    best_prefix_masklen = prefix_masklen;
	}
    }

#if 1
    tstring *ts = NULL;
    err = ts_join_tokens(best_intfs, ',', 0, 0, 0, &ts);
    bail_error(err);
    err = md_mgmtd_routes_dump_rec(LOG_INFO, "ROUTE INTF", rec);
    bail_error(err);
    lc_log_basic(LOG_INFO, "ROUTE INTF: --> interfaces '%s'", ts_str(ts));
#endif

    if (rec->mrr_ifname) {
	err = tstr_array_linear_search_str(best_intfs, rec->mrr_ifname, 0,
					   &idx);
	if (err == lc_err_not_found) {
	    err = 0;
	    keep_rec = false;
	}
	bail_error(err);
    } else {
	tstr_array_free(&(rec->mrr_valid_intfs));
	rec->mrr_valid_intfs = best_intfs;
	best_intfs = NULL;

	/*
	 * If this route is not valid for any interface, we shouldn't
	 * even keep it in the list.  It won't match any existing
	 * routes, so the existing routes will be removed.  If it
	 * matched anything, we'd want to add it and let 'route'
	 * figure out which one to assign it to, presumably one of the
	 * ones we like.  And that is what we do.  But if it doesn't
	 * match anything, we don't want to try to apply it.
	 */
	if (tstr_array_length_quick(rec->mrr_valid_intfs) == 0) {
	    keep_rec = false;
	}
    }

  bail:
    tstr_array_free(&best_intfs);
    if (ret_keep_rec) {
	*ret_keep_rec = keep_rec;
    }
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_maybe_discard_rec(md_commit * commit, const mdb_db * db,
				  md_mgmt_route_rec_array * routes,
				  md_mgmt_route_rec ** inout_rec)
{
    int err = 0;
    const char *filter_bname = NULL;
    tbool filter_routes = true, found = false;

    bail_null(routes);
    bail_null(inout_rec);
    bail_null(*inout_rec);

    filter_bname = ((*inout_rec)->mrr_family == AF_INET) ?
	"/net/general/config/filter_routes" :
	"/net/general/config/filter_ipv6_routes";

    err = mdb_get_node_value_tbool
	(commit, db, filter_bname, 0, &found, &filter_routes);
    bail_error(err);
    complain_require(found);

    if (filter_routes) {
	err = md_mgmtd_routes_dump_rec
	    (MD_ROUTES_LOG_LEVEL, "Routing: discarding invalid route",
	     *inout_rec);
	complain_error(err);
	md_mgmt_route_rec_free(inout_rec);
    } else {
	err = md_mgmtd_routes_dump_rec
	    (MD_ROUTES_LOG_LEVEL, "Routing: keeping invalid route because "
	     "we're not doing filtering", *inout_rec);
	complain_error(err);
	(*inout_rec)->mrr_invalid = true;
	/*
	 * Our caller will add this route when we return.
	 */
    }

  bail:
    return (err);
}


/* ------------------------------------------------------------------------- */
/* To an array of routes, add whichever statically configured ones we
 * want to make sure we have, for a given family (AF_INET or AF_INET6).
 */
static int
md_mgmtd_routes_add_wanted_family(md_commit * commit, const mdb_db * db,
				  int family,
				  const md_intf_network_array * intf_nets,
				  const tstr_array * intfs_ipv4_up,
				  const tstr_array * intfs_ipv6_up,
				  md_mgmt_route_rec_array * routes,
				  tbool * ret_have_static_default)
{
    int err = 0;
    const char *subtree_root = NULL;
    bn_binding_array *pbindings = NULL, *nhbindings = NULL, *nkn_bindings =
	NULL;
    int i = 0, k = 0, num_prefixes = 0, num_nhs = 0;
    const bn_binding *binding = NULL;
    const tstring *bname_root = NULL;
    char *bname = NULL;
    tbool found = false, enable = false;
    bn_inetprefix prefix = lia_inetprefix_any;
    bn_inetaddr gw = lia_inetaddr_any;
    bn_inetaddr src = lia_inetaddr_any;
    uint32 metric = 0;
    tstr_array *bname_parts = NULL;
    const tstr_array *bname_parts_const = NULL;
    const char *prefix_str = NULL, *nh = NULL;
    tstring *nh_type_str = NULL, *nh_intf = NULL;
    md_route_type nh_type = 0;
    md_mgmt_route_rec *rec = NULL;
    bn_binding *gw_binding = NULL;
    bn_binding *src_binding = NULL;
    tbool have_static_default = false;
    tbool keep_rec = false;
    const char *nkn_subtree_root = NULL;
    const lc_enum_string_map *map = NULL;

    switch (family) {
    case AF_INET:
	subtree_root = "/net/routes/config/ipv4/prefix";
	nkn_subtree_root = "/nkn/net/routes/config/ipv4/prefix";
	break;

    case AF_INET6:
	subtree_root = "/net/routes/config/ipv6/prefix";
	break;

    default:
	bail_force(lc_err_unexpected_case);
	break;
    }

    err = mdb_iterate_binding(commit, db, subtree_root, 0, &pbindings);
    bail_error(err);

    err =
	mdb_iterate_binding(commit, db, nkn_subtree_root, 0,
			    &nkn_bindings);
    bail_error(err);

    err = bn_binding_array_append_array(pbindings, nkn_bindings);
    bail_error(err);
    num_prefixes = bn_binding_array_length_quick(pbindings);
    for (i = 0; i < num_prefixes; ++i) {
	binding = bn_binding_array_get_quick(pbindings, i);
	bail_null(binding);
	err = bn_binding_get_name(binding, &bname_root);
	bail_error(err);

	/* ....................................................................
	 * We only want this prefix if it's enabled.
	 */
	safe_free(bname);
	bname = smprintf("%s/enable", ts_str(bname_root));
	bail_null(bname);
	err =
	    mdb_get_node_value_tbool(commit, db, bname, 0, &found,
				     &enable);
	bail_error(err);
	complain_require_msg(found, "Missing node %s", bname);
	if (!enable) {
	    continue;
	}

	/* ....................................................................
	 * For IPv4, get the metric on a per-prefix level.
	 * For IPv6, we'll get it below, from the next hop.
	 */
	if (family == AF_INET) {
	    safe_free(bname);
	    bname = smprintf("%s/metric", ts_str(bname_root));
	    bail_null(bname);
	    err = mdb_get_node_value_uint32(commit, db, bname, 0, &found,
					    &metric);
	    bail_error(err);
	    complain_require_msg(found, "Missing node %s", bname);
	}

	/* ....................................................................
	 * Get the prefix itself, in the form we'll want to add it to
	 * the array.
	 */
	err = bn_binding_get_name_parts_const(binding, &bname_parts_const,
					      NULL);
	bail_error(err);
	if (bname_parts_const == NULL) {
	    tstr_array_free(&bname_parts);
	    err = bn_binding_get_name_parts(binding, &bname_parts);
	    bail_error(err);
	    bname_parts_const = bname_parts;
	}

	/* XXX: JUNIPER ADDED
	 * We use  both /net/routes and /nkn/net/routes. So the
	 * prefix index on the binding name parts gets shifted.
	 * Account for this shift here and get the prefix_str
	 * accordingly.
	 */
	if (tstr_array_length_quick(bname_parts_const) ==
	    md_mgmtd_routes_prefix_idx + 1) {
	    prefix_str =
		tstr_array_get_str_quick(bname_parts_const,
					 md_mgmtd_routes_prefix_idx);
	} else {
	    prefix_str = tstr_array_get_str_quick
		(bname_parts_const, md_mgmtd_routes_prefix_idx + 1);
	}
	bail_null(prefix_str);

	err = lia_str_to_inetprefix(prefix_str, &prefix);
	bail_error(err);

	if (lia_inetprefix_is_zero_quick(&prefix)) {
	    have_static_default = true;
	}

	/* ....................................................................
	 * Get all the next hops for this prefix, and add an entry to the
	 * array for each one.
	 */
	safe_free(bname);
	bname = smprintf("%s/nh", ts_str(bname_root));
	bail_null(bname);
	bn_binding_array_free(&nhbindings);
	err = mdb_iterate_binding(commit, db, bname, 0, &nhbindings);
	bail_error(err);
	num_nhs = bn_binding_array_length_quick(nhbindings);
	for (k = 0; k < num_nhs; ++k) {
	    binding = bn_binding_array_get_quick(nhbindings, k);
	    bail_null(binding);
	    err = bn_binding_get_name(binding, &bname_root);
	    bail_error(err);

	    /* ................................................................
	     * For IPv6, this is where we get the metric.
	     */
	    if (family == AF_INET6) {
		safe_free(bname);
		bname = smprintf("%s/metric", ts_str(bname_root));
		bail_null(bname);
		err =
		    mdb_get_node_value_uint32(commit, db, bname, 0, &found,
					      &metric);
		bail_error(err);
		complain_require_msg(found, "Missing node %s", bname);
	    }

	    /* ................................................................
	     * Get the gw, interface, and type for this nexthop.
	     */
	    ts_free(&nh_type_str);
	    err = mdb_get_child_value_tstr(commit, db, "type", binding, 0,
					   &nh_type_str);
	    bail_error(err);
	    nh_type = 0;
	    if (nh_type_str) {
		map = (family == AF_INET) ?
		    md_route_type_ipv4_config_map : md_route_type_new_map;
		err = lc_string_to_enum(md_route_type_ipv4_config_map,
					ts_str(nh_type_str), &nh_type);
		bail_error(err);
	    }

	    ts_free(&nh_intf);
	    err =
		mdb_get_child_value_tstr(commit, db, "interface", binding,
					 0, &nh_intf);
	    bail_error(err);
	    if (ts_num_chars(nh_intf) == 0) {
		ts_free(&nh_intf);	/* Want to be able to test for NULL later */
	    }

	    safe_free(bname);
	    bname = smprintf("%s/gw", ts_str(bname_root));
	    bail_null(bname);

	    bn_binding_free(&gw_binding);
	    err = mdb_get_node_binding(commit, db, bname, 0, &gw_binding);
	    bail_error(err);
	    if (family == AF_INET) {
		err =
		    bn_binding_get_ipv4addr_inetaddr(gw_binding, ba_value,
						     NULL, &gw);
		bail_error(err);
	    } else {
		err =
		    bn_binding_get_ipv6addr_inetaddr(gw_binding, ba_value,
						     NULL, &gw);
		bail_error(err);
	    }

	    /* XXX: JUNIPER Added */
	    safe_free(bname);
	    bname = smprintf("%s/source", ts_str(bname_root));
	    bail_null(bname);

	    bn_binding_free(&src_binding);
	    err = mdb_get_node_binding(commit, db, bname, 0, &src_binding);
	    bail_error(err);
	    src = lia_inetaddr_ipv4addr_any;
	    if (src_binding && family == AF_INET) {
		err =
		    bn_binding_get_ipv4addr_inetaddr(src_binding, ba_value,
						     NULL, &src);
		bail_error(err);
	    } else {
		src = lia_inetaddr_ipv4addr_any;
	    }

	    /* ................................................................
	     * Copy all this info into a new md_mgmt_route_rec.  Verify it
	     * against the set of current interface state, and if it's
	     * approved, add it to the array.
	     */
	    md_mgmt_route_rec_free(&rec);
	    err = md_mgmt_route_rec_new(&rec);
	    bail_error_null(err, rec);
	    if (nh_intf) {
		err = ts_detach(&nh_intf, &(rec->mrr_ifname), NULL);
		bail_error(err);
	    }

	    rec->mrr_family = family;
	    rec->mrr_type = nh_type;
	    rec->mrr_metric = metric;
	    rec->mrr_prefix = prefix;
	    rec->mrr_gateway = gw;
	    rec->mrr_protocol = RTPROT_STATIC;
	    rec->mrr_srcaddr = src;

	    err = md_route_check_route_intfs(rec, intf_nets,
					     intfs_ipv4_up, intfs_ipv6_up,
					     &keep_rec);
	    bail_error(err);

	    /*
	     * If we don't think the route should be kept, discard it
	     * (if we're doing discarding in general).  This will free
	     * the record if it's to be discarded; leave it alone
	     * otherwise.
	     */
	    if (!keep_rec) {
		err =
		    md_mgmtd_routes_maybe_discard_rec(commit, db, routes,
						      &rec);
		bail_error(err);
	    }
	    if (rec) {
		err =
		    md_mgmt_route_rec_array_append_takeover(routes, &rec);
		bail_error(err);
	    }
	}
    }

  bail:
    safe_free(bname);
    tstr_array_free(&bname_parts);
    bn_binding_array_free(&pbindings);
    bn_binding_array_free(&nhbindings);
    bn_binding_array_free(&nkn_bindings);
    bn_binding_free(&gw_binding);
    ts_free(&nh_type_str);
    ts_free(&nh_intf);
    if (ret_have_static_default) {
	*ret_have_static_default = have_static_default;
    }
    return (err);
}


/* ------------------------------------------------------------------------- */
/* Remove any routes that match this one, not counting "protocol".
 * Then append this one.
 */
static int
md_mgmtd_routes_replace(md_mgmt_route_rec_array * routes,
			md_mgmt_route_rec ** inout_route)
{
    int err = 0;
    md_mgmt_route_rec *old_route = NULL, *new_route = NULL;
    int i = 0, num_routes = 0;
    int cmp = 0;

    bail_null(routes);
    bail_null(inout_route);
    new_route = *inout_route;
    bail_null(new_route);

    num_routes = md_mgmt_route_rec_array_length_quick(routes);
    for (i = 0; i < num_routes; ++i) {
	old_route = md_mgmt_route_rec_array_get_quick(routes, i);
	bail_null(old_route);
	cmp =
	    md_mgmt_route_rec_compare(old_route, new_route, false, false,
				      true);
	if (cmp == 0) {
	    err =
		md_mgmtd_routes_dump_rec(MD_ROUTES_LOG_LEVEL,
					 "Replace: old", old_route);
	    bail_error(err);
	    err =
		md_mgmtd_routes_dump_rec(MD_ROUTES_LOG_LEVEL,
					 "Replace: new", new_route);
	    bail_error(err);
	    err = md_mgmt_route_rec_array_delete(routes, i);
	    bail_error(err);
	    --i;
	    --num_routes;
	}
    }

    err = md_mgmt_route_rec_array_append_takeover(routes, inout_route);
    bail_error(err);

  bail:
    return (err);
}


/* ------------------------------------------------------------------------- */
/* Create an array that reflects what we wish the routing state of
 * the system were right now, based on configuration plus whatever
 * overrides/overlays are in effect.
 */
static int
md_mgmtd_routes_calc_wanted(md_commit * commit, const mdb_db * db,
			    const md_mgmt_route_rec_array * routes_have,
			    md_mgmt_route_rec_array ** ret_routes_want)
{
    int err = 0;
    md_mgmt_route_rec_array *routes = NULL;
    tbool have_static_default = false, yield = false;
    tstr_array *overlay_routes = NULL;
    md_intf_network_array *intf_nets = NULL;
    tstr_array *intfs_ipv4_up = NULL, *intfs_ipv6_up = NULL;
    tbool keep_rec = false, found = false;
    tbool filter_ipv4 = false, filter_ipv6 = false;
    int i = 0, num_nhs = 0;
    const char *nh = NULL;
    md_mgmt_route_rec *new_rec = NULL;
    const md_mgmt_route_rec *rec = NULL, *next_rec = NULL;
    struct in_addr zeroaddr;
    int32 cmp = 0;
    char *msg = NULL;

    memset(&zeroaddr, 0, sizeof(zeroaddr));

    bail_null(db);
    bail_null(ret_routes_want);

    /*
     * We need to know what the current interface state is, so we can
     * figure out which interfaces a route would be valid for.
     */
    lc_log_basic(MD_ROUTES_LOG_LEVEL,
		 "Routing: getting list of up interfaces "
		 "and their networks...");
    err =
	md_mgmt_get_interface_networks(commit, db, true, routes_have,
				       &intf_nets, &intfs_ipv4_up, NULL);
    bail_error(err);

    err = md_mgmt_route_rec_array_new(&routes);
    bail_error(err);

    /* ------------------------------------------------------------------------
     * Add statically-configured routes.
     *
     * Note that we only care about whether there's a static default
     * in the IPv4 case, since that's the only place where we have
     * overrides (for now).
     */
    err = md_mgmtd_routes_add_wanted_family(commit, db, AF_INET, intf_nets,
					    intfs_ipv4_up, intfs_ipv6_up,
					    routes, &have_static_default);
    bail_error(err);
#if 0
    if (md_proto_ipv6_enabled(commit, db)) {
	err =
	    md_mgmtd_routes_add_wanted_family(commit, db, AF_INET6,
					      intf_nets, intfs_ipv4_up,
					      intfs_ipv6_up, routes, NULL);
	bail_error(err);
    }
#endif

    /* ........................................................................
     * The 'routes' array now reflects the statically-configured
     * routes.  Now make any adjustments triggered by our overlays.
     * Essentially, we just want to add whichever default routes are
     * listed for the acting primary DHCP interface.  EXCEPT we don't
     * do this if we already have a statically-configured default
     * route, and the 'yield_to_static' flag is set.
     */
    yield = false;
    if (have_static_default) {
	err = mdb_get_node_value_tbool
	    (commit, db,
	     "/net/general/config/default_gateway/yield_to_static", 0,
	     &found, &yield);
	bail_error(err);
	complain_require(found);
    }

    if (!yield) {
	err =
	    md_mgmtd_routes_get_extras(commit, db, NULL, &overlay_routes);
	bail_error(err);
	num_nhs =
	    overlay_routes ? tstr_array_length_quick(overlay_routes) : 0;
	for (i = 0; i < num_nhs; ++i) {
	    nh = tstr_array_get_str_quick(overlay_routes, i);
	    bail_null(nh);
	    err = md_mgmt_route_rec_new(&new_rec);
	    bail_error_null(err, new_rec);
	    new_rec->mrr_ifindex = -1;
	    new_rec->mrr_metric = md_mgmtd_routes_default_metric_int;
	    new_rec->mrr_type = mrt_unicast;
	    new_rec->mrr_family = AF_INET;
	    new_rec->mrr_protocol = MD_ROUTE_ORIGIN_DHCP;
	    err = lia_ipv4addr_to_inetaddr
		(&lia_ipv4addr_any, &(new_rec->mrr_srcaddr));
	    bail_error(err);

	    /*
	     * We can't use lia_inetprefix_any here.  This is an IPv4
	     * route, and we need it to have that family.
	     */
	    err = lia_inaddr_masklen_to_inetprefix(&zeroaddr, 0,
						   &(new_rec->mrr_prefix));
	    bail_error(err);

	    err = lia_str_to_inetaddr(nh, &(new_rec->mrr_gateway));
	    bail_error(err);

	    err = md_route_check_route_intfs(new_rec, intf_nets,
					     intfs_ipv4_up, intfs_ipv6_up,
					     &keep_rec);
	    bail_error(err);

	    if (!keep_rec) {
		err = md_mgmtd_routes_maybe_discard_rec(commit, db, routes,
							&new_rec);
		bail_error(err);
	    }
	    if (new_rec) {
		err = md_mgmtd_routes_replace(routes, &new_rec);
		bail_error(err);
	    }
	}
    }

    /* ........................................................................
     * Sort our routes array, so it can be compared easily with the
     * runtime state array, which is also sorted by the same criteria.
     */
    err = md_mgmt_route_rec_array_sort(routes);
    bail_error(err);

    /* ........................................................................
     * Finally, scan the result and remove any redundancies.  The
     * array's duplicate policy prevents outright duplicates, but what
     * we might still have is two routes which are identical except
     * that one specifies a device, and the other doesn't.  This could
     * cause apply errors: say we apply the one without a device first,
     * and it gets assigned to the device named by the second.  This
     * would result in "RTNETLINK answers: File exists".
     *
     * Note that we only do this if filtering is enabled for the given
     * family.  We don't want to remove anything we needed -- e.g. what
     * if the route wouldn't apply to the interface named, so we need
     * the record without the interface name, to get it assigned to a
     * good interface?  If filtering is enabled, the presence of the
     * other one should mean that it will succeed.  If filtering is
     * disabled, all bets are off, but we didn't want to try to remove
     * anything anyway, and any errors (like the "file exists" one)
     * will be ignored.
     *
     * So if we see such records (the one without the interface name
     * should always come first, due to our sorting criteria), delete
     * the one
     */
    err = mdb_get_node_value_tbool
	(commit, db, "/net/general/config/filter_routes", 0, &found,
	 &filter_ipv4);
    bail_error(err);
    complain_require(found);
    err = mdb_get_node_value_tbool
	(commit, db, "/net/general/config/filter_ipv6_routes", 0, &found,
	 &filter_ipv6);
    bail_error(err);
    complain_require(found);
    num_nhs = md_mgmt_route_rec_array_length_quick(routes);
    for (i = 0; i < num_nhs - 1; ++i) {
	rec = md_mgmt_route_rec_array_get_quick(routes, i);
	bail_null(rec);
	if ((rec->mrr_family == AF_INET && !filter_ipv4) ||
	    (rec->mrr_family == AF_INET6 && !filter_ipv6)) {
	    continue;
	}
	if (rec->mrr_ifname == NULL || rec->mrr_ifname[0] == '\0') {
	    next_rec = md_mgmt_route_rec_array_get_quick(routes, i + 1);
	    bail_null(next_rec);

	    cmp =
		md_mgmt_route_rec_compare(rec, next_rec, false, true,
					  false);
	    if (cmp == 0) {
		/*
		 * We have found a record (rec) which is the same as
		 * another record (next_rec), except that it has no
		 * device (interface) name.  So as explained above,
		 * we can -- nay, we must! -- delete it.
		 */
		safe_free(msg);
		msg =
		    smprintf(_
			     ("Routing: deleting redundant route (found "
			      "another identical, except with interface "
			      "'%s')"),
			     next_rec->mrr_ifname ? next_rec->
			     mrr_ifname : "(null)");
		bail_null(msg);
		err =
		    md_mgmtd_routes_dump_rec(MD_ROUTES_LOG_LEVEL, msg,
					     rec);
		complain_error(err);

		err = md_mgmt_route_rec_array_delete(routes, i);
		bail_error(err);
		--i;
		--num_nhs;
	    }
	}
    }

    /*
     * XXX/EMT: we could be vulnerable to race conditions here, in
     * case an interface goes up or down while we're doing this
     * calculation.  Could query interface state both before and after
     * querying routes, and repeat if there is a change...
     */

  bail:
    safe_free(msg);
    md_intf_network_array_free(&intf_nets);
    tstr_array_free(&intfs_ipv4_up);
    tstr_array_free(&intfs_ipv6_up);
    md_mgmt_route_rec_free(&new_rec);	/* Only in error case */
    /* Do NOT free overlay_routes; we can modify, but do not own it */
    if (ret_routes_want) {
	*ret_routes_want = routes;
    }
    return (err);
}


/* ------------------------------------------------------------------------- */
/* Add or remove a route (according to the 'want' parameter) specified
 * in the 'rec' parameter.
 */
static int
md_mgmtd_routes_change_route_rec(md_commit * commit, tbool want,
				 const md_mgmt_route_rec * rec)
{
    int err = 0;
    char *prefix_str = NULL, *gw_str = NULL, *metric_str = NULL, *src_str =
	NULL;
    tstring *errors = NULL;
    uint16 code = 0;
    tbool doing_apply = false;
    bn_inetaddr gw = LIA_INETADDR_IPV4ADDR_ANY_INIT;
    bn_inetaddr src = LIA_INETADDR_IPV4ADDR_ANY_INIT;
    tbool is_srcvalid = false;

    bail_null(rec);

    err = lia_inetprefix_to_str(&(rec->mrr_prefix), &prefix_str);
    bail_error_null(err, prefix_str);
    if (rec->mrr_type == mrt_unicast) {
	if (rec->mrr_family == AF_INET6 &&
	    lia_inetaddr_is_zero_quick(&(rec->mrr_gateway))) {
	    /*
	     * "ip -6" doesn't like it when we say "via ::", while "ip -4"
	     * is fine with "via 0.0.0.0".  So leave the gw_str NULL in
	     * this case, to omit the argument.
	     */
	} else {
	    err = lia_inetaddr_to_str(&(rec->mrr_gateway), &gw_str);
	    bail_error(err);
	    gw = rec->mrr_gateway;
	}

	err = lia_inetaddr_is_zero(&(rec->mrr_srcaddr), &is_srcvalid);
	if (err == lc_err_bad_type) {
	    /* XXX: JUNIPER ADDED
	     * Do nothing. no source address specification was
	     * given for this route.
	     */
	    /* do nothing */
	} else {
	    err = lia_inetaddr_to_str(&(rec->mrr_srcaddr), &src_str);
	    bail_error(err);
	    src = rec->mrr_srcaddr;

	}
    }
    metric_str = smprintf("%" PRIu32, rec->mrr_metric);
    bail_null(metric_str);

    if (want) {
	err =
	    md_mgmtd_routes_change_route(rec->mrr_family, mro_add,
					 prefix_str, &gw, gw_str, &src,
					 src_str, metric_str,
					 rec->mrr_ifname,
					 rec->mrr_protocol, &errors);
	bail_error(err);
	if (errors) {
	    if (rec->mrr_invalid) {
		/*
		 * We suspected this route was invalid, so we are not
		 * surprised to have gotten errors.  We'll stifle these
		 * from the user, since otherwise they would be overrun
		 * with errors.  The details of the outcome are available
		 * in logs at INFO, as logged by md_mgmtd_routes_change_route().
		 */
	    } else {
		/*
		 * We only want to return an "advisory" error, not a real
		 * failure.  The only case we can set an error code
		 * without it being considered a failure is during an
		 * apply.  Otherwise (e.g. processing an action request,
		 * such as one initiated by DHCP) we cannot return a code,
		 * but can still return a message.
		 */
		err = md_commit_get_doing_apply(commit, &doing_apply);
		bail_error(err);
		code = (doing_apply) ? 1 : 0;
		err = md_commit_set_return_status_msg_fmt
		    (commit, code,
		     _("Error applying gateway %s for prefix " "%s: %s"),
		     gw_str ? gw_str : "NONE", prefix_str, ts_str(errors));
		bail_error(err);
	    }
	}
    } else {
	err =
	    md_mgmtd_routes_change_route(rec->mrr_family, mro_delete,
					 prefix_str, &gw, gw_str, &src,
					 src_str, metric_str,
					 rec->mrr_ifname,
					 rec->mrr_protocol, NULL);
	bail_error(err);
    }

  bail:
    safe_free(prefix_str);
    safe_free(gw_str);
    safe_free(metric_str);
    ts_free(&errors);
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_dump_rec(int log_level, const char *prefix,
			 const md_mgmt_route_rec * rec)
{
    int err = 0;
    tbool is_zero = false;
    char *gw_str = NULL, *prefix_str = NULL, *src_str = NULL;

    err = lia_inetaddr_is_zero(&(rec->mrr_gateway), &is_zero);
    if (err || is_zero) {	/* Error if family not set */
	err = 0;
	gw_str = strdup("NONE");
	bail_null(gw_str);
    } else {
	err = lia_inetaddr_to_str(&(rec->mrr_gateway), &gw_str);
	bail_error(err);
    }

    err = lia_inetprefix_is_zero(&(rec->mrr_prefix), &is_zero);
    if (err || is_zero) {	/* Error if family not set */
	err = 0;
	prefix_str = strdup("NONE");
	bail_null(prefix_str);
    } else {
	err = lia_inetprefix_to_str(&(rec->mrr_prefix), &prefix_str);
	bail_error(err);
    }

    err = lia_inetaddr_is_zero(&(rec->mrr_srcaddr), &is_zero);
    if (err || is_zero) {
	err = 0;
	src_str = strdup("NONE");
	bail_null(src_str);
    } else {
	err = lia_inetaddr_to_str(&(rec->mrr_srcaddr), &src_str);
	bail_error(err);
    }

    lc_log_basic(log_level, "%s: prefix %s, gw %s, metric %" PRIu32
		 ", intf %s, table %" PRId64
		 ", proto %d, flags 0x%x, src %s", prefix, prefix_str,
		 gw_str ? gw_str : "NONE", rec->mrr_metric,
		 rec->mrr_ifname ? rec->mrr_ifname : "NONE",
		 rec->mrr_table, rec->mrr_protocol, rec->mrr_flags,
		 src_str);

  bail:
    safe_free(gw_str);
    safe_free(src_str);
    safe_free(prefix_str);
    return (err);
}


/* ------------------------------------------------------------------------- */
/* Given representations of what routes we want, and what routes we
 * currently have, apply any necessary changes to the system.  This
 * will generally involve a combination of route deletions and
 * additions.
 *
 * There is no such thing as a "modification", since ALL of the fields
 * in md_mgmt_route_rec are keys in the routing table.  That is, if any one
 * field is different (except mrr_type, which does not get applied
 * directly anyway, but only indicates whether or not we should expect
 * anything in mrr_gw), the record would be a separate entry.
 *
 * We destroy the arrays passed to us.  This avoids needing to
 * duplicate the records or have fancy memory ownership, since we want
 * to build up two different arrays containing a subset of these
 * records: one of routes to be added, and one of routes to be
 * removed.  Some of the routes from the two arrays will not end up in
 * either, in the case where no changes are needed.
 */
static int
md_mgmtd_routes_apply_diffs(md_commit * commit,
			    md_mgmt_route_rec_array ** inout_rwant,
			    md_mgmt_route_rec_array ** inout_rhave)
{
    int err = 0;
    int nwant = 0, nhave = 0;
    int iwant = 0, ihave = 0;
    int cmp = 0;
    md_mgmt_route_rec_array *rwant = NULL, *rhave = NULL;
    md_mgmt_route_rec_array *radd = NULL, *rdel = NULL;
    md_mgmt_route_rec *cwant = NULL, *chave = NULL;
    int chk = 0;
    char *pfx = NULL;
    static int iter = 0;
    int i = 0, num_add = 0, num_del = 0;
    int32 status = 0;
    tstring *output = NULL;

    bail_null(inout_rwant);
    bail_null(inout_rhave);
    rwant = *inout_rwant;
    rhave = *inout_rhave;
    bail_null(rwant);
    bail_null(rhave);

    nwant = md_mgmt_route_rec_array_length_quick(rwant);
    nhave = md_mgmt_route_rec_array_length_quick(rhave);

    lc_log_basic(MD_ROUTES_LOG_LEVEL,
		 "Routing: check #%d: nwant=%d, nhave=%d", ++iter, nwant,
		 nhave);

    err = md_mgmt_route_rec_array_new(&radd);
    bail_error(err);
    err = md_mgmt_route_rec_array_new(&rdel);
    bail_error(err);

    /* ........................................................................
     * First, figure out which wanted routes are to be added, which
     * existing routes are to be deleted, and which are to be left
     * alone.  We could add and delete as we go along, except that we
     * want to do all adds before all deletes, to minimize unnecessary
     * disruptions (e.g. temporarily not having a route for some prefix,
     * when all we are doing is changing its metric).
     */
    while (iwant < nwant || ihave < nhave) {
	cwant = md_mgmt_route_rec_array_get_quick(rwant, iwant);
	chave = md_mgmt_route_rec_array_get_quick(rhave, ihave);
	/* Either may be NULL, but not both */
	bail_require(cwant || chave);

	++chk;
	if (cwant) {
	    safe_free(pfx);
	    pfx = smprintf("Routing: check %d, cwant[%d]", chk, iwant);
	    bail_null(pfx);
	    err =
		md_mgmtd_routes_dump_rec(MD_ROUTES_LOG_LEVEL, pfx, cwant);
	    bail_error(err);
	}
	if (chave) {
	    safe_free(pfx);
	    pfx = smprintf("Routing: check %d, chave[%d]", chk, ihave);
	    bail_null(pfx);
	    err =
		md_mgmtd_routes_dump_rec(MD_ROUTES_LOG_LEVEL, pfx, chave);
	    bail_error(err);
	}

	cmp = md_mgmt_route_rec_compare(cwant, chave, false, false, false);

	/*
	 * Adjust the comparison in one way: if these two are
	 * identical except that cwant has no device name, and chave
	 * has one, then consider them identical... IFF chave's
	 * interface name is on cwant's list of eligible interface
	 * names (computed earlier).
	 */
	if (cmp < 0 && cwant != NULL && cwant->mrr_ifname == NULL) {
	    lc_log_basic(MD_ROUTES_LOG_LEVEL,
			 "Routing: reconsidering route "
			 "match with cwant's ifname being any of those "
			 "eligible interfaces");
	    cmp =
		md_mgmt_route_rec_compare(cwant, chave, true, false,
					  false);
	}

	if (cmp == 0) {
	    /*
	     * cwant is the same as chave, so we don't have to do
	     * anything with either one.  Just skip over.
	     */
	    lc_log_basic(MD_ROUTES_LOG_LEVEL, "Routing: records match, "
			 "no action");
	    ++iwant;
	    ++ihave;
	} else if (!chave || (cwant && cmp < 0)) {
	    /*
	     * !chave means we're at the end of the routes we have, and
	     * there are only ones we want left.  Or, cmp<0 means that
	     * cwant comes before chave, meaning that we don't have any
	     * route that matches cwant.
	     *
	     * In either case, we want to add cwant.
	     */
	    bail_null(cwant);
	    lc_log_basic(MD_ROUTES_LOG_LEVEL, "Routing: wanted record "
			 "missing, adding route");
	    cwant = NULL;
	    err =
		md_mgmt_route_rec_array_remove_no_shift(rwant, iwant++,
							&cwant);
	    bail_error(err);
	    err = md_mgmt_route_rec_array_append_takeover(radd, &cwant);
	    bail_error(err);
	} else if (!cwant || (chave && cmp > 0)) {
	    /*
	     * !cwant means we're at the end of the routes we want, and
	     * there are only ones to delete left.  Or, cmp>0 means that
	     * chave comes before cwant, meaning that we don't want the
	     * route that matches chave.
	     *
	     * In either case, we want to delete chave.
	     *
	     * That is, unless the route is one which was added by
	     * something else with which we don't want to interfere.
	     * e.g. it could be an interface route, added automatically
	     * by the kernel.  We use rtnetlink to get the "protocol"
	     * field to help us distinguish, and simply skip over
	     * such records.
	     *
	     * The only records we think we own are ones whose
	     * protocol is RTPROT_STATIC, and which are in the main
	     * routing table (RT_TABLE_MAIN).  Anything else we assume
	     * is there for a reason, and is not to be touched.
	     *
	     * We had hoped we could also claim ownership of anything
	     * with RTPROT_BOOT -- though there's at least one case of
	     * such a route which we don't want to clobber.  When
	     * virtualization is enabled, it adds a route from
	     * fe80::/64 to one of the virbr interfaces, to the main
	     * table, with RTPROT_BOOT.  We're not sure what that's
	     * for, but don't want to disturb it.
	     */
	    bail_null(chave);
#ifdef PROD_TARGET_OS_LINUX
	    if ((chave->mrr_protocol == RTPROT_STATIC ||
		 chave->mrr_protocol == MD_ROUTE_ORIGIN_DHCP) &&
		chave->mrr_table == RT_TABLE_MAIN) {
#else
	    if (chave->mrr_protocol != -1) {
#endif

		/* XXX: JUNIPER ADDED
		 * This seems to cause an issue when DHCP added routes are added.
		 * Basically, DHCP added routes are removed because this module
		 * thinks it did not add the route.
		 */
		lc_log_basic(MD_ROUTES_LOG_LEVEL,
			     "Routing: existing record "
			     "unwanted (and originally created from "
			     "userland), removing route");
		chave = NULL;

		err =
		    md_mgmt_route_rec_array_remove_no_shift(rhave, ihave,
							    &chave);
		bail_error(err);
		err =
		    md_mgmt_route_rec_array_append_takeover(rdel, &chave);
		bail_error(err);

	    } else if (chave->mrr_protocol == -1) {
		err = md_mgmtd_routes_dump_rec(LOG_INFO, "Mystery route "
					       "(origin unknown), leaving alone",
					       chave);
		bail_error(err);
	    } else {
		lc_log_basic(MD_ROUTES_LOG_LEVEL,
			     "Routing: existing record "
			     "unwanted, but leaving it alone since we did not "
			     "install it ourselves");
	    }
	    ++ihave;

	}			/* else if (!cwant || (chave && cmp > 0)) { */
    }				/* while (iwant < nwant || ihave < nhave) { */

    /* ........................................................................
     * Add all the routes we wanted but didn't have.
     */
    num_add = md_mgmt_route_rec_array_length_quick(radd);
    for (i = 0; i < num_add; ++i) {
	cwant = md_mgmt_route_rec_array_get_quick(radd, i);
	bail_null(cwant);
	lc_log_basic(MD_ROUTES_LOG_LEVEL, "Routing: doing add...");
	err = md_mgmtd_routes_change_route_rec(commit, true, cwant);
	bail_error(err);
    }

    /* ........................................................................
     * Delete all the routes we had but didn't want.
     */
    num_del = md_mgmt_route_rec_array_length_quick(rdel);
    for (i = 0; i < num_del; ++i) {
	chave = md_mgmt_route_rec_array_get_quick(rdel, i);
	bail_null(chave);
	lc_log_basic(MD_ROUTES_LOG_LEVEL, "Routing: doing delete...");
	err = md_mgmtd_routes_change_route_rec(commit, false, chave);
	bail_error(err);
    }

    /*
     * XXXX/EMT: we could go through and re-add all the routes we
     * wanted.  This should not be necessary, but would be a defensive
     * measure against some unexpected scenario where a route we added
     * was deleted in the delete phase.  e.g. see bug 12110.
     */

    /* ........................................................................
     * There are rumors that the kernel has a "route cache" which
     * sometimes gets out of sync with the routing config you have
     * applied.  This sounds like a kernel bug, but the people
     * reporting this suggest flushing the cache after changing
     * routing config.
     */
    if (num_add > 0 || num_del > 0) {
	/* XXX #dep/args: ip */
	err = lc_launch_quick(&status, &output, 4, prog_ip_path, "route",
			      "flush", "cache");
	bail_error(err);
	err = lc_check_exit_status_full(status, NULL, MD_ROUTES_LOG_LEVEL,
					LOG_INFO, LOG_ERR,
					"ip route flush cache");
	bail_error(err);
	if (ts_num_chars(output) > 2) {
	    /*
	     * lc_check_exit_status_full() should really have done this
	     * for us!
	     */
	    lc_log_basic(LOG_INFO,
			 "Output from 'ip route flush cache': %s",
			 ts_str(output));
	}
    }

  bail:
    /*
     * Since we have probably already removed some or all of the
     * elements from these arrays, we might as well delete them for
     * the caller.
     */
    if (inout_rwant) {
	md_mgmt_route_rec_array_free(inout_rwant);
    }
    if (inout_rhave) {
	md_mgmt_route_rec_array_free(inout_rhave);
    }
    md_mgmt_route_rec_array_free(&radd);
    md_mgmt_route_rec_array_free(&rdel);
    safe_free(pfx);
    ts_free(&output);
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_reassert_handle_timer(int fd, short event_type, void *data,
				      lew_context * ctxt)
{
    int err = 0;
    md_commit *commit = NULL;
    mdb_db **dbp = NULL;
    tstring *reason = NULL;

    /*
     * XXX/EMT: if another reassert has already been done since the
     * last state change event we received, we could skip it here.
     * But we still might want to log something.
     */

    err = lew_event_cancel(md_lwc, &md_mgmtd_routes_reassert_event_short);
    bail_error(err);

    err = lew_event_cancel(md_lwc, &md_mgmtd_routes_reassert_event_long);
    bail_error(err);

    err = md_commit_create_commit(&commit);
    bail_error_null(err, commit);

    err = md_commit_get_main_db(commit, &dbp);
    bail_error(err);
    bail_null(dbp);

    /* ........................................................................
     * Make up the reason string, for logging purposes.
     */

    /* XXX/EMT: numeric sort? */
    err = tstr_array_sort(md_mgmtd_routes_ifnames_changed);
    bail_error(err);

    err =
	ts_join_tokens(md_mgmtd_routes_ifnames_changed, ',', 0, 0, 0,
		       &reason);
    bail_error(err);

    err = ts_insert_sprintf(reason, 0, _("Now handling state change after "
					 "delay for %d interface(s): "),
			    tstr_array_length_quick
			    (md_mgmtd_routes_ifnames_changed));
    bail_error(err);

    /*
     * Lest syslog chop less gracefully at 1024 characters.  Leave some
     * room for the prefix inserted by md_mgmtd_routes_reassert_state().
     */
    err = md_utils_log_truncate_value(reason, 900);
    bail_error(err);

    err = tstr_array_empty(md_mgmtd_routes_ifnames_changed);
    bail_error(err);

    /* ..................................................................... */

    err = md_mgmtd_routes_reassert_state(commit, *dbp, ts_str(reason));
    bail_error(err);

  bail:
    ts_free(&reason);
    md_commit_commit_state_free(&commit);
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_handle_ifstate_change(md_commit * commit,
				      const mdb_db * db,
				      const char *event_name,
				      const bn_binding_array * bindings,
				      void *cb_reg_arg, void *cb_arg)
{
    int err = 0;
    tstring *ifname = NULL;
    tbool short_pending = false, long_pending = false;
    lt_time_ms long_time_ms = 0;
    int log_level = LOG_INFO;

    err = bn_binding_array_get_value_tstr_by_name(bindings, "ifname", NULL,
						  &ifname);
    bail_error(err);

    /* Don't do takeover, since we may want it for logging below */
    err = tstr_array_append(md_mgmtd_routes_ifnames_changed, ifname);
    bail_error(err);

    /*
     * We basically just want to call md_mgmtd_routes_reassert_state().
     * But we're concerned that in some cases, there might be several
     * interface state change events in quick succession, and it can
     * get expensive to reassert routes too many times.  So we'll set
     * a timer, and only reassert after we've gone a certain time
     * without an event.  And to protect against starvation (in the
     * unlikely event of a continuous stream of events), we'll also
     * reassert no matter what after a longer interval.
     *
     * XXX/EMT: perf: could look inside state change event, and do a
     * partial reassert based on what's relevant?
     */

    err = lew_event_is_pending
	(md_lwc,
	 (const lew_event **) &md_mgmtd_routes_reassert_event_short,
	 &short_pending, NULL);
    bail_error(err);
    err = lew_event_is_pending
	(md_lwc, (const lew_event **) &md_mgmtd_routes_reassert_event_long,
	 &long_pending, &long_time_ms);
    bail_error(err);

    /*
     * XXX/EMT: if 'long_time_ms' is before now, then the reassert is overdue,
     * so we could force it now...
     */

    err =
	lew_event_reg_timer_full(md_lwc,
				 &md_mgmtd_routes_reassert_event_short,
				 "md_mgmtd_routes_reassert_event_short",
				 "md_mgmtd_routes_reassert_handle_timer",
				 md_mgmtd_routes_reassert_handle_timer,
				 NULL,
				 md_mgmtd_routes_reassert_event_short_ms);
    bail_error(err);

    complain_require(short_pending == long_pending);

    if (short_pending) {
	lc_log_basic(log_level,
		     "md_mgmtd_routes intf state change (%s): postponing "
		     "routes reassert to be %" PRId64 " ms out.  "
		     "Definite reconfig now %" PRId64 " ms out.",
		     ts_str(ifname),
		     md_mgmtd_routes_reassert_event_short_ms,
		     long_time_ms - lt_curr_time_ms());
    } else {
	err =
	    lew_event_reg_timer_full(md_lwc,
				     &md_mgmtd_routes_reassert_event_long,
				     "md_mgmtd_routes_reassert_event_long",
				     "md_mgmtd_routes_reassert_handle_timer",
				     md_mgmtd_routes_reassert_handle_timer,
				     NULL,
				     md_mgmtd_routes_reassert_event_long_ms);
	bail_error(err);
	lc_log_basic(log_level,
		     "md_mgmtd_routes intf state change (%s): scheduling "
		     "routes reassert for %" PRId64 " ms out.  "
		     "Even if it gets postponed, it will never be more "
		     "that %" PRId64 " ms out.", ts_str(ifname),
		     md_mgmtd_routes_reassert_event_short_ms,
		     md_mgmtd_routes_reassert_event_long_ms);
    }

  bail:
    ts_free(&ifname);
    return (err);
}


/* ------------------------------------------------------------------------- */
/* If the last nexthop for a given prefix is deleted, we want to
 * delete the prefix itself.  Otherwise, the inert node is just
 * leftover, cluttering the tree.
 */
static int
md_mgmtd_routes_nexthop_side_effects(md_commit * commit,
				     const mdb_db * old_db,
				     mdb_db * inout_new_db,
				     const mdb_db_change_array *
				     change_list, uint32 change_list_idx,
				     const tstring * node_name,
				     const tstr_array * node_name_parts,
				     mdb_db_change_type change_type,
				     const bn_attrib_array * old_attribs,
				     const bn_attrib_array * new_attribs,
				     const char *reg_name,
				     const tstr_array * reg_name_parts,
				     bn_type reg_value_type,
				     const bn_attrib_array *
				     reg_initial_attribs, void *arg)
{
    int err = 0;
    tstr_array *anc_name_parts = NULL;
    char *parent_name = NULL, *grandparent_name = NULL;
    const char *prefix_str = NULL, *nh_str = NULL;
    int num_parts = 0;
    bn_binding_array *peers = NULL;
    int num_peers = 0;

    if (change_type == mdct_delete) {
	prefix_str = tstr_array_get_str_quick(node_name_parts, 5);
	bail_null(prefix_str);
	nh_str = tstr_array_get_str_quick(node_name_parts, 7);
	bail_null(nh_str);
	lc_log_basic(LOG_INFO, "IPv6 routing prefix %s: next hop %s "
		     "deleted, checking for remaining peers...",
		     prefix_str, nh_str);

	/*
	 * Get the name of our parent, so we can see if we have any
	 * surviving siblings.
	 */
	err = tstr_array_duplicate(node_name_parts, &anc_name_parts);
	bail_error(err);
	num_parts = tstr_array_length_quick(anc_name_parts);
	err = tstr_array_delete(anc_name_parts, num_parts - 1);
	bail_error(err);
	--num_parts;
	err = bn_binding_name_parts_to_name(anc_name_parts, true,
					    &parent_name);
	bail_error(err);

	err = mdb_iterate_binding(commit, inout_new_db, parent_name, 0,
				  &peers);
	bail_error(err);

	/*
	 * If we have no peers left, let's delete our grandparent, the
	 * prefix itself.
	 */
	num_peers = bn_binding_array_length_quick(peers);
	if (num_peers == 0) {
	    err = tstr_array_delete(anc_name_parts, num_parts - 1);
	    bail_error(err);
	    --num_parts;
	    err = bn_binding_name_parts_to_name(anc_name_parts, true,
						&grandparent_name);
	    bail_error(err);

	    lc_log_basic(LOG_INFO, "IPv6 routing prefix %s: no more "
			 "next hops, so deleting it", prefix_str);

	    err = mdb_set_node_str(commit, inout_new_db, bsso_delete, 0,
				   bt_none, NULL, "%s", grandparent_name);
	    bail_error(err);
	} else {
	    lc_log_basic(LOG_INFO, "IPv6 routing prefix %s: still has "
			 "%d next hops, so leaving it", prefix_str,
			 num_peers);
	}
    }

  bail:
    tstr_array_free(&anc_name_parts);
    safe_free(parent_name);
    safe_free(grandparent_name);
    bn_binding_array_free(&peers);
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_add_initial(md_commit * commit, mdb_db * db, void *arg)
{
    int err = 0;

  bail:
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_upgrade(const mdb_db * old_db, mdb_db * inout_new_db,
			uint32 from_module_version,
			uint32 to_module_version, void *arg)
{
    int err = 0;
  bail:
    return (err);
}

static int md_mgmtd_routes_apply_order = 0;

/* ------------------------------------------------------------------------- */
int md_mgmtd_routes_init(const lc_dso_info * info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;
    tstr_array_options opts;

    err =
	md_mgmtd_routes_if_overlay_array_new(&md_mgmtd_routes_if_overlays);
    bail_error(err);

    err = mdm_add_module("nkn-routes", 1,
			 "/nkn/net/routes", "/nkn/net/routes/config",
			 0,
			 NULL, NULL,
			 md_mgmtd_routes_commit_check, NULL,
			 md_mgmtd_routes_commit_apply, NULL,
			 0, md_mgmtd_routes_apply_order,
			 md_mgmtd_routes_upgrade,
			 &md_mgmtd_routes_upgrade_rules,
			 md_mgmtd_routes_add_initial, NULL,
			 NULL, NULL, NULL, NULL, &module);
    bail_error(err);

    /*
     * If any of certain kinds of network-related config changes, we
     * want to re-apply routes.  The list of subtree roots here
     * is taken from md_mgmtd_routes_check_relevance().
     *
     * Note that these items have to be conditionalized because mgmtd
     * will complain about subtrees it doesn't recognize.
     */
    err = mdm_set_interest_subtrees(module, "/net/interface",
#ifdef PROD_FEATURE_BRIDGING
				    "/net/bridge",
#endif
#ifdef PROD_FEATURE_BONDING
				    "/net/bonding",
#endif
				    "/net/general",
				    "/net/routes");
    bail_error(err);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = mdm_module_set_gettext_domain(module, GETTEXT_DOMAIN);
    bail_error(err);
#endif				/* PROD_FEATURE_I18N_SUPPORT */

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);
    opts.tao_dup_policy = adp_delete_new;
    err = tstr_array_new(&md_mgmtd_routes_ifnames_changed, &opts);
    bail_error(err);

    err = md_events_handle_int_interest_add
	("/net/general/events/acting_overlay_ifname_changed",
	 md_mgmtd_routes_handle_acting_overlay_ifname_change, NULL);
    bail_error(err);

    err = md_events_handle_int_interest_add
	("/net/interface/events/state_change",
	 md_mgmtd_routes_handle_ifstate_change, NULL);
    bail_error(err);

    err = md_events_handle_int_interest_add
	("/net/interface/events/link_up",
	 md_mgmtd_routes_handle_ifstate_change, NULL);
    bail_error(err);

    err = md_events_handle_int_interest_add
	("/net/interface/events/link_down",
	 md_mgmtd_routes_handle_ifstate_change, NULL);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/net/routes";
    node->mrn_node_reg_flags = mrf_acl_inheritable;
    mdm_node_acl_add(node, tacl_sbm_net);
    node->mrn_description = md_acl_dummy_node_descr;
    err = mdm_override_node_flags_acls(module, &node, 0);
    bail_error(err);

    /* ........................................................................
     * IPv4 route config
     */

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/net/routes/config/ipv4/prefix/*";
    node->mrn_value_type = bt_ipv4prefix;
    node->mrn_node_reg_flags = mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask = mcf_cap_node_restricted;
    node->mrn_description = "List of IPv4 prefixes for which we have "
	"static routes";
    node->mrn_audit_descr = N_("IPv4 static route for $v$");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/net/routes/config/ipv4/prefix/*/enable";
    node->mrn_value_type = bt_bool;
    node->mrn_initial_value = "true";
    node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
    node->mrn_cap_mask = mcf_cap_node_restricted;
    node->mrn_description = "Is this route currently active?";
    node->mrn_audit_flags = mnaf_enabled_flag;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/net/routes/config/ipv4/prefix/*/metric";
    node->mrn_value_type = bt_uint32;
    node->mrn_initial_value = md_mgmtd_routes_default_metric;
    node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
    node->mrn_cap_mask = mcf_cap_node_restricted;
    node->mrn_description = "Metric for this route";
    node->mrn_audit_descr = N_("metric");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/net/routes/config/ipv4/prefix/*/nh/*";
    node->mrn_value_type = bt_uint8;
    node->mrn_node_reg_flags = mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask = mcf_cap_node_restricted;
    node->mrn_description = "Ordered list of next hops from this IP "
	"prefix";
    node->mrn_audit_descr = N_("IPv4 static route for $6$: hop index $v$");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/net/routes/config/ipv4/prefix/*/nh/*/type";
    node->mrn_value_type = bt_string;
    node->mrn_initial_value = "gw";
    node->mrn_limit_str_choices_str = ",gw,interface,unicast,reject";
    node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
    node->mrn_cap_mask = mcf_cap_node_restricted;
    node->mrn_description = "Type of next hop: gw or reject";
    node->mrn_audit_descr = N_("route type");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/net/routes/config/ipv4/prefix/*/nh/*/gw";
    node->mrn_value_type = bt_ipv4addr;
    node->mrn_initial_value = "0.0.0.0";
    node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
    node->mrn_cap_mask = mcf_cap_node_restricted;
    node->mrn_description = "IP address of next hop";
    node->mrn_audit_descr = N_("next hop");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/net/routes/config/ipv4/prefix/*/nh/*/interface";
    node->mrn_value_type = bt_string;
    node->mrn_initial_value = "";
    node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
    node->mrn_cap_mask = mcf_cap_node_restricted;
    node->mrn_description = "Interface of next hop";
    node->mrn_audit_descr = N_("interface");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/net/routes/config/ipv4/prefix/*/nh/*/source";
    node->mrn_value_type = bt_ipv4addr;
    node->mrn_initial_value = "0.0.0.0";
    node->mrn_node_reg_flags = mrf_flags_reg_config_literal;
    node->mrn_cap_mask = mcf_cap_node_restricted;
    node->mrn_description =
	"Source IP address to use when route is selected";
    node->mrn_audit_descr = N_("source");
    err = mdm_add_node(module, &node);
    bail_error(err);



    /* ........................................................................
     * Actions
     */

    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name = "/nkn/net/routes/actions/overlay/add/ipv4/default";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    node->mrn_cap_mask = mcf_cap_action_privileged;
    node->mrn_action_func = md_mgmtd_routes_overlay_add;
    node->mrn_action_audit_descr = N_("add default route overlay");
    node->mrn_actions[0]->mra_param_name = "ifname";
    node->mrn_actions[0]->mra_param_audit_descr =
	N_("source interface name");
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[0]->mra_param_default_value_node =
	md_mgmtd_routes_acting_overlay_ifname;
    node->mrn_actions[1]->mra_param_name = "gw";
    node->mrn_actions[1]->mra_param_audit_descr = N_("gateway address");
    node->mrn_actions[1]->mra_param_required = true;
    node->mrn_actions[1]->mra_param_type = bt_ipv4addr;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name = "/nkn/net/routes/actions/overlay/clear";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    node->mrn_cap_mask = mcf_cap_action_privileged;
    node->mrn_action_audit_descr = N_("clear default route overlay");
    node->mrn_action_func = md_mgmtd_routes_overlay_clear_action;
    node->mrn_actions[0]->mra_param_name = "ifname";
    node->mrn_actions[0]->mra_param_audit_descr =
	N_("source interface name");
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[0]->mra_param_default_value_node =
	md_mgmtd_routes_acting_overlay_ifname;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name = "/nkn/net/routes/actions/recheck";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    node->mrn_cap_mask = mcf_cap_action_privileged;
    node->mrn_action_func = md_mgmtd_routes_recheck;
    /*
     * Most of the time, this is used only by the dhclient-script, and
     * we don't need to hear about it.  But if a user did this
     * manually for some reason, we might as well log it.
     */
    node->mrn_action_audit_flags |=
	maaf_suppress_logging_if_non_interactive;
    node->mrn_action_audit_descr = N_("re-apply all routes");
    err = mdm_add_node(module, &node);
    bail_error(err);


    /* ------------------------------------------------------------------------
     * Upgrade registration
     */

    err = md_upgrade_rule_array_new(&md_mgmtd_routes_upgrade_rules);
    bail_error(err);
    ra = md_mgmtd_routes_upgrade_rules;

  bail:
    return (err);
}


/* ------------------------------------------------------------------------- */
int md_mgmtd_routes_deinit(const lc_dso_info * info, void *data)
{
    int err = 0;

    md_mgmtd_routes_if_overlay_array_free(&md_mgmtd_routes_if_overlays);
    tstr_array_free(&md_mgmtd_routes_ifnames_changed);
    md_upgrade_rule_array_free(&md_mgmtd_routes_upgrade_rules);

  bail:
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_commit_check(md_commit * commit,
			     const mdb_db * old_db,
			     const mdb_db * new_db,
			     mdb_db_change_array * change_list, void *arg)
{
    int err = 0;
    uint32 i = 0, num_changes = 0;
    tstring *ifname = NULL;
    mdb_db_change *change = NULL;
    tbool intf_ok = false;
    bn_attrib *attrib = NULL;

    num_changes = mdb_db_change_array_length_quick(change_list);
    for (i = 0; i < num_changes; ++i) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if (!ts_has_prefix_str
	    (change->mdc_name, "/nkn/net/routes/", false)) {
	    continue;
	}

	if (change->mdc_new_attribs == NULL) {
	    continue;
	}

	err =
	    bn_attrib_array_get(change->mdc_new_attribs, ba_value,
				&attrib);
	bail_error(err);

	if (attrib == NULL) {
	    continue;
	}
	if (bn_binding_name_parts_pattern_match_va
	    (change->mdc_name_parts, 0, 10,
	     "nkn", "net", "routes", "config", "*", "prefix", "*", "nh", "*", "interface")) {
	    if (change->mdc_change_type != mdct_delete) {
		err = ts_free(&ifname);
		bail_error(err);

		err = bn_attrib_get_tstr(attrib, NULL, bt_string, NULL,
					 &ifname);
		bail_error(err);

		if (ifname && ts_length(ifname) > 0) {
		    err = lc_ifname_validate(ts_str(ifname), &intf_ok);
		    bail_error(err);
		    if (!intf_ok) {
			err = md_commit_set_return_status_msg_fmt
			    (commit, 1,
			     _("The interface name %s is not valid."),
			     ts_str(ifname));
			bail_error(err);
			goto bail;
		    }
		}
	    }
	}
    }

  bail:
    ts_free(&ifname);
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_reassert_state(md_commit * commit, const mdb_db * db,
			       const char *reason)
{
    int err = 0;
    md_mgmt_route_rec_array *rwant = NULL;
    md_mgmt_route_rec_array *rhave = NULL;

    const md_mgmt_route_rec *route = NULL;
    int i = 0, num_rh = 0;

    lc_log_basic(LOG_INFO, "Routing: reassert state: %s", reason);

    lc_log_basic(MD_ROUTES_LOG_LEVEL, "Routing: getting list of routes we "
		 "have...");

    err = md_mgmt_rts_read_route_info(true, true, &rhave);
    bail_error(err);

    num_rh = md_mgmt_route_rec_array_length_quick(rhave);
    for (i = 0; i < num_rh; i++) {

	route = md_mgmt_route_rec_array_get_quick(rhave, i);
	bail_null(route);
	err =
	    md_mgmtd_routes_dump_rec(MD_ROUTES_LOG_LEVEL, "Routing have: ",
				     route);
	bail_error(err);
    }

    /*
     * We pass in 'rhave' here because when determining which
     * interfaces are eligible to be devices on a route, we have to
     * consider what interfaces are already devices on existing
     * routes.  Normally this should not add any new info, but see
     * bug 12453: sometimes in odd cases, an interface which appeared
     * to be link down was actually in the routing table -- and
     * stubbornly denying that it is a valid interface only causes
     * problems.
     */
    lc_log_basic(MD_ROUTES_LOG_LEVEL, "Routing: getting list of routes we "
		 "want...");
    err = md_mgmtd_routes_calc_wanted(commit, db, rhave, &rwant);
    bail_error(err);

    lc_log_basic(MD_ROUTES_LOG_LEVEL,
		 "Routing: applying diffs of want vs. " "have...");
    err = md_mgmtd_routes_apply_diffs(commit, &rwant, &rhave);
    bail_error(err);

  bail:
    md_mgmt_route_rec_array_free(&rwant);	/* Only needed in failure case */
    md_mgmt_route_rec_array_free(&rhave);	/* Only needed in failure case */
    return (err);
}


/* ------------------------------------------------------------------------- */
#if 0
static int
md_mgmtd_routes_delete_routes(md_commit * commit, const mdb_db * db,
			      const char *reason,
			      const md_routes_delete_criteria * crit);
static int
md_mgmtd_routes_delete_routes(md_commit * commit, const mdb_db * db,
			      const char *reason,
			      const md_routes_delete_criteria * crit)
{
    int err = 0;
    md_mgmt_route_rec_array *rhave = NULL;
    int num_routes = 0, i = 0;
    const md_mgmt_route_rec *route = NULL;

    bail_null(db);
    bail_null(crit);

    lc_log_basic(LOG_INFO, "Routing: deleting routes: %s", reason);

    err = md_rts_read_route_info(true, true, &rhave);
    bail_error(err);

    num_routes = md_mgmt_route_rec_array_length_quick(rhave);
    for (i = 0; i < num_routes; ++i) {
	route = md_mgmt_route_rec_array_get_quick(rhave, i);
	bail_null(route);
	if (crit->mrdc_ifname &&
	    safe_strcmp(crit->mrdc_ifname, route->mrr_ifname)) {
	    continue;
	}
	if (crit->mrdc_table > 0 && crit->mrdc_table != route->mrr_table) {
	    continue;
	}
	if (crit->mrdc_protocol > 0 &&
	    crit->mrdc_protocol != route->mrr_protocol) {
	    continue;
	}
	if ((route->mrr_flags | crit->mrdc_rflags_set) != route->mrr_flags) {
	    continue;
	}
	if ((route->mrr_flags & ~(crit->mrdc_rflags_clear)) !=
	    route->mrr_flags) {
	    continue;
	}

	/*
	 * The route passed all of our criteria, so we'd better delete it.
	 */
	err = md_mgmtd_routes_dump_rec(LOG_INFO,	/* Could be MD_ROUTES_LOG_LEVEL */
				       "Routing: deleting route", route);
	bail_error(err);
	err = md_mgmtd_routes_change_route_rec(commit, false, route);
	bail_error(err);
    }

  bail:
    md_mgmt_route_rec_array_free(&rhave);
    return (err);
}
#endif


/* ------------------------------------------------------------------------- */
/* The acting overlay interface name has changed.  Remove any overlays
 * associated with the old one, and add any associated with the new one.
 */
static int
md_mgmtd_routes_handle_acting_overlay_ifname_change(md_commit * commit,
						    const mdb_db * db,
						    const char *event_name,
						    const bn_binding_array
						    * bindings,
						    void *cb_reg_arg,
						    void *cb_arg)
{
    int err = 0;
    tstring *old_ifname = NULL, *new_ifname = NULL;
    char reason[256];

    err =
	bn_binding_array_get_value_tstr_by_name(bindings, "old_ifname",
						NULL, &old_ifname);
    bail_error(err);
    err =
	bn_binding_array_get_value_tstr_by_name(bindings, "new_ifname",
						NULL, &new_ifname);
    bail_error(err);

    snprintf(reason, sizeof(reason), "Acting primary changed from "
	     "'%s' to '%s'", ts_str_maybe_empty(old_ifname),
	     ts_str_maybe_empty(new_ifname));

    err = md_mgmtd_routes_reassert_state(commit, db, reason);
    bail_error(err);

  bail:
    ts_free(&old_ifname);
    ts_free(&new_ifname);
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_commit_apply(md_commit * commit,
			     const mdb_db * old_db, const mdb_db * new_db,
			     mdb_db_change_array * change_list, void *arg)
{
    int err = 0;
    tbool relevant = false;

    /*
     * First find out if anything we are interested in has changed.
     * This is any routing configuration, or any interface
     * configuration, or any of a few other possibilities.
     */
    err = mdb_db_change_array_foreach
	(change_list, md_mgmtd_routes_check_relevance, &relevant);
    bail_error(err);

    if (!relevant) {
	goto bail;
    }

    err =
	md_mgmtd_routes_reassert_state(commit, new_db,
				       "Routing commit apply");
    bail_error(err);

  bail:
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_check_relevance(const mdb_db_change_array * arr,
				uint32 idx, mdb_db_change * change,
				void *data)
{
    int err = 0;
    tbool *relevant = data;

    bail_null(relevant);
    bail_null(change);
    bail_null(change->mdc_name_parts);

    if (ts_has_prefix_str(change->mdc_name, "/net/interface/", false) ||
	ts_has_prefix_str(change->mdc_name, "/nkn/net/routes/", false) ||
	ts_has_prefix_str(change->mdc_name, "/net/routes/", false) ||
	ts_has_prefix_str(change->mdc_name, "/net/general/", false)) {
	*relevant = true;
    }

    if ((change->mdc_change_type == mdct_add ||
	 change->mdc_change_type == mdct_delete) &&
	(ts_has_prefix_str(change->mdc_name, "/net/bridge/", false) ||
	 ts_has_prefix_str(change->mdc_name, "/net/bonding/", false))) {
	*relevant = true;
    }

  bail:
    return (err);
}


/* ------------------------------------------------------------------------- */
/* Run the following command:
 *
 * ip {-4,-6} route <oper> unicast <prefix> proto <proto>
 *                         [via <gateway>] [metric <metric>] [dev <ifname>]
 *
 * The 'proto' parameter is mandatory, and used for both adding and
 * deleting routes, since it is part of the key which distinguishes
 * routes from one another.
 *
 * The 'via', 'metric', and 'dev' parameters are optional, based on
 * the 'gw', 'metric', and 'dev' parameters to the function, respectively.
 *
 * This is similar to the invocation of the 'route' command we used before:
 * route <oper> -net <prefix> netmask <netmask> [gw <gw>] [metric <metric>]
 *              [dev <dev>]
 *
 * The gw, dev and metric parameters are optional.
 */
static int
md_mgmtd_routes_change_route(int family, md_route_oper oper,
			     const char *prefix, const bn_inetaddr * gw,
			     const char *gw_str, const bn_inetaddr * src,
			     const char *src_str, const char *metric,
			     const char *dev, int protocol,
			     tstring ** ret_errors)
{
    int err = 0;
    const char *family_str = NULL;
    const char *oper_str = NULL;
    lc_launch_params *lp = NULL;
    lc_launch_result lr = LC_LAUNCH_RESULT_INIT;
    int offset = 0;
    tbool link_local = false;

    bail_null(prefix);

    /*
     * Partial fix / workaround for bug 14213: don't apply the route if
     * the gateway is link-local and we don't have a device name.
     */
    if (oper == mro_add && !lia_inetaddr_is_zero_quick(gw) &&
	(dev == NULL || strlen(dev) == 0)) {
	err = lia_inetaddr_is_linklocal(gw, &link_local);
	bail_error(err);
	if (link_local) {
	    lc_log_basic(MD_ROUTES_LOG_LEVEL,
			 "Not applying route because gateway "
			 "is link-local, and no device specified "
			 "(prefix %s, gw %s)", prefix, gw_str);

	    /*
	     * XXXX/EMT: we are not reporting the error in the return
	     * message here.  The logs will say the route was dropped,
	     * but those will probably be lost in a sea of other logs,
	     * and the user will be unaware.  The alternative (short of
	     * really fixing bug 14213) is to return a message, which
	     * would get returned every time route state was reasserted.
	     * There, at least, they would be aware that something went
	     * wrong, and could take corrective action.  But this was
	     * judged to be too verbose.
	     */

	    goto bail;
	}
    }

    oper_str = lc_enum_to_string_quiet_null(md_route_oper_map, oper);
    bail_null(oper_str);

    switch (family) {
    case AF_INET:
	family_str = "-4";
	break;

    case AF_INET6:
	family_str = "-6";
	break;

    default:
	bail_force(lc_err_unexpected_case);
	break;
    }

    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);

    err = ts_new_str(&(lp->lp_path), prog_ip_path);
    bail_error(err);

    err = tstr_array_new_va_str
	(&(lp->lp_argv), NULL, 6,
	 prog_ip_path, family_str, "route", oper_str, "unicast", prefix);
    bail_error(err);

    if (gw_str && strlen(gw_str) > 0) {
	err = tstr_array_append_str(lp->lp_argv, "via");
	bail_error(err);
	err = tstr_array_append_str(lp->lp_argv, gw_str);
	bail_error(err);
    }

    if (metric && strlen(metric) > 0) {
	err = tstr_array_append_str(lp->lp_argv, "metric");
	bail_error(err);
	err = tstr_array_append_str(lp->lp_argv, metric);
	bail_error(err);
    }

    if (dev && strlen(dev) > 0) {
	err = tstr_array_append_str(lp->lp_argv, "dev");
	bail_error(err);
	err = tstr_array_append_str(lp->lp_argv, dev);
	bail_error(err);
    }

    /* XXX: JUNIPER ADDED
     * Setup the source address if it is valid.
     */
    if (oper == mro_add && !lia_inetaddr_is_zero_quick(src) &&
	src_str && strlen(src_str) > 0) {
	err = tstr_array_append_str(lp->lp_argv, "src");
	bail_error(err);
	err = tstr_array_append_str(lp->lp_argv, src_str);
	bail_error(err);
    }

    if (protocol < 0) {
	lc_log_basic(LOG_WARNING,
		     "Protocol not set!  Defaulting to 'static'");
	protocol = RTPROT_STATIC;
    }

    err = tstr_array_append_str(lp->lp_argv, "proto");
    bail_error(err);
    err = tstr_array_append_sprintf(lp->lp_argv, "%d", protocol);
    bail_error(err);

    lp->lp_wait = true;
    lp->lp_log_level = MD_ROUTES_LOG_LEVEL;
    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;

    if (lc_devel_is_production()) {
	/* XXX #dep/args: ip */
	err = lc_launch(lp, &lr);
	bail_error(err);

	/*
	 * XXX/EMT: should exitcode_loglevel be LOG_WARNING if we have
	 * filtering enabled?
	 */
	err = lc_check_exit_status_full(lr.lr_exit_status, NULL,
					MD_ROUTES_LOG_LEVEL,
					LOG_INFO, LOG_ERR, "ip route");
	bail_error(err);
    } else {
	lc_log_debug(LOG_INFO, "Devenv: no apply for routes");
	goto bail;
    }

    /*
     * Do not stifle any specific errors we get back from trying to
     * apply these changes, but do log the full results at INFO.
     *
     * Back when we were doing commit_apply based on the change list,
     * and when we used the "route" command, we suppressed two particular
     * error messages which were likely to come up in normal cases:
     *   "SIOCADDRT: File exists"
     *   "SIOCADDRT: Network is unreachable"
     *
     * Now that we have switched to using the "ip" command, these two
     * strings are no longer the ones you get.  Instead, you get:
     *   "RTNETLINK answers: File exists"
     *   "RTNETLINK answers: Network is unreachable"   (for IPv4)
     *   "RTNETLINK answers: Invalid argument"         (for IPv6)
     *
     * However, we don't want to search for these strings anymore.
     * For one thing, that last string is so vague we wouldn't want
     * to presume we know what it means.  Also, we have switched
     * our approach, and now ignore the change list, and just apply
     * the diffs between what we want and what we have.  Because of
     * our extensive calculations, we should NEVER get ANY errors,
     * and anything we have gotten should be reported to the user.
     *
     * The exception is if the user has disabled filtering of routes.
     * Then we will always try to apply all the diffs, even routes
     * which we expect to fail.  In that case, we want to suppress
     * all errors.  This suppression is done by our caller; we always
     * report the errors.
     *
     * For debugging purposes, we always log whatever result we got
     * at INFO.
     */

    if (lr.lr_captured_output && ts_num_chars(lr.lr_captured_output) > 2) {
	lc_log_basic(LOG_INFO, "Output from 'ip route': %s",
		     ts_str(lr.lr_captured_output));
	if (ret_errors) {
	    *ret_errors = lr.lr_captured_output;
	    lr.lr_captured_output = NULL;
	}
    }

  bail:
    lc_launch_params_free(&lp);
    lc_launch_result_free_contents(&lr);
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_overlay_add(md_commit * commit, const mdb_db * db,
			    const char *action_name,
			    bn_binding_array * params, void *cb_arg)
{
    int err = 0;
    tstring *gw = NULL, *ifname = NULL;
    tstr_array *overlays = NULL;
    char reason[256];

    err = bn_binding_array_get_value_tstr_by_name
	(params, "ifname", NULL, &ifname);
    bail_error_null(err, ifname);

    if (!ts_nonempty(ifname)) {
	lc_log_basic(LOG_NOTICE,
		     _("Action %s rejected due to zero length ifname."),
		     action_name);
	err = md_commit_set_return_status_msg(commit, 1,
					      _
					      ("Binding 'ifname' may not be an empty string-- to request the "
					       "default interface, omit this binding from the request"));
	bail_error(err);
	goto bail;
    }

    /* ........................................................................
     * Store the new overlay in our library of overlay info.
     */

    err =
	md_mgmtd_routes_get_extras(commit, db, ts_str_maybe(ifname),
				   &overlays);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name(params, "gw", NULL, &gw);
    bail_error_null(err, gw);

    err = tstr_array_append(overlays, gw);
    bail_error(err);

    /* ........................................................................
     * Re-apply the desired routes to the system, in case this new one
     * changes anything.
     *
     * It's possible that simply getting an overlay could make this
     * interface the acting primary, where it was not before.  That's
     * because an interface is not eligible to be primary until it has
     * gotten an overlay.  If that happens, it will get applied when
     * we send our event, below, instead of right now.
     */
    snprintf(reason, sizeof(reason),
	     "Route overlay '%s' added for interface " "'%s'", ts_str(gw),
	     ts_str(ifname));
    err = md_mgmtd_routes_reassert_state(commit, db, reason);
    bail_error(err);

    /*
     * We no longer send the /net/general/events/overlay_applied event
     * (as it no longer exists).  It's our caller's responsibility to
     * call the /net/general/actions/set_overlay_valid action when
     * all the overlays are applied.
     *
     * When they do call that action, it may affect md_net_general's
     * determination of acting primary overlay interface.  If this
     * changes anything at all, it will make this interface the acting
     * primary.  Then the route, which was not applied above because
     * we were not the acting primary yet, will be applied by our
     * acting_overlay_ifname_changed event handler.
     */

  bail:
    ts_free(&gw);
    ts_free(&ifname);
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_overlay_clear_action(md_commit * commit, const mdb_db * db,
				     const char *action_name,
				     bn_binding_array * params,
				     void *cb_arg)
{
    int err = 0;
    tstring *ifname = NULL;

    err = bn_binding_array_get_value_tstr_by_name(params, "ifname", NULL,
						  &ifname);
    bail_error(err);

    if (ts_length(ifname) == 0) {
	lc_log_basic(LOG_NOTICE,
		     _("Action %s rejected due to zero length ifname."),
		     action_name);
	err = md_commit_set_return_status_msg(commit, 1,
					      _
					      ("Binding 'ifname' may not be an empty string-- to request the "
					       "default interface, omit this binding from the request"));
	bail_error(err);
	goto bail;
    }

    err = md_mgmtd_routes_overlay_clear(commit, db, ts_str(ifname));
    bail_error(err);

  bail:
    ts_free(&ifname);
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_overlay_clear(md_commit * commit, const mdb_db * db,
			      const char *ifname)
{
    int err = 0;
    tstr_array *overlays = NULL;
    char reason[256];

    err = md_mgmtd_routes_get_extras(commit, db, ifname, &overlays);
    bail_error(err);

    err = tstr_array_empty(overlays);
    bail_error(err);

    snprintf(reason, sizeof(reason), "Route overlay cleared for interface "
	     "'%s'", ifname);

    err = md_mgmtd_routes_reassert_state(commit, db, reason);
    bail_error(err);

  bail:
    return (err);
}


/* ------------------------------------------------------------------------- */
static int
md_mgmtd_routes_recheck(md_commit * commit, const mdb_db * db,
			const char *action_name,
			bn_binding_array * params, void *cb_arg)
{
    int err = 0;

    err =
	md_mgmtd_routes_reassert_state(commit, db,
				       "Routes recheck action");
    bail_error(err);

  bail:
    return (err);
}
