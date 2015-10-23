/*
 *
 * tinttypes.h
 *
 *
 *
 */

#ifndef __TINTTYPES_H_
#define __TINTTYPES_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file src/include/tinttypes.h Expansion of defines for 64-bit compatibility
 * \ingroup lc
 */

/* As we do not include common.h, check if this include is prohibited */
#if defined(NON_REDIST_HEADERS_PROHIBITED)
#error Non-redistributable header file included when prohibited
#endif

#include <inttypes.h>

/* Set LC_WORDSIZE to 32 or 64 */
# if defined(__arch64__) || defined(_LP64) || defined(__LP64_)
#define LC_WORDSIZE 64
#else
#define LC_WORDSIZE 32
#endif

# if LC_WORDSIZE == 64
#  define __INT64_C(c)  c ## L
#  define __UINT64_C(c) c ## UL
# else
#  define __INT64_C(c)  c ## LL
#  define __UINT64_C(c) c ## ULL
# endif

/* Solaris does not define this, so help it out */
#ifndef PRIdPTR

# if LC_WORDSIZE == 64
#  define LCF_PRI64_PREFIX        "l"
#  define LCF_PRIPTR_PREFIX       "l"
# else
#  define LCF_PRI64_PREFIX        "ll"
#  define LCF_PRIPTR_PREFIX
# endif

#define PRIdPTR        LCF_PRIPTR_PREFIX "d"
#define PRIiPTR        LCF_PRIPTR_PREFIX "i"
#define PRIoPTR        LCF_PRIPTR_PREFIX "o"
#define PRIuPTR        LCF_PRIPTR_PREFIX "u"
#define PRIxPTR        LCF_PRIPTR_PREFIX "x"
#define PRIXPTR        LCF_PRIPTR_PREFIX "X"

#endif

/* Now, some of our own, type-specific format strings */

/* 
 * These are for outputing posix types.  In particular:
 *
 * LPRI_d_SIZE_T : size_t
 * LPRI_d_OFF_T :  off_t
 * LPRI_d_ID_T :   pid_t, uid_t, gid_t
 *
 * XXX: should also to MODE_T, etc.
 *
 */
# if LC_WORDSIZE == 64
# define LPRI_d_SIZE_T PRId64
#else
# define LPRI_d_SIZE_T PRId32
#endif

#define LPRI_d_OFF_T PRId64

/* Solaris has a strange idea of uids, etc. on 32 bit */
#if defined(PROD_TARGET_OS_SUNOS) && LC_WORDSIZE == 32
# define LPRI_d_ID_T "ld"
#else
# define LPRI_d_ID_T PRId32
#endif

#ifdef __cplusplus
}
#endif

#endif /* __TINTTYPES_H_ */
