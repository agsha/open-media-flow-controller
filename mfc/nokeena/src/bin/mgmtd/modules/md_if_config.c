/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */

static const char rcsid[] = "$Id: md_if_config.c,v 1.345 2012/10/03 22:36:16 et Exp $";


/* For interface definitions, like 'struct ifreq' */
#undef __STRICT_ANSI__

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "common.h"
#include "dso.h"
#include "file_utils.h"
#include "proc_utils.h"
#include "md_utils.h"
#include "md_utils_internal.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "mdb_db.h"
#include "mdb_dbbe.h"
#include "mdmod_common.h"
#include "mdm_events.h"
#include "tacl.h"
#include "md_client.h"
#include "md_upgrade.h"
#include "array.h"
#include "tpaths.h"
#include "md_main.h" /* XXX/EMT: for md_lwc */
#include "addr_utils.h"
#include "type_conv.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#ifdef PROD_TARGET_OS_LINUX
#include <linux/types.h>
#include <linux/if.h>
#include <linux/if_addr.h>
#include <net/if_arp.h>
#include <net/route.h>

#undef __STRICT_ANSI__ /* rtnetlink.h fails on i386 without this */
#include <linux/rtnetlink.h>

#endif

/*****************************************************************************/
/*****************************************************************************/
/*** GLOBAL VARIABLES                                                      ***/
/*****************************************************************************/
/*****************************************************************************/

static md_upgrade_rule_array *md_if_upgrade_rules = NULL;

md_system_override_clear_ext_type md_system_override_clear_ext_func = NULL;
md_resolver_overlay_clear_ext_type md_resolver_overlay_clear_ext_func = NULL;
md_routes_overlay_clear_type md_routes_overlay_clear_func = NULL;
md_routes_delete_routes_type md_routes_delete_routes_func = NULL;

int md_ifc_sock = -1;
tbool md_ifc_seen_ethtool_notsupp = false;

TYPED_ARRAY_HEADER_NEW_NONE(md_ifc_dhcp_rec, struct md_ifc_dhcp_rec *);
TYPED_ARRAY_IMPL_NEW_NONE(md_ifc_dhcp_rec, md_ifc_dhcp_rec *, NULL);

md_ifc_dhcp_rec_array *md_ifc_dhcp_recs = NULL;

tstr_array *md_ifc_vmfc_ifnames = NULL;
tstr_array *md_ifc_vmfc_ifspeed = NULL;

/* Keep track of client pids, and related state, for zeroconf */
tstr_array *md_ifc_zeroconf_names = NULL;
uint32_array *md_ifc_zeroconf_pids = NULL;
tstr_array *md_ifc_zeroconf_ifs_retry = NULL;

static const char md_ifc_lo_mtu[] = "16436";
static const char md_ifc_ethernet_mtu[] = "1500";
static lt_dur_ms md_ifc_dhcp_kill_time_ms = 2000;
static lt_dur_ms md_ifc_zeroconf_kill_time_ms = 2000;
static lt_dur_ms md_ifc_zeroconf_retry_interval_ms = 5000;

/*
 * This is how long we'll let a DHCP release run before killing it.
 * The other timeouts are just how patient we are when killing a 
 * process, but this one is how patient we are when waiting for a 
 * server response.  The "short" one is how long we wait if we're
 * blocked on it, and the "long" one is how long we wait if we're
 * not blocked, but still don't want to let it run forever.  In the
 * short case, we're not all that patient.  We use 1500 instead of 
 * 1000 because the dhclient seems to retransmit its release request
 * after just around 1000 ms, so we want to try to give it a chance to
 * do that at least once.
 */
static lt_dur_ms md_ifc_dhcp_release_timeout_short_ms = 1500;
static lt_dur_ms md_ifc_dhcp_release_timeout_long_ms = 9000;

/*
 * XXX/EMT: this should be interface type-specific.
 * This used to be set to 16110, based off maximums for GigE,
 * but that was causing some issues in some cases. We make the
 * value 1500 which should be correct 99% of the time and if not
 * Graft point 1 below should be used to disable the stock check
 * MTU code (by setting 'check_mtu' to false) and insert whatever
 * logic is required at that point or by setting
 * 'check_max_mtu' to some other value. 
 */
static uint16 md_ifc_ether_min_mtu = 68;
static uint16 md_ifc_ether_max_mtu = 1500;

static uint16 md_ifc_infiniband_min_mtu = 252;
static uint16 md_ifc_infiniband_max_mtu = 65535;

/* The kernel will not run IPv6 below an MTU of 1280 */
static uint16 md_ifc_ipv6_min_mtu = 1280;

/* 
 * For a period of time after adding a new alias (via an action) we will
 * send out gratuitous arp request/replys at the reched interval.  This
 * is to help clear out stale ARP entries in cases where there have been
 * network or interface state transitions.  It is also to avoid trouble
 * with bonding convergence causing ARP packet loss.
 *
 */
static const lt_dur_ms md_ifc_gratarp_resched_interval_ms = 15 * 1000;
static const int32 md_ifc_gratarp_resched_count = 3;
static lew_event *md_ifc_gratarp_resched_timer = NULL;
static lew_event *md_ifc_zeroconf_launch_timer = NULL;

/* List of interface/addr/masklen/tag tupes of alias addresses to be assigned */
tstr_array *md_interface_aliases = NULL;
tstr_array *md_interface_ipv6_aliases = NULL;
uint32_array *md_interface_aliases_gratarp_count = NULL;

/* List of names of interfaces which currently have overrides active */
tstr_array *md_interface_overrides = NULL;

/*
 * An IPv6 address we can try to add to an interface if it does not have
 * IPv6 sysctl settings, to kick the interface into having IPv6 sysctl
 * settings.  This address is multicast, and so will fail to be added on
 * modern kernels.  For older kernels, we'll just delete it right off,
 * and it should not have any negative impact.  In any case, we'll try
 * to add this with scope "host" , so as to limit exposure.
 */
static const char *md_interface_bad_ipv6_addr = "ff11::ffff:ffff";

tbool md_if_global_ipv6_enable = -1;

/*****************************************************************************/
/*****************************************************************************/
/*** PROTOTYPES                                                            ***/
/*****************************************************************************/
/*****************************************************************************/

int md_if_config_init(const lc_dso_info *info, void *data);

int md_if_config_deinit(const lc_dso_info *info, void *data);

static int md_interface_register_config_nodes(md_module *new_mod);

static int md_interface_register_action_nodes(md_module *new_mod);

static int md_interface_register_event_nodes(md_module *new_mod);

static int md_interface_add_initial(md_commit *commit, mdb_db *db, void *arg);

static int md_interface_upgrade_downgrade(const mdb_db *old_db,
                                          mdb_db *inout_new_db,
                                          uint32 from_module_version,
                                          uint32 to_module_version, void *arg);

static int md_interface_commit_check(md_commit *commit,
                                     const mdb_db *old_db, 
                                     const mdb_db *new_db, 
                                     mdb_db_change_array *change_list,
                                     void *arg);

typedef struct md_if_config_check_change_context {
    md_commit *micc_commit;
    const mdb_db *micc_db;
} md_if_config_check_change_context;

static int md_interface_check_change(const mdb_db_change_array *arr,
                                     uint32 idx, mdb_db_change *change,
                                     void *data);

static int md_interface_commit_apply(md_commit *commit,
                                     const mdb_db *olddb, 
                                     const mdb_db *newdb, 
                                     mdb_db_change_array *change_list,
                                     void *arg);

static int md_interface_handle_change(const mdb_db_change_array *arr,
                                      uint32 idx, mdb_db_change *change,
                                      void *data);

static int md_interface_validate_ipv4addr_masklen(md_commit *commit,
                                                  const char *intf,
                                                  bn_ipv4addr ip,
                                                  uint8 masklen,
                                                  tbool *ret_is_valid);

static int md_interface_validate_ipv6addr_masklen(md_commit *commit,
                                                  const char *intf,
                                                  bn_ipv6addr ip,
                                                  uint8 masklen,
                                                  tbool *ret_is_valid);

static int md_interface_override_handle_set(md_commit *commit,
                                            const mdb_db *db,
                                            const char *action_name,
                                            bn_binding_array *params,
                                            void *cb_arg);

static int md_interface_override_handle_clear(md_commit *commit,
                                              const mdb_db *db,
                                              const char *action_name,
                                              bn_binding_array *params,
                                              void *cb_arg);

static int md_interface_alias_handle_add(md_commit *commit,
                                         const mdb_db *db,
                                         const char *action_name,
                                         bn_binding_array *params,
                                         void *cb_arg);

static int md_interface_alias_handle_delete(md_commit *commit,
                                            const mdb_db *db,
                                            const char *action_name,
                                            bn_binding_array *params,
                                            void *cb_arg);

static int md_interface_alias_clear(md_commit *commit, const mdb_db *db,
                                    const char *ifname, const char *tag);

static int md_interface_remove_aliases(const char *ifname);

static int md_interface_alias_handle_clear(md_commit *commit,
                                           const mdb_db *db,
                                           const char *action_name,
                                           bn_binding_array *params,
                                           void *cb_arg);

static int md_interface_search_aliases(const char *ifname, const char *ip,
                                       uint8 masklen,
                                       const char *tag,
                                       int32 *ret_found_index);

static int md_interface_search_ipv6_aliases(const char *ifname,
                                            bn_ipv6addr *addr,
                                            uint8 masklen, const char *tag,
                                            int32 *ret_found_index);

static int md_interface_renew_dhcpv4(md_commit *commit, const mdb_db *db,
                                     const char *action_name,
                                     bn_binding_array *params, void *cb_arg);

static int md_interface_renew_dhcpv6(md_commit *commit, const mdb_db *db,
                                     const char *action_name,
                                     bn_binding_array *params, void *cb_arg);

static int md_interface_get_config(const char *binding_name, 
                                   const tstr_array *binding_name_parts,
                                   md_commit *commit, const mdb_db *db,
                                   tbool *ret_found, char **ret_ifname,
                                   tbool *ret_enabled,
                                   tbool *ret_overridden,
                                   bn_ipv4addr *ret_addr,
                                   uint8 *ret_masklen,
                                   tbool *ret_dhcp_enabled,
                                   tbool *ret_zeroconf_enabled,
                                   tbool *ret_bridge_member,
                                   tbool *ret_bond_member,
                                   tbool *ret_isalias,
                                   tbool *ret_dhcpv6_enabled);

static int md_interface_get_config_by_ifname(const char *if_name,
                                             md_commit *commit,
                                             const mdb_db *db,
                                             tbool *ret_found,
                                             tbool *ret_enabled,
                                             tbool *ret_overridden,
                                             bn_ipv4addr *ret_addr,
                                             uint8 *ret_masklen,
                                             tbool *ret_dhcp_enabled,
                                             tbool *ret_zeroconf_enabled,
                                             tbool *ret_bridge_member,
                                             tbool *ret_bond_member,
                                             tbool *ret_isalias,
                                             tbool *ret_dhcpv6_enabled);

int md_ifc_side_effect_get_names(const tstr_array *old_name_parts,
                                 tbool is_duplex, 
                                 char **ret_ifname, char **ret_other_name);

int md_ifc_side_effect_unset_auto(md_commit *commit, mdb_db *db,
                                  const char *ifname, tbool is_duplex,
                                  const char *other_name);

static int md_interface_override_set(const char *ifname, tbool override);

/*
 * Call md_interface_set_ipv4_addr() only if it has not yet been called
 * during this commit (i.e. if it isn't in the list of strings
 * provided).
 */
static int md_interface_maybe_set_ipv4_addr(md_commit *commit,
                                            const mdb_db *db,
                                            tstr_array *addrs_already_set,
                                            const char *ifname,
                                            bn_ipv4addr addr, uint8 masklen,
                                            tbool is_alias, tbool enslaved);

static int md_interface_set_link_internal(md_commit *commit,
                                          const mdb_db *db,
                                          tbool no_prep,
                                          const char *ifname,
                                          tbool up);

/*
 * Bring up an interface and set its IP address and netmask.
 */
static int md_interface_set_ipv4_addr(md_commit *commit, const mdb_db *db,
                                      const char *ifname,
                                      bn_ipv4addr addr, uint8 masklen,
                                      tbool is_alias, tbool enslaved);

static int md_interface_addr_ipv6_add(md_commit *commit, const mdb_db *db,
                                      const char *ifname,
                                      const bn_ipv6addr *addr,
                                      uint8 masklen);


static int md_interface_addr_ipv6_remove(md_commit *commit, const mdb_db *db,
                                         const char *ifname,
                                         const bn_ipv6addr *addr,
                                         uint8 masklen);

static int md_interface_bring_down_full(md_commit *commit, const mdb_db *db,
                                        const char *ifname,
                                        tbool skip_down, tbool is_alias,
                                        const char *addr);

static int md_interface_bring_down_alias(md_commit *commit, const mdb_db *db,
                                         const char *ifname,
                                         const char *addr);

/*
 * Send a type of unsolicited ARP messsage
 */
static int md_interface_grat_arp(md_commit *commit, const mdb_db *db, 
                                 const char *ifname, const char *addr,
                                 tbool arp_reply);

int md_ifc_kill_all_dhclients(void);

int md_ifc_kill_all_zeroconfs(void);

/*
 * Make sure a DHCP client daemon is running iff it should be, killing
 * it or launching it as appropriate.  If 'renew' is set, we kill it
 * even if it's supposed to be running, and then restart it.
 * If 'release_lease' is set, this means that if we are killing the 
 * DHCP client, we should release the lease file after it exits.
 */
static int md_interface_check_dhcp(const char *if_name, 
                                   md_commit *commit, const mdb_db *db,
                                   tbool ipv6,
                                   tbool dhcp_enabled, tbool if_enabled,
                                   tbool renew, tbool release_lease);

static int md_interface_check_zeroconf(const char *if_name, 
                                   md_commit *commit, const mdb_db *db,
                                   tbool zeroconf_enabled, tbool if_enabled,
                                   tbool renew);

/*
 * Check to see if there is a PID file for a DHCP client for the
 * specified interface.  If so, return the PID in it.  Otherwise,
 * return -1 for the PID.
 */
static int md_interface_check_for_dhcp_pid(const char *if_name, tbool ipv6,
                                           int *ret_pid);

static int md_interface_check_for_zeroconf_pid(const char *if_name, 
                                               int *ret_pid);

/*
 * Launch a DHCP client daemon for the specified interface.
 */
static int md_interface_dhcp_launch(md_commit *commit, const mdb_db *db,
                                    const char *if_name, tbool ipv6);

static int md_if_config_linkup(md_commit *commit, const mdb_db *db,
                               const char *event_name,
                               const bn_binding_array *bindings,
                               void *cb_reg_arg, void *cb_arg);

static int md_interface_zeroconf_launch(md_commit *commit, 
                                        const char *if_name);

/*
 * Check to see if any overrides need to be cleared.  We know we need to
 * clear the ones for this interface.  We call this only right after
 * we've turned off and/or killed zeroconf or DHCP.
 */
static int md_interface_maybe_clear_overrides(const char *if_name, tbool ipv6,
                                              md_commit *commit,
                                              const mdb_db *db);

static int md_interface_apply_ipv6_kernel_config(md_commit *commit,
                                                 const mdb_db *db,
                                                 const char *ifname,
                                                 tbool bringing_up_ifname);


/* Send additional gratuitous arps after alias additions */
static int md_ifc_gratarp_resched_handler(int fd, 
                                          short event_type, void *data,
                                          lew_context *ctxt);

/*
 * Make sure that speed and duplex are both either auto, or not auto,
 * since the driver does not allow them to be set indepedently.  If
 * one is switched from not auto to auto, this is easy: just make the
 * same change to the other.  If one is switched from auto to not
 * auto, this is trickier: find out what the running setting of the
 * other one is, and switch it to that.
 */
static int md_interface_commit_side_effects(md_commit *commit, 
                                            const mdb_db *old_db, 
                                            mdb_db *inout_new_db, 
                                            mdb_db_change_array *change_list,
                                            void *arg);

int
lc_is_if_driver_virtio(const char *ifname, int ifc_sock, int *if_virtio);


/* ------------------------------------------------------------------------- */
static int
md_ifc_dhcp_rec_free(md_ifc_dhcp_rec **inout_rec)
{
    int err = 0;
    md_ifc_dhcp_rec *rec = NULL;

    bail_null(inout_rec);
    rec = *inout_rec;
    if (rec) {
        safe_free(rec->midr_ifname);
        if (md_lwc && rec->midr_release_timeout) {
            err = lew_event_delete(md_lwc, &(rec->midr_release_timeout));
            complain_error(err);
            err = 0;
        }
        safe_free(*inout_rec);
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static void
md_ifc_dhcp_rec_free_for_array(void *rec_v)
{
    md_ifc_dhcp_rec *rec = rec_v;
    md_ifc_dhcp_rec_free(&rec);
}


/*****************************************************************************/
/*****************************************************************************/
/*** NODE REGISTRATION                                                     ***/
/*****************************************************************************/
/*****************************************************************************/


/* ------------------------------------------------------------------------- */
int
md_if_config_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *new_mod = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;
    int32 status = -1;
    tstring *output = NULL;
    tbool iai = false;
    array_options opts;

    err = mdm_add_module("interface", 42,
                         "/net/interface", "/net/interface/config",
                         0,
                         md_interface_commit_side_effects, NULL,
                         md_interface_commit_check, NULL, 
                         md_interface_commit_apply, NULL,
                         0, md_if_config_apply_order,
                         md_interface_upgrade_downgrade, &md_if_upgrade_rules,
                         md_interface_add_initial, NULL,
                         NULL, NULL, NULL, NULL, &new_mod);
    bail_error(err);

    /*
     * A comment towards the top of md_interface_reapply_config()
     * implies that we need to be called anytime anything in /net
     * changes.  Not sure if this could be made more specific, or
     * how much performance impact that would have.  Note that we
     * at least need /net/general/config/ipv6/ , and likely
     * some unknown set of other things.
     */
    err = mdm_set_interest_subtrees(new_mod,
                                    "/net");
    bail_error(err);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = mdm_module_set_gettext_domain(new_mod, GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =                  "/net/interface";
    node->mrn_node_reg_flags =        mrf_acl_inheritable;
    mdm_node_acl_add(node,            tacl_sbm_net);
    node->mrn_description =           md_acl_dummy_node_descr;
    err = mdm_override_node_flags_acls(new_mod, &node, 0);
    bail_error(err);

    err = md_interface_register_config_nodes(new_mod);
    bail_error(err);

    err = md_interface_register_action_nodes(new_mod);
    bail_error(err);

    err = md_interface_register_event_nodes(new_mod);
    bail_error(err);

    err = md_upgrade_rule_array_new(&md_if_upgrade_rules);
    bail_error(err);
    ra = md_if_upgrade_rules;

    /*
     * Create MTU bindings.
     *
     * XXX/EMT: for all existing interfaces, we should really query
     * the current actual MTU and use it as the default, since
     * loopback and various interface types may have different default
     * MTUs on different platforms.  But for expediency, hardwire
     * Linux's default for loopback for now, and assume that all other
     * interfaces are ethernet and can be defaulted to 1500.
     */
    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name =             "/net/interface/config/*/mtu";
    rule->mur_new_value_type =   bt_uint16;
    rule->mur_new_value_str =    md_ifc_ethernet_mtu;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name =             "/net/interface/config/lo/mtu";
    rule->mur_new_value_type =   bt_uint16;
    rule->mur_new_value_str =    md_ifc_lo_mtu;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name =             "/net/interface/config/*/enslaved";
    rule->mur_new_value_type =   bt_bool;
    rule->mur_new_value_str =    "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name =             "/net/interface/config/*/display";
    rule->mur_new_value_type =   bt_bool;
    rule->mur_new_value_str =    "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name =             "/net/interface/config/*/alias/ifdevname";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name =             "/net/interface/config/*/alias/index";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name =             "/net/interface/config/*/comment";
    rule->mur_new_value_type =   bt_string;
    rule->mur_new_value_str =    "";
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name =             "/net/interface/config/*/addr/ipv6/enable";
    rule->mur_new_value_type =   bt_bool;
    rule->mur_new_value_str =    "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name =         "/net/interface/config/*/addr/ipv6/slaac/enable";
    rule->mur_new_value_type =   bt_bool;
    rule->mur_new_value_str =    "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name = "/net/interface/config/*/addr/ipv6/slaac/default_route";
    rule->mur_new_value_type =   bt_bool;
    rule->mur_new_value_str =    "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name =     "/net/interface/config/*/addr/ipv6/slaac/privacy";
    rule->mur_new_value_type =   bt_bool;
    rule->mur_new_value_str =    "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name =         "/net/interface/config/*/addr/ipv6/slaac/enable";
    rule->mur_new_value_type =   bt_bool;
    rule->mur_new_value_str =    "false";
    MD_ADD_RULE(ra);

    /*
     * The upgrade steps from 23 through 41 (23-to-24 ... 40-to-41)
     * are empty (see bug 14242).
     */

    MD_NEW_RULE(ra, 41, 42);
    rule->mur_upgrade_type =     MUTT_ADD;
    rule->mur_name =           "/net/interface/config/*/addr/ipv6/dhcp/enable";
    rule->mur_new_value_type =   bt_bool;
    rule->mur_new_value_str =    "false";
    MD_ADD_RULE(ra);

    /*
     * ------------------------------
     * Required setup and initialization
     * ------------------------------
     */
#ifdef PROD_FEATURE_IPV6
    if (!lc_devel_is_production()) {
        lc_log_debug(LOG_INFO, "Devenv: not running modprobe ipv6");
        goto post_probe;
    }

    /* If it's already loaded, no need to do modprobe it */
    err = lf_test_exists(file_if_addr_ipv6, &iai);
    if (err || !iai) {
        err = 0;

        /* XXX #dep/args: modprobe */
        ts_free(&output);
        err = lc_launch_quick_status(&status, &output, true,
                                     2, prog_modprobe_path,
                                     "ipv6");
        complain_error(err);

        if (status != 0) {
            lc_log_basic(LOG_WARNING,
                         "Could not load IPv6 support: %s",
                         ts_str(output));
        }
    }

    err = lf_test_exists(file_if_addr_ipv6, &iai);
    if (err || !iai) {
        err = 0;
        lc_log_basic(LOG_WARNING,
                     "Could not detect or load IPv6 support.");
    }
#endif

 post_probe:

    /* XXX/EMT: we should share with md_ifs_sock */
    md_ifc_sock =  socket(AF_INET, SOCK_DGRAM, 0);
    bail_require(md_ifc_sock >= 0);

    err = tstr_array_new(&md_interface_overrides, NULL);
    bail_error(err);

    err = tstr_array_new(&md_interface_aliases, NULL);
    bail_error(err);

    err = tstr_array_new(&md_interface_ipv6_aliases, NULL);
    bail_error(err);

    err = uint32_array_new(&md_interface_aliases_gratarp_count);
    bail_error(err);

    err = array_options_get_defaults(&opts);
    bail_error(err);
    opts.ao_elem_free_func = md_ifc_dhcp_rec_free_for_array;
    err = md_ifc_dhcp_rec_array_new_full(&md_ifc_dhcp_recs, &opts);
    bail_error_null(err, md_ifc_dhcp_recs);

    err = md_interface_remove_aliases("");
    bail_error(err);

    err = mdm_get_symbol("md_resolver", "md_resolver_overlay_clear_ext",
                         (void **)&md_resolver_overlay_clear_ext_func);
    bail_error_null(err, md_resolver_overlay_clear_ext_func);

    err = mdm_get_symbol("md_system", "md_system_override_clear_ext",
                         (void **)&md_system_override_clear_ext_func);
    bail_error_null(err, md_system_override_clear_ext_func);

    err = mdm_get_symbol("md_routes", "md_routes_overlay_clear",
                         (void **)&md_routes_overlay_clear_func);
    bail_error_null(err, md_routes_overlay_clear_func);

    err = mdm_get_symbol("md_routes", "md_routes_delete_routes",
                         (void **)&md_routes_delete_routes_func);
    bail_error_null(err, md_routes_delete_routes_func);

    err = tstr_array_new(&md_ifc_vmfc_ifnames, NULL);
    bail_error(err);

    err = tstr_array_new(&md_ifc_vmfc_ifspeed, NULL);
    bail_error(err);

    err = tstr_array_new(&md_ifc_zeroconf_names, NULL);
    bail_error(err);

    err = tstr_array_new(&md_ifc_zeroconf_ifs_retry, NULL);
    bail_error(err);

    err = uint32_array_new(&md_ifc_zeroconf_pids);
    bail_error(err);

    err = md_ifc_kill_all_dhclients();
    complain_error(err);

    err = md_ifc_kill_all_zeroconfs();
    complain_error(err);

    err = md_events_handle_int_interest_add("/net/interface/events/link_up",
                                            md_if_config_linkup, NULL);
    bail_error(err);

 bail:
    ts_free(&output);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
md_if_config_deinit(const lc_dso_info *info, void *data)
{
    int err = 0;

    tstr_array_free(&md_interface_overrides);

    err = md_interface_alias_clear(NULL, NULL, NULL, NULL);
    bail_error(err);

    tstr_array_free(&md_interface_aliases);

    tstr_array_free(&md_interface_ipv6_aliases);

    uint32_array_free(&md_interface_aliases_gratarp_count);

    err = md_ifc_kill_all_dhclients();
    complain_error(err);

    err = md_ifc_kill_all_zeroconfs();
    complain_error(err);

    md_ifc_dhcp_rec_array_free(&md_ifc_dhcp_recs);

    tstr_array_free(&md_ifc_vmfc_ifnames);
    tstr_array_free(&md_ifc_vmfc_ifspeed);

    tstr_array_free(&md_ifc_zeroconf_names);
    tstr_array_free(&md_ifc_zeroconf_ifs_retry);
    uint32_array_free(&md_ifc_zeroconf_pids);

    md_upgrade_rule_array_free(&md_if_upgrade_rules);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Get a pointer to a DHCP record for a given interface+family.
 * If such a record does not yet exist, one is created iff create_if_missing
 * is true.
 *
 * NOTE: the pointer returned is non-const so that the caller may modify
 * the record, but the caller DOES NOT OWN the record, and MUST NOT free it.
 */
int
md_ifc_dhcp_rec_lookup(const char *ifname, tbool ipv6,
                       tbool create_if_missing,
                       md_ifc_dhcp_rec **ret_rec)
{
    int err = 0;
    int i = 0, num_recs = 0;
    md_ifc_dhcp_rec *rec = NULL, *new_rec = NULL;

    bail_null(ifname);
    bail_null(ret_rec);
    bail_null(md_ifc_dhcp_recs);

    *ret_rec = NULL;

    num_recs = md_ifc_dhcp_rec_array_length_quick(md_ifc_dhcp_recs);
    for (i = 0; i < num_recs; ++i) {
        rec = md_ifc_dhcp_rec_array_get_quick(md_ifc_dhcp_recs, i);
        bail_null(rec);
        if (!safe_strcmp(ifname, rec->midr_ifname) && rec->midr_ipv6 == ipv6) {
            *ret_rec = rec;
            goto bail;
        }
    }

    if (create_if_missing) {
        new_rec = malloc(sizeof(*new_rec));
        bail_null(new_rec);
        memset(new_rec, 0, sizeof(*new_rec));
        
        new_rec->midr_ifname = strdup(ifname);
        bail_null(new_rec->midr_ifname);
        new_rec->midr_ipv6 = ipv6;
        
        rec = new_rec;
        err = md_ifc_dhcp_rec_array_append_takeover(md_ifc_dhcp_recs,
                                                    &new_rec);
        bail_error(err);
        *ret_rec = rec;
    }

 bail:
    md_ifc_dhcp_rec_free(&new_rec); /* Only in error case */
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_register_config_nodes(md_module *new_mod)
{
    int err = 0;
    md_reg_node *node = NULL;
    mdm_node_audit_flags_bf ipv6_audit_flags = mnaf_none;

    bail_null(new_mod);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =               "/net/interface/config/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "List of network interfaces";
    node->mrn_audit_descr =       N_("interface $v$");
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =               "/net/interface/config/*/enable";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Enable this interface";
    node->mrn_audit_flags =       mnaf_enabled_flag;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =               "/net/interface/config/*/comment";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_audit_descr =       N_("comment");
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =               "/net/interface/config/*/mtu";
    node->mrn_value_type =         bt_uint16;
    /*
     * XXX/EMT: this should be selected on an interface type-specific
     * basis.  But for now, we know that all of our interfaces are
     * ethernet (except for loopback, which gets its default through
     * the initial values mechanism).
     */
    node->mrn_initial_value =      md_ifc_ethernet_mtu;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Interface MTU";
    node->mrn_audit_descr =       N_("MTU");
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =               "/net/interface/config/*/enslaved";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Interface enslaved state";
    node->mrn_audit_descr =       N_("enslaved");
    node->mrn_audit_flags =       mnaf_enabled_flag;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =               "/net/interface/config/*/display";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Interface display disposition";
    node->mrn_audit_descr =       N_("display");
    node->mrn_audit_flags =       mnaf_enabled_flag;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =            "/net/interface/config/*/type/ethernet/speed";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "auto";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "";
    node->mrn_audit_descr =       N_("speed");
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =            "/net/interface/config/*/type/ethernet/duplex";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "auto";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "";
    node->mrn_audit_descr =       N_("duplex");
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =               "/net/interface/config/*/alias/ifdevname";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Primary name if interface is an alias";
    node->mrn_audit_descr =       N_("device name");
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =               "/net/interface/config/*/alias/index";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Sub-index name if interface is an alias";
    node->mrn_audit_descr =       N_("alias index");
    err = mdm_add_node(new_mod, &node);
    bail_error(err);


    /* ==================== IPv4 specific */

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =               "/net/interface/config/*/addr/ipv4/dhcp";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Enable DHCP for this interface";
    node->mrn_audit_descr =       N_("DHCP");
    node->mrn_audit_flags =       mnaf_enabled_flag;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =               "/net/interface/config/*/addr/ipv4/zeroconf";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Enable zeroconf for this interface";
    node->mrn_audit_descr =       N_("zeroconf");
    node->mrn_audit_flags =       mnaf_enabled_flag;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =              "/net/interface/config/*/addr/ipv4/static/*";
    node->mrn_value_type =         bt_uint8;
    node->mrn_limit_wc_count_max = 1;
    node->mrn_bad_value_msg =      N_("Can only have 1 static IPv4 address "
                                      "configured per interface");
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "List of IPv4 addresses for this interface";
    node->mrn_audit_descr =       N_("interface $4$ static IP address $v$");
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =           "/net/interface/config/*/addr/ipv4/static/*/ip";
    node->mrn_value_type =         bt_ipv4addr;
    node->mrn_initial_value =      "0.0.0.0";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Static IP address";
    node->mrn_audit_descr =       N_("address");
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =     "/net/interface/config/*/addr/ipv4/static/*/mask_len";
    node->mrn_value_type =         bt_uint8;
    node->mrn_initial_value =      "0";
    node->mrn_limit_num_min =      "0";
    node->mrn_limit_num_max =      "32";
    node->mrn_bad_value_msg =
        N_("Mask length must be between 1 and 32 inclusive, or zero if "
           "the address is zero");
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Mask length";
    node->mrn_audit_descr =       N_("mask length");
    err = mdm_add_node(new_mod, &node);
    bail_error(err);


    /* ==================== IPv6 specific */

    /*
     * These IPv6-specific nodes are special, as they are the
     * IPv6-specific children of a non-IPv6-specific wildcard (an
     * interface).  On systems where PROD_FEATURE_IPV6=0, we do not want
     * to see these IPv6-specific changes in the config change audit log
     * when an interface is created.  However, we do want to see changes
     * when the IPv6 configuration is explicitly changed from the
     * defaults, which is why these nodes are not simply marked
     * suppressed.  Such config changes are hard to do without use of
     * the "internal" command or the like, but could happen from a
     * program, XML gateway, etc.
     */
#ifdef PROD_FEATURE_IPV6
    ipv6_audit_flags = mnaf_none;
#else
    ipv6_audit_flags = mnaf_suppress_logging_if_default;
#endif

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =               "/net/interface/config/*/addr/ipv6/enable";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Enable IPv6 for this interface";
    node->mrn_audit_descr =        N_("IPv6");
    node->mrn_audit_flags =        mnaf_enabled_flag | ipv6_audit_flags;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =          "/net/interface/config/*/addr/ipv6/slaac/enable";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Enable SLAAC for this interface";
    node->mrn_audit_descr =        N_("IPv6 SLAAC (autoconfig)");
    node->mrn_audit_flags =        mnaf_enabled_flag | ipv6_audit_flags;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name = "/net/interface/config/*/addr/ipv6/slaac/default_route";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Enable SLAAC to accept a default route";
    node->mrn_audit_descr =        N_("IPv6 SLAAC accept default route");
    node->mrn_audit_flags =        mnaf_enabled_flag | ipv6_audit_flags;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =        "/net/interface/config/*/addr/ipv6/slaac/privacy";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Enable SLAAC privacy extensions";
    node->mrn_audit_descr =        N_("IPv6 SLAAC privacy extensions");
    node->mrn_audit_flags =        mnaf_enabled_flag | ipv6_audit_flags;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =           "/net/interface/config/*/addr/ipv6/dhcp/enable";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Enable DHCPv6 for this interface";
    node->mrn_audit_descr =        N_("DHCPv6");
    node->mrn_audit_flags =        mnaf_enabled_flag | ipv6_audit_flags;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =              "/net/interface/config/*/addr/ipv6/static/*";
    node->mrn_value_type =         bt_uint8;
    node->mrn_limit_wc_count_max = 8;
    node->mrn_bad_value_msg =      N_("Can only have 8 static IPv6 addresses "
                                      "configured per interface");
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "List of IPv6 addresses for this interface";
    node->mrn_audit_descr =        N_("interface $4$ static IPv6 address $v$");
    node->mrn_audit_flags =        mnaf_none | ipv6_audit_flags;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =          "/net/interface/config/*/addr/ipv6/static/*/ip";
    node->mrn_value_type =         bt_ipv6addr;
    node->mrn_initial_value =      "::";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Static IPv6 address";
    node->mrn_audit_descr =        N_("address");
    node->mrn_audit_flags =        mnaf_none | ipv6_audit_flags;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_node(new_mod, &node);
    bail_error(err);
    node->mrn_name =     "/net/interface/config/*/addr/ipv6/static/*/mask_len";
    node->mrn_value_type =         bt_uint8;
    node->mrn_initial_value =      "0";
    node->mrn_limit_num_min =      "0";
    node->mrn_limit_num_max =      "128";
    node->mrn_bad_value_msg =
        N_("Mask length must be between 1 and 128 inclusive, or zero if "
           "the address is ::");
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "Mask length";
    node->mrn_audit_descr =        N_("mask length");
    node->mrn_audit_flags =        mnaf_none | ipv6_audit_flags;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_register_action_nodes(md_module *new_mod)
{
    md_reg_node *node = NULL;
    int err = 0;

    bail_null(new_mod);

    err = mdm_new_action(new_mod, &node, 4);
    bail_error(err);
    node->mrn_name = "/net/interface/actions/override/set";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    node->mrn_cap_mask = mcf_cap_action_privileged;
    node->mrn_action_func = md_interface_override_handle_set;
    node->mrn_action_audit_descr =
        N_("network interface configuration override");
    node->mrn_actions[0]->mra_param_name = "ifname";
    node->mrn_actions[0]->mra_param_audit_descr = N_("interface name");
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[0]->mra_param_required = true;
    node->mrn_actions[1]->mra_param_name = "ip";
    node->mrn_actions[1]->mra_param_audit_descr = N_("IP address");
    node->mrn_actions[1]->mra_param_type = bt_ipv4addr;
    /* Default value handled by action handler */
    node->mrn_actions[2]->mra_param_name = "netmask";
    node->mrn_actions[2]->mra_param_audit_descr = N_("netmask");
    node->mrn_actions[2]->mra_param_type = bt_ipv4addr;
    /* Default value handled by action handler */
    node->mrn_actions[3]->mra_param_name = "enabled";
    node->mrn_actions[3]->mra_param_audit_descr = N_("enabled");
    node->mrn_actions[3]->mra_param_type = bt_bool;
    node->mrn_actions[3]->mra_param_required = true;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);


    err = mdm_new_action(new_mod, &node, 1);
    bail_error(err);
    node->mrn_name = "/net/interface/actions/override/clear";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    node->mrn_cap_mask = mcf_cap_action_privileged;
    node->mrn_action_func = md_interface_override_handle_clear;
    node->mrn_action_audit_descr =
        N_("clear network interface configuration override");
    node->mrn_actions[0]->mra_param_name = "ifname";
    node->mrn_actions[0]->mra_param_audit_descr = N_("interface name");
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[0]->mra_param_required = true;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_action(new_mod, &node, 1);
    bail_error(err);
    node->mrn_name = "/net/interface/actions/dhcp/renew";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    node->mrn_cap_mask = mcf_cap_action_privileged;
    node->mrn_action_func = md_interface_renew_dhcpv4;
    node->mrn_action_audit_descr =
        N_("renew DHCP lease");
    node->mrn_actions[0]->mra_param_name = "ifname";
    node->mrn_actions[0]->mra_param_audit_descr = N_("interface name");
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[0]->mra_param_required = true;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_action(new_mod, &node, 1);
    bail_error(err);
    node->mrn_name = "/net/interface/actions/ipv6/dhcp/renew";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    node->mrn_cap_mask = mcf_cap_action_privileged;
    node->mrn_action_func = md_interface_renew_dhcpv6;
    node->mrn_action_audit_descr =
        N_("renew DHCPv6 lease");
    node->mrn_actions[0]->mra_param_name = "ifname";
    node->mrn_actions[0]->mra_param_audit_descr = N_("interface name");
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[0]->mra_param_required = true;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_action(new_mod, &node, 8);
    bail_error(err);
    node->mrn_name = "/net/interface/actions/alias/add";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    node->mrn_cap_mask = mcf_cap_action_privileged;
    node->mrn_action_func = md_interface_alias_handle_add;
    node->mrn_action_audit_flags |= maaf_suppress_logging_if_non_interactive;
    node->mrn_action_audit_descr =
        N_("add interface alias (secondary address)");
    node->mrn_actions[0]->mra_param_name = "ifname";
    node->mrn_actions[0]->mra_param_audit_descr = N_("interface name");
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[0]->mra_param_required = true;

    /* Deprecated form of address specification */
    node->mrn_actions[1]->mra_param_name = "ip";
    node->mrn_actions[1]->mra_param_audit_descr = N_("IPv4 address");
    node->mrn_actions[1]->mra_param_type = bt_ipv4addr;
    node->mrn_actions[2]->mra_param_name = "netmask";
    node->mrn_actions[2]->mra_param_audit_descr = N_("netmask");
    node->mrn_actions[2]->mra_param_type = bt_ipv4addr;

    /* Curent form of address specification */
    node->mrn_actions[3]->mra_param_name = "inet_address";
    node->mrn_actions[3]->mra_param_audit_descr = N_("IP address");
    node->mrn_actions[3]->mra_param_type = bt_inetaddr;
    node->mrn_actions[4]->mra_param_name = "masklen";
    node->mrn_actions[4]->mra_param_audit_descr = N_("masklen");
    node->mrn_actions[4]->mra_param_type = bt_uint8;

    node->mrn_actions[5]->mra_param_name = "tag";
    node->mrn_actions[5]->mra_param_audit_descr = N_("alias tag");
    node->mrn_actions[5]->mra_param_type = bt_string;
    node->mrn_actions[5]->mra_param_required = true;
    node->mrn_actions[6]->mra_param_name = "arp_count";
    node->mrn_actions[6]->mra_param_audit_descr = N_("ARP count");
    node->mrn_actions[6]->mra_param_type = bt_uint32;
    node->mrn_actions[6]->mra_param_default_value_integer =
                                            md_ifc_gratarp_resched_count;
    node->mrn_actions[7]->mra_param_name = "remove_tag";
    node->mrn_actions[7]->mra_param_audit_descr = N_("alias tag for removal");
    node->mrn_actions[7]->mra_param_type = bt_string;
        /* omitted parameter handled by action handler */
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_action(new_mod, &node, 6);
    bail_error(err);
    node->mrn_name = "/net/interface/actions/alias/delete";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    node->mrn_cap_mask = mcf_cap_action_privileged;
    node->mrn_action_func = md_interface_alias_handle_delete;
    node->mrn_action_audit_flags |= maaf_suppress_logging_if_non_interactive;
    node->mrn_action_audit_descr =
        N_("delete interface alias (secondary address)");
    node->mrn_actions[0]->mra_param_name = "ifname";
    node->mrn_actions[0]->mra_param_audit_descr = N_("interface name");
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[0]->mra_param_required = true;

    /*
     * Some representation of the IP address (via "ip" or "inet_address")
     * and some representation of the mask length (via "netmask" or "masklen")
     * is mandatory.  But we can't mark any of them required because we 
     * don't know which one they're going to choose.  The action handler
     * will enforce.
     */

    /* Deprecated form of address specification */
    node->mrn_actions[1]->mra_param_name = "ip";
    node->mrn_actions[1]->mra_param_audit_descr = N_("IP address");
    node->mrn_actions[1]->mra_param_type = bt_ipv4addr;
    node->mrn_actions[2]->mra_param_name = "netmask";
    node->mrn_actions[2]->mra_param_audit_descr = N_("netmask");
    node->mrn_actions[2]->mra_param_type = bt_ipv4addr;

    /* Current form of address specification */
    node->mrn_actions[3]->mra_param_name = "inet_address";
    node->mrn_actions[3]->mra_param_audit_descr = N_("IP address");
    node->mrn_actions[3]->mra_param_type = bt_inetaddr;
    node->mrn_actions[4]->mra_param_name = "masklen";
    node->mrn_actions[4]->mra_param_audit_descr = N_("masklen");
    node->mrn_actions[4]->mra_param_type = bt_uint8;

    node->mrn_actions[5]->mra_param_name = "tag";
    node->mrn_actions[5]->mra_param_audit_descr = N_("alias tag");
    node->mrn_actions[5]->mra_param_type = bt_string;
    node->mrn_actions[5]->mra_param_required = true;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_action(new_mod, &node, 2);
    bail_error(err);
    node->mrn_name = "/net/interface/actions/alias/clear";
    node->mrn_node_reg_flags = mrf_flags_reg_action;
    node->mrn_cap_mask = mcf_cap_action_privileged;
    node->mrn_action_func = md_interface_alias_handle_clear;
    node->mrn_action_audit_flags |= maaf_suppress_logging_if_non_interactive;
    node->mrn_action_audit_descr =
        N_("clear interface aliases (secondary addresses)");
    node->mrn_actions[0]->mra_param_name = "ifname";
    node->mrn_actions[0]->mra_param_audit_descr = N_("interface name");
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[0]->mra_param_required = true;
    node->mrn_actions[1]->mra_param_name = "tag";
    node->mrn_actions[1]->mra_param_audit_descr = N_("alias tag");
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_required = true;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_register_event_nodes(md_module *new_mod)
{
    md_reg_node *node = NULL;
    int err = 0;

    bail_null(new_mod);

    err = mdm_new_event(new_mod, &node, 8);
    bail_error(err);
    node->mrn_name = "/net/interface/events/state_change";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_events[0]->mre_binding_name = "ifname";
    node->mrn_events[0]->mre_binding_type = bt_string;
    node->mrn_events[1]->mre_binding_name = "cause";
    node->mrn_events[1]->mre_binding_type = bt_string;
    node->mrn_events[2]->mre_binding_name = "old/flags/oper_up";
    node->mrn_events[2]->mre_binding_type = bt_bool;
    node->mrn_events[3]->mre_binding_name = "old/addr/ipv4/1/ip";
    node->mrn_events[3]->mre_binding_type = bt_ipv4addr;
    node->mrn_events[4]->mre_binding_name = "old/addr/ipv4/1/mask_len";
    node->mrn_events[4]->mre_binding_type = bt_uint8;
    node->mrn_events[5]->mre_binding_name = "new/flags/oper_up";
    node->mrn_events[5]->mre_binding_type = bt_bool;
    node->mrn_events[6]->mre_binding_name = "new/addr/ipv4/1/ip";
    node->mrn_events[6]->mre_binding_type = bt_ipv4addr;
    node->mrn_events[7]->mre_binding_name = "new/addr/ipv4/1/mask_len";
    node->mrn_events[7]->mre_binding_type = bt_uint8;
    node->mrn_description = "Notification that the up/down status, IP "
        "address, or mask length of an interface has changed";
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_event(new_mod, &node, 5);
    bail_error(err);
    node->mrn_name = "/net/interface/events/link_up";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_events[0]->mre_binding_name = "/net/interface/state/*";
    node->mrn_events[1]->mre_binding_name = "/net/interface/state/*/ifindex";
    node->mrn_events[1]->mre_binding_type = bt_int32; /* Override for SNMP */
    node->mrn_events[2]->mre_binding_name =
        "/net/interface/state/*/snmp/admin_status";
    node->mrn_events[3]->mre_binding_name =
        "/net/interface/state/*/snmp/oper_status";
    node->mrn_events[4]->mre_binding_name = "ifname";
    node->mrn_events[4]->mre_binding_type = bt_string;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

    err = mdm_new_event(new_mod, &node, 5);
    bail_error(err);
    node->mrn_name = "/net/interface/events/link_down";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_events[0]->mre_binding_name = "/net/interface/state/*";
    node->mrn_events[1]->mre_binding_name = "/net/interface/state/*/ifindex";
    node->mrn_events[1]->mre_binding_type = bt_int32; /* Override for SNMP */
    node->mrn_events[2]->mre_binding_name =
        "/net/interface/state/*/snmp/admin_status";
    node->mrn_events[3]->mre_binding_name =
        "/net/interface/state/*/snmp/oper_status";
    node->mrn_events[4]->mre_binding_name = "ifname";
    node->mrn_events[4]->mre_binding_type = bt_string;
    err = mdm_add_node(new_mod, &node);
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static const bn_str_value md_interface_initial_values[] = {
    {"/net/interface/config/lo/addr/ipv4/static/1/ip",
     bt_ipv4addr, "127.0.0.1"},

    {"/net/interface/config/lo/addr/ipv4/static/1/mask_len",
     bt_uint8, "8"},
};


/* ------------------------------------------------------------------------- */
static int
md_interface_copy_mfdb(md_commit *commit, mdb_db *db)
{
    int err = 0;
    mdb_db *mfdb = NULL;
    bn_binding_array *bindings = NULL;
    int i = 0, num_bindings = 0;
    const bn_binding *binding = NULL;
    bn_binding *new_binding = NULL;
    tstr_array *bname_parts = NULL;
    const tstring *mfdb_bname = NULL;
    char *main_bname = NULL;

    err = mdb_dbbe_open(&mfdb, NULL, md_mfdb_path);
    if (err == lc_err_file_not_found && !lc_devel_is_production()) {
        err = 0;
        lc_log_debug(LOG_INFO, "Devenv: skipping copy of interface mfdb "
                     "nodes");
        goto bail;
    }
    bail_error_errno(err, "Opening mfdb");

    err = mdb_iterate_binding(commit, mfdb, "/mfg/mfdb/net/interface/config",
                              mdqf_sel_iterate_subtree, &bindings);
    bail_error(err);

    num_bindings = bn_binding_array_length_quick(bindings);

    lc_log_basic(LOG_INFO, "Found %d interface bindings from mfdb",
                 num_bindings);

    for (i = 0; i < num_bindings; ++i) {
        binding = bn_binding_array_get_quick(bindings, i);
        bail_null(binding);
        tstr_array_free(&bname_parts);
        err = bn_binding_get_name_parts(binding, &bname_parts);
        bail_error(err);
        mfdb_bname = bn_binding_get_name_quick(binding);
        bail_null(mfdb_bname);

        /*
         * We could use bn_binding_name_parts_pattern_match_va() here,
         * but it's more typing, and perf is immaterial here.
         *
         * Note: we can't just take whatever we find in the mfdb, in case
         * it's from a later version (possibly following a downgrade) --
         * we might end up trying to set nodes that are not registered.
         * So we have a whitelist of nodes we know we can take.
         */
        if (!bn_binding_name_parts_pattern_match
                (bname_parts, true, 
                 "/mfg/mfdb/net/interface/config/*/addr/ipv4/dhcp") &&
            !bn_binding_name_parts_pattern_match
                (bname_parts, true,
                 "/mfg/mfdb/net/interface/config/*/addr/ipv4/zeroconf") &&
            !bn_binding_name_parts_pattern_match
                (bname_parts, true,
                 "/mfg/mfdb/net/interface/config/*/addr/ipv6/dhcp/enable") &&
            !bn_binding_name_parts_pattern_match
                (bname_parts, true,
                 "/mfg/mfdb/net/interface/config/*/enable") &&
            !bn_binding_name_parts_pattern_match
                (bname_parts, true,
                 "/mfg/mfdb/net/interface/config/*/display") &&
            !bn_binding_name_parts_pattern_match
                (bname_parts, true,
                 "/mfg/mfdb/net/interface/config/*/mtu") &&
            !bn_binding_name_parts_pattern_match
                (bname_parts, true,
                 "/mfg/mfdb/net/interface/config/*/addr/ipv4/static/*/ip") &&
            !bn_binding_name_parts_pattern_match
                (bname_parts, true,
                 "/mfg/mfdb/net/interface/config/*/addr/ipv4/static/*/"
                 "mask_len")) {
            /*
             * This is a node that we do not support from the mfdb.
             * We limit this so we can make sure to cover all the bases
             * in the (unlikely) event of an upgrade.
             */
            lc_log_basic(LOG_WARNING, "Unrecognized node in mfdb (%s), "
                         "ignoring", ts_str(mfdb_bname));
            continue;
        }

        /*
         * Delete the first two name parts, and then we have the name
         * we want to set into the db.
         */
        err = tstr_array_delete(bname_parts, 0);
        bail_error(err);
        err = tstr_array_delete(bname_parts, 0);
        bail_error(err);
        safe_free(main_bname);
        err = bn_binding_name_parts_to_name(bname_parts, true, &main_bname);
        bail_error(err);
        bn_binding_free(&new_binding);
        err = bn_binding_dup(binding, &new_binding);
        bail_error(err);
        err = bn_binding_set_name(new_binding, main_bname);
        bail_error(err);
        err = mdb_set_node_binding(commit, db, bsso_create, 0, new_binding);
        bail_error(err);
    }

 bail:
    tstr_array_free(&bname_parts);
    bn_binding_array_free(&bindings);
    bn_binding_free(&new_binding);
    safe_free(main_bname);
    mdb_dbbe_close(&mfdb);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_interface_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    bn_binding *binding = NULL;

    err = mdb_create_node_bindings
        (commit, db, md_interface_initial_values,
         sizeof(md_interface_initial_values) / sizeof(bn_str_value));
    bail_error(err);

    /*
     * Create binding for loopback's MTU.
     *
     * XXX/EMT: we should really query the current actual MTU and use
     * this, since loopback may have a different MTU on different
     * platforms.  But for expediency, hardwire Linux's value for now.
     */
    err = bn_binding_new_str
        (&binding, "/net/interface/config/lo/mtu", ba_value,
         bt_uint16, 0, md_ifc_lo_mtu);
    err = mdb_set_node_binding(commit, db, bsso_create, 0, binding);
    bail_error(err);

    /*
     * Copy stuff from the mfdb -- see mfdb-nodes.txt.
     */
    err = md_interface_copy_mfdb(commit, db);
    bail_error(err);
    
 bail:
    bn_binding_free(&binding);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_upgrade_downgrade(const mdb_db *old_db,
                               mdb_db *inout_new_db,
                               uint32 from_module_version,
                               uint32 to_module_version, void *arg)
{
    int err = 0;
    bn_binding_array *ba = NULL;
    uint32 i = 0, num_bindings = 0, num_sis = 0;
    bn_binding *binding = NULL;
    bn_binding *t_binding = NULL;
    char *ifname = NULL;
    tbool is_physical = false;
    char *speed_str = NULL;
    char *duplex_str = NULL;
    char new_name[17], *np = NULL, *cp = NULL;
    tbool ok = false;
    bn_attrib *dup_test_attr = NULL;
    tstr_array *si_nodes = NULL;
    const char *si_node = NULL;
    tstring *masklen_str = NULL, *addr_str = NULL;
    tbool first_intf = false;
    uint32 idx = 0;
    tstr_array *ip_names = NULL, *ip_dups = NULL;
    const char *sn = NULL, *psn = NULL;
    const char *this_dup = NULL;
    char *addr_bname = NULL, *masklen_bname = NULL;
    tstring *ifname_ts = NULL;
    char ab[INET6_ADDRSTRLEN];
    tbool found = false, found2 = false, is_valid = false;
    bn_ipv4addr ipv4_addr = lia_ipv4addr_any;
    bn_ipv6addr ipv6_addr = lia_ipv6addr_any;
    uint8 masklen = 0;
    uint16 mtu = 0;
    tbool ipv6_if_enabled = false;

    /*
     * For most upgrade/downgrade scenarios, we just want the basic
     * behavior...
     */
    err = md_generic_upgrade_downgrade
        (old_db, inout_new_db, from_module_version, to_module_version, arg);
    bail_error(err);

    if ((from_module_version == 2) && (to_module_version == 3)) {
        err = md_upgrade_db_transform(old_db, inout_new_db,
                                      from_module_version, to_module_version,
                                      md_if_upgrade_rules);
        bail_error(err);
    }
    else if ((from_module_version == 3) && (to_module_version == 4)) {
        err = mdb_iterate_binding(NULL, inout_new_db, "/net/interface/config",
                                  0, &ba);
        bail_error(err);

        err = bn_binding_array_length(ba, &num_bindings);
        bail_error(err);

        for (i = 0; i < num_bindings; ++i) {
            err = bn_binding_array_get(ba, i, &binding);
            bail_error(err);

            err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
                                     &ifname);
            bail_error(err);

            is_physical = false;
            err = md_interface_is_physical(ifname, &is_physical);
            bail_error(err);

            if (is_physical) {
                safe_free(ifname);
                continue;
            }

            /* Change the speed binding to auto regardless */
            speed_str =
                smprintf("/net/interface/config/%s/type/ethernet/speed",
                         ifname);
            bail_null(speed_str);

            err = bn_binding_new_str
                (&t_binding, speed_str, ba_value, bt_string, 0, "auto");
            bail_error(err);
            err = mdb_set_node_binding
                (NULL, inout_new_db, bsso_modify, 0, t_binding);
            bail_error(err);
            bn_binding_free(&t_binding);

            /* Change the duplex binding to auto regardless */
            duplex_str =
                smprintf("/net/interface/config/%s/type/ethernet/duplex",
                         ifname);
            bail_null(duplex_str);

            err = bn_binding_new_str
                (&t_binding, duplex_str, ba_value, bt_string, 0, "auto");
            bail_error(err);
            err = mdb_set_node_binding
                (NULL, inout_new_db, bsso_modify, 0, t_binding);
            bail_error(err);
            bn_binding_free(&t_binding);

            lc_log_basic(LOG_DEBUG, "Interface upgrade looking at interface"
                         " %s", ifname);


            safe_free(speed_str);
            safe_free(duplex_str);
            safe_free(ifname);
        }
        err = bn_binding_array_free(&ba);
        bail_error(err);
    }
    else if (((from_module_version == 7) && (to_module_version == 8)) ||
             ((from_module_version == 12) && (to_module_version == 13))) {

        err = mdb_iterate_binding(NULL, inout_new_db, "/net/interface/config",
                                  0, &ba);
        bail_error(err);

        err = bn_binding_array_length(ba, &num_bindings);
        bail_error(err);

        for (i = 0; i < num_bindings; ++i) {
            err = bn_binding_array_get(ba, i, &binding);
            bail_error(err);

            safe_free(ifname);
            err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
                                     &ifname);
            bail_error(err);

            /* Test if the ifname is now forbidden */
            err = lc_ifname_validate(ifname, &ok);
            bail_error(err);

            if (!ok) {
                cp = ifname;
                np = new_name;
                *np = '\0';
                while (cp && *cp &&
                       (np - new_name < (long) sizeof(new_name))) {
                    if (cp - ifname > 0 && cp[-1] == '.' && cp[0] == '.') {
                        *np++ = '-';
                        cp++;
                    }
                    else if (*cp == '/' ||
                             (to_module_version >= 13 && *cp == '%') ||
                             (*cp >= 0 && *cp <= 32) ||
                             (*cp >= 127)) {
                        *np++ = '-';
                        cp++;
                    }
                    else {
                        *np++ = *cp++;
                    }
                }
                if (np) {
                    *np = '\0';
                }

                /* Deal with duplicates, by appending a '-' */
                while (true) {
                    bn_attrib_free(&dup_test_attr);
                    err = mdb_get_node_attrib_fmt(NULL, inout_new_db, 0, 
                                                  ba_value,
                                                  &dup_test_attr, 
                                                  "/net/interface/config/%s", 
                                                  new_name);
                    bail_error(err);
                    if (!dup_test_attr) {
                        break;
                    }
                    else { /* Duplicate detected */
                        if (strlen(new_name) == sizeof(new_name) - 1) {
                            break;
                        }
                        else {
                            safe_strlcat(new_name, "-");
                        }
                    }
                }

                /* Test if the ifname is now forbidden */
                err = lc_ifname_validate(new_name, &ok);
                bail_error(err);

                if (!strcmp(new_name, ifname)) {
                    lc_log_basic(LOG_WARNING, _("Could not rename interface "
                                                "with invalid name: %s"),
                                 ifname);
                    continue;
                }

                /* If it's still not okay, just delete it */
                if (!ok) {
                    lc_log_basic(LOG_WARNING, _("Removing interface with "
                                                "invalid name: %s"), 
                                 ifname);
                    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete, 
                                           0, bt_none, NULL,
                                           "%s", 
                                           ts_str(bn_binding_get_name_quick
                                                  (binding)));
                    bail_error(err);
                }
                else {
                    lc_log_basic(LOG_WARNING, _("Renaming interface with "
                                                "invalid name: %s to %s"), 
                                 ifname, new_name);
                    err = mdb_reparent_node(NULL, inout_new_db, bsso_reparent,
                                   0, 
                                   ts_str(bn_binding_get_name_quick(binding)),
                                   NULL, new_name, true);
                    bail_error(err);
                }
            }
        }
        err = bn_binding_array_free(&ba);
        bail_error(err);
    }
    else if (((from_module_version == 11) && (to_module_version == 12)) ||
             ((from_module_version == 22) && (to_module_version == 23))) {
        err = md_if_names_fix(inout_new_db);
        bail_error(err);
    }

    /*
     * If any nonzero static IP addresses have zero for their mask
     * lengths, set the address to be 0.0.0.0 to make it legal.
     *
     * This used to be the 9-to-10 upgrade, but was changed to 10-to-11
     * to make it run again, to fix a loophole whereby someone could
     * have still had a nonzero address and a zero netmask.
     */
    else if (((from_module_version == 10) && (to_module_version == 11)) ||
             ((from_module_version == 13) && (to_module_version == 14))) {

        err = mdb_get_matching_tstr_array
            (NULL, inout_new_db, "/net/interface/config/*/addr/ipv4/static/*",
             0, &si_nodes);
        bail_error(err);
        num_sis = tstr_array_length_quick(si_nodes);
        for (i = 0; i < num_sis; ++i) {
            si_node = tstr_array_get_str_quick(si_nodes, i);
            bail_null(si_node);
            ts_free(&masklen_str);
            err = mdb_get_node_attrib_tstr_fmt
                (NULL, inout_new_db, 0, ba_value, NULL, &masklen_str,
                 "%s/mask_len", si_node);
            bail_error(err);
            ts_free(&addr_str);
            err = mdb_get_node_attrib_tstr_fmt
                (NULL, inout_new_db, 0, ba_value, NULL, &addr_str,
                 "%s/ip", si_node);
            bail_error(err);
            if (ts_equal_str(masklen_str, "0", false) &&
                !ts_equal_str(addr_str, "0.0.0.0", false)) {
                lc_log_basic(LOG_NOTICE, "Setting address %s with zero "
                             "mask length to 0.0.0.0", ts_str(addr_str));
                err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
                                       0, bt_ipv4addr, "0.0.0.0",
                                       "%s/ip",
                                       si_node);
                bail_error(err);
            }
            else if (!ts_equal_str(masklen_str, "0", false) &&
                     ts_equal_str(addr_str, "0.0.0.0", false)) {
                lc_log_basic(LOG_NOTICE, "Fixing address %s to have zero "
                             "mask length", ts_str(addr_str));
                err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
                                       0, bt_uint8, "0", "%s/mask_len",
                                       si_node);
                bail_error(err);
            }
        }
    }

    else if ((from_module_version == 14) && (to_module_version == 15)) {
        err = mdb_get_matching_tstr_array_attrib(NULL, inout_new_db,
                            "/net/interface/config/*/addr/ipv4/static/*",
                            0, ba_value, &ip_names);
        bail_error(err);

        if (ip_names && tstr_array_length_quick(ip_names) > 1) {
            err = tstr_array_sort(ip_names);
            bail_error(err);

            /* Build a list of duplicates IPv4 addrs in string form */
            for (idx = tstr_array_length_quick(ip_names) - 1; idx > 0; idx--) {
                sn = tstr_array_get_str_quick(ip_names, idx);
                psn = tstr_array_get_str_quick(ip_names, idx - 1);
                if (!safe_strcmp(sn, "0.0.0.0")) {
                    continue;
                }
                if (!safe_strcmp(sn, psn)) {
                    if (!ip_dups) {
                        err = tstr_array_new(&ip_dups, NULL);
                        bail_error_null(err, ip_dups);
                    }
                    err = tstr_array_append_str(ip_dups, sn);
                    bail_error(err);
                }
            }
        }
        /* Now find and delete these duplicates, except for the first one. */
        if (ip_dups && tstr_array_length_quick(ip_dups) > 0) {

            for (idx = 0; idx < tstr_array_length_quick(ip_dups); idx++) {
                this_dup = tstr_array_get_str_quick(ip_dups, idx);
                bail_null(this_dup);
                first_intf = true;

                tstr_array_free(&si_nodes);
                err = mdb_get_matching_tstr_array
                    (NULL, inout_new_db,
                     "/net/interface/config/*/addr/ipv4/static/*",
                     0, &si_nodes);
                bail_error(err);
                num_sis = tstr_array_length_quick(si_nodes);
                for (i = 0; i < num_sis; ++i) {
                    si_node = tstr_array_get_str_quick(si_nodes, i);
                    bail_null(si_node);
                    ts_free(&addr_str);
                    err = mdb_get_node_attrib_tstr_fmt
                        (NULL, inout_new_db, 0, ba_value, NULL, &addr_str,
                 "%s/ip", si_node);
                    bail_error(err);
                    if (ts_equal_str(addr_str, this_dup, false)) {
                        if (first_intf) {
                            first_intf = false;
                            continue;
                        }

                        lc_log_basic(LOG_NOTICE,
                                     "Removing duplicate IP address %s",
                                     ts_str(addr_str));
                        err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
                                           0, bt_ipv4addr, "0.0.0.0", "%s/ip",
                                           si_node);
                        bail_error(err);
                        err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
                                           0, bt_uint8, "0", "%s/mask_len",
                                           si_node);
                        bail_error(err);
                    }
                }
            }
        }
    }

    else if ((from_module_version == 17) && (to_module_version == 18)) {

        /* Do not allow bad IPv4 addrs or IPv4 addr/masklen combos */

        err = mdb_get_matching_tstr_array
            (NULL, inout_new_db, "/net/interface/config/*/addr/ipv4/static/*",
             0, &si_nodes);
        bail_error(err);

        num_sis = tstr_array_length_quick(si_nodes);
        for (i = 0; i < num_sis; ++i) {
            si_node = tstr_array_get_str_quick(si_nodes, i);
            bail_null(si_node);

            ts_free(&ifname_ts);
            err = bn_binding_name_to_parts_va(si_node, false, 1, 3, &ifname_ts);
            bail_error(err);
            safe_free(addr_bname);
            addr_bname = smprintf("%s/ip", si_node);
            bail_null(addr_bname);
            safe_free(masklen_bname);
            masklen_bname = smprintf("%s/mask_len", si_node);
            bail_null(masklen_bname);

            err = mdb_get_node_value_ipv4addr_bnipv4(NULL, inout_new_db, addr_bname,
                                                     0, &found, &ipv4_addr);
            bail_error(err);
            err = mdb_get_node_value_uint8(NULL, inout_new_db, masklen_bname,
                                           0, &found2, &masklen);
            bail_error(err);

            if (found && found2) {
                err = md_interface_validate_ipv4addr_masklen(NULL, ts_str(ifname_ts),
                                                             ipv4_addr,
                                                             masklen,
                                                             &is_valid);
                bail_error(err);
            }
            else {
                is_valid = false;
            }

            if (!is_valid) {
                lc_log_basic(LOG_NOTICE,
                             "Removing invalid IP address %s/%d",
                             lia_ipv4addr_to_str_buf_quick(&ipv4_addr, ab, sizeof(ab)),
                             masklen);
                err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
                                       0, bt_ipv4addr, "0.0.0.0", "%s", addr_bname);
                bail_error(err);
                err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
                                       0, bt_uint8, "0", "%s", masklen_bname);
                bail_error(err);
            }
        }
    }
    else if (((from_module_version == 18) && (to_module_version == 19)) ||
             ((from_module_version == 20) && (to_module_version == 21))) {

        /* Do not allow bad IPv6 addrs or IPv6 addr/masklen combos */

        err = mdb_get_matching_tstr_array
            (NULL, inout_new_db, "/net/interface/config/*/addr/ipv6/static/*",
             0, &si_nodes);
        bail_error(err);

        num_sis = tstr_array_length_quick(si_nodes);
        for (i = 0; i < num_sis; ++i) {
            si_node = tstr_array_get_str_quick(si_nodes, i);
            bail_null(si_node);

            ts_free(&ifname_ts);
            err = bn_binding_name_to_parts_va(si_node, false, 1, 3, &ifname_ts);
            bail_error(err);
            safe_free(addr_bname);
            addr_bname = smprintf("%s/ip", si_node);
            bail_null(addr_bname);
            safe_free(masklen_bname);
            masklen_bname = smprintf("%s/mask_len", si_node);
            bail_null(masklen_bname);

            err = mdb_get_node_value_ipv6addr(NULL, inout_new_db, addr_bname,
                                          0, &found, &ipv6_addr);
            bail_error(err);
            err = mdb_get_node_value_uint8(NULL, inout_new_db, masklen_bname,
                                           0, &found2, &masklen);
            bail_error(err);

            if (found && found2) {
                err = md_interface_validate_ipv6addr_masklen(NULL, ts_str(ifname_ts),
                                                             ipv6_addr,
                                                             masklen,
                                                             &is_valid);
                bail_error(err);

                if (is_valid && !safe_strcmp(ts_str(ifname_ts), "lo") &&
                    (!lia_ipv6addr_is_loopback_quick(&ipv6_addr) ||
                     masklen != 128)) {
                    is_valid = false;
                }
            }
            else {
                is_valid = false;
            }

            if (!is_valid) {
                lc_log_basic(LOG_NOTICE,
                             "Removing invalid IPv6 address %s/%d from %s",
                             lia_ipv6addr_to_str_buf_quick(&ipv6_addr,
                                                           ab, sizeof(ab)),
                             masklen, ts_str(ifname_ts));
                err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
                                       0, bt_none, NULL, "%s", si_node);
                bail_error(err);
            }
        }
    }
    else if (((from_module_version == 19) && (to_module_version == 20)) ||
             ((from_module_version == 21) && (to_module_version == 22))) {
             /*
              * Administratively disable IPv6 on interfaces with MTU < 1280.
              *
              * Run this again for bug 14100 (orig. for bug 14005):  Even if
              * IPv6 is not feature-enabled yet, we want IPv6 disabled on an
              * smaller MTU interfaces now, so that we're consistent when IPv6
              * becomes feature-enabled.  Once IPv6 is feature-enabled,
              * side-effects will raise the MTU as needed per interface if the
              * user enables IPv6.  The rationale here is that it's better to
              * change IPv6 config that doesn't yet apply than to have
              * side-effects unexpectedly change MTU on an upgrade's initial
              * commit, which might be unexpected or go unnoticed.
              */
        bn_binding_array_free(&ba);
        err = mdb_iterate_binding(NULL, inout_new_db, "/net/interface/config",
                                  0, &ba);
        bail_error(err);

        err = bn_binding_array_length(ba, &num_bindings);
        bail_error(err);

        for (i = 0; i < num_bindings; ++i) {
            err = bn_binding_array_get(ba, i, &binding);
            bail_error(err);

            safe_free(ifname);
            err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
                                     &ifname);
            bail_error(err);

            err = mdb_get_child_value_uint16(NULL, inout_new_db, "mtu",
                                             binding, 0, &mtu);
            bail_error(err);
            if (mtu < md_ifc_ipv6_min_mtu) {
                err = mdb_get_child_value_tbool(NULL, inout_new_db,
                                                "addr/ipv6/enable",
                                                binding, 0, &ipv6_if_enabled);
                bail_error(err);
                if (ipv6_if_enabled) {
                    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify, 0,
                                           bt_bool, "false", "%s/%s",
                                           ts_str(bn_binding_get_name_quick
                                                    (binding)),
                                            "addr/ipv6/enable");
                    bail_error(err);
                    lc_log_basic
                        (md_proto_ipv6_feature_enabled() ?
                            LOG_NOTICE : LOG_DEBUG, /* quiet if no IPv6 */
                         _("Interface %s IPv6 configuration disabled due to "
                           "MTU of %hu below the minimum required (%hu)"),
                           ifname, mtu, md_ifc_ipv6_min_mtu);
                }
            }
        }
    }


 bail:
    safe_free(speed_str);
    safe_free(duplex_str);
    safe_free(ifname);
    bn_binding_free(&t_binding);
    bn_binding_array_free(&ba);
    bn_attrib_free(&dup_test_attr);
    ts_free(&masklen_str);
    ts_free(&addr_str);
    tstr_array_free(&si_nodes);
    tstr_array_free(&ip_names);
    tstr_array_free(&ip_dups);
    ts_free(&ifname_ts);
    safe_free(addr_bname);
    safe_free(masklen_bname);

    return(err);
}

/*****************************************************************************/
/*****************************************************************************/
/*** VALIDATION                                                            ***/
/*****************************************************************************/
/*****************************************************************************/

/* ------------------------------------------------------------------------- */
static int
md_interface_commit_check(md_commit *commit,
                          const mdb_db *old_db, 
                          const mdb_db *new_db, 
                          mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    uint32 i = 0, num_bindings = 0;
    bn_binding_array *ba = NULL;
    bn_binding *binding = NULL, *tbinding = NULL;
    char *name = NULL;
    tstr_array *ip_addrs = NULL, *ipv6_addrs = NULL;
    tstr_array *intf_names = NULL;
    tstring *dup_ip_addr = NULL, *dup_ipv6_addr = NULL;
    uint32 num_changes = 0;
    mdb_db_change *change = NULL;
    const char *intf = NULL;
    tbool intf_ok = false;
    tbool dhcpv4 = false, zeroconf = false;
    char *alias_ifdevname = NULL, *alias_index = NULL;
    bn_attrib *attrib = NULL;
    char *addr_bname = NULL, *masklen_bname = NULL;
    int addr_idx = 0;
    bn_ipv4addr ipv4_addr = lia_ipv4addr_any;
    uint8 masklen = 0;
    tbool found = false;
    char *addr_str = NULL;
    md_if_config_check_change_context ctxt;
    bn_ipv6addr ipv6_addr = lia_ipv6addr_any;
    tbool is_valid = false;

    num_changes = mdb_db_change_array_length_quick(change_list);
    for (i = 0; i < num_changes; ++i) {
        change = mdb_db_change_array_get_quick(change_list, i);
        bail_null(change);

        if (!lc_is_prefix("/net/interface/config/", ts_str(change->mdc_name),
                          false)) {
            continue;
        }

        /* Interface name validation */
        if (bn_binding_name_parts_pattern_match_va
            (change->mdc_name_parts, 0, 4, 
             "net", "interface", "config", "*")) {
            if (change->mdc_change_type != mdct_delete) {
                intf = tstr_array_get_str_quick(change->mdc_name_parts, 3);
                bail_null(intf);

                err = lc_ifname_validate(intf, &intf_ok);
                bail_error(err);
                if (!intf_ok) {
                    err = md_commit_set_return_status_msg_fmt
                        (commit, 1,
                         _("The interface name %s is not valid."), intf);
                    bail_error(err);
                    goto bail;
                }
            }
        }

        /* ---------- IPv4 specific checks */

        /* Static address / masklen validity */
        if (bn_binding_name_parts_pattern_match_va
            (change->mdc_name_parts, 0, 9, "net", "interface", "config", "*",
             "addr", "ipv4", "static", "*", "mask_len") ||
            bn_binding_name_parts_pattern_match_va
            (change->mdc_name_parts, 0, 9, "net", "interface", "config", "*",
             "addr", "ipv4", "static", "*", "ip")) {
            if (change->mdc_change_type != mdct_delete) {
                bail_null(change->mdc_new_attribs);
                intf = tstr_array_get_str_quick(change->mdc_name_parts, 3);
                bail_null(intf);
                addr_idx = atoi(tstr_array_get_str_quick
                                (change->mdc_name_parts, 7));
                safe_free(addr_bname);
                addr_bname = smprintf("/net/interface/config/%s/addr/ipv4/"
                                      "static/%d/ip", intf, addr_idx);
                bail_null(addr_bname);
                safe_free(masklen_bname);
                masklen_bname = smprintf("/net/interface/config/%s/addr/ipv4/"
                                      "static/%d/mask_len", intf, addr_idx);
                bail_null(masklen_bname);

                err = mdb_get_node_value_ipv4addr_bnipv4(commit, new_db, addr_bname,
                                                  0, &found, &ipv4_addr);
                bail_error(err);
                bail_require(found);

                err = mdb_get_node_value_uint8(commit, new_db, masklen_bname,
                                               0, &found, &masklen);
                bail_error(err);
                bail_require(found);

                err = md_interface_validate_ipv4addr_masklen(commit, intf,
                                                             ipv4_addr,
                                                             masklen,
                                                             &is_valid);
                bail_error(err);
                if (!is_valid) {
                    goto bail;
                }
            }
        }

        /* IPv4 alias interface name validity */
        if (bn_binding_name_parts_pattern_match_va
            (change->mdc_name_parts, 0, 6,
             "net", "interface", "config", "*", "alias", "ifdevname")) {
            if (change->mdc_change_type != mdct_delete) {
                safe_free(alias_ifdevname);
                if (change->mdc_new_attribs) {
                    err = bn_attrib_array_get
                        (change->mdc_new_attribs, ba_value, &attrib);
                    bail_error(err);
                    if (attrib) {
                        err = bn_attrib_get_str(attrib, NULL, bt_string, NULL,
                                                &alias_ifdevname);
                        bail_error(err);

                        if (alias_ifdevname &&
                            (strlen(alias_ifdevname) > 0)) {
                            err = lc_ifname_validate(alias_ifdevname, 
                                                     &intf_ok);
                            bail_error(err);

                            if (!intf_ok) {
                                err = md_commit_set_return_status_msg_fmt
                                    (commit, 1,
                                     _("The interface name %s for alias is "
                                       "not valid."), alias_ifdevname);
                                bail_error(err);
                                goto bail;
                            }
                        }
                    }
                }
            }
        }

        /* IPv4 alias index validity */
        if (bn_binding_name_parts_pattern_match_va
            (change->mdc_name_parts, 0, 6,
             "net", "interface", "config", "*", "alias", "index")) {
            if (change->mdc_change_type != mdct_delete) {
                safe_free(alias_index);
                if (change->mdc_new_attribs) {
                    err = bn_attrib_array_get
                        (change->mdc_new_attribs, ba_value, &attrib);
                    bail_error(err);
                    if (attrib) {
                        err = bn_attrib_get_str(attrib, NULL, bt_string, NULL,
                                                &alias_index);
                        bail_error(err);

                        if (alias_index && (strlen(alias_index) > 0)) {
                            err = lc_ifalias_index_validate(alias_index,
                                                            &intf_ok);
                            bail_error(err);
                            if (!intf_ok) {
                                err = md_commit_set_return_status_msg_fmt
                                    (commit, 1,
                                     _("The index %s for alias is not "
                                       "valid."), alias_index);
                                bail_error(err);
                                goto bail;
                            }
                        }
                    }
                }
            }
        }

        /* ========== IPv6 tests */

        /* Static address / masklen validity */
        if (bn_binding_name_parts_pattern_match_va
            (change->mdc_name_parts, 0, 9, "net", "interface", "config", "*",
             "addr", "ipv6", "static", "*", "mask_len") ||
            bn_binding_name_parts_pattern_match_va
            (change->mdc_name_parts, 0, 9, "net", "interface", "config", "*",
             "addr", "ipv6", "static", "*", "ip")) {
            if (change->mdc_change_type != mdct_delete) {
                bail_null(change->mdc_new_attribs);
                intf = tstr_array_get_str_quick(change->mdc_name_parts, 3);
                bail_null(intf);
                addr_idx = atoi(tstr_array_get_str_quick
                                (change->mdc_name_parts, 7));
                safe_free(addr_bname);
                addr_bname = smprintf("/net/interface/config/%s/addr/ipv6/"
                                      "static/%d/ip", intf, addr_idx);
                bail_null(addr_bname);
                safe_free(masklen_bname);
                masklen_bname = smprintf("/net/interface/config/%s/addr/ipv6/"
                                      "static/%d/mask_len", intf, addr_idx);
                bail_null(masklen_bname);

                err = mdb_get_node_value_ipv6addr(commit, new_db, addr_bname,
                                                  0, &found, &ipv6_addr);
                bail_error(err);
                bail_require(found);

                err = mdb_get_node_value_uint8(commit, new_db, masklen_bname,
                                               0, &found, &masklen);
                bail_error(err);
                bail_require(found);

                err = md_interface_validate_ipv6addr_masklen(commit, intf,
                                                             ipv6_addr,
                                                             masklen,
                                                             &is_valid);
                bail_error(err);
                if (!is_valid) {
                    goto bail;
                }

                if (!strcmp(intf, "lo")) {
                    if (!lia_ipv6addr_is_loopback_quick(&ipv6_addr) ||
                        masklen != 128) {

                        err = md_commit_set_return_status_msg
                            (commit, 1, _("Modifications to the loopback interface are "
                                          "not permitted."));
                        bail_error(err);
                    }
                }
            }
        }

    } /* walking the change list */


    /*
     * IPv4 alias interface check, to make sure zeroconf and DHCP are
     * not turned on for it.
     */
    err = tstr_array_new(&intf_names, NULL);
    bail_error(err);

    err = mdb_iterate_binding(commit, new_db, "/net/interface/config",
                              0, &ba);
    bail_error(err);

    err = bn_binding_array_length(ba, &num_bindings);
    bail_error(err);

    for (i = 0; i < num_bindings; ++i) {
        err = bn_binding_array_get(ba, i, &binding);
        bail_error(err);

        safe_free(name);
        err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
                                 &name);
        bail_error(err);

        err = tstr_array_append_str(intf_names, name);
        bail_error(err);

        bn_binding_free(&tbinding);
        /* Should probably make sure subindex for alias was set as well */
        err = mdb_get_node_binding_fmt(commit, new_db, 0, &tbinding, 
                                    "/net/interface/config/%s/alias/ifdevname",
                                    name);
        bail_error(err);
        safe_free(alias_ifdevname);
        if (tbinding) {
            err = bn_binding_get_str(tbinding, ba_value, bt_string, NULL,
                                     &alias_ifdevname);
            bail_error(err);
        }

        if (alias_ifdevname && (strlen(alias_ifdevname) > 0)) {
            bn_binding_free(&tbinding);
            err = mdb_get_node_binding_fmt(commit, new_db, 0, &tbinding, 
                                     "/net/interface/config/%s/addr/ipv4/dhcp",
                                     name);
            bail_error(err);
            dhcpv4 = false;
            if (tbinding) {
                err = bn_binding_get_tbool(tbinding, ba_value, NULL, &dhcpv4);
                bail_error(err);
            }

            bn_binding_free(&tbinding);
            err = mdb_get_node_binding_fmt(commit, new_db, 0, &tbinding, 
                                 "/net/interface/config/%s/addr/ipv4/zeroconf",
                                 name);
            bail_error(err);
            zeroconf = false;
            if (tbinding) {
                err = bn_binding_get_tbool(tbinding, ba_value, NULL,
                                           &zeroconf);
                bail_error(err);
            }

            if (dhcpv4 || zeroconf) {
                err = md_commit_set_return_status_msg_fmt
                  (commit, 1, _("Alias IP address on interface %s cannot"
                                " have DHCP or zeroconf enabled"), name);
                bail_error(err);
            }
        }
    }

    /* IPv4 duplicate address check */
    err = md_get_ipv4_static_addrs(commit, new_db, intf_names, &ip_addrs,
                                   &dup_ip_addr);
    bail_error(err);

    if (dup_ip_addr && ts_length(dup_ip_addr) > 0) {
            err = md_commit_set_return_status_msg_fmt
                  (commit, 1, _("Duplicate static IP address %s not allowed."),
                   ts_str(dup_ip_addr));
            bail_error(err);
    }

    /* IPv6 duplicate address check */
    err = md_get_ipv6_static_addrs(commit, new_db, intf_names,
                                   &ipv6_addrs, &dup_ipv6_addr);
    bail_error(err);

    if (dup_ipv6_addr && ts_length(dup_ipv6_addr) > 0) {
            err = md_commit_set_return_status_msg_fmt
                (commit, 1, _("Duplicate static IPv6 address %s not allowed."),
                 ts_str(dup_ipv6_addr));
            bail_error(err);
    }

    memset(&ctxt, 0, sizeof(ctxt));
    ctxt.micc_commit = commit;
    ctxt.micc_db = new_db;

    err = mdb_db_change_array_foreach(change_list,
                                      md_interface_check_change, &ctxt);
    bail_error(err);

 bail:
    safe_free(addr_bname);
    safe_free(masklen_bname);
    bn_binding_free(&tbinding);
    bn_binding_array_free(&ba);
    safe_free(name);
    safe_free(alias_ifdevname);
    safe_free(alias_index);
    tstr_array_free(&intf_names);
    tstr_array_free(&ip_addrs);
    ts_free(&dup_ip_addr);
    tstr_array_free(&ipv6_addrs);
    ts_free(&dup_ipv6_addr);
    safe_free(addr_str);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_check_change(const mdb_db_change_array *arr, uint32 idx,
                          mdb_db_change *change, void *data)
{
    int err = 0;
    md_if_config_check_change_context *ctxt = data;
    md_commit *commit = NULL;
    const mdb_db *db = NULL;
    tstring *value = NULL;
    uint32 mtu = 0;
    tbool check_mtu = true;
    const char *ifname = NULL;
    uint32 check_min_mtu = 0, check_max_mtu = 0;
    char *bname = NULL;
    tstr_array *speeds = NULL;
    tstring *speeds_str = NULL;
    int i = 0, num_speeds = 0;

    bail_null(ctxt);
    commit = ctxt->micc_commit;
    bail_null(commit);
    db = ctxt->micc_db;
    bail_null(db);

    bail_null(change);

    if (!ts_has_prefix_str(change->mdc_name, "/net/", false)) {
        goto bail;
    }

/* ========================================================================= */
/* Customer-specific graft point 4: per-node check overrides.
 * Enforce your validation and/or override the Samara default validation,
 * e.g. for speed and duplex for non-Ethernet interfaces.
 * 
 * If after performing your checks, you do not want us to perform
 * ours, then 'goto bail'.  This function is called once for every
 * change in the change list.
 * =========================================================================
 */
#ifdef INC_MD_IF_CONFIG_INC_GRAFT_POINT
#undef MD_IF_CONFIG_INC_GRAFT_POINT
#define MD_IF_CONFIG_INC_GRAFT_POINT 4
#include "../bin/mgmtd/modules/md_if_config.inc.c"

#endif /* INC_MD_IF_CONFIG_INC_GRAFT_POINT */

    /* 
     * If node was deleted, there will be no new attributes
     */
    if (change->mdc_new_attribs) {
        err = bn_attrib_array_get_attrib_tstr
            (change->mdc_new_attribs, ba_value, NULL, &value);
        bail_error(err); /* Could be NULL for certain types */
    }

    /*
     * XXX/EMT: need a way to only set the message once.  As it is, if two
     * loopback interface bindings are modified, we get the message
     * twice.  I don't see a way to query to see if a code has already
     * been set, etc.
     */

    if (ts_has_prefix_str(change->mdc_name, "/net/interface/config/", false)) {
        ifname = tstr_array_get_str_quick(change->mdc_name_parts, 3);
        bail_null(ifname);
    }

    /*
     * Raise a fuss if anyone's trying to modify the loopback interface,
     * other than the comment, which is harmless.  For IPv6 we've
     * already dealt with the static IPv6 address case above.
     *
     * The reason this is sufficient for lo / IPv4 is that only one
     * address is allowed per interface, and it already has 127.0.0.1 on
     * it.  For lo / IPv6, we check static addresses above, and fail
     * everything else here.
     */
    if (bn_binding_name_parts_pattern_match_va
        (change->mdc_name_parts, 0, 5,
         "net", "interface", "config", "lo", "**") &&
        !ts_equal_str(change->mdc_name, "/net/interface/config/lo/comment",
                      false) &&
        (change->mdc_change_type != mdct_add) &&
        !bn_binding_name_parts_pattern_match_va
        (change->mdc_name_parts, 0, 8,
         "net", "interface", "config", "lo", "addr", "ipv6", "static", "**")) {
        err = md_commit_set_return_status_msg
            (commit, 1, _("Modifications to the loopback interface are "
                          "not permitted."));
        bail_error(err);
    }
    
    /*
     * If the node was deleted, there is nothing to check for validity.
     */    
    if (change->mdc_change_type == mdct_delete) {
        goto bail;
    }
    
    /*
     * We cannot limit the speed based on the actual supported speeds,
     * since that may change, causing a previously-valid db to not
     * validate.  But the message we give can be a little more
     * helpful, by listing the ones currently supported on this
     * interface.
     *
     * XXX/EMT: these are standard checks of a node against a list of 
     * strings; infrastructure should support this.  It currently lets
     * you specify a list of values, but the message it prints is
     * not user-friendly enough.
     */
    if (bn_binding_name_parts_pattern_match_va
        (change->mdc_name_parts, 0, 7,
         "net", "interface", "config", "*", "type", "ethernet", "speed")) {
        if (!ts_equal_str(value, "10", false) &&
            !ts_equal_str(value, "100", false) &&
            !ts_equal_str(value, "1000", false) &&
            !ts_equal_str(value, "10000", false) &&
            !ts_equal_str(value, "auto", false)) {
            bname = smprintf
                ("/net/interface/state/%s/type/ethernet/supported/speeds",
                 ifname);
            bail_null(bname);

            err = mdb_get_tstr_array(commit, db, bname, NULL, false, 0,
                                     &speeds);
            bail_error(err);
            err = ts_new(&speeds_str);
            num_speeds = tstr_array_length_quick(speeds);
            for (i = 0; i < num_speeds; ++i) {
                err = ts_append_sprintf(speeds_str, "%s, ", 
                                        tstr_array_get_str_quick(speeds, i));
                bail_error(err);
            }

            err = md_commit_set_return_status_msg_fmt
                (commit, 1, _("Invalid speed %s on interface %s.\n"
                              "  Valid options in general are "
                              "10, 100, 1000, 10000, or %s.\n"
                              "  This interface currently supports %sor %s."),
                 ts_str(value), ifname, "auto", ts_str(speeds_str), "auto");
            bail_error(err);
        }
    }
    
    if (bn_binding_name_parts_pattern_match_va
        (change->mdc_name_parts, 0, 7, 
         "net", "interface", "config", "*", "type", "ethernet", "duplex")) {
        if (!ts_equal_str(value, "half", false) &&
            !ts_equal_str(value, "full", false) &&
            !ts_equal_str(value, "auto", false)) {
            err = md_commit_set_return_status_msg_fmt
                (commit, 1, _("Invalid duplex %s on interface %s; "
                              "options are %s, %s, or %s"), ts_str(value),
                 ifname, "half", "full", "auto");
            bail_error(err);
        }
    }

    /*
     * Don't check MTU for loopback.  We know it's set to what we want
     * and we don't let users change it, and the rules we apply to
     * other interfaces don't apply to it anyway.
     */
    if (bn_binding_name_parts_pattern_match_va
        (change->mdc_name_parts, 0, 5,
         "net", "interface", "config", "*", "mtu")) {

        check_mtu = !ts_equal_str(change->mdc_name,
                                   "/net/interface/config/lo/mtu", false);

        check_min_mtu = md_ifc_ether_min_mtu;
        check_max_mtu = md_ifc_ether_max_mtu;
        if (safe_strcmp(ifname, "ib0") == 0) {
            check_min_mtu = md_ifc_infiniband_min_mtu;
            check_max_mtu = md_ifc_infiniband_max_mtu;
        }

/* ========================================================================= */
/* Customer-specific graft point 1: allow MTU checks
 * 
 * Customers may set: check_max_mtu, check_min_mtu and check_mtu .
 *
 * =========================================================================
 */
#ifdef INC_MD_IF_CONFIG_INC_GRAFT_POINT
#undef MD_IF_CONFIG_INC_GRAFT_POINT
#define MD_IF_CONFIG_INC_GRAFT_POINT 1
#include "../bin/mgmtd/modules/md_if_config.inc.c"
#endif /* INC_MD_IF_CONFIG_INC_GRAFT_POINT */

        if (check_mtu) {
            mtu = atoi(ts_str(value));

            /* Enforce the limits */
            if (mtu < check_min_mtu) {
                err = md_commit_set_return_status_msg_fmt
                    (commit, 1, _("MTU %u for interface %s is out of range; "
                                  "must be at least %d"),
                     mtu, ifname, check_min_mtu);
                bail_error(err);
            }
            else if (mtu > check_max_mtu) {
                err = md_commit_set_return_status_msg_fmt
                    (commit, 1, _("MTU %u for interface %s is out of range; "
                                  "must be no greater than %d"),
                     mtu, ifname, check_max_mtu);
                bail_error(err);
            }
        }
    }

 bail:
    ts_free(&value);
    safe_free(bname);
    tstr_array_free(&speeds);
    ts_free(&speeds_str);
    return(err);
}


typedef struct md_interface_change_state {
    md_commit *mics_commit;
    const mdb_db *mics_olddb;
    const mdb_db *mics_newdb;

    /*
     * Contains names of interfaces on which duplex and speed have been
     * set (using ethtool), and on which IP address, netmask, and
     * broadcast address have been set, respectively.
     */
    tstr_array *mics_duplex_speed_set;
    tstr_array *mics_address_set;
} md_interface_change_state;


static int
md_interface_apply_ipv6(md_commit *commit,
                        const char *match_ifname,
                        const mdb_db *olddb,
                        const mdb_db *newdb)
{
    int err = 0;
    md_if_addr_rec_array *kernel_addrs = NULL, *want_addrs = NULL;
    md_if_addr_rec mar;
    md_if_addr_rec *curr_addr_rec = NULL;
    uint32 i = 0, num_sis = 0;
    tstr_array *si_nodes = NULL;
    const char *si_node = NULL;
    char *ssn = NULL;
    char *msn = NULL;
    uint32 fidx = 0;
    int failed = 0;
    tstring *ifname = NULL;
    uint32 num_addrs = 0;
    bn_ipv6addr addr = lia_ipv6addr_any;
    tbool ipv6_enabled = false, ipv6_supported = false;
    tbool found = false, if_enabled = false, if_ipv6_enabled = false;
    tbool exists = false;
    tbool admin_enabled = false;
    tbool is_bridge_member = false, is_bond_member = false;
    tbool is_alias = false;
    tbool add_ipv6_loopback = true;
    char *ifns = NULL;
    tstr_array *tokens = NULL;
    uint32 num_aliases = 0;
    const char *a_ifname = NULL, *a_ip = NULL, *a_masklen = NULL;

    /* First, the big picture: is IPv6 disabled or unsupported altogether? */
    err = mdb_get_node_value_tbool(commit, newdb,
                                   "/net/general/config/ipv6/enable",
                                   0, NULL, &ipv6_enabled);
    bail_error(err);
    ipv6_supported = md_proto_ipv6_supported();

    if (!ipv6_enabled || !ipv6_supported) {
#ifndef PROD_FEATURE_IPV6
        if (!md_proto_ipv6_avail()) {
            goto bail;
        }
#endif
        if (!match_ifname) {
            lc_log_basic(LOG_INFO,
                         "IPv6 is currently disabled, "
                         "not applying interface configuration");
        }
        goto bail;
    }

    lc_log_basic(LOG_INFO, "Applying IPv6 addresses to %s",
                 !match_ifname ? ":ALL:" : match_ifname);

    /*
     * XXXXX NOTE: this function will soon be called from
     * md_interface_reapply_config() after an interface override has
     * been cleared.
     */

    /* XXXXXXXXX should only do this if some networking config has changed like:
     * - bridging (which never does reapply!)
     * - bonding
     * - vlan
     * - interface
     *
     * XXXXXXXXXX The "real" solution is to make the reapply config
     * operation deal with IPv6 statics.
     */

    /*
     * Get the list of interface/IPv6 address pairs that the kernel has.
     */
    err = md_interface_read_ipv6_if_addr_info(match_ifname, &kernel_addrs);
    bail_error(err);

    err = md_if_addr_rec_array_new(&want_addrs);
    bail_error(err);

    /*
     * Walk each configured interface, and build up our "want" list of
     * IPv6 addresses.
     */
    bail_require(!match_ifname || strchr(match_ifname, '/') == NULL);
    safe_free(msn);
    msn = smprintf("/net/interface/config/%s/addr/ipv6/static/*",
                   !match_ifname ? "*" : match_ifname);
    bail_null(msn);
    err = mdb_get_matching_tstr_array
        (NULL, newdb, msn, 0, &si_nodes);
    bail_error(err);
    num_sis = tstr_array_length_quick(si_nodes);
    for (i = 0; i < num_sis; ++i) {
        si_node = tstr_array_get_str_quick(si_nodes, i);
        bail_null(si_node);

        ts_free(&ifname);
        err = bn_binding_name_to_parts_va(si_node, false, 1, 3, &ifname);
        bail_error(err);

        /*
         * See if the entire interface is disabled, or if IPv6 is
         * disabled on this interface, and if so, skip it.
         */
        safe_free(ifns);
        ifns = smprintf("/net/interface/config/%s/enable",
                        ts_str(ifname));
        err = mdb_get_node_value_tbool(commit, newdb,
                                       ifns,
                                       0, &found, &if_enabled);
        bail_error(err);
        if (!found || !if_enabled) {
            continue;
        }
        safe_free(ifns);
        ifns = smprintf("/net/interface/config/%s/addr/ipv6/enable",
                        ts_str(ifname));
        err = mdb_get_node_value_tbool(commit, newdb,
                                       ifns,
                                       0, &found, &if_ipv6_enabled);
        bail_error(err);
        if (!found || !if_ipv6_enabled) {
            continue;
        }

        /*
         * Also skip the interface if it is configured to be a member of
         * a bridge or bond, or is an alias (aliases should not get here).
         */
        err = md_interface_get_config_by_ifname
            (ts_str(ifname), commit, newdb, &found, &admin_enabled, NULL,
             NULL, NULL, NULL, NULL,
             &is_bridge_member, &is_bond_member, &is_alias, NULL);
        bail_error(err);

        if (found && (!admin_enabled || is_bridge_member ||
                      is_bond_member || is_alias)) {
            continue;
        }

        /* Do not "want" addresses on non-existent interfaces */
        err = md_interface_exists(ts_str(ifname), &exists);
        if (err || !exists) {
            err = 0;
            continue;
        }

        memset(&mar, 0, sizeof(mar));

        err = safe_strlcpy(mar.mar_ifname, ts_str(ifname));
        bail_error(err);

        safe_free(ssn);
        ssn = smprintf("%s/ip", si_node);
        err = mdb_get_node_value_ipv6addr_inetaddr
            (NULL, newdb, ssn, 0, NULL,
             &(mar.mar_addr));
        bail_error(err);

        safe_free(ssn);
        ssn = smprintf("%s/mask_len", si_node);
        err = mdb_get_node_value_uint8
            (NULL, newdb, ssn, 0, NULL,
             &(mar.mar_masklen));
        bail_error(err);

        /*
         * Make it obvious we expect this 0.  We zero the kernel
         * ifindex's below to match this.
         */
        mar.mar_ifindex = 0;
        mar.mar_scope = 0;
        mar.mar_flags = 0;

        err = md_if_addr_rec_array_append(want_addrs, &mar);
        bail_error(err);
    }

    /* Now append alias IPv6 addresses to the "want" list */
    num_aliases = tstr_array_length_quick(md_interface_ipv6_aliases);
    for (i = 0; i < num_aliases; i++) {
        tstr_array_free(&tokens);
        err = ts_tokenize(tstr_array_get_quick(md_interface_ipv6_aliases, i),
                          ',', '\0', '\0', 0, &tokens);
        bail_error_null(err, tokens);

        bail_require(tstr_array_length_quick(tokens) == 4);

        a_ifname = tstr_array_get_str_quick(tokens, 0);
        a_ip = tstr_array_get_str_quick(tokens, 1);
        a_masklen = tstr_array_get_str_quick(tokens, 2);

        if (match_ifname && safe_strcmp(a_ifname, match_ifname)) {
            continue;
        }

        if (!a_ifname) {
            continue;
        }
        
        /* ....................................................................
         * Apply the same set of filters we did for static addresses
         * above.  The interface must be:
         *   - Enabled
         *   - IPv6-enabled
         *   - Not a member of a bridge or bond
         *   - Not an alias
         *   - Detected on system
         *
         * Note that all of this is per-interface, not per-address, so
         * it could be checked at a higher level to avoid redundant
         * rechecks.  However, note the "*" used in the query to get
         * addresses: if match_ifname is NULL, we are looking at the 
         * list of all addresses on ALL interfaces.  So it's simpler
         * just to check it from in here.
         */
        safe_free(ifns);
        ifns = smprintf("/net/interface/config/%s/addr/ipv6/enable",
                        a_ifname);
        bail_null(ifns);
        err = mdb_get_node_value_tbool(commit, newdb, ifns,
                                       0, &found, &if_ipv6_enabled);
        bail_error(err);
        if (!found || !if_ipv6_enabled) {
            continue;
        }

        err = md_interface_get_config_by_ifname
            (a_ifname, commit, newdb, &found, &admin_enabled, NULL,
             NULL, NULL, NULL, NULL,
             &is_bridge_member, &is_bond_member, &is_alias, NULL);
        bail_error(err);

#if 0
        lc_log_basic(LOG_NOTICE, "Sanity check for interface '%s': "
                     "is_bridge_member = %s, "
                     "is_bond_member = %s, "
                     "is_alias = %s",
                     a_ifname,
                     lc_bool_to_str(is_bridge_member),
                     lc_bool_to_str(is_bond_member),
                     lc_bool_to_str(is_alias));
#endif

        if (found && (!admin_enabled || is_bridge_member ||
                      is_bond_member || is_alias)) {
            continue;
        }

        err = md_interface_exists(a_ifname, &exists);
        if (err || !exists) {
            err = 0;
            continue;
        }

        /* ....................................................................
         * Ok, this interface/address combo is good.
         */
        memset(&mar, 0, sizeof(mar));

        err = safe_strlcpy(mar.mar_ifname, a_ifname);
        bail_error(err);
        err = lia_str_to_inetaddr_ex(a_ip, lacf_accept_ipv6_only,
                                     &(mar.mar_addr));
        bail_error(err);
        err = lc_str_to_uint8(a_masklen, &(mar.mar_masklen));
        bail_error(err);

        /*
         * Make it obvious we expect this 0.  We zero the kernel
         * ifindex's below to match this.
         */
        mar.mar_ifindex = 0;
        mar.mar_scope = 0;
        mar.mar_flags = 0;

        err = md_if_addr_rec_array_append(want_addrs, &mar);
        bail_error(err);
    }

    /* Append ::1/128 on lo to the "want" list */
    if (add_ipv6_loopback) {
        memset(&mar, 0, sizeof(mar));

        err = safe_strlcpy(mar.mar_ifname, "lo");
        bail_error(err);
        err = lia_str_to_inetaddr_ex("::1", lacf_accept_ipv6_only,
                                     &(mar.mar_addr));
        bail_error(err);
        mar.mar_masklen = 128;

        /*
         * Make it obvious we expect this 0.  We zero the kernel
         * ifindex's below to match this.
         */
        mar.mar_ifindex = 0;
        mar.mar_scope = 0;
        mar.mar_flags = 0;

        err = md_if_addr_rec_array_append(want_addrs, &mar);
        bail_error(err);
    }

    /*
     * All the interfaces which are no longer configured have already
     * been down'd.  All SLAAC config has already been applied.  The
     * interface may not be marked UP (or it may), depending on IPv4
     * configuration.
     */

    /*
     * First, walk the kernel list, and delete any addrs not found in
     * config.  Note that we ignore some things we never add:
     *
     *  - non-permanent addresses
     *  - addresses of non-universe scope
     *
     * This ignoring allows SLAAC and loopback to be undisturbed.
     *
     * We also set if ifindex to 0 for all kernel records, as our 'want
     * list' does not set the ifindex, and we want lookups to work
     * properly.
     *
     */

    /* First, walk the running list and delete any addrs we do not want */
    num_addrs = md_if_addr_rec_array_length_quick(kernel_addrs);
    for (i = 0; i < num_addrs; i++) {
        curr_addr_rec = md_if_addr_rec_array_get_quick(kernel_addrs, i);
        bail_null(curr_addr_rec);

        /* So our lookups will work against the want list (ifindex==0) */
        curr_addr_rec->mar_ifindex = 0;

        if (lia_inetaddr_get_family_quick(&(curr_addr_rec->mar_addr)) !=
            AF_INET6) {
            continue;
        }
        if ((curr_addr_rec->mar_flags & IFA_F_PERMANENT) != IFA_F_PERMANENT) {
            continue;
        }
        if (curr_addr_rec->mar_scope != mast_universe) {
            continue;
        }

        /* Look up this addr in the "want" list */
        fidx = 0;
        err = md_if_addr_rec_array_best_search(want_addrs,
                                               curr_addr_rec, &fidx);
        if (err != lc_err_not_found) {
            bail_error(err);
        }
        else if (err == lc_err_not_found) {
            /* We'll have to delete this addr from the running system */
            err = lia_inetaddr_get_ipv6addr(&(curr_addr_rec->mar_addr),
                                            &addr);
            bail_error(err);
            err = md_interface_addr_ipv6_remove(commit, newdb,
                                                curr_addr_rec->mar_ifname,
                                                &addr,
                                                curr_addr_rec->mar_masklen);
            failed |= err;
            err = 0;
        }
    }

    /* Now walk the want list, and add any addrs we did not used to have */
    num_addrs = md_if_addr_rec_array_length_quick(want_addrs);
    for (i = 0; i < num_addrs; i++) {
        curr_addr_rec = md_if_addr_rec_array_get_quick(want_addrs, i);
        bail_null(curr_addr_rec);

        if (lia_inetaddr_get_family_quick(&(curr_addr_rec->mar_addr)) !=
            AF_INET6) {
            continue;
        }

        /* Look up this addr in the kernel list */
        fidx = 0;
        err = md_if_addr_rec_array_best_search(kernel_addrs,
                                               curr_addr_rec, &fidx);
        if (err != lc_err_not_found) {
            bail_error(err);
        }
        else if (err == lc_err_not_found) {
            /* We'll have to add this addr to the running system */
            err = lia_inetaddr_get_ipv6addr(&(curr_addr_rec->mar_addr),
                                            &addr);
            bail_error(err);
            err = md_interface_addr_ipv6_add(commit, newdb,
                                             curr_addr_rec->mar_ifname,
                                             &addr,
                                             curr_addr_rec->mar_masklen);

            failed |= err;
            err = 0;
        }
    }

 bail:
    md_if_addr_rec_array_free(&kernel_addrs);
    md_if_addr_rec_array_free(&want_addrs);
    tstr_array_free(&si_nodes);
    tstr_array_free(&tokens);
    safe_free(msn);
    ts_free(&ifname);
    safe_free(ssn);
    safe_free(ifns);
    return(err);
}


/*****************************************************************************/
/*****************************************************************************/
/*** COMMIT APPLY                                                          ***/
/*****************************************************************************/
/*****************************************************************************/

/* ------------------------------------------------------------------------- */
static int
md_interface_commit_apply(md_commit *commit,
                          const mdb_db *olddb, 
                          const mdb_db *newdb, 
                          mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    md_interface_change_state cs;
    tbool changed = false;

    memset(&cs, 0, sizeof(cs));
    bail_null(change_list);

    cs.mics_commit = commit;
    cs.mics_olddb = olddb;
    cs.mics_newdb = newdb;
    err = tstr_array_new(&(cs.mics_duplex_speed_set), NULL);
    bail_error(err);
    err = tstr_array_new(&(cs.mics_address_set), NULL);
    bail_error(err);

    /*
     * XXXX This is because currently reapply of config for bridging,
     * bonding and vlans is not fully correct, and so we rely on
     * md_interface_apply_ipv6() being called when any of those change.
     */
    err = md_find_changes(&changed, change_list, 1, "/net");
    bail_error(err);
    if (!changed) {
        goto bail;
    }

    /* This does all the link and IPv4 config */
    err = mdb_db_change_array_foreach(change_list, 
                                      md_interface_handle_change,
                                      (void *)&cs);
    bail_error(err);

    /* This does all the IPv6 static config */
    err = md_interface_apply_ipv6(commit, NULL, olddb, newdb);
    bail_error(err);

 bail:
    tstr_array_free(&(cs.mics_duplex_speed_set));
    tstr_array_free(&(cs.mics_address_set));
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_handle_change(const mdb_db_change_array *arr, uint32 idx,
                           mdb_db_change *change, void *data)
{
    int err = 0;
    const char *name = NULL;
    const tstr_array *name_parts = NULL;
    const mdb_db *old_db = NULL;
    const mdb_db *new_db = NULL;
    md_commit *commit = NULL;
    md_interface_change_state *cs = data;
    tstring *t_if_name = NULL;
    tbool found = false, enabled = false, overridden = false;
    tbool dhcpv4_enabled = false, dhcpv6_enabled = false;
    tbool release_lease = false;
    tbool zeroconf_enabled = false;
    tbool is_bridge_member = false, is_bond_member = false;
    tbool is_alias = false;
    char *ifname = NULL;
    bn_ipv4addr addr = lia_ipv4addr_any;
    char addr_buf[INET_ADDRSTRLEN];
    tbool speed_duplex_applied = false;
    uint8 masklen = 0;

    bail_null(change);
    name = ts_str(change->mdc_name);
    bail_null(name);
    bail_null(data);
    name_parts = change->mdc_name_parts;
    bail_null(name_parts);

    old_db = cs->mics_olddb;
    new_db = cs->mics_newdb;
    commit = cs->mics_commit;
    bail_null(new_db);

    /* There's no way we'll be interested in anything but these subtrees */
    if (!lc_is_prefix("/net/interface/config/", ts_str(change->mdc_name),
                      false) &&
        !lc_is_prefix("/net/general/config/", ts_str(change->mdc_name),
                      false)) {
        goto bail;
    }

    /* ........................................................................
     * Deal with deletion of an interface.
     */
    if (change->mdc_change_type == mdct_delete) {
        /*
         * If we're deleting a binding that happens to be the root of
         * an interface config tree, it is possible that it will not
         * be found in the new db. So for any delete, we down the interface
         * when we see the top level config binding.
         * XXX do we need to worry about if this is done in relation to
         * any of the interfaces related nodes (i.e. address?)
         */
        if (bn_binding_name_parts_pattern_match(name_parts, true, 
                                                "/net/interface/config/*")) {
            err = bn_binding_name_to_parts_va(name, false, 1, 3, &t_if_name);
            bail_error(err);

            /* Don't have the full config to know, so must embed colon
             * knowledge here... */
            is_alias = strchr(ts_str(t_if_name), ':') ? true : false;

            /* XXXX should pass the alias IP in here */
            err = md_interface_bring_down_full(commit, new_db,
                                               ts_str(t_if_name),
                                               false, is_alias, NULL);
            bail_error(err);

            /*
             * Make sure any related state arrays get cleaned up when
             * the interface is being deleted (which is treated as 'not
             * enabled').  This may clear overrides.
             */
            err = md_interface_check_dhcp(ts_str(t_if_name), commit, new_db,
                                          false, false, false, false, true);
            bail_error(err);

            err = md_interface_check_dhcp(ts_str(t_if_name), commit, new_db,
                                          true, false, false, false, true);
            bail_error(err);

            err = md_interface_check_zeroconf(ts_str(t_if_name), commit,
                                              new_db, false, false, false);
            bail_error(err);

            goto bail;
        }
    }

    /* ........................................................................
     * If this is a change to an interface, fetch some more info about it.
     * If not, or if the interface is not found, we have special handling.
     * It's OK to call this without knowing, since it verifies the name
     * begins with "/net/interface/config" and returns found == false
     * if not.
     */
    err = md_interface_get_config(name, name_parts, commit, new_db, 
                                  &found, &ifname, &enabled,
                                  &overridden,
                                  &addr, &masklen,
                                  &dhcpv4_enabled,
                                  &zeroconf_enabled, &is_bridge_member,
                                  /* &is_bond_member */ NULL, &is_alias,
                                  &dhcpv6_enabled);
    bail_error(err);

    /*
     * Either we somehow failed to find this interface; or more likely,
     * this was a change under some different tree like /net/general.
     */
    if (!found) {
        /* It is possible IPv6 was globally enabled or disabled */
        if (lc_is_prefix("/net/general/config/ipv6/", name, false)) {
            err = md_interface_apply_ipv6_kernel_config(commit, new_db,
                                                        NULL, false);
            bail_error(err);

            /*
             * e.g. enabling or disabling IPv6 overall cause us to
             * need to check (kill) DHCPv6 clients on all interfaces.
             */
            err = md_interface_check_dhcp(NULL, commit, new_db, true,
                                          false, false, false, true);
            bail_error(err);
        }

        goto bail;
    }

    /* ........................................................................
     * This is a change to an interface.
     */
    err = lia_ipv4addr_to_str_buf(&addr, addr_buf, sizeof(addr_buf));
    bail_error(err);

    /*
     * These are IPv6 interface enable/disable and kernel IPv6 settings.
     * The full IPv6 config is asserted each time we call.
     */
    if (bn_binding_name_parts_pattern_match
        (name_parts, true, "/net/interface/config/*/addr/ipv6/*")) {

        if (enabled) {
            err = md_interface_apply_ipv6_kernel_config(commit, new_db,
                                                        ifname, true);
            bail_error(err);

            /*
             * e.g. enabling or disabling IPv6 on an interface might
             * cause us to need to adjust the DHCPv6 client.
             */
            err = md_interface_check_dhcp(ifname, commit, new_db, true,
                                          dhcpv6_enabled, enabled, false,
                                          true);
            bail_error(err);
        }

        goto bail;
    }

    if (bn_binding_name_parts_pattern_match
        (name_parts, true, "/net/interface/config/*/addr/ipv6/slaac/**")) {

        if (enabled) {
            err = md_interface_apply_ipv6_kernel_config(commit, new_db,
                                                        ifname, false);
            bail_error(err);
        }

        goto bail;
    }

    if (bn_binding_name_parts_pattern_match
        (name_parts, true, "/net/interface/config/*/addr/ipv6/dhcp/**")) {
        /*
         * This is any modification to DHCPv6 config for this interface.
         * We only want to release the lease file if it's an overall
         * disable.  It happens that the 'enable' flag is the only thing
         * under here for now, but to be defensive, we'll check for this,
         * since if another node were added under here, we'd probably NOT
         * want to release the lease when it was changed.
         */
        release_lease = bn_binding_name_parts_pattern_match
            (name_parts, true,
             "/net/interface/config/*/addr/ipv6/dhcp/enable");
        err = md_interface_check_dhcp(ifname, commit, new_db, true,
                                      dhcpv6_enabled, enabled, false,
                                      release_lease);
        bail_error(err);
        goto bail;
    }

    if (bn_binding_name_parts_pattern_match
        (name_parts, true, "/net/interface/config/*/type/ethernet/speed") ||
        bn_binding_name_parts_pattern_match
        (name_parts, true, "/net/interface/config/*/type/ethernet/duplex")) {
        err = tstr_array_linear_search_str
            (cs->mics_duplex_speed_set, ifname, 0, NULL);
        if (err == lc_err_not_found) {
            err = tstr_array_append_str(cs->mics_duplex_speed_set, ifname);
            bail_error(err);

            speed_duplex_applied = false;

/* ========================================================================= */
/* Customer-specific graft point 5: override for applying speed and duplex.
 * If we get here, we have a speed or duplex binding for an interface for
 * which speed and duplex have not yet been set during this commit.
 * (We track those together because often, they are applied together.)
 *
 * You may read from the following variables:
 *   - ifname
 *   - change
 *   - new_db
 *   - commit (used for querying new_db)
 *
 * If you want to apply the speed/duplex to this interface yourself,
 * do so, and then set speed_duplex_applied to 'true'.  If you do not,
 * do nothing, and the base Samara apply logic will be used.
 * =========================================================================
 */
#ifdef INC_MD_IF_CONFIG_INC_GRAFT_POINT
#undef MD_IF_CONFIG_INC_GRAFT_POINT
#define MD_IF_CONFIG_INC_GRAFT_POINT 5
#include "../bin/mgmtd/modules/md_if_config.inc.c"

uint32_t i = 0;
tbool is_pacifica = false;
tstr_array *ret_lines = NULL;

err = lf_read_file_lines("/proc/cmdline", &ret_lines);
bail_error_null(err, ret_lines);

for(i = 0; i < tstr_array_length_quick(ret_lines); i++) {
    const char *line = tstr_array_get_str_quick(ret_lines, i);
    if(line && strstr(line, "model=pacifica"))
	is_pacifica = true;
}

tstr_array_free(&ret_lines);

if(ifname && is_pacifica &&
	(!strcmp(ifname, "eth10") ||
	 !strcmp(ifname, "eth11") ||
	 !strcmp(ifname, "eth12") ||
	 !strcmp(ifname, "eth13") ||
	 !strcmp(ifname, "eth20") ||
	 !strcmp(ifname, "eth21") ||
	 !strcmp(ifname, "eth22") ||
	 !strcmp(ifname, "eth23")) ) {

    tstring *speed_ts = NULL;
    err = mdb_get_node_attrib_tstr_fmt
	(commit, new_db, 0, ba_value, NULL, &speed_ts,
	 "/net/interface/config/%s/type/ethernet/speed", ifname);
    bail_error(err);
    bail_null(speed_ts);

    /* We dont support auot-neg for broadcom as of now */
    if(ts_equal_str(speed_ts, "auto", false))
	speed_duplex_applied = true;

    ts_free(&speed_ts);
} else {
    /* not pacifica, check if have virt-io interface */
    int if_virtio = 0;
    uint32_t ifname_idx = 0;
    err = lc_is_if_driver_virtio(ifname, md_ifc_sock, &if_virtio);
    bail_error(err);
    lc_log_basic(LOG_NOTICE, "applying values for: %s / %d", ifname, if_virtio);
    if (if_virtio == 1) {
	tstring *speed_ts = NULL;
	err = mdb_get_node_attrib_tstr_fmt
	    (commit, new_db, 0, ba_value, NULL, &speed_ts,
	     "/net/interface/config/%s/type/ethernet/speed", ifname);
	bail_error(err);
	bail_null(speed_ts);

	err = tstr_array_linear_search_str(md_ifc_vmfc_ifnames, ifname, 0,
		&ifname_idx);
	if (err != lc_err_not_found) {
	    /* if found, delete first */
	    err = tstr_array_delete(md_ifc_vmfc_ifspeed, ifname_idx);
	    bail_error(err);

	    err = tstr_array_delete(md_ifc_vmfc_ifnames, ifname_idx);
	    bail_error(err);
	}
	err = tstr_array_append_str(md_ifc_vmfc_ifspeed, ts_str_maybe_empty(speed_ts));
	/* free memory first, to avoid any leak in err cases */
	ts_free(&speed_ts);
	bail_error(err);

	err = tstr_array_append_str(md_ifc_vmfc_ifnames, ifname);
	bail_error(err);

	tstr_array_dump(md_ifc_vmfc_ifnames, "IF-NAME :");
	tstr_array_dump(md_ifc_vmfc_ifspeed, "IF-SPEED:");
	/* set the flag */
	speed_duplex_applied = true;
    }
}
#endif /* INC_MD_IF_CONFIG_INC_GRAFT_POINT */

            if (!speed_duplex_applied) {
                err = md_interface_sd_set(ifname, commit, new_db);
                bail_error(err);
            }
        }
        goto bail;
    }

    if (bn_binding_name_parts_pattern_match(name_parts, true, 
                                            "/net/interface/config/*/mtu")) {
        err = md_interface_mtu_set(ifname, true, commit, new_db);
        bail_error(err);
        /* XXXXX This is part of the partial workaround for bug 14015 */
        if (enabled) {
            err = md_interface_apply_ipv6_kernel_config(commit, new_db,
                                                        ifname, true);
            bail_error(err);
        }
        goto bail;
    }

    /*
     * XXX/EMT: note: the lettered cases below are references to a
     * document which is not yet checked in.  It should be...
     */

    if (change->mdc_change_type == mdct_delete) {
        /*
         * Case (b): a configured IP address and netmask was deleted
         */
        if (bn_binding_name_parts_pattern_match
            (name_parts, true, "/net/interface/config/*/addr/ipv4/static/*")) {
            if (enabled && !overridden) {
                err = md_interface_bring_down_full(commit, new_db, ifname,
                                                   true, is_alias, addr_buf);
                bail_error(err);
            }
        }
        /* Note that IPv6 statics are handled elsewhere */
    }

    else {
        bail_require(change->mdc_change_type == mdct_add ||
                     change->mdc_change_type == mdct_modify);

        if (enabled && change->mdc_change_type == mdct_modify &&
            bn_binding_name_parts_pattern_match
            (name_parts, true,
             "/net/interface/config/*/enslaved")) {
            if (!is_bridge_member) {
                /*
                 * Reapply the interface config, as we've just been
                 * taken out of a bridge group.
                 *

                 * XXXX This is not how this should be accomplished:
                 * instead the bridging module should call us
                 * explicitly.
                 */

                err = md_interface_reapply_config(ifname, commit, new_db, true);
                bail_error(err);
            }
            
        }

        /*
         * Case (a): a configured IP address or netmask was changed or
         * added.  Note: this will call ifconfig twice for each
         * change, once when it sees the new ip and once when it sees
         * the new masklen.  This could be prevented with some
         * additional effort.  Can't just look for one, since
         * one could be changed without the other.
         */
        else if (bn_binding_name_parts_pattern_match
            (name_parts, true, 
             "/net/interface/config/*/addr/ipv4/static/*/ip") ||
            bn_binding_name_parts_pattern_match
            (name_parts, true, 
             "/net/interface/config/*/addr/ipv4/static/*/mask_len")) {

            if (enabled) {
                if (!dhcpv4_enabled && !zeroconf_enabled) {
                    err = md_interface_maybe_set_ipv4_addr
                        (commit, new_db, cs->mics_address_set, ifname,
                         addr, masklen, is_alias, is_bridge_member);
                    bail_error(err);
                }
            }
        }
        /* Note that IPv6 statics are handled elsewhere */

        /*
         * Case (c): an interface was enabled.
         */
        else if (bn_binding_name_parts_pattern_match
                 (name_parts, true, "/net/interface/config/*/enable") &&
                 enabled == true) {
            if (!dhcpv4_enabled && !zeroconf_enabled) {
                err = md_interface_maybe_set_ipv4_addr
                    (commit, new_db, cs->mics_address_set, ifname, addr,
                     masklen, is_alias, is_bridge_member);
                bail_error(err);
            }
            else {
                /*
                 * We do not want to set an address, but we need the
                 * link to be "up" .
                 */
                err = md_interface_set_link_internal(commit, new_db, true,
                                                     ifname, true);
                bail_error(err);
            }
            err = md_interface_check_dhcp(ifname, commit, new_db, false,
                                          dhcpv4_enabled, enabled, false,
                                          true);
            bail_error(err);
            err = md_interface_check_dhcp(ifname, commit, new_db, true,
                                          dhcpv6_enabled, enabled, false,
                                          true);
            bail_error(err);

            err = md_interface_check_zeroconf(ifname, commit, new_db,
                                          zeroconf_enabled, enabled, false);
            bail_error(err);
        }

        /*
         * Case (d): an interface was disabled.
         */
        else if (bn_binding_name_parts_pattern_match
                 (name_parts, true, "/net/interface/config/*/enable") &&
                 enabled == false) {
            err = md_interface_check_dhcp(ifname, commit, new_db, false,
                                          dhcpv4_enabled, enabled, false,
                                          true);
            bail_error(err);

            err = md_interface_check_dhcp(ifname, commit, new_db, true,
                                          dhcpv6_enabled, enabled, false,
                                          true);
            bail_error(err);

            err = md_interface_check_zeroconf(ifname, commit, new_db,
                                          zeroconf_enabled, enabled, false);
            bail_error(err);

            err = md_interface_bring_down_full(commit, new_db, ifname,
                                               false, is_alias, addr_buf);
            bail_error(err);
        }

        /*
         * Case (e): DHCPv4 was enabled on an interface
         *
         * (Note: enabling of DHCPv6 is detected above, when we're
         * checking for ".../addr/ipv6/dhcp/ **")
         */
        else if (bn_binding_name_parts_pattern_match
                 (name_parts, true, 
                  "/net/interface/config/*/addr/ipv4/dhcp") &&
                 dhcpv4_enabled == true) {
            if (enabled) {
                err = md_interface_check_dhcp(ifname, commit, new_db, false,
                                              dhcpv4_enabled, enabled, false,
                                              true);
                bail_error(err);
            }
        }

        /*
         * Case (f): DHCPv4 was disabled on an interface
         *
         * (Note: disabling of DHCPv6 is detected above, when we're
         * checking for ".../addr/ipv6/dhcp/ **")
         */
        else if (bn_binding_name_parts_pattern_match
                 (name_parts, true, 
                  "/net/interface/config/*/addr/ipv4/dhcp") &&
                 dhcpv4_enabled == false) {
            if (enabled) {
                err = md_interface_check_dhcp(ifname, commit, new_db, false,
                                              dhcpv4_enabled, enabled, false,
                                              true);
                bail_error(err);

                if (!zeroconf_enabled) {
                    err = md_interface_maybe_set_ipv4_addr
                        (commit, new_db, cs->mics_address_set, ifname,
                         addr, masklen, is_alias, is_bridge_member);
                    bail_error(err);
                    
                    /*
                     * XXX/EMT: if the address is NULL,
                     * md_interface_maybe_set_ipv4_addr() just does
                     * "ifconfig <ifname> up".  This does not seem
                     * right, but we don't want to change it there, in
                     * case something else relies on that...
                     */
                    if (lia_ipv4addr_is_zero_quick(&addr)) {
                        err = md_interface_bring_down_full(commit, new_db,
                                                           ifname,
                                                           true,
                                                           is_alias, addr_buf);
                        bail_error(err);
                    }
                }
            }
        }

        /*
         * Case (g): zeroconf was enabled on an interface
         */
        else if (bn_binding_name_parts_pattern_match
                 (name_parts, true, 
                  "/net/interface/config/*/addr/ipv4/zeroconf") &&
                 zeroconf_enabled == true) {
            if (enabled) {
                err = md_interface_check_zeroconf(ifname, commit, new_db,
                                              zeroconf_enabled, enabled, false);
                bail_error(err);
            }
        }

        /*
         * Case (h): zeroconf was disabled on an interface
         */
        else if (bn_binding_name_parts_pattern_match
                 (name_parts, true, 
                  "/net/interface/config/*/addr/ipv4/zeroconf") &&
                 zeroconf_enabled == false) {
            if (enabled) {
                err = md_interface_check_zeroconf(ifname, commit, new_db,
                                              zeroconf_enabled, enabled, false);
                bail_error(err);

                if (!dhcpv4_enabled) {
                    err = md_interface_maybe_set_ipv4_addr
                        (commit, new_db, cs->mics_address_set, ifname,
                         addr, masklen, is_alias, is_bridge_member);
                    bail_error(err);

                    if (lia_ipv4addr_is_zero_quick(&addr)) {
                        err = md_interface_bring_down_full(commit, new_db,
                                                           ifname,
                                                           true,
                                                           is_alias, addr_buf);
                        bail_error(err);
                    }
                }
            }
        }
    }

 bail:
    safe_free(ifname);
    ts_free(&t_if_name);
    return(err);
}


/*
 * If 'commit' is not NULL, error messages will go to it.
 *
 */
static int
md_interface_validate_ipv4addr_masklen(md_commit *commit,
                                       const char *intf,
                                       bn_ipv4addr ip, uint8 masklen,
                                       tbool *ret_is_valid)
{
    int err = 0;
    tbool is_valid = false;
    char msg_part[128];

    /*
     * Ignore (pass as valid) loopback changes, as we'll address those
     * elsewhere.
     */
    if (!safe_strcmp(intf, "lo")) {
        is_valid = true;
        goto bail;
    }

    snprintf(msg_part, sizeof(msg_part), " for interface %s", intf);

    err = md_net_validate_ipv4addr_masklen(commit, msg_part,
                                           ip, masklen,
                                           lia_addrtype_match_ifaddr_ll_maybe,
                                           &is_valid);
    if (err || !is_valid) {
        err = 0;
        is_valid = false;
        /* Ensure on error we fail the commit */
        if (commit) {
            err = md_commit_set_return_status(commit, 1);
            bail_error(err);
        }
        goto bail;
    }

 bail:
    if (ret_is_valid) {
        *ret_is_valid = is_valid;
    }
    return(err);
}

static int
md_interface_validate_ipv6addr_masklen(md_commit *commit,
                                       const char *intf,
                                       bn_ipv6addr ip, uint8 masklen,
                                       tbool *ret_is_valid)
{
    int err = 0;
    tbool is_valid = false;
    char msg_part[128];

    if (!safe_strcmp(intf, "lo") &&
        lia_ipv6addr_is_loopback_quick(&ip)) {
        is_valid = true;
        goto bail;
    }

    snprintf(msg_part, sizeof(msg_part), " for interface %s", intf);

    err = md_net_validate_ipv6addr_masklen(commit, msg_part,
                                           ip, masklen,
                                           lia_addrtype_match_ifaddr,
                                           &is_valid);
    if (err || !is_valid) {
        err = 0;
        is_valid = false;
        /* Ensure on error we fail the commit */
        if (commit) {
            err = md_commit_set_return_status(commit, 1);
            bail_error(err);
        }
        goto bail;
    }

 bail:
    if (ret_is_valid) {
        *ret_is_valid = is_valid;
    }
    return(err);
}

/*****************************************************************************/
/*****************************************************************************/
/*** ACTION HANDLERS                                                       ***/
/*****************************************************************************/
/*****************************************************************************/

/* ------------------------------------------------------------------------- */
static int
md_interface_override_handle_set(md_commit *commit, const mdb_db *db,
                                 const char *action_name,
                                 bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    tstring *ifname = NULL;
    const bn_binding *enabled_binding = NULL;
    const bn_binding *ip_binding = NULL, *netmask_binding = NULL;
    tbool found = false, enabled = false;
    bn_ipv4addr ip = lia_ipv4addr_any, netmask = lia_ipv4addr_any;
    uint8 masklen = 0;
    tbool is_valid = false;

    err = bn_binding_array_get_value_tstr_by_name
        (params, "ifname", NULL, &ifname);
    bail_error_null(err, ifname);

    if (strchr(ts_str(ifname), ':')) {
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, _("Override interface name may not be an alias"));
        bail_error(err);
        goto bail;
    }

    /*
     * If this interface is not enabled, stop here.  This should only
     * come up in a race condition, since the DHCP client should not
     * be running if the interface is disabled.
     */
    err = md_interface_get_config_by_ifname
        (ts_str(ifname), commit, db, &found, &enabled, 
         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    bail_error(err);

    if (!found || !enabled) {
        goto bail;
    }

    err = bn_binding_array_get_binding_by_name
        (params, "ip", &ip_binding);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name
        (params, "netmask", &netmask_binding);
    bail_error(err);

    if ((ip_binding == NULL) != (netmask_binding == NULL)) {
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, _("Address and netmask cannot be set separately"));
        bail_error(err);
        goto bail;
    }
    /* Check if ip and netmask are consistent */
    if (ip_binding) {
        err = bn_binding_get_ipv4addr_bnipv4(ip_binding, ba_value, 0, &ip);
        bail_error(err);

        err = bn_binding_get_ipv4addr_bnipv4(netmask_binding, ba_value, 0, &netmask);
        bail_error(err);

        err = lia_ipv4addr_to_masklen(&netmask, &masklen);
        if (err) {
            err = 0;
            err = md_commit_set_return_status_msg_fmt
                (commit, 1, _("Netmask is invalid"));
            bail_error(err);
            goto bail;
        }

        err = md_interface_validate_ipv4addr_masklen(commit, ts_str(ifname),
                                                     ip, masklen,
                                                     &is_valid);
        bail_error(err);
        if (!is_valid) {
            goto bail;
        }
    }

    err = bn_binding_array_get_binding_by_name
        (params, "enabled", &enabled_binding);
    bail_error_null(err, enabled_binding);

    err = bn_binding_get_tbool(enabled_binding, ba_value, NULL, &enabled);
    bail_error(err);

    err = md_interface_override_set(ts_str(ifname), true);
    bail_error(err);

    if (enabled) {
        if (ip_binding) {
            err = md_interface_set_ipv4_addr(commit, db, ts_str(ifname),
                                             ip, masklen,
                                             false, false);
            bail_error(err);
        }
        else {
            /* No IP, just bring up the interface */
            err = md_interface_bring_up(commit, db, ts_str(ifname));
            bail_error(err);
        }
    }

    else {
        err = md_interface_bring_down(commit, db, ts_str(ifname),
                                      false, false);
        bail_error(err);
    }

 bail:
    ts_free(&ifname);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_override_handle_clear(md_commit *commit, const mdb_db *db,
                                   const char *action_name,
                                   bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    tstring *ifname = NULL;
    bn_ipv4addr addr = lia_ipv4addr_any;
    uint8 masklen = 0;
    tbool found = false, enabled = false;

    err = bn_binding_array_get_value_tstr_by_name
        (params, "ifname", NULL, &ifname);
    bail_error_null(err, ifname);

    if (strchr(ts_str(ifname), ':')) {
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, _("Override interface name may not be an alias"));
        bail_error(err);
        goto bail;
    }

    /*
     * If this interface is not enabled, stop here.  This should only
     * come up in a race condition, since the DHCP client should not
     * be running if the interface is disabled.
     */
    err = md_interface_get_config_by_ifname
        (ts_str(ifname), commit, db, &found, &enabled, NULL,
         &addr, &masklen, NULL, NULL, NULL, NULL, NULL, NULL);
    bail_error(err);

    if (!found || !enabled) {
        goto bail;
    }

    err = md_interface_override_set(ts_str(ifname), false);
    bail_error(err);

    /* XXXXX See bug 14016: this should likely be (instead of the next call):
     *
     *     err = md_interface_reapply_config(ts_str(ifname), commit, db, true);
     *     bail_error(err);
     */

    err = md_interface_set_ipv4_addr(commit, db, ts_str(ifname),
                                     addr, masklen,
                                     false, false);
    bail_error(err);

 bail:
    ts_free(&ifname);
    return(err);
}

int
md_interface_reapply_config(const char *ifname, md_commit *commit,
                            const mdb_db *db, tbool renew)
{
    int err = 0;
    tbool found = false, enabled = false, overridden = false;
    tbool dhcpv4_enabled = false, dhcpv6_enabled = false;
    tbool zeroconf_enabled = false;
    tbool is_bridge_member = false, is_bond_member = false;
    tbool is_alias = false;
    bn_ipv4addr addr = lia_ipv4addr_any;
    uint8 masklen = 0;

    err = md_interface_get_config_by_ifname
        (ifname, commit, db, &found, &enabled, &overridden,
         &addr, &masklen,
         &dhcpv4_enabled, &zeroconf_enabled,
         &is_bridge_member, /* &is_bond_member */ NULL, &is_alias,
         &dhcpv6_enabled);
    bail_error(err);

    if (!found || !enabled) {
        goto bail;
    }

    /*
     * XXXX This should handle IPv6 static addresses, but instead we
     * rely on our re-application of any IPv6 addresses any time
     * anything in /net changes.
     */

    if (enabled) {
        /* XXXXX See bug 14016: this should likely have:
         *     err = md_interface_mtu_set(ifname, true, commit, db);
         *     bail_error(err);
         */

        /* Note that this will cause our IPv6 kernel config to get applied */
        err = md_interface_set_ipv4_addr(commit, db, ifname, addr, masklen,
                                         false, is_bridge_member);
        bail_error(err);

        /*
         * XXX/EMT: not sure about the 'false' for 'release_lease'.
         * Are there cases here where we could have been effectively
         * disabling DHCP?  But note that some of our callers pass
         * 'true' for 'renew', so we expect to bounce the dhclient,
         * and those do NOT sound like good candidates for a release.
         * So do we need an extra param?  For now, the worst that could
         * happen is that we'd fail to release the lease in some cases.
         * Of course, the user can always manually renew.
         */

        err = md_interface_check_dhcp(ifname, commit, db, false,
                                      dhcpv4_enabled, enabled, renew, false);
        bail_error(err);

        err = md_interface_check_dhcp(ifname, commit, db, true,
                                      dhcpv6_enabled, enabled, renew, false);
        bail_error(err);

        err = md_interface_check_zeroconf(ifname, commit, db,
                                          zeroconf_enabled, enabled, renew);
        bail_error(err);

        /* XXXXX reapply our non-kernel IPv6 config */

        /* XXXXX See bug 14016: this should likely have:
         *    err = md_interface_apply_ipv6(commit, ifname, NULL, db);
         *    bail_error(err);
         */
    }

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_interface_alias_handle_add(md_commit *commit, const mdb_db *db,
                              const char *action_name,
                              bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    tstring *ifname = NULL, *tag = NULL;
    tstring *remove_tag = NULL;
    tbool found = false, enabled = false, found_addr = false;
    tstr_array *addrs = NULL, *netmasks = NULL;
    char *aif_name = NULL, *tuple = NULL;
    int32 ipv4_alias_num = 0, a_idx = 0, a_r_idx = 0;
    tbool pending = false;
    uint32 ga_count = 0;
    const bn_binding *ac_binding = NULL;
    bn_ipv4addr oldmask = lia_ipv4addr_any;
    bn_inetaddr addr = lia_inetaddr_ipv6addr_any;
    bn_ipv4addr addrv4 = lia_ipv4addr_any;
    bn_ipv6addr addrv6 = lia_ipv6addr_any;
    uint8 masklen = 0;
    const bn_binding *ip_b = NULL, *netmask_b = NULL;
    const bn_binding *inet_b = NULL, *masklen_b = NULL;
    tbool ok = false, is_ipv4 = false;
    char *addr_str = NULL, *mask_str = NULL;
    tbool is_bridge_member = false, is_bond_member = false, is_alias = false;

    bail_null(md_interface_aliases);
    bail_null(md_interface_ipv6_aliases);
    bail_null(md_interface_aliases_gratarp_count);
    
    err = bn_binding_array_get_value_tstr_by_name
        (params, "ifname", NULL, &ifname);
    bail_error_null(err, ifname);

    err = md_interface_get_config_by_ifname
        (ts_str(ifname), commit, db, &found, &enabled, NULL, NULL, NULL,
         NULL, NULL, &is_bridge_member, &is_bond_member, &is_alias, NULL);
    bail_error(err);

    if (!found) {
        goto bail;
    }

    err = bn_binding_array_get_binding_by_name
        (params, "ip", &ip_b);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name
        (params, "netmask", &netmask_b);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name
        (params, "inet_address", &inet_b);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name
        (params, "mask_len", &masklen_b);
    bail_error(err);

    /* Some validation of alias address and masks */
    if (!((((ip_b != NULL) && (netmask_b != NULL)) &&
           ((inet_b == NULL) && (masklen_b == NULL))) ||
          (((ip_b == NULL) && (netmask_b == NULL)) &&
           ((inet_b != NULL) && (masklen_b != NULL)))) ||
        ((inet_b == NULL) == (ip_b == NULL))) {
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, _("Address and mask must both be specified"));
        bail_error(err);
        goto bail;
    }

    if (inet_b) {
        err = bn_binding_get_inetaddr(inet_b, ba_value, NULL, &addr);
        bail_error(err);
        err = bn_binding_get_uint8(masklen_b, ba_value, NULL, &masklen);
        bail_error(err);
    }
    else {
        err = bn_binding_get_ipv4addr_inetaddr(ip_b, ba_value, NULL, &addr);
        bail_error(err);
        err = bn_binding_get_ipv4addr_bnipv4(netmask_b, ba_value, NULL, &oldmask);
        bail_error(err);
        err = lia_ipv4addr_to_masklen(&oldmask, &masklen);
        if (err) {
            masklen = 0xff;
            err = 0;
        }
    }

    /* Make sure the masklen is okay */
    err = lia_masklen_valid_for_inetaddr(masklen, &addr, &ok);
    if (err || !ok) {
        err = 0;
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, _("Invalid mask specified"));
        bail_error(err);
        goto bail;
    }

    /*
     * Make sure addr/masklen go together, and describe an endpoint
     * address.
     */
    err = md_net_validate_inetaddr_masklen(commit, "",
                                           addr, masklen,
                                           lia_inetaddr_is_ipv4_quick(&addr) ?
                                           lia_addrtype_match_ifaddr_ll :
                                           lia_addrtype_match_ifaddr,
                                           &ok);
    if (err || !ok) {
        err = 0;
        /* The call above should have set the commit code of failure */
        err = md_commit_set_return_status(commit, 1);
        bail_error(err);
        goto bail;
    }

    err = bn_binding_array_get_value_tstr_by_name
        (params, "tag", NULL, &tag);
    bail_error_null(err, tag);

    err = bn_binding_array_get_value_tstr_by_name
        (params, "remove_tag", NULL, &remove_tag);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name(params, "arp_count",
                                               &ac_binding);
    bail_error_null(err, ac_binding);
    err = bn_binding_get_uint32(ac_binding, ba_value, NULL, &ga_count);
    bail_error(err);

    /* 
     * Avoid some remove_tag corner cases.  We'll know if remove_tag is
     * set, that it has a tag name different than tag's.
     */
    if (ts_equal(tag, remove_tag) ||
        (remove_tag && ts_length(remove_tag) == 0)) {
        ts_free(&remove_tag);
    }

    err = lia_inetaddr_to_str(&addr, &addr_str);
    bail_error(err);
    err = lia_inetaddr_is_ipv4(&addr, &is_ipv4);
    bail_error(err);

    if (is_ipv4) {
        err = lia_inetaddr_get_ipv4addr(&addr, &addrv4);
        bail_error(err);
    }
    else {
        err = lia_inetaddr_get_ipv6addr(&addr, &addrv6);
        bail_error(err);
    }

    /* We always want to update the cache if we're doing config */
    err = md_do_interface_scan_cached(0, NULL);
    bail_error(err);

    err = md_interface_find_addr(ts_str(ifname), &addr, &found_addr,
                                 &ipv4_alias_num,
                                 NULL, NULL);
    bail_error(err);

    if (!found_addr) {
        if (is_ipv4) {
            aif_name = smprintf("%s:%d", ts_str(ifname), ipv4_alias_num);
            bail_null(aif_name);

            err = md_interface_set_ipv4_addr(commit, db, aif_name,
                                             addrv4, masklen,
                                             true, false);
            bail_error(err);

            /*
             * Send gratuitous ARP for new alias address on interface
             * Both a request and a reply
             */
            err = md_interface_grat_arp(commit, db, ts_str(ifname),
                                        addr_str, true);
            bail_error(err);
            err = md_interface_grat_arp(commit, db, ts_str(ifname),
                                        addr_str, false);
            bail_error(err);

            a_idx = -1;
            err = md_interface_search_aliases(ts_str(ifname), addr_str,
                                              masklen, ts_str(tag),
                                              &a_idx);
            if (err == lc_err_not_found) {
                err = lc_err_no_error;

                tuple = smprintf("%s,%s,%u,%s", aif_name, addr_str,
                                 masklen, ts_str(tag));
                bail_null(tuple);

                err = tstr_array_insert_str_sorted(md_interface_aliases,
                                                   tuple);
                bail_error(err);

                a_idx = -1;
                if (md_ifc_gratarp_resched_count > 0) {
                    err = tstr_array_binary_search_str(md_interface_aliases,
                                                       tuple,
                                                       (uint32 *) &a_idx);
                    bail_error(err);
                    err = uint32_array_insert
                        (md_interface_aliases_gratarp_count,
                         a_idx,
                         ga_count);
                    bail_error(err);
                }
            }
            else {
                bail_error(err);
            }

            /* We may want to send more gratuitous arp's */
            if (md_ifc_gratarp_resched_count > 0 && a_idx != -1) {
                err = uint32_array_set
                    (md_interface_aliases_gratarp_count, a_idx,
                     ga_count);
                bail_error(err);

                pending = false;
                err = lew_event_is_pending(md_lwc,
                                           (const lew_event **)
                                           &(md_ifc_gratarp_resched_timer),
                                           &pending, NULL);
                bail_error(err);

                if (!pending) {
                    err = lew_event_reg_timer_full(md_lwc,
                                      &(md_ifc_gratarp_resched_timer),
                                      "md_ifc_gratarp_resched_timer",
                                      "md_ifc_gratarp_resched_handler",
                                      md_ifc_gratarp_resched_handler, NULL,
                                      md_ifc_gratarp_resched_interval_ms);
                    bail_error(err);
                }
            }
        }
        else {  /* IPv6 */
            /* ................................................................
             * As in md_interface_apply_ipv6(), we have to make sure
             * we're allowed to apply this address.  Of course, if
             * this module was structured better, these checks would
             * only be done in one place.
             *
             * Note that this only affects if we try to install it,
             * not whether we keep track of it.  Even if the interface
             * is ineligible for this alias right now, we'll track it
             * so we can apply it later if the situation changes.
             */
            if (!is_bridge_member && !is_bond_member && !is_alias) {
                err = md_interface_addr_ipv6_add(commit, db,
                                                 ts_str(ifname),
                                                 &addrv6,
                                                 masklen);
                bail_error(err);
            }

            a_idx = -1;
            err = md_interface_search_ipv6_aliases(ts_str(ifname), &addrv6,
                                                   masklen, ts_str(tag),
                                                   &a_idx);
            if (err == lc_err_not_found) {
                err = lc_err_no_error;
                
                tuple = smprintf("%s,%s,%u,%s", ts_str(ifname), addr_str,
                                 masklen, ts_str(tag));
                bail_null(tuple);
                
                err = tstr_array_insert_str_sorted(md_interface_ipv6_aliases,
                                                   tuple);
                bail_error(err);
            }
        }

        /* Flush the cache now that we've changed the config */
        err = md_do_interface_scan_cached(-1, NULL);
        bail_error(err);
    }

    if (remove_tag) {
        a_r_idx = -1;
        if (is_ipv4) {
            err = md_interface_search_aliases(ts_str(ifname), addr_str,
                                              masklen,
                                              ts_str(remove_tag),
                                              &a_r_idx);
        }
        else {
            err = md_interface_search_ipv6_aliases(ts_str(ifname), &addrv6,
                                                   masklen,
                                                   ts_str(remove_tag),
                                                   &a_r_idx);
        }
        if (err == lc_err_not_found) {
            err = lc_err_no_error;
        }
        else {
            bail_error(err);

            /*
             * We were asked to add an alias, and we already have added
             * the same ifname-ip-netmask tuple with the 'remove_tag' .
             * Remove our internal record which has the 'remove_tag',
             * and add a new internal record for the alias with 'tag' .
             * Do not put this in the kernel, as it is already there,
             * and we do not want to disturb it.
             */

            err = tstr_array_delete(is_ipv4 ? md_interface_aliases:
                                    md_interface_ipv6_aliases,
                                    a_r_idx);
            bail_error(err);

            if (is_ipv4) {
                if (md_ifc_gratarp_resched_count > 0) {
                    err = uint32_array_delete
                        (md_interface_aliases_gratarp_count,
                         a_r_idx);
                    bail_error(err);
                }
            }

            if (is_ipv4) {
                err = md_interface_search_aliases(ts_str(ifname), addr_str,
                                                  masklen, ts_str(tag),
                                                  &a_idx);
            }
            else {
                err = md_interface_search_ipv6_aliases(ts_str(ifname), &addrv6,
                                                       masklen, ts_str(tag),
                                                       &a_idx);
            }
            if (err == lc_err_not_found) {
                err = lc_err_no_error;

                if (is_ipv4) {
                    aif_name = smprintf("%s:%d", ts_str(ifname),
                                        ipv4_alias_num);
                    bail_null(aif_name);

                    tuple = smprintf("%s,%s,%u,%s",
                                     aif_name,
                                     addr_str,
                                     masklen, ts_str(tag));
                    bail_null(tuple);

                    err = tstr_array_insert_str_sorted(md_interface_aliases,
                                                       tuple);
                    bail_error(err);

                    a_idx = -1;
                    if (md_ifc_gratarp_resched_count > 0) {
                        err = tstr_array_binary_search_str
                            (md_interface_aliases,
                             tuple,
                             (uint32 *) &a_idx);
                        bail_error(err);
                        err = uint32_array_insert
                            (md_interface_aliases_gratarp_count,
                             a_idx,
                             ga_count);
                        bail_error(err);
                    }
                }
                else {
                    tuple = smprintf("%s,%s,%u,%s", ts_str(ifname), addr_str,
                                     masklen, ts_str(tag));
                    bail_null(tuple);

                    err = tstr_array_insert_str_sorted
                        (md_interface_ipv6_aliases, tuple);
                    bail_error(err);
                }
            }
        }
    }

 bail:
    safe_free(tuple);
    safe_free(aif_name);
    ts_free(&ifname);
    safe_free(addr_str);
    safe_free(mask_str);
    ts_free(&tag);
    ts_free(&remove_tag);
    tstr_array_free(&addrs);
    tstr_array_free(&netmasks);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_interface_alias_handle_delete(md_commit *commit, const mdb_db *db,
                                 const char *action_name,
                                 bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    tstring *ifname = NULL, *tag = NULL;
    tbool found = false, enabled = false, found_addr = false;
    tstr_array *addrs = NULL, *netmasks = NULL;
    char *aif_name = NULL;
    int32 ipv4_alias_num = 0, a_idx = -1;
    uint8 masklen = 0;
    bn_ipv4addr oldmask = lia_ipv4addr_any;
    bn_inetaddr addr = lia_inetaddr_ipv6addr_any;
    bn_ipv6addr addrv6 = lia_ipv6addr_any;
    const bn_binding *ip_b = NULL, *netmask_b = NULL;
    const bn_binding *inet_b = NULL, *masklen_b = NULL;
    tbool ok = false, is_ipv4 = false;
    char *addr_str = NULL;
    tbool alias_removed = false;

    err = bn_binding_array_get_value_tstr_by_name
        (params, "ifname", NULL, &ifname);
    bail_error_null(err, ifname);

    err = md_interface_get_config_by_ifname
        (ts_str(ifname), commit, db, &found, &enabled, NULL, NULL, NULL,
         NULL, NULL, NULL, NULL, NULL, NULL);
    bail_error(err);

    if (!found) {
        goto bail;
    }

    err = bn_binding_array_get_binding_by_name
        (params, "ip", &ip_b);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name
        (params, "netmask", &netmask_b);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name
        (params, "inet_address", &inet_b);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name
        (params, "mask_len", &masklen_b);
    bail_error(err);

    /* Some validation of alias address and masks */
    if (!((((ip_b != NULL) && (netmask_b != NULL)) &&
           ((inet_b == NULL) && (masklen_b == NULL))) ||
          (((ip_b == NULL) && (netmask_b == NULL)) &&
           ((inet_b != NULL) && (masklen_b != NULL)))) ||
        ((inet_b == NULL) == (ip_b == NULL))) {
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, _("Address and mask must both be specified"));
        bail_error(err);
        goto bail;
    }

    if (inet_b) {
        err = bn_binding_get_inetaddr(inet_b, ba_value, NULL, &addr);
        bail_error(err);
        err = bn_binding_get_uint8(masklen_b, ba_value, NULL, &masklen);
        bail_error(err);
    }
    else {
        err = bn_binding_get_ipv4addr_inetaddr(ip_b, ba_value, NULL, &addr);
        bail_error(err);
        err = bn_binding_get_ipv4addr_bnipv4(netmask_b, ba_value, NULL,
                                             &oldmask);
        bail_error(err);
        err = lia_ipv4addr_to_masklen(&oldmask, &masklen);
        if (err) {
            masklen = 0xff;
            err = 0;
        }
    }

    /* Make sure the masklen is okay */
    err = lia_masklen_valid_for_inetaddr(masklen, &addr, &ok);
    if (err || !ok) {
        err = 0;
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, _("Invalid mask specified"));
        bail_error(err);
        goto bail;
    }

    /* Make sure addr/masklen go together, and describe an endpoint
     * address, or are zero.
     */
    err = md_net_validate_inetaddr_masklen(commit, "",
                                       addr, masklen,
                                       lia_inetaddr_is_ipv4_quick(&addr) ?
                                       lia_addrtype_match_ifaddr_ll_maybe :
                                       lia_addrtype_match_ifaddr_maybe,
                                       &ok);
    if (err || !ok) {
        err = 0;
        /* The call above should have set the commit code of failure */
        err = md_commit_set_return_status(commit, 1);
        bail_error(err);
        goto bail;
    }

    err = bn_binding_array_get_value_tstr_by_name
        (params, "tag", NULL, &tag);
    bail_error_null(err, tag);

    err = lia_inetaddr_to_str(&addr, &addr_str);
    bail_error(err);
    err = lia_inetaddr_is_ipv4(&addr, &is_ipv4);
    bail_error(err);

    if (!is_ipv4) {
        err = lia_inetaddr_get_ipv6addr(&addr, &addrv6);
        bail_error(err);
    }

    /* We always want to update the cache if we're doing config */
    err = md_do_interface_scan_cached(0, NULL);
    bail_error(err);

    err = md_interface_find_addr(ts_str(ifname), &addr, &found_addr,
                                 &ipv4_alias_num,
                                 NULL, NULL);
    bail_error(err);

    if (found_addr) {
        if (is_ipv4 && (ipv4_alias_num != -1)) {
            aif_name = smprintf("%s:%d", ts_str(ifname), ipv4_alias_num);
            bail_null(aif_name);

            err = md_interface_search_aliases(ts_str(ifname), addr_str,
                                              masklen, ts_str(tag),
                                              &a_idx);
            if (err == lc_err_not_found) {
                err = lc_err_no_error;
            }
            else {
                bail_error(err);

                err = md_interface_bring_down_alias(commit, db, aif_name,
                                                    addr_str);
                bail_error(err);
                alias_removed = true;

                err = tstr_array_delete(md_interface_aliases, a_idx);
                bail_error(err);
                if (md_ifc_gratarp_resched_count > 0) {
                    err = uint32_array_delete
                        (md_interface_aliases_gratarp_count,
                         a_idx);
                    bail_error(err);
                }
            }
        }
        else if (!is_ipv4) { /* IPv6 */
            err = md_interface_search_ipv6_aliases(ts_str(ifname), &addrv6,
                                              masklen, ts_str(tag),
                                              &a_idx);
            if (err == lc_err_not_found) {
                err = lc_err_no_error;
            }
            else {
                bail_error(err);

                err = md_interface_addr_ipv6_remove(commit, db,
                                                    ts_str(ifname),
                                                    &addrv6,
                                                    masklen);
                bail_error(err);
                alias_removed = true;

                err = tstr_array_delete(md_interface_ipv6_aliases, a_idx);
                bail_error(err);
            }
        }

        /* Flush the cache now that we've changed the config */
        err = md_do_interface_scan_cached(-1, NULL);
        bail_error(err);
    }

 bail:
    if (alias_removed) {
        lc_log_basic(LOG_INFO, "Alias delete: matching alias found and "
                     "removed");
    }
    else {
        lc_log_basic(LOG_INFO, "Alias delete: no matching alias found");
    }
    safe_free(aif_name);
    ts_free(&ifname);
    ts_free(&tag);
    safe_free(addr_str);
    tstr_array_free(&addrs);
    tstr_array_free(&netmasks);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Determine if the interface name in 'alias_ifname' is an alias of
 * the base interface name held in 'ifname'.  e.g. passing {ether1,
 * ether1:5} would return true.  Note that it is not sufficient to
 * just us lc_is_prefix() because we don't want things like ether10,
 * or ether10:5, being detected as aliases of ether1.
 */
static int
md_interface_is_alias_of(const char *ifname, const char *alias_ifname,
                         tbool *ret_is_alias)
{
    int err = 0;
    int ifname_len = 0, colon_offset = 0;
    tbool is_alias = false;
    char *end = NULL;
    const char *colon = NULL;
    unsigned long aidx = 0;

    bail_null(ifname);
    bail_null(alias_ifname);

    if (!lc_is_prefix(ifname, alias_ifname, false)) {
        goto bail;
    }

    ifname_len = strlen(ifname);

    /*
     * We were given an empty interface name, which in some contexts
     * is intended as a wildcard (the caller wants to match all
     * aliases, regardless of which interface they go with).
     */
    if (ifname_len == 0) {
        colon = strchr(alias_ifname, ':');
        if (colon == NULL) {
            goto bail;
        }
        else {
            colon_offset = colon - alias_ifname;
        }
    }

    /*
     * We were given a real interface name, so the ':' must come
     * immediately after it in the alias name.
     */
    else {
        colon_offset = ifname_len;
        if (alias_ifname[colon_offset] != ':') {
            goto bail;
        }
    }
    
    errno = 0;
    aidx = strtoul(alias_ifname + colon_offset + 1, &end, 10);
    if (errno != 0 || end == NULL || *end != '\0') {
        lc_log_basic(LOG_INFO, "Alias name '%s' has unexpected index format",
                     alias_ifname);
        goto bail;
    }

    is_alias = true;

 bail:
    if (ret_is_alias) {
        *ret_is_alias = is_alias;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_alias_clear(md_commit *commit, const mdb_db *db,
                         const char *ifname, const char *tag)
{
    int err = 0, err2 = 0;
    tstr_array *tokens = NULL;
    const char *aif_name = NULL, *a_tag = NULL, *a_ip = NULL;
    uint32 num_aliases = 0, i = 0;
    tbool is_alias_of = false;

    num_aliases = tstr_array_length_quick(md_interface_aliases);

    for (i = 0; i < num_aliases; i++) {
        tstr_array_free(&tokens);
        err = ts_tokenize(tstr_array_get_quick(md_interface_aliases, i),
                          ',', '\0', '\0', 0, &tokens);
        bail_error_null(err, tokens);

        aif_name = tstr_array_get_str_quick(tokens, 0);
        a_ip = tstr_array_get_str_quick(tokens, 1);
        a_tag = tstr_array_get_str_quick(tokens, 3);

        is_alias_of = false;
        if (ifname && aif_name) {
            err = md_interface_is_alias_of(ifname, aif_name, &is_alias_of);
            complain_error(err);
        }

        if ((!ifname || is_alias_of)
            && (!tag || ((strlen(tag) > 0) && (strcmp(tag, a_tag) == 0)))) {
            err = md_interface_bring_down_alias(commit, db, aif_name, a_ip);
            bail_error(err);

            err = tstr_array_delete(md_interface_aliases, i);
            bail_error(err);

            if (md_ifc_gratarp_resched_count > 0) {
                err = uint32_array_delete(md_interface_aliases_gratarp_count,
                                          i);
                bail_error(err);
            }

            /* XXX is this really the best plan? */
            --i;
            --num_aliases;
        }
    }

 bail:
    if (!ifname) {
        /* Should be empty, but make sure */
        err2 = tstr_array_empty(md_interface_aliases);
        complain_error(err2);
        err2 = uint32_array_empty(md_interface_aliases_gratarp_count);
        complain_error(err2);
    }
    tstr_array_free(&tokens);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_interface_alias_ipv6_clear(md_commit *commit, const mdb_db *db,
                              const char *ifname, const char *tag)
{
    int err = 0, err2 = 0;
    tstr_array *tokens = NULL;
    const char *aif_name = NULL, *a_tag = NULL, *a_ip = NULL, *a_masklen = NULL;
    uint32 num_aliases = 0, i = 0;
    bn_ipv6addr addr = lia_ipv6addr_any;
    uint8 masklen =0;

    num_aliases = tstr_array_length_quick(md_interface_ipv6_aliases);

    for (i = 0; i < num_aliases; i++) {
        tstr_array_free(&tokens);
        err = ts_tokenize(tstr_array_get_quick(md_interface_ipv6_aliases, i),
                          ',', '\0', '\0', 0, &tokens);
        bail_error_null(err, tokens);

        aif_name = tstr_array_get_str_quick(tokens, 0);
        a_ip = tstr_array_get_str_quick(tokens, 1);
        a_masklen = tstr_array_get_str_quick(tokens, 2);
        a_tag = tstr_array_get_str_quick(tokens, 3);

        err = lia_str_to_ipv6addr(a_ip, &addr);
        bail_error(err);
        err = lc_str_to_uint8(a_masklen, &masklen);
        bail_error(err);

        if ((!ifname || ifname[0] == '\0' || !safe_strcmp(ifname, aif_name)) &&
            (!tag || ((strlen(tag) > 0) && (strcmp(tag, a_tag) == 0)))) {
            err = md_interface_addr_ipv6_remove(commit, db, aif_name,
                                                &addr, masklen);
            bail_error(err);

            err = tstr_array_delete(md_interface_ipv6_aliases, i);
            bail_error(err);

            /* XXX is this really the best plan? */
            --i;
            --num_aliases;
        }
    }

 bail:
    if (!ifname) {
        /* Should be empty, but make sure */
        err2 = tstr_array_empty(md_interface_aliases);
        complain_error(err2);
        err2 = uint32_array_empty(md_interface_aliases_gratarp_count);
        complain_error(err2);
    }
    tstr_array_free(&tokens);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_interface_remove_aliases(const char *ifname)
{
    int err = 0;
    const tstr_array *ifstats_lines = NULL;
    uint32 i = 0, j = 0, num_lines = 0;
    const char *line_str = NULL;
    const tstring *line = NULL;
    int end = -1, ifname_len = 0;
    tstring *found_ifname = NULL;
    tbool is_alias_of = false;

    err = md_do_interface_scan_cached(0, &ifstats_lines);
    bail_error(err);

    num_lines = tstr_array_length_quick(ifstats_lines);
    for (i = 0; i < num_lines; i++) {
        line = tstr_array_get_quick(ifstats_lines, i);
        line_str = ts_str(line);
        bail_null(line_str);

        if (line_str[0] == ' ') {
            /* Not a line containing the interface name */
            continue;
        }

        end = -1;
        /* Now get the interface name */
        for (j = 0; line_str[j]; ++j) {
            if (line_str[j] == ' ') {
                end = j;
                break;
            }
        }

        if (end < 0) {
            continue;
        }

        ifname_len = end;
            
        ts_free(&found_ifname);
        err = ts_substr(line, 0, ifname_len, &found_ifname);
        bail_error_null(err, found_ifname);

        if (found_ifname && ifname) {
            is_alias_of = false;
            err = md_interface_is_alias_of(ifname, ts_str(found_ifname),
                                           &is_alias_of);
            complain_error(err);
        }

        if (is_alias_of) {
            /* It's an alias, remove it */
            /* XXXX should pass the alias IP in here */
            err = md_interface_bring_down_alias(NULL, NULL,
                                                ts_str(found_ifname), NULL);
            bail_error(err);
        }
    }

    /* Flush the cache now that we've changed the config */
    err = md_do_interface_scan_cached(-1, NULL);
    bail_error(err);

 bail:
    ts_free(&found_ifname);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_interface_alias_handle_clear(md_commit *commit, const mdb_db *db,
                                const char *action_name,
                                bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    tstring *ifname = NULL, *tag = NULL;

    err = bn_binding_array_get_value_tstr_by_name
        (params, "ifname", NULL, &ifname);
    bail_error_null(err, ifname);

    err = bn_binding_array_get_value_tstr_by_name
        (params, "tag", NULL, &tag);
    bail_error_null(err, tag);

    err = md_interface_alias_clear(commit, db, ts_str(ifname), ts_str(tag));
    bail_error(err);

    err = md_interface_alias_ipv6_clear(commit, db, ts_str(ifname),
                                        ts_str(tag));
    bail_error(err);

 bail:
    ts_free(&ifname);
    ts_free(&tag);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Delete all aliases of a specified interface whose tag matches the
 * one given.
 *
 * Note that this functionality is NOT exposed through
 * /net/interface/actions/alias/delete, which still requires a specified
 * address and masklen.
 */
static int
md_interface_delete_aliases_by_tag(md_commit *commit, const mdb_db *db,
                                   const char *ifname, const char *tag)
{
    int err = 0;
    int i = 0, num_aliases = 0;
    tstr_array *tokens = NULL;
    const char *a_ifname = NULL, *a_addr_str = NULL, *a_masklen_str = NULL;
    const char *a_tag = NULL;
    bn_ipv6addr a_addr = lia_ipv6addr_any;
    uint8 a_masklen = 0;

    bail_null(ifname);
    bail_null(tag);

    num_aliases = tstr_array_length_quick(md_interface_ipv6_aliases);
    for (i = 0; i < num_aliases; ++i) {
        tstr_array_free(&tokens);
        err = ts_tokenize(tstr_array_get_quick(md_interface_ipv6_aliases, i),
                          ',', '\0', '\0', 0, &tokens);
        bail_error_null(err, tokens);

        bail_require(tstr_array_length_quick(tokens) == 4);

        a_ifname = tstr_array_get_str_quick(tokens, 0);
        a_tag = tstr_array_get_str_quick(tokens, 3);

        if (safe_strcmp(a_ifname, ifname) ||
            safe_strcmp(a_tag, tag)) {
            continue;
        }

        a_addr_str = tstr_array_get_str_quick(tokens, 1);
        err = lia_str_to_ipv6addr(a_addr_str, &a_addr);
        complain_error_msg(err, "Cannot convert string '%s' to IPv6 addr",
                           a_addr_str);
        if (err) {
            continue;
        }

        a_masklen_str = tstr_array_get_str_quick(tokens, 2);
        err = lc_str_to_uint8(a_masklen_str, &a_masklen);
        complain_error_msg(err, "Cannot convert string '%s' to uint8 masklen",
                           a_masklen_str);
        if (err) {
            continue;
        }
        
        err = md_interface_addr_ipv6_remove(commit, db, ifname, &a_addr,
                                            a_masklen);
        bail_error(err);
        
        err = tstr_array_delete(md_interface_ipv6_aliases, i);
        bail_error(err);

        --i;
        --num_aliases;
    }

    /* Flush the cache now that we've changed the config */
    err = md_do_interface_scan_cached(-1, NULL);
    bail_error(err);

 bail:
    tstr_array_free(&tokens);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_search_aliases(const char *ifname, const char *ip,
                            uint8 masklen, const char *tag,
                            int32 *ret_found_index)
{
    int err = 0;
    uint32 num_aliases = 0, i = 0;
    tstr_array *tokens = NULL;
    const char *a_ifname = NULL, *a_ip = NULL, *a_tag = NULL;
    int32 found_index = -1;
    tbool is_alias_of = false;

    bail_null(md_interface_aliases);

    num_aliases = tstr_array_length_quick(md_interface_aliases);
    for (i = 0; i < num_aliases; i++) {
        tstr_array_free(&tokens);
        err = ts_tokenize(tstr_array_get_quick(md_interface_aliases, i),
                          ',', '\0', '\0', 0, &tokens);
        bail_error_null(err, tokens);

        bail_require(tstr_array_length_quick(tokens) == 4);

        a_ifname = tstr_array_get_str_quick(tokens, 0);
        a_ip = tstr_array_get_str_quick(tokens, 1);
        a_tag = tstr_array_get_str_quick(tokens, 3);

        is_alias_of = false;
        if (ifname && a_ifname) {
            err = md_interface_is_alias_of(ifname, a_ifname, &is_alias_of);
            complain_error(err);
        }

        if (is_alias_of && !strcmp(a_ip, ip)
            && (!tag || (strcmp(tag, a_tag) == 0))) {
            found_index = i;
            if (ret_found_index) {
                *ret_found_index = i;
            }
            break;
        }
    }

    if (found_index == -1) {
        err = lc_err_not_found;
    }

 bail:
    tstr_array_free(&tokens);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* XXXX/EMT: this ignores its 'ifname' parameter, which does not seem right.
 * This seems modeled on md_interface_search_aliases(), which does heed it.
 */
static int
md_interface_search_ipv6_aliases(const char *ifname, bn_ipv6addr *addr,
                                 uint8 masklen, const char *tag,
                                 int32 *ret_found_index)
{
    int err = 0;
    uint32 num_aliases = 0, i = 0;
    tstr_array *tokens = NULL;
    const char *a_ip = NULL, *a_tag = NULL;
    int32 found_index = -1;
    bn_ipv6addr a_addr = lia_ipv6addr_any;

    bail_null(md_interface_ipv6_aliases);
    bail_null(addr);

    num_aliases = tstr_array_length_quick(md_interface_ipv6_aliases);
    for (i = 0; i < num_aliases; i++) {
        tstr_array_free(&tokens);
        err = ts_tokenize(tstr_array_get_quick(md_interface_ipv6_aliases, i),
                          ',', '\0', '\0', 0, &tokens);
        bail_error_null(err, tokens);

        bail_require(tstr_array_length_quick(tokens) == 4);

        a_ip = tstr_array_get_str_quick(tokens, 1);
        a_tag = tstr_array_get_str_quick(tokens, 3);

        err = lia_str_to_ipv6addr(a_ip, &a_addr);
        bail_error(err);

        if (lia_ipv6addr_equal_quick(addr, &a_addr) &&
            (!tag || (strcmp(tag, a_tag) == 0))) {
            found_index = i;
            if (ret_found_index) {
                *ret_found_index = i;
            }
            break;
        }
    }

    if (found_index == -1) {
        err = lc_err_not_found;
    }

 bail:
    tstr_array_free(&tokens);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_interface_renew_dhcp(md_commit *commit, const mdb_db *db,
                        tbool ipv6, const char *action_name,
                        bn_binding_array *params, void *cb_arg)
{
    int err = 0;
    tstring *ifname = NULL;
    char *bname = NULL;
    tbool found = false, enabled = false, dhcp_enabled = false;
    tbool dhcpv4_enabled = false, dhcpv6_enabled = false, ipv6_enabled = false;

    err = bn_binding_array_get_value_tstr_by_name
        (params, "ifname", NULL, &ifname);
    bail_error_null(err, ifname);

    /*
     * If this interface is not enabled, stop here.  This should only
     * come up in a race condition, since the DHCP client should not
     * be running if the interface is disabled.
     */
    err = md_interface_get_config_by_ifname
        (ts_str(ifname), commit, db, &found, &enabled, NULL,
         NULL, NULL, &dhcpv4_enabled, NULL, NULL, NULL, NULL, &dhcpv6_enabled);
    bail_error(err);

    dhcp_enabled = (ipv6 ? dhcpv6_enabled : dhcpv4_enabled);

    if (!found) {
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, _("No configuration found for interface %s."),
             ts_str(ifname));
        bail_error(err);
        goto bail;
    }

    if (!enabled) {
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, _("Interface %s is disabled."), ts_str(ifname));
        bail_error(err);
        goto bail;
    }

    /*
     * If we're talking about renewing DHCPv6, there are some other
     * things we need to check.
     */
    if (ipv6) {
        if (!md_proto_ipv6_enabled(commit, db)) {
            err = md_commit_set_return_status_msg(commit, 1,
                                                  _("IPv6 is disabled."));
            bail_error(err);
            goto bail;
        }
        bname = smprintf("/net/interface/config/%s/addr/ipv6/enable",
                         ts_str(ifname));
        bail_null(bname);
        err = mdb_get_node_value_tbool(commit, db, bname, 0, &found, 
                                       &ipv6_enabled);
        bail_error(err);
        if (!found || !ipv6_enabled) {
            err = md_commit_set_return_status_msg_fmt
                (commit, 1, _("IPv6 is disabled on interface %s"),
                 ts_str(ifname));
            bail_error(err);
            goto bail;
        }
    }

    if (!dhcp_enabled) {
        /*
         * XXX/EMT: UI: we're intentionally NOT saying "v4" for DHCPv4
         * here, in keeping with our standard UI terminology.  But this
         * is one of those cases where it might be particularly confusing,
         * if you think we're referring to DHCPv6 also.
         */
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, _("DHCP%s is not enabled on interface %s."),
             ipv6 ? "v6" : "", ts_str(ifname));
        bail_error(err);
        goto bail;
    }

    lc_log_debug(LOG_DEBUG, "Renew DHCP%s for interface %s",
                 ipv6 ? "v6" : "v4", ts_str(ifname));

    err = md_interface_check_dhcp(ts_str(ifname), commit, db, ipv6,
                                  dhcp_enabled, enabled, true, true);
    bail_error(err);

 bail:
    ts_free(&ifname);
    safe_free(bname);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_renew_dhcpv4(md_commit *commit, const mdb_db *db,
                          const char *action_name,
                          bn_binding_array *params, void *cb_arg)
{
    int err = 0;

    err = md_interface_renew_dhcp(commit, db, false, action_name, params, 
                                  cb_arg);
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_renew_dhcpv6(md_commit *commit, const mdb_db *db,
                          const char *action_name,
                          bn_binding_array *params, void *cb_arg)
{
    int err = 0;

    err = md_interface_renew_dhcp(commit, db, true, action_name, params,
                                  cb_arg);
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
/* md_interface_renew_dhcp_interface_ext provides an external interface for
 * DHCP interface renewal when system level dhcp parameters change.
 * This is called e.g. from md_net_general.c
 */
int
md_interface_renew_dhcp_interface_ext(md_commit *commit, const mdb_db *db,
                                      tbool ipv6, const char *ifname)
{
    int err = 0;
    tstr_array *intf_names = NULL;
    int ifcount = 0;
    int ifidx = 0;
    const mdb_db_query_flags_bf dbqflags = 0;
    tbool found = false, enabled = false;
    tbool dhcpv4_enabled = false, dhcpv6_enabled = false;

    if (ifname == NULL) {   /* Update all interfaces */
        /* Get list of current interfaces */
        err = mdb_get_tstr_array(commit, db,
                "/net/interface/config", (const char *) NULL,
                false, dbqflags, &intf_names);
        bail_error(err);
    }
    else {                  /* Update the specified interface */
        err = tstr_array_new(&intf_names, NULL);
        bail_error(err);
        err = tstr_array_append_str(intf_names, ifname);
        bail_error(err);
    }

    /* Renew each DHCP enabled interface */
    ifcount = tstr_array_length_quick(intf_names);
    for (ifidx = 0; ifidx < ifcount; ++ifidx) {
        ifname = tstr_array_get_str_quick(intf_names, ifidx);
        complain_null(ifname);
        if (ifname == NULL || ifname[0] == '\0') {
            continue;
        }

        err = md_interface_get_config_by_ifname
            (ifname, commit, db, &found, &enabled, NULL,
             NULL, NULL, &dhcpv4_enabled, NULL, NULL, NULL, NULL,
             &dhcpv6_enabled);
        bail_error(err);

        if (!ipv6 && found && enabled && dhcpv4_enabled) {
            lc_log_debug(LOG_DEBUG, "Renew DHCPv4 for interface %s", ifname);

            err = md_interface_check_dhcp(ifname, commit, db, false,
                                          dhcpv4_enabled, enabled, true,
                                          false);
            bail_error(err);
        }
        else if (ipv6 && found && enabled && dhcpv6_enabled) {
            lc_log_debug(LOG_DEBUG, "Renew DHCPv6 for interface %s", ifname);

            err = md_interface_check_dhcp(ifname, commit, db, true,
                                          dhcpv6_enabled, enabled, true,
                                          false);
            bail_error(err);
        }
        else {
            lc_log_debug(LOG_DEBUG, "Skipping non-DHCP enabled interface %s",
                         ifname);
        }
    }

 bail:
    tstr_array_free(&intf_names);
    return(err);
}


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/


/* ------------------------------------------------------------------------- */
static int
md_interface_get_config(const char *binding_name, const tstr_array *name_parts,
                        md_commit *commit, const mdb_db *db,
                        tbool *ret_found, char **ret_ifname,
                        tbool *ret_enabled, tbool *ret_overridden,
                        bn_ipv4addr *ret_addr, uint8 *ret_masklen,
                        tbool *ret_dhcp_enabled, tbool *ret_zeroconf_enabled,
                        tbool *ret_bridge_member, tbool *ret_bond_member,
                        tbool *ret_isalias, tbool *ret_dhcpv6_enabled)
{
    int err = 0;
    tstr_array *name_parts_alloc = NULL;
    tbool is_abs = false;
    uint32 num_parts = 0;

    bail_null(binding_name);
    bail_null(ret_found);
    bail_null(ret_ifname);
    *ret_ifname = NULL;

    if (name_parts == NULL) {
        err = bn_binding_name_to_parts(binding_name, &name_parts_alloc,
                                       &is_abs);
        bail_error(err);
        bail_require_msg(is_abs, "non-absolute name: %s", binding_name);
        name_parts = name_parts_alloc;
    }

    num_parts = tstr_array_length_quick(name_parts);
    if (num_parts >= 4 &&
        ts_equal_str(tstr_array_get_quick(name_parts, 0), "net", false) &&
        ts_equal_str(tstr_array_get_quick(name_parts, 1), "interface",
                     false) &&
        ts_equal_str(tstr_array_get_quick(name_parts, 2), "config", false)) {
        *ret_found = true; /* But still have to check for config... */
    }
    else {
        *ret_found = false;
        goto bail;
    }
    
    *ret_ifname = safe_strdup(tstr_array_get_str_quick(name_parts, 3));
    bail_null(*ret_ifname);

    err = md_interface_get_config_by_ifname
        (*ret_ifname, commit, db, ret_found, ret_enabled, ret_overridden,
         ret_addr, ret_masklen, ret_dhcp_enabled, ret_zeroconf_enabled,
         ret_bridge_member, ret_bond_member, ret_isalias, ret_dhcpv6_enabled);
    bail_error(err);

 bail:
    tstr_array_free(&name_parts_alloc);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_get_config_by_ifname(const char *if_name, md_commit *commit, 
                                  const mdb_db *db,
                                  tbool *ret_found, tbool *ret_enabled,
                                  tbool *ret_overridden,
                                  bn_ipv4addr *ret_addr, uint8 *ret_masklen,
                                  tbool *ret_dhcp_enabled,
                                  tbool *ret_zeroconf_enabled,
                                  tbool *ret_bridge_member,
                                  tbool *ret_bond_member, tbool *ret_isalias,
                                  tbool *ret_dhcpv6_enabled)
{
    int err = 0;
    bn_attrib *attrib = NULL;
    tstring *tstr = NULL;
    char *alias_ifdevname = NULL, *alias_index = NULL;
    tbool is_bond_member = false;
    char *s = NULL, *bens = NULL;
    tstr_array *si_nodes = NULL;
    const char *si_node = NULL;
    uint32 i = 0, num_sis = 0;
    tbool found = false, enabled = false;
    tstring *bond_num = NULL;

    bail_null(if_name);
    bail_null(db);
    bail_null(ret_found);
    *ret_found = false;

    err = mdb_get_node_attrib_fmt(commit, db, 0, ba_value, &attrib,
                                  "/net/interface/config/%s/enable", if_name);
    bail_error(err);

    if (attrib == NULL) {
        goto bail;
    }

    *ret_found = true;

    if (ret_enabled) {
        err = bn_attrib_get_tbool(attrib, NULL, NULL, ret_enabled);
        bail_error(err);
    }

    if (ret_overridden) {
        err = md_interface_override_is_set(if_name, ret_overridden);
        bail_error(err);
    }

    if (ret_bridge_member) {
        bn_attrib_free(&attrib);

        err = mdb_get_node_attrib_fmt(commit, db, 0, ba_value, &attrib,
                                      "/net/interface/config/%s/enslaved",
                                      if_name);
        bail_error(err);

        if (attrib) {
            err = bn_attrib_get_tbool(attrib, NULL, NULL, ret_bridge_member);
            bail_error(err);
        }
        else {
            lc_log_basic(LOG_WARNING,
                         I_("No 'enslaved' value for interface %s"), if_name);
        }

    }

    if (ret_bond_member) {
        /* XXXXX this is a potentially expensive lookup */

        is_bond_member = false;
        s = smprintf("/net/bonding/config/*/interface/%s", if_name);
        bail_null(s);

        err = mdb_get_matching_tstr_array
            (commit, db, s, 0, &si_nodes);
        bail_error(err);
        num_sis = tstr_array_length_quick(si_nodes);
        for (i = 0; i < num_sis; ++i) {
            si_node = tstr_array_get_str_quick(si_nodes, i);
            bail_null(si_node);

            ts_free(&bond_num);
            err = bn_binding_name_to_parts_va(si_node, false, 1, 3, &bond_num);
            bail_error(err);

            /*
             * See if the bond is disabled and if so, skip it.
             */
            safe_free(bens);
            bens = smprintf("/net/bonding/config/%s/enable",
                            ts_str(bond_num));
            err = mdb_get_node_value_tbool(commit, db,
                                           bens,
                                           0, &found, &enabled);
            bail_error(err);
            if (!found || !enabled) {
                continue;
            }

            is_bond_member = true;
            break;
        }

        *ret_bond_member = is_bond_member;
    }


    if (ret_isalias) {
        *ret_isalias = false;

        bn_attrib_free(&attrib);

        err = mdb_get_node_attrib_fmt(commit, db, 0, ba_value, &attrib,
                                   "/net/interface/config/%s/alias/ifdevname",
                                   if_name);
        bail_error(err);

        safe_free(alias_ifdevname);
        if (attrib) {
            err = bn_attrib_get_str
                (attrib, NULL, bt_string, NULL, &alias_ifdevname);
            bail_error(err);
        }

        bn_attrib_free(&attrib);

        err = mdb_get_node_attrib_fmt(commit, db, 0, ba_value, &attrib,
                                   "/net/interface/config/%s/alias/index",
                                   if_name);
        bail_error(err);

        safe_free(alias_index);
        if (attrib) {
            err = bn_attrib_get_str
                (attrib, NULL, bt_string, NULL, &alias_index);
            bail_error(err);
        }

        if (alias_ifdevname && alias_index) {
            if ((strlen(alias_ifdevname) > 0) && (strlen(alias_index) > 0)) {
                *ret_isalias = true;
            }
        }
        else {
            lc_log_basic(LOG_WARNING,
                         I_("No alias values for interface %s"), if_name);
        }

    }

    if (ret_addr) {
        bn_attrib_free(&attrib);
        err = mdb_get_node_attrib_fmt
            (commit, db, 0, ba_value, &attrib,
             "/net/interface/config/%s/addr/ipv4/static/1/ip", if_name);
        bail_error(err);
        if (attrib) {
            err = bn_attrib_get_ipv4addr_bnipv4(attrib, NULL, NULL, ret_addr);
            bail_error(err);
        }
        else {
            *ret_addr = lia_ipv4addr_any;
        }
    }

    if (ret_masklen) {
        bn_attrib_free(&attrib);
        err = mdb_get_node_attrib_fmt
            (commit, db, 0, ba_value, &attrib,
             "/net/interface/config/%s/addr/ipv4/static/1/mask_len", if_name);
        bail_error(err);
        if (attrib) {
            err = bn_attrib_get_uint8(attrib, NULL, NULL, ret_masklen);
            bail_error(err);
        }
        else {
            *ret_masklen = 0;
        }
    }

    if (ret_dhcp_enabled) {
        bn_attrib_free(&attrib);
        err = mdb_get_node_attrib_fmt
            (commit, db, 0, ba_value, &attrib,
             "/net/interface/config/%s/addr/ipv4/dhcp", if_name);
        bail_error(err);
        if (attrib) {
            err = bn_attrib_get_tbool(attrib, NULL, NULL, ret_dhcp_enabled);
            bail_error(err);
        }
    }

    if (ret_zeroconf_enabled) {
        bn_attrib_free(&attrib);
        err = mdb_get_node_attrib_fmt
            (commit, db, 0, ba_value, &attrib,
             "/net/interface/config/%s/addr/ipv4/zeroconf", if_name);
        bail_error(err);
        if (attrib) {
            err = bn_attrib_get_tbool(attrib, NULL, NULL, 
                                      ret_zeroconf_enabled);
            bail_error(err);
        }
    }

    if (ret_dhcpv6_enabled) {
        bn_attrib_free(&attrib);
        err = mdb_get_node_attrib_fmt
            (commit, db, 0, ba_value, &attrib,
             "/net/interface/config/%s/addr/ipv6/dhcp/enable", if_name);
        bail_error(err);
        if (attrib) {
            err = bn_attrib_get_tbool(attrib, NULL, NULL, ret_dhcpv6_enabled);
            bail_error(err);
        }
    }

 bail:
    bn_attrib_free(&attrib);
    ts_free(&tstr);
    safe_free(alias_ifdevname);
    safe_free(alias_index);
    safe_free(s);
    safe_free(bens);
    tstr_array_free(&si_nodes);
    ts_free(&bond_num);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_interface_override_set(const char *ifname, tbool override)
{
    int err = 0;
    tbool already_overridden = false;
    uint32 idx = 0;

    bail_null(ifname);
    bail_null(md_interface_overrides);

    /*
     * Check what the state was, since we don't want to add it twice.
     */
    err = tstr_array_binary_search_str(md_interface_overrides, ifname, &idx);
    if (err == lc_err_not_found) {
        err = lc_err_no_error;
        already_overridden = false;
    }
    else {
        bail_error(err);
        already_overridden = true;
    }

    if (override && !already_overridden) {
        err = tstr_array_insert_str_sorted(md_interface_overrides, ifname);
        bail_error(err);
    }

    else if (!override && already_overridden) {
        err = tstr_array_delete(md_interface_overrides, idx);
        bail_error(err);
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
int
md_interface_override_is_set(const char *ifname, tbool *ret_override)
{
    int err = 0;
    uint32 idx = 0;

    bail_null(ifname);
    bail_null(ret_override);
    bail_null(md_interface_overrides);

    err = tstr_array_binary_search_str(md_interface_overrides, ifname, &idx);
    if (err == lc_err_not_found) {
        err = lc_err_no_error;
        *ret_override = false;
    }
    else {
        bail_error(err);
        *ret_override = true;
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_maybe_clear_overrides(const char *if_name, tbool ipv6,
                                   md_commit *commit, const mdb_db *db)
{
    int err = 0;
    const char *action_name = NULL, *reason = NULL;
    bn_request *req = NULL;

    /*
     * Remove any interface aliases installed by DHCPv6 for the
     * specified interface.
     *
     * (Why doesn't dhclient6-script do this for us?  See its note
     * next to the RELEASE6 and STOP6 reasons: it's because of how we
     * will be terminating the dhclient.)
     */
    if (ipv6) {
        err = md_interface_delete_aliases_by_tag(commit, db, if_name,
                                                 md_net_overlay_tag_dhcpv6);
        bail_error(err);
    }

    /*
     * Directly remove the interface address override.  Don't need to
     * apply any configuration, since other parts of the code handle
     * this.
     *
     * This only applies to IPv4, because it's only for DHCPv4 where
     * we use interface address overrides.  DHCPv6 uses interface
     * aliases instead.
     */
    else {
        err = md_interface_override_set(if_name, false);
        bail_error(err);

        /*
         * The above call only removes the interface name from the
         * list of those which have active overrides, it does NOT
         * actually apply the change.  We are similar to
         * md_interface_override_handle_clear(), except that we might
         * want to zero out the interface address instead of applying
         * the static one.
         *
         * XXXX/EMT: bug 14517.  Need to check whether to apply
         * static IP addr vs. zero addr?  Also, is there a race condition?
         * See bug comments.
         */
#if 0
        lc_log_basic(LOG_INFO, "Removing address from interface: %s", if_name);
        err = md_interface_set_ipv4_addr(commit, db, if_name, lia_ipv4addr_any,
                                         0, false, false);
        bail_error(err);
#endif
    }

    /*
     * We no longer check here to verify whether any other interfaces
     * still have DHCP enabled.  That is not relevant, because all we
     * want to do below is to clear the overlay records in the name of
     * the current interface.  This will not affect the system's
     * current state at all if the interface is not the acting primary
     * overlay interface.  If the interface is the acting primary, the
     * overlays will be removed -- but this is okay, because we only
     * get here if the interface is becoming ineligible to be acting
     * primary, so shortly hereafter, it is going to lose that
     * designation.
     */

    err = (*md_resolver_overlay_clear_ext_func)(commit, db,
                                                ipv6 ?
                                                md_net_overlay_tag_dhcpv6 :
                                                md_net_overlay_tag_dhcpv4,
                                                if_name);
    bail_error(err);

    /* This currently is not used by DHCP */
    err = (*md_system_override_clear_ext_func)(commit, db, if_name);
    bail_error(err);

    if (!ipv6) {
        /* This will also reassert the route state */
        err = (*md_routes_overlay_clear_func)(commit, db, if_name);
        bail_error(err);
    }

    /* ........................................................................
     * This interface is no longer eligible to be acting primary.
     * For config changes, md_net_general should notice this on its
     * own, but we don't want to rely on that.  And for an action,
     * it definitely would not notice, since it isn't even called.
     *
     * XXXX/EMT: we don't necessarily need to do this on a config
     * change, since it could end up happening multiple times.  It's
     * more efficient for md_net_general to do it in its commit_apply,
     * though more fragile since it has to know all the different
     * reasons it might have to recompute.  Leave it here for now,
     * since fragility is worse than inefficiency.
     */
    if (ipv6) {
        action_name = "/net/general/actions/ipv6/set_overlay_valid";
        reason = "releasing DHCPv6 lease";
    }
    else {
        action_name = "/net/general/actions/set_overlay_valid";
        reason = "releasing DHCP lease";
    }
    err = bn_action_request_msg_create(&req, action_name);
    bail_error(err);
    err = bn_action_request_msg_append_new_binding(req, 0, "ifname", bt_string,
                                                   if_name, NULL);
    bail_error(err);
    err = bn_action_request_msg_append_new_binding(req, 0, "valid", bt_bool,
                                                   lc_bool_to_str(false),
                                                   NULL);
    bail_error(err);
    err = bn_action_request_msg_append_new_binding(req, 0, "reason", bt_string,
                                                   reason, NULL);
    bail_error(err);
    err = md_commit_handle_action_from_module(commit, req, NULL, NULL, NULL,
                                              NULL, NULL);
    bail_error(err);

 bail:
    bn_request_msg_free(&req);
    return(err);
}

static int
md_interface_is_admin_up(const char *ifname, tbool *ret_up)
{
    int err = 0;
    struct ifreq ifr;
    tbool is_up = false;

    bail_require(ifname);
    bail_require(ret_up);

    bail_require(md_ifc_sock >= 0);

    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
    err = ioctl(md_ifc_sock, SIOCGIFFLAGS, &ifr);
    if (err < 0) {
        err = lc_err_generic_error;
        goto bail;
    }

    is_up = ((ifr.ifr_flags & IFF_UP) ==
             IFF_UP);

 bail:
    if (ret_up) {
        *ret_up = is_up;
    }
    return(err);
}

static int
md_interface_wait_until_admin_up_matches(const char *ifname, tbool is_up,
                                         uint32 max_wait_ms, tbool *ret_matches)
{
    int err = 0;
    tbool matches_up = false;
    tbool curr_oper = false;
    char *nn = NULL;
    uint32 num_passes = 0, pass = 0;
    const uint32 per_pass_sleep = 100;

    bail_null(ifname);

    num_passes = max_wait_ms / per_pass_sleep;
    do {

        err = md_interface_is_admin_up(ifname, &curr_oper);
        bail_error_quiet(err);

        if (curr_oper == is_up) {
            matches_up = true;
            break;
        }
        usleep(per_pass_sleep * 1000);
    } while (pass < num_passes);

 bail:
    safe_free(nn);
    if (ret_matches) {
        *ret_matches = matches_up;
    }
    return(err);
}



/*
 * If ifname is NULL, only do the system-wide parts of the kernel config.
 */
static int
md_interface_apply_ipv6_kernel_config(md_commit *commit, const mdb_db *db,
                                      const char *ifname,
                                      tbool bringing_up_ifname)
{
    int err = 0;
    const char *addr_flush_scope_ids[] =
        {"link", "global", "site"}; /* Not "host" */
    uint32 scididx = 0;
    tbool ipv6_enabled = false, if_ipv6_enabled = false;
    tbool admin_enabled = false;
    tbool is_bridge_member = false, is_bond_member = false;
    tbool is_alias = false;
    char *ifns = NULL;
    tstring *nval = NULL;
    tbool found = false, enabled = false;
    int32 status = 0;
    tbool recursive_apply = false;
    bn_attrib *attr  = NULL;
    const char *c_ifname = NULL;
    tbool c_enabled = false;
    tstr_array *ifnames = NULL;
    uint32 num_ifs = 0;
    tbool ipv6_supported = false, prev_global_ipv6_enable = false;
    tbool slaac_enabled = false;
    char *slaac_sysctl_name = NULL;
    tstring *output = NULL;
    tbool exists = false;
    tbool changed = false, changed2 = false, changed_slaac = false;
    tbool changed_other = false;
    tbool sysctl_present = false;
    char *sn = NULL, *sv = NULL;
    uint32 tss = 0;
    uint32 llac = 0, num_addrs = 0, i = 0;
    tbool found_sysctl = false;
    tbool need_sysctl_bounce = false;
    tbool need_ll_bounce = false;
    tbool bounced_for_sysctl = false;
    tbool is_down = false, curr_oper = false;
    md_if_addr_rec_array *addrs = NULL;
    const md_if_addr_rec *mar = NULL;
    int logl = 0;
    char *reason = NULL;
    md_routes_delete_criteria rdc;

    memset(&rdc, 0, sizeof(rdc));

    bail_null(db);

    /* There is no IPv6 configuration for IPv4 alias subinterfaces */
    if (ifname && strchr(ifname, ':')) {
        goto bail;
    }

    /*
     * We'll make sure all our kernel settings are in place, before the
     * interface is brought up.  This is happening after static IPv4 and
     * IPv6 address have been added.
     */

    /* First, the big picture: is IPv6 disabled or unsupported altogether? */
    err = mdb_get_node_value_tbool(commit, db,
                                   "/net/general/config/ipv6/enable",
                                   0, NULL, &ipv6_enabled);
    bail_error(err);
    ipv6_supported = md_proto_ipv6_supported();

#ifndef PROD_FEATURE_IPV6
    if (!md_proto_ipv6_avail()) {
        goto bail;
    }
#else
    /*
     * We could goto bail here, but we're going to act as if IPv6 is
     * admin disabled, and attempt to apply our config anyway.  We want
     * to make sure we disable IPv6 if possible, so that more confusion
     * will not ensure later.
     */

    if (!md_proto_ipv6_avail()) {
        ipv6_enabled = false;
    }
#endif

    lc_log_basic(LOG_INFO,
                 "Applying IPv6 kernel interface config for %s",
                 ifname ? ifname : ":ALL:");

    if (!ipv6_enabled || !ipv6_supported) {
        prev_global_ipv6_enable = md_if_global_ipv6_enable;
        md_if_global_ipv6_enable = false;

        /*
         * This will disable IPv6 even on interfaces not explicitly
         * configured.  We do this as we want to be certain there's no
         * IPv6 left in the system.
         */
        err = md_utils_sysctl_set_maybe_changed
            ("/net/ipv6/conf/all/disable_ipv6", "1",
             &changed);
        if (ipv6_supported) {
            complain_error(err);
        }
        err = 0;

        /*
         * We do this set (potentially again) unconditionally, in case
         * we've done a disable before, and we're on a newer kernel.
         */
        if (!changed && md_if_global_ipv6_enable != prev_global_ipv6_enable) {
            err = md_utils_sysctl_set
                ("/net/ipv6/conf/all/disable_ipv6", "1");
            if (ipv6_supported) {
                complain_error(err);
            }
            err = 0;
        }

        err = md_utils_sysctl_set_maybe_changed
            ("/net/ipv6/conf/default/disable_ipv6", "1",
             &changed2);
        if (ipv6_supported) {
            complain_error(err);
        }
        err = 0;

        if (changed || changed2 ||
            md_if_global_ipv6_enable != prev_global_ipv6_enable) {

            /*
             * Loop over all the interfaces, and call ourself back to
             * re-apply any per-interface IPv6 kernel config needed for
             * the IPv6 disabled case.
             */
            tstr_array_free(&ifnames);
            err = mdb_get_tstr_array(commit, db, "/net/interface/config",
                                     NULL, false, 0, &ifnames);
            bail_error(err);

            num_ifs = tstr_array_length_quick(ifnames);
            for (i = 0; i < num_ifs; ++i) {
                c_ifname = tstr_array_get_str_quick(ifnames, i);
                bail_null(c_ifname);

                if (!safe_strcmp(c_ifname, ifname)) {
                    continue;
                }

                bn_attrib_free(&attr);
                err = mdb_get_node_attrib_fmt(commit, db, 0, ba_value, &attr,
                                              "/net/interface/config/%s/enable",
                                              c_ifname);
                bail_error(err);

                if (!attr) {
                    continue;
                }
                err = bn_attrib_get_tbool(attr, NULL, NULL, &c_enabled);
                bail_error(err);
                if (!c_enabled) {
                    continue;
                }
                err = md_interface_apply_ipv6_kernel_config(commit, db,
                                                            c_ifname, true);
                complain_error(err);
            }

            lc_log_basic(LOG_NOTICE, _("IPv6 disabled"));

            /*
             * Flush all the IPv6 addresses the system has, except the
             * "host" ones.
             */
            for (scididx = 0; scididx < SAFE_ARRAY_SIZE(addr_flush_scope_ids);
                 scididx++) {

                /* XXX #dep/args: ip */
                ts_free(&output);
                err = lc_launch_quick_status
                    (&status, &output, false, 6, prog_ip_path,
                     "-6", "addr", "flush", "scope",
                     addr_flush_scope_ids[scididx]);
                bail_error(err);

                if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                    err = md_commit_set_return_status_msg_fmt
                        (commit, 1,
                         _("Error flushing IPv6 addreses for scope: %s"),
                         addr_flush_scope_ids[scididx]);
                    bail_error(err);
                }
            }

            /* Flush all the IPv6 routes we added */

            /* XXX #dep/args: ip */
            ts_free(&output);
            err = lc_launch_quick_status
                (&status, &output, false, 6, prog_ip_path,
                 "-6", "route", "flush", "proto", "static");
            bail_error(err);

            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                err = md_commit_set_return_status_msg_fmt
                    (commit, 1, _("Error flushing IPv6 routes"));
                bail_error(err);
            }
        }
    }
    else {
        prev_global_ipv6_enable = md_if_global_ipv6_enable;
        md_if_global_ipv6_enable = true;

        /*
         * We do not set /all/disable_ipv6 to 0, as that could (on newer
         * kernels) bring up IPv6 on interfaces for which we have no
         * configuration.
         */

        /*
         * This default of IPv6 off is so we can bring interfaces up
         * without worrying about IPv6 exposure, and so newly created
         * interfaces not under mgmtd control will not get IPv6 on them
         * without explicitly enabling it.
         */
        err = md_utils_sysctl_set_maybe_changed
            ("/net/ipv6/conf/default/disable_ipv6", "1",
             &changed);
        bail_error(err);

        /*
         * Set the global accept_dad setting, so that newly created
         * interfaces will get the right setting sooner.
         */
        ts_free(&nval);
        err = mdb_get_node_value_tstr(commit, db,
                                      "/net/general/config/ipv6/accept_dad",
                                      0, &found, &nval);
        complain_error(err);
        if (err || !found) {
            err = 0;
            ts_free(&nval);
            err = ts_new_str(&nval, "1");
            bail_error(err);
        }
        err = md_utils_sysctl_set_maybe_changed
            ("/net/ipv6/conf/default/accept_dad", ts_str(nval),
             &changed_other);
        complain_error(err);
        err = 0;

        recursive_apply = false;

        if (changed ||
            md_if_global_ipv6_enable != prev_global_ipv6_enable) {

            lc_log_basic(LOG_NOTICE, "IPv6 enabled");

            /*
             * On older kernels this will fail, on newer it is required to
             * get loopback back in action for IPv6 after shutdown.
             */
            err = md_utils_sysctl_set_maybe
                ("/net/ipv6/conf/lo/disable_ipv6", "0");
            err = 0;

            recursive_apply = true;
        }
        else if (changed_other) {
            recursive_apply = true;
        }

        if (recursive_apply) {
            /*
             * Loop over all the interfaces, and call ourself back to
             * re-apply any per-interface IPv6 kernel config.
             */
            tstr_array_free(&ifnames);
            err = mdb_get_tstr_array(commit, db, "/net/interface/config",
                                     NULL, false, 0, &ifnames);
            bail_error(err);

            num_ifs = tstr_array_length_quick(ifnames);
            for (i = 0; i < num_ifs; ++i) {
                c_ifname = tstr_array_get_str_quick(ifnames, i);
                bail_null(c_ifname);

                if (!safe_strcmp(c_ifname, ifname)) {
                    continue;
                }

                bn_attrib_free(&attr);
                err = mdb_get_node_attrib_fmt(commit, db, 0, ba_value, &attr,
                                              "/net/interface/config/%s/enable",
                                              c_ifname);
                bail_error(err);

                if (!attr) {
                    continue;
                }
                err = bn_attrib_get_tbool(attr, NULL, NULL, &c_enabled);
                bail_error(err);
                if (!c_enabled) {
                    continue;
                }
                err = md_interface_apply_ipv6_kernel_config(commit, db,
                                                            c_ifname, true);
                complain_error(err);
            }

            /*
             * XXXX? Is there any IPv4 or link config per interface that
             * we need to reapply here?
             */
        }
    }


    /* This is the end of the "system wide" kernel config */
    if (!ifname || !safe_strcmp(ifname, "lo")) {
        goto bail;
    }
    err = md_interface_exists(ifname, &exists);
    bail_error(err);
    if (!exists) {
        goto bail;
    }

    /* Is IPv6 enabled for this interface? */
    ifns = smprintf("/net/interface/config/%s/addr/ipv6/enable", ifname);
    err = mdb_get_node_value_tbool(commit, db,
                                   ifns,
                                   0, &found, &if_ipv6_enabled);
    bail_error(err);
    /* If not config, take the overall default */
    if (!found) {
        if_ipv6_enabled = ipv6_enabled;
    }

    /*
     * Also have IPv6 off if the interface is configured to be a member
     * of a bridge or bond, or is an alias.
     */
    err = md_interface_get_config_by_ifname
        (ifname, commit, db, &found, &admin_enabled, NULL,
         NULL, NULL, NULL, NULL,
         &is_bridge_member, &is_bond_member, &is_alias, NULL);
    bail_error(err);
    if (found && (is_bridge_member || is_bond_member || is_alias)) {
        if_ipv6_enabled = false;
    }

    /* See if the IPv6 sysctl settings for the interface exist */
    sn = smprintf("/net/ipv6/conf/%s/disable_ipv6", ifname);
    bail_null(sn);
    safe_free(sv);
    err = md_utils_sysctl_get(sn, &sv);
    bail_error(err);

    found_sysctl = (sv != NULL);

    bounced_for_sysctl = false;

    if (!found_sysctl && (if_ipv6_enabled &&
                          md_if_global_ipv6_enable == true)) {
        /*
         * The IPv6 sysctl settings do not exist.  If we're going to
         * configure the interface, we need them first.
         */
        err = 0;

        if (bringing_up_ifname && if_ipv6_enabled) {

            /*
             * Try to get sysctl settings back, by trying to put a
             * (bogus) IPv6 address on the interface.  On later kernels
             * (like 2.6.32) this address will never make it on to the
             * interface even if it is up, which is why this particular
             * type of bogus unicast IPv6 address (a _multicast_
             * interface-local scoped address) was chosen.  For older
             * kernels, it may make it on, but we'll immediately delete
             * it.  Either way, it will cause the kernel to get IPv6 on
             * to the interface, and so get us our sysctl settings.  Of
             * course, disable_ipv6 may still be 0.
             *
             * For older kernels, we can only do this with the interface
             * up.
             *
             */

            err = md_interface_is_admin_up(ifname, &curr_oper);
            if (err || !curr_oper) {
                err = 0;

                /*
                 * It is not up, but it's going to be.  Newer kernels
                 * make sysctl settings show up if we bring it up, so
                 * try that first.
                 */
                lc_log_basic(LOG_INFO,
                             "Bring interface admin up to enable IPv6 "
                             "for interface: %s ", ifname);

                /* XXX #dep/args: ip */
                ts_free(&output);
                err = lc_launch_quick_status(NULL, &output,
                                             false, 5, prog_ip_path,
                                             "link", "set", ifname, "up");
                err = 0;

                /* Now, wait a bit to see if the sysctl settings are there */
                found_sysctl = false;
                for (tss = 0; tss < 5; tss++) {
                    safe_free(sv);
                    err = md_utils_sysctl_get(sn, &sv);
                    bail_error(err);
                    if (sv) {
                        found_sysctl = true;
                        safe_free(sv);
                        break;
                    }
                    usleep(100 * 1000); /* 100ms */
                }
            }

            if (!found_sysctl) {
                /* See if doing our bogus address trick will get us sysctl */

                lc_log_basic(LOG_INFO,
                             "IPv6 settings missing for interface: %s ;"
                             " attempting to fix by adding and removing"
                             " an IPv6 address",
                             ifname);

                /*
                 * ip -6 addr add ff11::ffff:ffff dev ether1 scope host
                 */
                /* XXX #dep/args: ip */
                ts_free(&output);
                err = lc_launch_quick_status(NULL, &output,
                                             false, 9, prog_ip_path,
                                             "-6", "addr", "add",
                                             md_interface_bad_ipv6_addr,
                                             "dev", ifname, "scope", "host");
                err = 0;

                /*
                 * In theory, this should not be needed, as the address
                 * add should have failed due to disable_ipv6=1 being
                 * the default (or because a newer kernel will always
                 * reject it), but perhaps there is some race in which
                 * it could have worked on an older kernel.
                 *
                 * ip -6 addr del ff11::ffff:ffff dev ether1
                 */
                /* XXX #dep/args: ip */
                ts_free(&output);
                err = lc_launch_quick_status(NULL, &output,
                                             false, 7, prog_ip_path,
                                             "-6", "addr", "del",
                                             md_interface_bad_ipv6_addr,
                                             "dev", ifname);
                err = 0;

                /* Now, wait a bit until we can see the sysctl settings */
                found_sysctl = false;
                for (tss = 0; tss < 5; tss++) {
                    safe_free(sv);
                    err = md_utils_sysctl_get(sn, &sv);
                    bail_error(err);
                    if (sv) {
                        found_sysctl = true;
                        safe_free(sv);
                        break;
                    }
                    usleep(100 * 1000); /* 100ms */
                }
            }

            if (!found_sysctl) {
                lc_log_basic(LOG_WARNING,
                             "IPv6 settings missing for interface: %s",
                             ifname);

                /*
                 * XXXX This is pretty sad.  We're going to bring down
                 * the link, and bring it back up so that IPv6 will
                 * start.
                 */

                bounced_for_sysctl = true;

                lc_log_basic(LOG_WARNING,
                             "IPv6 settings missing for interface: %s;"
                             " link will be bounced", ifname);

                /* XXX #dep/args: ip */
                ts_free(&output);
                err = lc_launch_quick_status(NULL, &output,
                                         false, 5, prog_ip_path,
                                         "link", "set", ifname, "down");
                err = 0;

                /* Wait until it shows down */
                err = md_interface_wait_until_admin_up_matches(ifname,
                                                               false,
                                                               5000, &is_down);
                bail_error(err);
                if (!is_down) {
                    lc_log_basic(LOG_WARNING,
                                 "Could not bring down link %s", ifname);
                }

                /* Bring it (back) up */

                /* XXX #dep/args: ip */
                ts_free(&output);
                err = lc_launch_quick_status(NULL, &output,
                                             false, 5, prog_ip_path,
                                             "link", "set", ifname, "up");
                err = 0;

                /* Now, wait until we can see the sysctl settings */
                found_sysctl = false;
                for (tss = 0; tss < 20; tss++) {
                    safe_free(sv);
                    err = md_utils_sysctl_get(sn, &sv);
                    bail_error(err);
                    if (sv) {
                        found_sysctl = true;
                        safe_free(sv);
                        break;
                    }
                    usleep(100 * 1000); /* 100ms */
                }

                if (!found_sysctl) {
                    lc_log_basic(LOG_WARNING,
                                 "IPv6 settings missing for interface: %s",
                                 ifname);
                }
            }
        }
        else {
            /* Before we _do_ bring it up, we'll be back here */
            lc_log_basic(LOG_INFO,
                         "Skipping IPv6 configuration"
                         " on %s due to no kernel settings",
                         ifname);
            goto bail;
        }
        if (found_sysctl) {
            lc_log_basic(LOG_INFO,
                         "IPv6 settings are now found on interface: %s",
                         ifname);
        }
    }


    /* SLAAC enable: get our info ready, but do not do anything yet */
    safe_free(ifns);
    ifns = smprintf("/net/interface/config/%s/addr/ipv6/slaac/enable",
                    ifname);
    bail_null(ifns);
    err = mdb_get_node_value_tbool(commit, db,
                                   ifns,
                                   0, &found, &slaac_enabled);
    bail_error(err);
    if (!found) {
        slaac_enabled = false;
    }
    safe_free(slaac_sysctl_name);
    slaac_sysctl_name = smprintf("/net/ipv6/conf/%s/accept_ra", ifname);
    bail_null(slaac_sysctl_name);

    /*
     * This interface is not IPv6 enabled, turn off IPv6, and get rid of
     * any IPv6 addresses.
     *
     * XXXX Note that flushing the link-local seems to be irreversable
     * via sysctl, as you cannot get back your link-local address (at
     * least on some older kernels) or sysctl settings without bring the
     * link down and up, which disturbs IPv4.  In some cases using the
     * bad IPv6 address hack (as above) can get the sysctl settings
     * back.  Going from IPv6 enabled to disabled is never disruptive.
     */
    if (!if_ipv6_enabled || md_if_global_ipv6_enable != true) {
        /* If there's no sysctl settings for IPv6, it is already off */
        if (!found_sysctl) {
            goto bail;
        }

        err = md_utils_sysctl_set_maybe(slaac_sysctl_name, "0");
        if (err == lc_err_file_not_found) {
            err = 0;
        }
        complain_error(err);

        safe_free(ifns);
        ifns = smprintf("/net/ipv6/conf/%s/disable_ipv6", ifname);
        bail_null(ifns);
        sysctl_present = true;
        err = md_utils_sysctl_set_maybe_changed(ifns, "1", &changed);
        if (err == lc_err_file_not_found) {
            err = 0;
            sysctl_present = false;
        }
        complain_error(err);

        if (changed) {
            lc_log_basic(LOG_NOTICE, _("Disabled IPv6 on interface %s"),
                         ifname);
        }
        else if (!changed && sysctl_present) {
            goto bail;
        }

        /* Flush all the IPv6 addresses the interface has */
        /* XXX #dep/args: ip */
        ts_free(&output);
        err = lc_launch_quick_status
            (&status, &output, false, 6, prog_ip_path,
             "-6", "addr", "flush", "dev", ifname);
        complain_error(err);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            err = md_commit_set_return_status_msg_fmt
                (commit, 1,
                 _("Error flushing IPv6 addreses for interface: %s"),
                 ifname);
            bail_error(err);
        }

        /* Flush all the IPv6 routes we added to this interface */
        /* XXX #dep/args: ip */
        ts_free(&output);
        err = lc_launch_quick_status
            (&status, &output, false, 8, prog_ip_path,
             "-6", "route", "flush", "dev", ifname, "proto", "static");
        complain_error(err);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            err = md_commit_set_return_status_msg_fmt
                (commit, 1,
                 _("Error flushing IPv6 routes for interface: %s"),
                 ifname);
            bail_error(err);
        }

        /* Do not try to do any more IPv6-related kernel config */
        goto bail;
    }

    /* Settings which are for IPv6, but only if it's on */
    bail_require(if_ipv6_enabled && md_if_global_ipv6_enable == true);

    /*
     * Disable SLAAC if it should be disabled.  Note that this may fail
     * if sysctl is not working.  We ignore this, because presumably we
     * complained about sysctl not working at WARNING above.  This is
     * not a problem, as the default for SLAAC is off, so if there's no
     * sysctl settings, there's no risk of SLAAC being on anyway.  If
     * this were to fail, it would only be because it had been enabled,
     * and the user just disabled it.  We try this again at the end to
     * catch this failure case.
     */
    if (!slaac_enabled) {
        err = md_utils_sysctl_set_maybe_changed(slaac_sysctl_name, "0",
                                                &changed_slaac);
        err = 0;
    }

    /* Interface IPv6 enabled */
    safe_free(ifns);
    ifns = smprintf("/net/ipv6/conf/%s/disable_ipv6", ifname);
    bail_null(ifns);
    sysctl_present = true;
    err = md_utils_sysctl_set_maybe_changed(ifns, "0", &changed);
    if (err == lc_err_file_not_found) {
        sysctl_present = false;
        err = 0;
    }
    bail_error(err);

    /*
     * The interface was just IPv6 enabled, and has IPv6 sysctl
     * settings.  So if it is down, or is up but has at least an IPv6
     * link local, go with it, otherwise we need to bounce it to get a
     * link local.
     */
    need_ll_bounce = false;
    if (sysctl_present && changed) {
        err = md_interface_is_admin_up(ifname, &curr_oper);
        if (err) {
            err = 0;
            need_ll_bounce = true;
            curr_oper = false;
        }
        if (curr_oper) {

            md_if_addr_rec_array_free(&addrs);
            err = md_interface_read_ipv6_if_addr_info(ifname, &addrs);
            bail_error(err);

            llac = 0;

            num_addrs = md_if_addr_rec_array_length_quick(addrs);
            for (i = 0; i < num_addrs; i++) {
                mar = md_if_addr_rec_array_get_quick(addrs, i);
                bail_null(mar);

                if (lia_inetaddr_get_family_quick(&(mar->mar_addr)) !=
                    AF_INET6) {
                    continue;
                }
                if (lia_inetaddr_is_linklocal_quick(&(mar->mar_addr))) {
                    llac++;
                }
            }

            /*
             * It's up, but has no IPv6 link locals.  We need to take it
             * down to get one.
             */
            if (llac == 0) {
                need_ll_bounce = true;
            }
        }
    }

    if (!sysctl_present) {
        /*
         * This is surprising, as we though we were able to read from
         * sysctl above.  Perhaps this is a race.  Respond by bouncing
         * the link.
         */

        lc_log_basic(LOG_WARNING,
                     "IPv6 settings expected but missing for interface: %s",
                     ifname);

        need_sysctl_bounce = true;
    }

    if (need_sysctl_bounce || need_ll_bounce) {

        /*
         * If need_ll_bounce, the interface was already admin up, had
         * sysctl settings, but no IPv6 link locals, so it needs a
         * bounce.
         */

        lc_log_basic(LOG_WARNING,
                     "IPv6 link local missing for interface: %s;"
                     " link will be bounced", ifname);

        /* XXX #dep/args: ip */
        ts_free(&output);
        err = lc_launch_quick_status(NULL, &output,
                                     false, 5, prog_ip_path,
                                     "link", "set", ifname, "down");
        err = 0;

        err = md_interface_wait_until_admin_up_matches(ifname, false, 5000,
                                                       &is_down);
        bail_error(err);
        if (!is_down) {
            lc_log_basic(LOG_WARNING, "Could not bring down link %s", ifname);
        }

        /* XXX #dep/args: ip */
        ts_free(&output);
        err = lc_launch_quick_status(NULL, &output,
                                     false, 5, prog_ip_path,
                                     "link", "set", ifname, "up");
        err = 0;
    }

    if (changed || need_sysctl_bounce || need_ll_bounce) {
        lc_log_basic(LOG_NOTICE, "Enabling IPv6 on interface %s",
                     ifname);
    }

    err = md_utils_sysctl_set_maybe(ifns, "0");
    if (err) {
        lc_log_basic(LOG_ERR, "Failed to enable IPv6 on interface %s",
                     ifname);
        err = 0;
        goto bail;
    }

    /* ===== SLAAC default route */
    safe_free(ifns);
    ifns = smprintf("/net/interface/config/%s/addr/ipv6/slaac/default_route",
                    ifname);
    bail_null(ifns);
    err = mdb_get_node_value_tbool(commit, db,
                                   ifns,
                                   0, &found, &enabled);
    bail_error(err);
    if (!found) {
        enabled = slaac_enabled;
    }
    safe_free(ifns);
    ifns = smprintf("/net/ipv6/conf/%s/accept_ra_defrtr", ifname);
    bail_null(ifns);
    err = md_utils_sysctl_set_maybe(ifns, enabled ? "1" : "0");
    bail_error(err);
    safe_free(ifns);

    /* ===== SLAAC privacy (random addresses) */
    safe_free(ifns);
    ifns = smprintf("/net/interface/config/%s/addr/ipv6/slaac/privacy",
                    ifname);
    bail_null(ifns);
    err = mdb_get_node_value_tbool(commit, db,
                                   ifns,
                                   0, &found, &enabled);
    bail_error(err);
    if (!found) {
        enabled = false;
    }
    safe_free(ifns);
    ifns = smprintf("/net/ipv6/conf/%s/use_tempaddr", ifname);
    bail_null(ifns);
    err = md_utils_sysctl_set_maybe(ifns, enabled ? "1" : "0");
    bail_error(err);

    /*
     * We never set accept_ra_pinfo to 0, but make sure it is 1 if SLAAC
     * is enabled, in case a default or something else set it to 0.
     * accept_ra_pinfo means: if we're accepting router advertisements
     * ("accept_ra", our autoconf/SLAAC enabled flag), should we use the
     * 'prefixes' out of router advertisements.  And by 'prefixes' it
     * means IPv6 address/masklen pairs, and not routes.
     */
    if (slaac_enabled) {
        safe_free(ifns);
        ifns = smprintf("/net/ipv6/conf/%s/accept_ra_pinfo", ifname);
        bail_null(ifns);
        err = md_utils_sysctl_set_maybe(ifns, slaac_enabled ? "1" : "0");
        bail_error(err);
    }

    /*
     * ===== SLAAC enable: set now that we know all SLAAC settings are
     *       set, and IPv6 is on.
     */
    err = md_utils_sysctl_set_maybe_changed(slaac_sysctl_name,
                                            slaac_enabled ? "1" : "0",
                                            &changed);
    bail_error(err);
    if (slaac_enabled == 0 && (changed || changed_slaac)) {
        /* Flush SLAAC addresses for this interface */

        /* XXX #dep/args: ip */
        ts_free(&output);
        err = lc_launch_quick_status
            (&status, &output, false, 7, prog_ip_path,
             "-6", "addr", "flush", "dev", ifname, "dynamic");
        complain_error(err);
        reason = smprintf(_("disabled SLAAC for interface '%s'"), ifname);
        bail_null(reason);

        /* Flush SLAAC added routes for this interface */
        rdc.mrdc_ifname = ifname;
        rdc.mrdc_table = RT_TABLE_MAIN;
        rdc.mrdc_rflags_set = RTF_ADDRCONF;
        err = (*md_routes_delete_routes_func)(commit, db, reason, &rdc);
        bail_error(err);
    }

    /*
     * Apply global (non-interface-specific) IPv6 kernel settings to the
     * particular interface.
     *
     * ===== accept_dad
     */
    ts_free(&nval);
    err = mdb_get_node_value_tstr(commit, db,
                             "/net/general/config/ipv6/accept_dad",
                             0, &found, &nval);
    bail_error(err);
    if (!found) {
        ts_free(&nval);
        err = ts_new_str(&nval, "1");
        bail_error(err);
    }
    safe_free(ifns);
    ifns = smprintf("/net/ipv6/conf/%s/accept_dad", ifname);
    bail_null(ifns);
    err = md_utils_sysctl_set_maybe(ifns, ts_str(nval));
    bail_error(err);
    safe_free(ifns);

 bail:
    ts_free(&output);
    ts_free(&nval);
    safe_free(sn);
    safe_free(sv);
    safe_free(ifns);
    safe_free(slaac_sysctl_name);
    safe_free(reason);
    md_if_addr_rec_array_free(&addrs);
    tstr_array_free(&ifnames);
    bn_attrib_free(&attr);
    return(err);
}

static int
md_interface_ensure_preup_kernel_config(md_commit *commit, const mdb_db *db,
                                        const char *ifname)
{
    int err = 0;
    char *sn = NULL, *sv = NULL;
    tstring *output = NULL;

    /* There's not such thing for IPv4 alias subinterfaces */
    if (ifname && strchr(ifname, ':')) {
        goto bail;
    }

    /* XXXXX This is part of the partial workaround for bug 14015 */
    if (ifname) {
        err = md_interface_mtu_set(ifname, true, commit, db);
        bail_error(err);
    }

    lc_log_basic(LOG_INFO, "Applying preup kernel interface config for %s",
                 ifname ? ifname : ":ALL:");

    /*
     * Before an interface is ever configured by us, we need to make
     * sure it has the per-protocol (IPv4 and IPv6) sysctl network
     * config.
     */
    err = md_interface_apply_ipv6_kernel_config(commit, db, ifname, true);
    complain_error(err);

    /*
     * Right now, there's no IPv4 kernel config needed.  However, for
     * consistency we'll make sure that at least IPv4 is on the
     * interface.
     */
    if (!ifname) {
        goto bail;
    }
    sn = smprintf("/net/ipv4/conf/%s/forwarding", ifname);
    bail_null(sn);
    safe_free(sv);
    err = md_utils_sysctl_get(sn, &sv);
    bail_error(err);
    if (!sv) {
        err = 0;
        /* IPv4 will set up its config even if the interface is down */

        /* XXX #dep/args: ip */
        ts_free(&output);
        err = lc_launch_quick_status
            (NULL, &output, false, 7, prog_ip_path,
             "-4", "addr", "add", "0.0.0.0/0", "dev", ifname);
        err = 0;
    }

 bail:
    ts_free(&output);
    safe_free(sn);
    safe_free(sv);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_interface_maybe_set_ipv4_addr(md_commit *commit, const mdb_db *db,
                                 tstr_array *addrs_already_set,
                                 const char *ifname,
                                 bn_ipv4addr addr, uint8 masklen,
                                 tbool is_alias, tbool is_bridge_member)
{
    int err = 0;

    err = tstr_array_linear_search_str(addrs_already_set, ifname, 0, NULL);
    if (err == lc_err_not_found) {
        err = tstr_array_append_str(addrs_already_set, ifname);
        bail_error(err);
        err = md_interface_set_ipv4_addr(commit, db, ifname, addr, masklen,
                                         is_alias, is_bridge_member);
        bail_error(err);
    }

 bail:
    return(err);    
}


/* Only called if the ifname is enabled */
static int
md_interface_set_link_internal(md_commit *commit, const mdb_db *db,
                               tbool do_prep,
                               const char *ifname,
                               tbool is_up)
{
    int err = 0;
    lc_launch_params *params = NULL;
    lc_launch_result result = LC_LAUNCH_RESULT_INIT;
    tbool normal_exit = false, ok_to_enable = false;

    err = lc_launch_params_get_defaults(&params);
    bail_error(err);

    err = ts_new_str(&(params->lp_path), prog_ip_path);
    bail_error(err);

    /*
     * Unless it's a special circumstance, we assume it's OK to bring
     * up the interface.  We are only called in cases when the rest of
     * the module decided it was time to do so anyway.
     */
    ok_to_enable = true;
        
    /* ================================================================= */
    /* Customer-specific graft point 2: override bringing the
     * interface up, and optionally bring it down instead.  Set
     * 'ok_to_enable' to false if you want it to be down.
     * =================================================================
     */
#ifdef INC_MD_IF_CONFIG_INC_GRAFT_POINT
#undef MD_IF_CONFIG_INC_GRAFT_POINT
#define MD_IF_CONFIG_INC_GRAFT_POINT 2
#include "../bin/mgmtd/modules/md_if_config.inc.c"
#endif /* INC_MD_IF_CONFIG_INC_GRAFT_POINT */

    if (ok_to_enable && is_up) {
        /*
         * Set all the pre-up kernel config for this interface
         */
        if (do_prep) {
            err = md_interface_ensure_preup_kernel_config(commit, db, ifname);
            bail_error(err);
        }

        lc_log_basic(LOG_INFO, "Bringing interface %s %s", ifname,
                     is_up ? "up" : "down");
        err = tstr_array_new_va_str
            (&(params->lp_argv), NULL, 5, prog_ip_path,
             "link", "set", ifname, "up");
        bail_error(err);
    }
    else {
        lc_log_basic(LOG_INFO, "Bringing interface %s down", ifname);
        err = tstr_array_new_va_str
            (&(params->lp_argv), NULL, 5, prog_ip_path,
             "link", "set", ifname, "down");
        bail_error(err);
    }

    params->lp_wait = true;
    params->lp_env_opts = eo_preserve_all;
    params->lp_log_level = LOG_INFO;
    params->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
    params->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;

    if (!lc_devel_is_production()) {
        lc_log_debug(LOG_INFO, "Devenv: skipping ip link");
        goto bail;
    }

    /*
     * XXX #dep/args: ip
     *
     * ip link set <ifname> {up,down}
     */
    err = lc_launch(params, &result);
    bail_error(err);

    err = lc_check_exit_status(result.lr_exit_status, &normal_exit, 
                               "ip");
    bail_error(err);

    if (!normal_exit || WEXITSTATUS(result.lr_exit_status)) {
        lc_log_basic(LOG_NOTICE, I_("#PROBLEM WITH IFNAME %s"),
                     ifname);
    }

    /*
     * ip doesn't print anything in this case unless something
     * is wrong.
     */    
    if (result.lr_captured_output &&
        ts_num_chars(result.lr_captured_output) > 2) {
        err = md_commit_set_return_status_msg
            (commit, 1, ts_str(result.lr_captured_output));
    }

 bail:
    lc_launch_params_free(&params);
    lc_launch_result_free_contents(&result);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_set_ipv4_addr(md_commit *commit, const mdb_db *db,
                           const char *ifname,
                           bn_ipv4addr addr, uint8 masklen,
                           tbool is_alias, tbool is_bridge_member)
{
    int err = 0;
    lc_launch_params *params = NULL;
    lc_launch_result result = LC_LAUNCH_RESULT_INIT;
    bn_ipv4addr ipv4broadcast = lia_ipv4addr_any;
    char *broadcast = NULL;
    tbool normal_exit = false;
    tbool down_intf = false;
    char ab[INET_ADDRSTRLEN];
    char *addr_str = NULL, *netmask_str = NULL;
    tbool exists = false;

    if (is_alias) {
        bail_require(!lia_ipv4addr_is_zero_quick(&addr));
    }

    if ((!is_alias && !is_bridge_member) && lia_ipv4addr_is_zero_quick(&addr)) {
#ifdef md_if_zero_addr_down
        down_intf = true;
#else
        down_intf = false;
#endif
        err = md_interface_bring_down(commit, db, ifname, !down_intf,
                                      false);
        bail_error(err);

        if (down_intf) {
            goto bail;
        }
    }

    if (is_bridge_member && !lia_ipv4addr_is_zero_quick(&addr)) {
        /* XXX meaning a module needs to set the enslaved flag and
         * clear the address before getting here */
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, _("Enslaved interface %s has a non-zero address"),
             ifname);
        bail_error(err);

        lc_log_basic(LOG_WARNING, I_("Enslaved interface %s desires a"
                                     "non zero address configured"), ifname);
        goto bail;
    }

    if (!is_alias) {
        err = md_interface_exists(ifname, &exists);
        bail_error(err);
        if (!exists) {
            lc_log_basic(LOG_INFO,
                 "Skipping setting up and IP for non-existent interface %s",
                  ifname);
            goto bail;
        }

        /* This will do our IPv6 kernel config */
        err = md_interface_set_link_internal(commit, db, true, ifname, true);
        bail_error(err);
    }

    err = lc_launch_params_get_defaults(&params);
    bail_error(err);

    err = ts_new_str(&(params->lp_path), prog_ifconfig_path);
    bail_error(err);

    err = tstr_array_new_va_str
        (&(params->lp_argv), NULL, 2, prog_ifconfig_path, ifname);
    bail_error(err);

    err = lia_ipv4addr_to_str(&addr, &addr_str);
    bail_error_null(err, addr_str);
    err = tstr_array_append_str_takeover(params->lp_argv, &addr_str);
    bail_error(err);

    if (!is_bridge_member && masklen != 0) {
        err = lia_masklen_to_ipv4addr_str(masklen, &netmask_str);
        bail_error_null(err, netmask_str);

        err = tstr_array_append_str(params->lp_argv, "netmask");
        bail_error(err);
        err = tstr_array_append_str_takeover(params->lp_argv, &netmask_str);
        bail_error(err);
    }

    if (!is_alias && !is_bridge_member &&
        !lia_ipv4addr_is_zero_quick(&addr) && masklen != 0) {

        err = lia_ipv4addr_bitop_masklen(&addr, masklen,
                                         lbo_a_and_b,
                                         &ipv4broadcast);
        bail_error(err);
        err = lia_ipv4addr_bitop_masklen(&ipv4broadcast, masklen,
                                          lbo_a_or_not_b,
                                          &ipv4broadcast);
        bail_error(err);

        err = lia_ipv4addr_to_str(&ipv4broadcast, &broadcast);
        bail_error_null(err, broadcast);

        err = tstr_array_append_str(params->lp_argv, "broadcast");
        bail_error(err);
        err = tstr_array_append_str_takeover(params->lp_argv, &broadcast);
        bail_error(err);
    }

    params->lp_wait = true;
    params->lp_env_opts = eo_preserve_all;
    params->lp_log_level = LOG_INFO;
    params->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
    params->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;

    if (!lc_devel_is_production()) {
        lc_log_debug(LOG_INFO, "Devenv: skipping ifconfig");
        goto bail;
    }

    /*
     * XXX #dep/args: ifconfig
     *
     * ifconfig <ifname> <addr> [netmask <mask>] [broadcast <broadcast>]
     */
    err = lc_launch(params, &result);
    bail_error(err);

    err = lc_check_exit_status(result.lr_exit_status, &normal_exit, 
                               "ifconfig");
    bail_error(err);

    if (!normal_exit || WEXITSTATUS(result.lr_exit_status)) {
        lc_log_basic(LOG_NOTICE, I_("#PROBLEM WITH IFNAME %s ADDR %s/%d"),
                     ifname,
                     lia_ipv4addr_to_str_buf_quick(&addr, ab, sizeof(ab)),
                     masklen);
    }

    /*
     * Ifconfig doesn't print anything in this case unless something
     * is wrong.
     */    
    if (result.lr_captured_output &&
        ts_num_chars(result.lr_captured_output) > 2) {
        err = md_commit_set_return_status_msg
            (commit, 1, ts_str(result.lr_captured_output));
    }

    err = 0;
    
 bail:
    safe_free(addr_str);
    safe_free(netmask_str);
    safe_free(broadcast);
    lc_launch_params_free(&params);
    lc_launch_result_free_contents(&result);
    return(err);
}

static int
md_interface_addr_ipv6_add_remove(md_commit *commit, const mdb_db *db,
                                  tbool is_add,
                                  const char *ifname,
                                  const bn_ipv6addr *addr,
                                  uint8 masklen)
{
    int err = 0;
    lc_launch_params *params = NULL;
    lc_launch_result result = LC_LAUNCH_RESULT_INIT;
    tbool normal_exit = false;
    char sb[INET6_ADDRSTRLEN + 4];
    char mlb[5];

    bail_null(ifname);
    bail_require(ifname[0] != '\0');
    bail_null(addr);

    lc_log_basic(LOG_INFO, "IPv6: %s address %s/%u on %s",
                 is_add ? "adding" : "removing",
                 lia_ipv6addr_to_str_buf_quick(addr, sb, sizeof(sb)),
                 masklen,
                 ifname);

    err = lc_launch_params_get_defaults(&params);
    bail_error(err);

    err = ts_new_str(&(params->lp_path), prog_ip_path);
    bail_error(err);

    err = tstr_array_new_va_str
        (&(params->lp_argv), NULL, 4,
         prog_ip_path, "-6", "address", is_add ? "add" : "delete");
    bail_error(err);

    err = lia_ipv6addr_to_str_buf(addr, sb, sizeof(sb));
    bail_error(err);
    snprintf(mlb, sizeof(mlb), "/%u", masklen);
    err = safe_strlcat(sb, mlb);
    bail_error(err);
    err = tstr_array_append_str(params->lp_argv, sb);
    bail_error(err);

    err = tstr_array_append_str(params->lp_argv, "dev");
    bail_error(err);
    err = tstr_array_append_str(params->lp_argv, ifname);
    bail_error(err);

    params->lp_wait = true;
    params->lp_env_opts = eo_preserve_all;
    params->lp_log_level = LOG_INFO;
    params->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
    params->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;

    if (!lc_devel_is_production()) {
        lc_log_debug(LOG_INFO, "Devenv: skipping ip addr");
        goto bail;
    }

    /*
     * XXX #dep/args: ip
     *
     * ip -6 address {add,delete} <addr> dev <ifname>
     */
    err = lc_launch(params, &result);
    bail_error(err);

    /*
     * XXX #dep/parse: ip
     *
     * Don't complain if we tried to remove an alias that was not
     * there -- this isn't really an error.
     *
     * XXX/EMT: we get "No such device or address" on tb1/tb2, but we
     * get "Cannot assign requested address" on mako.  (Due to
     * different NIC drivers?)  That's an odd message to get when
     * we're trying to delete an address, but since we are testing
     * for !is_add here, it ought to be OK to stifle.
     */
    if (!is_add && ts_nonempty(result.lr_captured_output) &&
        (strstr(ts_str(result.lr_captured_output),
                "No such device or address") ||
         strstr(ts_str(result.lr_captured_output),
                "Cannot assign requested address"))) {
        lc_log_basic(LOG_INFO, I_("Unable to remove address %s from "
                                  "interface %s: %s"), sb, ifname,
                     ts_str(result.lr_captured_output));
    }

    /*
     * Otherwise, do normal error checking.
     */
    else {
        err = lc_check_exit_status(result.lr_exit_status, &normal_exit, "ip");
        bail_error(err);
        if (!normal_exit || WEXITSTATUS(result.lr_exit_status)) {
            lc_log_basic(LOG_NOTICE, I_("#PROBLEM WITH IFNAME %s ADDR %s"),
                         ifname, sb);
        }

        /*
         * Except in the case tested above, 'ip' doesn't print
         * anything in this case unless something is wrong.
         */
        if (result.lr_captured_output &&
            ts_num_chars(result.lr_captured_output) > 2) {
            err = md_commit_set_return_status_msg
                (commit, 1, ts_str(result.lr_captured_output));
            bail_error(err);
        }
    }

 bail:
    lc_launch_params_free(&params);
    lc_launch_result_free_contents(&result);
    return(err);
}

static int
md_interface_addr_ipv6_add(md_commit *commit, const mdb_db *db,
                           const char *ifname,
                           const bn_ipv6addr *addr,
                           uint8 masklen)
{
    return(md_interface_addr_ipv6_add_remove(commit, db,
                                             true,
                                             ifname, addr, masklen));
}

static int
md_interface_addr_ipv6_remove(md_commit *commit, const mdb_db *db,
                              const char *ifname,
                              const bn_ipv6addr *addr,
                              uint8 masklen)
{
    return(md_interface_addr_ipv6_add_remove(commit, db,
                                             false,
                                             ifname, addr, masklen));
}


/*
 * A) Bringing down a non-alias interface, skip_down == false:
 *
 *     1) Remove all IPv4 addresses and aliases from the device
 *     2) Clear the interface's UP flag.  This also removes all the IPv6
 *        addresses, automatically.
 *
 *  XXXXXXX should we remove it from any bonds?
 *  XXXXXXX bridges?
 *
 * B) Bringing down a non-alias interface, with skip_down == true:
 *
 *     1) Remove all IPv4 addresses (leave aliases, do not touch IPv6)
 *
 * C) All alias bring downs are deletes of the IPv4 alias interface, done by
 *    md_interface_bring_down_alias(), below.
 *
 * Notes: done with ifconfig now, could be done instead with:
 *     A)  ip -4 addr flush dev ether1
 *         ip link set dev ether1 down
 *     B)  ip -4 addr flush dev ether1 label ether1
 *
 *
 */

/* ------------------------------------------------------------------------- */
static int
md_interface_bring_down_full(md_commit *commit, const mdb_db *db,
                             const char *ifname,
                             tbool skip_down, tbool is_alias,
                             const char *addr)
{
    int err = 0;
    lc_launch_params *params = NULL;
    lc_launch_result result = LC_LAUNCH_RESULT_INIT;
    tbool exists = false;

    if (is_alias) {
        return(md_interface_bring_down_alias(commit, db, ifname, addr));
    }

    err = lc_launch_params_get_defaults(&params);
    bail_error(err);

    err = ts_new_str(&(params->lp_path), prog_ifconfig_path);
    bail_error(err);

    if (skip_down) {
        err = tstr_array_new_va_str
            (&(params->lp_argv), NULL, 3,
             prog_ifconfig_path, ifname, "0.0.0.0");
        bail_error(err);
    }
    else {
        err = tstr_array_new_va_str
            (&(params->lp_argv), NULL, 4,
             prog_ifconfig_path, ifname, "0.0.0.0", "down");
        bail_error(err);
    }

    params->lp_wait = true;
    params->lp_env_opts = eo_preserve_all;
    params->lp_log_level = LOG_INFO;

    if (!lc_devel_is_production()) {
        lc_log_debug(LOG_INFO, "Devenv: skipping ifconfig");
        goto bail;
    }

    /*
     * As it's not an alias, make sure it exists first.  This could
     * happen if a bridge or bond got deleted.
     */
    err = md_interface_exists(ifname, &exists);
    bail_error(err);
    if (!exists) {
        lc_log_basic(LOG_INFO,
                     "Skipping bringing down of non-existent interface %s",
                     ifname);
        goto bail;
    }

    /*
     * XXX #dep/args: ifconfig
     *
     * ifconfig <ifname> 0.0.0.0 [down]
     */
    err = lc_launch(params, &result);
    bail_error(err);

    err = lc_check_exit_status(result.lr_exit_status, NULL, "ifconfig down");
    bail_error(err);

    err = 0;

 bail:
    lc_launch_params_free(&params);
    lc_launch_result_free_contents(&result);
    return(err);
}

int
md_interface_bring_down(md_commit *commit, const mdb_db *db,
                        const char *ifname,
                        tbool skip_down, tbool is_alias)
{
    return(md_interface_bring_down_full(commit, db, ifname, skip_down,
                                        is_alias, NULL));
}

/*
 * This is an alias-specific form of md_interface_bring_down().  addr
 * may be NULL.  If addr is not NULL, then this function makes sure that
 * an address is really gone, and so does an extra flush of the local
 * route table, which is basically local addresses.  We have seen some
 * setups where this is required to get the local route table entry
 * deleted, although such cases are not fully understood.
 *
 * ip -4 addr flush dev eth0 label eth0:0
 * ip -4 route flush table local 10.23.121.58/32
 * ip -4 route flush cache
 */
/* ------------------------------------------------------------------------- */
static int
md_interface_bring_down_alias(md_commit *commit, const mdb_db *db,
                              const char *ifname, const char *addr)
{
    int err = 0;
    int32 status = 0;
    tstring *output = NULL;
    char *base_ifname = NULL, *asp = NULL;
    const char *colon = NULL;

    bail_null(ifname);

    colon = strchr(ifname, ':');
    bail_require(colon);

    base_ifname = strldup(ifname, (colon - ifname) + 1);
    bail_null(base_ifname);

    /* Flush the IPv4 address of the alias interface */
    /* XXX #dep/args: ip */
    ts_free(&output);
    err = lc_launch_quick_status
        (&status, &output, true, 8, prog_ip_path,
         "-4", "addr", "flush", "dev", base_ifname, "label", ifname);
    complain_error(err);

#ifndef MD_IF_CONFIG_NO_FLUSH_ALIAS_ADDR_LOCAL
    if (addr && *addr) {
        /*
         * Flush the IPv4 address from the local address table.  This is
         * a workaround for if/when the kernel fails to remove the
         * address.
         */
        ts_free(&output);
        asp = smprintf("%s/32", addr);
        bail_null(asp);
        /* XXX #dep/args: ip */
        err = lc_launch_quick_status
            (&status, &output, true, 7, prog_ip_path,
             "-4", "route", "flush", "table", "local", asp);
        complain_error(err);
    }
#endif

    /* Flush the routing cache */
    /* XXX #dep/args: ip */
    ts_free(&output);
    err = lc_launch_quick_status
        (&status, &output, true, 5, prog_ip_path,
         "-4", "route", "flush", "cache");
    complain_error(err);

 bail:
    ts_free(&output);
    safe_free(base_ifname);
    safe_free(asp);
    return(err);
}

/*
 * This function is called from md_bonding.c and md_vlan.c .
 */
int
md_interface_bring_up(md_commit *commit, const mdb_db *db, 
                      const char *ifname)
{
    int err = 0;
    lc_launch_params *params = NULL;
    lc_launch_result result = LC_LAUNCH_RESULT_INIT;
    tbool normal_exit = false;
    tbool exists = false;

    err = lc_launch_params_get_defaults(&params);
    bail_error(err);

    err = ts_new_str(&(params->lp_path), prog_ip_path);
    bail_error(err);

    err = tstr_array_new_va_str
        (&(params->lp_argv), NULL, 5, prog_ip_path,
         "link", "set", ifname, "up");
    bail_error(err);

    params->lp_wait = true;
    params->lp_env_opts = eo_preserve_all;
    params->lp_log_level = LOG_INFO;
    params->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
    params->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;

    if (!lc_devel_is_production()) {
        lc_log_debug(LOG_INFO, "Devenv: skipping ip link up");
        goto bail;
    }

    /*
     * Make sure it exists first.  This could happen if a bridge or bond
     * got deleted.
     */
    err = md_interface_exists(ifname, &exists);
    bail_error(err);
    if (!exists) {
        lc_log_basic(LOG_INFO,
                     "Skipping bringing up of non-existent interface %s",
                     ifname);
        goto bail;
    }

    /*
     *  XXXXXXXXXX this is a problem when called from external modules
     *  like bonding, so really must be fixed!
     */
#if 0
    /*
     * Set all the pre-up kernel config for this interface
     */
    err = md_interface_ensure_preup_kernel_config(commit, db, ifname);
    bail_error(err);
#endif

    /*
     * XXX #dep/args: ip
     *
     * ip link set <ifname> up
     */
    err = lc_launch(params, &result);
    bail_error(err);

    err = lc_check_exit_status(result.lr_exit_status, &normal_exit, 
                               "ip link up");
    bail_error(err);

    if (!normal_exit || WEXITSTATUS(result.lr_exit_status)) {
        lc_log_basic(LOG_NOTICE, I_("#PROBLEM WITH INTERFACE %s"), ifname);
    }

    if (result.lr_captured_output &&
        ts_num_chars(result.lr_captured_output) > 2) {
        if (commit) {
            err = md_commit_set_return_status_msg
                (commit, 1, ts_str(result.lr_captured_output));
        }
        goto bail;
    }

    err = 0;

 bail:
    lc_launch_params_free(&params);
    lc_launch_result_free_contents(&result);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_interface_grat_arp(md_commit *commit, const mdb_db *db, 
                      const char *ifname, const char *addr, tbool arp_reply)
{
    int err = 0;
    lc_launch_params *params = NULL;
    lc_launch_result result = LC_LAUNCH_RESULT_INIT;
    tbool normal_exit = false;

    err = lc_launch_params_get_defaults(&params);
    bail_error(err);

    err = ts_new_str(&(params->lp_path), prog_arping_path);
    bail_error(err);

    err = tstr_array_new_va_str
        (&(params->lp_argv), NULL, 8, prog_arping_path, "-c", "1", "-q",
         arp_reply ? "-A" : "-U", "-I", ifname, addr);
    bail_error(err);

    params->lp_wait = true;
    params->lp_env_opts = eo_preserve_all;
    params->lp_log_level = LOG_INFO;
    params->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture;
    params->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture;

    if (!lc_devel_is_production()) {
        lc_log_debug(LOG_INFO, "Devenv: skipping arping");
        goto bail;
    }

    /* 
     * XXX #dep/args: arping
     *
     * arping -c 1 -q {-A,-U} -I <ifname> <addr>
     */
    err = lc_launch(params, &result);
    bail_error(err);

    err = lc_check_exit_status(result.lr_exit_status, &normal_exit, 
                               "arping");
    bail_error(err);

    if (!normal_exit || WEXITSTATUS(result.lr_exit_status)) {
        lc_log_basic(LOG_NOTICE, I_("#PROBLEM WITH IFNAME %s ADDR %s"),
                     ifname, addr);
    }
    
 bail:
    lc_launch_params_free(&params);
    lc_launch_result_free_contents(&result);
    return(err);
}

/* ------------------------------------------------------------------------- */
int
md_interface_mtu_set(const char *ifname, tbool only_if_changed,
                     md_commit *commit, const mdb_db *db)
{
    int err = 0;
    int32 status = 0;
    tstring *mtu = NULL, *curr_mtu = NULL;
    tstring *output = NULL;
    tbool exists = false;

    if (!lc_devel_is_production()) {
        lc_log_debug(LOG_INFO, "Devenv: skipping link mtu");
        goto bail;
    }

    err = mdb_get_node_attrib_tstr_fmt
        (commit, db, 0, ba_value, NULL, &mtu, 
         "/net/interface/config/%s/mtu", ifname);
    bail_error(err);

    if (mtu == NULL || ts_equal_str(mtu, "0", false)) {
        goto bail;
    }

    err = md_interface_exists(ifname, &exists);
    bail_error(err);

    if (!exists) {
        lc_log_basic(LOG_INFO, 
                     "Skipping MTU setting on non-existent interface %s", 
                     ifname);
        goto bail;
    }

    if (only_if_changed) {
        err = mdb_get_node_attrib_tstr_fmt
            (commit, db, 0, ba_value, NULL, &curr_mtu,
             "/net/interface/state/%s/mtu", ifname);
        bail_error(err);

        if (ts_equal(mtu, curr_mtu)) {
            goto bail;
        }
    }

    /*
     * XXX #dep/args: ip
     *
     * ip link set <ifname> mtu <mtu>
     */
    ts_free(&output);
    err = lc_launch_quick_status
        (&status, &output, false, 6, prog_ip_path,
         "link", "set", ifname, "mtu", ts_str(mtu));
    bail_error(err);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        err = md_commit_set_return_status_msg_fmt
            (commit, 1, _("Error setting MTU for interface %s: %s"), ifname,
             ts_str_maybe_empty(output));
        bail_error(err);
    }

 bail:
    ts_free(&mtu);
    ts_free(&curr_mtu);
    ts_free(&output);
    return(err);
}


/*****************************************************************************/
/*****************************************************************************/
/*** DHCP SUPPORT                                                          ***/
/*****************************************************************************/
/*****************************************************************************/

/*
 * Our overall strategy is this:
 *   - When we launch a client, save its pid in an interface-name-to-pid 
 *     mapping table.
 *   - When we want to "check DHCP", look up the pid in this table, and
 *     also look for a pid file.
 *   - If there is a pid file but no entry in the table, kill the pid
 *     named in the file.  It must be since the last reboot, since /var/run
 *     is cleared on boot, so the chances of collision with another 
 *     process are slim.  It's probably a client leftover from a previous
 *     run of mgmtd, perhaps not killed because mgmtd crashed.
 *   - If there is an entry in the table but no pid file, the table is 
 *     authoritative; we probably just launched the dhclient and it
 *     hasn't had a chance to write its pid file yet.
 *   - When killing all DHCP clients at the end, first run through the
 *     table, deleting corresponding pid files as you go, then kill any
 *     pids named in remaining pid files.
 *
 * We run different DHCP client daemons for DHCPv4 and DHCPv6, and
 * they each run different scripts.  But we try to factor out commonly
 * shared code in this module.
 */

/*
 *   Should be   PID file 
 *    running     found    Launched   Action
 *   ---------   --------  --------   ------
 *      no          no        no       -
 *      no          no       yes       Kill
 *      no         yes        no       Kill
 *      no         yes       yes       Kill
 *     yes          no        no       Launch
 *     yes          no       yes       -
 *     yes         yes        no       Kill, then Launch
 *     yes         yes       yes       -
 *
 *
 * We have some policy about how we terminate DHCP clients, and what
 * we do with the leftover lease files.  The main question is whether
 * we (a) release the lease, and (b) toast the lease file.  We always
 * do these things together, never just one.  Just toasting the lease
 * file would mean that we are now squatting on an IP address that was
 * given to us, but we have lost track of it (unless we were
 * stateless), so it is basically out of play until the lease expires
 * (unless the address was reserved specifically for us).  If the
 * allocated address space is small (more likely a problem under IPv4),
 * and/or lease times are long, we could run out of addresses.  Just
 * releasing the lease should not be a problem, since the lease file
 * should have a notation about the release, but we want to delete it
 * anyway to be certain.
 *
 * Toasting the lease file is important if we want to make sure we 
 * pick up all the changes from the server, at least with DHCPv6.
 * In that case, if dhclient comes up and finds a lease file with
 * an unexpired lease in it, it sends a request to the server to 
 * confirm the IP address we have.  If the server is still happy
 * with us having that address, it says so, and we continue with
 * the entire lease.  If the stateless info (DNS) has changed, this
 * is not detected.
 *
 * Most of the time, when we have been given a lease and have no
 * specific reason to believe its parameters want to change, we hold
 * onto a lease.  Dhclient won't try to use an expired lease (we
 * hope), so we're really only talking about a lease which is still
 * within the time period the server had told us it would be valid.
 * So when shutting down, or when changing DHCP parameters (stateless,
 * send-hostname, etc.) we retain the lease.  The cases where we
 * surrender the lease are (a) renew, where the user is explicitly
 * asking to start fresh, and (b) disabling DHCP.  One could
 * reasonably expect this to surrender a lease, and users often think
 * of disable/enable as tantamount to a renew anyway, so they had best
 * have the same behavior.  So the behavior is as follows:
 *
 *                   kill       release lease /    start
 *                  dhclient   kill lease file    dhclient
 * -------------------------------------------------------
 * shutdown            y              -              -
 * startup             -              -              y
 * disable DHCP        y              y              -
 * enable DHCP         -              -              y
 * renew               y              y              y
 * change params       y              -              y
 *
 * The tricky part about releasing a lease is that when we fork
 * dhclient (with the '-r' parameter), it sends the RELEASE packet,
 * but then it blocks on a server response before exiting.  We want
 * this confirmation, of course, because it's not a reliable link.
 * We don't really have to wait for the server response before
 * deleting the lease file and proceeding, but we DO at least have to
 * wait for it to come up and READ the lease file, and send the
 * release (since if we start a client and get an address, and that
 * happens to be the same address as before, we don't want to release
 * it afterwards!).  And although that ought to happen very quickly,
 * we have no way of knowing how long it will be on a loaded system.
 * So we still would rather wait for it to exit.
 *
 * On a renew, we're going to be relaunching dhclient right away, so
 * we just block.  If it hasn't succeeded in a short amount of time,
 * we just give up and kill it.  It has probably sent the release
 * already, and it has probably made it to the server, which is
 * probably just being slow.  If it didn't, well, it's not that big of
 * a deal.  (In practice, we don't expect many appliances to be
 * deployed using DHCP without reserved addresses, so we don't obsess
 * about this too much.)  A short delay is OK, because people are
 * likely used to a DHCP renew taking a few seconds.
 *
 * On a disable, it's trickier.  We don't want to block the disable,
 * since (a) it's less likely expected to cause a delay, and (b) a
 * delay is unnecessary unless you're just about to reenable DHCP.
 * So we fork the release process, and keep track of it (with a child
 * completion handler).  Only if someone tries to reenable DHCP before
 * the child has exited will we have to block (for the remainder of our
 * short timeout), and then kill it if necessary before proceeding.
 *
 * If a DHCP release that we're not specifically blocked on doesn't
 * complete, we still eventually want to give up on it, just not so
 * urgently.  So we have a separate long timeout we use for that.
 * If we ever actually need it to be done (basically, an enable
 * shortly following a disable), we'll revert to the short timeout.
 *
 * NOTE: all this talk about waiting for the server applies only to 
 * DHCPv6, where we are waiting for a server response.  DHCPv4 does
 * not wait -- it exits right after sending the packet, so we don't
 * expect to have to time out on it.
 *
 * Other note: some logic would suggest that we should ALWAYS release
 * our old lease.  On startup, we might think we have a valid lease,
 * but what if the clock has changed?  And when DHCP params are changed,
 * what if the user expected them to take effect immediately?
 * This is a tradeoff, and we decided not to release the lease in those
 * cases for now.
 */

/* ------------------------------------------------------------------------- */
static int
md_ifc_send_dhcp_kill_event(md_commit *commit, const char *ifname, tbool ipv6,
                            int32 pid)
{
    int err = 0;
    bn_request *req = NULL;
    bn_binding *binding = NULL;

    err = bn_event_request_msg_create
        (&req, ipv6 ?
         "/net/general/events/ipv6/dhcp_client_exited" :
         "/net/general/events/dhcp_client_exited");
    bail_error(err);

    err = bn_event_request_msg_append_new_binding
        (req, 0, "ifname", bt_string, ifname, NULL);
    bail_error(err);

    err = bn_binding_new_int32(&binding, "pid", ba_value, 0, pid);
    bail_error(err);

    err = bn_event_request_msg_append_binding_takeover(req, 0, &binding, NULL);
    bail_error(err);

    err = md_commit_handle_event_from_module(commit, req, NULL, NULL, NULL, 
                                             NULL);
    bail_error(err);

 bail:
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_dhcp_release_cleanup(md_ifc_dhcp_rec *dhcp_rec)
{
    int err = 0;
    char *pid_filename = NULL, *lease_filename = NULL;

    bail_null(dhcp_rec);

    /*
     * If it was not the child completion handler itself that called
     * us, we'd sort of like to be able to delete it.  But proc_utils
     * does not support this, and our completion handler doesn't really
     * mind being called too late anyway, so let it go.
     */

    dhcp_rec->midr_release_pid = 0;
    dhcp_rec->midr_release_launch_uptime = 0;

    err = lew_event_cancel(md_lwc, &(dhcp_rec->midr_release_timeout));
    bail_error(err);

    /*
     * Delete the pid and lease files.  The pid file is right because
     * the process should be gone (we either have confirmation of its
     * exit, or we sent it a SIGKILL and waited our timeout).  The
     * lease file, well, this is probably OK to delete, since it 
     * probably at least read it and got off the RELEASE message,
     * if nothing else.
     */
    if (dhcp_rec->midr_ipv6) {
        pid_filename = smprintf(dhcp6_pid_release_filename_fmt,
                                dhcp_rec->midr_ifname);
        bail_null(pid_filename);
        lease_filename = smprintf(dhcp6_lease_filename_fmt,
                                  dhcp_rec->midr_ifname);
        bail_null(lease_filename);
    }
    else {
        pid_filename = smprintf(dhcp_pid_release_filename_fmt,
                                dhcp_rec->midr_ifname);
        bail_null(pid_filename);
        lease_filename = smprintf(dhcp_lease_filename_fmt,
                                  dhcp_rec->midr_ifname);
        bail_null(lease_filename);
    }

    err = lf_remove_file(pid_filename);
    if (err && errno == ENOENT) {
        /* This is OK, we just wanted to make sure it was gone */
        err = 0;
        errno = 0;
    }
    bail_error_errno(err, "Deleting file %s", pid_filename);

    err = lf_remove_file(lease_filename);
    if (err && errno == ENOENT) {
        /* This is OK, we just wanted to make sure it was gone */
        err = 0;
        errno = 0;
    }
    bail_error_errno(err, "Deleting file %s", lease_filename);

 bail:
    safe_free(pid_filename);
    safe_free(lease_filename);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* We are passed a timeout because we are called both in the short timeout
 * (need the thing to exit so we can get proceed) and long timeout
 * (just wanted it to be over eventually) cases, and want to log the
 * right number.
 */
static int
md_interface_dhcp_release_timeout(md_ifc_dhcp_rec *dhcp_rec,
                                  lt_dur_ms timeout_ms)
{
    int err = 0;

    bail_null(dhcp_rec);

    /*
     * Since we were waiting for a reply from the server, and nothing
     * really bad is happening as a result, we can't complain too
     * loudly.
     */
    lc_log_basic(LOG_INFO, "DHCP%s release for interface %s "
                 "(pid %" LPRI_d_ID_T "): %" PRId64 " ms timeout expired, "
                 "killing client process",
                 dhcp_rec->midr_ipv6 ? "v6" : "v4", dhcp_rec->midr_ifname,
                 dhcp_rec->midr_release_pid, timeout_ms);

    err = lc_kill_and_reap(dhcp_rec->midr_release_pid, SIGKILL,
                           md_ifc_dhcp_release_timeout_short_ms, SIGKILL);
    if (err != lc_err_not_found) {
        complain_error(err);
    }
    err = 0;

    err = md_interface_dhcp_release_cleanup(dhcp_rec);
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_dhcp_release_timeout_handle(int fd, short event_type, void *data,
                                         lew_context *ctxt)
{
    int err = 0;
    md_ifc_dhcp_rec *dhcp_rec = data; /* Do not free, we don't own! */    
    
    bail_null(dhcp_rec);

    /*
     * Not sure if this is even possible, but in case there is some
     * race condition where the timer fires around the same time as
     * the SIGCHLD, make sure we haven't already reaped the process.
     */
    if (dhcp_rec->midr_release_pid == 0) {
        goto bail;
    }

    err = md_interface_dhcp_release_timeout(dhcp_rec,
                                          md_ifc_dhcp_release_timeout_long_ms);
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
/* A wrapper around md_interface_check_dhcp() which doesn't require
 * the caller to do any research beforehand.  Many callers already
 * naturally have the info that function needs, but we'll gather it
 * here for those that don't.
 */
static int
md_interface_check_dhcp_fresh(md_ifc_dhcp_rec *dhcp_rec, tbool renew,
                              tbool release_lease)
{
    int err = 0;
    md_commit *commit = NULL;
    mdb_db **dbp = NULL;
    const mdb_db *db = NULL;
    char *bname = NULL;
    tbool dhcp_enabled = false, if_enabled = false;

    bail_null(dhcp_rec);

    err = md_commit_create_commit(&commit);
    bail_error_null(err, commit);
    
    err = md_commit_get_main_db(commit, &dbp);
    bail_error(err);
    db = *dbp;
    bail_null(db);

    safe_free(bname);
    bname = smprintf("/net/interface/config/%s/enable",
                     dhcp_rec->midr_ifname);
    bail_null(bname);
    err = mdb_get_node_value_tbool(commit, db, bname, 0, NULL, &if_enabled);
    bail_error(err);

    safe_free(bname);
    if (dhcp_rec->midr_ipv6) {
        bname = smprintf("/net/interface/config/%s/addr/ipv6/dhcp/enable",
                         dhcp_rec->midr_ifname);
        bail_null(bname);
    }
    else {
        bname = smprintf("/net/interface/config/%s/addr/ipv4/dhcp",
                         dhcp_rec->midr_ifname);
        bail_null(bname);
    }
    err = mdb_get_node_value_tbool(commit, db, bname, 0, NULL, &dhcp_enabled);
    bail_error(err);

    err = md_interface_check_dhcp(dhcp_rec->midr_ifname, commit, db,
                                  dhcp_rec->midr_ipv6, dhcp_enabled,
                                  if_enabled, renew, release_lease);
    bail_error(err);

 bail:
    md_commit_commit_state_free(&commit);
    safe_free(bname);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_dhcp_exit(lc_child_completion_handle *cmpl_hand,
                       pid_t pid, int wait_status, tbool completed,
                       void *arg)
{
    int err = 0;
    int i = 0, num_recs = 0;
    md_ifc_dhcp_rec *test_rec = NULL, *dhcp_rec = NULL;
    char *pidfile = NULL;

    /*
     * This can happen at least on deinit.  We'd better not do
     * anything, since the process has not actually exited!  Launching
     * a new process here leads us to an endless loop (initiated on
     * shutdown) until we are killed by PM.
     */
    if (completed == false) {
        goto bail;
    }

    /* ........................................................................
     * Find the DHCP record corresponding to this pid.  Bail out if there
     * is none.
     *
     * Whenever we kill a dhclient ourselves, we use
     * lc_kill_and_reap(), so the process should be reaped and its pid
     * removed from the records by the time the mgmtd infrastructure
     * gets control back to call us here.  So if we don't find the
     * record, we assume that's what happened.  But if we DO find the
     * record, it probably means something suspicious is up.
     */
    num_recs = md_ifc_dhcp_rec_array_length_quick(md_ifc_dhcp_recs);
    for (i = 0; i < num_recs; ++i) {
        test_rec = md_ifc_dhcp_rec_array_get_quick(md_ifc_dhcp_recs, i);
        bail_null(test_rec);
        if (test_rec->midr_dhclient_pid == pid) {
            dhcp_rec = test_rec;
            break;
        }
    }

    if (dhcp_rec == NULL) {
        goto bail;
    }

    /* ........................................................................
     * Clean up after the dead process.  It may have left a pid file,
     * and we also have to update the data structures to reflect that
     * it isn't running anymore.
     */
    dhcp_rec->midr_dhclient_pid = 0;
    pidfile = smprintf(dhcp_rec->midr_ipv6 ?
                       dhcp6_pid_filename_fmt :
                       dhcp_pid_filename_fmt,
                       dhcp_rec->midr_ifname);
    bail_null(pidfile);
    err = lf_remove_file(pidfile);
    err = 0; /* Ignore errors, file may not have existed */

    /* ........................................................................
     * Find out if a dhclient should even be running at all.  Probably
     * if it was running before, it was supposed to be.  But we'll
     * check again, jut in case of some race condition we haven't
     * thought of.
     *
     * Note that these two bools are not definitive, as there are some
     * other considerations which md_interface_check_dhcp() checks.
     * (And so we can't modify our log message to be more specific
     * about whether we're going to launch another client.)
     */
    lc_log_basic(LOG_NOTICE, _("DHCP%s client for interface '%s' "
                               "(pid %" LPRI_d_ID_T ") died "
                               "unexpectedly, checking for relaunch"),
                 dhcp_rec->midr_ipv6 ? "v6" : "",
                 dhcp_rec->midr_ifname, pid);

    err = md_interface_check_dhcp_fresh(dhcp_rec, false, false);
    bail_error(err);

 bail:
    safe_free(pidfile);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_dhcp_release_exit(lc_child_completion_handle *cmpl_hand,
                               pid_t pid, int wait_status, tbool completed,
                               void *arg)
{
    int err = 0;
    int i = 0, num_recs = 0;
    md_ifc_dhcp_rec *test_rec = NULL, *dhcp_rec = NULL;
    lt_dur_ms time_elapsed = 0;
    
    /*
     * This can happen at least on deinit -- ignore it.
     */
    if (completed == false) {
        goto bail;
    }

    /*
     * Find the DHCP record corresponding to this pid.
     */
    num_recs = md_ifc_dhcp_rec_array_length_quick(md_ifc_dhcp_recs);
    for (i = 0; i < num_recs; ++i) {
        test_rec = md_ifc_dhcp_rec_array_get_quick(md_ifc_dhcp_recs, i);
        bail_null(test_rec);
        if (test_rec->midr_release_pid == pid) {
            dhcp_rec = test_rec;
            break;
        }
    }

    /*
     * We didn't find one.  This isn't too surprising, since we have
     * no way to remove the completion handler once we have waited on
     * the process ourselves earlier and already cleaned up after
     * that.  Removing the completion handler is not supported by
     * proc_utils, and perhaps with good reason, because if you
     * accidentally remove a completion handler from that same
     * handler, the thing that calls the handlers gets confused.  So
     * if we can't find the right record, we just silently bail out.
     */
    if (dhcp_rec == NULL) {
        goto bail;
    }

    /*
     * Ok, this DHCP release process was still outstanding, and we'll
     * clean up after it now.
     */
    time_elapsed = lt_sys_uptime_ms() - dhcp_rec->midr_release_launch_uptime;
    lc_log_basic(LOG_INFO, "DHCP%s release for interface %s "
                 "(pid %" LPRI_d_ID_T "): completed successfully "
                 "after %" PRId64 " ms", 
                 dhcp_rec->midr_ipv6 ? "v6" : "v4",
                 dhcp_rec->midr_ifname, pid, time_elapsed);

    err = md_interface_dhcp_release_cleanup(dhcp_rec);
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
/* We are just about to launch a DHCP client process for this
 * interface+family, and we can't do that if there's still a release
 * process running.  So wait for it to exit (up to our maximum
 * patience starting from when it was launched), and then kill it if
 * necessary.
 *
 * We uninstall our timeout and child completion handlers up front,
 * since we're going to take the place of both of those ourselves here,
 * and we don't want them interfering somehow.
 */
static int
md_interface_dhcp_release_wait(md_ifc_dhcp_rec *dhcp_rec)
{
    int err = 0;
    lt_dur_ms time_elapsed = 0, time_left = 0;
    const lt_dur_ms retry_interval_ms = 100;
    uint32 num_retries = 0;
    pid_t got_pid = 0;
    int status = 0;

    bail_null(dhcp_rec);

    if (dhcp_rec->midr_release_pid == 0) {
        goto bail;
    }

    err = lew_event_cancel(md_lwc, &(dhcp_rec->midr_release_timeout));
    bail_error(err);

    /*
     * We'd sort of like to be able to delete the child completion
     * handler.  But proc_utils does not support this, and our
     * completion handler doesn't really mind being called too late
     * anyway, so let it go.
     */

    /*
     * First, give it as long as it has left to exit on its own.
     * Even if it doesn't have any time left, we still do this, as
     * we still want to reap it.
     */
    time_elapsed = lt_sys_uptime_ms() - dhcp_rec->midr_release_launch_uptime;
    time_left = md_ifc_dhcp_release_timeout_short_ms - time_elapsed;
    if (time_left < 0) {
        time_left = 0;
    }

    /* Round up to next highest number of retries */
    num_retries = (time_left + retry_interval_ms - 1) / retry_interval_ms;

    lc_log_basic(LOG_INFO, "DHCP%s release for interface %s "
                 "(pid %" LPRI_d_ID_T "): "
                 "waiting for completion, with %" PRId64 " ms elapsed, and %"
                 PRId64 " ms left",
                 dhcp_rec->midr_ipv6 ? "v6" : "v4",
                 dhcp_rec->midr_ifname, dhcp_rec->midr_release_pid,
                 time_elapsed, time_left);

    err = lc_wait(dhcp_rec->midr_release_pid, num_retries, retry_interval_ms,
                  &got_pid, &status);
    complain_error_errno(err, "Waiting for exit of DHCP%s release for "
                         "interface %s (pid %" LPRI_d_ID_T ")",
                         dhcp_rec->midr_ipv6 ? "v6" : "v4",
                         dhcp_rec->midr_ifname,
                         dhcp_rec->midr_release_pid);

    if (got_pid > 0) {
        time_elapsed = lt_sys_uptime_ms() -
            dhcp_rec->midr_release_launch_uptime;
        lc_log_basic(LOG_INFO, "DHCP%s release for interface %s "
                     "(pid %" LPRI_d_ID_T "): "
                     "completed successfully after %" PRId64 " ms", 
                     dhcp_rec->midr_ipv6 ? "v6" : "v4",
                     dhcp_rec->midr_ifname, got_pid, time_elapsed);
        err = md_interface_dhcp_release_cleanup(dhcp_rec);
        bail_error(err);
    }
    else {
        err = md_interface_dhcp_release_timeout(dhcp_rec,
                                         md_ifc_dhcp_release_timeout_short_ms);
        bail_error(err);
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Fork dhclient with the '-r' parameter to release the lease we have
 * in our lease file.
 *
 * Because we should only be called immediately after a DHCP client
 * for this interface+family was killed, we assume that generally
 * (a) there is no PID file for such a DHCP client, and (b) there is
 * a lease file.  Either might be violated by a race condition, or by
 * a meddling user, but we don't expect this to happen often.
 * If either one is not true, we'll just skip the release.  If (a) fails
 * to be true, that is the more alarming case, and we log at WARNING.
 * If (b) fails to be true, maybe that's because the dhclient we just
 * killed wasn't up for very long and hadn't gotten a lease yet, so
 * we'll just mumble something at INFO.
 *
 * Similarly, we'd better not have any running DHCP processes for this
 * interface+family in our data structures.
 *
 * XXX/EMT: this will not work if we were disabling DHCP through some
 * higher-order configuration (like disabling the interface, or disabling
 * IPv6 on the interface or overall), since we will not be able to send
 * the release packet!
 */
static int
md_interface_dhcp_release_lease(const char *ifname, tbool ipv6)
{
    int err = 0;
    lc_launch_params *lp = NULL;
    const char *dhclient_path = NULL, *script_path = NULL, *log_str = NULL;
    const char *ver_param = NULL;
    const char *config_path = NULL;
    char *pid_filename = NULL, *lease_filename = NULL;
    char *event_descr = NULL;
    md_ifc_dhcp_rec *dhcp_rec = NULL; /* Do not free, we don't own! */
    tbool exists = false;
    lc_launch_result lr = LC_LAUNCH_RESULT_INIT;

    /* ........................................................................
     * Make sure our assumptions described above (no DHCP processes known
     * to be running, we have a lease file, but no pid file) all hold true.
     */
    if (ipv6) {
        dhclient_path = prog_dhcp6_client;
        script_path = DHCP6_CLIENT_SCRIPT_PATH;
        config_path = dhcp6_config_path;
        log_str = "v6";
        ver_param = "-6";
        pid_filename = smprintf(dhcp6_pid_release_filename_fmt, ifname);
        bail_null(pid_filename);
        lease_filename = smprintf(dhcp6_lease_filename_fmt, ifname);
        bail_null(lease_filename);
    }
    else {
        dhclient_path = prog_dhcp_client;
        script_path = DHCP_CLIENT_SCRIPT_PATH;
        config_path = dhcp_config_path;
        log_str = "v4";
        ver_param = "-4";
        pid_filename = smprintf(dhcp_pid_release_filename_fmt, ifname);
        bail_null(pid_filename);
        lease_filename = smprintf(dhcp_lease_filename_fmt, ifname);
        bail_null(lease_filename);
    }

    err = md_ifc_dhcp_rec_lookup(ifname, ipv6, true, &dhcp_rec);
    bail_error_null(err, dhcp_rec);

    if (dhcp_rec->midr_dhclient_pid > 0) {
        lc_log_basic(LOG_WARNING, "DHCP%s release for interface %s: "
                     "cannot do because regular dhclient(pid %" LPRI_d_ID_T
                     ") already running",
                     log_str, ifname, dhcp_rec->midr_dhclient_pid);
        goto bail;
    }

    if (dhcp_rec->midr_release_pid > 0) {
        lc_log_basic(LOG_WARNING, "DHCP%s release for interface %s: "
                     "cannot do because dhclient for release "
                     "(pid %" LPRI_d_ID_T ") already running",
                     log_str, ifname, dhcp_rec->midr_dhclient_pid);
        goto bail;
    }

    err = lf_test_exists(pid_filename, &exists);
    bail_error_errno(err, "Testing existence of '%s'", pid_filename);
    if (exists) {
        lc_log_basic(LOG_WARNING, "DHCP%s release for interface %s: "
                     "cannot do because PID file '%s' already exists", 
                     log_str, ifname, pid_filename);
        goto bail;
    }
        
    err = lf_test_exists(lease_filename, &exists);
    bail_error_errno(err, "Testing existence of '%s'", lease_filename);
    if (!exists) {
        lc_log_basic(LOG_INFO, "DHCP%s release for interface %s: "
                     "cannot do because lease file '%s' was not found",
                     log_str, ifname, lease_filename);
        goto bail;
    }

    /* ........................................................................
     * We're fine, so go ahead and launch the process to do the release.
     */
    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);

    /* The child should not create any files, but just in case... */
    lp->lp_umask = 022;

    err = ts_new_str(&(lp->lp_path), dhclient_path);
    bail_error(err);

    /*
     * Most of these parameters are the same as we use to launch a
     * normal dhclient.  The new one is '-r', which means to release
     * our current lease and exit.  This means run to completion, so
     * we don't need '-d' anymore.
     */
    err = tstr_array_new_va_str
        (&(lp->lp_argv), NULL, 13,
         dhclient_path, ver_param,
         "-q", "-r", "-lf", lease_filename, "-pf", pid_filename,
         "-sf", script_path, "-cf", config_path, ifname);
    bail_error(err);

    lp->lp_log_level = LOG_INFO;
    lp->lp_wait = false;

    /*
     * We pass "-q", but it seems to print stuff anyway, e.g. output
     * from mdreq if it gets an error.  So just stifle anything it
     * prints.  Hopefully anything important will also go to the logs.
     */
    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_devnull;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_devnull;

    /*
     * XXX #dep/args: dhclient
     *
     * dhclient {-4,-6} -q -r -lf <leasefile> -pf <pidfile> -sf <script>
     *          -cf <configfile> <ifname>
     */
    err = lc_launch(lp, &lr);
    bail_error(err);
    bail_require(lr.lr_child_pid > 0);

    /* ........................................................................
     * Make notes about our running process, and set a timeout timer.
     *
     * Note: it's OK to pass dhcp_rec here, although we don't own it,
     * because of our policy of never deleting these records.  Once one
     * gets created, it lasts for the life of the mgmtd process.
     */
    dhcp_rec->midr_release_pid = lr.lr_child_pid;
    dhcp_rec->midr_release_launch_uptime = lt_sys_uptime_ms();

    lc_log_basic(LOG_INFO, "DHCP%s release for interface %s "
                 "(pid %" LPRI_d_ID_T "): started",
                 log_str, ifname, dhcp_rec->midr_release_pid);

    event_descr = smprintf("DHCP%s release for %s", log_str, ifname);
    bail_null(event_descr);
    err = lew_event_reg_timer_full(md_lwc, &(dhcp_rec->midr_release_timeout),
                                   event_descr,
                                   "md_interface_dhcp_release_timeout_handle",
                                   md_interface_dhcp_release_timeout_handle,
                                   dhcp_rec,
                                   md_ifc_dhcp_release_timeout_long_ms);
    bail_error(err);
    err = lc_child_completion_register
        (md_cmpl_hand, dhcp_rec->midr_release_pid,
         md_interface_dhcp_release_exit, NULL);
    bail_error(err);

 bail:
    safe_free(pid_filename);
    safe_free(lease_filename);
    safe_free(event_descr);
    lc_launch_params_free(&lp);
    lc_launch_result_free_contents(&lr);
    /* Do not free dhcp_rec, we don't own it! */
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_check_dhcp_one(const char *if_name, 
                            md_commit *commit, const mdb_db *db, tbool ipv6,
                            tbool dhcp_enabled, tbool if_enabled, tbool renew,
                            tbool release_lease)
{
    int err = 0;
    tbool should_be_running = false;
    tbool pid_file_found = false;
    tbool client_launched = false;
    tbool found = false, ipv6_enabled = false;
    tbool killed_dhclient = false;
    char *bname = NULL;
    char *filename = NULL;
    int32 pid_from_file = -1, pid_from_array = -1, pid_just_killed = -1;
    md_ifc_dhcp_rec *dhcp_rec = NULL;  /* Not owned, DO NOT FREE */
    tbool is_bridge_member = false, is_bond_member = false, is_alias = false;

    bail_null(if_name);

    lc_log_debug(LOG_DEBUG, "Checking to see if DHCP%s is running for "
                 "interface %s", ipv6 ? "v6" : "v4", if_name);

    /*
     * A first-order calculation, which we may override below.
     */
    should_be_running = dhcp_enabled && if_enabled;

    /*
     * 'renew' is just a way of telling us that the DHCP client should
     * be killed, even if it is supposed to be running and is already
     * running.
     */

    /* ........................................................................
     * DHCPv6 has the unique consideration that IPv6 could be disabled
     * overall, or on this one interface.  In that case, the DHCPv6
     * client should never be running.
     */
    if (ipv6 && should_be_running) {
        if (!md_proto_ipv6_enabled(commit, db)) {
            should_be_running = false;
        }
        else {
            bname = smprintf("/net/interface/config/%s/addr/ipv6/enable",
                             if_name);
            bail_null(bname);
            err = mdb_get_node_value_tbool(commit, db, bname, 0, &found, 
                                           &ipv6_enabled);
            bail_error(err);
            if (!found || !ipv6_enabled) {
                should_be_running = false;
            }
        }
    }

    /* ........................................................................
     * DHCPv6 has another unique consideration that we need to avoid
     * running it when an interface is a member of a bridge or bond,
     * or is an alias.
     *
     * We also want that for DHCPv4, but it's done differently: we use
     * side effects to turn off the DHCPv4 config node if any of these
     * things are the case.
     */
    err = md_interface_get_config_by_ifname
        (if_name, commit, db, &found, NULL, NULL, NULL, NULL, NULL, NULL,
         &is_bridge_member, &is_bond_member, &is_alias, NULL);
    bail_error(err);
    if (is_bridge_member || is_bond_member || is_alias) {
        should_be_running = false;
    }

    /* ........................................................................
     * Learn what we can about the status of DHCP on this interface, from
     * two sources: our internal data structures, and the PID file written
     * by the dhclient.
     */
    err = md_interface_check_for_dhcp_pid(if_name, ipv6, &pid_from_file);
    bail_error(err);
    pid_file_found = (pid_from_file != -1);

    err = md_ifc_dhcp_rec_lookup(if_name, ipv6, true, &dhcp_rec);
    bail_error_null(err, dhcp_rec);
    pid_from_array = dhcp_rec->midr_dhclient_pid;
    client_launched = (pid_from_array != 0);

    /* ........................................................................
     * There are various cases where we'll have to kill a dhclient process:
     *   (a) If we are renewing DHCP (which means bouncing the dhclient),
     *       and one is running.
     *   (b) If we don't want one running, and one is.
     *   (c) If we didn't think one was running, but one seems to be.
     *       (Even if one is supposed to be running, we want it to be one
     *       we're aware of.)
     */
    if (/* Case (a) */
        (!should_be_running && (pid_file_found || client_launched)) ||

        /* Case (b) */
        (renew && (pid_file_found || client_launched)) ||

        /* Case (c) */
        (pid_file_found && !client_launched)) {

        killed_dhclient = true;

        /*
         * Now that we're in here, with a license to kill, there are
         * potentially two different dhclient processes to target: the
         * one we found a pid file for (pid_from_file), and the one we
         * knew about (pid_from_array).  There are three cases here:
         *
         *   (a) Most of the time, they should match.
         *
         *   (b) A few times, we might have one in pid_from_array,
         *       but not yet in pid_from_file, because we just launched it
         *       and it hasn't had a chance to write the pid file yet.
         *
         *   (c) Very rarely / never, we could have two different pids,
         *       or a pid in pid_from_file but none in pid_from_array.
         *       This is basically an error condition, but we try to
         *       deal with it gracefully anyway.
         *
         * We'll just try to kill each one in turn.  In case (a),
         * our second attempt to kill the same process should yield
         * lc_err_not_found, and we'll go silently on our way.
         */
        pid_just_killed = -1;

        if (pid_from_file > 0) {
            lc_log_basic(LOG_INFO, "Killing DHCP%s client on interface '%s', "
                         "pid %" PRId32, ipv6 ? "v6" : "v4", if_name,
                         pid_from_file);
            err = lc_kill_and_reap(pid_from_file, SIGTERM, 
                                   md_ifc_dhcp_kill_time_ms, SIGKILL);
            if (err == lc_err_not_found) {
                err = 0;
            }
            else {
                bail_error(err);
                err = md_ifc_send_dhcp_kill_event(commit, if_name, ipv6,
                                                  pid_from_file);
                bail_error(err);
            }

            pid_just_killed = pid_from_file;
            
            filename = smprintf(ipv6 ? dhcp6_pid_filename_fmt : 
                                dhcp_pid_filename_fmt, if_name);
            bail_null(filename);
            
            err = lf_remove_file(filename);
            if (err && errno == ENOENT) {
                /* This is OK, we just wanted to make sure it was gone */
                err = 0;
                errno = 0;
            }
            bail_error_errno(err, "Deleting file %s", filename);

            pid_from_file = -1;
            pid_file_found = false;
        }

        if (pid_from_array > 0) {
            /*
             * Avoid a redundant log message.
             */
            if (pid_from_array != pid_just_killed) {
                lc_log_basic(LOG_INFO, "Killing DHCP%s client on interface "
                             "'%s', pid %" PRId32, if_name, ipv6 ? "v6" : "v4",
                             pid_from_array);
                err = lc_kill_and_reap(pid_from_array, SIGTERM, 
                                       md_ifc_dhcp_kill_time_ms, SIGKILL);
                if (err == lc_err_not_found) {
                    err = 0;
                }
                else {
                    bail_error(err);
                    err = md_ifc_send_dhcp_kill_event(commit, if_name, ipv6,
                                                      pid_from_array);
                    bail_error(err);
                }
            }

            /*
             * We already looked up dhcp_rec above, and bailed if 
             * it was NULL.
             */
            dhcp_rec->midr_dhclient_pid = 0;

            pid_from_array = -1;
            client_launched = false;
        }

        err = md_interface_maybe_clear_overrides(if_name, ipv6, commit, db);
        bail_error(err);
    }

    /* ........................................................................
     * If we just killed a DHCP client, and were told to release any
     * old lease it had, do so now.
     *
     * Releasing the old lease, and deleting the lease file after the
     * release is complete, is (a) a courtesy to the server, and 
     * (b) sometimes necessary to prevent us from using stale config
     * we have gotten from DHCP, like in bug 10881.
     *
     * However, regardless of the circumstances, we should not release the
     * lease if the interface is part of a bond, since we expect the
     * interface to remain down in that case.     
     */
    if (release_lease && killed_dhclient) {
        if (!should_be_running || renew) {
            /*
             * These are the cases in which we expect this to happen,
             * according to our policy.
             */
        }
        else {
            /*
             * Not sure how this would happen, so complain (meekly).
             */
            lc_log_basic(LOG_INFO, "DHCP%s on interface %s: releasing lease "
                         "unexpectedly", ipv6 ? "v6" : "v4", if_name);
        }

        if (is_bond_member) {
            lc_log_basic(LOG_INFO, "DHCP%s on interface %s: not releasing "
                         "lease because we are now a member of a bond",
                         ipv6 ? "v6" : "v4", if_name);
        }
        else {
            err = md_interface_dhcp_release_lease(if_name, ipv6);
            bail_error(err);
        }
    }

    /* ........................................................................
     * If we want a client running, but there isn't one (whether or
     * not because we just killed it), then launch one.  But first,
     * make sure that any pending DHCP-release processes are done
     * (including deleting the lease file after they exit).
     */
    if (should_be_running && !client_launched) {
        err = md_interface_dhcp_release_wait(dhcp_rec);
        bail_error(err);
        err = md_interface_dhcp_launch(commit, db, if_name, ipv6);
        bail_error(err);
    }
    
 bail:
    /* Don't free dhcp_rec, we don't own it */
    safe_free(filename);
    safe_free(bname);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Mostly this is called for a single interface.  But we also support
 * being called with a NULL if_name, which means to check ALL of them!
 * In this case, the 'dhcp_enabled' and 'if_enabled' flags are ignored,
 * and determined for ourselves.
 */
static int
md_interface_check_dhcp(const char *if_name, 
                        md_commit *commit, const mdb_db *db, tbool ipv6,
                        tbool dhcp_enabled, tbool if_enabled, tbool renew,
                        tbool release_lease)
{
    int err = 0;
    tstr_array *ifnames = NULL;
    int i = 0, num_ifs = 0;
    tbool found = false, dv4 = false, dv6 = false;

    if (if_name) {
        err = md_interface_check_dhcp_one(if_name, commit, db, ipv6,
                                          dhcp_enabled, if_enabled, renew,
                                          release_lease);
        bail_error(err);
    }
    else {
        /*
         * What we're doing here is somewhat similar to what
         * md_interface_renew_dhcp_interface_ext() does,  except
         * that function only does it to interfaces which are enabled
         * and have DHCP enabled for the selected protocol.
         */
        err = mdb_get_tstr_array(commit, db, "/net/interface/config",
                                 NULL, false, 0, &ifnames);
        bail_error(err);
        
        num_ifs = tstr_array_length_quick(ifnames);
        for (i = 0; i < num_ifs; ++i) {
            if_name = tstr_array_get_str_quick(ifnames, i);
            bail_null(if_name);
            err = md_interface_get_config_by_ifname
                (if_name, commit, db, &found, &if_enabled, NULL, NULL, NULL,
                 &dv4, NULL, NULL, NULL, NULL, &dv6);
            bail_error(err);
            if (!found) {
                continue;
            }
            dhcp_enabled = ipv6 ? dv6 : dv4;
            err = md_interface_check_dhcp_one(if_name, commit, db, ipv6,
                                              dhcp_enabled, if_enabled, renew,
                                              release_lease);
            bail_error(err);
        }        
    }

 bail:
    tstr_array_free(&ifnames);
    return(err);
}


static int
md_interface_check_zeroconf(const char *if_name, 
                        md_commit *commit, const mdb_db *db,
                        tbool zeroconf_enabled, tbool if_enabled, tbool renew)
{
    int err = 0;
    tbool should_be_running = false;
    tbool pid_file_found = false;
    tbool client_launched = false;
    char *filename = NULL;
    uint32 idx = 0;
    int32 pid_from_file = -1, pid_from_array = -1;

    lc_log_debug(LOG_DEBUG, "Checking to see if zeroconf is running for "
                 "interface %s", if_name);

    should_be_running = zeroconf_enabled && if_enabled;

    err = md_interface_check_for_zeroconf_pid(if_name, &pid_from_file);
    bail_error(err);
    pid_file_found = (pid_from_file != -1);

    err = tstr_array_linear_search_str(md_ifc_zeroconf_names, if_name, 0, 
                                       &idx);
    client_launched = (err != lc_err_not_found);
    err = 0;
    if (client_launched) {
        err = uint32_array_get(md_ifc_zeroconf_pids, idx, 
                               (uint32 *) &pid_from_array);
        bail_error(err);
    }

    if ((!should_be_running && (pid_file_found || client_launched)) ||
        (pid_file_found && !client_launched) ||
        (pid_file_found && renew)) {

        if (pid_from_file > 0) {
            lc_log_basic(LOG_INFO, "Killing zeroconf client, pid %" PRId32,
                         pid_from_file);
            err = lc_kill_and_reap(pid_from_file, SIGTERM, 
                                   md_ifc_zeroconf_kill_time_ms, SIGKILL);
            if (err == lc_err_not_found) {
                err = 0;
            }
            bail_error(err);
            
            filename = smprintf(ZEROCONF_PID_FILENAME_FMT, if_name);
            bail_null(filename);
            
            err = lf_remove_file(filename);
            if (err && errno == ENOENT) {
                /* This is OK, we just wanted to make sure it was gone */
                err = 0;
                errno = 0;
            }
            bail_error_errno(err, "Deleting file %s", filename);
        }

        if (pid_from_array > 0) {
            lc_log_basic(LOG_INFO, "Killing zeroconf client, pid %" PRId32,
                         pid_from_array);
            err = lc_kill_and_reap(pid_from_array, SIGTERM, 
                                   md_ifc_zeroconf_kill_time_ms, SIGKILL);
            if (err == lc_err_not_found) {
                err = 0;
            }
            bail_error(err);

            err = tstr_array_delete(md_ifc_zeroconf_names, idx);
            bail_error(err);
            err = uint32_array_delete(md_ifc_zeroconf_pids, idx);
            bail_error(err);
        }

        err = md_interface_maybe_clear_overrides(if_name, false, commit, db);
        bail_error(err);

        /*
         * This is a hack to help make sure client is really dead by
         * the time we relaunch its replacement.  It should not come
         * up except in error conditions.
         */
        if (should_be_running) {
            lc_log_basic(LOG_WARNING, "Waiting 1 second between killing and "
                         "relaunching zeroconf client");
            sleep(1);
        }
    }

    if (should_be_running && (!client_launched || renew)) {
        err = md_interface_zeroconf_launch(commit, if_name);
        bail_error(err);
    }
    
 bail:
    safe_free(filename);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_interface_check_for_dhcp_pid(const char *if_name, tbool ipv6, int *ret_pid)
{
    int err = 0;
    char *filename = NULL;
    tbool pid_file_exists = false;
    char *pid_str = NULL;
    
    bail_null(ret_pid);
    *ret_pid = -1;

    filename = smprintf(ipv6 ? dhcp6_pid_filename_fmt : 
                        dhcp_pid_filename_fmt, if_name);
    bail_null(filename);

    err = lf_test_exists(filename, &pid_file_exists);
    bail_error(err);

    if (!pid_file_exists) {
        goto bail;
    }

    err = lf_read_file_str(filename, 10, &pid_str, NULL);
    if (err || pid_str == NULL) {
        lc_log_basic(LOG_INFO, "Unable to read pid file %s: %s "
                     "(race condition?)", filename, strerror(errno));
        err = 0;
        goto bail;
    }

    /*
     * This has been observed at least once.  It's probably a race
     * condition where the dhclient was just in the middle of writing
     * its own pid file, and it had just opened it but not written
     * anything yet.
     *
     * There better not be any worse race conditions, where we can
     * read a partially-written file.  Hopefully a write of such a 
     * small block of data is atomic.
     */
    if (strlen(pid_str) == 0) {
        goto bail;
    }

    *ret_pid = strtol(pid_str, NULL, 10);

    if (*ret_pid <= 1) {
        lc_log_basic(LOG_ERR, I_("Somehow read \"%s\" (parsed to %d) out "
                                 "of DHCP%s pid file %s, ignoring it"), 
                     pid_str, *ret_pid, ipv6 ? "v6" : "v4", filename);
        *ret_pid = -1;
    }

 bail:
    safe_free(filename);
    safe_free(pid_str);
    return(err);
}

static int
md_interface_check_for_zeroconf_pid(const char *if_name, int *ret_pid)
{
    int err = 0;
    char *filename = NULL;
    tbool pid_file_exists = false;
    char *pid_str = NULL;
    
    bail_null(ret_pid);
    *ret_pid = -1;

    filename = smprintf(ZEROCONF_PID_FILENAME_FMT, if_name);
    bail_null(filename);

    err = lf_test_exists(filename, &pid_file_exists);
    bail_error(err);

    if (!pid_file_exists) {
        goto bail;
    }

    err = lf_read_file_str(filename, 10, &pid_str, NULL);
    bail_error(err);

    *ret_pid = strtol(pid_str, NULL, 10);

    if (*ret_pid <= 1) {
        lc_log_basic(LOG_ERR, "Somehow read %d out of zeroconf pid file %s, "
                     "ignoring it", *ret_pid, filename);
        *ret_pid = -1;
    }

 bail:
    safe_free(filename);
    safe_free(pid_str);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_dhcp_extra_params(md_commit *commit, const mdb_db *db,
                               tbool ipv6, tstr_array *argv)
{
    int err = 0;
    const char *bname_sh_flag = NULL, *bname_sh_name = NULL;
    tstring *dhcp_hostname = NULL;
    tstring *vendor_class_id = NULL;
    tbool send_hostname = false;
    tbool stateless = false;
    tbool found = false;

    bail_null(db);
    bail_null(argv);

    /* ........................................................................
     * Add "-H" parameter to send hostname, if needed.
     */
    if (ipv6) {
        bname_sh_flag = "/net/general/config/ipv6/dhcp/send_hostname";
        bname_sh_name = "/net/general/config/ipv6/dhcp/hostname";
    }
    else {
        bname_sh_flag = "/net/general/config/dhcp/send_hostname";
        bname_sh_name = "/net/general/config/dhcp/hostname";
    }

    /* If send DHCP hostname is enabled, add -H <dhcp_hostname> */
    err = mdb_get_node_value_tbool(commit, db, bname_sh_flag,
                                   0, &found, &send_hostname);
    bail_error(err);

    /*
     * XXX/EMT: this is only implemented for DHCPv4.
     * Sending hostname for DHCPv6 is NYI: see bug 14464.
     */
    if (ipv6) {
        send_hostname = false;
    }

    if (send_hostname) {
        err = mdb_get_node_value_tstr(commit, db, bname_sh_name,
                                      0, &found, &dhcp_hostname);
        bail_error(err);

        if (!found || !ts_nonempty(dhcp_hostname)) {
            /* default to /system/hostname */
            ts_free(&dhcp_hostname);
            err = mdb_get_node_value_tstr(commit, db, "/system/hostname",
                                          0, &found, &dhcp_hostname);
            bail_error(err);
        }
        complain_null(dhcp_hostname);

        if (dhcp_hostname) {
            err = tstr_array_append_str(argv, "-H");
            bail_error(err);
            err = tstr_array_append_takeover(argv, &dhcp_hostname);
            bail_error(err);
        }
    }

    /* ........................................................................
     * Add "-V" parameter to send vendor class ID, if needed.
     */
#if 0
    /*
     * XXX/EMT: NYI -- see bug 14465.
     */
    err = mdb_get_node_value_tstr(commit, db,
                                  "/net/general/config/dhcp/vendor_class_id",
                                  0, NULL, &vendor_class_id);
    bail_error(err);
    if (ts_nonempty(vendor_class_id)) {
        err = tstr_array_append_str(argv, "-V");
        bail_error(err);
        err = tstr_array_append_takeover(argv, &vendor_class_id);
        bail_error(err);
    }    
#endif

    /* ........................................................................
     * Add "-S" parameter to do a stateless query (for DHCPv6), if needed.
     */
    if (ipv6) {
        found = false;
        err = mdb_get_node_value_tbool
            (commit, db, "/net/general/config/ipv6/dhcp/stateless", 0,
             &found, &stateless);
        bail_error(err);
        if (found && stateless) {
            err = tstr_array_append_str(argv, "-S");
            bail_error(err);
        }
    }

 bail:
    ts_free(&dhcp_hostname);
    ts_free(&vendor_class_id);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_dhcpv4_launch(md_commit *commit, const mdb_db *db,
                           const char *if_name)
{
    int err = 0;
    char *pid_filename = NULL, *lease_filename = NULL;
    lc_launch_params *lp = NULL;
    lc_launch_result lr = LC_LAUNCH_RESULT_INIT;
    int pid = 0;
    tbool one_shot = false;
    md_ifc_dhcp_rec *dhcp_rec = NULL; /* Not owned, DO NOT FREE */

    err = md_commit_get_commit_mode(commit, &one_shot);
    bail_error(err);

    if (one_shot) {
        lc_log_basic(LOG_INFO, "Skipping launching of DHCP client in "
                     "one-shot configuration mode");
        goto bail;
    }

    if (!lc_devel_is_production()) {
        lc_log_debug(LOG_INFO, "Devenv: skipping launch of DHCP client");
        goto bail;
    }

    pid_filename = smprintf(dhcp_pid_filename_fmt, if_name);
    bail_null(pid_filename);

    lease_filename = smprintf(dhcp_lease_filename_fmt, if_name);
    bail_null(lease_filename);

    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);

    /* The child will create files, we don't build it, and it uses the umask */
    lp->lp_umask = 022;

    err = ts_new_str(&(lp->lp_path), prog_dhcp_client);
    bail_error(err);

    /*
     * -4: do DHCPv4
     * -q: don't print error messages
     * -lf: override default lease filename
     * -pf: override default pid filename
     * -sf: override default DHCP client script path
     * -cf: override default config file path
     * -d: don't daemonize, so we can track the pid directly
     */
    err = tstr_array_new_va_str
        (&(lp->lp_argv), NULL, 12,
         prog_dhcp_client, "-4", "-q", "-lf", lease_filename, 
         "-pf", pid_filename, "-sf", DHCP_CLIENT_SCRIPT_PATH,
         "-cf", dhcp_config_path, "-d");
    bail_error(err);

    err = md_interface_dhcp_extra_params(commit, db, false, lp->lp_argv);
    bail_error(err);

    /*
     * This has to come last.
     */
    err = tstr_array_append_str(lp->lp_argv, if_name);
    bail_error(err);

    lp->lp_log_level = LOG_INFO;
    lp->lp_wait = false;

    /*
     * We pass "-q", but "-d" overrides this and forces
     * verbosity.  So just stifle anything it prints.  Presumably
     * anything important will also go to the logs.
     */
    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_devnull;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_devnull;

    /*
     * XXX #dep/args: dhclient
     *
     * dhclient -4 -q -lf <leasefile> -pf <pidfile> -sf <script>
     *          -cf <configfile> -d [...OTHER PARAMS...] <ifname>
     */
    err = lc_launch(lp, &lr);
    bail_error(err);
    bail_require(lr.lr_child_pid > 0);

    /*
     * We'd like to use the DHCP client's pid file to remember the pid
     * of the client.  But it does not write the pid file until after
     * it has successfully acquired an address and run the dhclient
     * script.  Since the script blocks on action requests to mgmtd,
     * we couldn't wait for this even if we were patient enough.  So
     * instead, keep our own records of the pids of running clients.
     * The pid file can, however, be useful to remind us of a pid of a
     * client that is leftover from a previous run.
     */

    err = md_ifc_dhcp_rec_lookup(if_name, false, true, &dhcp_rec);
    bail_error_null(err, dhcp_rec);

    dhcp_rec->midr_dhclient_pid = lr.lr_child_pid;

    err = lc_child_completion_register
        (md_cmpl_hand, dhcp_rec->midr_dhclient_pid,
         md_interface_dhcp_exit, NULL);
    bail_error(err);

 bail:
    /* Don't free dhcp_rec, we don't own it */
    safe_free(pid_filename);
    safe_free(lease_filename);
    lc_launch_params_free(&lp);
    lc_launch_result_free_contents(&lr);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_dhcpv6_launch(md_commit *commit, const mdb_db *db,
                           const char *if_name)
{
    int err = 0;
    char *pid_filename = NULL, *lease_filename = NULL;
    lc_launch_params *lp = NULL;
    lc_launch_result lr = LC_LAUNCH_RESULT_INIT;
    int pid = 0;
    tbool one_shot = false;
    md_ifc_dhcp_rec *dhcp_rec = NULL; /* Not owned, DO NOT FREE */

    err = md_commit_get_commit_mode(commit, &one_shot);
    bail_error(err);

    if (one_shot) {
        lc_log_basic(LOG_INFO, "Skipping launching of DHCPv6 client in "
                     "one-shot configuration mode");
        goto bail;
    }

    if (!lc_devel_is_production()) {
        lc_log_debug(LOG_INFO, "Devenv: skipping launch of DHCPv6 client");
        goto bail;
    }

    pid_filename = smprintf(dhcp6_pid_filename_fmt, if_name);
    bail_null(pid_filename);

    lease_filename = smprintf(dhcp6_lease_filename_fmt, if_name);
    bail_null(lease_filename);

    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);

    /* The child will create files, we don't build it, and it uses the umask */
    lp->lp_umask = 022;

    err = ts_new_str(&(lp->lp_path), prog_dhcp6_client);
    bail_error(err);

    /*
     * -6: do DHCPv6
     * -q: don't print error messages (XXXX/EMT: has no effect with '-d')
     * -lf: override default lease filename
     * -pf: override default pid filename
     * -sf: override default DHCP client script path
     * -cf: override default config file path
     * -d: don't daemonize, so we can track the pid directly
     */
    err = tstr_array_new_va_str
        (&(lp->lp_argv), NULL, 12,
         prog_dhcp6_client, "-6", "-q", "-lf", lease_filename, 
         "-pf", pid_filename, "-sf", DHCP6_CLIENT_SCRIPT_PATH,
         "-cf", dhcp6_config_path, "-d");
    bail_error(err);

    err = md_interface_dhcp_extra_params(commit, db, true, lp->lp_argv);
    bail_error(err);

    /*
     * This has to come last.
     */
    err = tstr_array_append_str(lp->lp_argv, if_name);
    bail_error(err);

    lp->lp_log_level = LOG_INFO;
    lp->lp_wait = false;

    /*
     * We pass "-q", but "-d" overrides this and forces verbosity.  So
     * just stifle anything it prints.  Presumably anything important
     * will also go to the logs.
     */
    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_devnull;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_devnull;

    /*
     * XXX #dep/args: dhclient
     *
     * dhclient -6 -q -lf <leasefile> -pf <pidfile> -sf <script>
     *          -cf <configfile> -d [...OTHER PARAMS...] <ifname>
     */
    err = lc_launch(lp, &lr);
    bail_error(err);
    bail_require(lr.lr_child_pid > 0);

    /*
     * We'd like to use the DHCP client's pid file to remember the pid
     * of the client.  But it does not write the pid file until after
     * it has successfully acquired an address and run the dhclient
     * script.  Since the script blocks on action requests to mgmtd,
     * we couldn't wait for this even if we were patient enough.  So
     * instead, keep our own records of the pids of running clients.
     * The pid file can, however, be useful to remind us of a pid of a
     * client that is leftover from a previous run.
     */

    err = md_ifc_dhcp_rec_lookup(if_name, true, true, &dhcp_rec);
    bail_error_null(err, dhcp_rec);

    dhcp_rec->midr_dhclient_pid = lr.lr_child_pid;

    err = lc_child_completion_register
        (md_cmpl_hand, dhcp_rec->midr_dhclient_pid,
         md_interface_dhcp_exit, NULL);
    bail_error(err);

 bail:
    /* Don't free dhcp_rec, we don't own it */
    safe_free(pid_filename);
    safe_free(lease_filename);
    lc_launch_params_free(&lp);
    lc_launch_result_free_contents(&lr);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_interface_dhcp_launch(md_commit *commit, const mdb_db *db,
                         const char *if_name, tbool ipv6)
{
    int err = 0;
    const tstr_array *all_ifs = NULL;
    uint32 idx = 0;

    err = md_interface_get_ifnames_cached(md_interface_ifnames_cache_ms,
                                          false, &all_ifs);
    bail_error_null(err, all_ifs);

    err = tstr_array_linear_search_str(all_ifs, if_name, 0, &idx);
    if (err == lc_err_not_found) {
        lc_log_basic(LOG_INFO, "Skipping DHCP for nonexistent interface '%s'",
                     if_name);
        err = 0;
        goto bail;
    }
    
    if (ipv6) {
        err = md_interface_dhcpv6_launch(commit, db, if_name);
        bail_error(err);
    }
    else {
        err = md_interface_dhcpv4_launch(commit, db, if_name);
        bail_error(err);
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
/* If an interface link comes up, we want to check its DHCP status.
 * If it is supposed to be running DHCP, and already has a client
 * running, we should bounce it (per bug 14488).
 */
static int
md_if_config_linkup(md_commit *commit, const mdb_db *db,
                    const char *event_name, const bn_binding_array *bindings,
                    void *cb_reg_arg, void *cb_arg)
{
    int err = 0;
    tstring *ifname = NULL;
    md_ifc_dhcp_rec *dhcp_rec = NULL;

    err = bn_binding_array_get_value_tstr_by_name(bindings, "ifname", NULL,
                                                  &ifname);
    bail_error(err);
    bail_null_msg(ifname, "Got linkup event without ifname!");

    lc_log_basic(LOG_INFO, "DHCP: link on interface '%s' just came up, "
                 "checking for DHCP clients to bounce...", ts_str(ifname));

    /*
     * Check IPv4.
     */
    err = md_ifc_dhcp_rec_lookup(ts_str(ifname), false, false, &dhcp_rec);
    bail_error(err);
    if (dhcp_rec) {
        err = md_interface_check_dhcp_fresh(dhcp_rec, true, false);
        bail_error(err);
    }

    /*
     * Check IPv6.
     */
    if (md_proto_ipv6_supported()) {
        err = md_ifc_dhcp_rec_lookup(ts_str(ifname), true, false, &dhcp_rec);
        bail_error(err);
        if (dhcp_rec) {
            err = md_interface_check_dhcp_fresh(dhcp_rec, true, false);
            bail_error(err);
        }
    }

 bail:
    ts_free(&ifname);
    return(err);
}


static int
md_ifc_zeroconf_launch_handler(int fd, short event_type, void *data,
                               lew_context *ctxt)
{
    int err = 0;
    const char *ifname = NULL;
    md_commit *commit = NULL;

    err = md_commit_create_commit(&commit);
    bail_error(err);

    /*
     * We could have just iterated over the array and emptied it at the
     * end, but this way we don't have to assume that it will not be
     * changed out from under us while we're iterating.
     */
    while (tstr_array_length_quick(md_ifc_zeroconf_ifs_retry) > 0) {
        ifname = tstr_array_get_str_quick(md_ifc_zeroconf_ifs_retry, 0);
        bail_null(ifname);
        lc_log_basic(LOG_INFO, "Retrying zeroconf for %s", ifname);
        err = md_interface_zeroconf_launch(commit, ifname);
        bail_error(err);
        err = tstr_array_delete(md_ifc_zeroconf_ifs_retry, 0);
        bail_error(err);
    }

    /*
     * Now there are none left to retry, so no need to reset the timer.
     * If someone puts another one in the queue, they will do so.
     */
    
 bail:
    md_commit_commit_state_free(&commit);
    return(err);
}


static int
md_interface_zeroconf_exit(lc_child_completion_handle *cmpl_hand,
                           pid_t pid, int wait_status, tbool completed,
                           void *arg)
{
    int err = 0;
    tstring *ifname_saved = arg;
    const char *ifname = NULL;
    pid_t pid_saved = 0;
    uint32 idx = 0;
    tbool is_pending = false;

    /*
     * This can happen at least on deinit -- ignore it.
     */
    if (completed == false) {
        goto bail;
    }

    /*
     * If the process did not exit normally (probably meaning it was
     * terminated by a signal), we don't want to interfere.  Other
     * code in this module terminates zeroconf with SIGTERM, and then
     * immediately updates the arrays.
     */
    if (!WIFEXITED(wait_status)) {
        goto bail;
    }

    /*
     * If the process exited normally, and with no error, we also
     * don't need to get involved.  We could remove the pid/ifname
     * from that arrays, but we're trying to follow a policy of
     * minimal risk, and this path worked fine before.
     */
    if (WEXITSTATUS(wait_status) == 0) {
        goto bail;
    }

    /*
     * If we get here, zeroconf exited with an error, suggesting that
     * it had some sort of trouble.  One possibility is a timeout on
     * mgmtd during the initial commit (see bug 12071).
     *
     * We are told the pid by the infrastructure, and we have the
     * original interface name from the callback data.  Double check
     * in our global arrays that these match.  This should probably
     * never fail, it's mainly just a sanity check.
     */
    lc_log_basic(LOG_INFO, "Zeroconf for interface %s exited with code %d",
                 ifname, WEXITSTATUS(wait_status));
    err = uint32_array_linear_search(md_ifc_zeroconf_pids, pid, 0, &idx);
    if (err == lc_err_not_found) {
        /*
         * Not sure exactly why this would come up, but it's best not
         * to get involved.
         */
        err = 0;
        goto bail;
    }
    ifname = tstr_array_get_str_quick(md_ifc_zeroconf_names, idx);
    if (ifname == NULL || !ts_equal_str(ifname_saved, ifname, false)) {
        /*
         * Not sure why this would happen either, but it seems a bit
         * more worrisome.
         */
        lc_log_basic(LOG_WARNING, "Pid %" LPRI_d_ID_T " used to be for "
                     "interface %s, but now for interface %s!", pid,
                     ts_str(ifname_saved), 
                     ifname ? ifname : "(NONE)");
        goto bail;
    }

    /*
     * Everything seems on the level here.  So we want to relaunch
     * zeroconf, and give it another chance.
     *
     * XXXX/EMT: how many chances do we give it?  I got this to repeat
     * ad infinitum.
     */
    lc_log_basic(LOG_NOTICE, "Zeroconf for interface %s either (a) could not "
                 "get an address, or (b) could not apply its address to the "
                 "system.  Maybe the management subsystem was too busy.  "
                 "Trying again in %" PRId64 " seconds or less...", ifname,
                 md_ifc_zeroconf_retry_interval_ms / 1000);
    
    err = tstr_array_append_str(md_ifc_zeroconf_ifs_retry, ifname);
    bail_error(err);

    /*
     * Don't relaunch zeroconf right away, per bug 12310.  At least
     * wait a few seconds.  But for simplicity, we want to have just a
     * single timer, no matter how many zeroconf interfaces there are.
     * So we'll retry a maximum of every few seconds, and retry all of
     * them at once.
     */
    err = lew_event_is_pending(md_lwc, (const lew_event **)
                               &md_ifc_zeroconf_launch_timer,
                               &is_pending, NULL);
    bail_error(err);
    if (!is_pending) {
        err = lew_event_reg_timer_full(md_lwc, &md_ifc_zeroconf_launch_timer,
                                  "md_ifc_zeroconf_launch_timer",
                                  "md_ifc_zeroconf_launch_handler",
                                  md_ifc_zeroconf_launch_handler, NULL,
                                  md_ifc_zeroconf_retry_interval_ms);
        bail_error(err);
    }

 bail:
    ts_free(&ifname_saved);
    return(err);
}


static int
md_interface_zeroconf_launch(md_commit * commit, const char *if_name)
{
    int err = 0;
    lc_launch_params *lp = NULL;
    char *pid_filename = NULL;
    lc_launch_result lr = LC_LAUNCH_RESULT_INIT;
    tbool one_shot = false;
    tstring *if_name_saved = NULL;

    bail_null(commit);

    err = md_commit_get_commit_mode(commit, &one_shot);
    bail_error(err);

    if (one_shot) {
        lc_log_basic(LOG_INFO, "Skipping launching of zeroconf client in "
                     "one-shot configuration mode");
        goto bail;
    }

    pid_filename = smprintf(ZEROCONF_PID_FILENAME_FMT, if_name);
    bail_null(pid_filename);

    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);

    /*
     * The child will create files, and it uses the umask.
     * We do build it, but would rather not modify if we can avoid it.
     */
    lp->lp_umask = 022;

    err = ts_new_str(&(lp->lp_path), prog_zeroconf);
    bail_error(err);

    /*
     * -n: do not fork a daemon
     * -q: quit after obtaining address or failing
     * -s: script file
     * -i: specify interface name
     */
    err = tstr_array_new_va_str
        (&(lp->lp_argv), NULL, 7,
         prog_zeroconf, "-n", "-q", "-s", ZEROCONF_SCRIPT_PATH, "-i",
         if_name);
    bail_error(err);

    lp->lp_log_level = LOG_INFO;
    lp->lp_wait = false;

    /*
     * XXX #dep/args: zeroconf
     *
     * zeroconf -n -q -s <script> -i <ifname>
     */
    err = lc_launch(lp, &lr);
    bail_error(err);
    bail_require(lr.lr_child_pid > 0);

    bail_require(tstr_array_length_quick(md_ifc_zeroconf_names) ==
                 uint32_array_length_quick(md_ifc_zeroconf_pids));

    err = tstr_array_append_str(md_ifc_zeroconf_names, if_name);
    bail_error(err);

    err = uint32_array_append(md_ifc_zeroconf_pids, lr.lr_child_pid);
    bail_error(err);

    /*
     * Register to be told when zeroconf client exits.
     * In particular, we want to know if it failed, since if it did,
     * we'll probably want to relaunch it.
     */
    err = ts_new_str(&if_name_saved, if_name);
    bail_error(err);
    err = lc_child_completion_register
        (md_cmpl_hand, lr.lr_child_pid,
         md_interface_zeroconf_exit, if_name_saved);
    bail_error(err);

  bail:
    /* Do NOT free if_name_saved; md_interface_zeroconf_exit() will do so */
    safe_free(pid_filename);
    lc_launch_params_free(&lp);
    lc_launch_result_free_contents(&lr);
    return (err);
}

static int
md_ifc_gratarp_resched_handler(int fd, short event_type, void *data,
                     lew_context *ctxt)
{
    int err = 0;
    uint32 num_aliases = 0, i = 0, curr = 0;
    tbool has_more = false;
    tstr_array *tokens = NULL;
    const char *a_ifname = NULL, *a_ip = NULL, *a_tag = NULL;

    /* 
     * Walk our array of counts.  For each one that is > 0, decrement,
     * and do the gratuitous arp.
     */

    num_aliases = 
        uint32_array_length_quick(md_interface_aliases_gratarp_count);
    for (i = 0; i < num_aliases; i++) {
        curr = uint32_array_get_quick(md_interface_aliases_gratarp_count, i);
        if (curr == 0) {
            continue;
        }

        tstr_array_free(&tokens);
        err = ts_tokenize(tstr_array_get_quick(md_interface_aliases, i),
                          ',', '\0', '\0', 0, &tokens);
        bail_error_null(err, tokens);

        bail_require(tstr_array_length_quick(tokens) == 4);

        a_ifname = tstr_array_get_str_quick(tokens, 0);
        a_ip = tstr_array_get_str_quick(tokens, 1);
        /* a_tag = tstr_array_get_str_quick(tokens, 3); */
        if (a_ifname && strchr(a_ifname, ':')) {
            *(strchr(a_ifname, ':')) = '\0';
        }

        /*
         * Send gratuitous ARP for new alias address on interface
         * Both a request and a reply
         */
        err = md_interface_grat_arp(NULL, NULL, a_ifname,
                                    a_ip, true);
        bail_error(err);
        err = md_interface_grat_arp(NULL, NULL, a_ifname,
                                    a_ip, false);
        bail_error(err);

        if (curr != UINT32_MAX) {
            curr--;
        }
        err = uint32_array_set(md_interface_aliases_gratarp_count, i, curr);
        bail_error(err);
        if (curr > 0) {
            has_more = true;
        }
    }

    if (has_more) {
        err = lew_event_reg_timer_full(md_lwc, &(md_ifc_gratarp_resched_timer),
                                  "md_ifc_gratarp_resched_timer",
                                  "md_ifc_gratarp_resched_handler",
                                  md_ifc_gratarp_resched_handler, NULL,
                                  md_ifc_gratarp_resched_interval_ms);
        bail_error(err);
    }

 bail:
    tstr_array_free(&tokens);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* NOTE: do not call this directly!  Call md_ifc_kill_all_dhclients
 * instead.  If you call this directly, it will leave incorrect
 * entries in md_ifc_dhcp_recs.
 */
static int 
md_ifc_kill_all_dhclients_onefam(tbool ipv6)
{
    int err = 0;
    tstr_array *paths = NULL;
    const char *path = NULL;
    char *pid_str = NULL;
    char *pid_filename = NULL;
    int pid = 0;
    uint32 num_paths = 0, num_procs = 0, i = 0;
    int pass = 0;
    md_ifc_dhcp_rec *dhcp_rec = NULL;

    /* 
     * We take two passes below so that the kills can proceed in parallel.
     * This becomes important if you have many interfaces running dhcp.
     */

    /* First kill the ones we know we launched */
    for (pass = 0; pass < 2; pass++) {
        num_procs = md_ifc_dhcp_rec_array_length_quick(md_ifc_dhcp_recs);
        for (i = 0; i < num_procs; ++i) {
            dhcp_rec = md_ifc_dhcp_rec_array_get_quick(md_ifc_dhcp_recs, i);
            bail_null(dhcp_rec);
            if (dhcp_rec->midr_ipv6 != ipv6) {
                continue;
            }
            pid = dhcp_rec->midr_dhclient_pid;
            if (pid > 0) {
                if (pass == 0) {
                    lc_log_basic(LOG_INFO, "Killing DHCP%s client on "
                                 "interface '%s', pid %d",
                                 ipv6 ? "v6" : "v4", dhcp_rec->midr_ifname,
                                 pid);
                    err = kill(pid, SIGTERM);
                    if (err) {
                        if (errno == ESRCH) {
                            err = 0;
                        }
                        else {
                            bail_error_errno(err, _("Could not kill pid %s"),
                                             pid_str);
                        }
                    }
                }
                else {
                    err = lc_kill_and_reap(pid, SIGTERM,
                                           md_ifc_dhcp_kill_time_ms, SIGKILL);
                    if (err == lc_err_not_found) {
                        err = 0;
                    }
                    else {
                        bail_error(err);

                        /*
                         * In case the process did not clean up its
                         * pid file on exit, delete it now so we won't
                         * detect it and redundantly try to kill the
                         * process again below.
                         */
                        safe_free(pid_filename);
                        pid_filename = smprintf(ipv6 ? dhcp6_pid_filename_fmt :
                                                dhcp_pid_filename_fmt,
                                                dhcp_rec->midr_ifname);
                        bail_null(pid_filename);
                        err = lf_remove_file(pid_filename);
                        err = 0; /* The file may not be there anymore */
                    }
                }
            }
        }
    }

    /* Now kill any ones still running */
    for (pass = 0; pass < 2; pass++) {
        err = tstr_array_free(&paths);
        bail_error(err);
        err = lf_dir_list_paths(dhcp_pid_file_dir, &paths);
        bail_error(err);

        num_paths = tstr_array_length_quick(paths);
        for (i = 0; i < num_paths; ++i) {
            safe_free(pid_str);
            path = tstr_array_get_str_quick(paths, i);
            bail_null(path);
            if (lc_is_prefix(ipv6 ? dhcp6_pid_filename_prefix : 
                             dhcp_pid_filename_prefix, path, false) &&
                lc_is_suffix(dhcp_pid_filename_suffix, path, false)) {
                err = lf_read_file(path, &pid_str, NULL);
                if (err == 0 && pid_str) {
                    pid = strtol(pid_str, NULL, 10);
                    if (pid > 1) {
                        if (lc_devel_is_production()) {
                            lc_log_basic(LOG_INFO, "Killing DHCP%s client "
                                         "(leftover), pid %d",
                                         ipv6 ? "v6" : "v4", pid);
                            if (pass == 0) {
                                err = kill(pid, SIGTERM);
                                if (err) {
                                    if (errno == ESRCH) {
                                        err = 0;
                                    }
                                    else {
                                        bail_error_errno(err, _("Could not kill pid %s"), pid_str);
                                    }
                                }
                            }
                            else {
                                err = lc_kill_and_reap
                                    (pid, SIGTERM, md_ifc_dhcp_kill_time_ms, SIGKILL);
                                if (err == lc_err_not_found) {
                                    err = 0;
                                }
                                bail_error(err);
                            }
                        }
                        else {
                            lc_log_debug(LOG_INFO, "Devenv: not killing "
                                         "DHCP%s client, pid %d", 
                                         ipv6 ? "v6" : "v4", pid);
                        }
                    }
                    if (lc_devel_is_production()) {
                        err = lf_remove_file(path);
                        complain_error_msg(err, "Could not remove file %s",
                                           path);
                        err = 0;
                    }
                    else {
                        lc_log_debug(LOG_INFO, "Devenv: not removing DHCP%s "
                                     "pid file %s", ipv6 ? "v6" : "v4", path);
                    }
                }
            }
        }
    }

 bail:
    tstr_array_free(&paths);
    safe_free(pid_str);
    safe_free(pid_filename);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Do NOT call this except at init and deinit, since we save a pointer
 * to one of these as a passback 'void *' for a timer.  (XXX/EMT: we should
 * just save the ifname/family, and look it up again...)
 */
int 
md_ifc_kill_all_dhclients(void)
{
    int err = 0;

    err = md_ifc_kill_all_dhclients_onefam(false);
    bail_error(err);

    err = md_ifc_kill_all_dhclients_onefam(true);
    bail_error(err);

    err = md_ifc_dhcp_rec_array_empty(md_ifc_dhcp_recs);
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
int 
md_ifc_kill_all_zeroconfs(void)
{
    int err = 0;
    tstr_array *paths = NULL;
    const char *path = NULL;
    char *pid_str = NULL;
    int pid = 0;
    uint32 num_paths = 0, num_procs = 0, i = 0;
    int pass = 0;

    /* 
     * We take two passes below so that the kills can proceed in parallel.
     * This becomes important if you have many interfaces running zeroconf.
     */

    /* First kill the ones we know we launched */
    for (pass = 0; pass < 2; pass++) {
        num_procs = uint32_array_length_quick(md_ifc_zeroconf_pids);
        for (i = 0; i < num_procs; ++i) {
            pid = uint32_array_get_quick(md_ifc_zeroconf_pids, i);
            if (pid > 0) {
                lc_log_basic(LOG_INFO, "Killing zeroconf client, pid %d", pid);
                if (pass == 0) {
                    err = kill(pid, SIGTERM);
                    if (err) {
                        if (errno == ESRCH) {
                            err = 0;
                        }
                        else {
                            bail_error_errno(err, _("Could not kill pid %s"), pid_str);
                        }
                    }
                }
                else {
                    err = lc_kill_and_reap(pid, SIGTERM,
                                           md_ifc_zeroconf_kill_time_ms, SIGKILL);
                    if (err == lc_err_not_found) {
                        err = 0;
                    }
                    bail_error(err);
                }
            }
        }
    }

    /* Now kill any ones still running */
    for (pass = 0; pass < 2; pass++) {
        err = tstr_array_free(&paths);
        bail_error(err);
        err = lf_dir_list_paths(zeroconf_pid_file_dir, &paths);
        bail_error(err);

        num_paths = tstr_array_length_quick(paths);
        for (i = 0; i < num_paths; ++i) {
            safe_free(pid_str);
            path = tstr_array_get_str_quick(paths, i);
            bail_null(path);
            if (lc_is_prefix(zeroconf_pid_filename_prefix, path, false) &&
                lc_is_suffix(zeroconf_pid_filename_suffix, path, false)) {
                err = lf_read_file(path, &pid_str, NULL);
                if (err == 0 && pid_str) {
                    pid = strtol(pid_str, NULL, 10);
                    if (pid > 1) {
                        if (lc_devel_is_production()) {
                            lc_log_basic(LOG_INFO, "Killing zeroconf client, "
                                         "pid %d", pid);
                            if (pass == 0) {
                                err = kill(pid, SIGTERM);
                                if (err) {
                                    if (errno == ESRCH) {
                                        err = 0;
                                    }
                                    else {
                                        bail_error_errno(err, _("Could not kill pid %s"), pid_str);
                                    }
                                }
                            }
                            else {
                                err = lc_kill_and_reap
                                    (pid, SIGTERM, md_ifc_zeroconf_kill_time_ms, SIGKILL);
                                if (err == lc_err_not_found) {
                                    err = 0;
                                }
                                bail_error(err);
                            }
                        }
                        else {
                            lc_log_debug(LOG_INFO, "Devenv: not killing zeroconf "
                                         "client, pid %d", pid);
                        }
                    }
                    err = lf_remove_file(path);
                    complain_error_msg(err, "Could not remove file %s", path);
                    err = 0;
                }
            }
        }
    }


    err = tstr_array_empty(md_ifc_zeroconf_names);
    bail_error(err);

    err = uint32_array_empty(md_ifc_zeroconf_pids);
    bail_error(err);

 bail:
    tstr_array_free(&paths);
    safe_free(pid_str);
    return(err);
}

/*****************************************************************************/
/*****************************************************************************/
/*** COMMIT SIDE EFFECTS                                                   ***/
/*****************************************************************************/
/*****************************************************************************/

/* ------------------------------------------------------------------------- */
/* NOTE: we have to know that the default for both speed and duplex is
 * auto.  If an interface record is created by having its speed or
 * duplex set manually, we will see one node showing up as manual and
 * another showing up as auto, and not know which is the one the
 * request set.  In this case, we assume the manual one was set, and
 * that the other one was created implicitly.  Thus, nodes created set
 * to auto are ignored, while nodes created set to manual are treated
 * the same as modifications.
 */
static int
md_interface_commit_side_effects(md_commit *commit, 
                                 const mdb_db *old_db, mdb_db *inout_new_db, 
                                 mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    uint32 num_changes = 0, i = 0;
    tbool is_duplex = false, is_speed = false;
    uint16 mtu = 0;
    uint32 min_mtu = 0;
    mdb_db_change *change = NULL;
    bn_attrib *new_value = NULL;
    bn_attrib *new_attrib = NULL;
    char *new_value_str = NULL;
    char *ifname = NULL;
    char *other_name = NULL;
    tstring *other_value = NULL;
    bn_binding *binding = NULL;
    tbool this_auto = false, other_auto = false;
    tbool dhcpv4_enabled = false, zeroconf_enabled = false;
    tbool ipv6_if_enabled = false;
    char *node_name = NULL;
    bn_attrib *attrib = NULL;
    const bn_attrib *old_attrib = NULL;
    tstr_array *ifc_dh_enabled = NULL, *ifc_zc_enabled = NULL;
    uint32 num_ifc = 0;
    uint32 idx = 0;
    const char *ifc_name = NULL;

    err = tstr_array_new(&ifc_dh_enabled, NULL);
    bail_error(err);

    err = tstr_array_new(&ifc_zc_enabled, NULL);
    bail_error(err);

    num_changes = mdb_db_change_array_length_quick(change_list);
    for (i = 0; i < num_changes; ++i) {
        change = mdb_db_change_array_get_quick(change_list, i);
        bail_null(change);

        if (!ts_has_prefix_str(change->mdc_name, "/net/interface/",
                               false)) {
            continue;
        }

        /*
         * If IPv6 is enabled on the interface and PROD_FEATURE_IPV6 is
         * on, ensure that the MTU is at least 1280, as otherwise the
         * kernel would automatically disable IPv6 on the interface.
         *
         * XXX/SML: To keep things simpler, we'll maintain this
         * constraint even if IPv6 is administratively disabled at the
         * system level.
         */
        if ((md_proto_ipv6_feature_enabled()) &&
            (change->mdc_change_type != mdct_delete) &&
            (bn_binding_name_parts_pattern_match
             (change->mdc_name_parts, true, 
              "/net/interface/config/*/addr/ipv6/enable") ||
             bn_binding_name_parts_pattern_match
             (change->mdc_name_parts, true, 
             "/net/interface/config/*/mtu"))) {
            ifc_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);
            safe_free(node_name);
            node_name = smprintf("net/interface/config/%s/addr/ipv6/enable",
                                 ifc_name);
            bail_null(node_name);
            err = mdb_get_node_value_tbool(commit, inout_new_db, node_name,
                                           0, NULL, &ipv6_if_enabled);
            bail_error(err);

            if (ipv6_if_enabled) {
                bn_binding_free(&binding);
                err = mdb_get_node_binding_fmt(commit, inout_new_db, 0,
                                               &binding,
                                               "/net/interface/config/%s/mtu",
                                               ifc_name);
                bail_error(err);
                err = bn_binding_get_attrib(binding, ba_value, &old_attrib);
                bail_error(err);
                err = bn_attrib_get_uint16(old_attrib, NULL, NULL, &mtu);
                bail_error(err);

                if (mtu < md_ifc_ipv6_min_mtu) {
                    /*
                     * If the MTU is already below the valid range, we don't
                     * want to modify it in side-effects, but let it be
                     * rejected on commit_check.  Also, loopback interface
                     * changes are not allowed.
                     */
                    min_mtu = md_ifc_ether_min_mtu;
                    if (safe_strcmp(ifc_name, "ib0") == 0) {
                        min_mtu = md_ifc_infiniband_min_mtu;
                    }
                    if (mtu >= min_mtu && (safe_strcmp(ifc_name, "lo") != 0)) {
                        /* Bump the MTU to md_ifc_ipv6_min_mtu */
                        bn_attrib_free(&new_attrib);
                        err = bn_attrib_new_uint16(&new_attrib, ba_value, 0,
                                                   md_ifc_ipv6_min_mtu);
                        bail_error_null(err, new_attrib);
                        err = bn_binding_set_attrib_takeover(binding, ba_value,
                                                             &new_attrib);
                        bail_error(err);
                        err = mdb_set_node_binding(commit, inout_new_db,
                                                   bsso_modify, 0, binding);
                        bail_error(err);
                        err = md_commit_set_return_status_msg_fmt
                            (commit, 0,
                             _("Interface %s MTU configuration changed "
                               "from %hu to %hu to support IPv6"),
                             ifc_name,
                             mtu, md_ifc_ipv6_min_mtu);
                    }
                }
            }
        }

        is_duplex = bn_binding_name_parts_pattern_match
            (change->mdc_name_parts, true,
             "/net/interface/config/*/type/ethernet/duplex");
        is_speed = bn_binding_name_parts_pattern_match
            (change->mdc_name_parts, true,
             "/net/interface/config/*/type/ethernet/speed");

        if (bn_binding_name_parts_pattern_match
            (change->mdc_name_parts, true,
             "/net/interface/config/*/addr/ipv4/dhcp")) {

            if (!change->mdc_new_attribs) {
                continue;
            }
                
            err = bn_attrib_array_get
                (change->mdc_new_attribs, ba_value, &attrib);
            bail_error(err);
            if (!attrib) {
                continue;
            }

            err = bn_attrib_get_tbool(attrib, NULL, NULL, &dhcpv4_enabled);
            bail_error(err);

            if (dhcpv4_enabled) {
                /*
                 * Wait to set all of the dhcp enabled interface's
                 * zeroconf state to false until after all changes
                 * have been processed.
                 */
                ifc_name = tstr_array_get_str_quick(change->mdc_name_parts, 3);

                err = tstr_array_append_str(ifc_dh_enabled, ifc_name);
                bail_error(err);
            }
            continue;
        }

        if (bn_binding_name_parts_pattern_match
            (change->mdc_name_parts, true,
             "/net/interface/config/*/addr/ipv4/zeroconf")) {

            if (!change->mdc_new_attribs) {
                continue;
            }
                
            err = bn_attrib_array_get
                (change->mdc_new_attribs, ba_value, &attrib);
            bail_error(err);
            if (!attrib) {
                continue;
            }

            err = bn_attrib_get_tbool(attrib, NULL, NULL, &zeroconf_enabled);
            bail_error(err);

            if (zeroconf_enabled) {
                ifc_name = tstr_array_get_str_quick(change->mdc_name_parts,
                                                       3);

                err = tstr_array_append_str(ifc_zc_enabled, ifc_name);
                bail_error(err);
            }
            continue;
        }
        
        if (!is_duplex && !is_speed) {
            continue;
        }

        bail_require(!is_duplex || !is_speed);
        
        safe_free(ifname);
        safe_free(other_name);
        err = md_ifc_side_effect_get_names
            (change->mdc_name_parts, is_duplex, &ifname, &other_name);
        bail_error(err);

        if (!strcmp(ifname, "lo")) {
            continue;
        }

        if (change->mdc_new_attribs == NULL) {
            continue; /* Maybe the whole interface was deleted? */
        }

/* ========================================================================= */
/* Customer-specific graft point 3: speed/duplex side effects.
 * Enforce your own side effects on the speed and duplex config nodes.
 *
 * You can read from the following variables:
 *   - old_db
 *   - inout_new_db
 *   - change (guaranteed to be either a speed or duplex node)
 *   - is_duplex (set iff 'change' refers to a duplex node)
 *   - is_speed (set iff 'change' refers to a speed node)
 *   - ifname (name of interface in question)
 *   - other_name (full binding node name of other S/D binding for this
 *     interface; speed node if is_duplex; duplex node if is_speed)
 *
 * If you want to enforce any side effects, modify inout_new_db.
 * If you want to prevent us from doing our normal side effects on
 * this change record, use 'continue'.
 * =========================================================================
 */
#ifdef INC_MD_IF_CONFIG_INC_GRAFT_POINT
#undef MD_IF_CONFIG_INC_GRAFT_POINT
#define MD_IF_CONFIG_INC_GRAFT_POINT 3
#include "../bin/mgmtd/modules/md_if_config.inc.c"
#endif /* INC_MD_IF_CONFIG_INC_GRAFT_POINT */

        new_value = bn_attrib_array_get_quick
            (change->mdc_new_attribs, ba_value);
        if (new_value == NULL) {
            continue; /* Maybe the whole interface was deleted? */
        }

        safe_free(new_value_str);
        err = bn_attrib_get_str
            (new_value, NULL, bt_string, NULL, &new_value_str);
        bail_error_null(err, new_value_str);

        this_auto = !strcmp(new_value_str, "auto");

        err = ts_free(&other_value);
        bail_error(err);
        err = mdb_get_node_attrib_tstr_fmt
            (commit, inout_new_db, 0, ba_value, NULL, &other_value,
             "%s", other_name);
        bail_error(err);

        other_auto = other_value && ts_equal_str(other_value, "auto", false);

        /*
         * If one changed from non-auto to auto, change the other to auto.
         */
        if (this_auto && change->mdc_change_type == mdct_modify &&
            !other_auto) {
            lc_log_basic(LOG_INFO, "Interface %s %s set to auto; "
                         "changing %s to auto", ifname,
                         is_duplex ? "duplex" : "speed", other_name);
            bn_binding_free(&binding);
            err = bn_binding_new_str
                (&binding, other_name, ba_value, bt_string, 0, "auto");
            bail_error(err);
            err = mdb_set_node_binding
                (commit, inout_new_db, bsso_modify, 0, binding);
            bail_error(err);
        }

        /*
         * If one changed from auto to non-auto, we might want to
         * change the other to non-auto.  But we only want to do it if
         * the other one was still auto.  It might not be if (a) a
         * single request changed both simultaneously, or (b) this is
         * the second pass on the side effects, and now we see the
         * change to non-auto we made the first time through.  If it
         * was still auto in the new_db, look up what its current
         * value is and make that the next configuration.
         */
        else if (!this_auto && other_auto) {
            lc_log_basic
                (LOG_INFO, "Interface %s %s set to manual; "
                 "changing %s to manual", ifname,
                 is_duplex ? "duplex" : "speed", other_name);
            err = md_ifc_side_effect_unset_auto
                (commit, inout_new_db, ifname, is_duplex, other_name);
            bail_error(err);
        }

        /*
         * If both are manual, make sure we don't have the disallowed
         * combination of speed 10000, duplex half.
         */
        else if (!this_auto && !other_auto) {
            if (is_duplex) {
                if (!strcmp(new_value_str, "half") &&
                    ts_equal_str(other_value, "10000", false)) {
                    err = md_commit_set_return_status_msg
                        (commit, 0, _("Speed/duplex cannot be 10000/half. "
                                      "Changing speed to 1000."));
                    bail_error(err);
                    bn_binding_free(&binding);
                    err = bn_binding_new_str
                        (&binding, other_name, ba_value, bt_string, 0, "1000");
                    bail_error(err);
                    err = mdb_set_node_binding
                        (commit, inout_new_db, bsso_modify, 0, binding);
                    bail_error(err);
                }
            }
            else {
                if (!strcmp(new_value_str, "10000") &&
                    ts_equal_str(other_value, "half", false)) {
                    err = md_commit_set_return_status_msg
                        (commit, 0, _("Speed/duplex cannot be 10000/half. "
                                      "Changing duplex to full."));
                    bail_error(err);
                    bn_binding_free(&binding);
                    err = bn_binding_new_str
                        (&binding, other_name, ba_value, bt_string, 0, "full");
                    bail_error(err);
                    err = mdb_set_node_binding
                        (commit, inout_new_db, bsso_modify, 0, binding);
                    bail_error(err);
                }
            }
        }
    }

    num_ifc = tstr_array_length_quick(ifc_dh_enabled);
    for (i = 0; i < num_ifc; i++) {
        ifc_name = tstr_array_get_str_quick(ifc_dh_enabled, i);
        bail_null(ifc_name);

#ifdef PROD_FEATURE_DHCP_CLIENT
        safe_free(node_name);
        node_name =
            smprintf("/net/interface/config/%s/addr/ipv4/zeroconf",
                     ifc_name);
        bail_null(node_name);
#else
        /*
         * If the DHCP feature is disabled, make sure DHCP is not
         * enabled on any nodes.  We unconditionally honor these
         * nodes, and still show their values in the Web UI, so they
         * have to be correct.
         *
         * The shared code belw only sets a single boolean binding to
         * 'false', so that will be the one we end with.  But since
         * we need two set, we do the first one ourselves.
         */
        safe_free(node_name);
        node_name =
            smprintf("/net/interface/config/%s/addr/ipv6/dhcp/enable",
                     ifc_name);
        bail_null(node_name);
        bn_binding_free(&binding);
        err = bn_binding_new_tbool(&binding, node_name, ba_value, 0, false);
        bail_error_null(err, binding);

        err = mdb_set_node_binding(commit, inout_new_db, bsso_modify, 0,
                                   binding);
        bail_error(err);

        /* Done with DHCPv6, now disable DHCPv4 also */
        safe_free(node_name);
        node_name =
            smprintf("/net/interface/config/%s/addr/ipv4/dhcp",
                     ifc_name);
        bail_null(node_name);
#endif

        bn_binding_free(&binding);
        err = bn_binding_new_tbool(&binding, node_name, ba_value, 0, false);
        bail_error_null(err, binding);

        err = mdb_set_node_binding(commit, inout_new_db, bsso_modify, 0,
                                   binding);
        bail_error(err);

    }

    num_ifc = tstr_array_length_quick(ifc_zc_enabled);
    for (i = 0; i < num_ifc; i++) {
        ifc_name = tstr_array_get_str_quick(ifc_zc_enabled, i);
        bail_null(ifc_name);

        /*
         * If some how (like in an old db) both zeroconf and DHCPv4
         * were enabled, prefer DHCPv4 (which will have already turned
         * zeroconf off above.
         */
        err = tstr_array_linear_search_str(ifc_dh_enabled, ifc_name, 0, &idx);
        if (err == lc_err_not_found) {
            safe_free(node_name);
            node_name =
                smprintf("/net/interface/config/%s/addr/ipv4/dhcp",
                         ifc_name);
        
            bn_binding_free(&binding);
            err = bn_binding_new_tbool(&binding, node_name, ba_value, 0,
                                       false);
            bail_error_null(err, binding);

            err = mdb_set_node_binding(commit, inout_new_db, bsso_modify, 0,
                                       binding);
            bail_error(err);
        }
        bail_error(err);
    }
    
 bail:
    safe_free(node_name);
    safe_free(new_value_str);
    safe_free(ifname);
    safe_free(other_name);
    ts_free(&other_value);
    bn_binding_free(&binding);
    bn_attrib_free(&new_attrib);
    tstr_array_free(&ifc_dh_enabled);
    tstr_array_free(&ifc_zc_enabled);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
md_ifc_side_effect_get_names(const tstr_array *old_name_parts, 
                             tbool is_duplex, 
                             char **ret_ifname, char **ret_other_name)
{
    int err = 0;
    tstr_array *name_parts = NULL;
    const char *ifname = NULL;

    bail_null(old_name_parts);
    bail_null(ret_ifname);
    bail_null(ret_other_name);
    *ret_ifname = NULL;
    *ret_other_name = NULL;

    err = tstr_array_duplicate(old_name_parts, &name_parts);
    bail_error(err);

    /*
     * The name provided is one of the following:
     *   - /net/interface/config/ * /type/ethernet/duplex
     *   - /net/interface/config/ * /type/ethernet/speed
     *
     * We want to
     *   (a) Change "duplex" to "speed" or vice-versa.
     *   (b) Extract the interface name from position 3.
     */

    err = tstr_array_delete
        (name_parts, tstr_array_length_quick(name_parts) - 1);
    bail_error(err);

    err = tstr_array_append_str
        (name_parts, (is_duplex) ? "speed" : "duplex");
    bail_error(err);

    err = bn_binding_name_parts_to_name(name_parts, true, ret_other_name);
    bail_error(err);

    ifname = tstr_array_get_str_quick(name_parts, 3);
    bail_null(ifname);
    *ret_ifname = strdup(ifname);
    bail_null(*ret_ifname);

 bail:
    tstr_array_free(&name_parts);
    return(err);
}
        

/* ------------------------------------------------------------------------- */
/* If both speed and duplex were auto, and one of them is now set
 * manually, the other cannot remain at auto, so a manual setting must
 * be chosen for it.  We will choose the manual setting based on its
 * current state, i.e. whatever was chosen automatically the last time
 * autonegotiation was performed.
 */
int
md_ifc_side_effect_unset_auto(md_commit *commit, mdb_db *db,
                              const char *ifname, tbool is_duplex,
                              const char *other_name)
{
    int err = 0;
    bn_binding *binding = NULL;
    tstring *str = NULL;

    /*
     * If duplex is being set manually, get the current speed, or
     * vice-versa.  If we can't get it (e.g. maybe network cable is
     * unplugged; see bugs 1018 and 1046), arbitrartily choose a
     * setting.  If it's wrong, it won't hurt, since we probably can't
     * set the speed or duplex anyway; the important thing is to let
     * the user change what he wanted to change, and keep the
     * configuration internally consistent.
     */
    if (is_duplex) {
        err = md_interface_sd_get(ifname, false, &str, NULL);
        bail_error(err);
        if (!str) {
            err = ts_new_str(&str, "10");
            bail_error_null(err, str);
        }
    }
    else {
        err = md_interface_sd_get(ifname, false, NULL, &str);
        bail_error(err);
        if (!str) {
            err = ts_new_str(&str, "full");
            bail_error_null(err, str);
        }
    }

    err = bn_binding_new_str
        (&binding, other_name, ba_value, bt_string, 0, ts_str(str));
    bail_error(err);
    
    err = mdb_set_node_binding(commit, db, bsso_modify, 0, binding);
    bail_error(err);

 bail:
    ts_free(&str);
    bn_binding_free(&binding);
    return(err);
}


/*****************************************************************************/
/*****************************************************************************/
/*** Speed / duplex utilities                                              ***/
/*****************************************************************************/
/*****************************************************************************/

#ifdef PROD_TARGET_OS_LINUX
typedef unsigned long long u64;/* hack, so we may include kernel's ethtool.h */
typedef __uint32_t u32;         /* ditto */
typedef __uint16_t u16;         /* ditto */
typedef __uint8_t u8;           /* ditto */

#include <linux/types.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#endif

#ifndef PROD_TARGET_OS_LINUX
#define SPEED_10     10
#define SPEED_100    100
#define SPEED_1000   1000
#define SPEED_10000  10000
#define DUPLEX_HALF  0
#define DUPLEX_FULL  1
#endif

static int md_interface_sd_get_internal(const char *ifname, int *ret_speed,
                                        int *ret_duplex, tbool *ret_autoneg);
 
/*
 * Although this is a method-independent interface, the #defines from
 * ethtool.h are used to express speed (SPEED_10, SPEED_100,
 * SPEED_1000, SPEED_10000) and duplex (DUPLEX_HALF, DUPLEX_FULL)
 * for convenience.
 */
static int md_interface_sd_set_internal(md_commit *commit, const char *ifname,
                                        tbool autoneg, int speed, int duplex);

static int md_interface_sd_set_ethtool(md_commit *commit,
                                       const char *ifname, tbool autoneg,
                                       int speed, int duplex);

/* ------------------------------------------------------------------------- */
static int
md_interface_sd_get_infiniband(const char *ifname, tbool pretty,
                               tstring **ret_speed, tstring **ret_duplex)
{
    int err = 0;
    const char *filename = "/sys/class/infiniband/is4_0/ports/0/rate";
    tstring *speed = NULL;
    tstring *duplex = NULL;
    tbool ex = false;

    bail_null(ifname);

    /* 
     * NOTE: we assume there is only one Infiniband interface, and that
     *       it is called 'ib0' . 
     */
    bail_require(strcmp(ifname, "ib0") == 0);

    err = lf_test_exists(filename, &ex);
    bail_error(err);
    if (!ex) {
        goto bail;
    }

    err = lf_read_file_tstr(filename, 1024, &speed);
    bail_error(err);

    err = ts_trim_whitespace(speed);
    bail_error(err);

    /*
     * XXX/EMT: hardwired for now, per request.
     */
    err = ts_new_str(&duplex, _("full"));
    bail_error(err);

 bail:
    if (ret_speed) {
        *ret_speed = speed;
    }
    else {
        ts_free(&speed);
    }
    if (ret_duplex) {
        *ret_duplex = duplex;
    }
    else {
        ts_free(&duplex);
    }
    return(err);
}

/* Note that this function is called from other modules */
/* ------------------------------------------------------------------------- */
int
md_interface_sd_get(const char *ifname, tbool pretty,
                    tstring **ret_speed, tstring **ret_duplex)
{
    int err = 0;
    int speed = -1, duplex = -1;
    tbool autoneg = false;
    const char *str = NULL;
    char *filename = NULL;
    tstring *if_type_ts = NULL;
    int if_type = 0;

    bail_null(ifname);

    if (ret_speed) {
        *ret_speed = NULL;
    }

    if (ret_duplex) {
        *ret_duplex = NULL;
    }

/* ========================================================================= */
/* Customer-specific graft point 6: speed/duplex monitoring override.
 * e.g. could be used to get speed and duplex for non-Ethernet interfaces.
 *
 * If you want to determine the speed and/or duplex of the interface
 * specified in "ifname", create tstrings with the answers and assign
 * them to *ret_speed and/or *ret_duplex, as appropriate.  NOTE: one
 * of these may be NULL, so check the pointer for NULL before
 * dereferencing it!  If you only assign to one, the other will still
 * be populated in our standard fashion.
 * =========================================================================
 */
#ifdef INC_MD_IF_CONFIG_INC_GRAFT_POINT
#undef MD_IF_CONFIG_INC_GRAFT_POINT
#define MD_IF_CONFIG_INC_GRAFT_POINT 6
#include "../bin/mgmtd/modules/md_if_config.inc.c"
    int if_virtio = 0;
    uint32_t idx = 0;
    const char *const_if_speed = NULL;
    err = lc_is_if_driver_virtio(ifname, md_ifc_sock, &if_virtio);
    bail_error(err);
    if (if_virtio == 1) {
	/* get speed from config array */
	err = tstr_array_linear_search_str(md_ifc_vmfc_ifnames, ifname, 0, 
		&idx);
	if ((err != lc_err_not_found)) {
	    err = tstr_array_get_str(md_ifc_vmfc_ifspeed, idx, &const_if_speed);
	    if (0 == strcmp(const_if_speed, "auto")) {
		if (pretty) {
		    const_if_speed = "1000Mb/s";
		} else {
		    const_if_speed = "1000";
		}
	    }
	} else {
	    lc_log_basic(LOG_NOTICE, "ifname: %s not found in array", ifname);
	}
	if (ret_speed) {
	    *ret_speed = NULL;
	    if (pretty) {
		err = ts_new_str(ret_speed, const_if_speed?:"100Mb/s (auto)");
	    } else {
		err = ts_new_str(ret_speed, const_if_speed?:"100");
	    }
	    bail_error(err);
	}

	if (ret_duplex) {
	    err = ts_new_str(ret_duplex,"full");
	    bail_error(err);
	}
    }
#endif /* INC_MD_IF_CONFIG_INC_GRAFT_POINT */

    /*
     * If both speed and duplex are either NULL or already answered,
     * we don't have to do our standard checks.
     */
    if ((ret_speed == NULL || *ret_speed != NULL) &&
        (ret_duplex == NULL || *ret_duplex != NULL)) {
        goto bail;
    }

    /*
     * If we're dealing with an Infiniband interface, we have a
     * different approach.  Detect and handle that here.
     */
    filename = smprintf("/sys/class/net/%s/type", ifname);
    bail_null(filename);
    err = lf_read_file_tstr(filename, 1000, &if_type_ts);
    if (err) {
        lc_log_basic(LOG_INFO, "Error reading %s to determine type of "
                     "interface %s: %s", filename, ifname, strerror(errno));
        err = 0;
    }
    else if (if_type_ts) {
        if_type = atoi(ts_str(if_type_ts));
#ifdef PROD_TARGET_OS_LINUX
        if (if_type == ARPHRD_INFINIBAND) {
            err = md_interface_sd_get_infiniband(ifname, pretty,
                                                 ret_speed, ret_duplex);
            bail_error(err);
            goto bail;
        }
#endif
    }

    err = md_interface_sd_get_internal(ifname, &speed, &duplex, &autoneg);

    /*
     * If we got an error don't complain too loudly, as this may have
     * just been the loopback interface, or a down interface, or the
     * cable may have been unplugged, etc.
     */
    if (err) {
        lc_log_basic(LOG_INFO, "Unable to query speed/duplex from "
                     "interface %s", ifname);
        err = 0;
    }

    if (ret_speed && *ret_speed == NULL) {
        *ret_speed = NULL;
        str = NULL;
        switch (speed) {
        case SPEED_10:
            str = "10";
            break;
        case SPEED_100:
            str = "100";
            break;
        case SPEED_1000:
            str = "1000";
            break;
        case SPEED_10000:
            str = "10000";
            break;
        default:
            /*
             * Just let str remain NULL
             */
            break;
        }

        if (pretty && str) {
            err = ts_new_sprintf_utf8
                (ret_speed, _("%sMb/s%s"), str, (autoneg == true) ? " (auto)" : "");
            bail_error(err);
        }

        else if (str) {
            err = ts_new_str(ret_speed, str);
            bail_error(err);
        }
    }

    if (ret_duplex && *ret_duplex == NULL) {
        *ret_duplex = NULL;
        str = NULL;
        switch (duplex) {
        case DUPLEX_HALF:
            str = "half";
            break;
        case DUPLEX_FULL:
            str = "full";
            break;
        default:
            /*
             * Just let str remain NULL
             */
            break;
        }

        if (pretty && str) {
            err = ts_new_sprintf_utf8
                (ret_duplex, "%s%s", str, (autoneg == true) ? " (auto)" : "");
            bail_error(err);
        }

        else if (str) {
            err = ts_new_str(ret_duplex, str);
            bail_error(err);
        }
    }

 bail:
    safe_free(filename);
    ts_free(&if_type_ts);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Set ethernet options on an interface.  Currently the options
 * supported are speed and duplex.  When this is called both options
 * are extracted from the database provided and set together.
 */
int
md_interface_sd_set(const char *ifname, md_commit *commit,
                    const mdb_db *db)
{
    int err = 0;
    tstring *speed_ts = NULL, *duplex_ts = NULL;
    int speed = 0, duplex = 0;
    tbool autoneg = false;
    tbool is_physical = false;
    tstring *devsource = NULL;

    bail_null(ifname);

    /*
     * We don't want to try to set the speed or duplex on a non-physical
     * interface, such as the loopback.
     */
    err = md_interface_is_physical(ifname, &is_physical);
    bail_error(err);
    if (!is_physical) {
        goto bail;
    }

    /*
     * Also, see what the running type of interface is.
     */
    err = md_interface_get_devsource(ifname, &devsource);
    bail_error(err);
    if (!ts_str_maybe(devsource) ||
        ts_equal_str(devsource, md_if_devsource_unknown, false) ||
        ts_equal_str(devsource, "loopback", false) ||
        ts_equal_str(devsource, "alias", false) ||
        ts_equal_str(devsource, "virtual", false) ||
        ts_equal_str(devsource, "bridge", false) ||
        ts_equal_str(devsource, "bond", false) ||
        ts_equal_str(devsource, "vlan", false)) {
        goto bail;
    }

    err = mdb_get_node_attrib_tstr_fmt
        (commit, db, 0, ba_value, NULL, &speed_ts, 
         "/net/interface/config/%s/type/ethernet/speed", ifname);
    bail_error(err);
    bail_null(speed_ts);

    if (ts_equal_str(speed_ts, "10", false)) {
        speed = SPEED_10;
        autoneg = false;
        lc_log_basic(LOG_INFO, "Setting speed for %s to 10", ifname);
    }
    else if (ts_equal_str(speed_ts, "100", false)) {
        speed = SPEED_100;
        autoneg = false;
        lc_log_basic(LOG_INFO, "Setting speed for %s to 100", ifname);
    }
    else if (ts_equal_str(speed_ts, "1000", false)) {
        speed = SPEED_1000;
        autoneg = false;
        lc_log_basic(LOG_INFO, "Setting speed for %s to 1000", ifname);
    }
    else if (ts_equal_str(speed_ts, "10000", false)) {
        speed = SPEED_10000;
        autoneg = false;
        lc_log_basic(LOG_INFO, "Setting speed for %s to 10000", ifname);
    }
    else if (ts_equal_str(speed_ts, "auto", false)) {
        autoneg = true;
        lc_log_basic(LOG_INFO, "Setting speed for %s to auto", ifname);
    }
    else {
        bail_force(lc_err_unexpected_case);
    }

    err = mdb_get_node_attrib_tstr_fmt
        (commit, db, 0, ba_value, NULL, &duplex_ts, 
         "/net/interface/config/%s/type/ethernet/duplex", ifname);
    bail_error(err);
    bail_null(duplex_ts);

    if (ts_equal_str(duplex_ts, "half", false)) {
        duplex = DUPLEX_HALF;
        bail_require(!autoneg);
        lc_log_basic(LOG_INFO, "Setting duplex for %s to half", ifname);
    }
    else if (ts_equal_str(duplex_ts, "full", false)) {
        duplex = DUPLEX_FULL;
        bail_require(!autoneg);
        lc_log_basic(LOG_INFO, "Setting duplex for %s to full", ifname);
    }
    else if (ts_equal_str(duplex_ts, "auto", false)) {
        bail_require(autoneg);
        lc_log_basic(LOG_INFO, "Setting duplex for %s to auto", ifname);
    }
    else {
        bail_force(lc_err_unexpected_case);
    }

    if (!lc_devel_is_production()) {
        lc_log_debug(LOG_INFO, "Devenv: skipping setting ethernet options");
        goto bail;
    }

    if (autoneg) {
        err = md_interface_sd_set_internal(commit, ifname, true, -1, -1);
    }

    else {
        err = md_interface_sd_set_internal(commit, ifname, false, 
                                           speed, duplex);
    }

    if (err) {
        lc_log_basic(LOG_INFO, "Unable to apply speed/duplex changes to "
                     "interface %s", ifname);
        err = 0;
    }

 bail:
    ts_free(&speed_ts);
    ts_free(&duplex_ts);
    ts_free(&devsource);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* This function used to sometimes use mii-tool, but now always uses ethtool.
 */
static int
md_interface_sd_get_internal(const char *ifname, int *ret_speed,
                             int *ret_duplex, tbool *ret_autoneg)
{
    int err = 0;
    int speed = -1, duplex = -1;
    tbool autoneg = false;
    int speed2 = -1, duplex2 = -1;
    tbool autoneg2 = false;

    bail_null(ifname);

    if (!lc_devel_is_production()) {
        lc_log_debug(LOG_INFO, "Devenv: not running ethtool to check "
                     "speed, duplex, auto");
        goto bail;
    }

    err = md_interface_sd_get_ethtool(ifname, &speed, &duplex, &autoneg, NULL);
    bail_error(err);

 bail:
    if (ret_speed) {
        *ret_speed = speed;
    }
    if (ret_duplex) {
        *ret_duplex = duplex;
    }
    if (ret_autoneg) {
        *ret_autoneg = autoneg;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
/* This function used to sometimes use mii-tool, but now always uses ethtool.
 */
static int 
md_interface_sd_set_internal(md_commit *commit, const char *ifname, 
                             tbool autoneg, int speed, int duplex)
{
    return(md_interface_sd_set_ethtool(commit, ifname, autoneg,
                                       speed, duplex));
}


/* ========================================================================= */
/* == ETHTOOL interface                                                      */
/* ========================================================================= */

/* Note that this function is called from other modules */
/* ------------------------------------------------------------------------- */
int
md_interface_sd_get_ethtool(const char *ifname, int *ret_speed,
                            int *ret_duplex, tbool *ret_autoneg,
                            uint32 *ret_supported)
{
    int err = 0, errno_save = 0;
    tstring *devsource = NULL;

#ifdef PROD_TARGET_OS_LINUX
    /*
     * XXX/EMT: we should do something better on Solaris, like make sure 
     * we return the right values, perhaps an error code too, etc.
     */
    int speed = -1, duplex = -1;
    tbool autoneg = false;
    struct ethtool_cmd ecmd;
    struct ifreq ifr;
    tbool is_physical = false;
    uint32 supported = 0;

    bail_null(ifname);

    memset(&ifr, 0, sizeof(struct ifreq));
    memset(&ecmd, 0, sizeof(struct ethtool_cmd));
    strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ecmd.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (void *)&ecmd;

    /*
     * XXX/EMT: it is SIOCETHTOOL (0x8946) that Valgrind complains
     * about: "noted but unhandled ioctl 0x8946 with no size/direction
     * hints This could cause spurious value errors to appear.".  We
     * could add an environment variable like PROD_OPT_VALGRIND that
     * caused us to not attempt these ioctls.  Might need to hardcode
     * some fake return values for those who actually needed the
     * values we were trying to use this ioctl() to query.
     */
    err = ioctl(md_ifc_sock, SIOCETHTOOL, &ifr);
    errno_save = errno;
    if (err < 0) {
        /* Gracefully fail if the driver just does not support ethtool */
        if ((errno_save == EOPNOTSUPP) || (errno_save == EINVAL)) {

            is_physical = false;
            err = md_interface_is_physical(ifname, &is_physical);
            bail_error(err);

            err = md_interface_get_devsource(ifname, &devsource);
            bail_error(err);
            if (!ts_str_maybe(devsource) ||
                ts_equal_str(devsource, md_if_devsource_unknown, false) ||
                ts_equal_str(devsource, "loopback", false) ||
                ts_equal_str(devsource, "alias", false) ||
                ts_equal_str(devsource, "virtual", false) ||
                ts_equal_str(devsource, "bridge", false) ||
                ts_equal_str(devsource, "bond", false) ||
                ts_equal_str(devsource, "vlan", false)) {
                speed = duplex = autoneg = -1;
                err = 0;
                goto bail;
            }

            /* 
             * We will log at most once for the system that ethtool is
             * lacking on some physical interface.  The assumption is that
             * all "real" hardware would have ethtool support, so there's no
             * need to complain very loudly.  Notably, vmware interfaces do
             * not support ethtool for speed and duplex.  For virtual
             * interfaces, there's no need to complain.
             *
             * Note that libvirt's bridge interfaces (prefixed with
             * md_virt_vbridge_prefix, which is "virbr") are NOT
             * considered virtual by md_interface_is_physical(), since
             * they have some association with physical interfaces, so
             * we have to check for them separately here.
             */
            if (strcmp(ifname, "lo") && strcmp(ifname, "ib0") &&
                strcmp(ifname, "sit0") &&
                !lc_is_prefix(md_virt_vbridge_prefix, ifname, false) &&
                is_physical && !md_ifc_seen_ethtool_notsupp) {
                lc_log_basic(LOG_WARNING, "ethtool unsupported "
                             "(interface %s).  "
                             "Speed and duplex information will not be "
                             "available.", ifname);
                md_ifc_seen_ethtool_notsupp = true;
            }
            speed = duplex = autoneg = -1;
            err = 0;
            goto bail;
        }
        if (errno_save == ENODEV) {
            /* Silently ignore missing devices */
            speed = duplex = autoneg = -1;
            err = 0;
            goto bail;
        }
        lc_log_basic(LOG_INFO, "Error %d (%s) doing ioctl "
                     "SIOCETHTOOL / ETHTOOL_GSET on interface %s",
                     errno_save, strerror(errno_save), ifname);
        err = lc_err_generic_error;
        goto bail;
    }

    speed = ecmd.speed;
    duplex = ecmd.duplex;
    /* 
     * Note: for 1000Mbps speeds and faster, trying to "force" the speed
     * and duplex to an exact setting may leave autonegotiation still
     * on, but with only a single speed/duplex advertised.  Thus, even
     * though the speed/duplex has been forced, it will report as
     * autonegotiated.  This may be confusing or misleading, but there's
     * not much to do about this.
     */
    autoneg = (ecmd.autoneg == AUTONEG_ENABLE);

    supported = ecmd.supported;

 bail:
    ts_free(&devsource);
    if (ret_speed) {
        *ret_speed = speed;
    }
    if (ret_duplex) {
        *ret_duplex = duplex;
    }
    if (ret_autoneg) {
        *ret_autoneg = autoneg;
    }
    if (ret_supported) {
        *ret_supported = supported;
    }
#endif /* PROD_TARGET_OS_LINUX */
    return(err);
}


/* ------------------------------------------------------------------------- */
static int 
md_interface_sd_set_ethtool(md_commit *commit, const char *ifname, 
                            tbool autoneg, int speed, int duplex)
{
    int err = 0, errno_save = 0;
#ifdef PROD_TARGET_OS_LINUX
    struct ethtool_cmd ecmd, orig_ecmd;
    struct ifreq ifr;
    tbool unsupp_sd = false;
    tbool do_ioctl = false;
    unsigned long int this_bit = 0;
    const long int ADVERTISED_ALL_SDS = (ADVERTISED_10baseT_Half |
                                         ADVERTISED_10baseT_Full |
                                         ADVERTISED_100baseT_Half |
                                         ADVERTISED_100baseT_Full |
                                         ADVERTISED_1000baseT_Half |
                                         ADVERTISED_1000baseT_Full |
                                         ADVERTISED_10000baseT_Full);

    bail_null(ifname);

    /*
     * There is no way to selectively set certain options; you pass
     * a structure with values for all of the options.  So:
     *   (a) Get the current options
     *   (b) Change the ones we want to change
     *   (c) Set them all.
     */

    /* ........................................................................
     * (a) get current options
     */

    memset(&ifr, 0, sizeof(struct ifreq));
    strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);

    memset(&ecmd, 0, sizeof(struct ethtool_cmd));
    ecmd.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (void *)&ecmd;
    
    err = ioctl(md_ifc_sock, SIOCETHTOOL, &ifr);
    errno_save = errno;
    if (err < 0) {
        /*
         * They may have configuration for interfaces that aren't
         * installed, and we don't want to scream too loudly about
         * that.
         */

        /* 
         * XXXX/EMT only report if the caller has already checked that 
         * we should support ethtool, since some interfaces don't, and
         * we don't want to complain about that.
         */

        lc_log_debug(LOG_DEBUG, "Error %d (%s) doing ioctl "
                     "SIOCETHTOOL / ETHTOOL_GSET on interface %s",
                     errno_save, strerror(errno_save), ifname);
        err = lc_err_generic_error;
        goto bail;
    }
    err = 0;

    /* ........................................................................
     * (b) change options
     */
    orig_ecmd = ecmd;

    /* 
     * What advertised / supported bit pattern is this speed / duplex?
     * Note that if either speed or duplex is -1 (auto) then this_bit
     * will be 0.
     */
    this_bit = 0;
    switch (speed + duplex ) {
    case SPEED_10 + DUPLEX_HALF:
        this_bit = ADVERTISED_10baseT_Half;
        break;
    case SPEED_10 + DUPLEX_FULL:
        this_bit = ADVERTISED_10baseT_Full;
        break;
    case SPEED_100 + DUPLEX_HALF:
        this_bit = ADVERTISED_100baseT_Half;
        break;
    case SPEED_100 + DUPLEX_FULL:
        this_bit = ADVERTISED_100baseT_Full;
        break;
    case SPEED_1000 + DUPLEX_HALF:
        this_bit = ADVERTISED_1000baseT_Half;
        break;
    case SPEED_1000 + DUPLEX_FULL:
        this_bit = ADVERTISED_1000baseT_Full;
        break;
    case SPEED_10000 + DUPLEX_FULL:
        this_bit = ADVERTISED_10000baseT_Full;
        break;
    default:
        break;
    }

    /* We'll just note this, as the ioctl() below will fail anyway */
    if (this_bit && ((ecmd.supported & this_bit) != this_bit)) {
        unsupp_sd = true;
        lc_log_basic(LOG_INFO, 
                     "Interface %s: unsupported speed/duplex: %d/%s",
                     ifname, speed, duplex ? "full" : "half");
    }

    if (autoneg) {
        ecmd.autoneg = AUTONEG_ENABLE;

        /* 
         * Advertise as before, but with exactly all the supported
         * speeds.  This assumes that the SUPPORTED and ADVERTISED
         * #defines mirror each other, but many other places depend on
         * this too.
         */
        ecmd.advertising = ((ecmd.advertising & ~ADVERTISED_ALL_SDS) |
                            (ecmd.supported & ADVERTISED_ALL_SDS));
    }
    else {
        ecmd.speed = speed;
        ecmd.duplex = duplex;
        ecmd.autoneg = AUTONEG_DISABLE;
    }

    /* ........................................................................
     * (c) set options
     */

    /* 
     * Only do the set if at least one of the speed, duplex, autoneg or
     * advertising are different.  We do this because otherwise the
     * ethtool ioctl can cause the card to unnecessarily toggle the link.
     */
    do_ioctl = true;
    if (orig_ecmd.advertising == ecmd.advertising &&
        orig_ecmd.speed == ecmd.speed &&
        orig_ecmd.duplex == ecmd.duplex) {

        /* 
         * If our autoneg is as before; and either autoneg is off, or is
         * on and is advertising at least our speed/duplex; then it's
         * always okay to skip the ioctl().  Currently this 'this_bit'
         * test is always not needed, as we do not allow explicitly
         * configured auto neg on with a forced speed and duplex.
         */
        if (orig_ecmd.autoneg == ecmd.autoneg &&
            (orig_ecmd.autoneg == AUTONEG_DISABLE ||
             ((ecmd.advertising & this_bit) == this_bit))) {
            do_ioctl = false;
        }
        
        /* 
         * At 1G / 10G, it is not necessarily supported to have
         * autonegotiation off.  The e1000 driver, for example, will let
         * you tell it to turn autoneg off, but it will just set
         * advertised flags to just the speed you tell it, but then
         * leave autonegotiation on.
         */
        else if ((ecmd.speed == 1000 || ecmd.speed == 10000) &&
                 (ecmd.autoneg == AUTONEG_DISABLE && 
                  orig_ecmd.autoneg == AUTONEG_ENABLE) &&
                 (((ecmd.advertising & ~ADVERTISED_ALL_SDS) | this_bit)
                  == orig_ecmd.advertising)) {
            do_ioctl = false;
        }
    }
    if (do_ioctl) {
        ecmd.cmd = ETHTOOL_SSET;
        ifr.ifr_data = (void *)&ecmd;
        
        err = ioctl(md_ifc_sock, SIOCETHTOOL, &ifr);
        errno_save = errno;
        if (err < 0) {
            if (unsupp_sd) {
                /*
                 * We thought it would fail, and it did.  So let's
                 * tell the user.  But we're not failing the commit,
                 * so don't use a nonzero error code.  Even if we used
                 * a nonzero code, it wouldn't fail the commit (from
                 * the apply phase, which we're presumably in), but
                 * the '%' sign might make the user THINK it failed...
                 */
                err = md_commit_set_return_status_msg_fmt
                    (commit, 0, _("Interface %s: unsupported speed/duplex: "
                                  "%d/%s"), ifname, speed, duplex ?
                     "full" : "half");
                complain_error(err);
            }

            lc_log_debug(LOG_DEBUG, "Error %d (%s) doing ioctl SIOCETHTOOL / "
                         "ETHTOOL_SSET on interface %s", errno_save,
                         strerror(errno_save), ifname);
            err = lc_err_generic_error;
            goto bail;
        }
        err = 0;
    }

 bail:
#endif /* PROD_TARGET_OS_LINUX */
    return(err);
}

/* ------------------------------------------------------------------------- */
int
md_interface_is_physical(const char *ifname, tbool *ret_is_physical)
{
    int err = 0;
    uint32 i = 0, slen = 0;
    const char **ifnames = NULL;
    char *tifname = NULL;
    tbool is_physical = false;

    bail_null(ifname);
    bail_null(ret_is_physical);

    /*
     * The interface name prefix md_virt_ifname_prefix ("vif") is
     * considered virtual regardless of what is in customer.h.
     */
    if (lc_is_prefix(md_virt_ifname_prefix, ifname, false)) {
        is_physical = false;
        goto bail;
    }

    /*
     * XXXX Maybe we could look in /sys/class/net/<ifname>/ at things
     * like device , and have some heuristics?
     */

#ifdef md_if_use_physical_interface_list
    is_physical = false;
    ifnames = physical_interfaces;
#else
    is_physical = true;
    ifnames = virtual_interfaces;
#endif

    for (i = 0; ifnames[i]; ++i) {
        safe_free(tifname);
        tifname = safe_strdup(ifnames[i]);
        bail_null(tifname);
        slen = strlen(tifname);
        if (tifname[slen - 1] == '*') {
            tifname[slen - 1] = '\0';
            if (lc_is_prefix(tifname, ifname, false)) {
                is_physical = !is_physical;
                break;
            }
        }
        else {
            if (!strcmp(tifname, ifname)) {
                is_physical = !is_physical;
                break;
            }
        }
    }

 bail:
    if (ret_is_physical) {
        *ret_is_physical = is_physical;
    }
    safe_free(tifname);
    return(err);
}
