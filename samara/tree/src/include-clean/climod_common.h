/*
 *
 * climod_common.h
 *
 *
 *
 */

#ifndef __CLIMOD_COMMON_H_
#define __CLIMOD_COMMON_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ========================================================================= */
/** \file climod_common.h CLI module shared headers
 * \ingroup cli
 *
 * This file contains definitions that are not part of the CLI framework
 * but are shared by all of the CLI modules.
 */

#include "dso.h"
#include "cli_module.h"
#include "md_client.h"
#include "customer.h"

/* ------------------------------------------------------------------------- */
/** @name Capability flags
 *
 * These are flags which control which commands are available to the
 * user in the CLI, if PROD_FEATURE_CAPABS is defined.  If that
 * feature not not defined (generally because you are using
 * PROD_FEATURE_ACLS instead), then these are not used.
 *
 * The first set (ccp_*_*_max) are simply passed
 * through from queries to the management daemon.  They represent the
 * maximum capabilities that the user can obtain.  The next set
 * (ccp_*_*_curr) represent the current capabilities that the user
 * has.  They will change as the user moves between standard, enable,
 * and config modes, but will never exceed the maximum capabilities.
 * The next set (ccp_mode_*) simply represent which mode the user is
 * in.  They are cumulative; a user always has the capabilities for
 * his current mode and all beneath it.
 *
 * Note: these are \#defines instead of static const variables because
 * gcc won't let us use the latter in constant initializations of
 * arrays of variables for some reason.
 *
 * ("rstr" is short for "restricted")
 */
/*@{*/

/*
 * Important to note that cli_capability is 64-bits in size.
 * The first set of \#defines indicate with octet the capability
 * class occupies. Within each type (i.e. octet), a total of 8 classes
 * may be defined. The system defines 3 (basic, restricted, privileged)
 * leaving 5 for expansion.
 * NOTE: We rely on the property that the max and curr bit positions
 * match up so a simple shift can make the two comparable bitwise.
 */
#define ccp_query_max_octet      (0 * 8)
#define ccp_action_max_octet     (1 * 8)
#define ccp_set_max_octet        (2 * 8)
#define ccp_query_curr_octet     (3 * 8)
#define ccp_action_curr_octet    (4 * 8)
#define ccp_set_curr_octet       (5 * 8)
#define ccp_mode_octet           (6 * 8)

#define ccp_curr_start_octet     (3 * 8)

#define ccp_class_basic          0x01
#define ccp_class_rstr           0x02
#define ccp_class_priv           0x04
/* ========================================================================= */
/* Customer-specific graft point 1: capability classes
 * Values available be: 0x08, 0x10, 0x20, 0x40, 0x80
 * =========================================================================
 */
#ifdef INC_CLIMOD_COMMON_HEADER_INC_GRAFT_POINT
#undef CLIMOD_COMMON_HEADER_INC_GRAFT_POINT
#define CLIMOD_COMMON_HEADER_INC_GRAFT_POINT 1
#include "../include/climod_common.inc.h"
#endif /* INC_CLIMOD_COMMON_HEADER_INC_GRAFT_POINT */


#define ccp_query_basic_max \
    (((cli_capability)ccp_class_basic) <<  ccp_query_max_octet)
#define ccp_query_rstr_max \
    (((cli_capability)ccp_class_rstr) <<  ccp_query_max_octet)
#define ccp_query_priv_max \
    (((cli_capability)ccp_class_priv) <<  ccp_query_max_octet)
#define ccp_action_basic_max \
    (((cli_capability)ccp_class_basic) <<  ccp_action_max_octet)
#define ccp_action_rstr_max \
    (((cli_capability)ccp_class_rstr) <<  ccp_action_max_octet)
#define ccp_action_priv_max \
    (((cli_capability)ccp_class_priv) <<  ccp_action_max_octet)
#define ccp_set_basic_max \
    (((cli_capability)ccp_class_basic) <<  ccp_set_max_octet)
#define ccp_set_rstr_max \
    (((cli_capability)ccp_class_rstr) <<  ccp_set_max_octet)
#define ccp_set_priv_max \
    (((cli_capability)ccp_class_priv) <<  ccp_set_max_octet)
/* ========================================================================= */
/* Customer-specific graft point 2: CLI capability maximum
 * Values available be: 0x08, 0x10, 0x20, 0x40, 0x80
 * =========================================================================
 */
#ifdef INC_CLIMOD_COMMON_HEADER_INC_GRAFT_POINT
#undef CLIMOD_COMMON_HEADER_INC_GRAFT_POINT
#define CLIMOD_COMMON_HEADER_INC_GRAFT_POINT 2
#include "../include/climod_common.inc.h"
#endif /* INC_CLIMOD_COMMON_HEADER_INC_GRAFT_POINT */

#define ccp_query_basic_curr \
    (((cli_capability)ccp_class_basic) <<  ccp_query_curr_octet)
#define ccp_query_rstr_curr \
    (((cli_capability)ccp_class_rstr) <<  ccp_query_curr_octet)
#define ccp_query_priv_curr \
    (((cli_capability)ccp_class_priv) <<  ccp_query_curr_octet)
#define ccp_action_basic_curr \
    (((cli_capability)ccp_class_basic) <<  ccp_action_curr_octet)
#define ccp_action_rstr_curr \
    (((cli_capability)ccp_class_rstr) <<  ccp_action_curr_octet)
#define ccp_action_priv_curr \
    (((cli_capability)ccp_class_priv) <<  ccp_action_curr_octet)
#define ccp_set_basic_curr \
    (((cli_capability)ccp_class_basic) <<  ccp_set_curr_octet)
#define ccp_set_rstr_curr \
    (((cli_capability)ccp_class_rstr) <<  ccp_set_curr_octet)
#define ccp_set_priv_curr \
    (((cli_capability)ccp_class_priv) <<  ccp_set_curr_octet)
/* ========================================================================= */
/* Customer-specific graft point 3: CLI capability current
 * Values available be: 0x08, 0x10, 0x20, 0x40, 0x80
 * =========================================================================
 */
#ifdef INC_CLIMOD_COMMON_HEADER_INC_GRAFT_POINT
#undef CLIMOD_COMMON_HEADER_INC_GRAFT_POINT
#define CLIMOD_COMMON_HEADER_INC_GRAFT_POINT 3
#include "../include/climod_common.inc.h"
#endif /* INC_CLIMOD_COMMON_HEADER_INC_GRAFT_POINT */


/*
 * These are capability flags used to track which mode the CLI is in,
 * when PROD_FEATURE_CAPABS is enabled.  Note that the regardless of
 * whether capabilities or ACLs is in use, the current mode is also
 * tracked with cli_get_mode() and cli_set_mode().
 *
 * In other words, any code changing the current mode must reflect the
 * change as follows:
 *
 *   1. If PROD_FEATURE_CAPABS is enabled:
 *        (a) Call cli_set_capabilities() or cli_replace_capabilities() to
 *            set these flags correctly.
 *        (b) Call cli_set_mode().
 *
 *   2. If PROD_FEATURE_ACLS is enabled:
 *        (a) Call cli_set_mode().
 *
 * The reflection of the current mode in capability flags is retained
 * for backward compatibility with old CLI modules which expect to
 * find the information there.  Also, under PROD_FEATURE_CAPABS, it
 * remains the only way a node can explicitly declare which mode(s) it
 * should be available in, using cc_capab_required and cc_capab_avoid.
 */
#define ccp_mode_standard \
    (((cli_capability)0x01) <<  ccp_mode_octet)
#define ccp_mode_enable \
    (((cli_capability)0x02) <<  ccp_mode_octet)
#define ccp_mode_config \
    (((cli_capability)0x04) <<  ccp_mode_octet)

/*@}*/


/* ------------------------------------------------------------------------- */
/* Common strings
 */

/* Must match GETTEXT_PACKAGE as defined in the libcli Makefile */
#define LIBCLI_GETTEXT_DOMAIN "libcli"

/*
 * It would be better for these to be static const char * variables,
 * so they can be queried from GDB.  But for I18N, we need them to
 * be \#defines so they can handle the string lookup for us.
 * 
 * I18N note: we use D_() instead of _() here because these may be
 * used from a variety of contexts from different gettext domains, so
 * the GETTEXT_DOMAIN preprocessor constant that _() depends on may be
 * wrong.
 */

/* ............................................................................
 * IP addresses
 *
 * Below we have various strings to give help for IP addresses.  These are
 * meant to cover a wide range of situations, as well as remain backward
 * compatible with modules which were already using constants that were in
 * here from before.  But there are only three that are used by Samara
 * components:
 *   - cli_msg_ip_addr_cond   (IPv4 address only)
 *   - cli_msg_ipv6_addr      (IPv6 address only
 *   - cli_msg_inet_addr_cond (IPv4 or IPv6 address)
 *
 * The basic idea is that when PROD_FEATURE_IPV6 is disabled, none of our
 * nodes permit IPv6 addresses, so we maintain our terminology from before,
 * and call everything just an "IP address".  But when PROD_FEATURE_IPV6
 * is enabled, we never just say "IP address" -- we always clarify which
 * kind(s) we want.
 */
#define cli_msg_ip_addr \
    D_("<IP address>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_ipv4_addr \
    D_("<IPv4 address>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_ipv6_addr \
    D_("<IPv6 address>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_inet_addr \
    D_("<IPv4 or IPv6 address>", LIBCLI_GETTEXT_DOMAIN)

/*
 * For things that can only be IPv4 addresses, like ARP.
 */
#ifdef PROD_FEATURE_IPV6
#define cli_msg_ip_addr_cond                                                  \
    D_("<IPv4 address>", LIBCLI_GETTEXT_DOMAIN)
#else
#define cli_msg_ip_addr_cond                                                  \
    D_("<IP address>", LIBCLI_GETTEXT_DOMAIN)
#endif

/*
 * For things that can be either IPv4 or IPv6, but are conditionalized
 * on PROD_FEATURE_IPV6.  When IPv6 is enabled, we want to clarify
 * that both are permitted; but when it's not, we retain our
 * terminology because we don't allow IPv6 addresses.
 */
#ifdef PROD_FEATURE_IPV6
#define cli_msg_inet_addr_cond                                                \
    D_("<IPv4 or IPv6 address>", LIBCLI_GETTEXT_DOMAIN)
#else
#define cli_msg_inet_addr_cond                                                \
    D_("<IP address>", LIBCLI_GETTEXT_DOMAIN)
#endif

/* ............................................................................
 * Hostnames, maybe plus IP addresses
 *
 * These are pretty much the same as the options in the last section,
 * except with a hostname listed as an alternative.  We lack only an
 * option for hostname or IPv6 address, since there are no cases like
 * that.  Again, some of these are meant for backward compatibility or 
 * covering all the bases for customer code, but Samara code is expected
 * to use only three:
 *   - cli_msg_hostname               (hostname only, no IP addresses)
 *   - cli_msg_hostname_or_ip_cond    (hostname or IPv4 address)
 *   - cli_msg_hostname_or_inet_cond  (hostname, IPv4 address, or IPv6 address)
 */

#define cli_msg_hostname \
    D_("<hostname>", LIBCLI_GETTEXT_DOMAIN)

#define cli_msg_hostname_or_ip \
    D_("<hostname or IP address>", LIBCLI_GETTEXT_DOMAIN)

#define cli_msg_hostname_or_ipv4 \
    D_("<hostname or IPv4 address>", LIBCLI_GETTEXT_DOMAIN)

#define cli_msg_hostname_or_inet \
    D_("<hostname, IPv4 or IPv6 address>", LIBCLI_GETTEXT_DOMAIN)

#ifdef PROD_FEATURE_IPV6
#define cli_msg_hostname_or_ip_cond \
    D_("<hostname or IPv4 address>", LIBCLI_GETTEXT_DOMAIN)
#else
#define cli_msg_hostname_or_ip_cond \
    D_("<hostname or IP address>", LIBCLI_GETTEXT_DOMAIN)
#endif

#ifdef PROD_FEATURE_IPV6
#define cli_msg_hostname_or_inet_cond                                         \
    D_("<hostname, IPv4 or IPv6 address>", LIBCLI_GETTEXT_DOMAIN)
#else
#define cli_msg_hostname_or_inet_cond                                         \
    D_("<hostname or IP address>", LIBCLI_GETTEXT_DOMAIN)
#endif


/* ............................................................................
 * Masks and such
 *
 * XXX/EMT: we'll need some IPv6 stuff in here.
 */

#define cli_error_netmask \
    D_("Please enter a netmask either as a dotted quad (e.g. 255.255.255.0)\n"\
       "or as a mask length after a slash (e.g. /29).  The mask length must\n"\
       "be between 1 and 32, inclusive (or 0 if the address is also 0).\n",   \
       LIBCLI_GETTEXT_DOMAIN)

#define cli_error_ipv6addr_masklen \
  D_("Please enter an IPv6 address/masklen (e.g. 2001:db8::123/64).\n"        \
     "The IPv6 address must be in the form 2001:db8::123 and the mask "       \
     "length\nmust be between 1 and 128, inclusive.\n",                       \
     LIBCLI_GETTEXT_DOMAIN)

/* ............................................................................
 * Other messages
 */

#define cli_msg_mac_addr \
    D_("<MAC address>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_domain \
    D_("<domain name>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_passwd \
    D_("<password>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_cleartext_passwd \
    D_("<cleartext password>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_encrypted_passwd \
    D_("<encrypted password>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_email_addr \
    D_("<email addr>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_time \
    D_("<hh>:<mm>:<ss>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_date \
    D_("<yyyy>/<mm>/<dd>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_minutes \
    D_("<number of minutes>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_filename \
    D_("<filename>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_url \
    D_("<URL or scp://username[:password]@hostname/path/" \
       "filename>", LIBCLI_GETTEXT_DOMAIN)

/* URL help for file downloads */
#define cli_msg_url_alt_dl \
    D_("<download URL>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_url_alt_dl_hint \
    D_("http, https, ftp, tftp, scp and sftp are supported.  " \
       "e.g. scp://username[:password]@hostname/path/filename", \
       LIBCLI_GETTEXT_DOMAIN)

/* URL help for file uploads */
#define cli_msg_url_alt_ul \
    D_("<upload URL>", LIBCLI_GETTEXT_DOMAIN)
#define cli_msg_url_alt_ul_hint \
    D_("ftp, tftp, scp and sftp are supported.  " \
       "e.g. scp://username[:password]@hostname/path/filename", \
       LIBCLI_GETTEXT_DOMAIN)

#define CLI_CMC_MANAGED_CLIENT_WARNING                                        \
_("*** Warning: this system is under management of a Media Flow\n"    \
  "             Manager (MFM).  You should generally not change shared\n"     \
  "             configuration locally on this system, as it may be\n"         \
  "             overwritten by commands issued from the MFM.\n\n")


/* ------------------------------------------------------------------------- */
/* Reverse-mapping ordering constants
 */

typedef enum {

    /* Licenses might be needed to unlock some functionality */
    cro_licenses =      -2000,

#ifdef PROD_FEATURE_BONDING
    /* Tightly coupled with interface configuration */
    cro_bonding =       -1010,
#endif /* PROD_FEATURE_BONDING */

#ifdef PROD_FEATURE_BRIDGING
    /* Put before interface commands, see bug 13240 */
    cro_bridging =      -1005,
#endif /* PROD_FEATURE_BRIDGING */

    cro_interface =     -1000,
#ifdef PROD_FEATURE_IPV6
    cro_interface_ipv6 = -990,
#endif
    cro_routing =        -950,
    cro_ip =             -900,
#ifdef PROD_FEATURE_IPV6
    cro_ipv6 =           -875,
#endif
    cro_logging =        -850,
    cro_aaa_roles =      -808,
    cro_local_users =    -805,
    cro_aaa_server =     -802,
    cro_aaa =            -800,
    cro_snmp =            900,
    cro_pm =              950,
    cro_mgmt =           1000,
#ifdef PROD_FEATURE_IPTABLES
    cro_iptables_ipv4 =  1010,
    cro_iptables_ipv6 =  1011,
#endif /* PROD_FEATURE_IPTABLES */
    cro_cmc =            1020,
    cro_cluster =        1050,
    cro_virt =           1060,
    cro_ssh =            1100,
#ifdef PROD_FEATURE_CRYPTO
    cro_crypto =         1110,
#endif /* PROD_FEATURE_CRYPTO */
    cro_certs =          1130,
    cro_unconsumed =    65535,
} cli_revmap_order_value;

static const lc_signed_enum_string_map cli_revmap_order_value_map[] = {
    {cro_licenses, N_("License keys")},
#ifdef PROD_FEATURE_BONDING
    {cro_bonding, N_("Bonded interface configuration")},
#endif /* PROD_FEATURE_BONDING */
#ifdef PROD_FEATURE_BRIDGING
    {cro_bridging, N_("Bridging configuration")},
#endif /* PROD_FEATURE_BRIDGING */
    {cro_interface, N_("Network interface configuration")},
#ifdef PROD_FEATURE_IPV6
    {cro_interface_ipv6, N_("Network interface IPv6 configuration")},
#endif
    {cro_routing, N_("Routing configuration")},
    {cro_ip, N_("Other IP configuration")},
#ifdef PROD_FEATURE_IPV6
    {cro_ipv6, N_("Other IPv6 configuration")},
#endif
    {cro_logging, N_("Logging configuration")},
    {cro_aaa_roles, N_("Access control role configuration")},
    {cro_local_users, N_("Local user account configuration")},
    {cro_aaa_server, N_("AAA remote server configuration")},
    {cro_aaa, N_("AAA configuration")},
    {cro_snmp, N_("SNMP configuration")},
    {cro_pm, N_("Process Manager configuration")},
    {cro_mgmt, N_("Network management configuration")},
#ifdef PROD_FEATURE_IPTABLES
#ifdef PROD_FEATURE_IPV6
    {cro_iptables_ipv4, N_("IPv4 packet filtering configuration")},
#else
    {cro_iptables_ipv4, N_("IP packet filtering configuration")},
#endif /* PROD_FEATURE_IPV6 */
    {cro_iptables_ipv6, N_("IPv6 packet filtering configuration")},
#endif /* PROD_FEATURE_IPTABLES */
    {cro_cmc, N_("CMC configuration")},
    {cro_cluster, N_("Cluster configuration")},
    {cro_virt, N_("Virtualization configuration")},
    {cro_ssh, N_("SSH and Key configuration")},
#ifdef PROD_FEATURE_CRYPTO
    {cro_crypto, N_("Cryptographic configuration")},
#endif /* PROD_FEATURE_CRYPTO */
    {cro_certs, N_("X.509 certificates configuration")},    
    {cro_unconsumed, N_("Miscellaneous other settings")},
    {0, NULL}
};


/* ------------------------------------------------------------------------- */
/* Initialization ordering constants.
 *
 * These are listed centrally here to make it easier to compare them
 * between modules.  These should be kept in sorted order.  Anything
 * not listed here has an initialization order of zero.
 *
 * We'd like these to be const variables, like the apply orders in
 * mdmod_common.h, but for some reason those cannot be used to initialize
 * other constant variables.
 */

enum {

    /*
     * Try to get initialized after "enable", so we can check if it's
     * available.
     */
    cli_shell_init_order = 100,
    
    /*
     * We want to initialize after cli_image_cmds, because we are potentially
     * hiding a command that it registers.
     */
    cli_cmc_server_init_order = 1000,
    
    /*
     * Try to get initialized after all of the modules whose commands we
     * might be restricting.  We don't use INT32_MAX, just in case 
     * someone else wants to go after us for some reason.  And we wanted
     * to be <10000, which is cli_restrict_cmds.c's, in case it wanted
     * to restrict any of ours.
     */
    cli_ip_init_order = 5000,

    /*
     * Try to get initialized after all of the modules whose commands we
     * might be restricting.  We don't use INT32_MAX, just in case 
     * someone else wants to go after us for some reason.
     */
    cli_restrict_init_order = 10000,

};


/* ------------------------------------------------------------------------- */
/* Substitution constants for building the prompt.
 *
 * Each number may only be used once.  Here we define the standard
 * numbers used by the infrastructure; customer code may use any of
 * the remaining numbers.  Please see cli_privmode_prompt_set() for
 * how the prompt is constructed, to choose which number you want to
 * substitute for.  Note that 7-9 are only visible in config mode.
 *
 * NOTE: even if you are not currently using the clustering and/or CMC
 * products, do not use these numbers!  They are reserved for the
 * general use of the infrastructure, and their potential use is not 
 * necessarily limited to clustering or the CMC.
 */

enum {
    clish_prompt_subst_hostname = 1,
    clish_prompt_subst_cluster  = 4,
    clish_prompt_subst_cmc      = 5,   /* Currently unused, but reserved */
};


/* ------------------------------------------------------------------------- */
/* Shared functions
 */

typedef int (*cli_stats_get_alarm_state_type)(const bn_binding_array *bindings,
                                              const char *alarm_id,
                                              const char *node_name,
                                              char **ret_state,
                                              tbool *ret_error);

int cli_stats_get_alarm_state(const bn_binding_array *bindings, 
                              const char *alarm_id,
                              const char *node_name,
                              char **ret_state, tbool *ret_error);

typedef int (*cli_stats_show_alarm_one_type)(const bn_binding_array *bindings,
                                             uint32 idx,
                                             const bn_binding *binding,
                                             const tstring *name,
                                             const tstr_array *name_components,
                                             const tstring *name_last_part,
                                             const tstring *value,
                                             void *callback_data);

int cli_stats_show_alarm_one(const bn_binding_array *bindings,
                             uint32 idx, const bn_binding *binding,
                             const tstring *name,
                             const tstr_array *name_components,
                             const tstring *name_last_part,
                             const tstring *value,
                             void *callback_data);

/* ......................................................................... */
/* From cli_interface_cmds.c
 */

typedef int (*cli_interface_help_type)(void *data, cli_help_type help_type, 
                                       const cli_command *cmd, 
                                       const tstr_array *cmd_line,
                                       const tstr_array *params,
                                       const tstring *curr_word,
                                       void *context);

int cli_interface_help(void *data, cli_help_type help_type, 
                       const cli_command *cmd, const tstr_array *cmd_line,
                       const tstr_array *params, const tstring *curr_word,
                       void *context);

typedef int (*cli_interface_completion_type)(void *data,
                                             const cli_command *cmd,
                                             const tstr_array *cmd_line,
                                             const tstr_array *params,
                                             const tstring *curr_word,
                                             tstr_array *ret_completions);

int cli_interface_completion(void *data, const cli_command *cmd,
                             const tstr_array *cmd_line,
                             const tstr_array *params,
                             const tstring *curr_word,
                             tstr_array *ret_completions);

/* ......................................................................... */
/* From cli_cmc_client_cmds.c
 */

#ifdef PROD_FEATURE_CMC_CLIENT

typedef int (*cli_cmc_client_is_managed_type)(tbool *ret_managed, 
                                              tbool *ret_server_init);

int cli_cmc_client_is_managed(tbool *ret_managed, tbool *ret_server_init);

#endif

#ifdef __cplusplus
}
#endif

#endif /* __CLIMOD_COMMON_H_ */
