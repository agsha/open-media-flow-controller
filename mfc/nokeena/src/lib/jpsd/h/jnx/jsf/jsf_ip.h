/*
 * $Id: jsf_ip.h 676216 2014-10-12 06:12:42Z jaiswalr $
 *
 * jsf_ip.h - JSF IP address definitions and macros
 *
 * Copyright (c) 2009-2013, Juniper Networks, Inc.
 * All rights reserved.
 */

/**
 * @file jsf_ip.h
 * 
 * @internal
 *
 * @brief
 * JSF IP address definitions and macros
 * 
 */

#ifndef __JSF_IP_H__
#define __JSF_IP_H__

/*
 * Use C linkage when using a C++ compiler
 */                               
#ifdef __cplusplus
extern "C" {
    namespace junos {
#endif /* __cplusplus */

#ifdef PLATFORM_MTX

#define IP_VERSION_4   0
#define IP_VERSION_6   1
#define IP_VERSION_DC  2 /* don't care */


#ifndef _IP6_ADDR_T_DECLARED

#define INET6_ADDR_BYTES       16
#define INET6_ADDR_HALFWORDS   8
#define INET6_ADDR_WORDS       4
        
typedef struct ip6_addr_ {
    union {
        uint8_t   byte[INET6_ADDR_BYTES];
        uint16_t  halfword[INET6_ADDR_HALFWORDS];
        uint32_t  word[INET6_ADDR_WORDS];
    } a;
#define s6_addr_sc   a.byte
#define s6_addr8_sc  a.byte
#define s6_addr16_sc a.halfword
#define s6_addr32_sc a.word
} ip6_addr_t;

#define _IP6_ADDR_T_DECLARED

#endif /* _IP6_ADDR_T_DECLARED */

typedef u_int32_t ip_version_t;

typedef struct ipvx_addr_ {
    union {
        ip6_addr_t _v6ip;
        uint32_t   _v4ip;
    } _u;
#define v6ip    _u._v6ip
#define v4ip    _u._v4ip
#define vxip    _u._v6ip

    uint8_t ver;
#define vxiplen ver*12+4
    uint8_t prefix_len;
    uint8_t pad[2];
    /*
     * the remaining 2 bytes here could used by the platform to store
     * some other info
     */
} ipvx_addr_t;


/* utility macros */

#define IPVX_CLEARADDR(a)                      \
    (a).v6ip.s6_addr32_sc[0] = 0;                            \
    (a).v6ip.s6_addr32_sc[1] = 0;                            \
    (a).v6ip.s6_addr32_sc[2] = 0;                            \
    (a).v6ip.s6_addr32_sc[3] = 0;                            \
    /* Write to ver and pad[3] in one machine instruction */ \
    *(uint32_t*)&((a).ver) = 0;                              

#define IPVX_IS_EQUAL_4(a,b)                                 \
    (((a)->ver == IP_VERSION_4) &&                           \
     ((b)->ver == IP_VERSION_4) &&                           \
     ((a)->v4ip == (b)->v4ip))	

#define IPVX_IS_EQUAL_6(a,b)                                           \
    (((a)->ver == IP_VERSION_6) &&                                     \
     ((b)->ver == IP_VERSION_6) &&                                     \
     ((a)->vxip.s6_addr32_sc[0] == (b)->vxip.s6_addr32_sc[0]) &&       \
     ((a)->vxip.s6_addr32_sc[1] == (b)->vxip.s6_addr32_sc[1]) &&       \
     ((a)->vxip.s6_addr32_sc[2] == (b)->vxip.s6_addr32_sc[2]) &&       \
     ((a)->vxip.s6_addr32_sc[3] == (b)->vxip.s6_addr32_sc[3]))

#define IPVX_IS_ZERO_4(a)                                    \
    (((a)->ver == IP_VERSION_4) &&                           \
     ((a)->v4ip == 0))

#define IPVX_IS_ZERO_6(a)                                    \
    (((a)->ver == IP_VERSION_6) &&                           \
     ((a)->vxip.s6_addr32_sc[0] == 0) &&                     \
     ((a)->vxip.s6_addr32_sc[1] == 0) &&                     \
     ((a)->vxip.s6_addr32_sc[2] == 0) &&                     \
     ((a)->vxip.s6_addr32_sc[3] == 0))

#define IPVX_VERSION_4(a)                                    \
    ((a)->ver == IP_VERSION_4)

#define IPVX_VERSION_6(a)                                    \
    ((a)->ver == IP_VERSION_6)

/*
 * IPVX_ARE_ADDR_EQUAL
 *
 * Checks if the given IP addresses are equal or not.
 * Return TRUE if they are equal, else returns FALSE.
 */
#define IPVX_ARE_ADDR_EQUAL(a,b)                             \
    (IPVX_IS_EQUAL_4((a),(b)) || IPVX_IS_EQUAL_6((a),(b)))

/*
 * IPVX_IS_ZERO
 * IPVX_ARE_ZERO (SRX platform uses this macro)
 *
 * Checks if the given IP address is ZERO
 */
#define IPVX_IS_ZERO(a)                                      \
    (IPVX_IS_ZERO_4((a)) || IPVX_IS_ZERO_6((a)))

#define IPVX_ARE_ZERO(a) IPVX_IS_ZERO(a)

/*
 * IPVX_COPYADDR
 *
 * Copies address in SRC to DEST.
 */
#define IPVX_COPYADDR(DEST, SRC)                             \
do {                                                         \
    if ((SRC)->ver == IP_VERSION_4) {                        \
        (DEST)->v4ip = (SRC)->v4ip;                          \
    } else {                                                 \
        (DEST)->vxip = (SRC)->vxip; /*  structure copy */    \
    }                                                        \
    (DEST)->ver = (SRC)->ver;                                \
} while (0)

/*
 * The following three macros return 1, 0 or -1, according
 * as the address 'a' is greater than, equal to, or less than the address 'b'.
 * IPVX_CMP will return -2 in case of ip version mismatch
 */
#define IPVX_CMP_4(a,b)                                                   \
    (((a)->v4ip > (b)->v4ip)?1:(((a)->v4ip < (b)->v4ip)?-1:0))

#define IPVX_CMP_6(a,b)                                                   \
    ((((a)->vxip.s6_addr32_sc[3] > (b)->vxip.s6_addr32_sc[3])?1:          \
     (((a)->vxip.s6_addr32_sc[3] < (b)->vxip.s6_addr32_sc[3])?-1:         \
     (((a)->vxip.s6_addr32_sc[2] > (b)->vxip.s6_addr32_sc[2])?1:          \
     (((a)->vxip.s6_addr32_sc[2] < (b)->vxip.s6_addr32_sc[2])?-1:         \
     (((a)->vxip.s6_addr32_sc[1] > (b)->vxip.s6_addr32_sc[1])?1:          \
     (((a)->vxip.s6_addr32_sc[1] < (b)->vxip.s6_addr32_sc[1])?-1:         \
     (((a)->vxip.s6_addr32_sc[0] > (b)->vxip.s6_addr32_sc[0])?1:          \
     (((a)->vxip.s6_addr32_sc[0] < (b)->vxip.s6_addr32_sc[0])?-1:0)))))))))

#define IPVX_CMP(a,b)                                                     \
    ((((a)->ver == IP_VERSION_6) && ((b)->ver == IP_VERSION_6))?          \
     IPVX_CMP_6(a,b):                                                     \
     (((a)->ver == IP_VERSION_4) && ((b)->ver == IP_VERSION_4)?           \
     IPVX_CMP_4(a,b):-2))


#else /* PLATFORM_MTX */

#define IP_VERSION_4   0
#define IP_VERSION_6   1
#define IP_VERSION_DC  2 /* don't care */


#ifndef _IP6_ADDR_T_DECLARED

#define INET6_ADDR_BYTES       16
#define INET6_ADDR_HALFWORDS   8
#define INET6_ADDR_WORDS       4
        
typedef struct ip6_addr_ {
    union {
        uint8_t   byte[INET6_ADDR_BYTES];
        uint16_t  halfword[INET6_ADDR_HALFWORDS];
        uint32_t  word[INET6_ADDR_WORDS];
    } a;
#define s6_addr_sc   a.byte
#define s6_addr8_sc  a.byte
#define s6_addr16_sc a.halfword
#define s6_addr32_sc a.word
} ip6_addr_t;

#define _IP6_ADDR_T_DECLARED

#endif /* _IP6_ADDR_T_DECLARED */

typedef u_int32_t ip_version_t;

typedef struct ipvx_addr_ {
    union {
        ip6_addr_t _v6ip;
        uint32_t   _v4ip;
    } _u;
#define v6ip    _u._v6ip
#define v4ip    _u._v4ip
#define vxip    _u._v6ip

    uint8_t ver;
#define vxiplen ver*12+4
    uint8_t prefix_len;
    uint8_t pad[2];
    /*
     * the remaining 2 bytes here could used by the platform to store
     * some other info
     */
} ipvx_addr_t;


/* utility macros */

#define IPVX_CLEARADDR(a)                      \
    (a).v6ip.s6_addr32_sc[0] = 0;                            \
    (a).v6ip.s6_addr32_sc[1] = 0;                            \
    (a).v6ip.s6_addr32_sc[2] = 0;                            \
    (a).v6ip.s6_addr32_sc[3] = 0;                            \
    /* Write to ver and pad[3] in one machine instruction */ \
    *(uint32_t*)&((a).ver) = 0;                              

#define IPVX_IS_EQUAL_4(a,b)                                 \
    (((a)->ver == IP_VERSION_4) &&                           \
     ((b)->ver == IP_VERSION_4) &&                           \
     ((a)->v4ip == (b)->v4ip))	

#define IPVX_IS_EQUAL_6(a,b)                                           \
    (((a)->ver == IP_VERSION_6) &&                                     \
     ((b)->ver == IP_VERSION_6) &&                                     \
     ((a)->vxip.s6_addr32_sc[0] == (b)->vxip.s6_addr32_sc[0]) &&       \
     ((a)->vxip.s6_addr32_sc[1] == (b)->vxip.s6_addr32_sc[1]) &&       \
     ((a)->vxip.s6_addr32_sc[2] == (b)->vxip.s6_addr32_sc[2]) &&       \
     ((a)->vxip.s6_addr32_sc[3] == (b)->vxip.s6_addr32_sc[3]))

#define IPVX_IS_ZERO_4(a)                                    \
    (((a)->ver == IP_VERSION_4) &&                           \
     ((a)->v4ip == 0))

#define IPVX_IS_ZERO_6(a)                                    \
    (((a)->ver == IP_VERSION_6) &&                           \
     ((a)->vxip.s6_addr32_sc[0] == 0) &&                     \
     ((a)->vxip.s6_addr32_sc[1] == 0) &&                     \
     ((a)->vxip.s6_addr32_sc[2] == 0) &&                     \
     ((a)->vxip.s6_addr32_sc[3] == 0))

#define IPVX_VERSION_4(a)                                    \
    ((a)->ver == IP_VERSION_4)

#define IPVX_VERSION_6(a)                                    \
    ((a)->ver == IP_VERSION_6)

/*
 * IPVX_ARE_ADDR_EQUAL
 *
 * Checks if the given IP addresses are equal or not.
 * Return TRUE if they are equal, else returns FALSE.
 */
#define IPVX_ARE_ADDR_EQUAL(a,b)                             \
    (IPVX_IS_EQUAL_4((a),(b)) || IPVX_IS_EQUAL_6((a),(b)))

/*
 * IPVX_IS_ZERO
 *
 * Checks if the given IP address is ZERO
 */
#define IPVX_IS_ZERO(a)                                      \
    (IPVX_IS_ZERO_4((a)) || IPVX_IS_ZERO_6((a)))

#define IPVX_ARE_ZERO(a) IPVX_IS_ZERO(a)

/*
 * IPVX_COPYADDR
 *
 * Copies address in SRC to DEST.
 */
#define IPVX_COPYADDR(DEST, SRC)                             \
do {                                                         \
    if ((SRC)->ver == IP_VERSION_4) {                        \
        (DEST)->v4ip = (SRC)->v4ip;                          \
    } else {                                                 \
        (DEST)->vxip = (SRC)->vxip; /*  structure copy */    \
    }                                                        \
    (DEST)->ver = (SRC)->ver;                                \
} while (0)

/*
 * The following three macros return 1, 0 or -1, according
 * as the address 'a' is greater than, equal to, or less than the address 'b'.
 * IPVX_CMP will return -2 in case of ip version mismatch
 */
#define IPVX_CMP_4(a,b)                                                   \
    (((a)->v4ip > (b)->v4ip)?1:(((a)->v4ip < (b)->v4ip)?-1:0))

#define IPVX_CMP_6(a,b)                                                   \
    ((((a)->vxip.s6_addr32_sc[3] > (b)->vxip.s6_addr32_sc[3])?1:          \
     (((a)->vxip.s6_addr32_sc[3] < (b)->vxip.s6_addr32_sc[3])?-1:         \
     (((a)->vxip.s6_addr32_sc[2] > (b)->vxip.s6_addr32_sc[2])?1:          \
     (((a)->vxip.s6_addr32_sc[2] < (b)->vxip.s6_addr32_sc[2])?-1:         \
     (((a)->vxip.s6_addr32_sc[1] > (b)->vxip.s6_addr32_sc[1])?1:          \
     (((a)->vxip.s6_addr32_sc[1] < (b)->vxip.s6_addr32_sc[1])?-1:         \
     (((a)->vxip.s6_addr32_sc[0] > (b)->vxip.s6_addr32_sc[0])?1:          \
     (((a)->vxip.s6_addr32_sc[0] < (b)->vxip.s6_addr32_sc[0])?-1:0)))))))))

#define IPVX_CMP(a,b)                                                     \
    ((((a)->ver == IP_VERSION_6) && ((b)->ver == IP_VERSION_6))?          \
     IPVX_CMP_6(a,b):                                                     \
     (((a)->ver == IP_VERSION_4) && ((b)->ver == IP_VERSION_4)?           \
     IPVX_CMP_4(a,b):-2))

#endif /* PLATFORM_MTX */ 


#ifdef __cplusplus
    }
}
#endif /* __cplusplus */

#endif /* __JSF_IP_H__ */
