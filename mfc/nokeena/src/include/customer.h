/* customer.h */

#ifndef __CUSTOMER_H_
#define __CUSTOMER_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/* This file contains customer-specific definitions for use in .c and .h
 * files.  The definitions are for use in a variety of contexts.  They
 * were consolidated into one file for simplicity.
 * -------------------------------------------------------------------------
 */

/*
 * We must not include anything from the base platform if the including
 * component's license requires redistribution of source modifications,
 * as does the GPL.
 */
#ifndef CUSTOMER_H_REDIST_ONLY

#include "common.h"
#include "license_validator.h"

#endif /* CUSTOMER_H_REDIST_ONLY */


#if defined(PROD_FEATURE_I18N_SUPPORT)
    /* Must match GETTEXT_PACKAGE as defined in the include Makefile */
/* Juniper changed the following. */
    #define CUSTOMER_INCLUDE_GETTEXT_DOMAIN "mfd_incl"
/* Juniper changed the preceeding. */
#endif

/*
 * The next block of this file is not needed by components requiring
 * redistribution of source code.
 */
#ifndef CUSTOMER_H_REDIST_ONLY

/******************************************************************************
 ******************************************************************************
 *** Logging
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * If you have any customer-specific logging clients, define their
 * logging component IDs here.  Your customer IDs should start at
 * 257 (0x101) and go upwards by ones.  You also need to provide an
 * lc_enum_string_map array to map any component IDs to component
 * names.  You must provide an (empty) array even if you have no extra
 * components, since the logging subsystem expects to find an array
 * with the name "lc_customer_component_id_map".  Make sure to terminate
 * the mapping array with a {0, NULL} element.  For example:
 *
 * enum {
 *     LCI_MY_COMPONENT = 257
 * };
 *
 * static const lc_enum_string_map lc_customer_component_id_map[] = {
 *     { LCI_MY_COMPONENT, "my_component" },
 *     { 0, NULL }
 * };
 */
/* Juniper added the following. */
 enum {
     LCI_NVSD = 257,
     LCI_NKNACCESSLOG,
     LCI_NKNERRORLOG,
     LCI_GMGMTHD,
     LCI_WATCHDOG,
     LCI_NVSD_DISK,
     LCI_MFP,
     LCI_GEODBD,
     LCI_PROXYD,
     LCI_SSLD,
     LCI_NKNLOGD,
     LCI_AGENTD,
     LCI_CSED, 
     LCI_DNSD,
     LCI_CRSTORED
 };
/* Juniper added the preceeding. */
 
static const lc_enum_string_map lc_customer_component_id_map[] = {
/* Juniper added the following. */
     { LCI_NVSD, "mfd" },
     { LCI_NKNACCESSLOG, "nknaccesslog" },
     { LCI_NKNERRORLOG, "nknerrorlog" },
     { LCI_GMGMTHD, "gmgmthd" },
     { LCI_WATCHDOG, "watchdog" },
     { LCI_NVSD_DISK, "mfd-disk" },
     { LCI_MFP, "mfp" },
     { LCI_GEODBD, "geodbd" },
     { LCI_PROXYD, "proxyd" },
     { LCI_SSLD, "ssld" },
     { LCI_NKNLOGD, "mfd-logger"},
     { LCI_AGENTD, "agentd"},
     { LCI_CSED, "csed" },
     { LCI_DNSD, "cr-dns"},
     { LCI_CRSTORED, "cr-store"},
/* Juniper added the preceeding. */
    { 0, NULL }
};

/*
 * The Samara components by default log into three facilities: 
 * LOCAL5 for mgmtd (called the "System Management Core";
 * LOCAL6 for other back-end components (called "System Management
 * back end components"); and LOCAL7 for front end components and
 * all utilities and tests (called "System Management front end 
 * components").
 *
 * If you would rather that mgmtd share the LOCAL6 facility with
 * the back-end components, uncomment this line.
 */
/* #define lc_log_mgmtd_share_facility */


/*
 * The following constant controls mgmtd's behavior in the case where the
 * initial commit fails.  The three possible cases are:
 *   1. Constant is defined...
 *      (a) to a logging level higher (i.e. more detailed) than the
 *          currently configured logging level for mgmtd.
 *      (b) to a logging level equal to or lower (i.e. less detailed)
 *          than the currently configured logging level for mgmtd.
 *   2. Constant is not defined.
 *
 * In case #1, we tweak syslogd's configuration for improved debug
 * logging, and retry the initial commit.  We at least change it to send
 * log messages at ERR or higher (though we never use CRIT, ALERT, or
 * EMERG, so mainly this just means ERR) to the console.  Additionally,
 * in case #1a, we raise mgmtd's logging level to the level defined.
 * e.g. if the constant is defined to LOG_INFO, and the effective logging
 * level for mgmtd's facility is LOG_NOTICE, then we'd rewrite it to be
 * LOG_INFO, hoping that the additional messages thus logged will help us
 * troubleshoot the initial commit failure.
 *
 * In case #2, we do nothing.  The initial commit is not rerun.
 */
/* #define md_initial_commit_retry_loglevel LOG_INFO */


/******************************************************************************
 ******************************************************************************
 *** GCL
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * If you have any customer-specific GCL clients, define their GCL
 * client IDs here.  For example:
 *
 * #define GCL_CLIENT_CLI "cli"
 * static const char *gcl_client_cli __attribute__((unused)) =
 *                    GCL_CLIENT_CLI;
 */

/* Juniper added the following. */
#define GCL_CLIENT_NVSD "nvsd"
static const char *gcl_client_nvsd __attribute__((unused)) =
                  GCL_CLIENT_NVSD;

#define GCL_CLIENT_CRAWLERD "csed"
static const char *gcl_client_csed __attribute__((unused)) =
                  GCL_CLIENT_CRAWLERD;

#define GCL_CLIENT_NKNACCESSLOG "nknaccesslog"
static const char *gcl_client_nknaccesslog __attribute__((unused)) =
                  GCL_CLIENT_NKNACCESSLOG;
#define GCL_CLIENT_NKNERRORLOG "nknerrorlog"
static const char *gcl_client_nknerrorlog __attribute__((unused)) =
                  GCL_CLIENT_NKNERRORLOG;

#define GCL_CLIENT_WATCHDOG "watchdog"
static const char *gcl_client_watchdog __attribute__((unused)) =
                  GCL_CLIENT_WATCHDOG;

#define GCL_CLIENT_GMGMTHD "gmgmthd"
static const char *gcl_client_gmgmthd __attribute__((unused)) =
                  GCL_CLIENT_GMGMTHD;
#define GCL_CLIENT_GMGMTHD_TIMER "gmgmthd_timer"
static const char *gcl_client_gmgmthd_timer __attribute__((unused)) =
                  GCL_CLIENT_GMGMTHD_TIMER;
#define GCL_CLIENT_DISK "disk"
static const char *gcl_client_disk __attribute__((unused)) =
                  GCL_CLIENT_DISK;
#define GCL_CLIENT_PROXYD "proxyd"
static const char *gcl_client_proxyd __attribute__((unused)) =
                  GCL_CLIENT_PROXYD;
#define GCL_CLIENT_MFP "mfp"
static const char *gcl_client_mfp __attribute__((unused)) =
                  GCL_CLIENT_MFP;
#define GCL_CLIENT_ADNSD "adnsd"
static const char *gcl_client_adnsd __attribute__((unused)) =
                  GCL_CLIENT_ADNSD;
#define GCL_CLIENT_SSLD "ssld"
static const char *gcl_client_ssld __attribute__((unused)) =
                  GCL_CLIENT_SSLD;
#define GCL_CLIENT_GEODBD "geodbd"
static const char *gcl_client_geodbd __attribute__((unused)) =
                  GCL_CLIENT_GEODBD;
#define GCL_CLIENT_AGENTD "agentd"
static const char *gcl_client_agentd __attribute__((unused)) =
                  GCL_CLIENT_AGENTD;
#define GCL_CLIENT_CRSTORED "cr_stored"
static const char *gcl_client_crstored __attribute__((unused)) = 
                  GCL_CLIENT_CRSTORED;
#define GCL_CLIENT_CRDNSD "cr_dnsd"
static const char *gcl_client_crdnsd __attribute__((unused)) = 
                  GCL_CLIENT_CRDNSD;
/* Juniper added the preceeding. */

/*
 * Controls the syslog log severity level used by the GCL to log 
 * messages that it sends and receives.  Set to -1 for no logging.
 * If these are not set, sent messages use LOG_DEBUG, and received
 * messages use -1 (not logged).
 *
 * CAUTION: GCL messages can contain sensitive information that the
 * system takes steps to conceal in other contexts.
 * PROD_FEATURE_RESTRICT_CMDS prevents the user from setting the log level
 * to DEBUG, so that is the only safe log level for this information.
 */
#define bn_msg_dump_send_log_level_def   LOG_DEBUG
#define bn_msg_dump_recv_log_level_def   -1

/*
 * Controls the maximum number of bindings from a GCL message that will
 * be logged.  Set to -1 for no limit.  If not set, 50 is used.
 */
#define bn_msg_dump_max_nodes_def        50

/*
 * Controls the level level used for bindings contained in event and
 * action requests processed by mgmtd.
 *
 * CAUTION: event and action requests can contain sensitive
 * information that the system takes steps to conceal in other
 * contexts.  PROD_FEATURE_RESTRICT_CMDS prevents the user from
 * setting the log level to DEBUG, so that is the only safe log
 * level for this information.
 */
/* #define md_event_action_binding_log_level_def LOG_DEBUG */

/* 
 * WARNING: Only enable GCL_PROVIDER_INET or MD_PROVIDER_INET for
 * testing, or in a very carefully constrained environment.  The first
 * allows a GCL provider socket to be AF_INET or AF_INET6, instead of
 * just AF_UNIX.  The second causes mgmtd to listen on the IPv4 or IPv6
 * addresses specified below for incoming AF_INET or AF_INET6
 * connections.
 *
 * There is no security of any kind for these INET connections.
 */

/* #define GCL_PROVIDER_INET 1 */

/* 
 * If GCL_PROVIDER_INET is set, each of the below can be set to one of:
 *
 *  An IP address:    "169.254.7.1" (IPv4) or "fd01:a9fe::7:1" (IPv6)
 *  Disabled:         "-"
 *  Any IP interface: "*"
 *  Loopback:         "L" (127.0.0.1 or ::1)
 *
 */
#define GCL_PROVIDER_INET_MGMTD_IPV4ADDR   "-"
#define GCL_PROVIDER_INET_MGMTD_IPV6ADDR   "-"


/******************************************************************************
 ******************************************************************************
 *** CLI 
 ******************************************************************************
 ******************************************************************************
 ***/

/* ----------------------------------------------------------------------------
 * CLI reverse mapping
 */

/*
 * If you define any customer-specific CLI commands that need to show
 * up in their own section of the "show configuration" output, define
 * their ordering constants here.  Note that all commands with the
 * same ordering constant will be grouped together, and the groups
 * will appear in ascending order of the ordering constant.  See the
 * section of climod_common.h headed "Reverse-mapping ordering
 * constants" for a list of constants in use by the generic CLI
 * commands.
 *
 * You also must provide an array of lc_enum_string_map structures,
 * mapping each ordering constants to the string that should be
 * provided as a heading for the section.  Include an empty array even
 * if you have no constants.
 */

/* Juniper added the following. */
enum {
    cro_delivery = 2000,
    cro_config_files,
    cro_virtual_player,
    cro_pub_point,
    cro_servermap,
    cro_clustermap,
    cro_accesslog,
    cro_device_map,
    cro_log_analyzer,
    cro_url_filtermap,
    cro_namespace,
    cro_media_cache,
    cro_analytics,
    cro_errorlog,
    cro_streamlog,
    cro_tracelog,
    cro_cachelog,
    cro_fuselog,
    cro_fmsaccesslog,
    cro_fmsedgelog,
    cro_publishlog,
    cro_resource_pool,
    cro_policy,
    cro_scheduler,
    cro_crawler,
    cro_ssl,
    cro_bgp, 
    cro_systune,
    cro_watchdog,
    cro_junos_re,
};
/* Juniper added the preceeding. */

static const lc_signed_enum_string_map
cli_customer_revmap_order_value_map[] = {
/* Juniper added the following. */
    {cro_delivery, N_("Delivery and Network configuration")},
    {cro_config_files, N_("Internal Config Files configuration")},
    {cro_virtual_player, N_("Virtual Player Configuration")},
    {cro_pub_point, N_("Publish Point Configuration")},
    {cro_servermap, N_("Server Map Configuration")},
    {cro_clustermap, N_("Cluster Configuration")},
    {cro_url_filtermap, N_("Filter-Map Configuration")},
    {cro_accesslog, N_("Accesslog Configuration")},
    {cro_namespace, N_("Namespace Configuration")},
    {cro_media_cache, N_("Media Cache Configuration")},
    {cro_analytics, N_("Analytics Configuration")},
    {cro_errorlog, N_("Errorlog Configuration")},
    {cro_streamlog, N_("Streamlog Configuration")},
    {cro_tracelog, N_("Tracelog Configuration")},
    {cro_cachelog, N_("Cachelog Configuration")},
    {cro_fuselog, N_("Fuselog Configuration")},
    {cro_fmsaccesslog, N_("FMS Accesslog Configuration")},
    {cro_fmsedgelog, N_("FMS Edgelog Configuration")},
    {cro_publishlog, N_("Media Flow Publisher Log Configuration")},
    {cro_resource_pool, N_("Resource Pool Configuration")},
    {cro_policy, N_("Policy Configuration")},
    {cro_scheduler, N_("Scheduler Configuration")},
    {cro_crawler, N_("Crawler Configuration")},
    {cro_ssl, N_("SSL Configuration")},
    {cro_bgp, N_("BGP Configuration")},
    {cro_systune, N_("Miscellaneous System Configuration and Tuning")},
    {cro_watchdog, N_("Watchdog Configuration")},
    {cro_device_map, N_("Device Map Configuration")},
    {cro_log_analyzer, N_("Log Analyzer Configuration")},
    {cro_junos_re, N_("Junos RE configuration")},
/* Juniper added the preceeding. */
    {0, NULL}
};


/* ----------------------------------------------------------------------------
 * Other CLI options
 */

/*
 * If you want to permit underscores in CLI command literal strings,
 * uncomment this line.  But before doing this, please consider
 * converting all of the underscores in your CLI commands to hyphens,
 * for consistency with the hyphens in the command set defined by the
 * Samara reference modules.
 */
/* #define cli_allow_underscores */

/*
 * The CLI infrastructure imposes a maximum length on each literal
 * word of a CLI command registration.  This is so that you can safely
 * assume that the rest of the line (on an 80-column terminal) is
 * available for help text.  The default for this maximum is 16.
 * If you want to override it, uncomment the definition below and
 * change the value to whatever you want.
 */
/* Juniper changed the following. */
#define clish_max_word_len 25 
/* Juniper changed the preceeding. */

/*
 * If you want to ignore case in matching CLI command literals,
 * uncomment this line.  Note that this does not apply to matching
 * wildcards.
 */
/* #define cli_case_insensitive */

/*
 * This constant allows you to restrict the use of the "-c" command line
 * parameter, which can allow users to run bash shell commands using
 * accounts that have the CLI as the shell, without having to escape to
 * the shell from the CLI.
 *
 * Anything in this list of commands will be fair game for a program
 * to be run using the technique:
 *
 *    ssh -l <username> <hostname> <command>
 *
 * To disallow all commands from running in this manner, set this to
 * the empty string.  To allow any commands to be run in this manner,
 * set this to NULL.  Note that this is the mechanism used by scp and
 * sftp, so you'll probably want to allow them unless you will offer
 * other mechanisms to transfer files onto and off of the system.
 * cli_exec_allow_standard is a preprocessor constant defined in
 * cli_module.h that permits scp and sftp only; you may use it alone,
 * or as a basis to which to concatenate your own list of additional
 * commands to allow.
 *
 * Note also that if you have either the CMC server or CMC client
 * features enabled, RGP will be permitted automatically even if it is
 * not listed in here.  This will allow opening a GCL session with the
 * local mgmtd as the user who logged in, but cannot be used to gain
 * shell access.
 *
 * The syntax of this string is that the first character is
 * interpreted as a delimeter, and the rest of the string is a series
 * of strings separated by that delimeter.  Each item will be a string
 * which the first word of argv[2] must match in order for the "-c"
 * option to be accepted.
 *
 * NOTE: If you want the XML gateway to be accessible via ssh, then
 * add ",/opt/tms/bin/xg" to your cli_exec_allow.
 *
 * NOTE: If you want to use libremgcl on a remote system to connect to
 * this system and run RGP to make mgmtd requests remotely, then add
 * ",/opt/tms/bin/rgp_raw" to your cli_exec_allow.
 *
 * NOTE: this constant is mandatory and must be defined in this file!
 * This is done, rather than having a resonable default, to ensure
 * that existing customers each makes a conscious decision about what
 * commands to allow or deny, lest this be left as an unknown security
 * hole.  New customers who don't see this setting will get good 
 * default protection because the generic customer sets it to
 * cli_exec_allow_standard, which only permits scp and sftp.
 */
/* Juniper added the following. */
#ifdef ALLOW_SCP_SFTP
/* For special builds, ALLOW: ",scp,/usr/bin/scp,/usr/libexec/openssh/sftp-server" */
/* Juniper added the preceeding. */
#define cli_exec_allow cli_exec_allow_standard
/* Juniper added the following. */
#else
/* By default, disallow scp and sftp. */
#define cli_exec_allow "/opt/tms/bin/rgp"
#endif
/* Juniper added the preceeding. */

/*
 * The "reset factory" command's default behavior is to reboot the
 * system when it's done.  To change the default to be halt instead
 * (as it used to be before the Fir release), enable this #define.
 */
/* #define cli_reset_factory_default_halt */


/*
 * This item is only relevant if PROD_FEATURE_SHELL_ACCESS is enabled.
 * If that feature is not enabled, defining the constant documented here
 * (cli_shell_access_license_action_basic) will produce a build failure.
 *
 * If PROD_FEATURE_SHELL_ACCESS is enabled, the "SHELL_ACCESS" license can
 * list user accounts to which it wants to grant the ability to enter the
 * UNIX shell.  However, by default its scope is limited by the access
 * control restrictions registered on the "_shell" and "_exec" commands.
 * Under PROD_FEATURE_CAPABS, these commands require 'action_restricted';
 * under PROD_FEATURE_ACLS, they require the all_action_9 (essentially
 * admin), or diag_action_7 auth groups.
 *
 * Under PROD_FEATURE_CAPABS, if you want a broader range of users to be
 * able to be granted shell access by means of the SHELL_ACCESS license,
 * you may enable the #define below.  The effect of this is as follows:
 *
 *   - The "_shell" and "_exec" commands will be registered to require
 *     action_basic, rather than action_restricted.  Note that this does
 *     not automatically grant shell access to this broader range of
 *     users -- when PROD_FEATURE_SHELL_ACCESS is enabled, they will
 *     still need a SHELL_ACCESS license which grants them access
 *     (either by specifically naming their username, or by granting it
 *     indiscriminately to all users).
 *
 *   - Two actions which had the same required capabilities as these
 *     commands will now also require only action_basic.  This is because
 *     they must be accessible to the user entering the shell.  The
 *     actions are "/license/actions/decode" (decode a license entered
 *     interactively, in case a sufficient license is not present in
 *     config); and "/logging/actions/systemlog/log" (log a message to
 *     the permanent systemlog, which we do anytime someone enters the
 *     shell, as a means of audit logging).
 *
 * Under PROD_FEATURE_ACLS, this #define has no effect.  If you want
 * additional users to be able to enter the shell using a SHELL_ACCESS
 * license, grant them the 'diag' role, which is a member of the
 * 'diag_action_7' auth group.  Note that this also confers some
 * additional privileges on the user, such as viewing the logs, which
 * they would not have previously had.
 *
 * Note that this does not make it any easier to gain access to the
 * shell using a RESTRICTED_CMDS license.  If a license for that feature
 * is present, it still only grants shell access for users with the 
 * original required capability, action_restricted.
 *
 * CAUTION: It is not recommend to enable this #define!
 * It is considered a weakening of security to allow lower-privileged
 * users to enter the shell, or to use the license decode or systemlog
 * actions mentioned above.
 */
/* #define cli_shell_access_license_action_basic */


/******************************************************************************
 ******************************************************************************
 *** Network interfaces
 ******************************************************************************
 ******************************************************************************
 ***/

/* ............................................................................
 * Default name for the primary interface. SHOULD NOT be used directly.
 * The name should be retrieved from the function lc_if_primary_name(),
 * which includes a graft-point for possible modifications, e.g. based on
 * product model numbers.
 */
#define md_if_primary_name "eth0"

/* ............................................................................
 * Name of the interface to potentially tie a license to (see license.h).
 * The current restriction is that this interface name would have to be
 * the same for all product model numbers build from this source.
 */
#define md_if_license_name "eth0"

/* ............................................................................
 * The interface module needs to be able to distinguish between
 * virtual and physical interfaces.  By default, this is done by
 * checking the list of virtual interfaces below, and considering an
 * interface to be physical only if it does not occur in that list.
 * If you want to do it the other way around -- check the list of
 * physical interfaces below and consider it physical only if it does
 * occur in that list -- then #define md_if_use_physical_interface_list
 * below and fill out the physical_interfaces variable instead.
 *
 * The strings in these lists are considered to represent exact string
 * matches, except if the last character is a '*', in which case what
 * comes before is considered to be a prefix.
 *
 * The effect of an interface being judged virtual is that if the user
 * attempts to query or set the speed or duplex of the interface, we
 * will decline the request without trying and without logging an
 * error.
 */

/* #define md_if_use_physical_interface_list */

/*
 * This is a list of virtual (non-physical) interfaces potentially in
 * the system.  It is only heeded if md_if_use_physical_interface_list
 * is NOT defined.
 *
 * Note: this list MUST be NULL-terminated. 
 */
static const char *virtual_interfaces[] __attribute__((unused)) = {
    "lo",
    NULL
};

/*
 * This is a list of physical interfaces in the system.  It is only 
 * heeded if md_if_use_physical_interface_list IS defined.
 *
 * Note: this list MUST be NULL-terminated. 
 */
static const char *physical_interfaces[] __attribute__((unused)) = {
    NULL
};


/* ............................................................................
 * This is an optional mechanism to hide interfaces from view.  If an
 * interface name is found in this list, or matches one of the
 * patterns in this list (with '*' at the end signalling a prefix), an
 * iterate under /net/interface/state will not turn it up.
 * Some implications of this are:
 *   - The "show interfaces" command will not show them (unless they 
 *     are named directly)
 *   - They will not be sampled by statsd's interface sample record
 *   - They will not show up in the Web UI's interface graph
 *
 * Note that the get processing for the state nodes do NOT heed this
 * array, so the interfaces will still show up if named directly.
 *
 * By default, this array is not used.  To use it, enable the #define
 * of MD_INTERFACE_HIDDEN_NAME_PATTERNS below, and populate the array.
 * The array MUST be NULL-terminated.
 */

#if 0
#define MD_INTERFACE_HIDDEN_NAME_PATTERNS

static const char *md_interface_hidden_name_patterns[] 
__attribute__((unused)) = {
    NULL
};
#endif


/* ............................................................................
 * This is a list of either exact interface names or interface
 * name prefixes (which must end in a '*' to be considered
 * a prefix) to enforce interface name validation for bindings
 * which take interface names. If empty any name is considered
 * valid, otherwise a match must occur. The loopback interface
 * 'lo' is always considered valid.
 *
 * Note: md_virt_ifname_prefix ("vif") and md_virt_vbridge_prefix
 * ("virbr") will not be permitted as interface name prefixes,
 * regardless of what is listed here.
 *
 * Note: this list MUST be NULL-terminated. 
 */
static const char *interface_name_validation[] __attribute__((unused)) = {
    NULL
};

/* ............................................................................
 * Must set this in order to use the validation array
 */
#define INTERFACE_ALIAS_INDEX_VALIDATION_ARRAY 1

/*
 * This is a list of either exact interface alias index names or
 * prefixes (which must end in a '*' to be considered a prefix)
 * to enforce interface alias index validation for bindings which
 * take alias index names. If empty, any index is considered valid,
 * otherwise a match must occur to be valid.
 *
 * Note: this list MUST be NULL-terminated. 
 */
static const char *interface_alias_index_validation[] 
__attribute__((unused)) = {
    NULL
};

/*
 * Note: the MD_IF_STATE_POLL_INTERVAL_SEC setting is no longer honored,
 * because we no longer poll for interface state changes.  Instead, we
 * rely on netlink events to notify us of changes.
 */


/* ............................................................................
 * Use this to override our default behavior for applying bonding changes
 * from md_bonding.c.  There is some optional logic called the
 * "streamlined apply" which, in a specific limited subset of cases, will
 * apply bonding changes in a more efficient and less disruptive manner.
 * However, this logic has been shown to be unsafe under EL5 using the
 * stock kernel (currently 2.6.18-274.7.1.el5) -- presumably due to a bug
 * in this kernel version -- and so it is disabled by default under EL5.
 * It is enabled by default under other Base OS platforms, such as EL6.
 * Bug 14695 in our Bugzilla describes this in more detail.
 * 
 * You should generally leave this default alone.  You might wish to
 * override it if (a) you are building on EL5; and (b) you have intergrated
 * a later kernel in which this bug is known to be fixed.
 *
 * CAUTION: if you override this to enable streamlined apply on EL5, you
 * should test bonding carefully to make sure there is no negative impact.
 * The pair of scripts test/bin/mgmtd/modules/md_bonding_bug_14695_* may
 * help reproduce the bug.
 */
/* #define MD_BONDING_STREAMLINED_APPLY true */


/******************************************************************************
 ******************************************************************************
 *** User Accounts, Capabilities, and ACLs
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * Default password hash algorithm to use, if none is specified.  Please
 * see ltc_password_algo for details.  The historical default was MD5,
 * but as it is now thought to be less secure, SHA256 or SHA512
 * (preferred) are thought to be better choices.
 *
 * The rounds settings, if SHA256 or SHA512 are in use, gives the number
 * of iterations used as part of the hash calculation.  Set to 0 to get
 * the algorithm default (5000 for SHA256 and SHA512).  Higher values
 * yield more costly to compute (and so potentially more secure)
 * password hashes.  This likely should not be set above 10000 on
 * anything but very fast hardware.
 *
 * Defaults to lpa_sha512 and 0 on x86, and lpa_md5 and 0 on PowerPC.
 */
/* #define ltc_default_password_algo lpa_sha512 */
/* #define ltc_default_password_rounds 0 */


/*
 * Default passwords to be used for admin, monitor, and operator
 * accounts.  These passwords should be in crypted form; i.e. the form
 * that would show up in /etc/shadow.  For each password string, your
 * options are:
 *   - To require no password for login, use the empty string.
 *   - To disallow password-based login, but to still permit login via
 *     ssh keys (once they are installed), use "*".
 *   - To disallow any login, use "!!", or do not define the constant.
 *     This is not permitted for the admin account.
 *   - To set a password for login, use the crypted password string.  To
 *     find the string to use for a given password, configure that
 *     password on an account on a running system, and copy the string out
 *     of /etc/shadow, or use the "crypt" binary, with "crypt -- password" .
 *     e.g. a string to use for the password "test123" using SHA512
 *     would be:
 *
 *        $6$sB07ON.G$lrhezOXeTDR6j0IsDCjElwCdzwO519XtR1lI9vxPDZENl.wPq2ahab6N3lyTEGFpHgD7wuh0e6ik9NRkjxTZP0
 *
 *     or using MD5 would be: "$1$X031XkCz$Kivxq78zKdGcT9Xymq0D11" .
 *
 *     Note that if you are targetting PowerPC, only MD5 is currently
 *     supported.
 *
 * These accounts each work as follows:
 *
 *   - Admin: must be enabled.  Status is determined as follows:
 *       (a) If md_passwd_default_admin_password is set, use it.
 *       (b) Otherwise, if md_passwd_default_password is set, use it.
 *       (c) Otherwise, it's a build-time error.
 *
 *   - Monitor: may optionally be enabled.  Status is determined as follows:
 *       (a) If md_passwd_default_monitor_password is set, use it.
 *       (b) Otherwise, if md_passwd_default_password is set, use it.
 *       (c) Otherwise, the account is locked out by default.
 *           (equivalent to setting hashed password to "!!")
 *
 *   - Operator: may optionally be enabled on systems with 
 *     PROD_FEATURE_ACLS enabled.  If an existing system running an
 *     image with PROD_FEATURE_CAPABS is upgraded to one with ACLs,
 *     an operator user is added, if and only if there was not already
 *     an operator user.  One will also be present on any newly 
 *     manufactured systems.  The status of the operator user is
 *     determined as follows:
 *       (a) If md_passwd_default_operator_password is set, use it.
 *       (b) Otherwise, the account is locked out by default
 *           (equivalent to setting hashed password to "!!")
 *
 * NOTE: the definitions below enable the admin account by default for
 * login with no password.  They also lock out the monitor and
 * operator accounts by default.  If copying this code, please review
 * it in light of your security policy.
 */
#define md_passwd_default_admin_password ""
/* #define md_passwd_default_operator_password "" */
/* Juniper changed the following. */
#define md_passwd_default_monitor_password ""
/* Juniper changed the preceeding. */


/*
 * Normally the default capability group for a freshly-created user
 * account is 'admin' (gid 0).  If you want it to be different, enable
 * the #define below, and set it to whichever instance of mdc_gid_type
 * you want to be the default.
 *
 * This is not relevant under ACLs, and there is no corresponding 
 * setting for that because the default set of roles a user gets 
 * on creation is controlled through configuration.
 */
/* #define md_passwd_default_gid mgt_monitor */


/*
 * Enable the #define below to make both mgmtd and the CLI enforce at
 * node registration time that ACLs should be registered before
 * capabilities.  Nothing makes this strictly necessary; it's just a
 * coding convention for the sake of readability, which the mgmtd and
 * CLI infrastructures can enforce for you if you want.  This means:
 *   - In mgmtd, call mdm_node_acl_add() before setting mrn_cap_mask.
 *   - In the CLI, call cli_acl_add() before setting cc_capab_required.
 *
 * If a violation is detected, a message is logged at WARNING.
 *
 * Note: this may not be useful to enable, except briefly during
 * development, because it yields false alarms when one module amends
 * the ACLs of a node registered by a different module.  (XXX/EMT: fix)
 */
/* #define ACL_CAPAB_ORDER_ENFORCE */


/*
 * Applicable only under PROD_FEATURE_ACLS.  By default, the commands
 * to set a capability level on a user account are hidden but available,
 * and they replace that user's current set of roles with those of the
 * specified capability.  The 'no' variant reset the set of roles to
 * the default.  This is for backward compatibility, per bug 14278.
 * Enable the #define below to leave these commands unavailable instead,
 * for a more pure ACLs-only environment.
 */
/* #define ACL_CAPAB_CLI_UNAVAIL */


/******************************************************************************
 ******************************************************************************
 *** General usage
 ******************************************************************************
 ******************************************************************************
 ***/

static const char *system_model_name __attribute__((unused)) =
"system";

/*
 * The base of the hostname for new machines.  After this we append
 * a hyphen and the last 6 digits of the MAC addr of the primary interface.
 */
/* Juniper changed the following. */
#define md_system_initial_hostname_base "mfc-unconfigured"
/* Juniper changed the preceeding. */

/*
 * Numeric tree node normalization bug 
 *
 * By default, the libcommon 'tree' data structure will NOT exhibit
 * bug 14235.  This bug caused certain peer nodes to be mistaken for
 * equal in some contexts, if they matched when converted to numbers,
 * but did not match as strings, e.g. "15" and "015".
 *
 * If a config database has been written in the past by a system which
 * exhibited this bug (any Samara release prior to Fir Update 2),
 * there is a slim but nonzero possibility of failure when upgrading
 * to an image with the bug fixed.  This could be an initial commit
 * failure, or it could be a runtime error later on, depending on the
 * circumstances.  At worst, this could cause the system to lose
 * network connectivity, and require the configuration to be reset to
 * factory defaults.
 *
 * Therefore, any existing customers with deployments where such a
 * failure would not be acceptable must define the preprocessor
 * constant lc_tree_numeric_compare_bug (as shown in the commented-out
 * #define below).  New customers, or those willing to run the risk of
 * failure and resetting to factory defaults, may omit the #define.
 *
 * A future release will take steps to eradicate this bug altogether.
 * This may entail upgrade logic to repair objects whose names have
 * caused them to be affected by the bug; or possibly adaptively
 * exhibiting the bug only if the database being read is detected to
 * need it.
 */
/* #define lc_tree_numeric_compare_bug */


/******************************************************************************
 ******************************************************************************
 *** Email notification
 ******************************************************************************
 ******************************************************************************
 ***/

#define md_email_event_subject_prefix \
    D_("System event", CUSTOMER_INCLUDE_GETTEXT_DOMAIN)

#define md_email_failure_subject_prefix \
    D_("System failure", CUSTOMER_INCLUDE_GETTEXT_DOMAIN)

#define md_email_debug_dump_name \
    D_("System debug dump", CUSTOMER_INCLUDE_GETTEXT_DOMAIN)

/*
 * This is embedded in the special headers we add to notification
 * emails, e.g. X-tallmaple-build_prod_release.  This should be kept in
 * sync with EMAIL_HEADER_BRANDING in customer.sh.  This must be
 * composed of only alphanumeric characters plus "-" and "_" , and be at
 * most 32 characters long.
 */
/* Juniper changed the following. */
#define md_email_header_branding "Juniper Networks Media Flow Controller"
/* Juniper changed the preceeding. */

/*
 * NOTE: the following values are used to initialize configuration
 * nodes, which are then used to control behavior.  So if you change
 * these values and there are already systems deployed with the old
 * values, you'll need to do a mgmtd module semantic upgrade if you
 * want the new values to be used.
 */

/* Juniper added the following. */
/* NOTE: These default values are not seen in the nodes.  The values
 * set in bin/mgmtd/modules/md_nvsd_pm.c seem to be used.
 */
/* Juniper added the preceeding. */
/* Juniper changed the following. */
static const char *md_email_autosupport_addr __attribute__((unused)) =
"";

static const char *md_email_autosupport_hub __attribute__((unused)) =
"";

static const char *md_email_autosupport_webpost_url __attribute__((unused)) =
"";
/* Juniper changed the preceeding. */


/******************************************************************************
 ******************************************************************************
 *** Licensing
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * Shared secret(s).
 *
 * Here we list one or more valid shared secrets, and zero or more
 * revoked shared secrets.  When a license is generated, a shared
 * secret is used to encode the hash -- in order for that license to
 * be considered valid, we must be able to reproduce the same hash
 * using one of the valid shared secrets.
 *
 * If a license is found to have been generated using a revoked shared
 * secret, it is treated the same as an unrecognized secret, i.e. it
 * is invalid.  The only difference is that the user is told that it 
 * was revoked, so they know that it used to be valid.
 *
 * Almost this entire subsection should be conditional on
 * LK_NEED_VALIDATORS.  This is defined only by liblicense_validation's
 * Makefile, to keep the keys from ending up in more binaries than
 * actually need them.  Note that the secret magic numbers should be
 * defined outside, since your global validation callbacks may need access
 * to them (without needing to know the specifics of the secrets).
 *
 * This list MUST be terminated with an empty entry:
 * {lvt_none, lvs_none, NULL, {-1}, 0, lvrc_none, NULL, 0, NULL}.
 */

#define LK_SECRET_MAGIC_1  1

#ifdef LK_NEED_VALIDATORS

#define LK_USE_VALIDATORS

#include "license_validator.h"

static const lk_validator lk_validators[] = {
/* Juniper changed the following. */
    {lvt_secret_plaintext,  lvs_valid,   "^#Ve@2n8+g5kMn588", {-1}, LK_SECRET_MAGIC_1,
/* Juniper changed the preceeding. */
     lvrc_none, NULL, 0, NULL},

    {lvt_none,              lvs_none,  NULL,     {-1}, 0,
     lvrc_none, NULL, 0, NULL}
};

#endif /* #ifdef LK_NEED_VALIDATORS */


/* ............................................................................
 * Hash requirements.  These settings allow you to control (a) the minimum
 * hash length that will be accepted; (b) the hash algorithms that will
 * be accepted; and (c) the default hash length and algorithm that will
 * be used by genlicense if not specified in its command-line arguments.
 *
 * Presumably, the setting for (c) will conform to the requirements set
 * by (a) and (b).  Furthermore, it is recommended that it conform to
 * the most strict requirements declared in lk_validators, if any of
 * the validators specify overrides for hash length and type.
 *
 * By default, if these are not defined, all hash lengths and all
 * algorithms are accepted.  You may make your licenses with long
 * SHA256 hashes; but if less secure ones are still accepted, and if
 * an attacker is familiar with the license key format and wanted to
 * forge a license, they would find it easier to do so if they used a
 * 48-bit MD5 hash.
 *
 * The minimum hash length is just an integer, and may be any of {48,
 * 96, 128, 256}.  The algorithm is a comma-delimited list of strings,
 * where the strings may be chosen from {hmac_md5, hmac_sha256}.
 * (Though in practicality, if you are setting it, you'll probably
 * only be specifying one of these, so there would be no commas.)
 *
 * Licenses which do not meet these minimum requirements will be
 * treated as invalid, just the same as if the hash was not consistent
 * with the rest of the license.  Genlicense by default will refuse to
 * generate licenses which do not meet these requirements, but it can be
 * forced to do so by using the "-f" option.
 */

#define lk_hash_min_length             48
#define lk_hash_permitted_algorithms   "hmac_md5,hmac_sha256"
#define lk_hash_type_default           lht_hmac_sha256_48_bit


/* ............................................................................
 * The licensed feature name we should use to control access to the
 * functionality managed by PROD_FEATURE_RESTRICT_CMDS.
 *
 * By default this is "RESTRICTED_CMDS", and should usually be left
 * this way.  The option to change it was added mainly to help
 * customers for whom insufficiently restricted licenses for this
 * feature has become too widely distributed.  If too many users get a
 * hold of such a license, it defeats the purpose of
 * PROD_FEATURE_RESTRICT_CMDS.  You could change your shared secret,
 * but that also invalidates other licenses you have distributed,
 * which may be too disruptive.  So by changing this feature name, all
 * the old RESTRICTED_CMDS licenses become inert, and you can reassert
 * control over these restricted features.
 *
 * NOTE: if this string is changed (or initially set) from one release
 * to another, no upgrade rules are necessary.  However, if an active
 * license for the old feature was installed before, it will become
 * inert, and the user will lose access to the features it used to
 * unlock; and there will be no warning or explanation for this in the
 * logs or the UI.
 *
 * See tdefaults.h for default (obviously, "RESTRICTED_CMDS").
 */
/* #define RESTRICTED_CMDS_LICENSED_FEATURE_NAME "RESTRICTED_CMDS_NEW" */


/* ............................................................................
 * Support for type 1 license keys ("LK1-*"): define LK1_OBSOLETE to
 * render all type 1 licenses invalid.
 *
 * Type 2 licenses are strictly better (more secure, support more options,
 * etc.)  than type 1 licenses.  The only reason to continue using a
 * type 1 license is the trouble of generating and installing an
 * equivalent type 2 license.  Type 2 licenses have been supported since
 * the Cedar release of Samara, so in many cases they will be the only
 * type of licenses still in active usage.
 */
#define LK1_OBSOLETE


/******************************************************************************
 ******************************************************************************
 *** Wizard
 ******************************************************************************
 ******************************************************************************
 ***/

#ifdef PROD_FEATURE_WIZARD

/*
 * Banner to be displayed when Wizard is launched.
 */
/* Juniper changed the following. */
#define wiz_banner \
    D_("Media Flow Controller - Configuration Wizard", \
       CUSTOMER_INCLUDE_GETTEXT_DOMAIN)
/* Juniper changed the preceeding. */

#define wiz_hostname_example "box1.example.com"
#define wiz_domain_example "example.com"

/*
 * The user will be prompted to set the admin password as one of the
 * Wizard steps.  If you also want them to be prompted to set the
 * monitor password, uncomment the following definition.
 */
/* #define wiz_prompt_monitor_password */

#endif /* PROD_FEATURE_WIZARD */


/******************************************************************************
 ******************************************************************************
 *** MOTD/ISSUE/ISSUE.NET
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * These messages are not internationalized.  That's probably OK
 * because they are initial config so they will be frozen at whatever
 * locale was in effect at the time the DB was created, and they are
 * strings that the end user is allowed to change anyway.
 */

/* Juniper changed the following. */
/* These two are used to create /etc/motd and /etc/issue and /etc/issue.net */
#define md_system_motd \
"\nJuniper Networks Media Flow Controller\n"

#define md_system_issue \
"\nJuniper Networks Media Flow Controller\n"
/* Juniper changed the preceeding. */

#define md_system_issue_net md_system_issue


#endif /* CUSTOMER_H_REDIST_ONLY */


/*
 * The next block of this file is needed by components requiring
 * redistribution of source code, either directly or indirectly.
 */

/******************************************************************************
 ******************************************************************************
 *** SNMP
 ******************************************************************************
 ******************************************************************************
 ***/
/* Juniper added the following. */
//NET-SNMP-AGENT-MIB.txt
//NET-SNMP-EXAMPLES-MIB.txt
//NET-SNMP-EXTEND-MIB.txt
//NET-SNMP-MIB.txt
//NET-SNMP-MONITOR-MIB.txt
//NET-SNMP-SYSTEM-MIB.txt
//NET-SNMP-TC.txt
/* Juniper added the preceeding. */

/* ............................................................................
 * This must list all MIBs you are adding, separated by ':' characters.
 * e.g. "TMS-MIB:MY-MIB:MY-OTHER-MIB".
 */
/* Juniper changed the following. */
#define CUSTOMER_MIB_NAME "JUNIPER-MFC-MIB:JUNIPER-MFC-NOTIF-MIB"
/* Juniper changed the preceeding. */

/* ............................................................................
 * Enterprise Numbers
 *
 * The 16858 you see below is the IANA Enterprise Number for
 * Tall Maple Systems.  You'll probably want to replace this with
 * your own.
 *
 * It is possible for you to have multiple MIBs underneath different
 * enterprise numbers.  This may happen in the case of company
 * mergers, acquisitions, etc.  See the examples in the 'demo' product
 * of OTHER-ENT-MIB.txt and sn_mib_other_ent.c.  These can be queried,
 * set, and traps can be sent from them with the correct trap OID and
 * enterprise number.
 *
 * But even if you have multiple enterprise numbers, you'll still only
 * define a single one below.  One of them has to be "dominant" for
 * cases outside the scope of your vendor MIBs which need a single
 * enterprise number.  This includes:
 *   - Serving the SNMP variable sysObjectID.0.
 *   - For inclusion in standard SNMP traps (coldStart, linkDown, etc.)
 *   - The first 4 octets of the SNMP engine ID.
 *   - RADIUS vendor-specific attributes (like our local user name
 *     mapping) are defined to be relative to the main enterprise number.
 */

/* Juniper changed the following. */
/* The assigned enterprise number for the NET_SNMP MIB modules. */
#define ENTERPRISE_OID                  2636
#define ENTERPRISE_MIB                  1,3,6,1,4,1,2636
#define ENTERPRISE_DOT_MIB              1.3.6.1.4.1.2636
#define ENTERPRISE_DOT_MIB_LENGTH       7

/* The assigned enterprise number for sysObjectID. */
#define SYSTEM_MIB              1,3,6,1,4,1,2636,1,2
#define SYSTEM_DOT_MIB          1.3.6.1.4.1.2636.1.2
#define SYSTEM_DOT_MIB_LENGTH   9

/* The assigned enterprise number for notifications. */
#define NOTIFICATION_MIB                1,3,6,1,4,1,2636,1,2,2
#define NOTIFICATION_DOT_MIB            1.3.6.1.4.1.2636,1.2.2
#define NOTIFICATION_DOT_MIB_LENGTH     10
/* Juniper changed the preceeding. */

/*
 * Alternative enterprise number to use when constructing the engine ID.
 * By default ENTERPRISE_OID is used here, and usually this should be
 * correct, so defining this to something else should be rare.
 */
/* #define SNMP_ENGINEID_ENTERPRISE_OID 16858 */


/* ............................................................................
 * Multiple community support
 *
 * Until Fir Update 1, Samara supported only a single read-only
 * community, but it was configurable with the same command that Cisco
 * used to manage multiple communities.  Starting in Fir Update 1,
 * the system can be configured into two modes:
 * 
 *   - No multiple communities: disallow multiple communities
 *     (backward compatible with previous versions of Samara).
 *
 *   - Multiple communities: allow multiple communities (compatible
 *     with Cisco commands; but changes behavior of Samara commands
 *     vs. previous releases).
 *
 * By default, multiple communities are enabled.  To disable them by
 * default (the user, or a mgmtd module of yours, may still enable
 * them later), uncomment the define below.
 *
 * NOTE: once a system has been upgraded to run an image with multiple
 * community support, the #define below will only affect initial
 * values, and will not automatically change the value on upgrade.
 * To get upgrade handled for you, this must be set before the system
 * is first upgraded to this version.  Otherwise, you may change the
 * value on upgrade from a mgmtd module of your own.
 */
/* #define SNMP_MULTIPLE_COMMUNITIES_DEFAULT_STR "false" */


/* ............................................................................
 * Define the value to be served for mib-2.system.sysServices.0 here.
 * See comments above sysServices in /usr/share/snmp/mibs/SNMPv2-MIB.txt 
 * for information on how to set this.  If you do not define anything,
 * a default of 72 will be chosen.  If you set it to a negative number,
 * sysServices will be suppressed altogether, and no value will be served.
 */
/* #define SNMP_SYS_SERVICES 72 */


/* ............................................................................
 * There are three occasions related to (re)initialization on which
 * snmpd can send traps:
 *
 *   1. When it is launched.  We automatically send the standard SNMP 
 *      coldStart trap, as required.  This is not configurable.
 *
 *   2. When it exits.  By default, we send nothing.  NET-SNMP originally
 *      sent enterprise-specific trap number 2, though this was removed
 *      from the default behavior because it had the potential to conflict
 *      with customer vendor MIBs.  To have a trap sent in this case,
 *      define NET_SNMP_SHUTDOWN_TRAP below to the enterprise specific
 *      trap number to send in this case.  As an SNMPv2c trap, this will
 *      have an OID formed by concatenating a 0, then this number, onto 
 *      the end of NOTIFICATION_MIB.
 *
 *   3. When it processes a SIGHUP, which means rereading its 
 *      configuration, but NOT resetting sysUpTime or its other counters.
 *      By default, we send nothing.  NET-SNMP originally sent 
 *      enterprise-specified trap number 3, though this was removed
 *      from the default behavior because it had the potential to conflict
 *      with customer vendor MIBs.
 *
 *      From the Alder through Eucalyptus Update 5 releases, we sent
 *      the standard coldStart trap in this case, though this was
 *      determined not to be correct: since the uptime and counters
 *      are not reset, this is not considered "reinitialization", even
 *      though the configuration may have changed.  So starting in Fir,
 *      this was changed to send no trap.  To revert to sending the 
 *      coldStart trap in this case, define NET_SNMP_COLDSTART_ON_SIGHUP
 *      below.  Please note that this is not recommended, but is only 
 *      offered for customers who wish to preserve the old behavior.
 */
/* #define NET_SNMP_SHUTDOWN_TRAP     2  */
/* #define NET_SNMP_COLDSTART_ON_SIGHUP  */


/* ............................................................................
 * Data for SNMP tables is always served out of a cache.  The cache
 * lifetime (maximum age of data in the cache before the cache must be
 * refreshed) can be defined here in milliseconds.
 *
 * There are two values you can define:
 *   - SNMP_CACHE_LIFETIME_MS
 *   - SNMP_CACHE_LIFETIME_LONG_MS
 *
 * We decide which lifetime to use based on a comparison between the
 * OID currently requested and the last one requested from this table.
 * If the currently requested one comes after the last requested (or
 * matches it, a necessary allowance due to how NET-SNMP transitions
 * from one column to the next during a walk), we use the long
 * lifetime, because this probably means the continuation of a walk.
 * Otherwise, we use the regular lifetime.
 *
 * If only SNMP_CACHE_LIFETIME_MS is defined, the same value is used
 * for both lifetimes.  If neither is defined, defaults are taken from
 * sn_cache_lifetime_default_ms and sn_cache_lifetime_long_default_ms
 * in sn_mod_reg.h.
 *
 * Data for SNMP scalar values is not cached.
 *
 * Note that the old cache settings, SNMP_CACHE_INDEX_LIFETIME_MS and
 * SNMP_CACHE_FIELDS_LIFETIME_MS, are no longer used.
 */
#if 0
#define SNMP_CACHE_LIFETIME_MS        10000   /* 10 seconds */
#define SNMP_CACHE_LIFETIME_LONG_MS   20000   /* 20 seconds */
#endif


/* ............................................................................
 * Timeouts on mgmtd requests
 *
 * When mgmtd requests are sent using the md_client API (anything
 * starting with "mdc_", like mdc_send_mgmt_msg()), they are synchronous,
 * and are subject to a timeout.  When they are sent using 
 * sn_send_mgmt_msg_async_takeover(), they are asynchronous, and are not
 * subject to a timeout (but can be cancelled by an explicit action).
 *
 * The default timeout for synchronous requests is 65 seconds.
 * This default can be overridden by setting the define below.
 */

/* Juniper changed the following. */
/* #define SNMP_MGMTD_TIMEOUT_SEC          120 */
/* Juniper changed the preceeding. */


/* ............................................................................
 * Lifetime of SNMP SET results
 *
 * By default, the outcome of an SNMP SET request will be retained in the
 * memory of the SNMP agent for 300 seconds (5 minutes) after it completes.
 * You can override that number here.  This is mainly a tradeoff between
 * memory consumption and availability of old information.
 */

/* #define SNMP_SET_RESULT_LIFETIME_SEC  300 */


/* ............................................................................
 * The settings below can be used to specify an alternate source of
 * data to be used when constructing the engine ID for SNMP v3.  The
 * first four bytes of the engine ID are determined automatically: they
 * contain the Enterprise OID as a uint32 in NBO, with the high bit set.
 * The optional settings below control what follows.
 *
 * What we need is a "method octet" (a single byte chosen from one of 
 * the set of choices listed in RFC 3411, section 5, item #3), followed
 * by a chunk of data which is 1-27 bytes long.  There are two ways you 
 * can specify this:
 *
 *   1. Set SNMP_ENGINEID_METHOD_OCTET to the method octet; and set
 *      SNMP_ENGINEID_TEXT_NODE_NAME to the name of a node from which
 *      the remaining data can be fetched.  In this case, the node name
 *      must point to a node which is either of type bt_binary, or of
 *      a string-based data type (a type for which bn_type_string()
 *      will return true).  The contents must be 1-27 bytes long.
 *
 *   2. Do not set SNMP_ENGINEID_METHOD_OCTET; and set 
 *      SNMP_ENGINEID_TEXT_NODE_NAME to the name of a node from which 
 *      both the method octet and the remaining data can be fetched.
 *      The method octet is taken from the first byte.  In this case,
 *      the node name must point to a node which is of type bt_binary,
 *      and which is 2-28 bytes long.
 *
 * In either case, the node referenced must be either one of the
 * following:
 *
 *   (a) An internal monitoring node whose value will never change.
 *       (If the value did change, the passwords set for SNMP v3 
 *       access would no longer work, at least until a reboot or 
 *       an SNMP-related configuration change forced the snmpd.conf
 *       file to be regenerated.  However, the UI would not know this,
 *       show "show snmp users" would show the password as set.)
 *
 *   (b) A configuration node.  The SNMP subsystem will watch this 
 *       node for changes, and take the steps necessary to adapt to
 *       the change.
 *
 * For case #1, please #define SNMP_ENGINEID_WATCH_CONFIG_CHANGES to
 * 'false'.  For case #2, please #define it to 'true'.  If you do not
 * define it, 'true' will be assumed (which will exact a small
 * performance penalty if it's a monitoring node which cannot be
 * watched).
 *
 * The example below demonstrates a node which contains only the extra
 * text (not the method octet), and is a configuration node.
 *
 * The method octet is limited in range.  The ranges 0-3 and 6-128 are
 * reserved; so it may only be 4-5, or 129-255.  Note that the
 * semantics of methods 129-255 are not defined by the RFC, but are
 * treated effectively like method 5 ("Octets, administratively
 * assigned") by the infrastructure.  If 4 is chosen, the extra data
 * may not have any zero bytes in it, since this method means "text",
 * which must be representable as a string.  However, any other values
 * are permitted.
 *
 * The default behavior, if these constants are not defined, is to
 * use method octet 4 ("Text, administratively assigned"), and to
 * use an ASCII representation of the system host ID in hexadecimal.
 * The default behavior is also the fallback behavior in case these
 * constants are defined but produce an invalid result, such as a 
 * bad method ID or a data chunk that is not [1..27] bytes long.
 */
/* #define SNMP_ENGINEID_METHOD_OCTET           0x04 */
/* #define SNMP_ENGINEID_TEXT_NODE_NAME        "/demo/snmp/engine_id/string" */
/* #define SNMP_ENGINEID_WATCH_CONFIG_CHANGES   true */
/* Juniper added the following. */
#define SNMP_ENGINEID_METHOD_OCTET           0x04
#define SNMP_ENGINEID_TEXT_NODE_NAME        "/nkn/nvsd/system/monitor/engine_id"
#define SNMP_ENGINEID_WATCH_CONFIG_CHANGES  false 
/* Juniper added the preceeding. */


/* ............................................................................
 * Native NET-SNMP modules to be loaded dynamically
 *
 * NOTE: this is for native NET-SNMP modules, NOT for Samara SNMP
 * plug-in modules.  Most SNMP MIBs implemented by customers should be
 * Samara modules -- this mechanism here only applies to NET-SNMP
 * modules.
 *
 * NET-SNMP has its own dynamically loadable module mechanism.
 * In order to get your module loaded by NET-SNMP, you need a "dlmod"
 * line emitted into the snmpd.conf.  This is a mechanism you can use
 * to get such a line emitted.  To do this:
 *
 *   1. Remove the #if 0 below, so the code inside it will be compiled.
 *
 *   2. Modify the array so it has one or more entries, one per module
 *      you want loaded, terminated by a NULL entry.  Each entry will be
 *      emitted into the snmpd.conf on a line by itself with "dlmod "
 *      before it.  Per the snmpd.conf man page, each entry should
 *      consist of (a) the module name (which is used to construct the
 *      name of the init function which NET-SNMP will call), and 
 *      (b) an absolute path to the module file, separated by a space.
 *      The example below demonstrates a module which lives in
 *      /opt/tms/lib/snmp/netsnmp-modules.
 *
 *   3. Make sure the array is terminated by a NULL entry.
 */

#if 0

#define SNMP_ADD_DLMOD_LINES

static const char *snmp_dlmod_lines[] __attribute__((unused)) = {
    "mymodule /opt/tms/lib/snmp/netsnmp-modules/mymodule.so",
    NULL
};

#endif
/* Juniper added the following to enable dynamicly loaded NET-SNMP modules. */
#define SNMP_ADD_DLMOD_LINES   1

static const char *snmp_dlmod_lines[] __attribute__((unused)) = {
               "jmfc_snmp       /opt/tms/lib/snmp/modules/libjmfc-snmp.so",
               NULL
	};
/* Juniper added the preceeding. */

/* ............................................................................
 * Native base NET-SNMP modules NOT to be loaded statically.
 *
 * NOTE: this is for native NET-SNMP modules, NOT for Samara SNMP
 * plug-in modules.  Most SNMP MIBs implemented by customers should be
 * Samara modules.
 *
 * This mechanism provides a way to prevent NET-SNMP from initializing
 * some of its base modules.  For example, if you wanted to
 * reimplement the hrStorage subtree of the HOST-RESOURCES-MIB using a
 * Samara SNMP plug-in module, you could specify "hr_storage" in this
 * list to prevent that MIB implementation from being registered by
 * NET-SNMP.  The coast would then be clear for you to register your
 * own implementation at the same OID.
 *
 * For a list of MIB implementation names which can be listed here,
 * please refer to:
 * src/bin/snmp/net-snmp/agent/mibgroup/mib_module_inits.h.linux
 *
 * To suppress a NET-SNMP MIB:
 * 
 *   1. Remove the #if 0 below, so the code inside it will be compiled.
 *
 *   2. Modify the array so it has one or more entries, one per module
 *      you want to suppress.
 *
 *   3. Make sure the array is terminated by a NULL entry.
 */

#if 0

#define SNMP_SUPPRESS_NETSNMP_MIBS

static const char *snmp_suppress_netsnmp_mibs[] __attribute__((unused)) = {
    NULL
};

#endif


/******************************************************************************
 ******************************************************************************
 *** Certificates
 ******************************************************************************
 ******************************************************************************
 ***/
/*
 * The following definitions specify factory default values for the generation
 * of self-signed certificates.   The web server, in particular, requires these
 * values for it's (default) self-signed certificate for https.  These defaults
 * should be set to vendor-appropriate values, and the user may override them
 * in configuration (see /certs/config/global/default/cert_gen/ * in
 * nodes.txt).  Note that the numeric values below must be defined as strings.
 * All string names below should be defined, as the initernal defaults for
 * these are not particularly appropriate for use, and are only meant to
 * prevent initial database commit failure.  All string values must be at least
 * one character in length (empty strings are not permitted).  The only fields
 * that have special constraints are country code (exactly two alphanumeric
 * characters, or else "--" for unspecified), and the numeric fields which are
 * uint16.  Key size has a minimum of 1024 bits, and days valid must be > 0.
 * Violations of these constraints will result in commit failure.
 */
/* Juniper changed the following. */
#define SSL_CERT_GEN_COUNTRY_NAME       "US"
#define SSL_CERT_GEN_STATE_OR_PROVINCE  "California"
#define SSL_CERT_GEN_LOCALITY           "Sunnyvale"
#define SSL_CERT_GEN_ORGANIZATION       "Juniper Networks, Inc."
#define SSL_CERT_GEN_ORG_UNIT           "JRS"
#define SSL_CERT_GEN_EMAIL_ADDR         "admin"
#define SSL_CERT_GEN_KEY_SIZE_BITS      "2048"
#define SSL_CERT_GEN_DAYS_VALID         "365"
/* Juniper changed the preceeding. */


/******************************************************************************
 ******************************************************************************
 *** AAA general options
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * Shared secret for hash used in TRUSTED_AUTH_INFO environment variable.
 *
 * Note: we need a 32-character secret to do the hash.  If your secret is
 * shorter than 32 characters, it will be padded with ' ' characters.  If
 * your secret is longer than 32 characters, it will be truncated.

 * Therefore, a secret of exactly 32 characters is ideal but not required.
 */
/* Juniper changed the following. */
#define AAA_TRUSTED_AUTH_INFO_SHARED_SECRET "G5%jkFd#@ioMK;pYR@!VXnKLM.0jG5SZ" 
/* Juniper changed the preceeding. */

/*
 * Under PROD_FEATURE_ACLS, if we are to support inclusion of extra
 * role strings for the user being authenticated in the local user
 * attribute (one of those defined below, according to auth method),
 * what string should be used to identify these?
 */
#define AAA_EXTRA_USER_PARAM_ROLE "role"

/******************************************************************************
 ******************************************************************************
 *** RADIUS
 ******************************************************************************
 ******************************************************************************
 ***/

/* 
 * Type definitions of customer-specific RADIUS attributes.  Note this
 * requires that ENTERPRISE_OID above is set, and is effectively
 * relative to it.  See doc/design/aaa.txt for more details.
 */
#define ENTERPRISE_LOCAL_USER_NAME_ATTR 1


/******************************************************************************
 ******************************************************************************
 *** TACACS+
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * Type definitions of customer-specific TACACS+ attributes.  See
 * doc/design/aaa.txt for more details.
 */

/* Name of the local user attribute used for authorization */
#define TACACS_LOCAL_USER_NAME_ATTR "local-user-name"

/* Service name used during authorization, may return above attribute */
/* Juniper changed the following. */
#define TACACS_AUTHORIZATION_SERVICE "mfc-login"
/* Juniper changed the preceeding. */

/* Protocol name used during authorization */
#define TACACS_AUTHORIZATION_PROTOCOL "unknown"

/* If TACACS+ authorization failures should be ignored */
/* #define TACACS_AUTHORIZATION_FAIL_IGNORE 1 */

/* Service name used during accounting */
/* Juniper changed the following. */
#define TACACS_ACCOUNTING_SERVICE "mfc-acct"
/* Juniper changed the preceeding. */

/* Protocol name used during accounting */
#define TACACS_ACCOUNTING_PROTOCOL "unknown"

/* Name of the accounting message attribute for change accounting */
#define TACACS_ACCOUNTING_CHANGES_MSG_ATTR "cmd"

/******************************************************************************
 ******************************************************************************
 *** LDAP
 ******************************************************************************
 ******************************************************************************
 ***/

/* Definition of customer-specific LDAP attribute.  See doc/design/aaa.txt */
#define LDAP_LOCAL_USER_NAME_ATTR "localUserName"

/* Typically should be "sAMAccountName" (for AD), or "uid" for LDAP */
#define md_ldap_default_login_name_attrib "sAMAccountName"

/*
 * Typically should be "member" (for group or groupOfNames) or
 * "uniqueMember" (for groupOfUniqueNames) .
 */
#define md_ldap_default_group_member_attrib "member"

/*
 * IMPORANT NOTE:  with the introduction of certificate configuration
 * management (bug  11995) and LDAP CA certificate configuration (bug
 * 14661), the former md_ldap_tls_cacertdir option is no longer supported.
 * Certificate files for the LDAP CA certificates directory are now managed
 * under user configuration.  Further, while the md_ldap_tls_cacertfile
 * option is still supported, it is considered deprecated for most
 * situations.  Supplemental CA certificates may now be configured by the
 * user with the "crypto certificate" and "ldap ssl ca-certs" CLI commands.
 *
 * The md_ldap_tls_cacertfile build option is not affected by certificate
 * configuration, but rather defines a set of system internal default CA
 * certificates.  This option should only be used if for some reason the CA
 * certificates that ship with OpenSSL (see /etc/pki/tls/cert.pem) prove
 * insufficient.  It should not be used for end user certificates.
 *
 * Please also see the man page for SSL_CTX_load_verify_locations() for how
 * this setting relates to the supplemental certificates directory, which is
 * populated by the "ldap ssl ca-certs" configuration command.
 *
 * A full file path is required for this option to be honored.
 *
 * NOTE: it appears that if a path referencing a good certificate file is
 * specified for 'md_ldap_tls_cacertfile' that "/etc/pki/tls/cert.pem" will
 * also be checked after the specified file.  This cert.pem file is the
 * default OpenSSL CA certificate file.  However, if nothing is specified
 * for 'md_ldap_tls_cacertfile', or the specified file does not exist, or
 * there is not at least one valid cert in the specified file, then no
 * certificates (including those in /etc/pki/tls/cert.pem) are checked, and
 * certificate verification will fail.
 */

/* DEPR: #define md_ldap_tls_cacertfile "/etc/pki/tls/cert.pem" */


/******************************************************************************
 ******************************************************************************
 *** AAA tally-by-name feature: locking out users based on history of
 *** authentication failures.
 ******************************************************************************
 ******************************************************************************
 ***/

/* ............................................................................
 * These two fields control how we limit the number of records we will
 * accumulate in our database of authentication failures.
 *
 * The main goal with the limiting is to prevent runaway database
 * growth caused by authentication failures for bogus accounts (those
 * that exist neither locally nor on any configured authentication
 * server), when tracking of unknown accounts is enabled.  In this
 * case, any unauthenticated user can give any username and have it
 * added to our database.  Without some sort of limiting mechanism,
 * this could leave us vulnerable to DOS attacks by filling up the
 * media on which we store the database.
 *
 * These numbers are used as follows: every n authentication failures
 * (where n is aaa_tbn_max_user_records_check_freq), we will run a
 * check on the database contents, and delete as many unknown user
 * records as necessary to reduce the database to having x records
 * (where x is aaa_tbn_max_user_records).  Note that x is in terms
 * of a TOTAL number of records (including local accounts), though
 * only unknown accounts are eligible for trimming.  If there are at
 * least x local user records, no records will be deleted.  Therefore
 * note that the database could theoretically grow beyond these
 * bounds, but that is under the control of the system administrator,
 * who must explicitly create the local accounts.  We maintain one
 * record per unique username, not one per failed authentication
 * attempt.
 *
 * Since we only trim unknown user accounts at cleanup time, records
 * for local accounts cannot be pushed out by a flood of unknown
 * accounts.  However, note also that records of login failures for
 * true unknown accounts CAN be pushed out by a flood of bogus unknown
 * login attempts.  This is an unfortunate vulnerability of the tally
 * system.
 *
 * If these are not set, the defaults are:
 *   aaa_tbn_max_user_records_check_freq = 100
 *   aaa_tbn_max_user_records = 10000
 *
 * If these are set to zero, no automatic trimming of the database is
 * done, and it will be allowed to grow indefinitely (though the
 * administrator can always manually clear it out).  It is an error to
 * set one to zero and the other to a nonzero value.
 *
 * The defaults limit the size of the database file to somewhere
 * around 2.5 MB.  (Usernames and remote hostnames are truncated at 63
 * characters.)  If considering increasing this value, please also
 * consider possible performance issues when querying the list of user
 * records.  If your hardware is particularly slow, you may wish to
 * lower this below 10000.
 *
 * Note: we recommend against making aaa_tbn_max_user_records_check_freq
 * too large (i.e. making the checks too infrequent).  A significant part
 * of the cost of doing a cleanup is linear with the number of records to 
 * be deleted.  So making the cleanups infrequent means a given cleanup 
 * will take longer.  An exclusive db lock is held during cleanup,
 * which means that either (a) logins will be delayed, if the timeout
 * is long enough, or (b) logins will be allowed to go by us unchecked
 * and untracked, if the timeout is not long enough.  So it is in your
 * interests to keep cleanups pretty quick.
 */
/* #define aaa_tbn_max_user_records_check_freq  100   */
/* #define aaa_tbn_max_user_records             10000 */


/* ............................................................................
 * This allows you to override the amount of time the PAM tally module
 * will block on its own database being busy (presumably because it is
 * being simultaneously accessed by another version of the PAM module)
 * before giving up.
 *
 * Normally the database should only be busy for very brief periods of
 * time as we update it to reflect a new failure or success, and for
 * that a modest delay like 1 second should be more than enough.
 * However, our periodic trimming of the database (see comments above
 * aaa_tbn_max_user_records) might take longer.
 *
 * If we give up when running in 'check' mode in the 'authenticate'
 * phase, a locked user might be permitted to attempt authentication.
 * (Of course, they would still have to successfully authenticate to 
 * gain access to the system.)  If we give up when running in 'failure'
 * or 'success' modes (in the 'authentication' or 'account' phases,
 * respectively), the user's login failure or success would not be 
 * recorded.
 *
 * Note that because a login must go through two of these phases
 * ('check', and then either 'success' or 'failure'), the user may
 * end up having to wait up to 2x the timeout value set here.
 *
 * So tuning this number is a tradeoff between how long you are
 * willing to delay user logins, and how worried you are about someone
 * sidestepping the tally mechanism.  Another factor to consider is
 * how long it takes to do a periodic database trim, which depends on
 * the speed of your hardware, the numbers you have chosen for
 * aaa_tbn_max_user_records and aaa_tbn_max_user_records_check_freq
 * above, and the load on the system at the time.
 */
/* #define aaa_tbn_db_busy_timeout_ms 7500 */


/*
 * Originally, some components logged unrecognized usernames
 * (i.e. usernames which were not found in the local passwd file) on
 * failed login, at levels INFO or NOTICE.  This was considered a
 * potential security hole, since sometimes users accidentally type their
 * password at the username prompt.  See bug 14543 and bug 14599.  These
 * messages have been changed to no longer include the username if it is
 * not recognized; and the username is now logged separately at level
 * DEBUG.
 *
 * The #define below can be enabled to regain the original behavior.
 * This represents a tradeoff between having informative logs on one 
 * hand, and protecting the security of users attempting login (possibly
 * including admins on this system!) on the other.  You might use the 
 * #define if you value the former over the latter.
 *
 * Note: pam_tallybyname (the PAM module for the feature that can lock out
 * user accounts based on history of authentication failures) has a
 * user-configurable option whether to hash unknown usernames before
 * storing or logging them.  That feature is unaffected by this #define.
 * The components affected by the #define are not user-configurable in
 * this regard.
 */
/* #define aaa_log_unknown_usernames */


/******************************************************************************
 ******************************************************************************
 *** SSH
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * The set of encryption ciphers which will be proposed by SSH clients
 * and servers.
 *
 * At most one of the following should be #define'd to 1:
 *
 * - OPENSSH_DEFAULT_ENCRYPT_CLASSIC : cipher list the same as in
 *     Samara Eucalyptus Update 3 and earlier.
 *
 * - OPENSSH_DEFAULT_ENCRYPT_CTR_FIRST : similar to CLASSIC, but with
 *     the CTR ciphers first, so they will be preferred over the
 *     CBC-based and other ciphers.  This helps address CVE-2008-5161.
 *     This is the default.
 *
 * - OPENSSH_DEFAULT_ENCRYPT_CTR_ONLY : similar to CTR_FIRST, but only
 *     lists the CTR-based ciphers.
 *
 * - OPENSSH_DEFAULT_ENCRYPT_FASTER : prefer the faster ciphers, but
 *    still similar to CLASSIC .  This might be suitable for slower
 *    CPUs, at the (potential) cost of some security.
 *
 * - OPENSSH_DEFAULT_ENCRYPT_HIGHER : only higher security ciphers
 *     allowed, with CTR first.
 *
 * Defaults as if OPENSSH_DEFAULT_ENCRYPT_CTR_FIRST is set.
 *
 */
#define OPENSSH_DEFAULT_ENCRYPT_CTR_FIRST 1


/*
 * The next block of this file is not needed by components requiring
 * redistribution of source code.
 */
#ifndef CUSTOMER_H_REDIST_ONLY

/******************************************************************************
 ******************************************************************************
 *** Front Panel Daemon
 ******************************************************************************
 ******************************************************************************
 ***/

/* Juniper changed the following. */
/* This would be used if we activate the front panel driver */
#define fp_boot_banner \
    D_("   Juniper\n    Networks", CUSTOMER_INCLUDE_GETTEXT_DOMAIN)
/* Juniper changed the preceeding. */


/******************************************************************************
 ******************************************************************************
 *** Image installation
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * If you are going to operate a server that allows checking for 
 * image updates, using tools/image-check.pl or another script that
 * follows the same protocol, define this string to be the base URL
 * to the script.  The /image/query_updates action will use this as
 * its default, and append any query parameters needed.
 *
 * NOTE: if this is not defined, or defined to the empty string, your
 * users will not be able to use the image update check feature by
 * default, and it will not show up in the UI.  Users can set it later
 * with a hidden command, though.
 *
 * NOTE: this string is copied into the config db whenever a fresh one
 * is created.  So if you change this and want it to be reflected in
 * config databases of deployed boxes, you'll also need to write an
 * upgrade function to change it.  The node to set is
 * /image/config/update_server.
 */
#define md_image_query_url_default ""

/*
 * If defined, this filename will be used by default for any image
 * downloaded via the CLI if no other filename is specified.  If left
 * undefined, the "image fetch" CLI command will have its original
 * behavior, which is to preserve the original name.  In either case,
 * the user can specify a name to use; and if this is defined, the
 * user can also explicitly request that the original name be
 * preserved anyway.
 */
/* #define cli_default_image_filename "cli_image.img" */

/*
 * If PROD_FEATURE_IMAGE_SECURITY is enabled, by default only signed
 * images can be installed, used for remanufacture, or used for a
 * virtualization manufacture ISO.  At the cost of decreasing security
 * (especially if PROD_FEATURE_RESTRICT_CMDS is enabled), here are some
 * notes on overriding this behavior.
 *
 *  - For customer not using PROD_FEATURE_RESTRICT_CMDS, to allow unsigned
 *    images to be installed:
 *
 *     #define IMAGE_SIGNATURE_VERIFY_NORESTRICT_OVERRIDE 1
 *
 *  - For customers using PROD_FEATURE_RESTRICT_CMDS, to allow unsigned
 *    images to be installed when a RESTRICTED_CMDS license is NOT
 *    active:
 *
 *    #define IMAGE_SIGNATURE_VERIFY_RESTRICT_UNLICENSED_OVERRIDE 1
 *
 *    This might be required if end users need to install an (older)
 *    unsigned images during a transition period.  This is not
 *    recommended, as it effectively allows PROD_FEATURE_RESTRICT_CMDS
 *    to be circumvented.
 *
 *  - For customers using PROD_FEATURE_RESTRICT_CMDS, to allow unsigned
 *    images to be installed when a RESTRICTED_CMDS license IS active:
 *
 *    #define IMAGE_SIGNATURE_VERIFY_RESTRICT_LICENSED_OVERRIDE 1
 *
 *    This might be required to allow a field support engineer or
 *    developer to install an (older) unsigned image after enabling a
 *    valid RESTRICTED_CMDS license.  This is the only of the three
 *    settings that might want to be enabled longer term.
 *
 * Defining any of these three may result in a decrease in security, and
 * so are only recommended if they are required.  Note that it is no
 * longer possible to install ill-signed images (as opposed to unsigned
 * or correctly signed images) when PROD_FEATURE_IMAGE_SECURITY is
 * enabled.
 *
 * See also demo customer's src/release/Makefile.inc for image signing
 * settings.
 *
 */


/******************************************************************************
 ******************************************************************************
 *** Configuration database options
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * If you enable PROD_FEATURE_SECURE_NODES, literal config nodes
 * registered with the mrf_flags_reg_config_literal_secure flag will
 * have their values encrypted when being written to persistent
 * storage.  AES-128 encryption is used.  The key is specified here,
 * and it must be exactly 16 characters (i.e. 128 bits).  Technically
 * a key should be allowed to contain zero bytes, but we use a string
 * here for convenience, with a slight loss of entropy.
 *
 * NOTE: there is no key management for encryption of config nodes at
 * this time, so changing the key will make all previously-encrypted
 * values unreadable.
 */

/* Juniper changed the following. */
#define md_config_crypt_key_str "#nkn1NTERNAL#101"
/* Juniper changed the preceeding. */


/*
 * This affects how we construct /mgmtd/notify/dbchange/as_saved events.
 * Currently the only base Samara component who registers for these events
 * is the Clustering daemon, so if you are not using the separately 
 * licenseable Clustering/HA product, these events will not be used, and
 * this setting will have no impact.
 *
 * The impact of this setting, if the .../dbchange/as_saved events are 
 * in use, is to control when the ::mrf_change_no_notify reg flag is
 * honored when constructing those events.  If the change list which we
 * are rendering as an event has fewer than this number of change records,
 * then the ::mrf_change_no_notify flag will not be honored.
 *
 * If this value is not defined, it will default to 250.  Note that 
 * regardless of the value assigned here, the reg flag will always be
 * honored in the other variants of the dbchange event.  It is only the
 * "as_saved" one which is affected.
 *
 * NOTE: if you are using clustering, this number should be greater
 * than or equal to the value used for CL_CONFIG_SYNC_THRESHOLD_COUNT.
 * If this number is less than the clustering number, it will be unable
 * to do incrementally sync for changes of certain sizes, since the
 * change-no-notify nodes will be compacted.
 */
/* #define md_changes_no_notify_min_changes_threshold 250 */


/******************************************************************************
 ******************************************************************************
 *** Memory tuning options
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * Some Samara components, like mgmtd and the cli, do many small
 * malloc()/free() pairs during normal execution.  This can cause the
 * processes in question to stay large, even though there are not memory
 * leaks.  This is the result of GLIBC's malloc() implementation holding
 * onto free'd blocks, so it can quickly reuse them.  However, it does
 * mean that these processes can stay larger than necessary, and under
 * memory pressure, which can be undesirable.
 *
 * LC_MTRIM_STD_RETAIN : The amount of memory, in bytes, to attempt to
 * retain during an automatic malloc trim operation.  Set to "(0)" to
 * try to trim as much as possible.  Set to "(-1)" it disable all
 * automatic trimming (the historical behavior).
 *
 * LC_MTRIM_STD_TIMER_INTERVAL_MS : For some Samara daemons (such as
 * mgmtd), the timer interval for automatically running an automatic
 * malloc trim operation.  Set to "(-1)" to disable all timer-based
 * automatic trimming (the historical behavior).
 */

#define LC_MTRIM_STD_FREE_RETAIN (4 * 1024 * 1024)
#define LC_MTRIM_STD_TIMER_INTERVAL_MS (5 * 1000)

/******************************************************************************
 ******************************************************************************
 *** System lockdown options
 ***
 *** If PROD_FEATURE_RESTRICT_CMDS is enabled, one of the ways the
 *** system is locked down is by disallowing access to certain files 
 *** via scp or sftp, even for uid 0.
 ***
 *** If you have enabled that feature, here you must choose what rules
 *** will be applied.  You can choose one of three pre-defined rule sets,
 *** all of which are customizable using graft points in 
 *** src/base_os/common/openssh/src/tms-ssh-utils.c.
 *** Or, you can choose to have no restrictions on the files that can
 *** be transferred with scp or sftp.
 ***
 *** Your choices are:
 ***
 ***    1. #define ssh_restrict_rules tms_access_rules_whitelist
 ***       Customize using tms-ssh-utils.c graft point #1.
 ***
 ***       These rules use a "whitelist" approach, where the default
 ***       action is to deny access, unless the file path is
 ***       explicitly listed as OK in one of the rules.
 ***
 ***    2. #define ssh_restrict_rules tms_access_rules_blacklist
 ***       Customize using tms-ssh-utils.c graft point #2.
 ***
 ***       These rules use a "blacklist" approach, where the default
 ***       action is to permit access, unless the file path is 
 ***       explicitly listed as inaccessible in one of the rules.
 ***
 ***    3. #define ssh_restrict_rules tms_access_rules_custom
 ***       Define your own rules using tms-ssh-utils.c graft point #3.
 ***
 ***       This is an empty rule set, permitting fully customized rules.
 ***       Note that you MUST define at least one rule, to specify whether
 ***       the default action should be accept or deny, since if a rule set
 ***       does not make a decision about a given path, a warning will be
 ***       logged.
 ***
 ***       IMPORTANT NOTE: if you select this option, please carefully
 ***       review the rules in our whitelist and blacklist rule sets,
 ***       for suggestions on what rules to use.  If you are going to
 ***       permit access to anything these rule sets disallow, or if 
 ***       you have added any other binaries which run as root or can
 ***       affect the behavior of something running as root, consider
 ***       what might happen if an attacker replaced one of these files
 ***       with one modified to their liking.  It may be unconventional 
 ***       to think of the admin of a system as an attacker, but that is
 ***       essentially what they are in this context.
 ***
 ***       Do not allow access to /var/opt/tms/output, because there
 ***       are ways a user could bypass our security if they could
 ***       write to it, e.g. setting the restricted_cmds_license
 ***       symlink to point to "1".
 ***
 ***    4. #define ssh_restrict_rules_NONE
 ***
 ***       Bypass all of our checks in scp and sftp-server, and do not
 ***       impose any further restrictions on files that can be transferred
 ***       over an ssh connection.  Of course, filesystem permissions will
 ***       still apply as always.  This just allows you to "opt out" of 
 ***       the entire scp/sftp limitation feature.
 ***
 ***       IMPORTANT NOTE: we recommend against doing this, as it will 
 ***       severely weaken the protection provided by the
 ***       PROD_FEATURE_RESTRICT_CMDS feature.  The admin user will be
 ***       able to circumvent the restrictions, e.g. by using scp or sftp
 ***       to upload an alternate version of /etc/shadow which permits 
 ***       root login.
 ******************************************************************************
 ******************************************************************************
 ***/

#ifdef PROD_FEATURE_RESTRICT_CMDS

/* Juniper changed the following. */
#define ssh_restrict_rules tms_access_rules_blacklist
/* Juniper changed the preceeding. */
/* Juniper added the following. */
/* The following two IMAGE_SIGNATURE*IGNORE defines make the image
** installation system not check for signatures in the image file.
** See $PROD_TREE_ROOT/src/bin/mgmtd/md_utils_internal.c
*/
#define IMAGE_SIGNATURE_VERIFY_RESTRICT_LICENSED_OVERRIDE_IGNORE
#define IMAGE_SIGNATURE_VERIFY_RESTRICT_UNLICENSED_OVERRIDE_IGNORE
/* Juniper added the preceeding. */

#endif /* PROD_FEATURE_RESTRICT_CMDS */


/******************************************************************************
 ******************************************************************************
 *** Stats tuning
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * All of the settings in this section are optional, and can be ignored
 * unless you have some specific reason to think they will be helpful.
 * Please see the 'demo' product's customer.h for explanations and 
 * examples of setting them.
 */


/******************************************************************************
 ******************************************************************************
 *** System Disk I/O Monitoring and Disk Statistics
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * These settings may be used to limit the scope of the mgmtd /system/disks/
 * iterate function.  By default, the disk iterate function iterates over all
 * disks visible to the system that have I/O activiity.
 *
 * Filtering /system/disks iteration may useful on systems that host a very
 * large number of disk devices, where disk I/O monitoring beyond core system
 * management disks is cost prohibitive, or where there is a desire to exclude
 * the appliance's user data disks from the system view.
 *
 * Devices that are excluded from the iteration view may still be queried
 * directly using a mgmtd get request, and the nodes under the device may be
 * iterated over with a request that specifies the device, assuming the caller
 * knows of the device's existence.  They are simply excluded from from
 * iteration at the /system/disks/{device_name} wildcard level.
 * 
 * Filtering mgmtd /system/disks iteration indirectly limits which disk devices
 * are included in included in the disk_device_io_hour statistics collection,
 * since chd depends on the /systems/disks iteration view.
 */

/*
 * By default, we filter out all devices that are not part of the main system
 * layout for the product manufactured, as defined in customer_rootflop.sh.
 * With this flag, the base set is changed to be unfiltered, i.e. all disks
 * devices visible to the mgmtd are included by default, and other filters are
 * applied if defined.
 */

/* #define MD_SYSTEM_DISKS_BASE_INCLUDE_ALL_VISIBLE */

/*
 * Exclude disk partition devices.  By default, partition devices (e.g. sda1,
 * sda2, ..., hda1, ...) for all included disk devices (e.g. sda), are included
 * in the system view.  Defining this flag excludes all disk partition devices
 * leaving only parent devices (e.g. sda, hda).
 *
 * Note that the Web UI graph for DISK I/O currently excludes disk partitions,
 * so this option only affects stats collection of I/O data for partitions.
 */

/* #define MD_SYSTEM_DISKS_EXCLUDE_PARTITIONS */

/* Exclude loopback devices (/dev/loop*) from view */
/* #define MD_SYSTEM_DISKS_EXCLUDE_LOOPBACK_DEVICES */

/* Exclude RAM Disk devices (/dev/ram*) from view */

/* #define MD_SYSTEM_DISKS_EXCLUDE_RAM_DISK_DEVICES */

/* Exclude Multiple Disk devices (/dev/md*) from view (e.g. RAID) */

/* #define MD_SYSTEM_DISKS_EXCLUDE_MD_DISK_DEVICES */

/*
 * By default, disk devices that show no read or write activity since the
 * system was booted are excluded from the view.  Enabling this flag will
 * expose all devices visible to the system that are not excluded by other
 * filters, regardless of whether they show any activity.  This might be useful
 * if it is important to have an explicit historical record of non-activity of
 * devices.  Note that if there are no other filters, this might cause special
 * disk pseudo-devices such as /dev/ram*, /dev/loop*, /dev/md*, etc., to become
 * exposed, which could be undesirable if there are is no real implementation
 * behind those devices.
 */
/* #define MD_SYSTEM_DISKS_INCLUDE_INACTIVE_DEVICES */


/******************************************************************************
 ******************************************************************************
 *** IPsec
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * If PROD_FEATURE_CRYPTO is enabled, there are two possible IKE daemons to
 * choose from, racoon (KAME) and pluto (Openswan).  Racoon only supports
 * IKEv1, and has traditionally been used on the platform.  Openswan is newer
 * to the platform, and has some additional features, as documented in the
 * cli-commands.txt and nodes.txt files.  It is also expected to support
 * IKEv2 in the future (presently a hidden feature).
 *
 * You should specify in your customer.mk file which daemon(s) you would like
 * to support, otherwise you will inherit internal build defaults defined in
 * common.mk (presently both daemons).
 *
 * If you wish to exclude one of the daemons in your product, or if you wish
 * to ensure that inclusion / exclusion is defined explicitly, add the
 * following flags in your customer.mk file, setting each to either 0
 * (exclude) or 1 (include):
 *
 * CRYPTO_INCLUDE_RACOON
 * CRYPTO_INCLUDE_OPENSWAN
 *
 * You may not exclude both.
 * 
 * For example, if you wish to build only racoon, you would set:
 *
 * CRYPTO_INCLUDE_RACOON=1
 * CRYPTO_INCLUDE_OPENSWAN=0
 *
 * If you do exclude one, be sure that it is not defined as the default
 * daemon (below).
 *
 * When both daemons are built, racoon presently remains the daemon that
 * runs in the factory default configuration (see CRYPTO_DAEMON_DEFAULT_*
 * include flag choices below).  With both daemons in the product, the
 * hidden cli command "crypto ipsec daemon" may be used to switch from one
 * daemon to the other.
 *
 * Note that due to certain feature limitations of both daemons, any
 * unsupported attributes in peering configuration will either be ignored or
 * alternate attribute settings will be applied, and warnings will be issued
 * in the system logs.  While affected peerings may continue to interoperate
 * if the peer is amenable to alternate proposals, be aware that the
 * negotiated outcome may differ from the values residing in configuration.
 * See the latest release notes for details.
 */

/*
 * The crypto module presenly supports two crypto IKE/ISAKMP daemons, KAME's
 * racoon, and Openswan's pluto.  One of these flags may be used to choose
 * the preferred daemon for factory default configuration.  That choice may
 * be modified by the user at run time, but only through the hidden CLI
 * command "crypto ipsec daemon".  While racoon is presently the factory
 * default daemon, this default may change in a future release or overlay
 * during the crypto daemon transition period.  Customers are encouraged to
 * try Openswan/pluto as the default.  It has some advantages over racoon,
 * including automatic bring-up of connections prior to traffic demand, and
 * additional IKE monitoring nodes.  Regardless of your preference, in order
 * to avoid inheriting the release's build time default daemon, you should
 * uncomment one of these two build flags.  Also, be sure that you are not
 * excluding the default daemon from the customer.mk file (described above).
 */
/* #define CRYPTO_DAEMON_DEFAULT_RACOON  */
/* #define CRYPTO_DAEMON_DEFAULT_PLUTO   */


/******************************************************************************
 ******************************************************************************
 *** Iptables
 ******************************************************************************
 ******************************************************************************
 ***/


/*
 * By default, the base product starts with three rules in iptables
 * configuration:
 *   - Permit all incoming ICMP traffic.
 *   - Permit all traffic on the loopback interface.
 *   - Permit all traffic whose state is ESTABLISHED or RELATED (to an
 *     existing connection).
 *
 * Iptables overall is disabled by default, but if the user enables
 * it, these are the rules they would get.  You can override these
 * initial rules from a mgmtd module, but as a shortcut, uncomment the
 * #define below to prevent these from being used as defaults.  If the
 * constant below is defined, no rules will be added by default.
 */

/* #define MD_IPTABLES_NO_BASE_DEFAULTS */


/******************************************************************************
 ******************************************************************************
 *** Clustering
 ******************************************************************************
 ******************************************************************************
 ***/

#ifdef PROD_FEATURE_CLUSTER

/*
 * Binding name (like "/foo/bar") to query periodically for application
 * health.  If the string form of the binding value is "false" (or
 * cannot be retrieved), the system will leave the cluster.
 */

#define CL_APPL_KA_BINDING_NAME "/system/hostname"
/*
 * Binding name (like "/foo/bar") to query.  Unless the string form of
 * the binding value is "false" , we will join a cluster master if we
 * find one.  Only checked when searching for a cluster to join or
 * become master of.
 *
 * Can be left empty to not check any binding before joining.
 *
 * Can be set to "=false" to never join a cluster.
 */

/* Juniper changed the following. */
#define CL_OK_TO_JOIN_BINDING_NAME "/system/hostname"
/* Juniper changed the preceeding. */
/*
 * Binding name (like "/foo/bar") to query.  Unless the string form of
 * the binding value is "false" , we will become master if no master is
 * found.  Only checked when searching for a cluster to join or become
 * master of.
 *
 * Can be left empty to not check any binding before becoming master.
 *
 * Can be set to "=false" to never become master (via searching, could
 * still become master via standby switchover).
 */
#define CL_OK_TO_BECOME_MASTER_BINDING_NAME ""

/*
 * Define this to enable incremental config syncs (subject to the 
 * limitations defined in the other two constants below).  By default,
 * incremental config sync is off.
 */
/* #define CL_CONFIG_SYNC_INCREMENTAL true */

/*
 * Configuration changes which have fewer than this number of bindings
 * may be sent incrementally, instead of sending the entire master
 * config db.  Defaults to 250.
 *
 * See also md_changes_no_notify_min_changes_threshold, which needs to
 * be set the same or greater than this setting.
 */
#define CL_CONFIG_SYNC_THRESHOLD_COUNT 250

/*
 * Maximum number of simultaneous pending incremental config syncs to
 * try to honor.  Above this, the pending incremental config syncs are
 * all converted to a single full config sync.  Defaults to 4.
 */
#define CL_CFG_SYNC_MAX_PENDING_INCREMENTAL 4

#endif /* PROD_FEATURE_CLUSTER */


/******************************************************************************
 ******************************************************************************
 *** Central Management Console (CMC)
 ******************************************************************************
 ******************************************************************************
 ***/

#ifdef PROD_FEATURE_CMC_SERVER

/*
 * This is a user-visible string, to be shown in the UI, that
 * describes all system software versions prior to the one based on
 * Dogwood.  What is it used for?  When rbmd connects to an appliance,
 * the appliance is supposed to include version info in its greeting.
 * But pre-Dogwood versions will not.  So rbmd doesn't find this info,
 * all it knows about the appliance's version is that it is pre-Dogwood.
 *
 * A customer should really change this to something appropriate to their
 * own versioning namespace.
 */
#define cmc_pre_dogwood_version_string "pre-Dogwood"

#endif /* PROD_FEATURE_CMC_SERVER */


/* ............................................................................
 * RGP settings.  This can apply whether on the CMC server, CMC client, or
 * if RGP is being used standalone.
 */
#if defined(PROD_FEATURE_CMC_SERVER) || defined(PROD_FEATURE_CMC_CLIENT) || defined(PROD_FEATURE_REMOTE_GCL)

/*
 * What will RGP assume about the other endpoint's IPv6 capability if
 * it does not receive the /rgp/actions/identify_self action?
 *
 * If the other side is not IPv6-capable, RGP will filter all outgoing
 * messages, to transcode any attributes with new data types into
 * bt_binary, to avoid having the other endpoint break the connection.
 * (See bug 13969)
 *
 * The /rgp/actions/identify_self action, sent by the CMC server as soon as
 * the session is brought up, is normally how RGP would know about the 
 * IPv6 capabilities of the other end.  If it contains the 'ipv6_capable'
 * binding set to 'true', RGP assumes the other side is IPv6-capable;
 * if it lacks the binding, or if it is present and set to 'false', RGP
 * assumes the other side is not IPv6-capable.
 *
 * However, if RGP is being used outside the context of the CMC, and if
 * this action is not sent at all, RGP has no data on which to base an
 * assumption.  This #define lets you control its behavior under those
 * circumstances.
 *
 * If this is not defined, or if you define it to 'true', RGP will
 * assume the other endpoint is IPv6-capable.  If this is defined to
 * 'false', then RGP will assume the other endpoint is NOT IPv6-capable.
 */
/* #define rgp_default_ipv6_capable true */

#endif /* PROD_FEATURE_CMC_SERVER || PROD_FEATURE_CMC_CLIENT || PROD_FEATURE_REMOTE_GCL */


/******************************************************************************
 ******************************************************************************
 *** XML Gateway
 ******************************************************************************
 ******************************************************************************
 ***/

/* Note: also see 'cli_exec_allow' comments above about usage over SSH */

#ifdef PROD_FEATURE_XML_GW

/*
 * XML Gateway with Capabilities (PROD_FEATURE_CAPABS not PROD_FEATURE_ACLS)
 * ==================================================
 *
 * If XG_RESTRICTED_CAP is not set, only "admin" users will be able to
 * use the XML gateway, but they will have no restrictions, and will be
 * able to operate on all nodes.  This means as much access as using the
 * "internal" CLI commands.
 *
 * If XG_RESTRICTED_CAP is set, it will mean that only "admin" (uid 0)
 * users, or (if XG_RESTRICTED_NONADMIN_ALLOWED is also set) users with
 * the given capability will be able to use the XML gateway.  All users'
 * access to mgmtd nodes will then be done with this defined capability,
 * even for "admin" users.  If customers then mark mgmtd nodes with the
 * corresponding capability if they should be exposed to the XML
 * gateway.  If XG_RESTRICTED_CAP is set, non-"admin" users can use the
 * XML gateway, if additionally XG_RESTRICTED_NONADMIN_ALLOWED is set.
 * It is not supported to have non-"admin" users except when
 * XG_RESTRICTED_CAP is set.
 *
 * WARNING: It is critical that the chosen capability be listed in the
 * customer's src/bin/mgmtd/md_mod_reg.inc.h in the gid to cap mapping
 * (graft point 11), with the third parameter
 * (mgcm_enforce_query_capability) set to "true".  If it is not listed,
 * or not set to "true", and you set XG_RESTRICTED_CAP, there will be no
 * access restrictions enforced, and all nodes will be exposed, as if
 * XG_RESTRICTED_CAP had not been set.
 *
 * Note that after honoring the above restrictions and mappings, if
 * PROD_FEATURE_RESTRICT_CMDS is on, the XML gateway will not allow any
 * users access unless there is an active RESTRICTED_CMDS license.
 *
 * The above restrictions and mappings all may be avoided for a special
 * unrestricted and always authorized user, by defining
 * XG_UNRESTRICTED_USER_CAP and optionally XG_UNRESTRICTED_USER_UID .
 * This may be useful if XML gateway is being used in some internal
 * programmatic way.  XG_UNRESTRICTED_USER_CAP if set allows a user with
 * the given capability to be granted access without mapping, without
 * needing to potentially be "admin", and even though there is no active
 * RESTRICT_CMDS license.  If XG_UNRESTRICTED_USER_UID is set to a uid,
 * the user must have the given uid to be considered unrestricted.
 * Either, but not both, of XG_UNRESTRICTED_USER_CAP or
 * XG_UNRESTRICTED_USER_UID may be set to -1 to match all capabilities
 * or uid's respectively.  The only limitation which applies to such
 * unrestricted users is if the XML gateway is disabled by
 * configuration.  The comments above regarding
 * mgcm_enforce_query_capability are also critical for the chosen
 * capability.
 *
 * If you are using PROD_FEATURE_RESTRICT_CMDS with
 * XG_UNRESTRICTED_USER_CAP to allow programmatic access to the XML
 * Gateway, make sure there is a mechanism in place to prevent the
 * admin from assigning that capability level to a user account under
 * his control, which would defeat the purpose of the protection.
 * Generally the user account used for this would be hidden and a
 * "system" account; and would be logged into using a private key kept
 * somewhere in the filesystem to which the user did not have access
 * (including using the scp/sftp transfer limitations described under
 * "System lockdown options" in this file).
 *
 *
 * XML Gateway with ACLs (PROD_FEATURE_ACLS not PROD_FEATURE_CAPABS)
 * ==================================================
 *
 * XXXX/RBAC: the XG_RESTRICTED_* options are not currently defined for
 * ACLs, and may not be used.  Only users with uid 0 may currently use
 * XML gateway with ACLs, and there is no authorization mapping.  In the
 * future there will be some ACL-related means to restrict access.
 *
 * XG_UNRESTRICTED_USER_UID and XG_UNRESTRICTED_USER_CAP (which is a
 * gid) may be used as described above to allow unrestricted, always
 * authorized access to the XML gateway.
 *
 */

#ifdef XG_INTERNAL_INC_1

#ifdef PROD_FEATURE_CAPABS

/* define XG_RESTRICTED_CAP mgt_myxgcaps */
/* define XG_RESTRICTED_NONADMIN_ALLOWED 1 */

/* define XG_UNRESTRICTED_USER_CAP mgt_myspecialcaps */
/* define XG_UNRESTRICTED_USER_UID myspecialuid */

#else /* PROD_FEATURE_CAPABS */

/* define XG_UNRESTRICTED_USER_CAP -1 */
/* define XG_UNRESTRICTED_USER_UID myspecialuid */

#endif /* !PROD_FEATURE_CAPABS (PROD_FEATURE_ACLS) */

#endif /* XG_INTERNAL_INC_1 */

/*
 * The default format in requests for values of type "binary".
 * Historically, this has behaved as if set to "xf_string", which does
 * not allow all binary values to be represented.
 *
 * It is recommended to use "xf_base64" instead, as that is the default
 * output format.  The current default is "xf_string", for compatability
 * with any deployed usage.
 */
/* define XG_REQUEST_DEFAULT_BINARY_FORMAT xf_string */

#endif /* PROD_FEATURE_XML_GW */


/******************************************************************************
 ******************************************************************************
 *** Virtualization
 ******************************************************************************
 ******************************************************************************
 ***/

/*
 * Normally, if we are unable to fetch disk stats for a running VM.
 * we log at WARNING.  But if this is expected for you (e.g. you are
 * using a different version of libvirt/qemu/kvm which does not work
 * properly), you may set this to lower the log level of those complaints.
 */

/* #define VIRT_LOG_LEVEL_DISK_STATS_FAILURE LOG_INFO */

/*
 * If PROD_FEATURE_VIRT_LICENSED is enabled, virtualization will be
 * available if and only if certain licensing conditions are met.
 * Those conditions are determined by the definition below.  There are
 * two possible outcomes:
 *
 *   1. The user must have an active VIRT license.  This is the default,
 *      if the constant below is not defined.
 *
 *   2. The user must have an active license with the feature name
 *      specified by VIRT_LICENSE_FEATURE_NAME.  This is assumed to
 *      be a more general license, so additionally, the "virt" 
 *      informational option tag on this feature must be present and
 *      set to 'true'.
 *
 * Note that there are other informational options which further 
 * restrict the use of the virtualization feature.  e.g. "virt_max_vms"
 * limits the maximum number of VMs that can be running at a time.
 * These are the same regardless of whether the below is defined.
 * The "virt" option is the only one that applies only to the extra
 * feature; it can be considered implicitly set to 'true' for any active
 * VIRT license.
 */

/* #define VIRT_LICENSE_FEATURE_NAME "TEST" */


/*
 * Virtual volumes are stored in "storage pools", which are like
 * directories.  By default, a Samara system has one storage pool
 * named "default", in which all volumes live.  Even if you add more
 * storage pools, we still need to have a default storage pool, to be
 * used if the user does not specify a particular pool.  If you want
 * the default storage pool to be something other than the one named
 * "default", define it here.
 *
 * NOTE: the pool you name here must be a pool which will exist in the
 * system.  The set of pools available is determined from the mfdb:
 * see /mfg/mfdb/virt/pools/pool in mfdb-nodes.txt.
 */

/* #define VIRT_DEFAULT_POOL "default" */


/*
 * Normally during shutdown, tvirtd will start saving VM state and
 * shutting down VMs as soon as it gets PM's shutdown event.  This is
 * an optimization to speed up overall system shutdown, since
 * sometimes it may take a while (if we are saving the state of
 * running VMs), and we want to parallelize it with other shutdown
 * tasks.  It may be at least a few seconds before tvirtd gets a
 * SIGTERM, since it has a relatively high shutdown order.
 *
 * However, if you want to do any virtualization-related shutdown
 * tasks of your own, you may want tvirtd to wait until it gets its
 * SIGTERM.  If so, enable the #define below.
 */

/* #define VIRT_DELAY_SHUTDOWN */


/*
 * The QEMU/KVM/Libvirt in the Samara base platform (from CentOS 5.x)
 * does not support the SCSI bus in virtual machines.  But if you are
 * running a custom QEMU/KVM/Libvirt which does support SCSI, you may
 * enable this #define, to offer the SCSI bus.
 *
 * If this is enabled, Samara will permit devices to be mounted on the
 * SCSI bus, but it will not permit devices of a given device type
 * (disk or cdrom) to be mounted on both the IDE and SCSI buses
 * at the same time on the same VM.  e.g. a VM might have a SCSI disk
 * and an IDE CD-ROM, but it could not have both SCSI and IDE disks.
 *
 * IMPORTANT NOTE: once you have enabled this, subsequently disabling
 * it risks breaking any systems which ran with the software where it
 * was enabled.  Their configuration files may have devices mounted on
 * the SCSI bus, which is not permitted when this is disabled.  This
 * could prevent you from switching to those config files, or in the
 * worst case, if such a config file was the active one, it could fail
 * the initial commit.
 *
 * ALSO NOTE: if you are using a custom QEMU/KVM/Libvirt, you may need
 * to override the path to the emulator, below.
 */

/* #define VIRT_SCSI_SUPPORT */


/*
 * If you need to override the emulator used for virtual machines,
 * define it here.  The default is defined in tpaths.h as QEMU_KVM_PATH.
 * The sample path shown here may not be correct -- be sure to verify 
 * it for your case.
 */

/* #define VIRT_QEMU_KVM_PATH "/usr/bin/qemu-system-x86_64" */


/*
 * When a new VM or Vnet is created, Samara automatically chooses a 
 * UUID for it which is a hash of the VM name and host ID (and then
 * made to conform to the UUID standard in RFC 4122).
 *
 * By default, this UUID is enforced, and the administrator will not
 * be able to change it.  This is partially to enable licenses for use
 * on VMs to be effectively tied to the machine's UUID.  If you do not
 * want the enforced, i.e. you want to allow users to set the UUID of
 * their VMs and Vnets, define the constant below.  Note that if you
 * do this, licenses using the tied_uuid activation option on a VM
 * will not be very effective, since the user could then change the
 * UUID of the VM to match the license.
 */

/* #define VIRT_NO_ENFORCE_UUID */


/* Juniper added the following. */
/******************************************************************************
 ******************************************************************************
 *** Custom path defines.
 ******************************************************************************
 ******************************************************************************
 ***/
#define ACCESSLOG_DIR_PATH  "/var/log/nkn"
static const char accesslog_dir_path[] = ACCESSLOG_DIR_PATH;
#define OBJECTLIST_DIR_PATH  "/nkn/ns_objects"
static const char objectlist_dir_path[] = OBJECTLIST_DIR_PATH;
#define FMSFILES_DIR_PATH  "/nkn/adobe/downloads"
static const char fmsfiles_dir_path[] = FMSFILES_DIR_PATH;
#define FMSLOG_DIR_PATH  "/nkn/adobe/fms/logs"
static const char fmslog_dir_path[] = FMSLOG_DIR_PATH;
#define GEO_MAXMIND_DIR_PATH  "/nkn/maxmind/downloads"
static const char geo_maxmind_dir_path[] = GEO_MAXMIND_DIR_PATH;
#define GEO_QUOVA_DIR_PATH  "/nkn/quova/downloads"
static const char geo_quova_dir_path[] = GEO_QUOVA_DIR_PATH;

/* hack: should actually get MD_GEN_OUTPUT_PATH from tpaths.h */
#define SSL_LICENSE_NAME    "ssl_license"
static const char file_ssl_cmds_license_path[] = "/var/opt/tms/output" "/" SSL_LICENSE_NAME;

#define LOG_EXPORT_CHROOT	    "/log/logexport/chroot"
#define LOG_EXPORT_HOME		    LOG_EXPORT_CHROOT "/home/LogExport"


/*
 * Define this copyright header to get printed when user does a "show version".
 * This is also printed at the bottom of all GUI pages.
 * NOTE: When the Copyright Year Needs Changing, two files need updating.
 * These are:
 *   include/customer.h
 *   bin/web/templates/customer-defines.tem
 * See notes about "Copyright Year" changes in mk/customer.mk.
 */
static const char nokeena_copyright[] = "Copyright (c) 2008-2015 by Juniper Networks, Inc\n\n";
/* Juniper added the preceeding. */
/******************************************************************************
 ******************************************************************************
 *** GRAFT point defines
 *** In C files that have graft points, we don't want to require the
 *** existence of a .inc.c file, if it is not needed. This is where
 *** #defines should be set that will cause the .inc.c files to be
 *** included if they are necessary.
 ******************************************************************************
 ******************************************************************************
 ***/

#undef INC_CLI_MOD_INC_GRAFT_POINT
/* Juniper changed the following. */
#define INC_CLI_MOD_HEADER_INC_GRAFT_POINT 1
/* Juniper changed the preceeding. */
#undef INC_CLI_CLUSTER_CMDS_INC_GRAFT_POINT
/* Juniper changed the following. */
#define INC_CLI_CONFIG_INC_GRAFT_POINT 1
/* Juniper changed the preceeding. */
#undef INC_CLI_DIAG_INC_GRAFT_POINT 
#undef INC_CLI_EMAIL_INC_GRAFT_POINT 
#undef INC_CLI_INTERFACE_CMDS_INC_GRAFT_POINT
#undef INC_CLI_PM_INC_GRAFT_POINT 
/* Juniper changed the following. */
#define INC_CLI_STATS_CMDS_INC_GRAFT_POINT 1
#define INC_CLI_FILE_UTILS_INC_GRAFT_POINT 1
#define INC_CLI_MODULE_INC_GRAFT_POINT 
#define INC_CLI_UTILS_INC_GRAFT_POINT
/* Juniper changed the preceeding. */
#undef INC_CLIMOD_COMMON_HEADER_INC_GRAFT_POINT
#undef INC_CLISH_MAIN_INC_GRAFT_POINT
#undef INC_CLI_PRIVMODE_CMDS_INC_GRAFT_POINT 
#undef INC_CLI_LOGGING_CMDS_INC_GRAFT_POINT 
/* Juniper changed the following. */
#define INC_CLI_RESTRICT_CMDS_INC_GRAFT_POINT 1
/* Juniper changed the preceeding. */

#undef INC_MD_NET_UTILS_INC_GRAFT_POINT
/* Juniper changed the following. */
#define INC_MD_MOD_INC_GRAFT_POINT 1
/* Juniper changed the preceeding. */
#undef INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT
#undef INC_MD_MOD_REG_INC_GRAFT_POINT
/* Juniper changed the following. */
#define INC_MD_CLIENT_INC_GRAFT_POINT 1
/* Juniper changed the preceeding. */
#undef INC_MD_CMC_SERVER_INC_GRAFT_POINT
#undef INC_MD_LOGGING_INC_GRAFT_POINT
/* Juniper changed the following. */
#define INC_MD_PASSWD_INC_GRAFT_POINT 1
#define INC_MD_IPTABLES_INC_GRAFT_POINT 1
/* Juniper changed the preceeding. */
#undef INC_MD_IMAGE_INC_GRAFT_POINT
#undef INC_MD_WEB_INC_GRAFT_POINT
/* Juniper changed the following. */
#define INC_MD_EMAIL_EVENTS_INC_GRAFT_POINT 
/* Juniper changed the preceeding. */
#undef INC_MD_EMAIL_UTILS_INC_GRAFT_POINT
#undef INC_MD_FILE_INC_GRAFT_POINT
/* Juniper changed the following. */
#define INC_MD_IF_CONFIG_INC_GRAFT_POINT 1
/* Juniper changed the preceeding. */
#undef INC_MD_SNMP_INC_GRAFT_POINT
#undef INC_MD_TIME_INC_GRAFT_POINT
#undef INC_MD_UTILS_INC_GRAFT_POINT
/* Juniper changed the following. */
#define INC_MDM_DB_INC_GRAFT_POINT	1
/* Juniper changed the preceeding. */
#undef INC_MDM_LICENSE_INC_GRAFT_POINT
#undef INC_MD_TMS_GRAPHS_INC_GRAFT_POINT

#undef INC_TACL_HEADER_INC_GRAFT_POINT
#undef INC_TACL_DEFS_INC_GRAFT_POINT

#undef INC_LICENSE_INC_GRAFT_POINT
/* Juniper changed the following. */
#define INC_EVENT_NOTIFS_INC_GRAFT_POINT 
/* Juniper changed the preceeding. */
#undef INC_BNODE_HEADER_INC_GRAFT_POINT
#undef INC_BNODE_TYPES_INC_GRAFT_POINT
#undef INC_SN_UTILS_INC_GRAFT_POINT

#undef INC_PM_CONFIG_INC_GRAFT_POINT
#undef INC_PM_MAIN_INC_GRAFT_POINT

#undef INC_COMMON_INC_GRAFT_POINT
/* Juniper changed the following. */
#define INC_MISC_INC_GRAFT_POINT 1
/* Juniper changed the preceeding. */
#undef INC_LICENSE_LK2_GRAFT_POINT
#undef INC_MDB_ADV_QUERY_GRAFT_POINT

#undef INC_LOGGING_HEADER_INC_GRAFT_POINT

#undef INC_SN_MOD_INC_GRAFT_POINT

/* Juniper changed the following. */
#define INC_ST_MOD_INC_GRAFT_POINT 1
/* Juniper changed the preceeding. */

#undef INC_WEB_MOD_INC_GRAFT_POINT
#undef INC_WEB_FAULTS_INC_GRAFT_POINT
#undef INC_WEB_CONFIG_FORM_CONFIG_INC_GRAFT_POINT 

#undef INC_WIZ_MOD_INC_GRAFT_POINT
#undef INC_WIZ_STEPS_INC_GRAFT_POINT

#undef INC_OPENSSH_TMS_SSH_UTILS_GRAFT_POINT

#undef INC_CL_MAIN_INC_GRAFT_POINT
#undef INC_CL_MGMT_INC_GRAFT_POINT
#undef INC_CL_CFG_SYNC_INC_GRAFT_POINT
#undef INC_CL_DISCOVERY_INC_GRAFT_POINT

#undef INC_RGP_ACTIONS_INC_GRAFT_POINT
#undef INC_RBMD_REMOTE_INC_GRAFT_POINT

#undef INC_APACHE_MODULES_INC_GRAFT_POINT

#endif /* CUSTOMER_H_REDIST_ONLY */


/******************************************************************************
 ******************************************************************************
 *** Defaults.  In case you do not define a value above which can be 
 *** defined in this file (whether intentionally, or simply because the
 *** value is optional and was added without fanfare), tdefaults.h defines
 *** the constants to the defaults which will be honored.
 ***
 *** (Note that not all defaults are done this way.  Some of them are
 *** defined in .c files, or other .h files wherever they are needed.
 *** But over time they should be migrated to defaults.h)
 ******************************************************************************
 ******************************************************************************
 ***/
#include "tdefaults.h"


#ifdef __cplusplus
}
#endif

#endif /* __CUSTOMER_H_ */
