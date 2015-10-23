/*
 *
 * src/bin/mgmtd/md_net_utils.h
 *
 *
 *
 */

#ifndef __MD_NET_UTILS_H_
#define __MD_NET_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


#include "mdb_db.h"

/**
 * \file src/bin/mgmtd/md_net_utils.h Networking-related utilities for 
 * mgmtd modules.
 * \ingroup mgmtd
 */

int md_net_utils_init(void);
int md_net_utils_deinit(void);

/*
 * We use the "proto" parameter to the "ip" command to keep track of
 * why we installed a route.  "static" is the only standard protocol
 * available for us to use.  Technically, all of the routes installed
 * by us are "static", inasmuch as that means that they are installed
 * by a userland process (mgmtd).  However, we want to distinguish
 * between certain routes we have installed.  A number is also
 * accepted for "proto", and we just want to use one which won't be
 * confused for anything else.  As of now, the following ranges are 
 * in use:
 *   - 0-4: standard protocols used by the kernel (see rtnetlink.h)
 *   - 8-15: some protocols which have been earmarked for specified
 *           userland purposes (see rtnetlink.h)
 *   - 246-254: other protocols earmarked for some gated-related
 *              purposes (see /etc/iproute2/rt_protos).
 *
 * To steer clear of these, we start in the middle, at 128.
 */
#define MD_ROUTE_ORIGIN_DHCP       128

/**
 * Make sure that no interface names begin with md_virt_ifname_prefix
 * ("vif") or md_virt_vbridge_prefix ("virbr").  We fix not only
 * interface config, but several other binding name patterns, listed
 * in md_ifname_bname_patterns in md_net_utils.c
 *
 * NOTE: this is meant to be called from an upgrade function, and is
 * called by md_if_config.c in its 11-to-12 upgrade.  It should not
 * need to be called from anywhere else.
 */
int md_if_names_fix(mdb_db *db);

int md_rts_protocol_to_string(uint32 rtproto, const char **ret_string);

int md_rts_table_to_string(uint32 rttable, const char **ret_string);

/**
 * Check if IPv6 PROD_FEATURE_IPV6 is on.
 */
tbool md_proto_ipv6_feature_enabled(void);

/**
 * See if IPv6 support is present in the kernel.
 */
tbool md_proto_ipv6_avail(void);

/**
 * Check if IPv6 is feature-enabled and is thought to be supported by
 * the platform.  This will always be 'false' if PROD_FEATURE_IPV6 is
 * off.  This does not change at runtime based on configuration.
 *
 * This is used to decide if IPv6-related configuration should be
 * applied.
 */
tbool md_proto_ipv6_supported(void);

/**
 * Check if IPv6 is feature-enabled, is thought to be supported by the
 * platform, and is administratively enabled.  This will always be
 * 'false' if PROD_FEATURE_IPV6 is off.  This can change based on
 * configuration.
 *
 * This is used to decide if interfaces and routes will be configured
 * for IPv6.
 */
tbool md_proto_ipv6_enabled(md_commit *commit, const mdb_db *db);


#ifdef __cplusplus
}
#endif

#endif /* __MD_NET_UTILS_H_ */
