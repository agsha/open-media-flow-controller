/*
 *
 * lockfile.h
 *
 *
 *
 */

#ifndef __LOCKFILE_H_
#define __LOCKFILE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file src/include/lockfile.h File locking utilities
 * \ingroup lc
 */

#include "common.h"

typedef enum {
    fll_none = 0,
    fll_unlock = 1,
    fll_shared_read,
    fll_exclusive_write
} fl_locktype;

int fl_set_lock(int fd, fl_locktype type, tbool block);

int fl_open_file_take_lock(const char *path, int open_flags, mode_t mode,
                           fl_locktype type, tbool block, int *ret_fd);

int fl_close_file_drop_lock(int fd);

#if 0
/* XXX? */
int fl_query_lock(int fd fl_locktype *ret_type);
#endif




#ifdef __cplusplus
}
#endif

#endif /* __LOCKFILE_H_ */
