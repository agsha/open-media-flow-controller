/*
 *
 * lock.h
 *
 *
 *
 */

#ifndef __LOCK_H_
#define __LOCK_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file lock.h Lock utilities
 * \ingroup conc
 */

#include "ttypes.h"

typedef struct lc_lock lc_lock;

int lc_lock_init(lc_lock **ret_lock, tbool recursive);
int lc_lock_deinit(lc_lock **ret_lock);
int lc_lock_lock(lc_lock *lock);
int lc_lock_unlock(lc_lock *lock);

#ifdef __cplusplus
}
#endif

#endif /* __LOCK_H_ */
