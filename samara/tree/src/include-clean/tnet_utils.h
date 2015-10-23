/*
 *
 * tnet_utils.h
 *
 *
 *
 */

#ifndef __TNET_UTILS_H_
#define __TNET_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* As we do not include common.h, check if this include is prohibited */
#if defined(NON_REDIST_HEADERS_PROHIBITED)
#error Non-redistributable header file included when prohibited
#endif

#include "ttypes.h"

/* ========================================================================= */
/** \file src/include/tnet_utils.h Networking related routines.
 * \ingroup lc
 *
 * This library provides some basic networking related routines.
 */

/**
 * See if a given socket protocol, like AF_INET6 or AF_INET is supported
 * by the running system.
 */
int tnet_protocol_available(int domain, tbool *ret_avail);


#ifdef __cplusplus
}
#endif

#endif /* __TNET_UTILS_H_ */
