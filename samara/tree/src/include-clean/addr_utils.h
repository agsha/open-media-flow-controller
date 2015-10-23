/*
 *
 * addr_utils.h
 *
 *
 *
 */

#ifndef __ADDR_UTILS_H_
#define __ADDR_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include "tinttypes.h"
#include "ttypes.h"


/* ------------------------------------------------------------------------- */
/** \file src/include/addr_utils.h Internet address conversion utilities
 * for IPv4 and IPv6 addresses stored in network byte order.
 * \ingroup lc
 *
 * These functions operate on IPv4 and IPv6 addresses, prefixes and masks.
 * They do so using addresses in network byte order, and use
 * types such as 'struct in_addr' , 'struct in6_addr' , 'struct
 * sockaddr_in' and 'struct sockaddr_in6'.  They also use our types such
 * as 'bn_inetaddr' 'bn_inetaddrz' and 'bn_inetprefix' .
 *
 */

/* ------------------------------------------------------------------------- */
/** Internet address types
 *
 * Note that except for bn_ipv4addr_octets and bn_ipv6addr_octets, it is
 * expected that callers will never directly access the members of these
 * structures.  Instead, the functions declared below should be used.
 */

/* -------------------- IPv4 */

typedef struct bn_ipv4addr {
    struct in_addr bi_addr;
} __attribute__ ((packed)) bn_ipv4addr;

typedef struct bn_ipv4addr_octets {
    uint8 bio_addr[4];
} __attribute__ ((packed)) bn_ipv4addr_octets;

typedef struct bn_ipv4prefix {
    struct in_addr bip_addr;
    uint8 bip_prefix_len; /* 0-32 */
} __attribute__ ((packed)) bn_ipv4prefix;

/* -------------------- IPv6 */

typedef struct bn_ipv6addr {
    struct in6_addr bi6_addr;
}  __attribute__ ((packed)) bn_ipv6addr;

typedef struct bn_ipv6addr_octets {
    uint8 bi6o_addr[16];
} __attribute__ ((packed)) bn_ipv6addr_octets;

typedef struct bn_ipv6addrz {
    struct in6_addr bi6z_addr;

    /**
     * For IPv6, the part after the % sign that
     * says, for example, which interface a link
     * local address is on.  This applies to
     * non-global addresses. See also struct
     * sockaddr_in6's sin6_scope_id.
     *
     * 0 for IPv4, or if no zone id specified.
     */
    uint32 bi6z_zone_id;
}  __attribute__ ((packed)) bn_ipv6addrz;

typedef struct bn_ipv6prefix {
    struct in6_addr bi6p_addr;
    uint8 bi6p_prefix_len; /* 0-128 */
}  __attribute__ ((packed)) bn_ipv6prefix;

/* -------------------- Inet (IPv4 or IPv6) */

typedef struct bn_inetaddr {
    uint16 bin_family;  /* AF_INET or AF_INET6 */
    union {
        struct in_addr bina_in4;
        struct in6_addr bina_in6;
    } bin_addr;
}  __attribute__ ((packed)) bn_inetaddr;

typedef struct bn_inetaddrz {
    uint16 binz_family;  /* AF_INET or AF_INET6 */
    union {
        struct in_addr binza_in4;
        struct in6_addr binza_in6;
    } binz_addr;

    /**
     * For IPv6, the part after the % sign that
     * says, for example, which interface a link
     * local address is on.  This applies to
     * non-global addresses. See also struct
     * sockaddr_in6's sin6_scope_id.
     *
     * 0 for IPv4, or if no zone id specified.
     */
    uint32 binz_zone_id;
}  __attribute__ ((packed)) bn_inetaddrz;

typedef struct bn_inetprefix {
    uint16 binp_family;  /* AF_INET or AF_INET6 */
    union {
        struct in_addr binpa_in4;
        struct in6_addr binpa_in6;
    } binp_addr;
    uint8 binp_prefix_len; /* IPv4 : 0-32, IPv6 : 0-128 */
}  __attribute__ ((packed)) bn_inetprefix;



/* ------------------------------------------------------------------------- */
/* Globals and constants */
/* ------------------------------------------------------------------------- */

static const char lia_inetprefix_delim_char = '/';
static const char lia_inet_zone_id_delim_char = '%';
static const uint8 lia_ipv4addr_bits = 32;
static const uint8 lia_ipv6addr_bits = 128;

/* Buffer size big enough for any IPv4 address, plus a NUL */
/* #define INET_ADDRSTRLEN 16     (In <netinet/in.h>) */

/** Buffer big enough for any IPv4 prefix ("10.1.2.3/24"), plus a NUL */
#define INET_PREFIXSTRLEN 19

/* Buffer size big enough for any IPv6 address, plus a NUL */
/* #define INET6_ADDRSTRLEN 46    (In <netinet/in.h>) */

/** Buffer size big enough for non-v4-mapped IPv6 address, plus a NUL */
#define INET6_NONV4MAPPED_ADDRSTRLEN 40

/** Buffer size big enough for any IPv6 prefix ("1234::/64"), plus a NUL */
#define INET6_PREFIXSTRLEN 50

/**
 * Buffer size big enough for any IPv6 address with numeric zone id
 * ("fe80::3210%7"), plus a NUL
 */
#define INET6_ADDRZSTRLEN 57


/* These are okay, as the network and host byte order are equal */
#define LIA_IPV4ADDR_ANY_INIT { { 0 } }
#define LIA_IPV4ADDR_OCTETS_ANY_INIT {{0, }}
#define LIA_IPV4ADDR_BROADCAST_INIT { { 0xffffffff } }
#define LIA_IPV4PREFIX_ANY_INIT { { 0 }, 0}

#define LIA_IPV6ADDR_ANY_INIT { IN6ADDR_ANY_INIT }
#define LIA_IPV6ADDR_OCTETS_ANY_INIT {{0, }}
#define LIA_IPV6ADDRZ_ANY_INIT { IN6ADDR_ANY_INIT, 0}
#define LIA_IPV6PREFIX_ANY_INIT { IN6ADDR_ANY_INIT, 0}

#define LIA_INETADDR_ANY_INIT { AF_UNSPEC, { { 0 } } }
#define LIA_INETADDR_IPV4ADDR_ANY_INIT { AF_INET, { { 0 } } }
#define LIA_INETADDR_IPV6ADDR_ANY_INIT { AF_INET6, { { 0 } } }
#define LIA_INETADDRZ_ANY_INIT { AF_UNSPEC, { { 0 } }, 0}
#define LIA_INETADDRZ_IPV4ADDRZ_ANY_INIT { AF_INET, { { 0 } }, 0}
#define LIA_INETADDRZ_IPV6ADDRZ_ANY_INIT { AF_INET6, { { 0 } }, 0}
#define LIA_INETPREFIX_ANY_INIT { AF_UNSPEC, { { 0 } }, 0}
#define LIA_INETPREFIX_IPV4PREFIX_ANY_INIT { AF_INET, { { 0 } }, 0}
#define LIA_INETPREFIX_IPV6PREFIX_ANY_INIT { AF_INET6, { { 0 } }, 0}

extern const bn_ipv4addr lia_ipv4addr_any;
extern const bn_ipv4addr_octets lia_ipv4addr_octets_any;
extern const bn_ipv4addr lia_ipv4addr_broadcast;
extern const bn_ipv4prefix lia_ipv4prefix_any;

extern const bn_ipv6addr lia_ipv6addr_any;
extern const bn_ipv6addr_octets lia_ipv6addr_octets_any;
extern const bn_ipv6addrz lia_ipv6addrz_any;
extern const bn_ipv6prefix lia_ipv6prefix_any;

extern const bn_inetaddr lia_inetaddr_any;
extern const bn_inetaddr lia_inetaddr_ipv4addr_any;
extern const bn_inetaddr lia_inetaddr_ipv6addr_any;
extern const bn_inetaddrz lia_inetaddrz_any;
extern const bn_inetaddrz lia_inetaddrz_ipv4addrz_any;
extern const bn_inetaddrz lia_inetaddrz_ipv6addrz_any;
extern const bn_inetprefix lia_inetprefix_any;
extern const bn_inetprefix lia_inetprefix_ipv4prefix_any;
extern const bn_inetprefix lia_inetprefix_ipv6prefix_any;

extern const uint16 lia_family_unspec;
extern const uint16 lia_family_inet;
extern const uint16 lia_family_inet6;

/*
 * Pick up lc_enum_string_map.
 *
 * XXXX This is a circular include, and we should move
 * lc_enum_string_map out of common.h and into a separate header to
 * avoid this.
 */
#include "common.h"

extern const lc_enum_string_map lia_family_map[];

/** Same as lia_family_map, but with "IP" instead of "IPv4" */
extern const lc_enum_string_map lia_family_map_inetip[];

/**
 * Flags to be passed to lia_addrtype_inetaddr_match() and friends.
 *
 * NOTE: lia_addrtype_flags_map (in addr_utils.c) must be kept in sync.
 */
typedef enum {
    lia_addrtype_none = 0,

    /** Address has family IPv4 */
    lia_addrtype_ipv4 = 1 << 0,

    /** Address has family IPv6 */
    lia_addrtype_ipv6 = 1 << 1,

    /** Address is 0.0.0.0 */
    lia_addrtype_unspecified_ipv4 = 1 << 2,

    /** Address is :: */
    lia_addrtype_unspecified_ipv6 = 1 << 3,

    /** Address is unspecified (either IPv4 or IPv6) */
    lia_addrtype_unspecified = (lia_addrtype_unspecified_ipv4 |
                                lia_addrtype_unspecified_ipv6),

    /** Address is 127.0.0.1 */
    lia_addrtype_localhost_ipv4 = 1 << 4,

    /** Address in 127/8 */
    lia_addrtype_loopback_ipv4 = 1 << 5,

    /** Address in 127/8 but NOT 127.0.0.1 */
    lia_addrtype_loopback_not_localhost_ipv4 = 1 << 6,

    /**
     * Address is ::1 , Loopback .  Note that unlike for IPv4, for IPv6
     * there are no non-localhost loopback addresses.  ::1 is the
     * only IPv6 loopback address.
     */
    lia_addrtype_loopback_ipv6 = 1 << 7,
    lia_addrtype_localhost_ipv6 = lia_addrtype_loopback_ipv6,

    /** 127/8 or ::1 */
    lia_addrtype_loopback = (lia_addrtype_loopback_ipv4 |
                             lia_addrtype_loopback_ipv6),

    /** 127.0.0.1 or ::1 */
    lia_addrtype_localhost = (lia_addrtype_localhost_ipv4 |
                              lia_addrtype_localhost_ipv6),

    /** Note that no IPv6 address matches this */
    lia_addrtype_loopback_not_localhost =
        lia_addrtype_loopback_not_localhost_ipv4,

    /** Address in 0/8 (note contains 0.0.0.0) (IPv4 only) */
    lia_addrtype_zeronet = 1 << 8,

    /** Address in 0/8 but NOT 0.0.0.0 (IPv4 only) */
    lia_addrtype_zeronet_not_unspecified = 1 << 9,

    /** Address in ::ffff:0:0/96 , IPv4-Mapped IPv6 (IPv6 only) */
    lia_addrtype_v4mapped = 1 << 10,

    /**
     * Address in ::<ipv4-address>/96 .  IPv4-Compatible IPv6 Address.
     * This is a deprecated address type.  (IPv6 only)
     */
    lia_addrtype_v4compat = 1 << 11,

    /** Address in 169.254/16 */
    lia_addrtype_linklocal_ipv4 = 1 << 12,

    /** Address in fe80::/10 , Link-Local unicast. */
    lia_addrtype_linklocal_ipv6 = 1 << 13,

    /** Address is link-local, either IPv4 or IPv6 */
    lia_addrtype_linklocal = (lia_addrtype_linklocal_ipv4 |
                              lia_addrtype_linklocal_ipv6),

    /** Address in 224/4 */
    lia_addrtype_multicast_ipv4 = 1 << 14,

    /** Address in ff00::/8 , Multicast */
    lia_addrtype_multicast_ipv6 = 1 << 15,

    /** Address is multicast, either IPv4 or IPv6 */
    lia_addrtype_multicast = (lia_addrtype_multicast_ipv4 |
                              lia_addrtype_multicast_ipv6),

    /**
     * Address in fec0::/10 . Site-Local scoped address.  This is a
     * deprecated address type.  (IPv6 only)
     */
    lia_addrtype_sitelocal = 1 << 16,

    /** Address in 240/4 (note contains 255.255.255.255) (IPv4 only) */
    lia_addrtype_badclass = 1 << 17,

    /** Address in 240/4, but NOT 255.255.255.255 (IPv4 only) */
    lia_addrtype_badclass_not_limited_broadcast = 1 << 18,

    /** Address is 255.255.255.255 (IPv4 only) */
    lia_addrtype_limited_broadcast = 1 << 19,

} lia_addrtype_flags;

/** Bit field of ::lia_addrtype_flags ORed together */
typedef uint32 lia_addrtype_flags_bf;

extern const lc_enum_string_map lia_addrtype_flags_map[];

/**
 * Typical flags to validate an endpoint address.
 */
static const lia_addrtype_flags_bf lia_addrtype_match_endpoint =
    (lia_addrtype_unspecified | lia_addrtype_loopback |
     lia_addrtype_zeronet_not_unspecified |
     lia_addrtype_v4mapped | lia_addrtype_v4compat |
     lia_addrtype_linklocal | lia_addrtype_multicast | lia_addrtype_sitelocal |
     lia_addrtype_badclass_not_limited_broadcast |
     lia_addrtype_limited_broadcast);

/**
 * Typical flags to validate an endpoint address, allowing link locals.
 */
static const lia_addrtype_flags_bf lia_addrtype_match_endpoint_ll =
    (lia_addrtype_unspecified | lia_addrtype_loopback |
     lia_addrtype_zeronet_not_unspecified |
     lia_addrtype_v4mapped | lia_addrtype_v4compat |
     lia_addrtype_multicast | lia_addrtype_sitelocal |
     lia_addrtype_badclass_not_limited_broadcast |
     lia_addrtype_limited_broadcast);

/**
 * Like "endpoint", but allows the address to be 0 (unspecified) to
 * indicate it is not set.
 */
static const lia_addrtype_flags_bf lia_addrtype_match_endpoint_maybe =
    (lia_addrtype_loopback |
     lia_addrtype_zeronet_not_unspecified |
     lia_addrtype_v4mapped | lia_addrtype_v4compat |
     lia_addrtype_linklocal | lia_addrtype_multicast | lia_addrtype_sitelocal |
     lia_addrtype_badclass_not_limited_broadcast |
     lia_addrtype_limited_broadcast);

/**
 * Like "endpoint", but allows the address to be 0 (unspecified)
 * to indicate it is not set, and allows link locals.
 */
static const lia_addrtype_flags_bf lia_addrtype_match_endpoint_ll_maybe =
    (lia_addrtype_loopback |
     lia_addrtype_zeronet_not_unspecified |
     lia_addrtype_v4mapped | lia_addrtype_v4compat |
     lia_addrtype_multicast | lia_addrtype_sitelocal |
     lia_addrtype_badclass_not_limited_broadcast |
     lia_addrtype_limited_broadcast);

/* ---------------------------------------- */

/**
 * Typical flags to validate an interface address to be assigned to a
 * local non-"lo" interface.  So "ifaddr" is like "endpoint" but allows
 * (does not match) non-localhost loopback.  This is to allow things
 * like 127.1.2.3 to be assigned to interfaces, but not 127.0.0.1 . Note
 * that "ifaddr" and "endpoint" are effectively the same for IPv6.
 */
static const lia_addrtype_flags_bf lia_addrtype_match_ifaddr =
    (lia_addrtype_unspecified | lia_addrtype_localhost |
     lia_addrtype_zeronet_not_unspecified |
     lia_addrtype_v4mapped | lia_addrtype_v4compat |
     lia_addrtype_linklocal | lia_addrtype_multicast | lia_addrtype_sitelocal |
     lia_addrtype_badclass_not_limited_broadcast |
     lia_addrtype_limited_broadcast);

/**
 * Like "ifaddr", but allows link locals.
 */
static const lia_addrtype_flags_bf lia_addrtype_match_ifaddr_ll =
    (lia_addrtype_unspecified | lia_addrtype_localhost |
     lia_addrtype_zeronet_not_unspecified |
     lia_addrtype_v4mapped | lia_addrtype_v4compat |
     lia_addrtype_multicast | lia_addrtype_sitelocal |
     lia_addrtype_badclass_not_limited_broadcast |
     lia_addrtype_limited_broadcast);

/**
 * Like "ifaddr", but allows the address to be 0 (unspecified) to
 * indicate it is not set.
 */
static const lia_addrtype_flags_bf lia_addrtype_match_ifaddr_maybe =
    (lia_addrtype_localhost |
     lia_addrtype_zeronet_not_unspecified |
     lia_addrtype_v4mapped | lia_addrtype_v4compat |
     lia_addrtype_linklocal | lia_addrtype_multicast | lia_addrtype_sitelocal |
     lia_addrtype_badclass_not_limited_broadcast |
     lia_addrtype_limited_broadcast);

/**
 * Like "ifaddr", but allows the address to be 0 (unspecified)
 * to indicate it is not set, and allows link locals.
 */
static const lia_addrtype_flags_bf lia_addrtype_match_ifaddr_ll_maybe =
    (lia_addrtype_localhost |
     lia_addrtype_zeronet_not_unspecified |
     lia_addrtype_v4mapped | lia_addrtype_v4compat |
     lia_addrtype_multicast | lia_addrtype_sitelocal |
     lia_addrtype_badclass_not_limited_broadcast |
     lia_addrtype_limited_broadcast);

/* ---------------------------------------- */

/**
 * All the addrtype flags.
 */
static const lia_addrtype_flags_bf lia_addrtype_match_all =
    (lia_addrtype_ipv4 | lia_addrtype_ipv6 |
     lia_addrtype_unspecified |
     lia_addrtype_localhost | lia_addrtype_loopback |
     lia_addrtype_loopback_not_localhost |
     lia_addrtype_zeronet |
     lia_addrtype_zeronet_not_unspecified |
     lia_addrtype_v4mapped | lia_addrtype_v4compat |
     lia_addrtype_linklocal | lia_addrtype_multicast | lia_addrtype_sitelocal |
     lia_addrtype_badclass |
     lia_addrtype_badclass_not_limited_broadcast |
     lia_addrtype_limited_broadcast);


/* ------------------------------------------------------------------------- */
/** Flags to be passed to lia_str_to_ipv6addr_ex() , etc.
 */
typedef enum {
    lacf_none =          0,

    /**
     * Only accept strings which are IPv4 address.  By default IPv4
     * addresses are accepted by functions which return either a binary
     * IPv4 address or a binary inetaddr, but are not accepted by
     * functions which return a binary IPv6 address.  If passed to a
     * function that returns a binary IPv6 address (but not one which
     * returns an inetaddr), ::lacf_conv_ipv4_to_v4mapped_ipv6 must also
     * be specified.
     */
    lacf_accept_ipv4_only = 1 << 0,

    /**
     * Only accept strings which are IPv6 addresses.  By default IPv6
     * addresses are accepted by functions returning a binary IPv6
     * address or binary inetaddr, but are not accepted by functions
     * which return a binary IPv4 address.
     */
    lacf_accept_ipv6_only = 1 << 1,

    /**
     * Convert any accepted IPv4 address to an IPv6 address.  The
     * resulting address will be of the form "::ffff:10.2.3.4".  This is
     * called an IPv4-mapped IPv6 address.  This flag and
     * ::lacf_accept_ipv6_only cannot be specified together.
     *
     * For functions which return a binary IPv6 address only, this flag
     * will also cause IPv4 addresses to be accepted.
     */
    lacf_conv_ipv4_to_v4mapped_ipv6 = 1 << 2,

#ifndef ADDR_UTILS_NO_DEPR
    /* An alias, now deprecated */
    lacf_conv_ipv4_to_ipv6 = lacf_conv_ipv4_to_v4mapped_ipv6,
#endif

    /**
     * Convert any accepted IPv6 address which is an IPv4-mapped IPv6
     * address into an IPv4 address.  Other IPv6 addresses are accepted
     * as IPv6 addresses and are not changed.  IPv4-mapped IPv6 addresses
     * are of the form: "::ffff:10.2.3.4", and the resulting converted
     * addresses will be of the form: "10.2.3.4".
     *
     * This flag and ::lacf_conv_ipv4_to_v4mapped_ipv6 cannot be
     * specified together.  This flag and ::lacf_accept_ipv4_only cannot
     * be specified together.
     */
    lacf_conv_v4mapped_ipv6_to_ipv4 = 1 << 3,

#ifndef ADDR_UTILS_NO_DEPR
    /* An alias, now deprecated */
    lacf_conv_ipv4_mapped_ipv6_to_ipv4 = lacf_conv_v4mapped_ipv6_to_ipv4,
#endif

    /**
     * If the address given to us has exactly 32 characters which are
     * all hex digits (including having no colons), then treat it as
     * if it had colons inserted every 4 characters.  e.g.
     * "fdaa0013cc0000020000000000000000" would be treated as
     * "fdaa:0013:cc00:0002:0000:0000:0000:0000".
     *
     * Currently this flag can only be used with
     * lia_str_to_ipv6addr_ex().  Passing it to any other conversion
     * function will result in an error.
     */
    lacf_accept_without_colons = 1 << 4,

    /**
     * For use only with lia_str_to_ipv6addrz_ex() and
     * lia_str_to_inetaddrz_ex(): if the zone id is not numeric, try
     * treating whatever follows the '%' as an interface name, and map
     * it to an ifindex.  If the interface name cannot be found, the
     * zone id will be invalid, and the conversion will fail.  See
     * ::lacf_convert_zone_invalid_to_zero for a way to still convert
     * such strings.
     */
    lacf_convert_zone_ifname_to_ifindex = 1 << 5,

    /**
     * For use only with lia_str_to_ipv6addrz_ex() and
     * lia_str_to_inetaddrz_ex(): the zone id is first tried as a
     * number, then (only if ::lacf_convert_zone_ifname_to_ifindex is
     * specified) as an interface name, and if the zone id has not been
     * converted, it is converted to 0.  This flag may be specified with
     * ::lacf_convert_zone_ifname_to_ifindex .
     */
    lacf_convert_zone_invalid_to_zero = 1 << 6,

    /**
     * For use only with lia_str_to_ipv6addrz_ex() and
     * lia_str_to_inetaddrz_ex(): ignores all non-empty zones.  This may
     * be useful when checking if a constrained string (like a
     * bt_hostname) is holding an IPv6 address with optional zone id,
     * where the zone id might not be numeric or the name of a current
     * interface.  This flag takes precedence over
     * ::lacf_convert_zone_ifname_to_ifindex and
     * ::lacf_convert_zone_invalid_to_zero .
     */
    lacf_convert_zone_to_zero = 1 << 7,

} lia_addr_conv_flags;

/** Bit field of ::lia_addr_conv_flags ORed together */
typedef uint32 lia_addr_conv_flags_bf;

/* ---------------------------------------------------------------------- */
/* bn_ipv4addr : a type which holds an IPv4 address.
 */

/**
 * Convert a string to a binary IPv4 address.
 * \param addr_str Source string
 * \retval ret_iaddr Computed IPv4 address
 */
int lia_str_to_ipv4addr(const char *addr_str,
                        bn_ipv4addr *ret_iaddr);

/**
 * Convert a string to a binary IPv4 address, with flags.
 *
 * Note that unlike lia_str_to_ipv4addr(), this function is able to
 * accept IPv4-mapped IPv6 addresses and convert them to IPv4 addresses.
 * By default this function only accepts IPv4 addresses.  To cause it to
 * also accept IPv4-mapped IPv6 addreses, specify flag
 * ::lacf_conv_ipv4_to_ipv6 .
 *
 * \param addr_str Source string
 * \param flags Conversion flags
 * \retval ret_iaddr Computed IPv4 address
 */
int lia_str_to_ipv4addr_ex(const char *addr_str,
                           lia_addr_conv_flags_bf flags,
                           bn_ipv4addr *ret_iaddr);

/**
 * Convert a 'struct in_addr' to a binary IPv4 address.
 *
 * \param inaddr Source 'struct in_addr'
 * \retval ret_iaddr Computed IPv4 address
 */
int lia_inaddr_to_ipv4addr(const struct in_addr *inaddr,
                           bn_ipv4addr *ret_iaddr);

/**
 * Convert a 'struct sockaddr' and associated length to a binary IPv4
 * address.
 *
 * \param saddr Source pointer to a 'struct sockaddr' .
 * The sockaddr must be of family AF_INET .
 * \param saddr_len length of saddr .
 * \retval ret_iaddr Computed IPv4 address
 */
int lia_saddr_to_ipv4addr(const struct sockaddr *saddr,
                          uint32 saddr_len,
                          bn_ipv4addr *ret_iaddr);

/**
 * Convert a 'struct sockaddr_in' to a binary IPv4 address.
 *
 * \param saddr_in Source pointer to a 'struct sockaddr_in' .
 * \retval ret_iaddr Computed IPv4 address
 */
int lia_saddrin_to_ipv4addr(const struct sockaddr_in *saddr_in,
                            bn_ipv4addr *ret_iaddr);

/**
 * Render a binary IPv4 address as a string.
 *
 * \param iaddr IPv4 address
 * \retval ret_str Rendered string
 */
int lia_ipv4addr_to_str(const bn_ipv4addr *iaddr,
                        char **ret_str);

/**
 * Render a binary IPv4 address as a string into a caller-supplied
 * buffer.
 *
 * \param iaddr IPv4 address
 * \retval ret_str_buf Buffer to render string into
 * \param str_buf_size Number of bytes available in ret_str_buf
 */
int lia_ipv4addr_to_str_buf(const bn_ipv4addr *iaddr,
                            char *ret_str_buf,
                            uint32 str_buf_size);


/**
 * Like lia_ipv4addr_to_str_buf(), but returns ret_str_buf on success,
 * and NULL on failure.  This can be useful when construting log
 * messages and the like.
 */
const char *lia_ipv4addr_to_str_buf_quick(const bn_ipv4addr *iaddr,
                                          char *ret_str_buf,
                                          uint32 str_buf_size);

/**
 * Convert a binary IPv4 address into an array of octets.  The octets
 * have the most significant byte first (network byte order).  For example,
 * IPv4(10.9.8.7) would convert to: {10, 9, 8, 7} .
 *
 * \param iaddr IPv4 address
 * \retval ret_ipv4addr_octets Buffer to hold rendered string
 */
int lia_ipv4addr_to_octets(const bn_ipv4addr *iaddr,
                           bn_ipv4addr_octets *ret_ipv4addr_octets);

/**
 * Convert an array of octets into a binary IPv4 address.
 * This is the inverse of lia_ipv4addr_to_octets().
 */
int lia_octets_to_ipv4addr(const bn_ipv4addr_octets *ipv4addr_octets,
                           bn_ipv4addr *ret_ipv4addr);

/**
 * Convert a binary IPv4 address to a binary IPv6 address which is the
 * IPv4-mapped IPv6 address of the specified IPv4 address.
 *
 * \param iaddr IPv4 address
 * \retval ret_iaddr IPv6 address (IPv4-mapped)
 */
int lia_ipv4addr_to_ipv6addr_map_v4mapped(const bn_ipv4addr *iaddr,
                                          bn_ipv6addr *ret_iaddr);


/**
 * Test if we can convert the given string into an IPv4 address.  This
 * is similar to using lia_str_to_ipv4addr(), but without the need for
 * an actual conversion.  This function does not return lc_err_bad_type
 * on invalid strings.
 *
 * \param addr_str Source string
 * \retval ret_valid true iff addr_str is a valid IPv4 address
 */
int lia_str_is_ipv4addr(const char *addr_str,
                              tbool *ret_valid);

#ifndef ADDR_UTILS_NO_DEPR
/* An alias, now deprecated */
#define lia_str_is_valid_ipv4addr lia_str_is_ipv4addr
#endif

/**
 * Test if we can convert the given string into an IPv4 address.
 * Returns ::false on all error conditions.
 *
 * \param addr_str Source string
 * \return true iff addr_str is a valid IPv4 address
 */
tbool lia_str_is_ipv4addr_quick(const char *addr_str);

/**
 * Test if the given string was converted to a bn_ipv4addr and back to a
 * string, would it be identical?  Does not return lc_err_bad_type on
 * invalid strings.
 *
 * \param addr_str Source string
 * \retval ret_valid true iff addr_str is a valid IPv4 address
 * \retval ret_canon true iff addr_str is a valid and canonical IPv4 address
 */
int lia_str_is_normalized_ipv4addr(const char *addr_str,
                                   tbool *ret_valid,
                                   tbool *ret_canon);

/**
 * Given an IPv4 address string, return the "canonical" (normalized)
 * form.  This is what would be given if the original string were
 * converted to binary form, and then back to a string.
 *
 * \param addr_str Source string
 * \retval ret_canon_str String which is the canonical representation of
 * addr_str, if it is a valid IPv4 address string.
 */
int lia_normalize_ipv4addr_str(const char *addr_str,
                               char **ret_canon_str);

/**
 * Given an IPv4 address string, return the "canonical" (normalized)
 * form into a caller-supplied buffer.  This is what would be given if
 * the original string were converted to binary form, and then back to a
 * string.
 *
 * \param addr_str Source string
 * \retval ret_canon_buf String which is the canonical representation of
 * addr_str, if it is a valid IPv4 address string.
 * \param canon_buf_size Number of bytes available in ret_canon_buf
 */
int lia_normalize_ipv4addr_str_buf(const char *addr_str,
                                   char *ret_canon_buf,
                                   uint32 canon_buf_size);


/**
 * Given a binary IPv4 address, return a 'struct in_addr' which holds
 * the same address.
 *
 * \param iaddr IPv4 address
 * \retval ret_inaddr struct in_addr which is filled in with the same
 * address
 */
int lia_ipv4addr_get_inaddr(const bn_ipv4addr *iaddr,
                            struct in_addr *ret_inaddr);

/**
 * Given a binary IPv4 address, return a const pointer to a 'struct
 * in_addr' which holds the same address.
 *
 * \param iaddr IPv4 address
 * \return const struct in_addr which points to the same address
 */
const struct in_addr *lia_ipv4addr_get_inaddr_quick(const bn_ipv4addr
                                                    *iaddr);

/**
 * Given a binary IPv4 address, return a pointer to a 'struct sockaddr'
 * which holds the same address.  inout_saddr_size works like in
 * accept(): on calling it must be the number of bytes available in
 * *ret_saddr, and on return it is the number of bytes used in
 * *ret_saddr .
 *
 * \param iaddr IPv4 address
 * \retval ret_saddr struct sockaddr which is filled in with the same
 * address
 * \param inout_saddr_size On calling the number of bytes available in
 * *ret_saddr, on return the number of bytes used in *ret_saddr
 */
int lia_ipv4addr_get_saddr(const bn_ipv4addr *iaddr,
                           struct sockaddr *ret_saddr,
                           uint32 *inout_saddr_size);


/*
 * --------------------------------------------------
 * Operations on one or more native bn_ipv4addr.
 * --------------------------------------------------
 */

/**
 * Test if two IPv4 addresses are identical.
 *
 * \param addr1 IPv4 address 1
 * \param addr2 IPv4 address 2
 * \retval ret_equal Result of comparison
 */
int lia_ipv4addr_equal(const bn_ipv4addr *addr1,
                       const bn_ipv4addr *addr2,
                       tbool *ret_equal);

/**
 * Test if two IPv4 addresses are identical.  Returns ::false if exactly
 * one address is NULL, and ::true if both addresses are NULL.
 *
 * \param addr1 IPv4 address 1
 * \param addr2 IPv4 address 2
 * \return Result of comparison
 */
tbool lia_ipv4addr_equal_quick(const bn_ipv4addr *addr1,
                               const bn_ipv4addr *addr2);

/**
 * Test if an IPv4 addresses is all zeros.  This may be useful after
 * doing a bitwise-operation, or to see if a setting is active.
 *
 * \param addr IPv4 address
 * \retval ret_zero true iff addr has all parts 0
 */
int lia_ipv4addr_is_zero(const bn_ipv4addr *addr,
                         tbool *ret_zero);

/**
 * Test if an IPv4 addresses is all zeros.  Returns ::false if ::addr is
 * NULL.  This may be useful after doing a bitwise-operation, or to see
 * if a setting is active.
 *
 * \param addr IPv4 address
 * \retval ret_zero true iff addr has all parts 0
 */
tbool lia_ipv4addr_is_zero_quick(const bn_ipv4addr *addr);


/* -------------------------- */
/* Common IPv4 address type checks */
/* -------------------------- */

/** Address in 0/8 (note contains 0.0.0.0) */
int lia_ipv4addr_is_zeronet(const bn_ipv4addr *iaddr,
                            tbool *ret_unspecified);
tbool lia_ipv4addr_is_zeronet_quick(const bn_ipv4addr *addr);

/** Address is 0.0.0.0 */
int lia_ipv4addr_is_unspecified(const bn_ipv4addr *iaddr,
                                tbool *ret_unspecified);
tbool lia_ipv4addr_is_unspecified_quick(const bn_ipv4addr *addr);

/** Address in 127/8 */
int lia_ipv4addr_is_loopback(const bn_ipv4addr *iaddr,
                             tbool *ret_loopback);
tbool lia_ipv4addr_is_loopback_quick(const bn_ipv4addr *addr);

/** Address is 127.0.0.1 */
int lia_ipv4addr_is_localhost(const bn_ipv4addr *iaddr,
                              tbool *ret_localhost);
tbool lia_ipv4addr_is_localhost_quick(const bn_ipv4addr *addr);

/** Address in 169.254/16 */
int lia_ipv4addr_is_linklocal(const bn_ipv4addr *iaddr,
                              tbool *ret_linklocal);
tbool lia_ipv4addr_is_linklocal_quick(const bn_ipv4addr *addr);

/** Address in 224/4 */
int lia_ipv4addr_is_multicast(const bn_ipv4addr *iaddr,
                              tbool *ret_multicast);
tbool lia_ipv4addr_is_multicast_quick(const bn_ipv4addr *addr);

/** Address in 240/4 (note contains 255.255.255.255) */
int lia_ipv4addr_is_badclass(const bn_ipv4addr *iaddr,
                             tbool *ret_badclass);
tbool lia_ipv4addr_is_badclass_quick(const bn_ipv4addr *addr);

/** Address is 255.255.255.255 */
int lia_ipv4addr_is_limited_broadcast(const bn_ipv4addr *iaddr,
                                      tbool *ret_unspecified);
tbool lia_ipv4addr_is_limited_broadcast_quick(const bn_ipv4addr *addr);

/** What type flags does the given address have */
int lia_addrtype_ipv4addr_type(const bn_ipv4addr *iaddr,
                               lia_addrtype_flags_bf *ret_match_mask);

/**
 * Given a set of address type flags, which of those does the given
 * address match?
 */
int lia_addrtype_ipv4addr_match(const bn_ipv4addr *iaddr,
                                lia_addrtype_flags_bf test_mask,
                                lia_addrtype_flags_bf *ret_match_mask);

/* ------------------------------------------------------------------------- */
/** Flags to be passed to bitwise operation functions, like
 *  lia_ipv4addr_bitop_ipv4addr().
 */
typedef enum {
    lbo_none        = 0,

    /**
     * Bitwise AND the two addressees.  Useful for taking IP and Mask
     * and returning the Network part of the address.
     */
    lbo_a_and_b     = 1 << 0,

    /**
     * Bitwise OR the two addressees.
     */
    lbo_a_or_b      = 1 << 1,

    /**
     * Bitwise XOR (exclusive OR) the two addressees.
     */
    lbo_a_xor_b     = 1 << 2,

    /**
     * Bitwise AND the first address with the bitwise NOT of the second
     * address.  Useful for taking an IP and a Mask, and returning the
     * Host portion of the address.
     */
    lbo_a_and_not_b = 1 << 3,

    /**
     * Bitwise OR the first address with the bitwise NOT of the second
     * address.  Useful for taking an IP and a Mask, and returning the
     * highest IP address in a network (the Broadcast address).
     */
    lbo_a_or_not_b  = 1 << 4,
} lia_bits_op;

/**
 * Perform a bitwise operation on two IPv4 addresses, and return the
 * resulting IPv4 address.  Note that it is supported to have
 * ::ret_result be the same as one of ::addr_a or ::addr_b .
 *
 * \param addr_a IPv4 address 1
 * \param addr_b IPv4 address 2
 * \param op Bitwise operation to perform
 * \retval ret_result Resulting IPv4 address
 *
 */
int lia_ipv4addr_bitop_ipv4addr(const bn_ipv4addr *addr_a,
                                const bn_ipv4addr *addr_b,
                                lia_bits_op op,
                                bn_ipv4addr *ret_result);

/**
 * Perform a bitwise operation on the given IPv4 address and the IPv4
 * address corresponding to the given mask length, and return the
 * resulting IPv4 address.  Note that it is supported to have
 * ::ret_result be the same as ::addr_a .
 *
 * \param addr_a IPv4 address 1
 * \param prefix_len Mask length, converted to an IPv4 address before usage
 * \param op Bitwise operation to perform
 * \retval ret_result Resulting IPv4 address
 *
 */
int lia_ipv4addr_bitop_masklen(const bn_ipv4addr *addr_a,
                               uint8 prefix_len,
                               lia_bits_op op,
                               bn_ipv4addr *ret_result);

/**
 * Compare two IPv4 addresses, and return -1, 0, or 1, if the first
 * address is less than, equal to, or greater than, the second address.
 *
 * \param addr1 IPv4 address 1
 * \param addr2 IPv4 address 2
 * \retval ret_cmp Comparison result (-1, 0 or 1)
 *
 */
int lia_ipv4addr_cmp(const bn_ipv4addr *addr1,
                     const bn_ipv4addr *addr2,
                     int *ret_cmp);


/**
 * Compare two IPv4 addresses, and return -1, 0, or 1, if the first
 * address is less than, equal to, or greater than, the second address.
 * Returns 0 if both addresses or NULL, -1 if only first address is
 * NULL, and 1 if only second address is NULL.
 *
 * \param addr1 IPv4 address 1
 * \param addr2 IPv4 address 2
 * \return Comparison result (-1, 0 or 1)
 *
 */
int32 lia_ipv4addr_cmp_quick(const bn_ipv4addr *addr1,
                             const bn_ipv4addr *addr2);


/* ---------- */
/* Masklen and maskspec conversions for IPv4 */
/* ---------- */

/**
 * Convert a mask length to an IPv4 address.
 *
 * Example: 24 -> IPv4(255.255.255.0)
 *
 * \param prefix_len Mask length
 * \retval ret_mask_addr Corresponding IPv4 address mask
 *
 */
int lia_masklen_to_ipv4addr(uint8 prefix_len,
                            bn_ipv4addr *ret_mask_addr);

/**
 * Convert a mask length to an IPv4 address string.
 *
 * Example: 24 -> "255.255.255.0"
 *
 * \param prefix_len Mask length
 * \retval ret_mask_str Corresponding IPv4 address string
 *
 */
int lia_masklen_to_ipv4addr_str(uint8 prefix_len,
                                char **ret_mask_str);

/**
 * Convert a mask length string to an IPv4 address.
 *
 * Example: "24" -> IPv4(255.255.255.0)
 *
 * \param prefix_len Mask length string
 * \retval ret_mask_addr Corresponding IPv4 address mask
 *
 */
int lia_masklen_str_to_ipv4addr(const char *prefix_len,
                                bn_ipv4addr *ret_mask_addr);

/**
 * Convert a mask length string to an IPv4 address string.
 *
 * Example: "24" -> "255.255.255.0"
 *
 * \param prefix_len Mask length string
 * \retval ret_mask_str Corresponding IPv4 address string
 *
 */
int lia_masklen_str_to_ipv4addr_str(const char *prefix_len,
                                    char **ret_mask_str);

/**
 * Convert a mask length string to a mask length.
 *
 * Example: "24" -> 24
 *
 * \param prefix_len Mask length string
 * \retval ret_prefix_len Corresponding mask length
 *
 */
int lia_ipv4addr_masklen_str_to_masklen(const char *prefix_len,
                                        uint8 *ret_prefix_len);

/**
 * Convert an IPv4 address to a mask length.  Returns lc_err_bad_type if
 * the address does not correspond to a valid network mask.
 *
 * Example: IPv4(255.255.255.0) -> 24
 *
 * \param mask_addr IPv4 address, which is a mask
 * \retval ret_prefix_len Corresponding mask length
 *
 */
int lia_ipv4addr_to_masklen(const bn_ipv4addr *mask_addr,
                            uint8 *ret_prefix_len);

/**
 * Determine the minimum masklen that can be used by a given IPv4 address.
 * This is the number of bits in an IPv4 address (32) minus the number of
 * trailing zero bits in the address.
 *
 * Note that on a netmask, this works the same as
 * lia_ipv4addr_to_masklen().  But additionally, we don't mind if
 * some of the bits higher than the lowest set were clear.
 */
int lia_ipv4addr_get_min_masklen(const bn_ipv4addr *addr,
                                 uint8 *ret_prefix_len);

/**
 * Convert a 'struct in_addr' to a mask length.  Returns lc_err_bad_type
 * if the address does not correspond to a valid network mask.
 *
 * Example: in_addr(255.255.255.0) -> 24
 *
 * \param mask_addr IPv4 address as a 'struct in_addr', which is a mask
 * \retval ret_prefix_len Corresponding mask length
 *
 */
int lia_inaddr_to_ipv4addr_masklen(const struct in_addr *mask_addr,
                                   uint8 *ret_prefix_len);

/**
 * Convert an IPv4 address to a mask length string.  Returns
 * lc_err_bad_type if the address does not correspond to a valid network
 * mask.
 *
 * Example: IPv4(255.255.255.0) -> "24"
 *
 * \param mask_addr IPv4 address, which is a mask
 * \retval ret_prefix_len_str Corresponding mask length string
 *
 */
int lia_ipv4addr_to_masklen_str(const bn_ipv4addr *mask_addr,
                                char **ret_prefix_len_str);

/**
 * Convert an IPv4 address string to a mask length.  Returns
 * lc_err_bad_type if the address does not correspond to a valid network
 * mask.
 *
 * Example: "255.255.255.0" -> 24
 *
 * \param mask_str IPv4 address string, which is a mask
 * \retval ret_prefix_len Corresponding mask length
 *
 */
int lia_ipv4addr_str_to_masklen(const char *mask_str,
                                uint8 *ret_prefix_len);

/**
 * Convert an IPv4 address string to a mask length string.  Returns
 * lc_err_bad_type if the address does not correspond to a valid network
 * mask.
 *
 * Example: "255.255.255.0" -> "24"
 *
 * \param mask_str IPv4 address string, which is a mask
 * \retval ret_prefix_len_str Corresponding mask length string
 *
 */
int lia_ipv4addr_str_to_masklen_str(const char *mask_str,
                                    char **ret_prefix_len_str);

/* ------------------------------------------------------------------------- */
/**
 * An IPv4 maskspec is either like "/24" or "255.255.255.0"
 * These are used in user data entry, such as in the CLI.
 */

/**
 * Convert an IPv4 address maskspec (/masklen or mask string) to a
 * mask length.  Returns lc_err_bad_type if the address does not
 * correspond to a valid maskspec.
 *
 * Example: "255.255.255.0" -> 24
 * Example: "/24"           -> 24
 *
 * \param maskspec IPv4 address maskspec string, which is a mask or /masklen
 * \retval ret_prefix_len Corresponding mask length
 *
 */
int lia_ipv4addr_maskspec_to_masklen(const char *maskspec,
                                     uint8 *ret_prefix_len);

/**
 * Convert an IPv4 address maskspec (/masklen or mask string) to a mask
 * length string.  Returns lc_err_bad_type if the address does not
 * correspond to a valid maskspec.
 *
 * Example: "255.255.255.0" -> "24"
 * Example: "/24"           -> "24"
 *
 * \param maskspec IPv4 address maskspec string, which is a mask or /masklen
 * \retval ret_prefix_len_str Corresponding mask length
 *
 */
int lia_ipv4addr_maskspec_to_masklen_str(const char *maskspec,
                                         char **ret_prefix_len_str);

/**
 * Convert an IPv4 address maskspec (/masklen or mask string) to an IPv4
 * address mask.  Returns lc_err_bad_type if the address does not
 * correspond to a valid maskspec.
 *
 * Example: "255.255.255.0" -> IPv4(255.255.255.0)
 * Example: "/24"           -> IPv4(255.255.255.0)
 *
 * \param maskspec IPv4 address maskspec string, which is a mask or /masklen
 * \retval ret_mask_addr Corresponding IPv4 address mask
 *
 */
int lia_ipv4addr_maskspec_to_ipv4addr(const char *maskspec,
                                      bn_ipv4addr *ret_mask_addr);

/**
 * Convert an IPv4 address maskspec (/masklen or mask string) to an IPv4
 * address string.  Returns lc_err_bad_type if the address does not
 * correspond to a valid maskspec.
 *
 * Example: "255.255.255.0" -> "255.255.255.0"
 * Example: "/24"           -> "255.255.255.0"
 *
 * \param maskspec IPv4 address maskspec string, which is a mask or /masklen
 * \retval ret_mask_str Corresponding IPv4 address mask string
 *
 */
int lia_ipv4addr_maskspec_to_ipv4addr_str(const char *maskspec,
                                          char **ret_mask_str);



/* ---------------------------------------------------------------------- */
/* bn_ipv4prefix : a type which holds an IPv4 address and mask length.
 */

/**
 * Convert a string to a binary IPv4 prefix.
 */
int lia_str_to_ipv4prefix(const char *addr_str,
                          bn_ipv4prefix *ret_iaddr);

int lia_ipv4addr_masklen_to_ipv4prefix(const bn_ipv4addr *addr,
                                       uint8 masklen,
                                       bn_ipv4prefix *ret_iaddr);

int lia_ipv4addr_mask_to_ipv4prefix(const bn_ipv4addr *addr,
                                    const bn_ipv4addr *mask,
                                    bn_ipv4prefix *ret_iaddr);

int lia_inaddr_masklen_to_ipv4prefix(const struct in_addr *addr,
                                     uint8 masklen,
                                     bn_ipv4prefix *ret_iaddr);

int lia_inaddr_mask_to_ipv4prefix(const struct in_addr *addr,
                                  const struct in_addr *mask,
                                  bn_ipv4prefix *ret_iaddr);

/*
 * The "masked" forms are like above, but explicitly zero any host bits
 * in the given address.
 */
int lia_ipv4addr_masklen_masked_to_ipv4prefix(const bn_ipv4addr *addr,
                                              uint8 masklen,
                                              bn_ipv4prefix *ret_iaddr);

int lia_ipv4addr_mask_masked_to_ipv4prefix(const bn_ipv4addr *addr,
                                           const bn_ipv4addr *mask,
                                           bn_ipv4prefix *ret_iaddr);

int lia_inaddr_masklen_masked_to_ipv4prefix(const struct in_addr *addr,
                                            uint8 masklen,
                                            bn_ipv4prefix *ret_iaddr);

int lia_inaddr_mask_masked_to_ipv4prefix(const struct in_addr *addr,
                                         const struct in_addr *mask,
                                         bn_ipv4prefix *ret_iaddr);


/**
 * Render a binary IPv4 prefix as a string.
 */
int lia_ipv4prefix_to_str(const bn_ipv4prefix *iaddr,
                          char **ret_str);

/** Like lia_ipv4prefix_to_str(), but into a caller-supplied buffer */
int lia_ipv4prefix_to_str_buf(const bn_ipv4prefix *iaddr,
                              char *ret_str_buf,
                              uint32 str_buf_size);

/**
 * Like lia_ipv4prefix_to_str_buf(), but returns ret_str_buf on success,
 * and NULL on failure.  This can be useful when construting log
 * messages and the like.
 */
const char *lia_ipv4prefix_to_str_buf_quick(const bn_ipv4prefix *iaddr,
                                            char *ret_str_buf,
                                            uint32 str_buf_size);

/**
 * Test if we can convert the given string into an IPv4 prefix.  This
 * is similar to using lia_str_to_ipv4prefix(), but without the need for
 * an actual conversion.  This function does not return lc_err_bad_type
 * on invalid strings.
 *
 * \param addr_str Source string
 * \retval ret_valid true iff addr_str is a valid IPv4 prefix
 */
int lia_str_is_ipv4prefix(const char *addr_str,
                          tbool *ret_valid);

#ifndef ADDR_UTILS_NO_DEPR
/* An alias, now deprecated */
#define lia_str_is_valid_ipv4prefix lia_str_is_ipv4prefix
#endif

/**
 * Test if we can convert the given string into an IPv4 prefix.
 * Returns ::false on all error conditions.
 *
 * \param addr_str Source string
 * \return true iff addr_str is a valid IPv4 prefix
 */
tbool lia_str_is_ipv4prefix_quick(const char *addr_str);

/**
 * If the given string was converted to a bn_ipv4prefix and back to a
 * string, would it be identical?  Does not return lc_err_bad_type on
 * invalid strings.
 */
int lia_str_is_normalized_ipv4prefix(const char *addr_str,
                                     tbool *ret_valid,
                                     tbool *ret_canon);

/**
 * Given an IPv4 address, return the "canonical" (normalized) form, the
 * would be given if it were converted to binary form, and then back to
 * a string.
 */
int lia_normalize_ipv4prefix_str(const char *addr_str,
                                 char **ret_canon_str);

/** Like lia_normalize_ipv4prefix_str(), but into a caller-supplied buffer */
int lia_normalize_ipv4prefix_str_buf(const char *addr_str,
                                     char *ret_canon_buf,
                                     uint32 canon_buf_size);

int lia_ipv4prefix_get_ipv4addr(const bn_ipv4prefix *iaddr,
                                bn_ipv4addr *ret_iaddr);

int lia_ipv4prefix_get_ipv4addr_str(const bn_ipv4prefix *iaddr,
                                    char **ret_addr_str);

int lia_ipv4prefix_get_masklen(const bn_ipv4prefix *iaddr,
                               uint8 *ret_prefix_len);

int lia_ipv4prefix_get_masklen_str(const bn_ipv4prefix *iaddr,
                                   char **ret_masklen_str);

int lia_ipv4prefix_get_mask(const bn_ipv4prefix *iaddr,
                            bn_ipv4addr *ret_iaddr);

int lia_ipv4prefix_get_mask_str(const bn_ipv4prefix *iaddr,
                                char **ret_mask_str);

/*
 * --------------------------------------------------
 * Operations on one or more native bn_ipv4prefix .
 * --------------------------------------------------
 */

int lia_ipv4prefix_equal(const bn_ipv4prefix *addr1,
                         const bn_ipv4prefix *addr2,
                         tbool *ret_equal);

tbool lia_ipv4prefix_equal_quick(const bn_ipv4prefix *addr1,
                                 const bn_ipv4prefix *addr2);

int lia_ipv4prefix_is_zero(const bn_ipv4prefix *addr,
                           tbool *ret_zero);

tbool lia_ipv4prefix_is_zero_quick(const bn_ipv4prefix *addr);


/** -1,0,1 if addr1 <,==,> addr2 */
int lia_ipv4prefix_cmp(const bn_ipv4prefix *addr1,
                       const bn_ipv4prefix *addr2,
                       int *ret_cmp);

int32 lia_ipv4prefix_cmp_quick(const bn_ipv4prefix *addr1,
                               const bn_ipv4prefix *addr2);

int lia_ipv4prefix_contains_ipv4addr(const bn_ipv4prefix *pfx,
                                     const bn_ipv4addr *addr,
                                     tbool *ret_contains);


/* ---------------------------------------------------------------------- */
/* bn_ipv6addr : a type which holds an IPv6 address.
 */

/**
 * Convert a string to a binary IPv6 address.
 */
int lia_str_to_ipv6addr(const char *addr_str,
                        bn_ipv6addr *ret_iaddr);

/**
 * Convert a string to a binary IPv6 address, with flags.  Note that
 * unlike lia_str_to_ipv6addr(), this function is able to accept IPv4
 * addresses and convert them to IPv4-mapped IPv6 addresses.  By default
 * this function does not accept IPv4 addresses, and will only do so if
 * given ::lacf_conv_ipv4_to_ipv6 .
 */
int lia_str_to_ipv6addr_ex(const char *addr_str,
                           lia_addr_conv_flags_bf flags,
                           bn_ipv6addr *ret_iaddr);

int lia_in6addr_to_ipv6addr(const struct in6_addr *inaddr,
                           bn_ipv6addr *ret_iaddr);

/** Works only for sockaddr's of family AF_INET6 . */
int lia_saddr_to_ipv6addr(const struct sockaddr *saddr,
                          uint32 saddr_len,
                          bn_ipv6addr *ret_iaddr);

int lia_saddrin6_to_ipv6addr(const struct sockaddr_in6 *saddr_in,
                             bn_ipv6addr *ret_iaddr);


/**
 * Render a binary IPv6 address as a string.
 */
int lia_ipv6addr_to_str(const bn_ipv6addr *iaddr,
                        char **ret_str);

/** Like lia_ipv6addr_to_str(), but into a caller-supplied buffer */
int lia_ipv6addr_to_str_buf(const bn_ipv6addr *iaddr,
                            char *ret_str_buf,
                            uint32 str_buf_size);

/**
 * Like lia_ipv6addr_to_str_buf(), but returns ret_str_buf on success,
 * and NULL on failure.  This can be useful when construting log
 * messages and the like.
 */
const char *lia_ipv6addr_to_str_buf_quick(const bn_ipv6addr *iaddr,
                                          char *ret_str_buf,
                                          uint32 str_buf_size);

/**
 * Convert IPv6(fd55:4100:13cc:0001:021c:23ff:fec1:4fb8) ->
 *             {fd, 55, 41, 00, 13 ...., 4f, b8}
 */
int lia_ipv6addr_to_octets(const bn_ipv6addr *iaddr,
                           bn_ipv6addr_octets *ret_ipv6addr_octets);


/**
 * Convert an array of octets into a binary IPv6 address.
 * This is the inverse of lia_ipv6addr_to_octets().
 */
int lia_octets_to_ipv6addr(const bn_ipv6addr_octets *ipv6addr_octets,
                           bn_ipv6addr *ret_ipv6addr);

/**
 * Test if we can convert the given string into an IPv6 address.  This
 * is similar to using lia_str_to_ipv6addr(), but without the need for
 * an actual conversion.  This function does not return lc_err_bad_type
 * on invalid strings.
 *
 * \param addr_str Source string
 * \retval ret_valid true iff addr_str is a valid IPv6 address
 */
int lia_str_is_ipv6addr(const char *addr_str,
                        tbool *ret_valid);

#ifndef ADDR_UTILS_NO_DEPR
/* An alias, now deprecated */
#define lia_str_is_valid_ipv6addr lia_str_is_ipv6addr
#endif

/**
 * Test if we can convert the given string into an IPv6 address.
 * Returns ::false on all error conditions.
 *
 * \param addr_str Source string
 * \return true iff addr_str is a valid IPv6 address
 */
tbool lia_str_is_ipv6addr_quick(const char *addr_str);

/**
 * If the given string was converted to a bn_ipv6addr and back to a
 * string, would it be identical?  Does not return lc_err_bad_type on
 * invalid strings.
 */
int lia_str_is_normalized_ipv6addr(const char *addr_str,
                                   tbool *ret_valid,
                                   tbool *ret_canon);

/**
 * Given an IPv6 address, return the "canonical" (normalized) form, the
 * would be given if it were converted to binary form, and then back to
 * a string.
 */
int lia_normalize_ipv6addr_str(const char *addr_str,
                               char **ret_canon_str);

/** Like lia_normalize_ipv6addr_str(), but into a caller-supplied buffer */
int lia_normalize_ipv6addr_str_buf(const char *addr_str,
                                   char *ret_canon_buf,
                                   uint32 canon_buf_size);


int lia_ipv6addr_get_in6addr(const bn_ipv6addr *iaddr,
                             struct in6_addr *ret_inaddr);

/**
 * Given a binary IPv6 address, return a const pointer to a 'struct
 * in6_addr' which holds the same address.
 *
 * \param iaddr IPv6 address
 * \return const struct in6_addr which points to the same address
 */
const struct in6_addr *lia_ipv6addr_get_in6addr_quick(const bn_ipv6addr
                                                      *iaddr);

/**
 * inout_saddr_size works like in accept(): on calling it must be the
 * size of *ret_saddr, and on return it is the number of bytes used in
 * *ret_saddr .
 */
int lia_ipv6addr_get_saddr(const bn_ipv6addr *iaddr,
                           struct sockaddr *ret_saddr,
                           uint32 *inout_saddr_size);

/**
 * If the given binary IPv6 address is an IPv4-mapped IPv6 address,
 * return that address.  Otherwise, return lc_err_bad_type.
 */
int lia_ipv6addr_get_ipv4addr_unmap_v4mapped(const bn_ipv6addr *iaddr,
                                             bn_ipv4addr *ret_v4addr);

/*
 * --------------------------------------------------
 * Operations on one or more native bn_ipv6addr.
 * --------------------------------------------------
 */

int lia_ipv6addr_equal(const bn_ipv6addr *addr1,
                       const bn_ipv6addr *addr2,
                       tbool *ret_equal);

tbool lia_ipv6addr_equal_quick(const bn_ipv6addr *addr1,
                               const bn_ipv6addr *addr2);

int lia_ipv6addr_is_zero(const bn_ipv6addr *addr,
                         tbool *ret_zero);

tbool lia_ipv6addr_is_zero_quick(const bn_ipv6addr *addr);


/* -------------------------- */
/* Common IPv6 address type checks */
/* -------------------------- */

/** Address is :: , Unspecified */
int lia_ipv6addr_is_unspecified(const bn_ipv6addr *iaddr,
                                tbool *ret_unspecified);
tbool lia_ipv6addr_is_unspecified_quick(const bn_ipv6addr *iaddr);

/** Address is ::1 , Loopback */
int lia_ipv6addr_is_loopback(const bn_ipv6addr *iaddr, tbool *ret_loopback);
tbool lia_ipv6addr_is_loopback_quick(const bn_ipv6addr *iaddr);

/** Address in ::ffff:0:0/96 , IPv4-Mapped IPv6 */
int lia_ipv6addr_is_v4mapped(const bn_ipv6addr *iaddr,
                             tbool *ret_v4mapped);
tbool lia_ipv6addr_is_v4mapped_quick(const bn_ipv6addr *iaddr);

/**
 * Address in ::<ipv4-address>/96 .  IPv4-Compatible IPv6 Address.
 * This is a deprecated address type.
 */
int lia_ipv6addr_is_v4compat(const bn_ipv6addr *iaddr, tbool *ret_v4compat);
tbool lia_ipv6addr_is_v4compat_quick(const bn_ipv6addr *iaddr);

/** Address in fe80::/10 , Link-Local unicast. */
int lia_ipv6addr_is_linklocal(const bn_ipv6addr *iaddr, tbool *ret_linklocal);
tbool lia_ipv6addr_is_linklocal_quick(const bn_ipv6addr *iaddr);

/** Address in ff00::/8 , Multicast */
int lia_ipv6addr_is_multicast(const bn_ipv6addr *iaddr, tbool *ret_multicast);
tbool lia_ipv6addr_is_multicast_quick(const bn_ipv6addr *iaddr);

/**
 * Address in fec0::/10 . Site-Local scoped address.  This is a
 * deprecated address type.
 */
int lia_ipv6addr_is_sitelocal(const bn_ipv6addr *iaddr, tbool *ret_sitelocal);
tbool lia_ipv6addr_is_sitelocal_quick(const bn_ipv6addr *iaddr);

/** What type flags does the given address have */
int lia_addrtype_ipv6addr_type(const bn_ipv6addr *iaddr,
                               lia_addrtype_flags_bf *ret_match_mask);

/**
 * Given a set of address type flags, which of those does the given
 * address match?
 */
int lia_addrtype_ipv6addr_match(const bn_ipv6addr *iaddr,
                                lia_addrtype_flags_bf test_mask,
                                lia_addrtype_flags_bf *ret_match_mask);

/**
 * Note that it is supported to have the result be the same as one of
 * the addresses.
 */
int lia_ipv6addr_bitop_ipv6addr(const bn_ipv6addr *addr_a,
                                const bn_ipv6addr *addr_b,
                                lia_bits_op op,
                                bn_ipv6addr *ret_result);

int lia_ipv6addr_bitop_masklen(const bn_ipv6addr *addr_a,
                               uint8 prefix_len,
                               lia_bits_op op,
                               bn_ipv6addr *ret_result);

/** -1,0,1 if addr1 <,==,> addr2 */
int lia_ipv6addr_cmp(const bn_ipv6addr *addr1,
                     const bn_ipv6addr *addr2,
                     int *ret_cmp);

int32 lia_ipv6addr_cmp_quick(const bn_ipv6addr *addr1,
                             const bn_ipv6addr *addr2);


/* ---------- */
/* Masklen and maskspec conversions for IPv6 */
/* ---------- */

/** 64 -> IPv6(ffff:ffff:ffff:ffff::) */
int lia_masklen_to_ipv6addr(uint8 prefix_len,
                            bn_ipv6addr *ret_mask_addr);

/** 64 -> "ffff:ffff:ffff:ffff::" */
int lia_masklen_to_ipv6addr_str(uint8 prefix_len,
                                char **ret_mask_str);

/** "64" -> IPv6(ffff:ffff:ffff:ffff::) */
int lia_masklen_str_to_ipv6addr(const char *prefix_len,
                                bn_ipv6addr *ret_mask_addr);

/** "64" -> "ffff:ffff:ffff:ffff::" */
int lia_masklen_str_to_ipv6addr_str(const char *prefix_len,
                                    char **ret_mask_str);

/** "64" -> 64 */
int lia_ipv6addr_masklen_str_to_masklen(const char *prefix_len,
                                        uint8 *ret_prefix_len);

/** IPv6(ffff:ffff:ffff:ffff::) -> 64 */
int lia_ipv6addr_to_masklen(const bn_ipv6addr *mask_addr,
                            uint8 *ret_prefix_len);

/**
 * Determine the minimum masklen that can be used by a given IPv6 address.
 * This is the number of bits in an IPv4 address (128) minus the number of
 * trailing zero bits in the address.
 *
 * Note that on a netmask, this works the same as
 * lia_ipv6addr_to_masklen().  But additionally, we don't mind if
 * some of the bits higher than the lowest set were clear.
 */
int lia_ipv6addr_get_min_masklen(const bn_ipv6addr *addr,
                                 uint8 *ret_prefix_len);

/** in_addr(255.255.255.0) -> 24 */
int lia_in6addr_to_ipv6addr_masklen(const struct in6_addr *mask_addr,
                                    uint8 *ret_prefix_len);

/** IPv6(ffff:ffff:ffff:ffff::) -> "64" */
int lia_ipv6addr_to_masklen_str(const bn_ipv6addr *mask_addr,
                                char **ret_prefix_len_str);

/** "ffff:ffff:ffff:ffff::" -> 64 */
int lia_ipv6addr_str_to_masklen(const char *mask_str,
                                uint8 *ret_prefix_len);

/** "ffff:ffff:ffff:ffff::" -> "64" */
int lia_ipv6addr_str_to_masklen_str(const char *mask_str,
                                    char **ret_prefix_len_str);

/*
 * An IPv6 maskspec is either like "/64" or "ffff:ffff:ffff:ffff::"
 * These are used in user data entry, like in the CLI.
 */

/** "/48" or "ffff:ffff:ffff::" -> 48 */
int lia_ipv6addr_maskspec_to_masklen(const char *maskspec,
                                     uint8 *ret_prefix_len);

/* REPL: lc_mask_or_masklen_to_masklen_str() */
/** "/48" or "ffff:ffff:ffff::" -> "48" */
int lia_ipv6addr_maskspec_to_masklen_str(const char *maskspec,
                                         char **ret_prefix_len_str);

/** "/48" or "ffff:ffff:ffff::" -> IPv6(ffff:ffff:ffff::) */
int lia_ipv6addr_maskspec_to_ipv6addr(const char *maskspec,
                                      bn_ipv6addr *ret_mask_addr);


/** "/48" or "ffff:ffff:ffff::" -> "ffff:ffff:ffff::" */
int lia_ipv6addr_maskspec_to_ipv6addr_str(const char *maskspec,
                                          char **ret_mask_str);



/* ---------------------------------------------------------------------- */
/* bn_ipv6addrz : a type which holds an IPv6 address and a zone id.
 */

/**
 * Convert a string to a binary IPv6 address with zone id.
 *
 * Please note that only an ifindex, not an interface name, is 
 * accepted for the zone id.  If you might have an interface name
 * after the '%' sign, consider using lia_str_to_ipv6addrz_ex() 
 * with the ::lacf_convert_zone_ifname_to_ifindex flag.
 */
int lia_str_to_ipv6addrz(const char *addr_str,
                         bn_ipv6addrz *ret_iaddr);

int lia_str_to_ipv6addrz_ex(const char *addr_str,
                            lia_addr_conv_flags_bf flags,
                            bn_ipv6addrz *ret_iaddr);

int lia_in6addr_zoneid_to_ipv6addrz(const struct in6_addr *inaddr,
                                    uint32 zone_id,
                                    bn_ipv6addrz *ret_iaddr);

int lia_ipv6addr_zoneid_to_ipv6addrz(const bn_ipv6addr *iaddr,
                                     uint32 zone_id,
                                     bn_ipv6addrz *ret_iaddr);

/** Works only for sockaddr's of family AF_INET6 . */
int lia_saddr_to_ipv6addrz(const struct sockaddr *saddr,
                          uint32 saddr_len,
                          bn_ipv6addrz *ret_iaddr);

int lia_saddrin6_to_ipv6addrz(const struct sockaddr_in6 *saddr_in,
                             bn_ipv6addrz *ret_iaddr);

/**
 * Render a binary IPv6 address as a string.
 */
int lia_ipv6addrz_to_str(const bn_ipv6addrz *iaddr,
                        char **ret_str);

/** Like lia_ipv6addrz_to_str(), but into a caller-supplied buffer */
int lia_ipv6addrz_to_str_buf(const bn_ipv6addrz *iaddr,
                             char *ret_str_buf,
                             uint32 str_buf_size);

/**
 * Like lia_ipv6addrz_to_str_buf(), but returns ret_str_buf on success,
 * and NULL on failure.  This can be useful when construting log
 * messages and the like.
 */
const char *lia_ipv6addrz_to_str_buf_quick(const bn_ipv6addrz *iaddr,
                                           char *ret_str_buf,
                                           uint32 str_buf_size);

/**
 * Test if we can convert the given string into an IPv6 address with
 * zone id.  This is similar to using lia_str_to_ipv6addrz(), but
 * without the need for an actual conversion.  This function does not
 * return lc_err_bad_type on invalid strings.
 *
 * \param addr_str Source string
 * \retval ret_valid true iff addr_str is a valid IPv6 address with zone id.
 */
int lia_str_is_ipv6addrz(const char *addr_str,
                         tbool *ret_valid);

#ifndef ADDR_UTILS_NO_DEPR
/* An alias, now deprecated */
#define lia_str_is_valid_ipv6addrz lia_str_is_ipv6addrz
#endif

/**
 * Test if we can convert the given string into an IPv6 address with zone id.
 * Returns ::false on all error conditions.
 *
 * \param addr_str Source string
 * \return true iff addr_str is a valid IPv6 address with zone id
 */
tbool lia_str_is_ipv6addrz_quick(const char *addr_str);

/**
 * If the given string was converted to a bn_ipv6addrz and back to a
 * string, would it be identical?  Does not return lc_err_bad_type on
 * invalid strings.
 */
int lia_str_is_normalized_ipv6addrz(const char *addr_str,
                                    tbool *ret_valid,
                                    tbool *ret_canon);

/**
 * Given an IPv6 address, return the "canonical" (normalized) form, the
 * would be given if it were converted to binary form, and then back to
 * a string.
 */
int lia_normalize_ipv6addrz_str(const char *addr_str,
                               char **ret_canon_str);

/** Like lia_normalize_ipv6addrz_str(), but into a caller-supplied buffer */
int lia_normalize_ipv6addrz_str_buf(const char *addr_str,
                                    char *ret_canon_buf,
                                    uint32 canon_buf_size);

int lia_ipv6addrz_get_zoneid(const bn_ipv6addrz *iaddr,
                             uint32 *ret_zone_id);

uint32 lia_ipv6addrz_get_zoneid_quick(const bn_ipv6addrz *iaddr);

int lia_ipv6addrz_get_ipv6addr_zoneid(const bn_ipv6addrz *iaddr,
                                      bn_ipv6addr *ret_iaddr,
                                      uint32 *ret_zone_id);

int lia_ipv6addrz_get_in6addr_zoneid(const bn_ipv6addrz *iaddr,
                                     struct in6_addr *ret_inaddr,
                                     uint32 *ret_zone_id);

const struct in6_addr *lia_ipv6addrz_get_in6addr_quick(const bn_ipv6addrz
                                                       *iaddr);


/**
 * inout_saddr_size works like in accept(): on calling it must be the
 * size of *ret_saddr, and on return it is the number of bytes used in
 * *ret_saddr .
 */
int lia_ipv6addrz_get_saddr(const bn_ipv6addrz *iaddr,
                            struct sockaddr *ret_saddr,
                            uint32 *inout_saddr_size);


/**
 * If the given binary IPv6 address is an IPv4-mapped IPv6 address,
 * return that address.  Otherwise, return lc_err_bad_type.
 */
int lia_ipv6addrz_get_ipv4addr_unmap_v4mapped_zoneid(const bn_ipv6addrz *iaddr,
                                                     bn_ipv4addr *ret_v4addr,
                                                     uint32 *ret_zone_id);


/*
 * --------------------------------------------------
 * Operations on one or more native bn_ipv6addrz.
 * --------------------------------------------------
 */

int lia_ipv6addrz_equal(const bn_ipv6addrz *addr1,
                       const bn_ipv6addrz *addr2,
                       tbool *ret_equal);

tbool lia_ipv6addrz_equal_quick(const bn_ipv6addrz *addr1,
                               const bn_ipv6addrz *addr2);

int lia_ipv6addrz_is_zero(const bn_ipv6addrz *addr,
                         tbool *ret_zero);

tbool lia_ipv6addrz_is_zero_quick(const bn_ipv6addrz *addr);

/* -------------------------- */
/* Common IPv6 address type checks */
/* -------------------------- */

/** Address is :: , Unspecified */
int lia_ipv6addrz_is_unspecified(const bn_ipv6addrz *iaddr,
                                tbool *ret_unspecified);
tbool lia_ipv6addrz_is_unspecified_quick(const bn_ipv6addrz *iaddr);

/** Address is ::1 , Loopback */
int lia_ipv6addrz_is_loopback(const bn_ipv6addrz *iaddr, tbool *ret_loopback);
tbool lia_ipv6addrz_is_loopback_quick(const bn_ipv6addrz *iaddr);

/** Address in ::ffff:0:0/96 , IPv4-Mapped IPv6 */
int lia_ipv6addrz_is_v4mapped(const bn_ipv6addrz *iaddr,
                             tbool *ret_v4mapped);
tbool lia_ipv6addrz_is_v4mapped_quick(const bn_ipv6addrz *iaddr);

/**
 * Address in ::<ipv4-address>/96 .  IPv4-Compatible IPv6 Address.
 * This is a deprecated address type.
 */
int lia_ipv6addrz_is_v4compat(const bn_ipv6addrz *iaddr, tbool *ret_v4compat);
tbool lia_ipv6addrz_is_v4compat_quick(const bn_ipv6addrz *iaddr);

/** Address in fe80::/10 , Link-Local unicast. */
int lia_ipv6addrz_is_linklocal(const bn_ipv6addrz *iaddr, tbool *ret_linklocal);
tbool lia_ipv6addrz_is_linklocal_quick(const bn_ipv6addrz *iaddr);

/** Address in ff00::/8 , Multicast */
int lia_ipv6addrz_is_multicast(const bn_ipv6addrz *iaddr, tbool *ret_multicast);
tbool lia_ipv6addrz_is_multicast_quick(const bn_ipv6addrz *iaddr);

/**
 * Address in fec0::/10 . Site-Local scoped address.  This is a
 * deprecated address type.
 */
int lia_ipv6addrz_is_sitelocal(const bn_ipv6addrz *iaddr, tbool *ret_sitelocal);
tbool lia_ipv6addrz_is_sitelocal_quick(const bn_ipv6addrz *iaddr);


/** What type flags does the given address have */
int lia_addrtype_ipv6addrz_type(const bn_ipv6addrz *iaddr,
                               lia_addrtype_flags_bf *ret_match_mask);

/**
 * Given a set of address type flags, which of those does the given
 * address match?
 */
int lia_addrtype_ipv6addrz_match(const bn_ipv6addrz *iaddr,
                                lia_addrtype_flags_bf test_mask,
                                lia_addrtype_flags_bf *ret_match_mask);

/** -1,0,1 if addr1 <,==,> addr2 */
int lia_ipv6addrz_cmp(const bn_ipv6addrz *addr1,
                      const bn_ipv6addrz *addr2,
                      int *ret_cmp);

int32 lia_ipv6addrz_cmp_quick(const bn_ipv6addrz *addr1,
                              const bn_ipv6addrz *addr2);


/* ---------------------------------------------------------------------- */
/* bn_ipv6prefix : a type which holds an IPv6 address and mask length.
 */

/**
 * Convert a string to a binary IPv6 prefix.
 */
int lia_str_to_ipv6prefix(const char *addr_str,
                          bn_ipv6prefix *ret_iaddr);

int lia_ipv6addr_masklen_to_ipv6prefix(const bn_ipv6addr *addr,
                                       uint8 masklen,
                                       bn_ipv6prefix *ret_iaddr);

int lia_ipv6addr_mask_to_ipv6prefix(const bn_ipv6addr *addr,
                                    const bn_ipv6addr *mask,
                                    bn_ipv6prefix *ret_iaddr);

int lia_in6addr_masklen_to_ipv6prefix(const struct in6_addr *addr,
                                      uint8 masklen,
                                      bn_ipv6prefix *ret_iaddr);

int lia_in6addr_mask_to_ipv6prefix(const struct in6_addr *addr,
                                   const struct in6_addr *mask,
                                   bn_ipv6prefix *ret_iaddr);

/**
 * The "masked" forms are like above, but explicitly zero any host bits
 * in the given address.
 */
int lia_ipv6addr_masklen_masked_to_ipv6prefix(const bn_ipv6addr *addr,
                                              uint8 masklen,
                                              bn_ipv6prefix *ret_iaddr);

int lia_ipv6addr_mask_masked_to_ipv6prefix(const bn_ipv6addr *addr,
                                           const bn_ipv6addr *mask,
                                           bn_ipv6prefix *ret_iaddr);

int lia_in6addr_masklen_masked_to_ipv6prefix(const struct in6_addr *addr,
                                            uint8 masklen,
                                            bn_ipv6prefix *ret_iaddr);

int lia_in6addr_mask_masked_to_ipv6prefix(const struct in6_addr *addr,
                                         const struct in6_addr *mask,
                                         bn_ipv6prefix *ret_iaddr);

/**
 * Render a binary IPv6 prefix as a string.
 */
int lia_ipv6prefix_to_str(const bn_ipv6prefix *iaddr,
                          char **ret_str);

/** Like lia_ipv6prefix_to_str(), but into a caller-supplied buffer */
int lia_ipv6prefix_to_str_buf(const bn_ipv6prefix *iaddr,
                              char *ret_str_buf,
                              uint32 str_buf_size);

/**
 * Like lia_ipv6prefix_to_str_buf(), but returns ret_str_buf on success,
 * and NULL on failure.  This can be useful when construting log
 * messages and the like.
 */
const char *lia_ipv6prefix_to_str_buf_quick(const bn_ipv6prefix *iaddr,
                                            char *ret_str_buf,
                                            uint32 str_buf_size);


/**
 * Test if we can convert the given string into an IPv6 prefix.  This
 * is similar to using lia_str_to_ipv6prefix(), but without the need for
 * an actual conversion.  This function does not return lc_err_bad_type
 * on invalid strings.
 *
 * \param addr_str Source string
 * \retval ret_valid true iff addr_str is a valid IPv6 prefix
 */
int lia_str_is_ipv6prefix(const char *addr_str,
                          tbool *ret_valid);

#ifndef ADDR_UTILS_NO_DEPR
/* An alias, now deprecated */
#define lia_str_is_valid_ipv6prefix lia_str_is_ipv6prefix
#endif

/**
 * Test if we can convert the given string into an IPv6 prefix.
 * Returns ::false on all error conditions.
 *
 * \param addr_str Source string
 * \return true iff addr_str is a valid IPv6 prefix
 */
tbool lia_str_is_ipv6prefix_quick(const char *addr_str);

/**
 * If the given string was converted to a bn_ipv6prefix and back to a
 * string, would it be identical?  Does not return lc_err_bad_type on
 * invalid strings.
 */
int lia_str_is_normalized_ipv6prefix(const char *addr_str,
                                     tbool *ret_valid,
                                     tbool *ret_canon);

/**
 * Given an IPv6 prefix, return the "canonical" (normalized) form, the
 * would be given if it were converted to binary form, and then back to
 * a string.
 */
int lia_normalize_ipv6prefix_str(const char *addr_str,
                               char **ret_canon_str);

/** Like lia_normalize_ipv6prefix_str(), but into a caller-supplied buffer */
int lia_normalize_ipv6prefix_str_buf(const char *addr_str,
                                     char *ret_canon_buf,
                                     uint32 canon_buf_size);

int lia_ipv6prefix_get_ipv6addr(const bn_ipv6prefix *iaddr,
                                bn_ipv6addr *ret_iaddr);

int lia_ipv6prefix_get_ipv6addr_str(const bn_ipv6prefix *iaddr,
                                    char **ret_addr_str);

int lia_ipv6prefix_get_masklen(const bn_ipv6prefix *iaddr,
                               uint8 *ret_prefix_len);

int lia_ipv6prefix_get_masklen_str(const bn_ipv6prefix *iaddr,
                                   char **ret_masklen_str);

int lia_ipv6prefix_get_mask(const bn_ipv6prefix *iaddr,
                            bn_ipv6addr *ret_iaddr);

int lia_ipv6prefix_get_mask_str(const bn_ipv6prefix *iaddr,
                                char **ret_mask_str);

/*
 * --------------------------------------------------
 * Operations on one or more native bn_ipv6prefix .
 * --------------------------------------------------
 */

int lia_ipv6prefix_equal(const bn_ipv6prefix *addr1,
                         const bn_ipv6prefix *addr2,
                         tbool *ret_equal);

tbool lia_ipv6prefix_equal_quick(const bn_ipv6prefix *addr1,
                                 const bn_ipv6prefix *addr2);

int lia_ipv6prefix_is_zero(const bn_ipv6prefix *addr,
                           tbool *ret_zero);

tbool lia_ipv6prefix_is_zero_quick(const bn_ipv6prefix *addr);

/** -1,0,1 if addr1 <,==,> addr2 */
int lia_ipv6prefix_cmp(const bn_ipv6prefix *addr1,
                       const bn_ipv6prefix *addr2,
                       int *ret_cmp);

int32 lia_ipv6prefix_cmp_quick(const bn_ipv6prefix *addr1,
                               const bn_ipv6prefix *addr2);

int lia_ipv6prefix_contains_ipv6addr(const bn_ipv6prefix *pfx,
                                     const bn_ipv6addr *addr,
                                     tbool *ret_contains);


/* ---------------------------------------------------------------------- */
/* bn_inetaddr : a type which holds an IPv4 or IPv6 address.
 */

/**
 * First tries conversion to IPv4 inetaddr, and then to IPv6 inetadddr.
 * No mapping is done.
 */
int lia_str_to_inetaddr(const char *addr_str,
                        bn_inetaddr *ret_iaddr);

/**
 * Convert a string to a binary inetaddress, with flags.  Note that
 * unlike lia_str_to_inetaddr(), this function is able to accept IPv4
 * addresses and convert them to IPv4-mapped IPv6 addresses.  By default
 * this function accepts both IPv4 and IPv6 addresses, and does not do
 * any mapping.  It can be told to only accept IPv4 or only IPv6, and
 * also to map IPv4 to IPv4-mapped IPv6.
 */

int lia_str_to_inetaddr_ex(const char *addr_str,
                           lia_addr_conv_flags_bf flags,
                           bn_inetaddr *ret_iaddr);

int lia_inaddr_to_inetaddr(const struct in_addr *inaddr,
                           bn_inetaddr *ret_iaddr);

int lia_in6addr_to_inetaddr(const struct in6_addr *inaddr,
                           bn_inetaddr *ret_iaddr);

int lia_ipv4addr_to_inetaddr(const bn_ipv4addr *iaddr,
                             bn_inetaddr *ret_iaddr);

/**
 * Make an inetaddr which is of family AF_INET6, corresponding to the
 * IPv4-mapped IPv6 address of the specified IPv4 address.
 */
int lia_ipv4addr_to_inetaddr_map_v4mapped(const bn_ipv4addr *iaddr,
                                          bn_inetaddr *ret_iaddr);

/**
 * Make an inetaddr which is of family AF_INET6.
 */
int lia_ipv6addr_to_inetaddr(const bn_ipv6addr *iaddr,
                             bn_inetaddr *ret_iaddr);

/**
 * Make an inetaddr which is of family AF_INET for IPv4 mapped IPv6
 * addresses, and otherwise of family IPv6.
 */
int lia_ipv6addr_to_inetaddr_unmap_v4mapped(const bn_ipv6addr *iaddr,
                                            bn_inetaddr *ret_iaddr);

/** Works only for sockaddr's of family AF_INET or AF_INET6 . */
int lia_saddr_to_inetaddr(const struct sockaddr *saddr,
                          uint32 saddr_len,
                          bn_inetaddr *ret_iaddr);

/**
 * Converts an IPv4 inetaddr into an IPv6 inetaddr, using an IPv4-mapped
 * IPv6 address.  This would mean converting IPv4 10.1.2.3 -> IPv6
 * ::ffff:10.1.2.3 .  If the inetaddr is not IPv4, nothing happens.
 */
int lia_inetaddr_map_v4mapped(bn_inetaddr *inout_iaddr);

/**
 * If the given inetaddr contains an IPv6 inetaddr that is an
 * IPv4-mapped IPv6 address, the address is converted into an IPv4
 * inetaddr.  Nothing happens if the inetaddr already contains an IPv4
 * address, or contains an IPv6 address that is not an IPv4-mapped IPv6
 * address.
 */
int lia_inetaddr_unmap_v4mapped(bn_inetaddr *inout_iaddr);

/**
 * XXXXX Add ..._ex from of the below with flags that talk about mapping
 * from ipv4-mapped-in-ipv6 to just ipv4, or always doing ipv6 (by
 * making ipv4 into ipv4-mapped-in-ipv6), and possibly other formatting
 * options.
 *
 */

/**
 * Render a binary Inet address as a string.  The Inet address must hold
 * an IPv4 or IPv6 address.  Inet addresses of other families, including
 * AF_UNSPEC, will cause ::lc_err_bad_type to be returned.
 *
 * \param iaddr Inet address
 * \retval ret_str Rendered string
 */
int lia_inetaddr_to_str(const bn_inetaddr *iaddr,
                        char **ret_str);

/**
 * Render a binary Inet address as a string into a caller-provided
 * buffer.  The Inet address must hold an IPv4 or IPv6 address.  Inet
 * addresses of other families, including AF_UNSPEC, will cause
 * ::lc_err_bad_type to be returned.
 *
 * \param iaddr Inet address
 * \retval ret_str_buf Rendered string
 * \param str_buf_size Number of bytes available in ret_str_buf
 */
int lia_inetaddr_to_str_buf(const bn_inetaddr *iaddr,
                            char *ret_str_buf,
                            uint32 str_buf_size);

/**
 * Like lia_inetaddr_to_str_buf(), but returns ret_str_buf on success,
 * and NULL on failure.  This can be useful when construting log
 * messages and the like.
 */
const char *lia_inetaddr_to_str_buf_quick(const bn_inetaddr *iaddr,
                                          char *ret_str_buf,
                                          uint32 str_buf_size);

/**
 * Test if we can convert the given string into an IPv4 or IPv6 address.
 * This is similar to using lia_str_to_inetaddr(), but without the need
 * for an actual conversion.  This function does not return
 * lc_err_bad_type on invalid strings.
 *
 * \param addr_str Source string
 * \retval ret_valid true iff addr_str is a valid IPv4 or IPv6 address
 */
int lia_str_is_inetaddr(const char *addr_str,
                        tbool *ret_valid);

#ifndef ADDR_UTILS_NO_DEPR
/* An alias, now deprecated */
#define lia_str_is_valid_inetaddr lia_str_is_inetaddr
#endif

/**
 * Test if we can convert the given string into an IPv4 or IPv6 address.
 * Returns ::false on all error conditions.
 *
 * \param addr_str Source string
 * \return true iff addr_str is a valid IPv4 or IPv6 address
 */
tbool lia_str_is_inetaddr_quick(const char *addr_str);

/**
 * If the given string was converted to a bn_inetaddr and back to a
 * string, would it be identical?  Does not return lc_err_bad_type on
 * invalid strings.
 */
int lia_str_is_normalized_inetaddr(const char *addr_str,
                                   tbool *ret_valid,
                                   tbool *ret_canon);

/**
 * Given an Inet address, return the "canonical" (normalized) form, the
 * would be given if it were converted to binary form, and then back to
 * a string.
 */
int lia_normalize_inetaddr_str(const char *addr_str,
                               char **ret_canon_str);

/** Like lia_normalize_inetaddr_str(), but into a caller-supplied buffer */
int lia_normalize_inetaddr_str_buf(const char *addr_str,
                                   char *ret_canon_buf,
                                   uint32 canon_buf_size);

/**
 * Returns all the information an inetaddr holds.  Both *ret_ipv4addr or
 * *ret_ipv6addr will be modified, one with 0s, and the other with the
 * address of the inetaddr.  The ret_family will let the caller know which
 * holds the address.  ret_family will be one of: AF_INET (IPv4),
 * AF_INET6 (IPv6) or AF_UNSPEC (empty).  Note that unlike most other
 * functions in this library, it is possible to have one of the return
 * address pointers be NULL.  However, if the family corresponds to that
 * return address pointer, lc_err_bad_type will be returned.
 */
 int lia_inetaddr_get_full(const bn_inetaddr *iaddr,
                           uint16 *ret_family,
                           bn_ipv4addr *ret_ipv4addr,
                           bn_ipv6addr *ret_ipv6addr);

/**
 * ret_family will be one of: AF_INET (IPv4), AF_INET6 (IPv6) or
 * AF_UNSPEC (empty)
 */
int lia_inetaddr_get_family(const bn_inetaddr *iaddr, uint16 *ret_family);

/** Returns one of: AF_INET (IPv4), AF_INET6 (IPv6) or AF_UNSPEC (empty/err) */
uint16 lia_inetaddr_get_family_quick(const bn_inetaddr *iaddr);

/**
 * inout_saddr_size works like in accept(): on calling it must be the
 * size of *ret_saddr, and on return it is the number of bytes used in
 * *ret_saddr .
 */
int lia_inetaddr_get_saddr(const bn_inetaddr *iaddr,
                           struct sockaddr *ret_saddr,
                           uint32 *inout_saddr_size);

/** 
 * Returns the IPv4 address, or lc_err_bad_type if the family is not AF_INET
 */
int lia_inetaddr_get_ipv4addr(const bn_inetaddr *iaddr,
                              bn_ipv4addr *ret_addr);

/**
 * Returns the IPv4 address, or if the inetaddr hold an IPv4-mapped IPv6
 * address, the unmapped IPv4 address.  Otherwise returns
 * lc_err_bad_type.
 */
int lia_inetaddr_get_ipv4addr_unmap_v4mapped(const bn_inetaddr *iaddr,
                                             bn_ipv4addr *ret_addr);

/**
 * Returns the IPv4 address, or lc_err_bad_type if the family is not
 * AF_INET .
 */
int lia_inetaddr_get_inaddr(const bn_inetaddr *iaddr,
                            struct in_addr *ret_inaddr);

/**
 * Returns the IPv4 address, or NULL if the family is not
 * AF_INET .
 */
const struct in_addr *lia_inetaddr_get_inaddr_quick(const bn_inetaddr
                                                    *iaddr);
/**
 * Returns the IPv6 address, or lc_err_bad_type if the family is not
 * AF_INET6 .
 */
int lia_inetaddr_get_ipv6addr(const bn_inetaddr *iaddr,
                              bn_ipv6addr *ret_addr);

/**
 * Returns the IPv6 address if the family is AF_INET6 , or the
 * converted IPv4-mapped address if the family is AF_INET.
 */
int lia_inetaddr_get_ipv6addr_map_v4mapped(const bn_inetaddr *iaddr,
                                           bn_ipv6addr *ret_addr);

/**
 * Returns the IPv6 address, or lc_err_bad_type if the family is not
 * AF_INET6 .
 */
int lia_inetaddr_get_in6addr(const bn_inetaddr *iaddr,
                             struct in6_addr *ret_inaddr);

/**
 * Returns the IPv6 address, or NULL if the family is not
 * AF_INET6 .
 */
const struct in6_addr *lia_inetaddr_get_in6addr_quick(const bn_inetaddr
                                                      *iaddr);

/*
 * --------------------------------------------------
 * Operations on one or more native bn_inetaddr.
 * --------------------------------------------------
 */

/**
 * Test if two inet (IPv4 or IPv6) addresses are identical.
 * Two address of different families are never equal.
 * Note that it is possible to compare against lia_inetaddr_any , which
 * is only equal to itself.
 *
 * \param addr1 Inet address 1
 * \param addr2 Inet address 2
 * \retval ret_equal Result of comparison
 */
int lia_inetaddr_equal(const bn_inetaddr *addr1,
                       const bn_inetaddr *addr2,
                       tbool *ret_equal);

tbool lia_inetaddr_equal_quick(const bn_inetaddr *addr1,
                               const bn_inetaddr *addr2);

/**
 * Test to see if an inetaddr has a zero address in it.
 *
 * NOTE: in order to qualify as a "zero address", the address must
 * have a family (AF_INET or AF_INET6).  If the family is neither of
 * these (which includes lia_inetaddr_any), this function will return
 * 'false', even if the address and/or family is all zeroes.
 */
int lia_inetaddr_is_zero(const bn_inetaddr *addr,
                         tbool *ret_zero);

/**
 * Quick variant of lia_inetaddr_is_zero(); see comment above.
 */
tbool lia_inetaddr_is_zero_quick(const bn_inetaddr *addr);

int lia_inetaddr_is_ipv4(const bn_inetaddr *addr,
                         tbool *ret_is_ipv4);

tbool lia_inetaddr_is_ipv4_quick(const bn_inetaddr *addr);

int lia_inetaddr_is_ipv6(const bn_inetaddr *iaddr, tbool *ret_is_ipv6);

tbool lia_inetaddr_is_ipv6_quick(const bn_inetaddr *iaddr);


/* -------------------------- */
/* Common IPv4 and IPv6 address type checks */
/* -------------------------- */

/** Address is IPv4 0.0.0.0 or IPv6 :: */
int lia_inetaddr_is_unspecified(const bn_inetaddr *iaddr,
                                tbool *ret_unspecified);
tbool lia_inetaddr_is_unspecified_quick(const bn_inetaddr *addr);

/** Address in IPv4 127/8 or is IPv6 ::1 */
int lia_inetaddr_is_loopback(const bn_inetaddr *iaddr,
                             tbool *ret_loopback);
tbool lia_inetaddr_is_loopback_quick(const bn_inetaddr *addr);

/** Address is IPv4 127.0.0.1 or is IPv6 ::1 */
int lia_inetaddr_is_localhost(const bn_inetaddr *iaddr,
                              tbool *ret_localhost);
tbool lia_inetaddr_is_localhost_quick(const bn_inetaddr *addr);

/**
 * Address is IPv4 in 0/8 (note contains 0.0.0.0) .
 * NOTE: IPv4 only.
 */
int lia_inetaddr_is_zeronet(const bn_inetaddr *iaddr,
                            tbool *ret_zeronet);
tbool lia_inetaddr_is_zeronet_quick(const bn_inetaddr *addr);

/**
 * Address in ::ffff:0:0/96 , IPv4-Mapped IPv6 .
 * NOTE: IPv6 only.
 */
int lia_inetaddr_is_v4mapped(const bn_inetaddr *iaddr,
                             tbool *ret_v4mapped);
tbool lia_inetaddr_is_v4mapped_quick(const bn_inetaddr *iaddr);

/**
 * Address in ::<ipv4-address>/96 .  IPv4-Compatible IPv6 Address.
 * This is a deprecated address type.
 * NOTE: IPv6 only.
 */
int lia_inetaddr_is_v4compat(const bn_inetaddr *iaddr, tbool *ret_v4compat);
tbool lia_inetaddr_is_v4compat_quick(const bn_inetaddr *iaddr);


/** Address in IPv4 169.254/16 or IPv6 fe80::/10 */
int lia_inetaddr_is_linklocal(const bn_inetaddr *iaddr,
                              tbool *ret_linklocal);
tbool lia_inetaddr_is_linklocal_quick(const bn_inetaddr *addr);

/** Address in IPv4 224/4 or IPv6 ff00::/8 */
int lia_inetaddr_is_multicast(const bn_inetaddr *iaddr,
                              tbool *ret_multicast);
tbool lia_inetaddr_is_multicast_quick(const bn_inetaddr *addr);

/**
 * Address in fec0::/10 . Site-Local scoped address.  This is a
 * deprecated address type.
 * NOTE: IPv6 only.
 */
int lia_inetaddr_is_sitelocal(const bn_inetaddr *iaddr, tbool *ret_sitelocal);
tbool lia_inetaddr_is_sitelocal_quick(const bn_inetaddr *iaddr);

/**
 * Address in IPv4 240/4 (note contains 255.255.255.255) .
 * NOTE: IPv4 only.
 */
int lia_inetaddr_is_badclass(const bn_inetaddr *iaddr,
                             tbool *ret_badclass);
tbool lia_inetaddr_is_badclass_quick(const bn_inetaddr *addr);

/**
 * Address is IPv4 255.255.255.255 .
 * NOTE: IPv4 only.
 */
int lia_inetaddr_is_limited_broadcast(const bn_inetaddr *iaddr,
                                      tbool *ret_limited_broadcast);
tbool lia_inetaddr_is_limited_broadcast_quick(const bn_inetaddr *addr);

/** What type flags does the given address have */
int lia_addrtype_inetaddr_type(const bn_inetaddr *iaddr,
                               lia_addrtype_flags_bf *ret_match_mask);

/**
 * Given a set of address type flags, which of those does the given
 * address match?
 */
int lia_addrtype_inetaddr_match(const bn_inetaddr *iaddr,
                                lia_addrtype_flags_bf test_mask,
                                lia_addrtype_flags_bf *ret_match_mask);

/**
 * Note: addresses must both be the same family, or lc_err_bad_type is
 * returned.
 *
 * Note that it is supported to have the result be the same as one of
 * the addresses.
 */
int lia_inetaddr_bitop_inetaddr(const bn_inetaddr *addr_a,
                                const bn_inetaddr *addr_b,
                                lia_bits_op op,
                                bn_inetaddr *ret_result);

int lia_inetaddr_bitop_masklen(const bn_inetaddr *addr_a,
                               uint8 prefix_len,
                               lia_bits_op op,
                               bn_inetaddr *ret_result);

/** -1,0,1 if addr1 <,==,> addr2 */
int lia_inetaddr_cmp(const bn_inetaddr *addr1,
                     const bn_inetaddr *addr2,
                     int *ret_cmp);

int32 lia_inetaddr_cmp_quick(const bn_inetaddr *addr1,
                             const bn_inetaddr *addr2);

/**
 * Family must be AF_INET, AF_INET6 or AF_UNSPEC.  AF_UNSPEC will select
 * AF_INET if it fits, and if not AF_INET6.
 */
/* 64 -> IPv6(ffff:ffff:ffff:ffff::) */
int lia_masklen_to_inetaddr(uint8 prefix_len,
                            uint16 family,
                            bn_inetaddr *ret_mask_addr);

/**
 * Family must be AF_INET, AF_INET6 or AF_UNSPEC.  AF_UNSPEC will select
 * AF_INET if it fits, and if not AF_INET6.
 */
/* 64 -> "ffff:ffff:ffff:ffff::" */
int lia_masklen_to_inetaddr_str(uint8 prefix_len,
                                uint16 family,
                                char **ret_mask_str);

/**
 * Checks if the masklen is in range for the given family.  For AF_INET
 * (IPv4) this is 0 to 32, for AF_INET6 (IPv6) this is 0 to 128.
 */
int
lia_masklen_valid(uint8 masklen,
                  uint16 family,
                  tbool *ret_masklen_okay);

/**
 * Checks if the masklen is in range for IPv4 (0 to 32).
 */
int lia_masklen_valid_ipv4(uint8 masklen,
                           tbool *ret_masklen_okay);

/**
 * Checks if the masklen is in range for IPv6 (0 to 128).
 */
int lia_masklen_valid_ipv6(uint8 masklen,
                           tbool *ret_masklen_okay);

/**
 * Sees if the masklen is in range for the family of the given inetaddr.
 */
int lia_masklen_valid_for_inetaddr(uint8 masklen,
                                   const bn_inetaddr *addr,
                                   tbool *ret_masklen_okay);

/**
 * Family must be AF_INET, AF_INET6 or AF_UNSPEC.  AF_UNSPEC will select
 * AF_INET if it fits, and if not AF_INET6.
 *
 * e.g. "64" -> IPv6(ffff:ffff:ffff:ffff::)
 */
int lia_masklen_str_to_inetaddr(const char *masklen,
                                uint16 family,
                                bn_inetaddr *ret_mask_addr);

/**
 * Family must be AF_INET, AF_INET6 or AF_UNSPEC.  AF_UNSPEC will select
 * AF_INET if it fits, and if not AF_INET6.
 *
 * e.g."64" -> "ffff:ffff:ffff:ffff::"
 */
int lia_masklen_str_to_inetaddr_str(const char *prefix_len,
                                    uint16 family,
                                    char **ret_mask_str);

/**
 * Family must be AF_INET, AF_INET6 or AF_UNSPEC.  AF_UNSPEC will select
 * AF_INET if it fits, and if not AF_INET6.
 *
 * e.g. "64" -> 64
 */
int lia_inetaddr_masklen_str_to_masklen(const char *prefix_len,
                                        uint16 family,
                                        uint8 *ret_prefix_len);

/** IPv6(ffff:ffff:ffff:ffff::) -> 64 */
int lia_inetaddr_to_masklen(const bn_inetaddr *mask_addr,
                            uint8 *ret_prefix_len);

/** IPv6(ffff:ffff:ffff:ffff::) -> "64" */
int lia_inetaddr_to_masklen_str(const bn_inetaddr *mask_addr,
                                char **ret_prefix_len_str);

/**
 * Determine the minimum masklen that can be used by a given IPv4 or
 * IPv6 address.  This is the number of bits in the family of this
 * address (either 32 or 128) minus the number of trailing zero bits
 * in the address.
 *
 * Note that on a netmask, this works the same as
 * lia_inetaddr_to_masklen().  But additionally, we don't mind if
 * some of the bits higher than the lowest set were clear.
 */
int lia_inetaddr_get_min_masklen(const bn_inetaddr *addr,
                                 uint8 *ret_prefix_len);

/**
 * Family must be AF_INET, AF_INET6 or AF_UNSPEC.  AF_UNSPEC will select
 * AF_INET if it fits, and if not AF_INET6.
 *
 * e.g. "ffff:ffff:ffff:ffff::" -> 64
 */
int lia_inetaddr_str_to_masklen(const char *mask_str,
                                uint16 family,
                                uint8 *ret_prefix_len);

/**
 * Family must be AF_INET, AF_INET6 or AF_UNSPEC.  AF_UNSPEC will select
 * AF_INET if it fits, and if not AF_INET6.
 *
 * e.g. "ffff:ffff:ffff:ffff::" -> "64"
 */
int lia_inetaddr_str_to_masklen_str(const char *mask_str,
                                    uint16 family,
                                    char **ret_prefix_len_str);


/* Inetaddr maskspec conversions  */

/** "/48" or "ffff:ffff:ffff::" -> 48 */
int lia_inetaddr_maskspec_to_masklen(const char *maskspec,
                                     uint16 family,
                                     uint8 *ret_prefix_len);

/** "/48" or "ffff:ffff:ffff::" -> "48" */
int lia_inetaddr_maskspec_to_masklen_str(const char *maskspec,
                                         uint16 family,
                                         char **ret_prefix_len_str);

/** "/48" or "ffff:ffff:ffff::" -> IPv6(ffff:ffff:ffff::) */
int lia_inetaddr_maskspec_to_inetaddr(const char *maskspec,
                                      uint16 family,
                                      bn_inetaddr *ret_mask_addr);

/** "/48" or "ffff:ffff:ffff::" -> "ffff:ffff:ffff::" */
int lia_inetaddr_maskspec_to_inetaddr_str(const char *maskspec,
                                          uint16 family,
                                          char **ret_mask_str);


/* ---------------------------------------------------------------------- */
/* bn_inetaddrz : a type which holds an Inet address and a zone id.
 */

/**
 * Convert a string to a binary Inet address with zone id.
 *
 * Please note that only an ifindex, not an interface name, is 
 * accepted for the zone id.  If you might have an interface name
 * after the '%' sign, consider using lia_str_to_inetaddrz_ex() 
 * with the ::lacf_convert_zone_ifname_to_ifindex flag.
 */
int lia_str_to_inetaddrz(const char *addr_str,
                         bn_inetaddrz *ret_iaddr);


int lia_str_to_inetaddrz_ex(const char *addr_str,
                            lia_addr_conv_flags_bf flags,
                            bn_inetaddrz *ret_iaddr);

int lia_inaddr_zoneid_to_inetaddrz(const struct in_addr *inaddr,
                                   uint32 zone_id,
                                   bn_inetaddrz *ret_iaddr);

int lia_in6addr_zoneid_to_inetaddrz(const struct in6_addr *inaddr,
                                    uint32 zone_id,
                                    bn_inetaddrz *ret_iaddr);

int lia_ipv4addr_zoneid_to_inetaddrz(const bn_ipv4addr *iaddr,
                                     uint32 zone_id,
                                     bn_inetaddrz *ret_iaddr);

/**
 * Make an inetaddrz which is of family AF_INET6, corresponding to the
 * IPv4-mapped IPv6 address of the specified IPv4 address.
 */
int lia_ipv4addr_zoneid_to_inetaddrz_map_v4mapped(const bn_ipv4addr *iaddr,
                                                  uint32 zone_id,
                                                  bn_inetaddrz *ret_iaddr);

int lia_ipv6addr_zoneid_to_inetaddrz(const bn_ipv6addr *iaddr,
                                     uint32 zone_id,
                                     bn_inetaddrz *ret_iaddr);

/** Works only for sockaddr's of family AF_INET or AF_INET6 . */
int lia_saddr_to_inetaddrz(const struct sockaddr *saddr,
                           uint32 saddr_len,
                           bn_inetaddrz *ret_iaddr);

int lia_inetaddr_zoneid_to_inetaddrz(const bn_inetaddr *iaddr,
                                     uint32 zone_id,
                                     bn_inetaddrz *ret_iaddr);

int lia_ipv6addrz_to_inetaddrz(const bn_ipv6addrz *iaddr,
                               bn_inetaddrz *ret_iaddr);

/**
 * Converts an IPv4 inetaddrz into an IPv6 inetaddrz, using an
 * IPv4-mapped IPv6 address.  This would mean converting IPv4 10.1.2.3
 * -> IPv6 ::ffff:10.1.2.3 .  If the inetaddrz is not IPv4, nothing
 * happens.
 */
int lia_inetaddrz_map_v4mapped(bn_inetaddrz *inout_iaddr);

/**
 * If the given inetaddrz contains an IPv6 inetaddrz that is an
 * IPv4-mapped IPv6 address, the address is converted into an IPv4
 * inetaddrz.  Nothing happens if the inetaddrz already contains an IPv4
 * address, or contains an IPv6 address that is not an IPv4-mapped IPv6
 * address.
 */
int lia_inetaddrz_unmap_v4mapped(bn_inetaddrz *inout_iaddr);


/**
 * Render a binary Inet address as a string.
 */
int lia_inetaddrz_to_str(const bn_inetaddrz *iaddr,
                         char **ret_str);

/** Like lia_inetaddrz_to_str(), but into a caller-supplied buffer */
int lia_inetaddrz_to_str_buf(const bn_inetaddrz *iaddr,
                             char *ret_str_buf,
                             uint32 str_buf_size);

/**
 * Like lia_inetaddrz_to_str_buf(), but returns ret_str_buf on success,
 * and NULL on failure.  This can be useful when construting log
 * messages and the like.
 */
const char *lia_inetaddrz_to_str_buf_quick(const bn_inetaddrz *iaddr,
                                           char *ret_str_buf,
                                           uint32 str_buf_size);

/**
 * Test if we can convert the given string into an IPv4 or IPv6 address
 * with zone id.  This is similar to using lia_str_to_inetaddrz(), but
 * without the need for an actual conversion.  This function does not
 * return lc_err_bad_type on invalid strings.
 *
 * \param addr_str Source string
 * \retval ret_valid true iff addr_str is a valid IPv4 or IPv6 address
 * with zone id.
 */
int lia_str_is_inetaddrz(const char *addr_str,
                         tbool *ret_valid);

#ifndef ADDR_UTILS_NO_DEPR
/* An alias, now deprecated */
#define lia_str_is_valid_inetaddrz lia_str_is_inetaddrz
#endif

/**
 * Test if we can convert the given string into an IPv4 or IPv6 address
 * with zone id .  Returns ::false on all error conditions.
 *
 * \param addr_str Source string
 * \return true iff addr_str is a valid IPv4 or IPv6 address with zone id.
 */
tbool lia_str_is_inetaddrz_quick(const char *addr_str);

/**
 * If the given string was converted to a bn_inetaddrz and back to a
 * string, would it be identical?  Does not return lc_err_bad_type on
 * invalid strings.
 */
int lia_str_is_normalized_inetaddrz(const char *addr_str,
                                    tbool *ret_valid,
                                    tbool *ret_canon);

/**
 * Given an Inet address with zone id, return the "canonical"
 * (normalized) form, the would be given if it were converted to binary
 * form, and then back to a string.
 */
int lia_normalize_inetaddrz_str(const char *addr_str,
                               char **ret_canon_str);

/** Like lia_normalize_inetaddrz_str(), but into a caller-supplied buffer */
int lia_normalize_inetaddrz_str_buf(const char *addr_str,
                                    char *ret_canon_buf,
                                    uint32 canon_buf_size);

/**
 * Returns all the information an inetaddrz holds.  Both *ret_ipv4addr
 * or *ret_ipv6addr will be modified, one with 0s, and the other with
 * the address of the inetaddr.  The family will let the caller know
 * which holds the address.  ret_family will be one of: AF_INET (IPv4),
 * AF_INET6 (IPv6) or AF_UNSPEC (empty).  Note that unlike most other
 * functions in this library, it is possible to have one of the return
 * address pointers be NULL.  However, if the family corresponds to that
 * return address pointer, lc_err_bad_type will be returned.
 */
int lia_inetaddrz_get_full(const bn_inetaddrz *iaddr,
                           uint16 *ret_family,
                           bn_ipv4addr *ret_ipv4addr,
                           bn_ipv6addr *ret_ipv6addr,
                           uint32 *ret_zone_id);

int lia_inetaddrz_get_zoneid(const bn_inetaddrz *iaddr,
                             uint32 *ret_zone_id);

uint32 lia_inetaddrz_get_zoneid_quick(const bn_inetaddrz *iaddr);

/**
 * ret_family will be one of: AF_INET (IPv4), AF_INET6 (IPv6) or
 * AF_UNSPEC (empty).
 */
int lia_inetaddrz_get_family(const bn_inetaddrz *iaddr, uint16 *ret_family);

/** Returns one of: AF_INET (IPv4), AF_INET6 (IPv6) or AF_UNSPEC (empty/err) */
uint16 lia_inetaddrz_get_family_quick(const bn_inetaddrz *iaddr);

/**
 * Returns the IPv4 address and zone id , or lc_err_bad_type if the
 * family is not AF_INET .
 */
int lia_inetaddrz_get_inaddr_zoneid(const bn_inetaddrz *iaddr,
                                    struct in_addr *ret_inaddr,
                                    uint32 *ret_zone_id);

/**
 * Returns the IPv4 address, or NULL if the family is not
 * AF_INET .
 */
const struct in_addr *lia_inetaddrz_get_inaddr_quick(const bn_inetaddrz
                                                     *iaddr);

int lia_inetaddrz_get_in6addr_zoneid(const bn_inetaddrz *iaddr,
                                     struct in6_addr *ret_inaddr,
                                     uint32 *ret_zone_id);

/**
 * Returns the IPv6 address, or NULL if the family is not
 * AF_INET6 .
 */
const struct in6_addr *lia_inetaddrz_get_in6addr_quick(const bn_inetaddrz
                                                       *iaddr);

int lia_inetaddrz_get_inetaddr_zoneid(const bn_inetaddrz *iaddr,
                                      bn_inetaddr *ret_iaddr,
                                      uint32 *ret_zone_id);



/**
 * Note that as 'struct sockaddr_in' has no zone id, this will be silently
 * lost of IPv4 addresses with a zone id.
 *
 * inout_saddr_size works like in accept(): on calling it must be the
 * size of *ret_saddr, and on return it is the number of bytes used in
 * *ret_saddr .
 */
int lia_inetaddrz_get_saddr(const bn_inetaddrz *iaddr,
                            struct sockaddr *ret_saddr,
                            uint32 *inout_saddr_size);

/**
 * Returns the IPv4 address and zone, or lc_err_bad_type if the family
 * is not AF_INET .
 */
int lia_inetaddrz_get_ipv4addr_zoneid(const bn_inetaddrz *iaddr,
                                      bn_ipv4addr *ret_addr,
                                      uint32 *ret_zone_id);

/**
 * Returns the IPv6 zoned address, or lc_err_bad_type if the family is
 * not AF_INET6 .
 */
int lia_inetaddrz_get_ipv6addrz(const bn_inetaddrz *iaddr,
                               bn_ipv6addrz *ret_addr);

/**
 * Returns the IPv6 address and zone, or lc_err_bad_type if the family
 * is not AF_INET6 .
 */
int lia_inetaddrz_get_ipv6addr_zoneid(const bn_inetaddrz *iaddr,
                                      bn_ipv6addr *ret_addr,
                                      uint32 *ret_zone_id);

/**
 * Returns the IPv4 address, or lc_err_bad_type if the family is not
 * AF_INET .
 */
int lia_inetaddrz_get_ipv4addr(const bn_inetaddrz *iaddr,
                               bn_ipv4addr *ret_addr);

/**
 * Returns the IPv4 address, or if the inetaddrz holds an IPv4-mapped
 * IPv6 address, the unmapped IPv4 address.  Otherwise returns
 * lc_err_bad_type.
 */
int lia_inetaddrz_get_ipv4addr_unmap_v4mapped_zoneid(const bn_inetaddrz *iaddr,
                                                     bn_ipv4addr *ret_addr,
                                                     uint32 *ret_zone_id);


/**
 * Returns the IPv6 address, or lc_err_bad_type if the family is not
 * AF_INET6 .
 */
int lia_inetaddrz_get_ipv6addr(const bn_inetaddrz *iaddr,
                               bn_ipv6addr *ret_addr);

/**
 * Returns the IPv6 address if the family is AF_INET6 , or the
 * converted IPv4-mapped address if the family is AF_INET.
 */
int lia_inetaddrz_get_ipv6addr_map_v4mapped_zoneid(const bn_inetaddrz *iaddr,
                                                   bn_ipv6addr *ret_addr,
                                                   uint32 *ret_zone_id);


/*
 * --------------------------------------------------
 * Operations on one or more native bn_inetaddrz.
 * --------------------------------------------------
 */

int lia_inetaddrz_equal(const bn_inetaddrz *addr1,
                       const bn_inetaddrz *addr2,
                       tbool *ret_equal);

tbool lia_inetaddrz_equal_quick(const bn_inetaddrz *addr1,
                               const bn_inetaddrz *addr2);

/**
 * Test to see if an inetaddrz has a zero address in it.
 *
 * NOTE: in order to qualify as a "zero address", the address must
 * have a family (AF_INET or AF_INET6).  If the family is neither of
 * these (which includes lia_inetaddrz_any), this function will return
 * 'false', even if the address and/or family is all zeroes.
 */
int lia_inetaddrz_is_zero(const bn_inetaddrz *addr,
                         tbool *ret_zero);

/**
 * Quick variant of lia_inetaddrz_is_zero(); see comment above.
 */
tbool lia_inetaddrz_is_zero_quick(const bn_inetaddrz *addr);

int lia_inetaddrz_is_ipv4(const bn_inetaddrz *addr,
                         tbool *ret_is_ipv4);

tbool lia_inetaddrz_is_ipv4_quick(const bn_inetaddrz *addr);

int lia_inetaddrz_is_ipv6(const bn_inetaddrz *iaddr, tbool *ret_is_ipv6);

tbool lia_inetaddrz_is_ipv6_quick(const bn_inetaddrz *iaddr);

/* -------------------------- */
/* Common IPv4 and IPv6 address type checks */
/* -------------------------- */

/** Address is IPv4 0.0.0.0 or IPv6 :: */
int lia_inetaddrz_is_unspecified(const bn_inetaddrz *iaddr,
                                tbool *ret_unspecified);
tbool lia_inetaddrz_is_unspecified_quick(const bn_inetaddrz *addr);

/** Address in IPv4 127/8 or is IPv6 ::1 */
int lia_inetaddrz_is_loopback(const bn_inetaddrz *iaddr,
                             tbool *ret_loopback);
tbool lia_inetaddrz_is_loopback_quick(const bn_inetaddrz *addr);

/** Address is IPv4 127.0.0.1 or is IPv6 ::1 */
int lia_inetaddrz_is_localhost(const bn_inetaddrz *iaddr,
                               tbool *ret_localhost);
tbool lia_inetaddrz_is_localhost_quick(const bn_inetaddrz *addr);

/**
 * Address is IPv4 in 0/8 (note contains 0.0.0.0) .
 * NOTE: IPv4 only.
 */
int lia_inetaddrz_is_zeronet(const bn_inetaddrz *iaddr,
                            tbool *ret_zeronet);
tbool lia_inetaddrz_is_zeronet_quick(const bn_inetaddrz *addr);

/**
 * Address in ::ffff:0:0/96 , IPv4-Mapped IPv6 .
 * NOTE: IPv6 only.
 */
int lia_inetaddrz_is_v4mapped(const bn_inetaddrz *iaddr,
                             tbool *ret_v4mapped);
tbool lia_inetaddrz_is_v4mapped_quick(const bn_inetaddrz *iaddr);

/**
 * Address in ::<ipv4-address>/96 .  IPv4-Compatible IPv6 Address.
 * This is a deprecated address type.
 * NOTE: IPv6 only.
 */
int lia_inetaddrz_is_v4compat(const bn_inetaddrz *iaddr, tbool *ret_v4compat);
tbool lia_inetaddrz_is_v4compat_quick(const bn_inetaddrz *iaddr);


/** Address in IPv4 169.254/16 or IPv6 fe80::/10 */
int lia_inetaddrz_is_linklocal(const bn_inetaddrz *iaddr,
                              tbool *ret_linklocal);
tbool lia_inetaddrz_is_linklocal_quick(const bn_inetaddrz *addr);

/** Address in IPv4 224/4 or IPv6 ff00::/8 */
int lia_inetaddrz_is_multicast(const bn_inetaddrz *iaddr,
                              tbool *ret_multicast);
tbool lia_inetaddrz_is_multicast_quick(const bn_inetaddrz *addr);

/**
 * Address in fec0::/10 . Site-Local scoped address.  This is a
 * deprecated address type.
 * NOTE: IPv6 only.
 */
int lia_inetaddrz_is_sitelocal(const bn_inetaddrz *iaddr,
                               tbool *ret_sitelocal);
tbool lia_inetaddrz_is_sitelocal_quick(const bn_inetaddrz *iaddr);

/**
 * Address in IPv4 240/4 (note contains 255.255.255.255) .
 * NOTE: IPv4 only.
 */
int lia_inetaddrz_is_badclass(const bn_inetaddrz *iaddr,
                             tbool *ret_badclass);
tbool lia_inetaddrz_is_badclass_quick(const bn_inetaddrz *addr);

/**
 * Address is IPv4 255.255.255.255 .
 * NOTE: IPv4 only.
 */
int lia_inetaddrz_is_limited_broadcast(const bn_inetaddrz *iaddr,
                                      tbool *ret_limited_broadcast);
tbool lia_inetaddrz_is_limited_broadcast_quick(const bn_inetaddrz *addr);

/** What type flags does the given address have */
int lia_addrtype_inetaddrz_type(const bn_inetaddrz *iaddr,
                               lia_addrtype_flags_bf *ret_match_mask);

/**
 * Given a set of address type flags, which of those does the given
 * address match?
 */
int lia_addrtype_inetaddrz_match(const bn_inetaddrz *iaddr,
                                lia_addrtype_flags_bf test_mask,
                                lia_addrtype_flags_bf *ret_match_mask);


/** -1,0,1 if addr1 <,==,> addr2 */
int lia_inetaddrz_cmp(const bn_inetaddrz *addr1,
                      const bn_inetaddrz *addr2,
                      int *ret_cmp);

int32 lia_inetaddrz_cmp_quick(const bn_inetaddrz *addr1,
                              const bn_inetaddrz *addr2);

/* ---------------------------------------------------------------------- */
/* bn_inetprefix : a type which holds an Inet address and mask length.
 */

/**
 * Convert a string to a binary Inet prefix.
 */
int lia_str_to_inetprefix(const char *addr_str,
                          bn_inetprefix *ret_iaddr);

int lia_inetaddr_masklen_to_inetprefix(const bn_inetaddr *addr,
                                       uint8 masklen,
                                       bn_inetprefix *ret_iaddr);

int lia_inetaddr_mask_to_inetprefix(const bn_inetaddr *addr,
                                    const bn_inetaddr *mask,
                                    bn_inetprefix *ret_iaddr);

int lia_ipv4prefix_to_inetprefix(const bn_ipv4prefix *addr,
                                 bn_inetprefix *ret_iaddr);

int lia_ipv6prefix_to_inetprefix(const bn_ipv6prefix *addr,
                                 bn_inetprefix *ret_iaddr);

int lia_ipv4addr_masklen_to_inetprefix(const bn_ipv4addr *addr,
                                       uint8 masklen,
                                       bn_inetprefix *ret_iaddr);

int lia_ipv4addr_mask_to_inetprefix(const bn_ipv4addr *addr,
                                    const bn_ipv4addr *mask,
                                    bn_inetprefix *ret_iaddr);

int lia_inaddr_masklen_to_inetprefix(const struct in_addr *addr,
                                     uint8 masklen,
                                     bn_inetprefix *ret_iaddr);

int lia_inaddr_mask_to_inetprefix(const struct in_addr *addr,
                                  const struct in_addr *mask,
                                  bn_inetprefix *ret_iaddr);

int lia_ipv6addr_masklen_to_inetprefix(const bn_ipv6addr *addr,
                                       uint8 masklen,
                                       bn_inetprefix *ret_iaddr);

int lia_ipv6addr_mask_to_inetprefix(const bn_ipv6addr *addr,
                                    const bn_ipv6addr *mask,
                                    bn_inetprefix *ret_iaddr);

int lia_in6addr_masklen_to_inetprefix(const struct in6_addr *addr,
                                      uint8 masklen,
                                      bn_inetprefix *ret_iaddr);

int lia_in6addr_mask_to_inetprefix(const struct in6_addr *addr,
                                   const struct in6_addr *mask,
                                   bn_inetprefix *ret_iaddr);

/**
 * The "masked" forms are like above, but explicitly zero any host bits
 * in the given address.
 */
int lia_inetaddr_masklen_masked_to_inetprefix(const bn_inetaddr *addr,
                                       uint8 masklen,
                                       bn_inetprefix *ret_iaddr);

int lia_inetaddr_mask_masked_to_inetprefix(const bn_inetaddr *addr,
                                    const bn_inetaddr *mask,
                                    bn_inetprefix *ret_iaddr);

/* XXXX add for all the non-"masked" forms above */

/**
 * Render a binary Inet prefix as a string.
 */
int lia_inetprefix_to_str(const bn_inetprefix *iaddr,
                          char **ret_str);

/** Like lia_inetprefix_to_str(), but into a caller-supplied buffer */
int lia_inetprefix_to_str_buf(const bn_inetprefix *iaddr,
                              char *ret_str_buf,
                              uint32 str_buf_size);

/**
 * Like lia_inetprefix_to_str_buf(), but returns ret_str_buf on success,
 * and NULL on failure.  This can be useful when construting log
 * messages and the like.
 */
const char *lia_inetprefix_to_str_buf_quick(const bn_inetprefix *iaddr,
                                            char *ret_str_buf,
                                            uint32 str_buf_size);


/**
 * Test if we can convert the given string into an IPv4 or IPv6 prefix.
 * This is similar to using lia_str_to_inetprefix(), but without the need
 * for an actual conversion.  This function does not return
 * lc_err_bad_type on invalid strings.
 *
 * \param addr_str Source string
 * \retval ret_valid true iff addr_str is a valid IPv4 or IPv6 prefix
 */
int lia_str_is_inetprefix(const char *addr_str,
                          tbool *ret_valid);

#ifndef ADDR_UTILS_NO_DEPR
/* An alias, now deprecated */
#define lia_str_is_valid_inetprefix lia_str_is_inetprefix
#endif

/**
 * Test if we can convert the given string into an IPv4 or IPv6 prefix.
 * Returns ::false on all error conditions.
 *
 * \param addr_str Source string
 * \return true iff addr_str is a valid IPv4 or IPv6 prefix
 */
tbool lia_str_is_inetprefix_quick(const char *addr_str);

/**
 * If the given string was converted to a bn_inetprefix and back to a
 * string, would it be identical?  Does not return lc_err_bad_type on
 * invalid strings.
 */
int lia_str_is_normalized_inetprefix(const char *addr_str,
                                     tbool *ret_valid,
                                     tbool *ret_canon);

/**
 * Given an Inet prefix, return the "canonical" (normalized) form, the
 * would be given if it were converted to binary form, and then back to
 * a string.
 */
int lia_normalize_inetprefix_str(const char *addr_str,
                               char **ret_canon_str);

/** Like lia_normalize_inetprefix_str(), but into a caller-supplied buffer */
int lia_normalize_inetprefix_str_buf(const char *addr_str,
                                     char *ret_canon_buf,
                                     uint32 canon_buf_size);

/**
 * Returns all the information an inetprefix holds.  Both
 * *ret_ipv4prefix or *ret_ipv6prefix will be modified, one with 0s, and
 * the other with the address of the inetprefix.  The family will let
 * the caller know which holds the address.  ret_family will be one of:
 * AF_INET (IPv4), AF_INET6 (IPv6) or AF_UNSPEC (empty).  Note that
 * unlike most other functions in this library, it is possible to have
 * one of the return address pointers be NULL.  However, if the family
 * corresponds to that return address pointer, lc_err_bad_type will be
 * returned.
 */
int lia_inetprefix_get_full(const bn_inetprefix *iaddr,
                            uint16 *ret_family,
                            bn_ipv4prefix *ret_ipv4prefix,
                            bn_ipv6prefix *ret_ipv6prefix);

/**
 * ret_family will be one of: AF_INET (IPv4), AF_INET6 (IPv6) or
 * AF_UNSPEC (empty).
 */
int lia_inetprefix_get_family(const bn_inetprefix *iaddr, uint16 *ret_family);

/** Returns one of: AF_INET (IPv4), AF_INET6 (IPv6) or AF_UNSPEC (empty/err) */
uint16 lia_inetprefix_get_family_quick(const bn_inetprefix *iaddr);

int lia_inetprefix_get_inetaddr(const bn_inetprefix *iaddr,
                                bn_inetaddr *ret_iaddr);

int lia_inetprefix_get_inetaddr_str(const bn_inetprefix *iaddr,
                                    char **ret_addr_str);

/**
 * Returns the IPv4 prefix, or lc_err_bad_type if the family is not
 * AF_INET .
 */
int lia_inetprefix_get_ipv4prefix(const bn_inetprefix *iaddr,
                                  bn_ipv4prefix *ret_addr);

/**
 * Returns the IPv4 address, or lc_err_bad_type if the family is not
 * AF_INET .
 */
int lia_inetprefix_get_ipv4addr(const bn_inetprefix *iaddr,
                                bn_ipv4addr *ret_addr);

/**
 * Returns the IPv6 prefix, or lc_err_bad_type if the family is not
 * AF_INET6 .
 */
int lia_inetprefix_get_ipv6prefix(const bn_inetprefix *iaddr,
                                  bn_ipv6prefix *ret_addr);

/**
 * Returns the IPv6 address, or lc_err_bad_type if the family is not
 * AF_INET6 .
 */
int lia_inetprefix_get_ipv6addr(const bn_inetprefix *iaddr,
                                bn_ipv6addr *ret_addr);

int lia_inetprefix_get_masklen(const bn_inetprefix *iaddr,
                               uint8 *ret_prefix_len);

int lia_inetprefix_get_masklen_str(const bn_inetprefix *iaddr,
                                   char **ret_masklen_str);

int lia_inetprefix_get_mask(const bn_inetprefix *iaddr,
                            bn_inetaddr *ret_mask);

/**
 * Returns the prefix mask as an IPv4 address, or lc_err_bad_type if the
 * family is not AF_INET .
 */
int lia_inetprefix_get_mask_ipv4addr(const bn_inetprefix *iaddr,
                                     bn_ipv4addr *ret_mask);

/**
 * Returns the prefix mask as an IPv6 address, or lc_err_bad_type if the
 * family is not AF_INET6 .
 */
int lia_inetprefix_get_mask_ipv6addr(const bn_inetprefix *iaddr,
                                     bn_ipv6addr *ret_mask);

int lia_inetprefix_get_mask_str(const bn_inetprefix *iaddr,
                                char **ret_mask_str);

/*
 * --------------------------------------------------
 * Operations on one or more native bn_inetprefix .
 * --------------------------------------------------
 */

int lia_inetprefix_equal(const bn_inetprefix *addr1,
                         const bn_inetprefix *addr2,
                         tbool *ret_equal);

tbool lia_inetprefix_equal_quick(const bn_inetprefix *addr1,
                                 const bn_inetprefix *addr2);

/**
 * Test to see if an inetprefix has a zero address and mask length 
 * in it.
 *
 * NOTE: in order to qualify as a "zero address", the address must
 * have a family (AF_INET or AF_INET6).  If the family is neither of
 * these (which includes lia_inetprefix_any), this function will
 * return 'false', even if the address and/or family, and mask length,
 * are all zeroes.
 */
int lia_inetprefix_is_zero(const bn_inetprefix *addr,
                           tbool *ret_zero);

/**
 * Quick variant of lia_inetprefix_is_zero(); see comment above.
 */
tbool lia_inetprefix_is_zero_quick(const bn_inetprefix *addr);

int lia_inetprefix_is_ipv4(const bn_inetprefix *addr,
                         tbool *ret_is_ipv4);

tbool lia_inetprefix_is_ipv4_quick(const bn_inetprefix *addr);

int lia_inetprefix_is_ipv6(const bn_inetprefix *iaddr, tbool *ret_is_ipv6);

tbool lia_inetprefix_is_ipv6_quick(const bn_inetprefix *iaddr);

/** -1,0,1 if addr1 <,==,> addr2 */
int lia_inetprefix_cmp(const bn_inetprefix *addr1,
                       const bn_inetprefix *addr2,
                       int *ret_cmp);

int32 lia_inetprefix_cmp_quick(const bn_inetprefix *addr1,
                               const bn_inetprefix *addr2);

int lia_inetprefix_contains_inetaddr(const bn_inetprefix *pfx,
                                     const bn_inetaddr *addr,
                                     tbool *ret_contains);


#ifdef __cplusplus
}
#endif

#endif /* __ADDR_UTILS_H_ */
