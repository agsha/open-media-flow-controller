/*
 *
 * cond.h
 *
 *
 *
 */

#ifndef __COND_H_
#define __COND_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file cond.h Condition variable
 * \ingroup conc
 */

#include "common.h"
#include "ttime.h"
#include "lock.h"

typedef struct lc_cond lc_cond;

/**
 * Create a new condition variable, associated with the specified lock.
 * Note that this lock must not be deleted as long as the condition
 * variable object exists.
 */
int lc_cond_init(lc_cond **ret_cond, lc_lock *lock);

/**
 * Destroy a condition variable.  There must be no other threads waiting
 * on the condition variable at this time.
 */
int lc_cond_deinit(lc_cond **inout_cond);

/**
 * Wait on a condition variable.  You must already hold the lock with
 * which the condition variable was initialized; if not, behavior is
 * undefined.  Assuming you do hold the lock, the lock will be
 * released as you wait, but when the call returns, you will have the
 * lock again (except in case of a timeout).
 *
 * Note that if the lock is recursive, and you hold it more than
 * once, it may not be released as you wait.  (XXX/EMT: check the
 * pthreads behavior here to verify)
 *
 * The timeout is optional.  Pass -1 for no timeout.  If a timeout is
 * specified, and the call returns due to a timeout, ret_timed_out
 * will be set to true; otherwise, it will be set to false.
 */
int lc_cond_wait(lc_cond *cond, lt_dur_ms timeout, tbool *ret_timed_out);

/**
 * Signal on a condition variable.  This will wake up exactly one
 * thread who is waiting on the condition, if there are any.
 */
int lc_cond_signal(lc_cond *cond);

/**
 * Broadcast on a condition variable.  This will wake up all threads
 * who are waiting on the condition.
 */
int lc_cond_broadcast(lc_cond *cond);

#ifdef __cplusplus
}
#endif

#endif /* __COND_H_ */
