/*
 *
 * ftrans_scp.h
 *
 *
 *
 */

#ifndef __FTRANS_SCP_H_
#define __FTRANS_SCP_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file ftrans_scp.h Utilities for transfering files via scp
 * \ingroup lc
 */

#include "common.h"
#include "tstring.h"
#include "ftrans_utils.h"

/**
 * Transfer a file via scp (blocking wait).  If pull_file == true, copy
 * from remote to local, else from local to remote.
 *
 * If the transfer does not complete successfully, a string containing
 * a description of the error is returned in ret_output.
 *
 * \param pull_file Direction of the operation (set to true for download)
 * \param password_fd Set to true if providing a remote_password.  The
 * password will be passed to the the ssh client via a secure pipe, else
 * it is ignored.  Note that if remote_password is NULL, nothing will be
 * sent on the pipe, and the protocol will assume an empty password.
 * \param remote_user Username to use for remote authentication
 * \param remote_password Password for user.  Note: 1) ignored unless
 * password_fd is true; 2) NULL is equivalent to an empty password.
 * \param remote_host Name or IP address of the remote system
 * \param remote_port The sshd service port number, or 0 for the default
 * \param remote_path Path for the file on the remote system
 * \param local_path The file to upload from or directory to download to
 * \param progress_oper_id Operation ID to use to track progress, or NULL
 * \retval ret_output Error text from scp
 */
int lft_scp_transfer_file(tbool pull_file, tbool password_fd,
                          const char *remote_user,
                          const char *remote_password,
                          const char *remote_host,
                          uint16 remote_port,
                          const char *remote_path,
                          const char *local_path,
                          const char *progress_oper_id,
                          tstring **ret_output);


/**
 * Transfer a file via scp without blocking.  If pull_file == true, copy
 * from remote to local, else from local to remote.  This function should
 * be followed with a call to lft_scp_transfer_file_non_blocking_complete()
 * when the child transfer process exists.
 *
 * \param pull_file Direction of the operation (set to true for download)
 * \param password_fd Set to true if providing a remote_password.  The
 * password will be passed to the the ssh client via a secure pipe, else
 * it is ignored.  Note that if remote_password is NULL, nothing will be
 * sent on the pipe, and the protocol will assume an empty password.
 * \param remote_user Username to use for remote authentication
 * \param remote_password Password for user.  Note: 1) ignored unless
 * password_fd is true; 2) NULL is equivalent to an empty password.
 * \param remote_host Name or IP address of the remote system
 * \param remote_port The sshd service port number, or 0 for the default
 * \param remote_path Path for the file on the remote system
 * \param local_path The file to upload from or directory to download to
 * \param progress_oper_id Operation ID to use to track progress, or NULL
 * \retval ret_launch_result Return variable for the transfer process
 * results.  The caller must free the contents and the structure by
 * invoking lc_launch_result_free_contents() and safe_free().
 * \retval ret_ft_state Return variable for contextual information about
 * the non-blocking transfer required by the transfer completion call
 * function.  Must be freed by the caller.
 */
int lft_scp_transfer_file_non_blocking(tbool pull_file, tbool password_fd,
                                       const char *remote_user,
                                       const char *remote_password,
                                       const char *remote_host,
                                       uint16 remote_port,
                                       const char *remote_path,
                                       const char *local_path,
                                       const char *progress_oper_id,
                                       lc_launch_result **ret_launch_result,
                                       lft_state **ret_ft_state);


/**
 * Completion function companion to lft_scp_transfer_file_non_blocking() to
 * be called when the child transfer process exits.
 *
 * If the transfer did not complete successfully, a non-zero lc error code is
 * is returned in ret_return_code and a string containing a description of
 * the error is returned in ret_output.
 *
 * \param inout_launch_result A copy of the launch result returned by the
 * transfer function.  Prior to passing this, set its lr_child_pid to -1,
 * and it's lr_exit_status to the status returned by waitpid().  Lanch
 * results are stored in lr_captured_output after some tailoring, and file
 * cleanup is performed if an error occurs.  The caller must free the
 * contents and the structure by invoking lc_launch_result_free_contents()
 * and safe_free().
 * \param inout_ft_state Contextual information about the transfer
 * indicating what file (if any) should be removed on error.  Presently
 * this parameter structure is not altered by the call.
 * \retval ret_return_code Error code from the transfer process itself.
 * \retval ret_output Error text from the transfer process (must be freed
 * by the caller).
 */
int lft_scp_transfer_file_non_blocking_complete(lc_launch_result **
                                                inout_launch_result,
                                                lft_state **
                                                    inout_ft_state,
                                                int *ret_return_code,
                                                tstring **ret_output);


#ifdef __cplusplus
}
#endif

#endif /* __FTRANS_SCP_H_ */
