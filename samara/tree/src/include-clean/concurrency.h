/*
 *
 * concurrency.h
 *
 *
 *
 */

#ifndef __CONCURRENCY_H_
#define __CONCURRENCY_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"

/** \defgroup conc LibTc_st and LibTc_mt: concurrency libraries
 *
 * Libtc_st and libtc_mt are two libraries that both implement the
 * same API.  The "tc" stands for "Tall maple Concurrency"; "st"
 * for "single-threaded" and "mt" for "multi-threaded".
 * If you are linking with libcommon, you must also link with one
 * of these libraries; choose one depending on whether or not your 
 * daemon uses threads.
 *
 * NOTE: if you are multithreaded, you MUST link with libtc_mt.
 * If you link with libtc_st, and if it ever finds out you are
 * multithreaded, it will flood the logs with errors, though it will
 * otherwise continue working as well as it did before.  i.e. we won't
 * return an error just for this reason, but we also won't be doing
 * anything to make multithreading safe.
 */

/**
 * \file src/include/concurrency.h Threading and concurrency-related APIs.
 * \ingroup conc
 */

/* ------------------------------------------------------------------------- */
/** Tells whether we are linked with libtc_st or libtc_mt, which
 * should equate to whether we are a singlethreaded or multithreaded
 * application.  Used in some cases to choose between threadsafe and
 * non-threadsafe behavior.
 */
extern const tbool lc_is_multithreaded;


/* ------------------------------------------------------------------------- */
/** Get the thread ID of the caller, if any.  
 * In pthreads, this should be a pthread_t,
 * but if we're building singlethreaded we may not have that type
 * defined.  So we hope that it is no bigger than a uint64, and that a
 * uint64 can be cast to it if necessary.
 *
 * \retval ret_thread_id The thread ID of the caller, if any, or 
 * 0 if the process is not multithreaded.
 */
int ltc_get_thread_id(uint64 *ret_thread_id);

uint64 ltc_get_thread_id_quick(void);


#ifdef __cplusplus
}
#endif

#endif /* __CONCURRENCY_H_ */
