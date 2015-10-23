/*
 *
 * signal_utils.h
 *
 *
 *
 */

#ifndef __SIGNAL_UTILS_H_
#define __SIGNAL_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <signal.h>
#include "common.h"

/* ------------------------------------------------------------------------- */
/** \file signal_utils.h Signal name to number conversions
 * \ingroup lc
 */

/* ......................................................................... */
/** @name Libcommon functions
 *
 * The following functions are in libcommon, and do not care about
 * threading.
 */

/*@{*/

const char *lc_signal_num_to_name(int signum);
int lc_signal_name_to_num(const char *signal_name);

extern const lc_enum_string_map lc_signal_names[];

/*@}*/

/* ......................................................................... */
/** @name Concurrency functions
 *
 * The following functions are in libtc_st and libtc_mt, and are
 * implemented differently in each.
 */

/*@{*/

/**
 * This just passes straight through to sigprocmask() in a
 * single-threaded app, and to pthread_sigmask() in a multi-threaded
 * app.  Neither one of those can be called from the other environment
 * (assuming a single-threaded app has not linked with libpthread).
 *
 * This is most interesting for libraries, who may not know whether or
 * not they are running in a multithreaded context.
 */
int tc_sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* __SIGNAL_UTILS_H_ */
