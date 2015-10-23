/*
 *
 * addr_utils_ipv4u32.h
 *
 *
 *
 */

#ifndef __ADDR_UTILS_IPV4U32_H_
#define __ADDR_UTILS_IPV4U32_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "tinttypes.h"
#include "ttypes.h"

#ifndef TMSLIBS_DISABLE_IPV4_UINT32


/* ------------------------------------------------------------------------- */
/** \file src/include/addr_utils_ipv4u32.h Network address conversion
 * utilities for IPv4 addresses stored in host byte order.  Many
 * functions in this component are deprecated.
 *
 * \deprecated For all new code, instead use addr_utils.h .
 *
 * \ingroup lc
 *
 * These functions operate on IPv4 addresses, masks and ports.  They do
 * so using addresses in HOST BYTE ORDER as "uint32" , and ports as host
 * byte order "uint16".
 *
 * NOTE: addresses and ports given to and received from the underlying
 * socket APIs are in NETWORK BYTE ORDER.  For example: 'struct
 * sockaddr_in' has 'sin_port' and 'sin_addr' in network byte order;
 * 'struct in_addr' has 's_addr' (a uint32) in network byte order.
 */


static const char lc_ipv4addr_delim_char = '.';
static const char lc_ipv4prefix_delim_char = '/';

/** Conversion function of string to IPv4 address (host order uint32) 
 * \param str Source string
 * \retval ret_ipv4addr Computed IPv4 address (host order uint32)
 */
int lc_str_to_ipv4addr(const char *str, uint32 *ret_ipv4addr);

/** Conversion function of string to IPv4 address & port (host order
 *  uint32 and uint16)
 * \param str Source string
 * \retval ret_ipv4addr Computed IPv4 address (host order uint32)
 * \retval ret_port Computed Port number (host order uint16)
 */
int lc_str_to_ipv4addr_and_port(const char *str, uint32 *ret_ipv4addr,
                                uint16 *ret_port);

/** Conversion function of string to IPv4 prefix (host order uint32
 *  addr and mask)
 * \param str Source string
 * \retval ret_ipv4prefix Computed IPv4 prefix (host order uint32 addr
 * and mask)
 */
int lc_str_to_ipv4prefix(const char *str, uint64 *ret_ipv4prefix);

/** Conversion function of IPv4 address (host order uint32) to string
 * \param ipv4addr Source IPv4 address (host order uint32)
 * \retval ret_str Derived string
 */
int lc_ipv4addr_to_str(uint32 ipv4addr, char **ret_str);

/** Decompose an IPv4 address (host order uint32) into its octets.
 * \param ipv4addr Source IPv4 address (host order uint32)
 * \retval ret_ipv4addr_octets The octets, in the order they would be 
 * displayed, e.g. the address 10.1.0.40 would return {10, 1, 0, 40}
 * in the array.
 */
int lc_ipv4addr_to_octets(uint32 ipv4addr, uint8 ret_ipv4addr_octets[4]);

/** Conversion function of IPv4 prefix to string
 * \param ipv4prefix Source IPv4 prefix (host order uint32 addr and mask)
 * \retval ret_str Derived string
 */
int lc_ipv4prefix_to_str(uint64 ipv4prefix, char **ret_str);

/** Conversion function of a mask length to an IPv4 mask string
 * \param masklen Source masklen (like 24)
 * \retval ret_netmask Derived IPv4 netmask string (like "255.255.255.0")
 */
int lc_masklen_to_netmask_str(uint8 masklen, char **ret_netmask);

/** Conversion function of an IPv4 mask string to a mask length string
 * \param mask Source IPv4 mask string (like "255.255.255.0" or "/24")
 * \retval ret_mask_len Derived mask length string (like "24")
 */
int lc_mask_or_masklen_to_masklen_str(const char *mask, char **ret_mask_len);

/** Conversion function of an IPv4 mask (host order uint32) to a mask length
 * \param mask Source IPv4 mask (host order uint32)
 * \retval ret_masklen Derived netmask length
 */
int lc_ipv4mask_to_masklen(uint32 mask, uint8 *ret_masklen);

/** Conversion function of a mask length to an IPv4 mask (host order uint32)
 * \param masklen Source mask length
 * \return An IPv4 mask (host order uint32)
 */
uint32 lc_mask_from_masklen(uint8 masklen);

/** Conversion function of an IPv4 prefix (host order uint32
 *  addr and mask) to an IPv4 address (host order uint32)
 * \param prefix Source IPv4 prefix (host order uint32 addr and mask)
 * \return An IPv4 address (host order uint32)
 */
uint32 lc_ip_from_ipv4prefix(uint64 prefix);

/** Conversion function of an IPv4 prefix (host order uint32 addr and
 *  mask) to an IPv4 mask (host order uint32)
 * \param prefix Source IPv4 prefix (host order uint32 addr and mask)
 * \return An IPv4 mask (host order uint32)
 */
uint32 lc_mask_from_ipv4prefix(uint64 prefix);

/** Conversion function of an IPv4 address and IPv4 mask to a IPv4
 *  prefix (host order uint32 address and mask)
 * \param ipv4addr An IPv4 address (host order uint32)
 * \param mask An IPv4 mask (host order uint32)
 * \return The IPv4 prefix with the given IPv4 network address and IPv4 mask
 * (host order uint32 address and mask)
 */
uint64 lc_ip_mask_to_ipv4prefix(uint32 ipv4addr, uint32 mask);


#endif /* TMSLIBS_DISABLE_IPV4_UINT32 */


#ifdef __cplusplus
}
#endif

#endif /* __ADDR_UTILS_IPV4U32_H_ */
