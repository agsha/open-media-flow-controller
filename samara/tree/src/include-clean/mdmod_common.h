/*
 *
 * mdmod_common.h
 *
 *
 *
 */

#ifndef __MDMOD_COMMON_H_
#define __MDMOD_COMMON_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "mdb_db.h"
#include "md_mod_reg.h"
#include "libevent_wrapper.h"

/* ========================================================================= */
/* Commit order constants for Samara base modules.
 *
 * These are listed centrally here to make it easier to compare them
 * between modules.  These should be kept in sorted order.  Anything
 * not listed here has a commit order of zero.
 */

/*
 * This is negative because we want our side effects function to run
 * before most modules.  This is a sort of workaround for bug 11668:
 * we want to update nodes reflecting license status before other side
 * effects functions in case they want to make updates of their own
 * based on this before check functions run.
 */
static const int32 mdm_license_commit_order =       -1000;

/*
 * In particular, we want our upgrade to run before md_passwd's.
 * When upgrading from PROD_FEATURE_CAPABS to PROD_FEATURE_ACLS,
 * md_passwd is going to map all the nonzero gids, but before it
 * does that, we want to refer to them when deciding which roles
 * to grant each account.
 */
static const int32 mdm_acl_commit_order =            -100;

/*
 * All modules not listed here should have a commit order of zero.
 */

/*
 * The commit_order of 10 was chosen to be higher than md_pm's commit
 * order, since our 9-to-10 and 12-to-13 upgrade rules must run after
 * md_pm's so we can override some of its default settings.
 */
static const int32 md_fp_commit_order =                10;

/*
 * The commit order of 10 was chosen to be higher than md_system's
 * commit order of 0, because our initial values function depends on
 * the hostname already having been computed.
 */
static const int32 md_static_hosts_commit_order =      10;


/* ========================================================================= */
/* Apply order constants for Samara base modules.
 *
 * These are listed centrally here to make it easier to compare them
 * between modules.  These should be kept in sorted order.  Anything
 * not listed here has an apply order of zero.
 */

/*
 * Have this before md_if_config, to prevent a window of insecurity 
 * after interfaces are brought up.  Iptables doesn't seem to mind if
 * our rules reference interfaces that don't exist yet.
 */
static const int32 md_iptables_apply_order =     -21500;

/*
 * The md_cert module is expected apply changes prior to md_web, md_ldap,
 * md_crypto, md_email_config, and any other modules that use configured
 * certificates.
 */
static const int32 md_cert_apply_order =         -21450;
static const int32 md_crypto_apply_order =       -21400;

static const int32 md_bonding_apply_order =      -21300;
static const int32 md_vlan_apply_order =         -21200;
static const int32 md_bridge_apply_order =       -21100;

/*
 * It is expected that most mgmtd modules want their apply callbacks
 * run after interface changes are applied (with the exceptions listed
 * above).  In particular, some of the daemons at -20000 (httpd,
 * snmpd, and sshd) for which we offer "listen" restrictions need to
 * run after this (see bug 13897).
 *
 * md_net_general is one that should run after us.  For one thing, it
 * can recompute the acting primary DHCP interface, and that
 * calculation may depend on things the interface module has done.
 */
static const int32 md_if_config_apply_order =    -21000;

static const int32 md_arp_apply_order =          -20900;
static const int32 md_neighbors_apply_order =    -20800;
static const int32 md_net_general_apply_order =  -20700;
static const int32 md_routes_apply_order =       -20600;

/*
 * The entries at -20000 are mainly for modules which represent
 * daemons for which we write config files, and which are managed by
 * PM.  We want to run before the dbchange event goes out (at
 * mdm_changes_apply_order), particularly for the case when the
 * dbchange event changes the PM auto_launch flag for the process we
 * represent.  If this is changed from false to true, it will trigger
 * PM to launch the process -- and by that time, we want to have
 * already written the config file.  See bug 12994.
 *
 * Similar logic applies for md_cli, even though the CLI is not a 
 * daemon: it writes the 'clirc' file, which is reread by the CLI
 * when a relevant binding changes.
 */
static const int32 md_cli_apply_order =          -20000;
static const int32 md_ntp_apply_order =          -20000;
static const int32 md_snmp_apply_order =         -20000;
static const int32 md_ssh_apply_order =          -20000;
static const int32 md_web_apply_order =          -20000;
static const int32 md_xinetd_apply_order =       -20000;

/*
 * Ensure that config change events are sent out before most other
 * apply functions run.  One example scenario where this might matter
 * is if a side effects function (or the main set request) changes
 * PM's configuration for a process, such as its launch parameters;
 * and then an apply function asks PM to restart the process.  If the
 * config change event has not already been sent out by the time the
 * apply function runs, PM would not know about the change, and would
 * still launch the process with its old parameters.
 *
 * XXXX/EMT: see bug 12994 for a reason why this is problematic...
 */
static const int32 mdm_changes_apply_order =     -10000;

/*
 * All modules not listed here should have an apply order of zero.
 */


/* ========================================================================= */
/* Miscellaneous constants shared between multiple modules.
 */

/*
 * List of acceptable ciphers, suitable for passing to
 * SSL_CTX_set_cipher_list().
 */
#define md_ssl_cipher_list "TLSv1:SSLv3:!aNULL:!ADH:!eNULL:!LOW:!EXP"


/* ========================================================================= */
/* The functions below are ones which are defined in one module,
 * but need to be called from another.  The typedefs are provided for
 * the caller, to cast the function pointers given back from the
 * system function to get a foreign symbol.  The prototypes are
 * provided for the implementation, kept here to ensure they stay in
 * sync with the typedefs.
 */

/* ------------------------------------------------------------------------- */
/* From system module
 */
typedef int (*md_system_override_clear_ext_type)(md_commit *commit, 
                                                 const mdb_db *db,
                                                 const char *ifname);

int md_system_override_clear_ext(md_commit *commit, const mdb_db *db,
                                 const char *ifname);

/* ------------------------------------------------------------------------- */
/* From resolver module
 */

/*
 * Special tag names for network config overlay info installed using
 * actions in md_resolver or md_if_config, used by DHCP to distinguish
 * its entries from those from other sources, with special semantics.
 */
#define MD_NET_OVERLAY_TAG_DHCPV4 "DHCP"
#define MD_NET_OVERLAY_TAG_DHCPV6 "DHCPv6"
extern const char md_net_overlay_tag_dhcpv4[];
extern const char md_net_overlay_tag_dhcpv6[];

typedef int (*md_resolver_overlay_clear_ext_type)(md_commit *commit, 
                                                  const mdb_db *db,
                                                  const char *tag,
                                                  const char *ifname);

int md_resolver_overlay_clear_ext(md_commit *commit, const mdb_db *db,
                                  const char *tag, const char *ifname);

/* ------------------------------------------------------------------------- */
/* From routes module
 */
typedef int (*md_routes_reassert_state_type)(md_commit *commit, 
                                             const mdb_db *db, 
                                             const char *reason);

int md_routes_reassert_state(md_commit *commit, const mdb_db *db,
                             const char *reason);

typedef int (*md_routes_overlay_clear_type)(md_commit *commit,
                                            const mdb_db *db,
                                            const char *ifname);

int md_routes_overlay_clear(md_commit *commit, const mdb_db *db,
                            const char *ifname);

typedef struct md_routes_delete_criteria {
    const char *mrdc_ifname;
    int64 mrdc_table;
    int mrdc_protocol;
    uint32 mrdc_rflags_set;
    uint32 mrdc_rflags_clear;
} md_routes_delete_criteria;

typedef int (*md_routes_delete_routes_type)(md_commit *commit,
                                            const mdb_db *db,
                                            const char *reason,
                                            const md_routes_delete_criteria 
                                            *criteria);

/*
 * Delete all routes matching the criteria specified.  No other
 * criteria are applied implicitly.  e.g. if you only want to delete
 * routes created from mgmtd, specify mrdc_protocol as RTPROT_STATIC.
 */
int md_routes_delete_routes(md_commit *commit, const mdb_db *db,
                            const char *reason, 
                            const md_routes_delete_criteria *criteria);


/* ------------------------------------------------------------------------- */
/* From interface config module
 */

typedef int (*md_interface_sd_get_type)(const char *ifname, tbool pretty,
                                        tstring **ret_speed,
                                        tstring **ret_duplex);

int md_interface_sd_get(const char *ifname, tbool pretty,
                        tstring **ret_speed, tstring **ret_duplex);

typedef int (*md_interface_sd_set_type)(const char *ifname, md_commit *commit,
                                        const mdb_db *db);

int md_interface_sd_set(const char *ifname, md_commit *commit,
                        const mdb_db *db);

typedef int (*md_interface_is_physical_type)(const char *ifname,
                                             tbool *ret_is_physical);

int md_interface_is_physical(const char *ifname, tbool *ret_is_physical);

typedef int (*md_interface_reapply_config_ext_type)(const char *ifname,
                                                    md_commit *commit,
                                                    const mdb_db *db,
                                                    tbool renew);

int md_interface_reapply_config(const char *ifname, md_commit *commit,
                                const mdb_db *db, tbool renew);

typedef int (*md_interface_renew_dhcp_interface_ext_type) (md_commit *commit,
                                                           const mdb_db *db,
                                                           tbool ipv6,
                                                           const char *ifname);

int md_interface_renew_dhcp_interface_ext(md_commit *commit, const mdb_db *db,
                                          tbool ipv6, const char *ifname);
                                  /* ifname == NULL renews all interfaces */

typedef int (*md_interface_bring_down_ext_type)(md_commit *commit,
                                                const mdb_db *db,
                                                const char *ifname,
                                                tbool skip_down,
                                                tbool is_alias);

int md_interface_bring_down(md_commit *commit, const mdb_db *db,
                            const char *ifname,
                            tbool skip_down, tbool is_alias);

typedef int (*md_interface_bring_up_ext_type)(md_commit *commit,
                                              const mdb_db *db, 
                                              const char *ifname);

int md_interface_bring_up(md_commit *commit, const mdb_db *db, 
                          const char *ifname);

typedef int (*md_interface_override_is_set_type)(const char *ifname,
                                                 tbool *ret_override);

int md_interface_override_is_set(const char *ifname, tbool *ret_override);

typedef int (*md_interface_sd_get_ethtool_type)(const char *ifname,
                                                int *ret_speed,
                                                int *ret_duplex,
                                                tbool *ret_autoneg,
                                                uint32 *ret_supported);

int md_interface_sd_get_ethtool(const char *ifname, int *ret_speed,
                                int *ret_duplex, tbool *ret_autoneg,
                                uint32 *ret_supported);

/*
 * Keep track of the pids of DHCP clients launched for various
 * interfaces.  We launch a separate dhclient per interface; and
 * within each interface, separate ones for DHCPv4 and DHCPv6.
 * The midr_ifname and midr_ipv6 fields are the keys to look up a
 * record in the array; the remainder of the fields are what we track
 * for each interface+family.
 */
typedef struct md_ifc_dhcp_rec {
    char *midr_ifname;
    tbool midr_ipv6;

    /*
     * PID of the main DHCP client we have launched.  This is expected
     * to stay up as long as we tell it to.  This will be 0 if no
     * dhclient is running.
     */
    pid_t midr_dhclient_pid;

    /*
     * PID of the DHCP client we have launched for purposes of
     * releasing a lease.  This is expected to exit as soon as it
     * gets a response back from the server confirming the release.
     * This will be 0 if no DHCP release process is running.
     */
    pid_t midr_release_pid;

    /*
     * If we have a DHCP release process running (midr_release_pid > 0),
     * at what system uptime (from lt_sys_uptime_ms()) did we launch it?
     */
    lt_dur_ms midr_release_launch_uptime;

    /*
     * Timer event used to time out on a DHCP 'release' process that
     * has taken too long.
     *
     * We have no hook to cancel/free these from md_event_cleanup(),
     * but that's OK since it uses lew_escape_from_dispatch() anyway.
     * We will clean them up from our deinit function, though.
     */
    lew_event *midr_release_timeout;

} md_ifc_dhcp_rec;

typedef int (*md_ifc_dhcp_rec_lookup_type)(const char *ifname, tbool ipv6,
                                           tbool create_if_missing,
                                           md_ifc_dhcp_rec **ret_rec);

int md_ifc_dhcp_rec_lookup(const char *ifname, tbool ipv6,
                           tbool create_if_missing,
                           md_ifc_dhcp_rec **ret_rec);

typedef int (*md_interface_mtu_set_type)(const char *ifname,
                                         tbool only_if_changed,
                                         md_commit *commit, const mdb_db *db);

int md_interface_mtu_set(const char *ifname, tbool only_if_changed,
                         md_commit *commit, const mdb_db *db);


/* ------------------------------------------------------------------------- */
/* From vlan module
 */
#ifdef PROD_FEATURE_VLAN

/* Reapply to kernel all the VLANs on top of the named interface */
typedef int (*md_vlan_reapply_config_ext_type)(md_commit *commit, 
                                               const mdb_db *db,
                                               const char *ifname);

int md_vlan_reapply_config(md_commit *commit, const mdb_db *db,
                           const char *ifname);

/* 
 * Remove from kernel all the VLANs which are on top of the named
 * interface, or currently have the mac address specified.  Only one may
 * be specified at a time.
 */
typedef int (*md_vlan_remove_vlans_ext_type)(md_commit *commit, 
                                             const mdb_db *db,
                                             const char *base_ifname,
                                             const char *base_macaddr);

int md_vlan_remove_vlans(md_commit *commit, const mdb_db *db,
                         const char *base_ifname,
                         const char *base_macaddr);
#endif

/* ------------------------------------------------------------------------- */
/* From stats module
 */
typedef enum {
    mse_none =                0,
    mse_rising_error =   1 << 0,
    mse_rising_clear =   1 << 1,
    mse_rising_all =     mse_rising_error | mse_rising_clear,
    mse_falling_error =  1 << 2,
    mse_falling_clear =  1 << 3,
    mse_falling_all =    mse_falling_error | mse_falling_clear
} md_stats_trigger_type;

typedef int (*md_stats_register_event_type)
     (md_module *module, const char *event_name_part,
      md_stats_trigger_type trigger_type);

int md_stats_register_event(md_module *module,
                            const char *event_name_part,
                            md_stats_trigger_type trigger_type);

/* ------------------------------------------------------------------------- */
/* From syslog module
 */
#define md_syslog_log_priorities ",none,debug,info,notice,warning,err,"  \
                                 "crit,alert,emerg"

#define md_syslog_log_priorities_inv ",none,debug,info,notice,warning,err,"  \
                                 "crit,alert,emerg,-"

/*
 * Note: it's an unholy alliance between mgmt module and CLI to not mention
 * the 'none' option here.  The CLI does not offer 'none' as a value to
 * enter explicitly; instead, this is the value used when they use the
 * "no" command to disable a particular type of logging; hence, don't
 * mention it here.
 *
 * XXX/EMT: I18N: not sure how to tell the translator not to translate
 * the strings 'debug', etc.  These are keywords that should not
 * change.  Cannot construct the string with printf, since the format
 * string part would need to be looked up by the infrastructure are
 * runtime.  The best way would be to have a check function, which
 * we'd have to arrange some way for multiple modules to share.
 */
#define md_syslog_prior_reqts                                                 \
N_("Log severity level must be 'debug', 'info', 'notice', 'warning', "        \
   "'err',\n'crit', 'alert', or 'emerg'.")

/* ------------------------------------------------------------------------- */
/* From CLI module
 */

#define CLI_RESTRICTED_CMDS_LICENSE_CONFIG "/cli/licenses/restricted_cmds"
#define CLI_SHELL_ACCESS_LICENSE "/cli/licenses/shell_access"
#define CLI_RESET_PASSWORD_LICENSE "/cli/licenses/reset_password"
extern const char cli_restricted_cmds_license_config[];
extern const char cli_shell_access_license[];
extern const char cli_reset_password_license[];


/* ------------------------------------------------------------------------- */
/* From passwd module
 */

/*
 * XXX/SML:  Note that GCL has a maximum length of 31 characters (plus
 * null terminator) for usernames, as currently defined via "magic number"
 * in gcl_internal.h.  We would like to keep this limit defined independently
 * of GCL limits, since GCL limits are required for continued protocol
 * compatibility.  However, since we do not wish to see usernames tacitly
 * truncated in GCL communication, we now impose the same maximum length in the
 * user database.  See bp_admin_msg_greeting* structs in gcl_internal.h.
 * See also bug 13384.
 */
#define MD_USERNAME_LENGTH_MAX 31
#define MD_USERNAME_STR_SIZE (MD_USERNAME_LENGTH_MAX + 1)

static const uint32 md_username_length_max = MD_USERNAME_LENGTH_MAX;


#ifdef __cplusplus
}
#endif

#endif /* __MDMOD_COMMON_H_ */
