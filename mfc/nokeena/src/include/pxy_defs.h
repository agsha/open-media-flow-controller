/*
 * (C) Copyright 2008 Nokeena Networks, Inc
 *
 * Definitions used commonly across the server.
 */
#ifndef __PXY_DEFS_H__
#define __PXY_DEFS_H__
#include <sys/time.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <atomic_ops.h>


/* Maximum network FDs that we can create/allocate in proxyd */
#ifdef MAX_GNM
#undef MAX_GNM
#endif
#define MAX_GNM 	(40*1024)

/* Optimized for rounding up when Y is power of 2 */
#define ROUNDUP_PWR2(x, y) ( ((x) + (y) - 1) & ~((y)-1) )

/* Expiry time constants */
#define NKN_EXPIRY_UNDEFINED 0
#define NKN_EXPIRY_FOREVER (-1)

#define MAX_URI_HDR_HOST_SIZE 128
// /{name max 16}:XXXXXXXX (max 8) _YYYYYYYY (host max 128):XXXXX (port max 5)
#define MAX_URI_HDR_SIZE (1+16+1+8+1+MAX_URI_HDR_HOST_SIZE+1+5)
#define MAX_URI_SIZE (MAX_URI_HDR_SIZE+512)

/* Pseudo file descriptor macros */
#ifndef PSEUDO_FD_BASE
#define PSEUDO_FD_BASE (1024 * 1024)
#endif

/* PSEUDO_FD_BASE must be > MAX_GNM to avoid any clash in the fd 
 * space. Check MAX_GNM
 */
#if (defined( MAX_GNM) && (MAX_GNM > 0))
#if (MAX_GNM >= PSEUDO_FD_BASE)
#error "Potential clash in fd space for MAX_GNM (nkn_defs.h) and PSEUDO_FD_BASE (nkn_defs.h). Resolve this first!"
#endif /* (MAX_GNM > PSEUDO_FD_BASE) */
#endif /* ifdef MAX_GNM */

#define IS_PSEUDO_FD(_fd) (((_fd) >= PSEUDO_FD_BASE) && ((_fd) > 0))

/*
 * nkn_attr_id_t definitions
 */
#define NKN_ATTR_MIME_HDR_ID   0xfff

#ifndef TRUE
#define FALSE    (0)
#define TRUE    (!FALSE)
#endif


/*
 * For less confusion,
 * Nokeena refers K, M, G to 1000 based
 * Nokeena refers Ki, Mi, Gi to 1024 based
 */
#define KBYTES	1000
#define KiBYTES	1024
#define MBYTES	(1000 * 1000)
#define MiBYTES	(1024 * 1024)
#define GBYTES	(1000 * 1000 * 1000)
#define GiBYTES	(1024 * 1024 * 1024)

/*
 * When -Wall option in compiler is enabled,
 * we should suppress any argument which is not used inside function
 * The following macro should work.
 */
#define UNUSED_ARGUMENT(x) (void)x

/*
 * Global variables that are kept updated by the timer using the time() system
 * call at a 1 second granularity.  This is a very cheap way to add
 * a coarse grained timestamp.
 */
extern time_t pxy_cur_ts;
extern int    pxy_system_inited;

#endif /* __PXY_DEFS_H__ */
