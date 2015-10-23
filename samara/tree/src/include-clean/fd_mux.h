/*
 *
 * fd_mux.h
 *
 *
 *
 */

#ifndef __FD_MUX_H_
#define __FD_MUX_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "fdintrcb.h"

/* ------------------------------------------------------------------------- */
/** \file fd_mux.h File descriptor multiplexing utilities.
 * \ingroup lc
 *
 * These functions allow you to multiplex two file descriptors -- one 
 * for input and one for output -- over a single one which can be both
 * read from and written to.  For example, you could get a single
 * file descriptor that represents stdin for reading, and stdout for
 * writing, by passing STDIN_FILENO and STDOUT_FILENO.
 */

typedef struct fdmux_state fdmux_state;

/**
 * Create a new file descriptor which multiplexes two existing file 
 * descriptors: one for input and one for output.
 *
 * \param input_fd The existing fd to use for input (reading).
 * \param output_fd The existing fd to use for output (writing).
 * \param fdich File descriptor interest callback handle to use.
 * See fdintrcb.h.
 * \retval ret_bidir_caller_fd Newly-created fd that multiplexes
 * two existing fds.
 * \retval ret_mux_state Newly create context structure which tracks
 * the state of this new fd.  When you are done with this, you
 * must deinitialize it with fds_mux_deinit().
 */
int fds_mux_init(int input_fd, int output_fd, fdic_handle *fdich,
                  int *ret_bidir_caller_fd,
                  fdmux_state **ret_mux_state);

/**
 * Close the fdmux without freeing the memory associated with it.
 * Might be useful if code which may run later still has a pointer to
 * the state and you don't want to free it out from under them.
 */
int fds_mux_close(fdmux_state *ux_state);

/**
 * Close the fdmux (if it's not already closed), and free any memory
 * associated with it.
 *
 * CAUTION: do not call this function from code that may have
 * fds_mux_close() below it in the stack.  For example, if you used
 * fdic_libevent_set_fd_close_handler() to set an fd close handler,
 * do not call fds_mux_deinit() from there.  It may have been called
 * during the execution of fds_mux_close(), in which case this call
 * could log errors and/or free data structures out from under the
 * call to fds_mux_close().  See bug 12618.
 */
int fds_mux_deinit(fdmux_state **inout_mux_state);

#ifdef __cplusplus
}
#endif

#endif /* __FD_MUX_H_ */
