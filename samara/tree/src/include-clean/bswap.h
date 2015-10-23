/*
 *
 */

#ifndef __BSWAP_H_
#define __BSWAP_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"

/* ------------------------------------------------------------------------- */
/** \file bswap.h Various byte swap routines
 * \ingroup lc
 */

uint16 bswap16_func(uint16 x);
uint32 bswap32_func(uint32 x);
uint64 bswap64_func(uint64 x);

float32 bswap_float32(float32 x);
float64 bswap_float64(float64 x);

/*
 * WARNING: the "const_always" macros always byte swap, regardless of
 * the target architecture.  These are likely not what you should use.
 * Please see bswap16() and bswap32() below.
 */
#define bswap16_const_always(x) ((uint16)                             \
                 ((((uint16) (x) & (uint16) 0x00ffU) << 8) |          \
                  (((uint16) (x) & (uint16) 0xff00U) >> 8)))          \

#define bswap32_const_always(x) ((uint32)                             \
                 ((((uint32) (x) & (uint32) 0x000000ffUL) << 24) |    \
                  (((uint32) (x) & (uint32) 0x0000ff00UL) <<  8) |    \
                  (((uint32) (x) & (uint32) 0x00ff0000UL) >>  8) |    \
                  (((uint32) (x) & (uint32) 0xff000000UL) >> 24)))

#define bswap64_const_always(x) ((uint64)                             \
         ((((uint64) (x) & (uint64) 0x00000000000000ffULL) << 56) |   \
          (((uint64) (x) & (uint64) 0x000000000000ff00ULL) << 40) |   \
          (((uint64) (x) & (uint64) 0x0000000000ff0000ULL) << 24) |   \
          (((uint64) (x) & (uint64) 0x00000000ff000000ULL) <<  8) |   \
          (((uint64) (x) & (uint64) 0x000000ff00000000ULL) >>  8) |   \
          (((uint64) (x) & (uint64) 0x0000ff0000000000ULL) >> 24) |   \
          (((uint64) (x) & (uint64) 0x00ff000000000000ULL) >> 40) |   \
          (((uint64) (x) & (uint64) 0xff00000000000000ULL) >> 56)))

#if defined(PROD_TARGET_CPU_ENDIAN_LITTLE)

/* This could use bswap16_func() instead of htons() */
#define bswap16(x) \
    (__builtin_constant_p((x)) ?                \
     bswap16_const_always(x) : htons(x))

/* This could use bswap32_func() instead of htonl() */
#define bswap32(x) \
    (__builtin_constant_p((x)) ?                \
     bswap32_const_always(x) : htonl(x))

/*
 * If we have bswap_64(), a non-standard macro, use it, otherwise use
 * our bswap64_func().
 */
#ifdef bswap_64
#define bswap64(x) \
    (__builtin_constant_p((x)) ?                \
     bswap64_const_always(x) : bswap_64(x))
#else
#define bswap64(x) \
    (__builtin_constant_p((x)) ?                \
     bswap64_const_always(x) : bswap64_func(x))
#endif

#define BSWAP16(x) { x = bswap16(x); }
#define BSWAP32(x) { x = bswap32(x); }
#define BSWAP64(x) { x = bswap64(x); }

#define BSWAP_FLOAT32(x) { x = bswap_float32(x); }
#define BSWAP_FLOAT64(x) { x = bswap_float64(x); }

#elif defined(PROD_TARGET_CPU_ENDIAN_BIG)

#define bswap16(x) (x)
#define bswap32(x) (x)
#define bswap64(x) (x)

#define BSWAP16(x) { }
#define BSWAP32(x) { }
#define BSWAP64(x) { }

#define BSWAP_FLOAT32(x) { }
#define BSWAP_FLOAT64(x) { }

#else
#error No valid PROD_TARGET_CPU_ENDIAN_xxx define found
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BSWAP_H_ */
